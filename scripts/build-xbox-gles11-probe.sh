#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export NXDK_DIR="$root/.cache/toolchains/nxdk"
if [[ "$(uname -s)" == Darwin ]] && command -v brew >/dev/null; then
    export PATH="$(brew --prefix llvm)/bin:$PATH"
fi
export PATH="$NXDK_DIR/bin:$PATH"
library="$root/.cache/toolchains/nxdk-gles11"
ref=0cf17fa709f42158d11a1571f87705b692699bd3
if [[ ! -d "$library/.git" ]]; then
    git clone https://github.com/Ryzee119/nxdk-gles11.git "$library"
    git -C "$library" checkout "$ref"
    git -C "$library" submodule update --init --recursive
fi
if [[ "$(git -C "$library" rev-parse HEAD)" != "$ref" ]]; then
    echo "Expected nxdk-gles11 $ref; refusing to change existing checkout." >&2
    exit 1
fi
cmake -S "$root/experiments/xbox-gles11" -B "$root/build-xbox/gles11-probe" \
    -DCMAKE_TOOLCHAIN_FILE="$NXDK_DIR/share/toolchain-nxdk.cmake" \
    -DGLES11_SOURCE_DIR="$library" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build-xbox/gles11-probe" --parallel 4
