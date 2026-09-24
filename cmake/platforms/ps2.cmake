set(SE_DEFAULT_OUTPUT_NAME "scratch-ps2")

set(SE_RENDERER_VALID_OPTIONS "sdl2" "sdl3")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2" "sdl3")
set(SE_DEPS_VALID_OPTIONS "fallback")

set(SE_CACHING_DEFAULT OFF)
set(SE_CMAKERC_DEFAULT ON)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM_DEFINITIONS "__PS2__")
set(SE_PLATFORM "ps2")

target_compile_options(scratch-everywhere PRIVATE -fno-pic -fno-pie -mxgot -G0)
target_link_options(scratch-everywhere PRIVATE "-fno-pic" "-fno-pie" "-lpng" "-lz")
target_link_libraries(se-interface INTERFACE -lpng -lz)

macro(package_platform)
    set_target_properties(scratch-everywhere PROPERTIES 
        OUTPUT_NAME "${SE_OUTPUT_NAME}"
        SUFFIX ".elf"
    )
endmacro()
