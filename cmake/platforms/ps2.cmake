set(SE_DEFAULT_OUTPUT_NAME "scratch-ps2")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT OFF)
set(SE_CMAKERC_DEFAULT ON)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_HAS_THREADS OFF)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM_DEFINITIONS "__PS2__")

add_compile_options(-fno-pic -fno-pie -mxgot -G0)
add_link_options(-fno-pic -fno-pie)

macro(package_platform)
    # TODO: uh, do it :nategrin:
endmacro()
