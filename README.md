# GoDark C++ Examples (Darkpool MM distribution)

This repository is a market-maker-facing distribution for GoDark's C++ SDK.
It includes:

- a vendored **prebuilt static library** (`sdk/lib/libgodark.a`) plus public headers and CMake package config under `sdk/include/godark/` and `sdk/lib/cmake/godark/` — **no private package registry required**; the bundle ships everything a consumer needs to `find_package(godark)` and link
- minimal darkpool trading examples (post-only **limit** orders priced from a live mark)
- a simple **`.env`** workflow (no shell `export` required)

The vendored `sdk/` is rebuilt and parity-checked against upstream
[`gq-godark/gdx-cpp-sdk`](https://github.com/gq-godark/gdx-cpp-sdk) on every CI
run; the exact upstream commit is recorded in `sdk/UPSTREAM_REF`. System deps
(`boost`, `openssl`, `protobuf`, `nlohmann-json`) install from apt or vcpkg as
usual — only the `godark` SDK itself comes entirely from this repo.

## Prerequisites

| Item | Requirement |
|------|-------------|
| OS | Linux x86_64 (matches published ZIPs; macOS / Windows untested) |
| Compiler | C++20 toolchain, **GCC ≥ 13** recommended |
| Build tools | **CMake ≥ 3.25**, Ninja (or another CMake generator) |
| System libs | Boost (Beast / Asio / System), OpenSSL **3.2+ with `openssl/hpke.h`** (`OSSL_HPKE_*`), Protobuf, nlohmann-json |

Install dependencies on Debian / Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake ninja-build \
    libboost-dev libboost-system-dev libssl-dev \
    libprotobuf-dev protobuf-compiler nlohmann-json3-dev
```

Prefer [vcpkg](https://vcpkg.io/)? The bundled `vcpkg.json` already lists the
required ports (`boost-system`, `boost-beast`, `boost-asio`, `boost-url`,
`openssl`, `protobuf`, `nlohmann-json`); set `CMAKE_TOOLCHAIN_FILE` to your
vcpkg toolchain and CMake will pick them up.

## Testnet onboarding

Before running the examples, complete this setup flow:

1. Open the testnet frontend: `https://app.godark-dex.com`
2. Create an account using email sign-up.
3. Fund your testnet account using the faucet: `https://faucet.godark-dex.com`
4. In the frontend, go to **Settings → API Key Management** and click **Create API Key**.
5. Use the generated key ID and secret for your local `.env`.

Encrypted trading links `libgodark.a` against **OpenSSL 3.2+** that ships
`openssl/hpke.h`. Ubuntu’s default `libssl-dev` often does not. Point CMake at
a build that does, for example
`cmake -B build -DOPENSSL_ROOT_DIR=$HOME/.local/openssl-3.3`. If link fails on
undefined `OSSL_HPKE_*`, install that OpenSSL before refreshing `sdk/`.

## Configure credentials

Copy `.env.example` to `.env` and fill in your API credentials:

```bash
cp .env.example .env
```

Required keys:

- `GODARK_API_KEY_ID`
- `GODARK_API_SECRET`
- `GODARK_PASSPHRASE` — required for API key-pair auth.

Optional:

- `GODARK_EDGE_URL` — override the edge URL (default: public testnet `wss://api.godark-dex.com` via the SDK Testnet environment preset). The SDK derives the REST host from this same URL.
- `GODARK_ACCOUNT` — canonical Solana account pubkey override for local
  fixtures; normal authentication returns the account automatically through
  `client.account()`.
- `GDX_HPKE_STATIC_PUBLIC_KEY` — sequencer HPKE static public key (64 hex). Required for **localnet/devnet** encrypted trading Aliases: `GDX_HPKE_STATIC_PUBKEY`, `GODARK_HPKE_STATIC_PUBLIC_KEY`, `VITE_GDX_HPKE_STATIC_PUBKEY`.
- `GODARK_TLS_SKIP_VERIFY` — set to `1` / `true` for dev TLS on `wss://`.

Legacy `GDX_*` names are accepted when the matching `GODARK_*` key is unset.

## Localnet (`gdx up`)

```bash
GODARK_EDGE_URL=ws://127.0.0.1:13300
GODARK_API_KEY=test-key-1
GDX_HPKE_STATIC_PUBLIC_KEY=1d61f116451fdfda1aa4aaf50b7200c3b362d0445bfa2d7ef1f80b3b8881a533
gdx fund 00000000-0000-4000-8000-000000000001
```

Copy `VITE_GDX_HPKE_STATIC_PUBKEY` from `gdx-web/.env.localnet` if your pin differs.

## Install

### From a released ZIP (recommended for MMs)

Download the latest `gdx-cpp-sdk-vX.Y.Z-build.N.zip` from the
[Releases tab](https://github.com/gq-godark/gdx-cpp-sdk-examples/releases) and
unzip it. The archive already contains a prebuilt `sdk/lib/libgodark.a`, vendored
headers, the CMake package config, the examples, and `README.md` +
`SDK_REFERENCE.md` at the archive root.

```bash
unzip gdx-cpp-sdk-vX.Y.Z-build.N.zip
cd gdx-cpp-sdk-vX.Y.Z-build.N
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/examples/quickstart
```

The top-level `CMakeLists.txt` prepends `sdk/` to `CMAKE_PREFIX_PATH` and runs
`find_package(godark REQUIRED)` so no separate SDK install step is needed.

### From a git clone (development)

```bash
git clone https://github.com/gq-godark/gdx-cpp-sdk-examples
cd gdx-cpp-sdk-examples
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/examples/quickstart
```

A preset is also available:

```bash
cmake --preset release
cmake --build build -j
```

## Participant walkthrough

Environment **names** (values stay in `.env`, never in source):

- `GODARK_API_KEY_ID`, `GODARK_API_SECRET`, `GODARK_PASSPHRASE`
- optional `GODARK_EDGE_URL`, `GODARK_REST_URL`, `GODARK_ACCOUNT`, `GDX_HPKE_STATIC_PUBLIC_KEY`, `GODARK_TLS_SKIP_VERIFY`

**REST auth.** `GodarkRestClient::connect()` posts `api_key_id`, `api_secret`, and `passphrase` to `POST /api/v1/auth/token` and keeps the returned `access_token`. Legacy `GODARK_API_KEY` (local `test-key-*`) is sent as-is.

**WebSocket login.** `GodarkClient::connect()` mints that same REST access token and logs in with it. The socket never carries `key_id:secret:passphrase`.

**Subscribe.** Trading `subscribe` / `unsubscribe` accept `orders`, `positions`, `volume`, `open_interest`, and `funding_rate`. An unknown channel throws immediately (`ConnectionError`) instead of waiting out the command timeout. `/ws/v1` does not serve trades or L2; order book and trades need the gomarket socket (`GODARK_MARKET_DATA_USE_GOMARKET=1` or `GODARK_MARKET_DATA_WS_URL`).

**Place.** Prices and sizes are `std::string` only (`"0.001"`, `"86705.5"`). There are no `double` overloads. The samples send a post-only limit at least 500 away from a live mark, size at most `0.001`, on the 0.5 tick. `slippage_bps` applies only to `MARKET` and `STOP_MARKET`. `PEG` is not post-only. The full WebSocket place also takes `aon`, `min_fill_size` (decimal string), and `expiry_time`.

**Client-order id.** It is registered only after a successful WebSocket place: `POST /api/v1/orders/_register_coid` with the header correlation id. The local map is updated only after HTTP 200. A 400 (or any non-2xx) is returned to the caller and the id is not stored. REST place forwards the id on the body and does **not** register it.

**Read a position.** After `subscribe({"positions"})`, use `on_position_update` / `try_recv_position()`, or `GodarkRestClient::get_positions()`.

**Cancel.** `cancel_order(order_id, symbol)` on either client, or `cancel_all_orders` on the WebSocket client.

## Examples

| Target | Source | Purpose |
|--------|--------|---------|
| `quickstart` | `examples/quickstart.cpp` | Minimal connect → `subscribe({"orders"})` → post-only LIMIT sell at least 500 above the live mark → cancel that order |
| `full_trader_example` | `examples/full_trader_example.cpp` | Primary WebSocket reference bot with callbacks, post-only place / modify / cancel, mass-quote / batch-cancel of its own orders, session summary |
| `full_trader_rest` | `examples/full_trader_rest.cpp` | REST auth, snapshots, and one post-only place / modify / cancel priced from the live mark |
| `rest_client_example` | `examples/rest_client_example.cpp` | Read-only REST: positions, open orders, account collateral, and public funding / open interest / volume |

Samples place **post-only `LIMIT`** orders only. They read a live mark (open-interest
notional/size, or a position snapshot that actually carries a mark) and exit
non-zero without placing if that mark is missing. The SDK also accepts `MARKET`.
See `bundle/SDK_REFERENCE.md` (shipped at the archive root as `SDK_REFERENCE.md`)
for the full API.

## Packaging for market makers

Create a clean distributable archive locally:

```bash
bash scripts/package.sh                        # gdx-cpp-sdk-examples.zip
bash scripts/package.sh my-release-name        # custom archive name stem
```

The script builds `libgodark.a` from `gq-godark/gdx-cpp-sdk` at exactly the SHA
recorded in `sdk/UPSTREAM_REF`, parity-checks it byte-for-byte against the
vendored `sdk/lib/libgodark.a` (and `diff -r` against the vendored headers /
CMake config), then stages the bundle and produces a `.zip`. A local edit to
`sdk/` **cannot** reach the released artifact — the artifact is built from the
pinned upstream tree.

CI runs the same script on every push to `main` and publishes a tagged GitHub
Release with the zip attached.

## Layout

| Path | Purpose |
|------|---------|
| `sdk/` | Vendored prebuilt SDK: `lib/libgodark.a` + `include/godark/*.hpp` + `lib/cmake/godark/*.cmake` |
| `sdk/UPSTREAM_REF` | Exact upstream git SHA used to produce `sdk/lib/libgodark.a` |
| `bundle/` | MM-facing docs (`README.md`, `SDK_REFERENCE.md`) shipped at the archive root |
| `examples/` | Example sources (`quickstart.cpp`, `full_trader_example.cpp`) + local `dotenv.hpp` helper |
| `.env.example` | Credential template copied to `.env` |
| `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json` | Build glue |
| `scripts/package.sh` | Maintainer / CI script: build from pin → parity check → zip |
| `scripts/refresh_sdk.sh` | Maintainer-only: rebuild `sdk/` from a sibling `gdx-cpp-sdk` checkout (never shipped) |
| `.github/workflows/release.yml` | CI/release pipeline (parity + smoke + tagged GitHub Release) |
| `CPP_AUTOMATION_PLAN.md` | Internal design doc for the multi-PR automation rollout |

## Refreshing `sdk/` (internal)

From a sibling development checkout of the upstream SDK:

```bash
git -C /path/to/gdx-cpp-sdk checkout <ref>
GDX_PROTO_ROOT=/path/to/gdx-proto \
  bash scripts/refresh_sdk.sh /path/to/gdx-cpp-sdk
git diff --stat -- sdk/
git add sdk/ && git commit -m "chore(sdk): bump pin to $(cut -c1-7 sdk/UPSTREAM_REF)"
```

The refresh script refuses to run against a dirty upstream worktree and builds
into a temp dir, so the upstream stays clean. The same script is invoked by
`.github/workflows/auto-bump-sdk-pin.yml` when `gq-godark/gdx-cpp-sdk` fires a
`gdx-sdk-changed` dispatch — a rolling PR (`auto/bump-sdk-pin`) is opened or
refreshed automatically, and merging it triggers a new tagged release.
