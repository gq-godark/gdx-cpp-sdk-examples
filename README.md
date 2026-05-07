# GoDark C++ Examples (Darkpool MM Distribution)

This repository is a market-maker-facing distribution for GoDark's C++ SDK.
It includes:

- a prebuilt SDK (`sdk/lib/libgodark.a`, headers, and CMake package config)
- minimal darkpool trading examples (market and limit order support)
- a simple `.env` workflow (no shell `export` required)

## Prerequisites

- Linux x86_64
- CMake >= 3.25
- Ninja (or another CMake generator)
- C++20 toolchain (GCC >= 13 recommended)
- system dependencies:

```bash
sudo apt-get install -y \
    libboost-dev libboost-system-dev libssl-dev \
    libprotobuf-dev protobuf-compiler nlohmann-json3-dev ninja-build
```

## Configure credentials

Copy `.env.example` to `.env` and fill in your API credentials:

```bash
cp .env.example .env
```

Required keys:

- `GODARK_API_KEY_ID`
- `GODARK_API_SECRET`

Optional:

- `GODARK_EDGE_URL` (for local testing). If unset, examples use `wss://api.godark-dex.com`.

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

You can also use the preset:

```bash
cmake --preset release
cmake --build build
```

## Examples

| Target | Source | Purpose |
|--------|--------|---------|
| `quickstart` | `examples/quickstart.cpp` | Minimal connect -> place limit sell -> cancel |
| `full_trader_example` | `examples/full_trader_example.cpp` | Full darkpool trading flow with callbacks, subscribe, place/modify/cancel |

Order-type support in this MM distribution is limited to `MARKET` and `LIMIT`.

## Packaging for Market Makers

Create a clean distributable archive:

```bash
./scripts/package.sh
```

This creates `godark-cpp-sdk.tar.gz` and includes:

- `sdk/`
- `examples/`
- `README.md`
- `SDK_REFERENCE.md`
- `.env.example`
- CMake files (`CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`)

Internal files like `scripts/`, `.git/`, build artifacts, and local `.env` are not included.

## Layout

| Path | Purpose |
|------|---------|
| `sdk/` | Prebuilt SDK: static library, headers, CMake package config |
| `examples/` | Example source files |
| `.env.example` | Credential template for local `.env` |
| `SDK_REFERENCE.md` | API usage reference for trading integration |
| `scripts/refresh_sdk.sh` | Internal-only SDK refresh script (not shipped in tar) |
