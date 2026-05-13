# GoDark C++ SDK

This package provides the GoDark C++ SDK (prebuilt static library + headers
+ CMake package config) and minimal examples for encrypted darkpool trading.

Supported order types in this distribution: `MARKET`, `LIMIT`.

## Package contents

- `sdk/include/godark/` — public headers (`godark.hpp` + types/enums/errors)
- `sdk/lib/libgodark.a` — prebuilt static library (no private package registry required)
- `sdk/lib/cmake/godark/` — CMake package config (`find_package(godark)`)
- `examples/` — minimal usage examples (`quickstart.cpp`, `full_trader_example.cpp`)
- `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json` — top-level build glue
- `SDK_REFERENCE.md` — API reference
- `.env.example` — environment template

## 1) Prerequisites

- Linux x86_64
- CMake >= 3.25
- Ninja (or another CMake generator)
- C++20 toolchain (GCC >= 13 recommended)
- System dependencies: Boost (Beast/Asio/System), OpenSSL, Protobuf, nlohmann-json

Install dependencies:

```bash
sudo apt-get update
sudo apt-get install -y \
    libboost-dev libboost-system-dev libssl-dev \
    libprotobuf-dev protobuf-compiler nlohmann-json3-dev \
    ninja-build cmake
```

If you prefer [vcpkg](https://vcpkg.io/), the bundled `vcpkg.json` already lists
the required ports (`boost-system`, `boost-beast`, `boost-asio`, `boost-url`,
`openssl`, `protobuf`, `nlohmann-json`).

## 2) Create testnet credentials

1. Open frontend: `https://app.godark-dex.com`
2. Create an account using email.
3. Fund the account using faucet: `https://faucet.godark-dex.com`
4. Go to **Settings -> API Key Management** and create an API key.

## 3) Configure environment

Copy `.env.example` to `.env` and set:

- `GODARK_API_KEY_ID`
- `GODARK_API_SECRET`

```bash
cp .env.example .env
```

Optional override:

- `GODARK_EDGE_URL` — defaults to `wss://api.godark-dex.com` if unset.

## 4) Build the SDK + examples

The top-level `CMakeLists.txt` resolves `godark` from the vendored `sdk/`
install tree, so no separate SDK install step is required.

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

You can also use the preset:

```bash
cmake --preset release
cmake --build build -j
```

## 5) Run quickstart

```bash
./build/examples/quickstart
```

Or run the full trader example:

```bash
./build/examples/full_trader_example
```

## CMake integration (your own bot)

Point `CMAKE_PREFIX_PATH` at the vendored `sdk/` directory and link against
`godark::godark`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(my_bot LANGUAGES CXX)

list(PREPEND CMAKE_PREFIX_PATH "path/to/godark-cpp-sdk/sdk")
find_package(godark REQUIRED)

add_executable(my_bot my_bot.cpp)
target_compile_features(my_bot PRIVATE cxx_std_20)
target_link_libraries(my_bot PRIVATE godark::godark)
```

Then in `my_bot.cpp`:

```cpp
#include <godark/godark.hpp>
#include <cstdlib>

int main() {
    godark::ClientConfig cfg;
    cfg.api_key_id = std::getenv("GODARK_API_KEY_ID");
    cfg.api_secret = std::getenv("GODARK_API_SECRET");
    if (const char* url = std::getenv("GODARK_EDGE_URL")) {
        cfg.base_url = url;
    } else {
        cfg.base_url = "wss://api.godark-dex.com";
    }

    godark::GodarkClient client(cfg);
    client.connect();

    auto ack = client.place_order(
        "BTC-USDC-PERP",
        godark::Side::SELL,
        godark::OrderType::LIMIT,
        0.01,
        999'999.0);

    client.cancel_order(ack.order_id, "BTC-USDC-PERP");
    client.disconnect();
}
```

See `SDK_REFERENCE.md` for full client API usage.
