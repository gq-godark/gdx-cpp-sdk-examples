#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <godark/enums.hpp>

namespace godark {

struct OrderAck {
    std::string order_id;
    bool success;
    std::string sequence;
    std::optional<std::string> error_code = std::nullopt;
    std::optional<std::string> error = std::nullopt;
};

struct OrderUpdate {
    std::string order_id;
    std::string user_uuid;
    int64_t symbol_id;
    Side side;
    OrderStatus status;
    OrderUpdateType update_type;
    std::string price;
    std::string quantity;
    std::string filled_qty;
    std::string remaining_qty;
    std::string cum_fill;
    std::optional<CancelReason> cancel_reason = std::nullopt;
    std::optional<int64_t> reject_reason_code = std::nullopt;
    int64_t correlation_id = 0;
    int64_t timestamp = 0;
};

struct PositionUpdate {
    std::string user_uuid;
    int64_t symbol_id;
    Side side;
    PositionUpdateType update_type;
    std::string size;
    std::string entry_price;
    std::string previous_size = "0";
    std::string fill_price = "0";
    std::string fill_qty = "0";
    int64_t correlation_id = 0;
    int64_t timestamp = 0;
};

struct LeverageSetting {
    uint64_t symbol_id = 0;
    uint32_t leverage = 1;
};

struct LeverageSettings {
    std::vector<LeverageSetting> settings;
};

struct MeProfile {
    std::string id;
    std::string dynamic_user_id;
    std::string email;
    std::string wallet_address;
    std::string referral_code;
    std::string tier;
};

struct Balance {
    uint64_t wallet_usdt_raw = 0;
    uint64_t pending_deposits_raw = 0;
    uint64_t shielded_balance_raw = 0;
    double wallet_usdt_ui = 0.0;
};

// ---------------------------------------------------------------------------
// Sequencer push types — mirrors `gdx.sequencer.v1.SequencerToEdgeMessage`
// `oneof inner` arms beyond order/position. Surfaced via the corresponding
// `on_*` callbacks and `try_recv_*()` queues on `GodarkClient`.
// ---------------------------------------------------------------------------

/// Why a [`PositionsSnapshot`] was emitted.
/// Mirrors `gdx.common.v1.PositionsSnapshotSource`.
enum class PositionsSnapshotSource : int {
    Unspecified = 0,
    /// First snapshot after `SubscribePositions`.
    Initial = 1,
    /// Background periodic sweep (default 5s cadence).
    Periodic = 2,
    /// Position-changing fill / flip / close (debounced on the sequencer).
    Event = 3,
};

/// One row of a [`PositionsSnapshot`] — a single open position w/ mark.
struct PositionRow {
    uint64_t symbol_id;
    Side side;
    std::string size;
    std::string entry_price;
    uint32_t leverage = 1;
    /// Server-computed mark price (Pyth Hermes). Empty optional if no Pyth
    /// tick has been observed yet for this symbol.
    std::optional<std::string> mark_price = std::nullopt;
    std::optional<std::string> unrealized_pnl = std::nullopt;
    std::optional<std::string> notional = std::nullopt;
    std::optional<uint64_t> mark_publish_time_sec = std::nullopt;
};

/// Full per-user positions batch (initial / periodic / event-triggered).
struct PositionsSnapshot {
    std::string user_uuid;
    std::vector<PositionRow> rows;
    /// Sequencer wall-clock (ns) when the batch was assembled.
    uint64_t server_timestamp = 0;
    PositionsSnapshotSource source = PositionsSnapshotSource::Unspecified;
    /// Echoed from the original `SubscribePositions` request — present on
    /// `Initial` snapshots only.
    std::optional<int64_t> correlation_id = std::nullopt;
};

/// Sequencer / MPC node health pulse routed via the trading WS.
struct SystemHealthUpdate {
    uint32_t total_nodes = 0;
    bool accepting_orders = false;
    uint32_t ready = 0;
    uint32_t degraded = 0;
    uint32_t exhausted = 0;
    uint32_t warming = 0;
    uint32_t draining = 0;
    uint32_t waiting = 0;
};

/// Updated shielded balance for the authenticated user.
struct BalanceUpdate {
    std::string user_uuid;
    uint64_t shielded_balance_raw = 0;
    uint64_t timestamp = 0;
};

/// Margin tier transition / recovery for `(owner, symbol_id)`.
struct MarginAlert {
    std::string owner;
    uint64_t symbol_id = 0;
    uint32_t tier = 0;
    uint32_t margin_ratio_bps = 0;
    uint64_t mark_price_bps = 0;
    uint64_t liquidation_price_bps = 0;
    int64_t ts = 0;
    uint64_t state_version = 0;
    /// True when the position recovered to `Healthy` — UI clears the tier
    /// badge for this `(owner, symbol_id)`.
    bool recovered = false;
};

/// Funding rate tick for a symbol.
struct FundingRateUpdate {
    uint64_t symbol_id = 0;
    std::string current_rate;
    std::string predicted_rate;
    uint64_t next_funding_time = 0;
    uint64_t timestamp = 0;
};

/// Status of a settlement batch tx.
/// Mirrors `gdx.sequencer.v1.SettlementBatchStatus`.
enum class SettlementBatchStatus : int {
    Unspecified = 0,
    Submitted = 1,
    Confirmed = 2,
    Failed = 3,
};

/// Settlement batch lifecycle update from the sequencer.
struct SettlementUpdate {
    uint64_t batch_id = 0;
    SettlementBatchStatus status = SettlementBatchStatus::Unspecified;
    std::string tx_signature;
    uint64_t timestamp = 0;
    std::vector<std::string> affected_user_uuids;
};

} // namespace godark
