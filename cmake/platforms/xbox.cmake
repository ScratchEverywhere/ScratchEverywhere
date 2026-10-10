# NXDK patch application

# TODO: maybe have the code changes applied upstream to nxdk?
# FIXME: make this a recipe
if(NOT "$ENV{NXDK_DIR}" STREQUAL "")
    set(NXDK_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/nxdk.patch")
    if(EXISTS "${NXDK_PATCH_FILE}")
        execute_process(
            COMMAND git apply --check "${NXDK_PATCH_FILE}"
            WORKING_DIRECTORY "$ENV{NXDK_DIR}"
            RESULT_VARIABLE GIT_APPLY_CHECK
            OUTPUT_QUIET ERROR_QUIET
        )
        if(GIT_APPLY_CHECK EQUAL 0)
            message(STATUS "Applying nxdk.patch to nxdk...")
            execute_process(
                COMMAND git apply "${NXDK_PATCH_FILE}"
                WORKING_DIRECTORY "$ENV{NXDK_DIR}"
                RESULT_VARIABLE GIT_APPLY_RESULT
            )
            if(GIT_APPLY_RESULT EQUAL 0)
                message(STATUS "Successfully applied nxdk.patch. Rebuilding libxboxrt.lib...")
                execute_process(
                    COMMAND bash -c "$ENV{NXDK_DIR}/bin/nxdk-cc -c $ENV{NXDK_DIR}/lib/xboxrt/libc_extensions/stdlib_ext_.c -o $ENV{NXDK_DIR}/lib/xboxrt/libc_extensions/stdlib_ext_.obj && llvm-ar rcs $ENV{NXDK_DIR}/lib/libxboxrt.lib $ENV{NXDK_DIR}/lib/xboxrt/libc_extensions/*.obj $ENV{NXDK_DIR}/lib/xboxrt/c_runtime/*.obj $ENV{NXDK_DIR}/lib/xboxrt/vcruntime/*.obj"
                    WORKING_DIRECTORY "$ENV{NXDK_DIR}"
                )
            else()
                message(FATAL_ERROR "Failed to apply nxdk.patch!")
            endif()
        else()
            message(STATUS "nxdk.patch is already applied.")
        endif()
    endif()
endif()

# NXDK should be present with $NXDK_DIR

# FIXME: "This should show a 'FATAL_ERROR' not build NXDK"
if(NOT "$ENV{NXDK_DIR}" STREQUAL "" AND NOT EXISTS "$ENV{NXDK_DIR}/lib/libnxdk.lib")
    message(STATUS "NXDK libraries not found. Building nxdk...")
    execute_process(
        COMMAND make -j NXDK_ONLY=y
        WORKING_DIRECTORY "$ENV{NXDK_DIR}"
        RESULT_VARIABLE NXDK_MAKE_RESULT
    )
    if(NOT NXDK_MAKE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build nxdk automatically.")
    endif()
endif()

# NXDK-SDL3 single patch application and subdirectory inclusion

# FIXME: should be using a catalog recipe for this
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
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl3")
set(SE_AUDIO_ENGINE_DEFAULT "sdl3")

set(SE_DEPS_VALID_OPTIONS "fallback" "system")
set(SE_LUA_BACKEND_VALID_OPTIONS "fallback")
set(SE_ZIP_BACKEND_VALID_OPTIONS "miniz")

# CPU expensive and nxdk implementation has type conflicts; keep off for now
set(SE_DECTALK_DEFAULT OFF)

set(SE_CACHING_DEFAULT ON)
set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

# TODO: try to implement using WinAPIs
set(SE_HAS_THREADS OFF)

set(SE_PLATFORM_DEFINITIONS "__XBOX__")
set(SE_PLATFORM "xbox")

# TODO: add custom extension support, no lua.h on this build
set(SE_ALLOW_CUSTOM_EXTENSIONS FALSE)
set(SE_CUSTOM_EXTENSIONS OFF)

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

    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND ${NXDK_DIR}/bin/cxbe
        -OUT:${CMAKE_CURRENT_BINARY_DIR}/default.xbe
        -TITLE:"${SE_APP_NAME}"
        $<TARGET_FILE:scratch-everywhere>
        COMMENT "Making default.xbe"
    )

    find_program(EXTRACT_XISO_BIN extract-xiso PATHS "${NXDK_DIR}/bin" "$ENV{NXDK_DIR}/bin")
    if(EXTRACT_XISO_BIN)
        set(SAMPLE_PROJECT_CMD "")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/project.sb3")
            set(SAMPLE_PROJECT_CMD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/project.sb3" "${CMAKE_CURRENT_BINARY_DIR}/iso_root/project.sb3")
        endif()
        add_custom_command(TARGET scratch-everywhere POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/iso_root
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/default.xbe ${CMAKE_CURRENT_BINARY_DIR}/iso_root/default.xbe
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/romfs ${CMAKE_CURRENT_BINARY_DIR}/iso_root/romfs
            ${SAMPLE_PROJECT_CMD}
            COMMAND ${EXTRACT_XISO_BIN} -c ${CMAKE_CURRENT_BINARY_DIR}/iso_root ${CMAKE_CURRENT_BINARY_DIR}/SE.iso
            COMMENT "Building SE.iso"
        )
    endif()
endmacro()
