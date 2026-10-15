set(SE_DEFAULT_OUTPUT_NAME "scratch-xbox")

set(SE_RENDERER_VALID_OPTIONS "sdl3")
set(SE_WINDOWING_VALID_OPTIONS "sdl3")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl3")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")
set(SE_ZIP_BACKEND_VALID_OPTIONS "miniz")

# CPU expensive and nxdk implementation has type conflicts; keep off for now
set(SE_DECTALK_DEFAULT OFF)

set(SE_CACHING_DEFAULT ON)
set(SE_BUNDLE_DEFAULT ON)
set(SE_ALLOW_BUNDLE ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)
set(SE_ALLOW_CUSTOM_EXTENSIONS FALSE)

# Force using WinAPI threads
set(Threads_FOUND TRUE CACHE BOOL "Forced WinAPI threads" FORCE)
set(CMAKE_THREAD_LIBS_INIT "" CACHE STRING "Forced WinAPI threads" FORCE)
set(CMAKE_HAVE_THREADS_LIBRARY 1 CACHE BOOL "Forced WinAPI threads" FORCE)
set(CMAKE_USE_WIN32_THREADS_INIT 1 CACHE BOOL "Forced WinAPI threads" FORCE)
set(CMAKE_USE_PTHREADS_INIT 0 CACHE BOOL "Forced WinAPI threads" FORCE)
set(SE_HAS_THREADS ON)

set(SE_PLATFORM_DEFINITIONS "__XBOX__")
set(SE_PLATFORM "xbox")

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include/platforms/xbox)

add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-include>
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/platforms/xbox/xbox_iostream_injector.hpp>
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:C>:-U_MSC_VER>
    -march=pentium3
    -mtune=pentium3
    -DSE_NO_OPENGL
    -DSTBI_NO_SIMD
    -DHAVE_UNISTD_H
    -DHAVE_DIRENT_H
    -DHAVE_UTIME_H
    -DMINIZ_NO_TIME=1
    -Drestrict=__restrict
    -DXBOXRT_RESTRICT=__restrict
    -Dalloca=__builtin_alloca
    -Dftello=ftell
    -Dfseeko=fseek
    -DUNLEN=256
    -fms-extensions
)

macro(package_platform)
    target_link_libraries(scratch-everywhere PUBLIC ${NXDK_DIR}/lib/libnxdk_automount_d.lib)
    target_link_options(scratch-everywhere PRIVATE "-include:_automount_d_drive")
    target_link_options(scratch-everywhere PRIVATE "-safeseh:no")
    target_link_options(scratch-everywhere PRIVATE "/STACK:0x400000,0x20000")

    find_program(CXBE_BIN cxbe PATHS "${NXDK_DIR}/bin" "${NXDK_DIR}/tools/cxbe" "$ENV{NXDK_DIR}/bin" "$ENV{NXDK_DIR}/tools/cxbe")
    if(NOT CXBE_BIN)
        set(CXBE_BIN "${NXDK_DIR}/bin/cxbe")
    endif()

    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND ${CXBE_BIN}
        -OUT:${CMAKE_CURRENT_BINARY_DIR}/default.xbe
        -TITLE:"${SE_APP_NAME}"
        $<TARGET_FILE:scratch-everywhere>
        COMMENT "Making default.xbe"
    )

    find_program(EXTRACT_XISO_BIN extract-xiso PATHS "${NXDK_DIR}/bin" "${NXDK_DIR}/tools/extract-xiso/build" "$ENV{NXDK_DIR}/bin" "$ENV{NXDK_DIR}/tools/extract-xiso/build")
    if(EXTRACT_XISO_BIN)
        add_custom_command(TARGET scratch-everywhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/iso_root
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/default.xbe ${CMAKE_CURRENT_BINARY_DIR}/iso_root/default.xbe
            COMMAND ${EXTRACT_XISO_BIN} -c ${CMAKE_CURRENT_BINARY_DIR}/iso_root ${CMAKE_CURRENT_BINARY_DIR}/SE.iso
            COMMENT "Building SE.iso"
        )
    endif()
endmacro()
