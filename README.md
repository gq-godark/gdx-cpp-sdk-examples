# Godark C++ examples

Sample programs that consume the **`godark`** package from **[Conan 2](https://docs.conan.io/)** only. There is **no** Git or source dependency on any SDK source tree: configure a Conan remote (e.g. [Conan Cloud](https://conan.io/cloud) or Artifactory) that publishes **`godark/<version>@gq/stable`** and run `conan install` for the CMake toolchain.

## Prerequisites

- Conan 2 (`pip install "conan>=2.4"`)
- CMake ≥ 3.25, C++20 toolchain
- Profile: `conan profile detect` (or your team’s profile)
- Remote lists `godark` (see `conan search godark`)

## Build

From the **repository root**:

```bash
mkdir -p build && cd build
conan install .. --build=missing -s build_type=Release
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Binaries (all Conan / public API)

| Target | Source | What it does |
|--------|--------|--------------|
| `quickstart` | `examples/quickstart.cpp` | Minimal connect → limit sell → cancel. Needs `GODARK_API_KEY_ID` + `GODARK_API_SECRET`. |
| `e2e_trading_smoke` | `examples/e2e_trading_smoke.cpp` | Scripted E2E check with `--auth-only`; exit codes for CI. |
| `market_data_example` | `examples/market_data_example.cpp` | Public gomarket order book + trades (no keys). |
| `full_trader_example` | `examples/full_trader_example.cpp` | Larger demo: callbacks, MD client, place/modify/cancel, `try_recv_order`. |
| `full_trader_rest` | `examples/full_trader_rest.cpp` | REST-only `GodarkRestClient`: session + encrypted place + cancel (`GDX_REST_URL`, keys). |

### Environment quick reference

- **Trading (most WS examples):** `GODARK_API_KEY_ID`, `GODARK_API_SECRET`, optional `GODARK_EDGE_URL` / `GDX_EDGE_URL`.
- **REST (`full_trader_rest`):** `GDX_REST_URL`, `GDX_API_KEY_ID` / `GDX_API_SECRET` or `GDX_API_KEY` (see sample fallbacks).
- **Market data:** `GODARK_EDGE_URL` or `GDX_EDGE_URL`; optional `GDX_TLS_SKIP_VERIFY` / `GODARK_TLS_SKIP_VERIFY`.

## Layout

| Path | Purpose |
|------|---------|
| `conanfile.txt` | Requires `godark/0.1.0@gq/stable` + CMake generators |
| `CMakeLists.txt` | Project root: `find_package(godark)` + `add_subdirectory(examples)` |
| `examples/CMakeLists.txt` | Builds every sample executable linked to `godark::godark` |
| `examples/*.cpp` | Sources for each binary |

## `conanfile.txt`

Edit the `godark` version/channel to match what your Conan remote provides.
