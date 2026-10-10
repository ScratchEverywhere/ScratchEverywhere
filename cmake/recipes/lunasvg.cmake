
# FIXME: use official recipe
# see PR review

function(_recipe_lunasvg_system)
    if(CL_REQ_VERSION)
        find_package(lunasvg ${CL_REQ_VERSION} CONFIG QUIET)
    else()
        find_package(lunasvg CONFIG QUIET)
    endif()
endfunction()

function(_recipe_lunasvg_source)
    cl_add_dep(plutovg)
    if(NOT TARGET plutovg::plutovg)
        _catalog_log(FATAL_ERROR "lunasvg: could not resolve a plutovg::plutovg target")
    endif()

    # FIXME: this is a temporary hack
    include(FetchContent)
    FetchContent_SetPopulated(plutovg)

    set(LUNASVG_TAG "v3.0.1")
    if(CL_REQ_VERSION)
        set(LUNASVG_TAG "v${CL_REQ_VERSION}")
    endif()

    set(LUNASVG_PATCHES)
    if(XBOX)
        set(LUNASVG_PATCHES PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/lunasvg.patch")
    endif()

    cl_import_source(
        NAME lunasvg
        URL https://github.com/sammycage/lunasvg/archive/refs/tags/${LUNASVG_TAG}.tar.gz
        OPTIONS "LUNASVG_BUILD_EXAMPLES" "OFF" "plutovg_FOUND" "TRUE"
        ${LUNASVG_PATCHES}
    )
endfunction()
