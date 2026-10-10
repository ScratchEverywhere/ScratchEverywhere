function(_recipe_lunasvg_system)
    if(XBOX OR CMAKE_CROSSCOMPILING)
        return()
    endif()
    find_package(lunasvg CONFIG QUIET)
endfunction()

function(_recipe_lunasvg_source)
    cl_add_dep(plutovg)
    if(NOT TARGET plutovg::plutovg)
        _catalog_log(FATAL_ERROR "lunasvg: could not resolve a plutovg::plutovg target")
    endif()

    set(LUNASVG_TAG "v3.5.0")
    if(CL_REQ_VERSION)
        set(LUNASVG_TAG "v${CL_REQ_VERSION}")
    endif()

    cl_import_source(
        NAME lunasvg
        URL https://github.com/sammycage/lunasvg/archive/refs/tags/${LUNASVG_TAG}.tar.gz
        OPTIONS "LUNASVG_BUILD_EXAMPLES" "OFF" "plutovg_FOUND" "TRUE"
    )
endfunction()
