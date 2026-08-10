#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include <godark/enums.hpp>
#include <godark/errors.hpp>
#include <godark/types.hpp>
#include <godark/visibility.hpp>

namespace godark {

/// Load offline fallback symbol map (embedded shared/symbols.json at build time).
/// Production clients fetch live instruments from edge on connect unless
/// [`ClientConfig::explicit_symbol_map`] is set.
GODARK_API std::map<std::string, uint64_t> load_default_symbol_map();

/// Default edge base URL (host only) for public testnet.
inline constexpr const char* kDefaultTestnetEdgeBaseUrl = "wss://api.godark-dex.com";

/// Default edge base URL (host only) for public Devnet.
inline constexpr const char* kDefaultDevnetEdgeBaseUrl = "ws://18.143.165.149:13300";

/// Sequencer Noise XK static public key for public testnet (64 hex).
/// This is a public pin, not a user secret.
inline constexpr const char* kTestnetNoiseStaticPublicKeyHex =
    "a9fdd7f26c0de36d82811e9fe1df2509960cd5b25eef037355e209b9222bea7d";

/// Sequencer Noise XK static public key for public Devnet (64 hex).
/// This is a public pin, not a user secret.
inline constexpr const char* kDevnetNoiseStaticPublicKeyHex =
    "a6807e2f6cd04b54cc19be2fd4faea2a1239f1e2896912d91222678ab54cdd45";

/// Named deployment target. Selects the default edge URL and, when known,
/// a baked-in sequencer Noise XK public key pin.
///
/// Explicit `ClientConfig::base_url` / `noise_static_public_key_hex` and the
/// corresponding environment variables still win over these presets.
enum class Environment {
    /// Public testnet (`wss://api.godark-dex.com`) with the published Noise pin.
    Testnet,
    /// Public Devnet (`ws://18.143.165.149:13300`) with its own Noise pin.
    Devnet,
    /// Local edge (`ws://127.0.0.1:4000`). No baked-in Noise pin — set via
    /// `noise_static_public_key_hex` or `GDX_NOISE_STATIC_PUBLIC_KEY`.
    Localnet,
};

/// Default edge base URL for \p environment (host only).
[[nodiscard]] inline constexpr const char* edge_base_url(Environment environment) noexcept {
    switch (environment) {
        case Environment::Testnet:
            return kDefaultTestnetEdgeBaseUrl;
        case Environment::Devnet:
            return kDefaultDevnetEdgeBaseUrl;
        case Environment::Localnet:
            return "ws://127.0.0.1:4000";
    }
    return kDefaultTestnetEdgeBaseUrl;
}

/// Baked-in sequencer Noise XK static public key (64 hex), or `nullptr` when
/// the environment has none (Localnet).
[[nodiscard]] inline constexpr const char* environment_noise_static_public_key_hex(
    Environment environment) noexcept {
    switch (environment) {
        case Environment::Testnet:
            return kTestnetNoiseStaticPublicKeyHex;
        case Environment::Devnet:
            return kDevnetNoiseStaticPublicKeyHex;
        case Environment::Localnet:
            return nullptr;
    }
    return nullptr;
}

/// Resolve edge base URL: non-empty explicit > `GODARK_EDGE_URL` /
/// `GDX_EDGE_URL` > environment preset.
GODARK_API std::string resolve_edge_base_url(
    std::string_view explicit_url,
    Environment environment = Environment::Testnet);

/// Resolve Noise XK pin: non-empty explicit > env vars >
/// baked-in environment pin (empty when Localnet and unset).
GODARK_API std::string resolve_noise_static_public_key_hex(
    std::string_view explicit_hex,
    Environment environment = Environment::Testnet);

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
    /// How long to wait for each Noise XK handshake reply from the edge,
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
    /// Named deployment. Defaults to [`Environment::Testnet`], which supplies
    /// the public testnet edge URL and Noise XK pin when those are not set
    /// explicitly or via environment variables. Use [`Environment::Devnet`]
    /// for the Devnet edge/pin, or [`Environment::Localnet`] for a local edge.
    Environment environment = Environment::Testnet;
    /// WebSocket base URL. The client appends `/ws/v1` automatically (the
    /// canonical endpoint per the public docs). Legacy `/ws` suffixes are
    /// upgraded transparently. Empty (default) resolves as: this field >
    /// `GODARK_EDGE_URL` / `GDX_EDGE_URL` > [`environment`] preset
    /// (testnet `wss://api.godark-dex.com`, Devnet `ws://18.143.165.149:13300`;
    /// no public mainnet today).
    std::string base_url;
    /// Optional user UUID. Falls back to GODARK_USER_UUID / GDX_USER_UUID env vars,
    /// then to the auth response. Required for local edge instances that omit it.
    std::string user_uuid;
    /// 64-hex-character pinned Noise XK sequencer static public key. Empty
    /// resolves as: this field > `GDX_NOISE_STATIC_PUBLIC_KEY` (aliases
    /// `GDX_NOISE_STATIC_PUBKEY`, `GODARK_NOISE_STATIC_PUBLIC_KEY`) >
    /// baked-in pin from [`environment`] (Testnet/Devnet only).
    std::string noise_static_public_key_hex;
    /// When true (default), automatic reconnect with backoff after transport disconnect.
    bool auto_reconnect = true;
    /// Bounded buffer size for order/position update queues (drop-oldest when full).
    size_t stream_buffer_size = 256;
    /// WebSocket transport settings.
    TransportConfig transport;
    /// Symbol name -> numeric ID mapping. Offline fallback until connect; replaced
    /// from edge ``GET /api/v1/instruments`` unless [`explicit_symbol_map`] is true.
    std::map<std::string, uint64_t> symbol_map = load_default_symbol_map();
    /// When true, use [`symbol_map`] as-is and skip edge instruments fetch on connect.
    bool explicit_symbol_map = false;
};

/// Encrypted trading client for the GoDark exchange.
///
/// Encrypted order commands (place/cancel/modify/batch) are multiplexed by
/// correlation ID on the client: multiple may be in flight on one session.
/// Cleartext transport commands (subscribe, auth, noise handshake) remain
/// single-flight on the transport pending-command slot.
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

    /// Place an order with explicit confirmation semantics. Ack returns after
    /// the command ACK; Book waits for OPEN, REJECTED, FILLED,
    /// PARTIALLY_FILLED, or CANCELLED. The existing overload defaults to Book.
    OrderAck place_order(
        const std::string& symbol,
        Side side,
        OrderType order_type,
        double quantity,
        std::optional<double> price,
        TimeInForce tif,
        PlaceOrderConfirmation confirmation);

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
