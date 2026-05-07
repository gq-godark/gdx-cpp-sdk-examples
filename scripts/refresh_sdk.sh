#!/usr/bin/env bash
set -euo pipefail

SDK_SRC="${1:?Usage: refresh_sdk.sh <path-to-gdx-cpp-sdk>}"
INSTALL_DIR="$(cd "$(dirname "$0")/.." && pwd)/sdk"

rm -rf "$INSTALL_DIR"

cmake -S "$SDK_SRC" -B "$SDK_SRC/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DGODARK_BUILD_TESTS=OFF \
    -DGODARK_BUILD_EXAMPLES=OFF

cmake --build "$SDK_SRC/build" -j"$(nproc)"
cmake --install "$SDK_SRC/build" --prefix "$INSTALL_DIR"

echo "SDK installed to $INSTALL_DIR"
echo "Commit the updated sdk/ directory."
