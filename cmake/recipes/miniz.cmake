function(_recipe_miniz_system)
    if(XBOX OR CMAKE_CROSSCOMPILING)
        return()
    endif()
    cl_format_pkgconfig_req("miniz" "${CL_VERSION_REQ}" PKG_SPEC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        if(CL_STATIC)
            pkg_check_modules(miniz IMPORTED_TARGET GLOBAL "--static" ${PKG_SPEC})
        else()
            pkg_check_modules(miniz IMPORTED_TARGET GLOBAL ${PKG_SPEC})
        endif()
    endif()
endfunction()

function(_recipe_miniz_package)
    if(CL_PACKAGE_MANAGER STREQUAL "apt")
        set(CL_PACKAGE_NAME "libminizip-dev" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "pacman")
        set(CL_PACKAGE_NAME "miniz" PARENT_SCOPE)
    endif()
endfunction()

function(_recipe_miniz_source)
    set(MINIZ_TAG "3.1.1")
    if(CL_REQ_VERSION)
        set(MINIZ_TAG "${CL_REQ_VERSION}")
    endif()

    set(MINIZ_BUILD_SHARED OFF)
    if(CL_REQ_TYPE STREQUAL "SHARED" OR CL_REQ_TYPE STREQUAL "PREFER_SHARED")
        set(MINIZ_BUILD_SHARED ON)
    endif()

    cl_import_source(
        NAME miniz
        URL https://github.com/richgel999/miniz/archive/refs/tags/${MINIZ_TAG}.tar.gz
        OPTIONS "BUILD_SHARED_LIBS" "${MINIZ_BUILD_SHARED}" "BUILD_EXAMPLES" "OFF" "BUILD_TESTS" "OFF" "BUILD_FUZZERS" "OFF"
    )
endfunction()
