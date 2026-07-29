# GoDark C++ Examples (Darkpool MM distribution)

This repository is a market-maker-facing distribution for GoDark's C++ SDK.
It includes:

- a vendored **prebuilt static library** (`sdk/lib/libgodark.a`) plus public headers and CMake package config under `sdk/include/godark/` and `sdk/lib/cmake/godark/` — **no private package registry required**; the bundle ships everything a consumer needs to `find_package(godark)` and link
- minimal darkpool trading examples (**market** and **limit** orders only in the samples)
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
| System libs | Boost (Beast / Asio / System), OpenSSL, Protobuf, nlohmann-json |

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

- `GODARK_EDGE_URL` — local testing only; if unset, examples use `wss://api.godark-dex.com`. Encrypted REST trading is unsupported; all order flow goes over the WebSocket client.
- `GODARK_USER_UUID` — some local edges need an explicit UUID from auth.
- `GODARK_TLS_SKIP_VERIFY` — set to `1` / `true` for dev TLS on `wss://`.

Legacy `GDX_*` names are accepted when the matching `GODARK_*` key is unset.

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

## Examples

| Target | Source | Purpose |
|--------|--------|---------|
| `quickstart` | `examples/quickstart.cpp` | Minimal connect → place LIMIT sell far from touch → cancel |
| `full_trader_example` | `examples/full_trader_example.cpp` | Reference bot flow with callbacks for all sequencer push variants, place / modify / cancel / mass-quote / batch-cancel, session summary |

Order-type support in this MM distribution is limited to **`MARKET`** and
**`LIMIT`**. See `bundle/SDK_REFERENCE.md` (shipped at the archive root as
`SDK_REFERENCE.md`) for the full API.

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
git -C /path/to/gdx-cpp-sdk submodule update --init --recursive
bash scripts/refresh_sdk.sh /path/to/gdx-cpp-sdk
git diff --stat -- sdk/
git add sdk/ && git commit -m "chore(sdk): bump pin to $(cut -c1-7 sdk/UPSTREAM_REF)"
```

The refresh script refuses to run against a dirty upstream worktree and builds
into a temp dir, so the upstream stays clean. The same script is invoked by
`.github/workflows/auto-bump-sdk-pin.yml` when `gq-godark/gdx-cpp-sdk` fires a
`gdx-sdk-changed` dispatch — a rolling PR (`auto/bump-sdk-pin`) is opened or
refreshed automatically, and merging it triggers a new tagged release.
