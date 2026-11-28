#!/usr/bin/env bash
set -euo pipefail

DISCORD_ARGS=()
if [ -s /run/secrets/discord_app_id ]; then
	DISCORD_ARGS=(-DSE_DISCORD_RPC=ON "-DSE_DISCORD_APP_ID=$(tr -d '[:space:]' < /run/secrets/discord_app_id)")
fi

mkdir build && cd build
"${ARCH}-apple-darwin24.5-cmake" \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
	-DSE_CLOUDVARS=ON \
	-DSE_DOWNLOAD=ON \
	-DSE_AUDIO_ENGINE=sdl2 \
	-DSE_WINDOWING=sdl2 \
	-DCMAKE_BUILD_TYPE=Release \
	"${DISCORD_ARGS[@]}" \
	..
cmake --build . -j 2
../scripts/macos/package.sh \
	--binary scratch-pc \
	--out macos \
	--version "$(sed -n 's/^project(ScratchEverywhere VERSION \([0-9.]*\).*/\1/p' ../CMakeLists.txt)" \
	--min-macos "$MACOSX_DEPLOYMENT_TARGET"
