
# FIXME: use official catalog recipe
# see PR review

function(_recipe_miniz_source)
    set(MINIZ_TAG "3.0.2")
    if(CL_REQ_VERSION)
        set(MINIZ_TAG "${CL_REQ_VERSION}")
    endif()

    cl_import_source(
        NAME miniz
        DOWNLOAD_ONLY
        URL "https://github.com/richgel999/miniz/archive/refs/tags/${MINIZ_TAG}.tar.gz"
    )

    file(WRITE "${CL_SOURCE_DIR}/miniz_export.h" "#pragma once\n#define MINIZ_EXPORT\n")

    add_library(miniz STATIC
        "${CL_SOURCE_DIR}/miniz.c"
        "${CL_SOURCE_DIR}/miniz_tdef.c"
        "${CL_SOURCE_DIR}/miniz_tinfl.c"
        "${CL_SOURCE_DIR}/miniz_zip.c"
    )
    target_include_directories(miniz PUBLIC $<BUILD_INTERFACE:${CL_SOURCE_DIR}>)
    add_library(deps::miniz ALIAS miniz)
endfunction()
