function(_recipe_mistpp_system)
    if(CMAKE_CROSSCOMPILING)
        return()
    endif()

    cl_format_pkgconfig_req("mist++" "${CL_VERSION_REQ}" PKG_SPEC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(mistpp IMPORTED_TARGET GLOBAL ${PKG_SPEC})
        if(mistpp_FOUND AND TARGET PkgConfig::mistpp)
            add_library(deps::mistpp ALIAS PkgConfig::mistpp)
        endif()
    endif()
endfunction()

function(_recipe_mistpp_source)
    set(MISTPP_TAG "v0.3.8")
    if(CL_REQ_VERSION)
        set(MISTPP_TAG "v${CL_REQ_VERSION}")
    endif()

    cl_import_source(
        NAME mistpp
        URL "https://github.com/ScratchEverywhere/mistpp/archive/refs/tags/${MISTPP_TAG}.tar.gz"
        OPTIONS "BUILD_TEST" "OFF"
    )

    if(TARGET mist++)
        add_library(deps::mistpp ALIAS mist++)
    endif()
endfunction()
