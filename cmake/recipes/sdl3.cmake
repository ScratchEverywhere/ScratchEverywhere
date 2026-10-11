function(_recipe_SDL3_toolchain)
    if(EMSCRIPTEN)
        add_library(SDL3 INTERFACE)
        target_compile_options(SDL3 INTERFACE "-sUSE_SDL=3")
        target_link_options(SDL3 INTERFACE "-sUSE_SDL=3")

    elseif(XBOX)
        if("$ENV{NXDK_SDL3_DIR}" STREQUAL "")
            message(FATAL_ERROR "NXDK_SDL3_DIR is not set")
        endif()

        set(NXDL_SDL3 "$ENV{NXDK_SDL3_DIR}")

        # SDL and xgu are submodules; a plain clone leaves them empty
        if(NOT EXISTS "${NXDL_SDL3}/SDL/include/SDL3/SDL.h"
            OR NOT EXISTS "${NXDL_SDL3}/nxdk_glue/render/xgu/xgu.h")
            execute_process(
                COMMAND git submodule update --init --recursive
                WORKING_DIRECTORY ${NXDK_NXDL_SDL3}
                RESULT_VARIABLE _submod
            )
            if(NOT _submod EQUAL 0)
                message(FATAL_ERROR "Failed to init nxdk-sdl3 submodules")
            endif()
        endif()

        set(NXDK_PATCH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/nxdk-sdl3.patch")
        if(EXISTS "${NXDK_PATCH}")
            # --check fails once applied, so re-configures are safe
            execute_process(
                COMMAND git apply --check "${NXDK_PATCH}"
                WORKING_DIRECTORY ${NXDL_SDL3}
                RESULT_VARIABLE _check OUTPUT_QUIET ERROR_QUIET
            )
            if(_check EQUAL 0)
                message(STATUS "Applying nxdk-sdl3.patch")
                execute_process(
                    COMMAND git apply "${NXDK_PATCH}"
                    WORKING_DIRECTORY ${NXDL_SDL3}
                    RESULT_VARIABLE _apply
                )
                if(NOT _apply EQUAL 0)
                    message(FATAL_ERROR "Failed to apply ${NXDK_PATCH}")
                endif()
            else()
                message(STATUS "nxdk-sdl3.patch already applied")
            endif()
        endif()

        add_subdirectory("${NXDL_SDL3}" nxdk-sdl3)

        # nxdk-sdl3 builds SDL3-static; expose the names consumers expect
        if(NOT TARGET SDL3 AND TARGET SDL3-static)
            add_library(SDL3 ALIAS SDL3-static)
        endif()
        if(NOT TARGET SDL3::SDL3 AND TARGET SDL3-static)
            add_library(SDL3::SDL3 ALIAS SDL3-static)
        endif()
    endif()
endfunction()

function(_recipe_SDL3_system)
    if(XBOX)
        return() # nxdk provides its own SDL3 port; never use the host's
    endif()

    cl_format_pkgconfig_req("sdl3" "${CL_VERSION_REQ}" PKG_SPEC)

    if(CL_STATIC)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL3 IMPORTED_TARGET GLOBAL "--static" ${PKG_SPEC})
        endif()
    else()
        if(NOT CMAKE_CROSSCOMPILING)
            if(CL_REQ_VERSION)
                find_package(SDL3 ${CL_REQ_VERSION} QUIET)
            else()
                find_package(SDL3 QUIET)
            endif()
        endif()

        if(NOT TARGET SDL3 AND NOT TARGET SDL3::SDL3)
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(SDL3 IMPORTED_TARGET GLOBAL ${PKG_SPEC})
            endif()
        endif()
    endif()
endfunction()

function(_recipe_SDL3_package)
    if(XBOX OR CL_REQUIRE_STATIC) # Most package managers don't provide static libs.
        return()
    endif()

    if(CL_PACKAGE_MANAGER STREQUAL "apt")
        set(CL_PACKAGE_NAME "libsdl3-dev" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "pacman")
        set(CL_PACKAGE_NAME "sdl3" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "brew")
        set(CL_PACKAGE_NAME "sdl3" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "yum")
        set(CL_PACKAGE_NAME "SDL3-devel" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "apk")
        set(CL_PACKAGE_NAME "sdl3-dev" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "zypper")
        set(CL_PACKAGE_NAME "libSDL3-devel" PARENT_SCOPE)
    endif()
endfunction()

function(_recipe_SDL3_source)
    set(SDL_BUILD_SHARED "ON")
    set(SDL_BUILD_STATIC "ON")

    if(CL_REQ_TYPE STREQUAL "STATIC" OR CL_REQ_TYPE STREQUAL "PREFER_STATIC")
        set(SDL_BUILD_SHARED "OFF")
        set(SDL_BUILD_STATIC "ON")
    elseif(CL_REQ_TYPE STREQUAL "SHARED" OR CL_REQ_TYPE STREQUAL "PREFER_SHARED")
        set(SDL_BUILD_SHARED "ON")
        set(SDL_BUILD_STATIC "OFF")
    endif()

    set(SDL_TAG "release-3.4.14")
    if(CL_REQ_VERSION)
        set(SDL_TAG "release-${CL_REQ_VERSION}")
    endif()

    cl_import_source(
        NAME SDL3
        URL https://github.com/libsdl-org/SDL/archive/refs/tags/${SDL_TAG}.tar.gz
        OPTIONS "SDL_SHARED" "${SDL_BUILD_SHARED}" "SDL_STATIC" "${SDL_BUILD_STATIC}" "SDL_TEST_LIBRARY" "OFF"
    )
endfunction()
