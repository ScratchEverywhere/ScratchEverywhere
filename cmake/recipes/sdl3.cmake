function(_recipe_SDL3_toolchain)
    if(EMSCRIPTEN)
        add_library(SDL3 INTERFACE)
        target_compile_options(SDL3 INTERFACE "-sUSE_SDL=3")
        target_link_options(SDL3 INTERFACE "-sUSE_SDL=3")
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
    if(XBOX)
        set(NXDK_SDL3_REPO "https://github.com/Ryzee119/nxdk-sdl3.git")
        if(DEFINED ENV{NXDK_SDL3_DIR} AND EXISTS "$ENV{NXDK_SDL3_DIR}")
            set(NXDK_SDL3_REPO "$ENV{NXDK_SDL3_DIR}")
        endif()

        cl_import_source(
            NAME SDL3
            REPO "${NXDK_SDL3_REPO}"
            REF "nxaudio"
            PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/nxdk-sdl3.patch"
        )

        if(NOT TARGET SDL3 AND TARGET SDL3-static)
            add_library(SDL3 ALIAS SDL3-static)
        endif()
        if(NOT TARGET SDL3::SDL3 AND TARGET SDL3-static)
            add_library(SDL3::SDL3 ALIAS SDL3-static)
        endif()
        return()
    endif()

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
