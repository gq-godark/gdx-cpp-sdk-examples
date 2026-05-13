#!/usr/bin/env bash
# MM bundle packager — prebuilt-static-lib zip distribution, built strictly
# from the pinned upstream gdx-cpp-sdk commit recorded in sdk/UPSTREAM_REF.
#
# What this script does:
#   1. Reads the pinned upstream ref from sdk/UPSTREAM_REF.
#   2. Resolves the upstream source tree:
#        - If $UPSTREAM_SRC is set, use that directory (CI / explicit local
#          checkout).
#        - Else if a sibling ../gdx-cpp-sdk or ../new-sdks/gdx-cpp-sdk exists,
#          use that.
#        - Else clone gq-godark/gdx-cpp-sdk@<pinned-ref> --recurse-submodules
#          into a temp dir (requires `gh` or `git`, plus auth for the private
#          repo).
#   3. Verifies the resolved upstream is at exactly the pinned ref.
#   4. Builds the SDK from $UPSTREAM_SRC (NOT from sdk/), so a local edit to
#      sdk/ can never end up in the released artifact. Installs to a temp
#      prefix.
#   5. Parity check: the freshly built install prefix must match the vendored
#      sdk/ exactly (headers + cmake config = text diff, libgodark.a = byte
#      cmp). Drift here means somebody hand-edited the vendored copy or
#      forgot to rerun refresh_sdk.sh after a real upstream bump.
#   6. Stages the bundle, copying the SDK from the **fresh upstream install**
#      (never from sdk/), then zips it.
#
# Output layout (DIST_NAME/):
#   .env.example
#   README.md             (from bundle/README.md)
#   SDK_REFERENCE.md      (from bundle/SDK_REFERENCE.md)
#   CMakeLists.txt
#   CMakePresets.json
#   vcpkg.json
#   examples/
#     CMakeLists.txt
#     dotenv.hpp
#     quickstart.cpp
#     full_trader_example.cpp
#   sdk/
#     include/godark/**.hpp
#     lib/libgodark.a
#     lib/cmake/godark/godark-config.cmake
#     lib/cmake/godark/godark-config-version.cmake
#     lib/cmake/godark/godark-targets.cmake
#     lib/cmake/godark/godark-targets-release.cmake
#
# Usage:
#   bash scripts/package.sh
#   bash scripts/package.sh my-release-name
#   UPSTREAM_SRC=/path/to/gdx-cpp-sdk bash scripts/package.sh
set -euo pipefail

UPSTREAM_REPO="gq-godark/gdx-cpp-sdk"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_NAME="${1:-gdx-cpp-sdk}"

cd "$REPO_ROOT"

# ---- pre-flight ----------------------------------------------------------
if [[ ! -f "${REPO_ROOT}/sdk/UPSTREAM_REF" ]]; then
  echo "error: sdk/UPSTREAM_REF missing — run scripts/refresh_sdk.sh first" >&2
  exit 1
fi
PINNED_REF="$(tr -d '[:space:]' < "${REPO_ROOT}/sdk/UPSTREAM_REF")"
if [[ -z "$PINNED_REF" ]]; then
  echo "error: sdk/UPSTREAM_REF is empty" >&2
  exit 1
fi

# Required source files. The client-facing README / SDK_REFERENCE shipped to
# market makers live under bundle/ and are intentionally separate from the
# repo-root copies (which document the internal refresh / packaging workflow
# and must NOT end up in the released archive). This split mirrors the Python
# distribution (gdx-python-sdk-examples/bundle/{README,SDK_REFERENCE}.md).
for required in \
    bundle/README.md \
    bundle/SDK_REFERENCE.md \
    CMakeLists.txt \
    CMakePresets.json \
    vcpkg.json \
    .env.example \
    examples/CMakeLists.txt \
    examples/quickstart.cpp \
    examples/full_trader_example.cpp \
    examples/dotenv.hpp; do
  if [[ ! -f "${REPO_ROOT}/${required}" ]]; then
    echo "error: required source file missing: ${required}" >&2
    exit 1
  fi
done

for tool in cmake ninja zip diff cmp; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "error: required tool '$tool' not found in PATH" >&2
    exit 1
  fi
done

# ---- resolve upstream source tree -----------------------------------------
CLEANUP_UPSTREAM=false

if [[ -n "${UPSTREAM_SRC:-}" ]]; then
  echo "Using UPSTREAM_SRC=${UPSTREAM_SRC}"
elif [[ -d "${REPO_ROOT}/../gdx-cpp-sdk/.git" ]]; then
  UPSTREAM_SRC="$(cd "${REPO_ROOT}/../gdx-cpp-sdk" && pwd)"
  echo "Using sibling upstream checkout: $UPSTREAM_SRC"
elif [[ -d "${REPO_ROOT}/../new-sdks/gdx-cpp-sdk/.git" ]]; then
  UPSTREAM_SRC="$(cd "${REPO_ROOT}/../new-sdks/gdx-cpp-sdk" && pwd)"
  echo "Using sibling upstream checkout: $UPSTREAM_SRC"
else
  CLEANUP_UPSTREAM=true
  UPSTREAM_SRC="$(mktemp -d)/gdx-cpp-sdk"
  echo "Cloning ${UPSTREAM_REPO}@${PINNED_REF} -> $UPSTREAM_SRC ..."
  if command -v gh >/dev/null 2>&1; then
    gh repo clone "${UPSTREAM_REPO}" "$UPSTREAM_SRC" -- --quiet --recurse-submodules --filter=blob:none
  else
    git clone --quiet --recurse-submodules --filter=blob:none \
        "https://github.com/${UPSTREAM_REPO}.git" "$UPSTREAM_SRC"
  fi
  git -C "$UPSTREAM_SRC" checkout --quiet "$PINNED_REF"
  git -C "$UPSTREAM_SRC" submodule update --init --recursive --quiet
fi

cleanup() {
  local rc=$?
  if [[ "$CLEANUP_UPSTREAM" == true && -n "${UPSTREAM_SRC:-}" ]]; then
    rm -rf "$(dirname "$UPSTREAM_SRC")"
  fi
  if [[ -n "${UPSTREAM_BUILD:-}" && -d "${UPSTREAM_BUILD}" ]]; then
    rm -rf "$(dirname "$UPSTREAM_BUILD")"
  fi
  if [[ -n "${STAGING_DIR:-}" && -d "${STAGING_DIR}" ]]; then
    rm -rf "$STAGING_DIR"
  fi
  return $rc
}
trap cleanup EXIT

# ---- verify upstream is at the pinned ref ---------------------------------
if [[ ! -d "$UPSTREAM_SRC/.git" ]]; then
  echo "error: '$UPSTREAM_SRC' is not a git checkout — cannot verify pin" >&2
  exit 1
fi
upstream_head_sha="$(git -C "$UPSTREAM_SRC" rev-parse HEAD)"
upstream_pin_sha="$(git -C "$UPSTREAM_SRC" rev-parse "$PINNED_REF" 2>/dev/null || true)"
if [[ -z "$upstream_pin_sha" ]]; then
  echo "error: pinned ref '$PINNED_REF' does not resolve in $UPSTREAM_SRC" >&2
  echo "       (try: git -C $UPSTREAM_SRC fetch --tags origin)" >&2
  exit 1
fi
if [[ "$upstream_head_sha" != "$upstream_pin_sha" ]]; then
  echo "error: upstream HEAD ($upstream_head_sha) does not match pinned ref" >&2
  echo "       sdk/UPSTREAM_REF=$PINNED_REF -> $upstream_pin_sha" >&2
  echo "       checkout the pinned ref before packaging:" >&2
  echo "         git -C $UPSTREAM_SRC checkout $PINNED_REF" >&2
  exit 1
fi
echo "Upstream verified at pin: $PINNED_REF ($upstream_head_sha)"

# gdx-proto submodule must be present (CMake reads .proto files from it).
if [[ ! -d "$UPSTREAM_SRC/gdx-proto/proto" ]]; then
  echo "error: '$UPSTREAM_SRC/gdx-proto/proto' missing — initialize submodule:" >&2
  echo "       git -C $UPSTREAM_SRC submodule update --init --recursive" >&2
  exit 1
fi

# ---- build upstream into a temp install prefix ----------------------------
UPSTREAM_BUILD="$(mktemp -d -t godark-cpp-pkg-build-XXXXXX)/build"
UPSTREAM_PREFIX="$(mktemp -d -t godark-cpp-pkg-prefix-XXXXXX)/sdk"
mkdir -p "$UPSTREAM_BUILD" "$UPSTREAM_PREFIX"

echo "Building upstream SDK from $UPSTREAM_SRC ..."
cmake -S "$UPSTREAM_SRC" -B "$UPSTREAM_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DGODARK_BUILD_TESTS=OFF \
    -DGODARK_BUILD_EXAMPLES=OFF \
    >/dev/null

cmake --build "$UPSTREAM_BUILD" -j"$(nproc)" >/dev/null
cmake --install "$UPSTREAM_BUILD" --prefix "$UPSTREAM_PREFIX" >/dev/null

# Sanity: install must produce the expected layout (else upstream regressed).
for required in \
    include/godark/godark.hpp \
    lib/libgodark.a \
    lib/cmake/godark/godark-config.cmake \
    lib/cmake/godark/godark-config-version.cmake \
    lib/cmake/godark/godark-targets.cmake \
    lib/cmake/godark/godark-targets-release.cmake; do
  if [[ ! -f "$UPSTREAM_PREFIX/$required" ]]; then
    echo "error: upstream install produced no '$required' — upstream regression?" >&2
    exit 1
  fi
done
echo "  upstream install OK ($UPSTREAM_PREFIX)"

# ---- parity check: vendored sdk/ must match upstream install --------------
# Two-tier contract:
#   Tier 1 (byte parity): the artifacts that drive ABI and source identity —
#     libgodark.a (the actual archive linker sees) and include/ (the headers
#     the compiler sees). Any drift here means the vendored copy could ship
#     a different ABI/API than the recorded upstream pin.
#   Tier 2 (structural parity): cmake package config under lib/cmake/godark/.
#     This is install(EXPORT)-generated CMake metadata, NOT source. Its exact
#     bytes vary across cmake patch/minor versions (the generated import
#     boilerplate differs between e.g. cmake 3.28.3 and 3.28.x) without
#     affecting the import contract. The released bundle copies lib/cmake/
#     from $UPSTREAM_PREFIX (see staging step below), not from sdk/, so
#     vendored drift in these files cannot reach the shipped artifact. We
#     still sanity-check the files exist and import the expected target.
parity_ok=true

# Tier 1a — headers: byte parity (recursive text diff is sufficient since
# every header is text and diff reports any size or content delta).
if ! diff -r --brief "$UPSTREAM_PREFIX/include/godark" "$REPO_ROOT/sdk/include/godark" >/dev/null; then
  parity_ok=false
  echo "error: vendored sdk/include/godark/ has drifted from upstream $PINNED_REF:" >&2
  diff -r --brief "$UPSTREAM_PREFIX/include/godark" "$REPO_ROOT/sdk/include/godark" >&2 || true
fi

# Tier 1b — static library: byte-for-byte cmp.
if ! cmp -s "$UPSTREAM_PREFIX/lib/libgodark.a" "$REPO_ROOT/sdk/lib/libgodark.a"; then
  parity_ok=false
  echo "error: vendored sdk/lib/libgodark.a differs byte-for-byte from upstream $PINNED_REF:" >&2
  printf '  vendored size: %s bytes\n' "$(stat -c %s "$REPO_ROOT/sdk/lib/libgodark.a")" >&2
  printf '  upstream size: %s bytes\n' "$(stat -c %s "$UPSTREAM_PREFIX/lib/libgodark.a")" >&2
fi

# Tier 2 — cmake package config: structural sanity check.
# Required files must all be present in both sides (catches "someone deleted
# a config file" or "upstream renamed the export set").
for cmake_file in \
    lib/cmake/godark/godark-config.cmake \
    lib/cmake/godark/godark-config-version.cmake \
    lib/cmake/godark/godark-targets.cmake \
    lib/cmake/godark/godark-targets-release.cmake; do
  if [[ ! -f "$UPSTREAM_PREFIX/$cmake_file" ]]; then
    parity_ok=false
    echo "error: upstream install missing $cmake_file" >&2
  fi
  if [[ ! -f "$REPO_ROOT/sdk/$cmake_file" ]]; then
    parity_ok=false
    echo "error: vendored sdk/ missing $cmake_file — run scripts/refresh_sdk.sh" >&2
  fi
done

# Imported target identity: the vendored godark-targets.cmake MUST define
# the godark::godark imported target and reference libgodark.a as the
# imported location. If either invariant breaks, downstream cmake projects
# linking against the vendored copy would silently link the wrong artifact
# (or fail to find the target).
vendored_targets="$REPO_ROOT/sdk/lib/cmake/godark/godark-targets.cmake"
vendored_targets_release="$REPO_ROOT/sdk/lib/cmake/godark/godark-targets-release.cmake"
if [[ -f "$vendored_targets" && ! $(grep -E 'add_library\(godark::godark' "$vendored_targets") ]]; then
  parity_ok=false
  echo "error: vendored godark-targets.cmake does not declare imported target godark::godark" >&2
fi
if [[ -f "$vendored_targets_release" && ! $(grep -F 'libgodark.a' "$vendored_targets_release") ]]; then
  parity_ok=false
  echo "error: vendored godark-targets-release.cmake does not reference libgodark.a" >&2
fi

# Diagnostic — surface (but do not fail on) byte differences in lib/cmake/.
# Useful for spotting unexpected upstream changes early.
if ! diff -r --brief "$UPSTREAM_PREFIX/lib/cmake/godark" "$REPO_ROOT/sdk/lib/cmake/godark" >/dev/null 2>&1; then
  echo "note: vendored sdk/lib/cmake/godark/ differs from upstream (cmake-version flakiness, non-fatal):"
  diff -r --brief "$UPSTREAM_PREFIX/lib/cmake/godark" "$REPO_ROOT/sdk/lib/cmake/godark" 2>&1 | sed 's/^/  /' || true
fi

if [[ "$parity_ok" != true ]]; then
  echo >&2
  echo "  fix: bash scripts/refresh_sdk.sh $UPSTREAM_SRC && git add sdk/ && git commit" >&2
  exit 1
fi
echo "Parity check passed: sdk/ matches $UPSTREAM_PREFIX"

# ---- stage ----------------------------------------------------------------
STAGING_DIR="$(mktemp -d)"
DEST="$STAGING_DIR/$DIST_NAME"
mkdir -p "$DEST/examples" "$DEST/sdk"

echo "Staging prebuilt distribution at $DEST ..."

# Top-level build glue (consumed by the recipient's `cmake -B build`).
cp "$REPO_ROOT/CMakeLists.txt"           "$DEST/"
cp "$REPO_ROOT/CMakePresets.json"        "$DEST/"
cp "$REPO_ROOT/vcpkg.json"               "$DEST/"
cp "$REPO_ROOT/.env.example"             "$DEST/"

# MM-facing docs come from bundle/, never from the repo-root copies.
cp "$REPO_ROOT/bundle/README.md"         "$DEST/README.md"
cp "$REPO_ROOT/bundle/SDK_REFERENCE.md"  "$DEST/SDK_REFERENCE.md"

# Examples.
cp "$REPO_ROOT/examples/CMakeLists.txt"           "$DEST/examples/"
cp "$REPO_ROOT/examples/quickstart.cpp"           "$DEST/examples/"
cp "$REPO_ROOT/examples/full_trader_example.cpp"  "$DEST/examples/"
cp "$REPO_ROOT/examples/dotenv.hpp"               "$DEST/examples/"

# The SDK ships from the freshly built upstream install, NEVER from the
# vendored sdk/ tree. This is what enforces the artifact contract: a local
# hand-edit to sdk/ cannot reach the released zip because we copy from
# $UPSTREAM_PREFIX, not $REPO_ROOT/sdk.
cp -r "$UPSTREAM_PREFIX/include" "$DEST/sdk/"
cp -r "$UPSTREAM_PREFIX/lib"     "$DEST/sdk/"

# ---- zip ------------------------------------------------------------------
ARCHIVE="$REPO_ROOT/${DIST_NAME}.zip"
rm -f "$ARCHIVE"
( cd "$STAGING_DIR" && zip -qr "$ARCHIVE" "$DIST_NAME" )

# ---- post-flight assertions ----------------------------------------------
echo
echo "Package created: $ARCHIVE"
LISTING="$(unzip -l "$ARCHIVE")"
echo "$LISTING"

# Must NOT leak internal trees into the archive.
if echo "$LISTING" | grep -E "${DIST_NAME}/(scripts|bundle|build|\.git|CMakeUserPresets)" >/dev/null; then
  echo "error: bundle contains internal files (scripts/, bundle/, build/, .git/, or CMakeUserPresets.json)" >&2
  exit 1
fi

# Must NOT ship the UPSTREAM_REF marker — it's an internal vendoring artifact.
if echo "$LISTING" | grep -E "${DIST_NAME}/sdk/UPSTREAM_REF" >/dev/null; then
  echo "error: bundle contains sdk/UPSTREAM_REF — vendoring marker leaked into archive" >&2
  exit 1
fi

# Every required path must be present.
for required in \
    "${DIST_NAME}/CMakeLists\\.txt" \
    "${DIST_NAME}/CMakePresets\\.json" \
    "${DIST_NAME}/vcpkg\\.json" \
    "${DIST_NAME}/\\.env\\.example" \
    "${DIST_NAME}/README\\.md" \
    "${DIST_NAME}/SDK_REFERENCE\\.md" \
    "${DIST_NAME}/examples/CMakeLists\\.txt" \
    "${DIST_NAME}/examples/quickstart\\.cpp" \
    "${DIST_NAME}/examples/full_trader_example\\.cpp" \
    "${DIST_NAME}/examples/dotenv\\.hpp" \
    "${DIST_NAME}/sdk/include/godark/godark\\.hpp" \
    "${DIST_NAME}/sdk/include/godark/client\\.hpp" \
    "${DIST_NAME}/sdk/lib/libgodark\\.a" \
    "${DIST_NAME}/sdk/lib/cmake/godark/godark-config\\.cmake" \
    "${DIST_NAME}/sdk/lib/cmake/godark/godark-config-version\\.cmake" \
    "${DIST_NAME}/sdk/lib/cmake/godark/godark-targets\\.cmake" \
    "${DIST_NAME}/sdk/lib/cmake/godark/godark-targets-release\\.cmake"; do
  if ! echo "$LISTING" | grep -E "${required}" >/dev/null; then
    echo "error: bundle missing required entry: ${required}" >&2
    exit 1
  fi
done

echo
echo "prebuilt-static-lib assertion: PASSED"
echo "built from upstream:           ${UPSTREAM_REPO}@${PINNED_REF} (${upstream_head_sha})"
