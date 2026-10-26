set(SE_DEFAULT_OUTPUT_NAME "scratch-ps3")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_WINDOWING_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")
set(SE_LUA_BACKEND_VALID_OPTIONS "fallback")

set(SE_CACHING_DEFAULT ON)
set(SE_DECTALK_DEFAULT OFF)

set(SE_ALLOW_BUNDLE ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_PLATFORM_DEFINITIONS "__PS3__")
set(SE_PLATFORM "ps3")

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

# for some reason it doesn't compile with svg????
set(SE_SVG OFF CACHE BOOL "Enable support for SVG images." FORCE)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-exceptions -fno-rtti")

macro(package_platform)
	target_link_libraries(scratch-everywhere PRIVATE /usr/local/ps3dev/ppu/lib/libpthread.a)
	# Packaging is done manually via gfx/ps3/makepkg.sh for now.
endmacro()