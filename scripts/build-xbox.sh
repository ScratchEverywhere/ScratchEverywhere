#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

# Pick up the optional project-local Rust installation described in the notes.
if [[ -x "$root/.cache/toolchains/cargo/bin/cargo" ]]; then
    export CARGO_HOME="$root/.cache/toolchains/cargo"
    export RUSTUP_HOME="$root/.cache/toolchains/rustup"
    export PATH="$CARGO_HOME/bin:$PATH"
fi
if ! command -v cargo >/dev/null; then
    echo "Rust/Cargo is required by the Xbox audio DSP assembler; see docs/xbox-fork-build.md." >&2
    exit 1
fi

# Use a project-local SDK: SE applies patches that other NXDK projects may not need.
export NXDK_DIR="$root/.cache/toolchains/nxdk"
nxdk_ref=14d5ee97e73347c973f1f57b68b79ec08c9e77f2
if [[ ! -d "$NXDK_DIR/.git" ]]; then
    mkdir -p "$(dirname "$NXDK_DIR")"
    git clone https://github.com/XboxDev/nxdk.git "$NXDK_DIR"
    git -C "$NXDK_DIR" checkout "$nxdk_ref"
    git -C "$NXDK_DIR" submodule update --init --recursive
fi
if [[ "$(git -C "$NXDK_DIR" rev-parse HEAD)" != "$nxdk_ref" ]]; then
    echo "Expected NXDK $nxdk_ref in $NXDK_DIR; refusing to change an existing SDK." >&2
    exit 1
fi

# Apple's clang does not ship the complete LLVM cross-compilation tools.
if [[ "$(uname -s)" == Darwin ]] && command -v brew >/dev/null; then
    export PATH="$(brew --prefix llvm)/bin:$PATH"
fi
export PATH="$NXDK_DIR/bin:$PATH"
export CATALOG_NON_INTERACTIVE=1
mkdir -p romfs

cmake -S . -B build-xbox \
    -DCMAKE_TOOLCHAIN_FILE="$NXDK_DIR/share/toolchain-nxdk.cmake" \
    -DXBOX=ON -DCMAKE_BUILD_TYPE=Release \
    -DHAVE_LONGLONG=1 -DSIZEOF_LONG_LONG=8 \
    -DSIZEOF_CURL_OFF_T=8 -DCURL_SIZEOF_CURL_OFF_T=8 "$@"
cmake --build build-xbox --parallel "${BUILD_JOBS:-4}"
