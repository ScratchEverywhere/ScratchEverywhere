#!/usr/bin/env bash
# Requires: git, cmake, make, a C compiler, zlib and bzip2 development headers.
# Usage: scripts/macos/install-dmg-tools.sh [PREFIX]   (default: /usr/local)
set -euo pipefail

PREFIX="${1:-/usr/local}"
COMMIT="e18579b391950589b9ed16ee630c54ce4319bb29"

SRC="$(mktemp -d)"
trap 'rm -rf "$SRC"' EXIT

git clone -q https://github.com/mozilla/libdmg-hfsplus "$SRC"
git -C "$SRC" checkout -q "$COMMIT"
cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
cmake --build "$SRC/build" -j "$(nproc)" >/dev/null

install -D "$SRC/build/dmg/dmg" "$PREFIX/bin/dmg"
install -D "$SRC/build/hfs/hfsplus" "$PREFIX/bin/hfsplus"
echo "Installed dmg and hfsplus to ${PREFIX}/bin"
