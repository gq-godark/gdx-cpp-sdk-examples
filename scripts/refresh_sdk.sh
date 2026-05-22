#!/usr/bin/env bash
# Refresh sdk/ (libgodark.a + headers + cmake config) from a sibling
# gdx-cpp-sdk checkout, AND record the upstream commit in sdk/UPSTREAM_REF
# so the release pipeline (scripts/package.sh and the GitHub Actions
# workflow) can verify the vendored copy hasn't drifted from upstream.
#
# Upstream repo: gq-godark/gdx-cpp-sdk (standalone). The CMakeLists.txt and
# include/godark/ live at the repo root, not under a cpp/ sub-tree.
#
# Usage:
#   ./scripts/refresh_sdk.sh /path/to/gdx-cpp-sdk
#
# Notes:
#   * Refuses to run against a dirty upstream worktree: a pin recorded
#     against uncommitted state is not reproducible and would fail CI's
#     parity check for unexplainable reasons.
#   * Builds into a temp dir so the upstream worktree stays clean.
#   * The gdx-proto submodule must be initialized in the upstream checkout
#     (CMake reads .proto files from there).
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/gdx-cpp-sdk" >&2
  exit 1
fi

SRC_ARG="$1"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DEST_SDK="$REPO_ROOT/sdk"

if [[ ! -d "$SRC_ARG" ]]; then
  echo "error: source directory '$SRC_ARG' does not exist" >&2
  exit 1
fi

SRC="$(cd "$SRC_ARG" && pwd)"

# Sanity: $SRC must look like gdx-cpp-sdk (CMakeLists.txt at root declaring
# project(godark-cpp ...)).
if [[ ! -f "$SRC/CMakeLists.txt" ]] || ! grep -q '^project(godark-cpp' "$SRC/CMakeLists.txt"; then
  echo "error: '$SRC' does not look like gdx-cpp-sdk" >&2
  echo "       expected: CMakeLists.txt at the root declaring 'project(godark-cpp ...)'" >&2
  exit 1
fi

if [[ ! -d "$SRC/.git" ]]; then
  echo "error: '$SRC' is not a git checkout — pin cannot be recorded" >&2
  exit 1
fi

# The build needs gdx-proto present (CMakeLists pulls .proto files from
# the gdx-proto submodule). Fail early with a clear message instead of
# letting CMake produce a confusing error later.
if [[ ! -d "$SRC/gdx-proto/proto" ]]; then
  echo "error: '$SRC/gdx-proto/proto' missing — initialize the gdx-proto submodule first:" >&2
  echo "       git -C \"$SRC\" submodule update --init --recursive" >&2
  exit 1
fi

# Refuse to refresh from a dirty upstream worktree — the pin would not be
# reproducible and the parity check would fail in CI for nobody-can-explain
# reasons.
if ! git -C "$SRC" diff --quiet || ! git -C "$SRC" diff --cached --quiet; then
  echo "error: upstream '$SRC' has uncommitted changes; commit or stash first:" >&2
  git -C "$SRC" status --short >&2 || true
  exit 1
fi

# Untracked files inside the upstream worktree would make the build
# non-reproducible.
DIRTY_PATHS="$(git -C "$SRC" ls-files --others --exclude-standard 2>/dev/null || true)"
if [[ -n "$DIRTY_PATHS" ]]; then
  echo "error: upstream '$SRC' has untracked files that would affect the build:" >&2
  printf '  %s\n' $DIRTY_PATHS >&2
  echo "commit, remove, or .gitignore them in upstream first." >&2
  exit 1
fi

UPSTREAM_SHA="$(git -C "$SRC" rev-parse HEAD)"
UPSTREAM_TAG="$(git -C "$SRC" describe --tags --exact-match HEAD 2>/dev/null || true)"

for tool in cmake ninja; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: required build tool '$tool' not found in PATH" >&2
    exit 1
  fi
done

echo "Refreshing $DEST_SDK from $SRC ..."
echo "  upstream HEAD: $UPSTREAM_SHA${UPSTREAM_TAG:+ (tag $UPSTREAM_TAG)}"

BUILD_DIR="$(mktemp -d -t godark-cpp-refresh-XXXXXX)"
trap 'rm -rf "$BUILD_DIR"' EXIT

rm -rf "$DEST_SDK"
mkdir -p "$DEST_SDK"

cmake -S "$SRC" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DGODARK_BUILD_TESTS=OFF \
    -DGODARK_BUILD_EXAMPLES=OFF \
    >/dev/null

cmake --build "$BUILD_DIR" -j"$(nproc)"
cmake --install "$BUILD_DIR" --prefix "$DEST_SDK" >/dev/null

# Pin the commit (prefer tag for human readability if HEAD is on one).
if [[ -n "$UPSTREAM_TAG" ]]; then
  printf '%s\n' "$UPSTREAM_TAG" > "$DEST_SDK/UPSTREAM_REF"
else
  printf '%s\n' "$UPSTREAM_SHA" > "$DEST_SDK/UPSTREAM_REF"
fi
echo "  wrote pin: $(cat "$DEST_SDK/UPSTREAM_REF")  -> sdk/UPSTREAM_REF"

# Sanity: confirm the expected install layout exists.
for required in \
    include/godark/godark.hpp \
    lib/libgodark.a \
    lib/cmake/godark/godark-config.cmake \
    lib/cmake/godark/godark-config-version.cmake \
    lib/cmake/godark/godark-targets.cmake; do
  if [[ ! -f "$DEST_SDK/$required" ]]; then
    echo "error: install produced no '$required' under $DEST_SDK" >&2
    echo "       did upstream's CMakeLists.txt regress its install rules?" >&2
    exit 1
  fi
done

echo
echo "Vendored size:"
du -sh "$DEST_SDK"
echo
echo "Next steps:"
echo "  git add sdk/ && git commit -m \"refresh: sync vendored sdk/ with upstream $(cat "$DEST_SDK/UPSTREAM_REF")\""
echo "  (cd \"$REPO_ROOT\" && cmake -B build -G Ninja && cmake --build build -j)  # smoke-build examples against refreshed sdk/"
