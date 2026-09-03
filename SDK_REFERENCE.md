# GoDark C++ SDK Reference (developer / maintainer)

This is the comprehensive reference for maintainers and developers working
*inside* this repository (writing or modifying examples, reviewing the
vendored `sdk/`, refreshing pins, etc.).

A trimmed, recipient-facing copy is maintained at
[`bundle/SDK_REFERENCE.md`](bundle/SDK_REFERENCE.md) and is the one copied
into the root of released ZIP bundles as `SDK_REFERENCE.md`. The bundle
version intentionally omits sections that recipients don't need (vendored
layout / pin discipline, refresh workflow, sourcing-from-git instructions,
ABI ownership notes).

> Scope: the MM examples use **WebSocket encrypted trading** via
> `godark::GodarkClient`. Encrypted REST trading is not supported — all order
> flow (place / modify / cancel / mass-quote) runs over the HPKE WebSocket
> client. Standalone market-data clients ship in the same library but are
> outside the bundled examples in this distribution. Order placement support
> is limited to `MARKET` and `LIMIT`.

## Quick Start

```cpp
#include <godark/godark.hpp>

godark::ClientConfig config;
config.api_key_id = "gdk_...";
config.api_secret = "...";
config.passphrase = "...";
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
- `GODARK_PASSPHRASE` (required for API key-pair auth)
- `GDX_HPKE_STATIC_PUBLIC_KEY` (required for encrypted WebSocket trading) — 64 hex chars; aliases `GDX_HPKE_STATIC_PUBKEY`, `GODARK_HPKE_STATIC_PUBLIC_KEY`
- `GODARK_EDGE_URL` (optional, defaults to `wss://api.godark-dex.com`)

Use `.env.example` as the template for your local `.env`.

## Adding `godark` to your project

In this repository, the example targets depend on the vendored static
library installed under `sdk/`. The recipe is two lines of CMake:

```cmake
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/sdk")
find_package(godark CONFIG REQUIRED)

add_executable(my_bot my_bot.cpp)
target_link_libraries(my_bot PRIVATE godark::godark)
```

Transitive runtime dependencies (Boost, OpenSSL, protobuf, nlohmann_json)
are **not** bundled; they are resolved by your project's package manager
(vcpkg, Conan, or system packages). The `sdk/lib/cmake/godark/` exports
declare which transitive targets the imported `godark::godark` target
expects, so they're picked up automatically when your toolchain provides
them.

To consume the SDK from your own project outside this repo, either:

1. Copy the vendored `sdk/` directory and depend on it via
   `CMAKE_PREFIX_PATH` (the same way this repo does), or
2. Build it from the upstream source pinned in
   [`sdk/UPSTREAM_REF`](sdk/UPSTREAM_REF):

   ```bash
   git clone --recurse-submodules https://github.com/gq-godark/gdx-cpp-sdk.git
   cd gdx-cpp-sdk
   git checkout <sha from sdk/UPSTREAM_REF>
   git submodule update --init --recursive
   cmake -S . -B build --preset release
   cmake --build build
   cmake --install build --prefix /your/install/prefix
   ```

   Then point `CMAKE_PREFIX_PATH` at `/your/install/prefix`.

The release bundle ships `libgodark.a` together with the same CMake
package config used internally, so no source build is required at the
consumer site.

## GodarkClient API

**Header:** `<godark/client.hpp>` (also re-exported via `<godark/godark.hpp>`)

### Core lifecycle

| Method | Signature | Purpose |
|--------|-----------|---------|
| `connect` | `void connect()` | Authenticate and establish HPKE WebSocket session |
| `disconnect` | `void disconnect()` | Graceful disconnect |
| `logout` | `void logout()` | Logout and disconnect |
| `is_connected` | `bool is_connected() const` | Connection state |
| `user_uuid` | `std::optional<std::string> user_uuid() const` | Authenticated user id |

### Trading commands

| Method | Signature | Purpose |
|--------|-----------|---------|
| `place_order` | `OrderAck place_order(symbol, side, order_type, quantity, price?, tif?)` | Place encrypted order |
| `update_leverage` | `OrderAck update_leverage(symbol, leverage)` | Set per-symbol account leverage |
| `cancel_order` | `OrderAck cancel_order(order_id, symbol)` | Cancel order |
| `modify_order` | `OrderAck modify_order(order_id, symbol, new_price?, new_quantity?, new_trigger_price?)` | Modify price, quantity, and/or stop trigger |
| `mass_quote` | `MassQuoteAck mass_quote(symbol, legs, post_only?)` | Bulk cancel-replace ladder (up to 20 legs) |
| `batch_cancel` | `BatchCancelAck batch_cancel(symbol, order_ids)` | Cancel multiple resting orders in one request |
| `batch_modify` | `BatchModifyAck batch_modify(symbol, legs)` | Amend multiple resting orders in one request |

### Streams

| Method | Signature | Purpose |
|--------|-----------|---------|
| `subscribe` | `void subscribe(channels)` | Subscribe to private channels (`orders`, `positions`) |
| `unsubscribe` | `void unsubscribe(channels)` | Unsubscribe |
| `try_recv_order` | `std::optional<OrderUpdate> try_recv_order()` | Non-blocking pull from order queue |
| `try_recv_position` | `std::optional<PositionUpdate> try_recv_position()` | Non-blocking pull from position queue |

### Callbacks

```cpp
client.on_order_update    = [](const godark::OrderUpdate& u)     {};
client.on_position_update = [](const godark::PositionUpdate& u)  {};
client.on_reconnect       = []()                                 {};
client.on_error           = [](const godark::Error& e)           {};
```

The C++ SDK uses **single-slot callback fields** — assigning a new lambda
replaces the previous one. To fan out to multiple subscribers, route through
your own dispatcher.

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
| `PositionsSnapshot`   | `rows[]` (`PositionRow{symbol_id, side, size, entry_price, mark_price, unrealized_pnl, ...}`), `source` (`Initial` / `Periodic` / `Event`) | Hydrate the open-positions table on connect; refresh every ~5s. |
| `SystemHealthUpdate`  | `total_nodes`, `ready`, `degraded`, `accepting_orders`                                          | Display node-cluster status; pause submissions if `accepting_orders == false`. |
| `BalanceUpdate`       | `shielded_balance_raw` (raw lamports-style integer)                                             | Refresh the wallet/equity widget after each fill or settlement. |
| `MarginAlert`         | `symbol_id`, `tier`, `margin_ratio_bps`, `liquidation_price_bps`, `recovered`                   | Show / clear the margin-tier banner per `(owner, symbol_id)`. |
| `FundingRateUpdate`   | `symbol_id`, `current_rate`, `predicted_rate`, `next_funding_time`                              | Update funding ticker / book metadata.               |
| `SettlementUpdate`    | `batch_id`, `status` (`Submitted` / `Confirmed` / `Failed`), `tx_signature`, `affected_user_uuids[]` | Reconcile settled batches, surface Solana tx links.  |

Each push has a single bounded queue (default 256). On overflow the oldest item
is dropped and an error is reported via `on_error`. Both the callback and the
matching `try_recv_*()` queue fire for the same item.

### Concurrency rule

**Single-flight commands**: `place_order`, `cancel_order`, and `modify_order`
each block until the exchange responds or `transport.command_timeout_sec`
expires. The transport maintains a single pending-command slot, so only one
command may be in-flight at a time. Call them sequentially from one thread.

Push streams above may be consumed concurrently from independent threads —
that's the intended pattern in `full_trader_example.cpp`.

### Ownership and movability

`godark::GodarkClient` and `godark::MarketDataClient` are **non-copyable**
*and* **non-movable** (move constructors/assignments are explicitly
deleted). The PIMPL holds a back-pointer to its owning client to dispatch
the public `on_*` callback fields from transport-owned threads (the
reconnect loop and encrypted-push handler); defaulting move would leave
that back-pointer aimed at the moved-from instance and break callback
delivery while worker threads are live. Keep client instances in
`std::unique_ptr` if you need to relocate ownership; never `std::move`
the client itself.

## Core Types

**Header:** `<godark/types.hpp>`

Wire decimals are exposed as **strings** on push types to preserve sequencer
precision. Command APIs use `double` / `std::optional<double>` where noted on
`place_order`.

### OrderAck

- `order_id` (`std::string`)
- `success` (`bool`)
- `sequence` (`std::string`)
- `error_code` (`std::optional<std::string>`) — symbolic code such as
  `"PRICE_DEVIATION_TOO_LARGE"` or `"MARGIN_INSUFFICIENT"`
- `error` (`std::optional<std::string>`) — human-readable message

### OrderUpdate

Includes order lifecycle fields such as:
`order_id`, `user_uuid`, `symbol_id`, `side`, `status`, `update_type`,
`price`, `quantity`, `filled_qty`, `remaining_qty`, `cum_fill`,
`cancel_reason`, `reject_reason`, `correlation_id`, `timestamp`.

### PositionUpdate

Per-fill delta. Use this stream to drive incremental P&L / position
accounting between `PositionsSnapshot` refreshes.

Includes position lifecycle fields such as:
`user_uuid`, `symbol_id`, `side`, `update_type`,
`size`, `entry_price`, `previous_size`, `fill_price`, `fill_qty`,
`correlation_id`, `timestamp`.

### PositionRow / PositionsSnapshot

`PositionsSnapshot` is the periodic/event-triggered authoritative view of
all open positions for the authenticated user. `rows` holds one
`PositionRow` per `(symbol_id, side)` pair, each with `size`,
`entry_price`, `leverage`, and (when fresh) `mark_price`,
`unrealized_pnl`, `notional`, `mark_publish_time_sec`.

`PositionsSnapshot` additionally carries a `source` (`Initial` /
`Periodic` / `Event`) and a `correlation_id`.

### Other push payloads

| Type | Notable fields |
|------|----------------|
| `SystemHealthUpdate` | `total_nodes`, `accepting_orders`, `ready`, `degraded`, `exhausted`, `warming`, `draining`, `waiting` |
| `BalanceUpdate` | `user_uuid`, `shielded_balance_raw`, `timestamp` |
| `MarginAlert` | `owner`, `symbol_id`, `tier`, `margin_ratio_bps`, `mark_price_bps`, `liquidation_price_bps`, `state_version`, `recovered`, `ts` |
| `FundingRateUpdate` | `symbol_id`, `current_rate`, `predicted_rate`, `next_funding_time`, `timestamp` |
| `SettlementUpdate` | `batch_id`, `status` (`SettlementBatchStatus`), `tx_signature`, `timestamp`, `affected_user_uuids` |

## Enums

**Header:** `<godark/enums.hpp>`

- `Side`: `BUY`, `SELL`
- `OrderType`: `MARKET`, `LIMIT`, `PEG`, `STOP_MARKET`, `STOP_LIMIT`
- `TimeInForce`: `GTC`, `IOC`, `FOK`, `GTD`
- `OrderStatus`: `NEW`, `PARTIALLY_FILLED`, `FILLED`, `CANCELLED`, `REJECTED`
- `OrderUpdateType`: `OPEN`, `FILLED`, `PARTIALLY_FILLED`, `CANCELLED`, `REJECTED`, `MODIFIED`, `CANCEL_REJECTED`, `MODIFY_REJECTED`
- `PositionUpdateType`: `SNAPSHOT`, `OPEN`, `INCREASE`, `DECREASE`, `CLOSE`, `FUNDING_APPLIED`
- `CancelReason`: `USER_REQUESTED`, `IOC_REMAINDER`, `FOK_NOT_FILLED`, `EXPIRED`, `SYSTEM`, `ADL`, `LIQUIDATED_CANCELED`, `MARGIN_CANCELED`, `REDUCE_ONLY`, `STP_EXPIRE_TAKER`, `STP_CANCEL_RESTING`
- `PositionsSnapshotSource`: `Unspecified`, `Initial`, `Periodic`, `Event`
- `SettlementBatchStatus`: `Unspecified`, `Submitted`, `Confirmed`, `Failed`

All enums provide string conversion helpers via `to_string(...)`.

`PlaceOrderOptions` on `place_order` includes `peg_offset_bps`, `trigger_price`, `take_profit_price`, and `stop_loss_price`. `PEG` pegs to the Pyth oracle mark.

Note: the SDK enum includes additional order types for compatibility, but this
MM distribution supports placing only `MARKET` and `LIMIT` orders.

## Errors

**Header:** `<godark/errors.hpp>`

All SDK exceptions inherit from `godark::Error` (which itself inherits from
`std::runtime_error`):

| Type | When |
|------|------|
| `AuthenticationError` | API key rejection at session bring-up |
| `SessionError` | HPKE setup handshake or rekey failure |
| `OrderError` | Order rejected; carries `std::optional<std::string> error_code` with the symbolic reason |
| `ConnectionError` | Transport-level disconnect or failure |
| `EncryptionError` | Cipher / nonce failure on encrypted payloads |
| `TimeoutError` | Per-command response timeout |

The `OrderError` variant is the one application code typically branches on:

```cpp
try {
    auto ack = client.place_order(...);
    if (!ack.success) {
        std::cerr << "rejected: " << ack.error.value_or("?")
                  << " (code=" << ack.error_code.value_or("?") << ")\n";
    }
} catch (const godark::OrderError& e) {
    std::cerr << "rejected: " << e.what()
              << " (code=" << e.error_code.value_or("?") << ")\n";
}
```

See `quickstart.cpp` and `full_trader_example.cpp` for the full try / catch
pattern.

## Example files in this distribution

| File | Purpose |
|------|---------|
| `examples/quickstart.cpp` | Minimal connect, place, cancel |
| `examples/full_trader_example.cpp` | Reference bot flow: callbacks, place / modify / cancel, mass-quote / batch-cancel, session summary |
| `examples/dotenv.hpp` | Header-only `.env` loader used by both examples |

## CMake integration

```cmake
list(PREPEND CMAKE_PREFIX_PATH "path/to/godark-cpp-sdk/sdk")
find_package(godark REQUIRED)

add_executable(my_bot my_bot.cpp)
target_compile_features(my_bot PRIVATE cxx_std_20)
target_link_libraries(my_bot PRIVATE godark::godark)
```

```cpp
#include <godark/godark.hpp>
```

## Vendored layout (`sdk/`)

The `godark` SDK is vendored as a prebuilt static library plus public
headers and CMake package config under `sdk/`:

```text
sdk/
├── UPSTREAM_REF                 # exact upstream commit SHA the install was cut from
├── include/
│   └── godark/                  # public headers (client.hpp, types.hpp, enums.hpp, …)
└── lib/
    ├── libgodark.a              # static archive (ABI surface)
    └── cmake/godark/
        ├── godark-config.cmake          # find_package entry point
        ├── godark-config-version.cmake  # version compatibility check
        ├── godark-targets.cmake         # imported target declaration
        └── godark-targets-release.cmake # imported location (libgodark.a path)
```

The release bundle ships exactly this layout. There is no consumer-side
build of the SDK itself — only the example targets (and your own bot) link
against the prebuilt archive.

The `vcpkg.json` and top-level `CMakeLists.txt` at the repo root drive the
example build only; they don't rebuild `sdk/`.

## Refreshing the vendored SDK

Maintainers refresh `sdk/` from a sibling `gdx-cpp-sdk` checkout:

```bash
./scripts/refresh_sdk.sh /path/to/gdx-cpp-sdk
```

The script:

1. Refuses to run if the upstream worktree is dirty (so the recorded SHA
   matches reality).
2. Builds the SDK in a temporary directory at the upstream `HEAD`.
3. Installs into `sdk/` (`include/`, `lib/libgodark.a`, `lib/cmake/godark/`).
4. Validates the install layout (every expected file present).
5. Writes the upstream HEAD SHA into `sdk/UPSTREAM_REF`.

After running it, `scripts/package.sh` performs a two-tier parity check
between the vendored `sdk/` and a fresh build at the pinned SHA:

- **Tier 1 (byte parity)**: `libgodark.a` (the actual archive linker
  sees) and `include/` (the headers the compiler sees) must match
  byte-for-byte. Any drift here means the vendored copy could ship a
  different ABI or API than the recorded pin.
- **Tier 2 (structural parity)**: `lib/cmake/godark/` files must declare
  the expected `godark::godark` imported target and reference
  `libgodark.a`. The exact bytes vary across cmake patch versions
  without affecting the import contract, so byte differences here are
  reported as a non-fatal note rather than failing the release.

The release bundle copies `lib/cmake/` from a fresh upstream install
(not from `sdk/`), so vendored drift in those files cannot reach the
shipped artifact even if the cmake-version flakiness sneaks past the
sanity check.

Layer 2 automation (`auto-bump-sdk-pin.yml`) wraps this loop into a
rolling auto-PR triggered by `gdx-cpp-sdk` pushes to `main`, using the
same `ubuntu-24.04` runner as `release.yml` so byte parity is
deterministic across CI runs.

## RestClient

**Header:** `<godark/rest_client.hpp>`

`GodarkRestClient` handles REST auth and encrypted snapshot reads. Order flow (place / modify / cancel) remains WebSocket-only via `GodarkClient`.

`rest_client_example` covers auth, `/auth/me`, leverage read, and public funding/OI/volume GETs. `full_trader_rest` adds encrypted snapshot reads and REST trading.

### Account info

Do **not** send raw WebSocket `account.info` — the edge rejects it as an unknown op.

Use `GodarkRestClient::get_account()` instead:

| Method | Path | `request_type` | Reply |
|---|---|---|---|
| `get_account()` | `POST /api/v1/account` | `get_account` | `AccountMarginUpdate` (`account_margin_update`) |

Related snapshot reads:

| Method | Path | `request_type` | Reply |
|---|---|---|---|
| `get_open_orders()` | `POST /api/v1/openOrders` | `get_open_orders` | `OpenOrdersSnapshot` |
| `get_positions()` | `POST /api/v1/positions` | `get_positions` | `PositionsSnapshot` |
