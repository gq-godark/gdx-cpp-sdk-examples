#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <godark/enums.hpp>
#include <godark/errors.hpp>
#include <godark/types.hpp>
#include <godark/visibility.hpp>

namespace godark {

/// Load symbol map from the embedded shared/symbols.json (build-time).
/// Falls back to a small hardcoded map if the JSON was unavailable.
GODARK_API std::map<std::string, uint64_t> load_default_symbol_map();

/// WebSocket transport configuration (TLS, headers, timeouts).
struct TransportConfig {
    /// When true, skip TLS certificate verification (development only).
    bool tls_skip_verify = false;
    /// Extra HTTP headers attached to the WebSocket upgrade handshake.
    std::map<std::string, std::string> extra_headers;
    /// TCP + TLS connect timeout in seconds.
    int connect_timeout_sec = 30;
    /// How long to wait for a command (place/cancel/modify) response in seconds.
    int command_timeout_sec = 30;
    /// How long to wait for the ECDH `session.setup` reply from the edge,
    /// in seconds. Matches Python (`client.py:475`), JS (`client.ts:489`),
    /// and Rust (`client.rs:27`) at 10 s. Older releases hardcoded 5 s,
    /// which flaked on contended localnet clusters where the sequencer was
    /// queued behind background traffic (vault MM, etc.).
    int session_setup_timeout_sec = 10;
    /// Client ping interval in seconds.
    int heartbeat_interval_sec = 30;
    /// No inbound message for this long triggers disconnect (seconds).
    int stale_timeout_sec = 60;
    /// When true (default), emit public-docs `{id, op, args}` frames and
    /// normalize docs replies into the legacy SDK callback/ack shapes.
    bool use_docs_wire = true;
};

struct ClientConfig {
    std::string api_key_id;
    std::string api_secret;
    /// User-chosen API key passphrase (required with key pair; also reads
    /// GODARK_PASSPHRASE / GDX_PASSPHRASE from the environment).
    std::string passphrase;
    /// Standalone API key (sent as-is). Use either api_key OR api_key_id+api_secret+passphrase.
    std::string api_key;
    /// WebSocket base URL. The client appends `/ws/v1` automatically (the
    /// canonical endpoint per the public docs). Legacy `/ws` suffixes are
    /// upgraded transparently. Defaults to the public testnet host
    /// `wss://api.godark-dex.com` (no public mainnet today); override via
    /// `base_url` directly or read `GODARK_EDGE_URL` / `GDX_EDGE_URL` at
    /// the call site to point at a localnet edge for development.
    std::string base_url = "wss://api.godark-dex.com";
    /// Optional user UUID. Falls back to GODARK_USER_UUID / GDX_USER_UUID env vars,
    /// then to the auth response. Required for local edge instances that omit it.
    std::string user_uuid;
    /// When true (default), automatic reconnect with backoff after transport disconnect.
    bool auto_reconnect = true;
    /// Bounded buffer size for order/position update queues (drop-oldest when full).
    size_t stream_buffer_size = 256;
    /// WebSocket transport settings.
    TransportConfig transport;
    /// Symbol name -> numeric ID mapping. Loaded from shared/symbols.json by default.
    std::map<std::string, uint64_t> symbol_map = load_default_symbol_map();
};

/// Encrypted trading client for the GoDark exchange.
///
/// **Single-flight command concurrency**: Only one order command
/// (place_order, cancel_order, modify_order) may be in-flight at a time.
/// The transport tracks a single pending command slot; issuing a second
/// command before the first completes will cause undefined behavior.
/// In practice, call these methods sequentially (each blocks until the
/// exchange responds or the command_timeout expires).
class GODARK_API GodarkClient {
public:
    explicit GodarkClient(const ClientConfig& config);
    ~GodarkClient();

    // Non-copyable and non-movable. The PIMPL (`Impl`) holds a back-pointer
    // to its owning `GodarkClient` for dispatching the public `on_*`
    // callback fields below from transport-owned threads (reconnect loop,
    // encrypted-push handler, cleartext order/position handlers). Defaulting
    // the move operations would leave that back-pointer aimed at the
    // moved-from instance, silently breaking callback delivery and creating
    // use-after-move hazards while worker threads are live. If a movable
    // variant is ever needed, the public callbacks must first migrate into
    // `Impl` (with thread-safe setters) so no back-pointer remains.
    GodarkClient(const GodarkClient&) = delete;
    GodarkClient& operator=(const GodarkClient&) = delete;
    GodarkClient(GodarkClient&&) = delete;
    GodarkClient& operator=(GodarkClient&&) = delete;

    void connect();
    void disconnect();
    void logout();

    bool is_connected() const;
    std::optional<std::string> user_uuid() const;

    OrderAck place_order(
        const std::string& symbol,
        Side side,
        OrderType order_type,
        double quantity,
        std::optional<double> price = std::nullopt,
        TimeInForce tif = TimeInForce::GTC);

    OrderAck cancel_order(const std::string& order_id, const std::string& symbol);
    OrderAck modify_order(const std::string& order_id,
                          const std::string& symbol,
                          std::optional<double> new_price = std::nullopt,
                          std::optional<double> new_quantity = std::nullopt);

    /// Bulk cancel-replace (market-maker mass quote) on one symbol (up to 20
    /// legs), fused into one MPC round. `post_only` selects the batch matching
    /// mode: std::nullopt keeps the node default (post-only), where a leg that
    /// would cross is rejected as "failed"; `false` enables the relaxed path,
    /// where a crossing leg takes liquidity up to its limit and rests the
    /// remainder (per-leg taker fills are surfaced as `fill_count`).
    MassQuoteAck mass_quote(const std::string& symbol,
                            const std::vector<MassQuoteLegInput>& legs,
                            uint32_t leverage = 1,
                            std::optional<bool> post_only = std::nullopt);

    /// Cancel multiple resting orders on one symbol in a single fanned-out
    /// request (up to 20 ids; zero online MPC rounds). An id that is not resting
    /// is reported cancelled=false (error_code 2003) without aborting the batch.
    BatchCancelAck batch_cancel(const std::string& symbol,
                                const std::vector<uint64_t>& order_ids);

    /// Amend multiple resting orders on one symbol in a single fanned-out
    /// post-only request (up to 20 legs). A leg whose amended order would cross
    /// is rejected (modified=false, error_code 2018); a missing id is reported
    /// modified=false (2003). Neither aborts the rest of the batch.
    BatchModifyAck batch_modify(const std::string& symbol,
                                const std::vector<BatchModifyLegInput>& legs);

    void subscribe(const std::vector<std::string>& channels = {"orders", "positions"});
    void unsubscribe(const std::vector<std::string>& channels = {"orders", "positions"});

    /// Non-blocking pull from the bounded order update queue.
    std::optional<OrderUpdate> try_recv_order();
    /// Non-blocking pull from the bounded position update queue.
    std::optional<PositionUpdate> try_recv_position();
    /// Non-blocking pull from the bounded `PositionsSnapshot` queue.
    std::optional<PositionsSnapshot> try_recv_positions_snapshot();
    /// Non-blocking pull from the bounded sequencer health-pulse queue.
    std::optional<SystemHealthUpdate> try_recv_system_health();
    /// Non-blocking pull from the bounded shielded-balance update queue.
    std::optional<BalanceUpdate> try_recv_balance();
    /// Non-blocking pull from the bounded margin-alert queue.
    std::optional<MarginAlert> try_recv_margin_alert();
    /// Non-blocking pull from the bounded funding-rate update queue.
    std::optional<FundingRateUpdate> try_recv_funding_rate();
    /// Non-blocking pull from the bounded settlement-update queue.
    std::optional<SettlementUpdate> try_recv_settlement();

    std::function<void(const OrderUpdate&)> on_order_update;
    std::function<void(const PositionUpdate&)> on_position_update;
    /// Full per-user positions snapshot (initial / periodic / event-driven).
    std::function<void(const PositionsSnapshot&)> on_positions_snapshot;
    /// Sequencer / MPC node health pulse routed via the trading WS.
    std::function<void(const SystemHealthUpdate&)> on_system_health;
    /// Updated shielded balance for the authenticated user.
    std::function<void(const BalanceUpdate&)> on_balance_update;
    /// Margin tier transitions / recoveries for `(owner, symbol_id)`.
    std::function<void(const MarginAlert&)> on_margin_alert;
    /// Per-symbol funding-rate ticks.
    std::function<void(const FundingRateUpdate&)> on_funding_rate_update;
    /// Settlement batch lifecycle updates.
    std::function<void(const SettlementUpdate&)> on_settlement_update;
    /// Invoked after a successful automatic reconnect and optional channel resubscribe.
    std::function<void()> on_reconnect;
    /// Non-fatal errors (rekey failures, decrypt/parse failures on pushes).
    /// The client remains usable; use this for logging / alerting.
    std::function<void(const Error&)> on_error;

private:
#if defined(GODARK_ENABLE_TEST_PEER)
    friend class GodarkClientTestPeer;
#endif
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::string ws_url() const;
};

} // namespace godark
