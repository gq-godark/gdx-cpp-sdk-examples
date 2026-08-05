# GoDark C++ SDK

This package provides the GoDark C++ SDK and minimal examples for encrypted
darkpool trading.

Supported order types in this distribution: `MARKET`, `LIMIT`.

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
- `GDX_NOISE_STATIC_PUBLIC_KEY` (64 hex chars; aliases `GDX_NOISE_STATIC_PUBKEY`, `GODARK_NOISE_STATIC_PUBLIC_KEY`)

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

## CMake integration (your own bot)

```cmake
find_package(godark REQUIRED)

add_executable(my_bot my_bot.cpp)
target_compile_features(my_bot PRIVATE cxx_std_20)
target_link_libraries(my_bot PRIVATE godark::godark)
```

See `SDK_REFERENCE.md` for full client API usage.
