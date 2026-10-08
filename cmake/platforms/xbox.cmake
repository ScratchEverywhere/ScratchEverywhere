set(SE_DEFAULT_OUTPUT_NAME "scratch-xbox")

# NXDK should be present with $NXDK_DIR
# NXDK-SDL3 should be present with $NXDK_SDL3_DIR
#set(SE_RENDERER_VALID_OPTIONS "sdl3") 
#set(SE_WINDOWING_VALID_OPTIONS "sdl3")
#set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl3")

# For the moment, we want to see if it will run headlessly
set(SE_RENDERER_VALID_OPTIONS "headless")
set(SE_WINDOWING_VALID_OPTIONS "headless")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "headless")

set(SE_DEPS_VALID_OPTIONS "fallback" "system") # DO NOT MODIFY
set(SE_LUA_BACKEND_VALID_OPTIONS "fallback")
set(SE_ZIP_BACKEND "minizip") # DO NOT MODIFY

set(SE_CACHING_DEFAULT OFF) # Stream from romfs or hard drive since we only have 64 MiB of RAM
set(SE_DECTALK_DEFAULT OFF) # CPU expensive and nxdk implementation has type conflicts; keep off for now
set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

# These may need to be adjusted
#set(SE_SVG ON)
#set(SE_BITMAP ON)
#set(SE_MENU ON)
#set(SE_LOADSCREEN ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)
set(SE_HAS_THREADS OFF)

set(SE_PLATFORM_DEFINITIONS "__XBOX__")
set(SE_PLATFORM "xbox")

set(SE_ALLOW_CUSTOM_EXTENSIONS FALSE)
set(SE_CUSTOM_EXTENSIONS OFF)

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include/platforms/xbox)

add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-include>
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/platforms/xbox/xbox_iostream_injector.hpp>
    
    # Needed for PIII Coppermine tuning
    -march=pentium3
    -mtune=pentium3

    # Required due to NV2A architecture limitations
    -DSE_NO_OPENGL

    # Required due to PIII Coppermine limitations
    -DSTBI_NO_SIMD
    -DDR_MP3_NO_SIMD
    -DDR_WAV_NO_SIMD
    -DDR_MP3_NO_WCHAR
    -DDR_WAV_NO_WCHAR

    # These need review
    -DHAVE_UNISTD_H
    -DHAVE_DIRENT_H
    -DHAVE_UTIME_H
    -DDR_MP3_NO_STDIO_SECURE
    -DDR_WAV_NO_STDIO_SECURE
    -DDR_FS_NO_CRT_SECURE
    -DMINIZ_NO_TIME=1
    -DMZ_FOPEN=fopen
    -DMZ_FCLOSE=fclose
    -DMZ_FREAD=fread
    -DMZ_FWRITE=fwrite
    -DMZ_FTELL64=ftell
    -DMZ_FSEEK64=fseek
    -DMZ_FILE_STAT_STRUCT=stat
    -DMZ_FILE_STAT=stat
    -DMZ_FREOPEN=freopen
    -DMZ_DELETE_FILE=remove
    -Drestrict=__restrict
    -DXBOXRT_RESTRICT=__restrict
    -Dalloca=__builtin_alloca
    -Dftello=ftell
    -Dfseeko=fseek
    -DUNLEN=256
    -fms-extensions

    # Required due to NXDK toolchain limitations
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:C>:-U_MSC_VER>
)

# Disable warnings, we only want to see the real errors
add_compile_options(-w)

# DO NOT MODIFY
macro(package_platform)
    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND ${NXDK_DIR}/bin/cxbe
            -OUT:${CMAKE_CURRENT_BINARY_DIR}/default.xbe
            -TITLE:"${SE_APP_NAME}"
            $<TARGET_FILE:scratch-everywhere>
        COMMENT "Making default.xbe"
    )

    find_program(EXTRACT_XISO_BIN extract-xiso PATHS "${NXDK_DIR}/bin" "$ENV{NXDK_DIR}/bin")
    if(EXTRACT_XISO_BIN)
        add_custom_command(TARGET scratch-everywhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/iso_root
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/default.xbe ${CMAKE_CURRENT_BINARY_DIR}/iso_root/default.xbe
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/romfs ${CMAKE_CURRENT_BINARY_DIR}/iso_root/romfs
            COMMAND ${EXTRACT_XISO_BIN} -c ${CMAKE_CURRENT_BINARY_DIR}/iso_root ${CMAKE_CURRENT_BINARY_DIR}/SE.iso
            COMMENT "Building SE.iso"
        )
    endif()
endmacro()
