function(_recipe_nxdk_toolchain)
endfunction()

function(_recipe_nxdk_system)
endfunction()

function(_recipe_nxdk_package)
endfunction()

function(_recipe_nxdk_source)
    if(NOT XBOX)
        return()
    endif()

    set(NXDK_REPO "https://github.com/xboxDev/nxdk.git")
    if(DEFINED ENV{NXDK_DIR} AND EXISTS "$ENV{NXDK_DIR}")
        set(NXDK_REPO "$ENV{NXDK_DIR}")
    endif()

    set(NXDK_REF "14d5ee97e73347c973f1f57b68b79ec08c9e77f2")
    if(CL_REQ_VERSION)
        set(NXDK_REF "${CL_REQ_VERSION}")
    endif()

    cl_import_source(
        NAME nxdk
        DOWNLOAD_ONLY
        REPO "${NXDK_REPO}"
        REF "${NXDK_REF}"
        PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/nxdk.patch"
    )

    set(_nxdk "${CL_SOURCE_DIR}")
    if(DEFINED ENV{NXDK_DIR} AND EXISTS "$ENV{NXDK_DIR}")
        set(_nxdk "$ENV{NXDK_DIR}")
    endif()

    if(NOT EXISTS "${_nxdk}/lib/libnxdk.lib")
        message(STATUS "Building nxdk (this can take a while)...")
        execute_process(
            COMMAND make -j NXDK_ONLY=y
            WORKING_DIRECTORY "${_nxdk}"
            RESULT_VARIABLE _make
        )
        if(NOT _make EQUAL 0)
            message(FATAL_ERROR "Failed to build nxdk")
        endif()
    endif()

    add_library(nxdk INTERFACE)
endfunction()
