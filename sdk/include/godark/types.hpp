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

/// Optional place-order flags mirrored from gdx-web / sequencer `PlaceOrderInput`.
struct PlaceOrderOptions {
    bool reduce_only = false;
    bool post_only = false;
    StpMode stp_mode = StpMode::Unspecified;
    std::optional<int32_t> peg_offset_bps = std::nullopt;
    std::optional<double> trigger_price = std::nullopt;
    std::optional<double> take_profit_price = std::nullopt;
    std::optional<double> stop_loss_price = std::nullopt;
};

/// Ack for account-wide `cancel_all`, `close_all`, or per-symbol `reverse`.
struct CountAck {
    std::string sequence;
    uint32_t count = 0;
    std::vector<std::string> order_ids;
    std::optional<uint32_t> error_code = std::nullopt;
    std::optional<std::string> reject_text = std::nullopt;
};

/// One cancel-replace leg of a mass quote. `cancel_order_id` 0/nullopt = pure
/// place; `time_in_force` defaults to "GTC"; `expiry_time` (ns) is required for GTD.
struct MassQuoteLegInput {
    std::string side;
    double price = 0.0;
    double quantity = 0.0;
    std::optional<uint64_t> cancel_order_id = std::nullopt;
    std::string time_in_force = "GTC";
    std::optional<uint64_t> expiry_time = std::nullopt;
};

/// One amend leg of a batch modify. At least one of new_price / new_quantity
/// must be set.
struct BatchModifyLegInput {
    uint64_t order_id = 0;
    std::optional<double> new_price = std::nullopt;
    std::optional<double> new_quantity = std::nullopt;
};

/// Outcome of one cancel-replace leg in a mass quote.
struct MassQuoteLegResult {
    uint32_t leg_index = 0;
    std::string status;  // "open" | "filled" | "failed" | "unspecified" | "unknown"
    std::optional<std::string> cancelled_order_id = std::nullopt;
    std::optional<std::string> new_order_id = std::nullopt;
    std::optional<uint32_t> error_code = std::nullopt;
    /// Number of taker fills this leg produced in relaxed (post_only=false)
    /// mode; 0 for a pure rest or a post-only leg.
    uint32_t fill_count = 0;
};

/// Batch-level result of a mass quote: one entry per submitted leg.
struct MassQuoteAck {
    /// Client-side convenience rollup: true only when `results` is non-empty and
    /// every leg has a non-"failed" status. This is a coarse summary — a batch
    /// can succeed at the wire while individual legs fail (e.g. a crossing leg
    /// in post-only mode). Always inspect per-leg `results` (status / error_code
    /// / fill_count) for the authoritative outcome.
    bool success = false;
    std::string sequence;
    std::vector<MassQuoteLegResult> results;
};

/// Outcome of cancelling one order id in a batch-cancel request.
struct BatchCancelLegResult {
    std::string order_id;
    bool cancelled = false;
    std::optional<uint32_t> error_code = std::nullopt;
};

/// Batch-level result of a batch cancel: one entry per submitted order id.
struct BatchCancelAck {
    /// Client-side convenience rollup: true only when `results` is non-empty and
    /// every id was cancelled. Note that an id which was not resting is an
    /// expected partial outcome (cancelled=false, error_code 2003) and will make
    /// this flag false even though the request itself was processed. Inspect
    /// per-leg `results` to distinguish partial from total failure.
    bool success = false;
    std::string sequence;
    std::vector<BatchCancelLegResult> results;
};

/// Outcome of amending one resting order in a batch-modify request.
struct BatchModifyLegResult {
    std::string order_id;
    bool modified = false;
    std::optional<uint32_t> error_code = std::nullopt;
};

/// Batch-level result of a batch modify: one entry per submitted leg.
struct BatchModifyAck {
    /// Client-side convenience rollup: true only when `results` is non-empty and
    /// every leg was modified. Expected partial outcomes — a leg whose amend
    /// would cross (modified=false, error_code 2018) or a missing order id
    /// (error_code 2003) — make this flag false. Inspect per-leg `results` for
    /// the authoritative outcome.
    bool success = false;
    std::string sequence;
    std::vector<BatchModifyLegResult> results;
};

/// RPC reply for amend / cancel TP-SL (`NodeResponse::tpsl_ack`).
struct TpslAck {
    std::vector<uint8_t> correlation_id;
    uint64_t parent_order_id = 0;
    std::optional<std::string> take_profit = std::nullopt;
    std::optional<std::string> stop_loss = std::nullopt;
    std::optional<uint32_t> error_code = std::nullopt;
    std::optional<std::string> reject_text = std::nullopt;
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
    bool reduce_only = false;
    bool post_only = false;
    /// Human-readable update/rejection text (`msg` / `reject_text` on wire).
    std::optional<std::string> msg = std::nullopt;
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
    std::string user_uuid;
    std::vector<LeverageSetting> settings;
    uint64_t server_timestamp = 0;
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
    /// Background periodic sweep (configurable cadence).
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

/// One resting order row inside an [`OpenOrdersSnapshot`].
struct OpenOrderRow {
    std::string order_id;
    uint64_t symbol_id = 0;
    uint32_t leverage = 1;
    std::string price;
    std::string quantity;
    std::string remaining_qty;
};

/// Encrypted `NodeResponse::OpenOrdersSnapshot` push (subscribe / UpdateLeverage refresh).
struct OpenOrdersSnapshot {
    std::vector<OpenOrderRow> rows;
    uint64_t server_timestamp = 0;
    int64_t correlation_id = 0;
};

/// Unified component health report routed via the trading WS.
struct SystemHealthUpdate {
    std::string component_id;
    int state = 0;
    bool serving = false;
    std::string cause;
    uint64_t updated_at_nanos = 0;
    uint64_t sequence = 0;
    uint32_t schema_version = 0;
};

/// Updated shielded balance for the authenticated user.
struct BalanceUpdate {
    std::string user_uuid;
    uint64_t shielded_balance_raw = 0;
    uint64_t timestamp = 0;
};

/// Authoritative account-level margin summary (decimal strings).
struct AccountMarginSummary {
    std::string total_collateral;
    std::string position_margin;
    std::string reserved_order_margin;
    std::string free_collateral;
    /// Isolated cash locks (no UPL).
    std::string isolated_margin;
    /// Isolated cash + isolated UPL, floored per position.
    std::string isolated_equity;
    /// Cross position IM (no order holds).
    std::string cross_im;
};

/// Account-margin push / GetAccount snapshot for a user.
struct AccountMarginUpdate {
    std::string user_uuid;
    uint64_t server_timestamp = 0;
    std::optional<AccountMarginSummary> account = std::nullopt;
};

/// Margin tier transition / recovery for `(owner, symbol_id)`.
struct MarginAlert {
    std::string owner;
    uint64_t symbol_id = 0;
    uint32_t tier = 0;
    uint32_t margin_ratio_bps = 0;
    std::string mark_price;
    std::string liquidation_price;
    int64_t ts = 0;
    uint64_t state_version = 0;
    /// True when the position recovered to `Healthy` — UI clears the tier
    /// badge for this `(owner, symbol_id)`.
    bool recovered = false;
};

/// Funding rate tick for a symbol.
struct FundingRateUpdate {
    uint64_t symbol_id = 0;
    /// In-progress hourly rate (TWAP / 8), decimal fraction.
    std::string funding_rate;
    uint64_t timestamp = 0;
    /// Last applied hourly rate, decimal fraction.
    std::string last_funding_rate;
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
