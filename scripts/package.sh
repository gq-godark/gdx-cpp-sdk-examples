#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_NAME="${1:-godark-cpp-sdk}"

STAGING_DIR="$(mktemp -d)"
DEST="$STAGING_DIR/$DIST_NAME"

mkdir -p "$DEST"

cp "$REPO_ROOT/CMakeLists.txt"     "$DEST/"
cp "$REPO_ROOT/CMakePresets.json"  "$DEST/"
cp "$REPO_ROOT/vcpkg.json"        "$DEST/"
cp "$REPO_ROOT/README.md"         "$DEST/"
cp "$REPO_ROOT/SDK_REFERENCE.md"  "$DEST/"
cp "$REPO_ROOT/.env.example"      "$DEST/"

cp -r "$REPO_ROOT/examples" "$DEST/examples"
cp -r "$REPO_ROOT/sdk"      "$DEST/sdk"

ARCHIVE="$REPO_ROOT/${DIST_NAME}.tar.gz"
tar -czf "$ARCHIVE" -C "$STAGING_DIR" "$DIST_NAME"
rm -rf "$STAGING_DIR"

echo "Package created: $ARCHIVE"
echo "Contents:"
tar -tzf "$ARCHIVE" | head -30
