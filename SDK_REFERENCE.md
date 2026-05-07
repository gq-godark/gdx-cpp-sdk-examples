# GoDark C++ SDK Reference (MM Distribution)

This reference describes the API and workflow used by the market-maker-facing
distribution in this repository.

The MM examples use WebSocket encrypted trading via `godark::GodarkClient`.
REST and standalone market-data examples are intentionally excluded from this
distribution.

Order placement support in this MM distribution is limited to `MARKET` and
`LIMIT`.

## Quick Start

```cpp
#include <godark/godark.hpp>

godark::ClientConfig config;
config.api_key_id = "gdk_...";
config.api_secret = "...";
config.base_url   = "wss://api.godark-dex.com"; // optional override

godark::GodarkClient client(config);
client.connect();

auto ack = client.place_order(
    "BTC-USDC-PERP", godark::Side::SELL, godark::OrderType::LIMIT, 0.01, 999999.0);

client.cancel_order(ack.order_id, "BTC-USDC-PERP");
client.disconnect();
```

## Configuration

The MM examples expect:

- `GODARK_API_KEY_ID` (required)
- `GODARK_API_SECRET` (required)
- `GODARK_EDGE_URL` (optional, defaults to `wss://api.godark-dex.com`)

Use `.env.example` as the template for your local `.env`.

## GodarkClient API

**Header:** `<godark/client.hpp>`

### Core lifecycle

| Method | Signature | Purpose |
|--------|-----------|---------|
| `connect` | `void connect()` | Authenticate and establish encrypted session |
| `disconnect` | `void disconnect()` | Graceful disconnect |
| `logout` | `void logout()` | Logout and disconnect |
| `is_connected` | `bool is_connected() const` | Connection state |
| `user_uuid` | `std::optional<std::string> user_uuid() const` | Authenticated user id |

### Trading commands

| Method | Signature | Purpose |
|--------|-----------|---------|
| `place_order` | `OrderAck place_order(symbol, side, order_type, quantity, price?, tif?)` | Place encrypted order |
| `cancel_order` | `OrderAck cancel_order(order_id, symbol)` | Cancel order |
| `modify_order` | `OrderAck modify_order(order_id, symbol, new_price?, new_quantity?)` | Modify order |

### Streams

| Method | Signature | Purpose |
|--------|-----------|---------|
| `subscribe` | `void subscribe(channels)` | Subscribe to private channels (`orders`, `positions`) |
| `unsubscribe` | `void unsubscribe(channels)` | Unsubscribe |
| `try_recv_order` | `std::optional<OrderUpdate> try_recv_order()` | Non-blocking pull from order queue |
| `try_recv_position` | `std::optional<PositionUpdate> try_recv_position()` | Non-blocking pull from position queue |

### Callbacks

```cpp
client.on_order_update = [](const godark::OrderUpdate& u) {};
client.on_position_update = [](const godark::PositionUpdate& u) {};
client.on_reconnect = []() {};
client.on_error = [](const godark::Error& e) {};
```

In addition, the SDK surfaces every other push the sequencer can emit on the
trading WebSocket. Each one has a matching `on_*` callback **and** a
non-blocking `try_recv_*()` queue (return type `std::optional<T>`):

```cpp
client.on_positions_snapshot   = [](const godark::PositionsSnapshot& s)   {};
client.on_system_health        = [](const godark::SystemHealthUpdate& h)  {};
client.on_balance_update       = [](const godark::BalanceUpdate& b)       {};
client.on_margin_alert         = [](const godark::MarginAlert& a)         {};
client.on_funding_rate_update  = [](const godark::FundingRateUpdate& f)   {};
client.on_settlement_update    = [](const godark::SettlementUpdate& s)    {};
```

| Push                  | Field highlights                                                                                | Typical use                                          |
|-----------------------|-------------------------------------------------------------------------------------------------|------------------------------------------------------|
| `PositionsSnapshot`   | `rows[]` (`PositionRow{symbol_id, side, size, entry_price, mark_price, unrealized_pnl, ...}`), `source` (Initial / Periodic / Event) | Hydrate the open-positions table on connect; refresh every ~5s. |
| `SystemHealthUpdate`  | `total_nodes`, `ready`, `degraded`, `accepting_orders`                                          | Display node-cluster status; pause submissions if `accepting_orders == false`. |
| `BalanceUpdate`       | `shielded_balance_raw` (raw lamports-style integer)                                             | Refresh the wallet/equity widget after each fill or settlement. |
| `MarginAlert`         | `symbol_id`, `tier`, `margin_ratio_bps`, `liquidation_price_bps`, `recovered`                   | Show / clear the margin-tier banner per `(owner, symbol_id)`. |
| `FundingRateUpdate`   | `symbol_id`, `current_rate`, `predicted_rate`, `next_funding_time`                              | Update funding ticker / book metadata.               |
| `SettlementUpdate`    | `batch_id`, `status` (Submitted / Confirmed / Failed), `tx_signature`, `affected_user_uuids[]`  | Reconcile settled batches, surface Solana tx links.  |

### Concurrency rule

Only one command (`place_order`, `cancel_order`, `modify_order`) should be in
flight at a time. Call these sequentially.

## Core Types

**Header:** `<godark/types.hpp>`

### OrderAck

- `order_id` (`std::string`)
- `success` (`bool`)
- `sequence` (`std::string`)
- `error_code` (`std::optional<std::string>`)
- `error` (`std::optional<std::string>`)

### OrderUpdate

Includes order lifecycle fields such as:
`order_id`, `symbol_id`, `side`, `status`, `update_type`,
`price`, `quantity`, `filled_qty`, `remaining_qty`, `timestamp`.

### PositionUpdate

Includes position lifecycle fields such as:
`user_uuid`, `symbol_id`, `side`, `update_type`,
`size`, `entry_price`, `fill_price`, `fill_qty`, `timestamp`.

## Enums

**Header:** `<godark/enums.hpp>`

Important enums used in MM examples:

- `Side`: `BUY`, `SELL`
- `OrderType`: `MARKET`, `LIMIT`, `PEG_TO_MID`, `PEG_TO_BID`, `PEG_TO_ASK`
- `TimeInForce`: `GTC`, `IOC`, `FOK`, `GTD`
- `OrderStatus`: `NEW`, `PARTIALLY_FILLED`, `FILLED`, `CANCELLED`, `REJECTED`
- `OrderUpdateType`: `OPEN`, `FILLED`, `PARTIALLY_FILLED`, `CANCELLED`, `REJECTED`, `MODIFIED`, `CANCEL_REJECTED`, `MODIFY_REJECTED`
- `PositionUpdateType`: `SNAPSHOT`, `OPEN`, `INCREASE`, `DECREASE`, `CLOSE`
- `CancelReason`: `USER_REQUESTED`, `IOC_REMAINDER`, `FOK_NOT_FILLED`, `EXPIRED`, `SYSTEM`

All enums provide string conversion helpers via `to_string(...)`.

Note: the SDK enum includes additional order types for compatibility, but this
MM distribution supports placing only `MARKET` and `LIMIT` orders.

## Errors

**Header:** `<godark/errors.hpp>`

All SDK exceptions inherit from `godark::Error`:

- `AuthenticationError`
- `SessionError`
- `OrderError` — also carries `std::optional<std::string> error_code` with the
  symbolic reason (e.g. `"PRICE_DEVIATION_TOO_LARGE"`, `"MARGIN_INSUFFICIENT"`).
  See `quickstart.cpp` for the catch-and-print pattern.
- `ConnectionError`
- `EncryptionError`
- `TimeoutError`

## Example files in this distribution

| File | Purpose |
|------|---------|
| `examples/quickstart.cpp` | Minimal connect, place, cancel |
| `examples/full_trader_example.cpp` | Reference bot flow with callbacks and order lifecycle |

## CMake integration

```cmake
find_package(godark REQUIRED)

add_executable(my_bot my_bot.cpp)
target_compile_features(my_bot PRIVATE cxx_std_20)
target_link_libraries(my_bot PRIVATE godark::godark)
```
