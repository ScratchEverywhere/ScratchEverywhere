function(_recipe_SDL2_toolchain)
  if(EMSCRIPTEN)
    add_library(SDL2 INTERFACE)
    target_compile_options(SDL2 INTERFACE "-sUSE_SDL=2")
    target_link_options(SDL2 INTERFACE "-sUSE_SDL=2")
  endif()
endfunction()

function(_recipe_SDL2_system)
  cl_format_pkgconfig_req("sdl2" "${CL_VERSION_REQ}" PKG_SPEC)

  if(CL_STATIC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(SDL2 IMPORTED_TARGET GLOBAL "--static" ${PKG_SPEC})
    endif()
  else()
    if(NOT CMAKE_CROSSCOMPILING)
      if(CL_REQ_VERSION)
        find_package(SDL2 ${CL_REQ_VERSION} QUIET)
      else()
        find_package(SDL2 QUIET)
      endif()
    endif()

    if(UBUNTU_TOUCH AND SDL2_FOUND AND NOT TARGET SDL2::SDL2) # workaround for older SDL2 versions
      add_library(SDL2::SDL2 INTERFACE IMPORTED)
      target_include_directories(SDL2::SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
      separate_arguments(SDL2_LIBRARIES_LIST NATIVE_COMMAND "${SDL2_LIBRARIES}")
      target_link_options(SDL2::SDL2 INTERFACE ${SDL2_LIBRARIES_LIST})
    endif()

    if(NOT TARGET SDL2 AND NOT TARGET SDL2::SDL2)
      find_package(PkgConfig QUIET)
      if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET GLOBAL ${PKG_SPEC})
      endif()
    endif()
  endif()
endfunction()

function(_recipe_SDL2_package)
  if(CL_REQUIRE_STATIC) # Most package managers don't provide static libs.
    return()
  endif()

  if(CL_PACKAGE_MANAGER STREQUAL "apt")
    set(CL_PACKAGE_NAME "libsdl2-dev" PARENT_SCOPE)
  elseif(CL_PACKAGE_MANAGER STREQUAL "pacman")
    set(CL_PACKAGE_NAME "sdl2" PARENT_SCOPE)
  elseif(CL_PACKAGE_MANAGER STREQUAL "brew")
    set(CL_PACKAGE_NAME "sdl2" PARENT_SCOPE)
  elseif(CL_PACKAGE_MANAGER STREQUAL "yum")
    set(CL_PACKAGE_NAME "SDL2-devel" PARENT_SCOPE)
  elseif(CL_PACKAGE_MANAGER STREQUAL "apk")
    set(CL_PACKAGE_NAME "sdl2-dev" PARENT_SCOPE)
  elseif(CL_PACKAGE_MANAGER STREQUAL "zypper")
    set(CL_PACKAGE_NAME "libSDL2-devel" PARENT_SCOPE)
  endif()
endfunction()

function(_recipe_SDL2_source)
  set(SDL_BUILD_SHARED "ON")
  set(SDL_BUILD_STATIC "ON")

  if(CL_REQ_TYPE STREQUAL "STATIC" OR CL_REQ_TYPE STREQUAL "PREFER_STATIC")
    set(SDL_BUILD_SHARED "OFF")
    set(SDL_BUILD_STATIC "ON")
  elseif(CL_REQ_TYPE STREQUAL "SHARED" OR CL_REQ_TYPE STREQUAL "PREFER_SHARED")
    set(SDL_BUILD_SHARED "ON")
    set(SDL_BUILD_STATIC "OFF")
  endif()

  set(SDL_URL_BASE "https://github.com/libsdl-org/SDL/archive/refs/tags")
  set(SDL_TAG "release-2.32.10")
  if(CL_REQ_VERSION)
    set(SDL_TAG "release-${CL_REQ_VERSION}")
  endif()
  set(SDL_URL "${SDL_URL_BASE}/${SDL_TAG}.tar.gz")

  set(SDL_PLATFORM_OPTIONS "")
  if(UBUNTU_TOUCH) # custom 2.0.18 with re-added mir backend
    set(SDL_URL "https://github.com/Dogo6647/SDL2/archive/refs/heads/main.tar.gz")
    set(SDL_PLATFORM_OPTIONS
      "SDL_INSTALL" "OFF"
      #"SDL_X11" "OFF"
      #"SDL_VIDEO_DRIVER_X11" "OFF"
      "SDL_MIR" "ON"
      "SDL_VIDEO_MIR" "ON"
      "SDL_VIDEO_DRIVER_MIR" "ON"
      "SDL_PTHREADS" "ON"
      "SDL_PTHREADS_SEM" "ON")
  endif()

  cl_import_source(
    NAME SDL2
    URL ${SDL_URL}
    OPTIONS "SDL_SHARED" "${SDL_BUILD_SHARED}" "SDL_STATIC" "${SDL_BUILD_STATIC}" "SDL_TEST" "OFF" ${SDL_PLATFORM_OPTIONS}
  )
endfunction()
