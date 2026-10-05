# GoDark C++ SDK

This package provides the GoDark C++ SDK and minimal examples for encrypted
darkpool trading.

Samples place post-only `LIMIT` orders priced from a live mark (size at most `0.001`). The SDK also accepts `MARKET` and `LIMIT`.

## Package contents

- `sdk/` — headers, static library, and CMake config
- `examples/` — minimal usage examples
- `SDK_REFERENCE.md` — API reference
- `.env.example` — environment template
- CMake files (`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`)

## 1) Prerequisites

- Linux x86_64
- CMake >= 3.25
- Ninja (or another CMake generator)
- C++20 toolchain (GCC >= 13 recommended)

Install dependencies:

```bash
sudo apt-get install -y \
    libboost-dev libboost-system-dev libssl-dev \
    libprotobuf-dev protobuf-compiler nlohmann-json3-dev ninja-build
```

`libgodark.a` needs **OpenSSL 3.2+ with `openssl/hpke.h`**. If `libssl-dev` has no HPKE headers, configure with `-DOPENSSL_ROOT_DIR` pointing at an OpenSSL build that does.

## 2) Create testnet credentials

1. Open frontend: `https://app.godark-dex.com`
2. Create an account using email.
3. Fund the account using faucet: `https://faucet.godark-dex.com`
4. Go to **Settings -> API Key Management** and create an API key.

## 3) Configure environment

Copy `.env.example` to `.env` and set:

- `GODARK_API_KEY_ID`
- `GODARK_API_SECRET`
- `GODARK_PASSPHRASE`

Public testnet needs only the three credential keys above for hosted testnet; localnet/devnet also require `GDX_HPKE_STATIC_PUBLIC_KEY`.

Optional:

- `GODARK_EDGE_URL` — override the edge URL.
- `GODARK_ACCOUNT` — canonical Solana account pubkey override for local
  fixtures; normal authentication returns it through `client.account()`.
- `GDX_HPKE_STATIC_PUBLIC_KEY` — override the sequencer HPKE pin (**not required for testnet**). Aliases: `GDX_HPKE_STATIC_PUBKEY`, `GODARK_HPKE_STATIC_PUBLIC_KEY`.

```bash
cp .env.example .env
```

## 4) Build examples

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Or with preset:

```bash
cmake --preset release
cmake --build build
```

## 5) Run quickstart

```bash
./build/examples/quickstart
```

See the repository `README.md` participant walkthrough: REST `POST /api/v1/auth/token`, WebSocket login with that `access_token` (not `key_id:secret:passphrase`), subscribe to `orders`, `positions`, `volume`, `open_interest`, `funding_rate` (unknown channel throws immediately; no trades or L2 on `/ws/v1`), string prices and sizes, then read a position and cancel.

`slippage_bps` is only for `MARKET` and `STOP_MARKET`. `PEG` is not post-only. WebSocket place also accepts `aon`, `min_fill_size` (string), and `expiry_time`. A client-order id is registered only after a successful WebSocket place; the local map updates only on HTTP 200, a 400 is returned to the caller, and REST place does not register it.

## CMake integration (your own bot)

```cmake
find_package(godark REQUIRED)

add_executable(my_bot my_bot.cpp)
target_compile_features(my_bot PRIVATE cxx_std_20)
target_link_libraries(my_bot PRIVATE godark::godark)
```

See `SDK_REFERENCE.md` for full client API usage.
