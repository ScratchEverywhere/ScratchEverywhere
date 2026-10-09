# NXDK should be present with $NXDK_DIR
if(NOT "$ENV{NXDK_DIR}" STREQUAL "" AND NOT EXISTS "$ENV{NXDK_DIR}/lib/libnxdk.lib")
    message(STATUS "NXDK libraries not found. Building nxdk...")
    execute_process(
        COMMAND make -j
        WORKING_DIRECTORY "$ENV{NXDK_DIR}"
        RESULT_VARIABLE NXDK_MAKE_RESULT
    )
    if(NOT NXDK_MAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build nxdk automatically.")
    endif()
endif()

# NXDK-SDL3 single patch application and subdirectory inclusion
if(NOT "$ENV{NXDK_SDL3_DIR}" STREQUAL "")
    set(PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/nxdk-sdl3.patch")
    
    if(EXISTS "${PATCH_FILE}")
        # Check if patch can be applied cleanly (prevents errors on re-configuration)
        execute_process(
            COMMAND git apply --check "${PATCH_FILE}"
            WORKING_DIRECTORY "$ENV{NXDK_SDL3_DIR}"
            RESULT_VARIABLE GIT_APPLY_CHECK
            OUTPUT_QUIET ERROR_QUIET
        )
        
        if(GIT_APPLY_CHECK EQUAL 0)
            message(STATUS "Applying nxdk-sdl3.patch to nxdk-sdl3...")
            execute_process(
                COMMAND git apply "${PATCH_FILE}"
                WORKING_DIRECTORY "$ENV{NXDK_SDL3_DIR}"
                RESULT_VARIABLE GIT_APPLY_RESULT
            )
            if(GIT_APPLY_RESULT EQUAL 0)
                message(STATUS "Successfully applied nxdk-sdl3.patch.")
            else()
                message(FATAL_ERROR "Failed to apply nxdk-sdl3.patch!")
            endif()
        else()
            message(STATUS "nxdk-sdl3.patch is already applied.")
        endif()
    endif()

    add_subdirectory($ENV{NXDK_SDL3_DIR} nxdk-sdl3)
endif()

set(SE_DEFAULT_OUTPUT_NAME "scratch-xbox")

set(SE_RENDERER_VALID_OPTIONS "sdl3") 
set(SE_WINDOWING_VALID_OPTIONS "sdl3")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "headless") # "sdl3") # FIXME: Audio is completely broken right now; check source/menus/*.cpp for audio disable calls

set(SE_DEPS_VALID_OPTIONS "fallback" "system") # DO NOT MODIFY
set(SE_LUA_BACKEND_VALID_OPTIONS "fallback")
set(SE_ZIP_BACKEND "minizip") # DO NOT MODIFY

set(SE_CACHING_DEFAULT ON) # Stream from romfs or hard drive since we only have 64 MiB of RAM
set(SE_DECTALK_DEFAULT OFF) # CPU expensive and nxdk implementation has type conflicts; keep off for now
set(SE_ALLOW_CMAKERC OFF)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

# These may need to be adjusted
set(SE_SVG ON) # FIXME: SVG loading is completely broken right now; check all source/menus/*.cpp for *.svg to *.png conversion
set(SE_BITMAP ON)
set(SE_MENU ON)
set(SE_LOADSCREEN ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)
set(SE_HAS_THREADS ON)

set(SE_PLATFORM_DEFINITIONS "__XBOX__")
set(SE_PLATFORM "xbox")

# TODO: add custom extension support, no lua.h on this build
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
    #-DDR_MP3_NO_SIMD
    #-DDR_WAV_NO_SIMD
    #-DDR_MP3_NO_WCHAR
    #-DDR_WAV_NO_WCHAR
    #-fno-fast-math

    # These need review
    -DHAVE_UNISTD_H
    -DHAVE_DIRENT_H
    -DHAVE_UTIME_H
    #-DDR_MP3_NO_STDIO_SECURE
    #-DDR_WAV_NO_STDIO_SECURE
    #-DDR_FS_NO_CRT_SECURE
    -DMINIZ_NO_TIME=1
    #-DMZ_FOPEN=fopen
    #-DMZ_FCLOSE=fclose
    #-DMZ_FREAD=fread
    #-DMZ_FWRITE=fwrite
    #-DMZ_FTELL64=ftell
    #-DMZ_FSEEK64=fseek
    #-DMZ_FILE_STAT_STRUCT=stat
    #-DMZ_FILE_STAT=stat
    #-DMZ_FREOPEN=freopen
    #-DMZ_DELETE_FILE=remove
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
    target_link_libraries(scratch-everywhere PUBLIC ${NXDK_DIR}/lib/libnxdk_automount_d.lib)
    target_link_options(scratch-everywhere PRIVATE "-include:_automount_d_drive")

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
