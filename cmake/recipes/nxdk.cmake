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

    if(NOT EXISTS "${_nxdk}/lib/libnxdk.lib" OR NOT EXISTS "${_nxdk}/lib/libnxdk_automount_d.lib" OR NOT EXISTS "${_nxdk}/lib/libxboxrt.lib")
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

    find_program(_cxbe_bin cxbe PATHS "${_nxdk}/bin" "${_nxdk}/tools/cxbe" NO_DEFAULT_PATH)
    if(NOT _cxbe_bin)
        message(STATUS "Building nxdk tools (cxbe, extract-xiso)...")
        execute_process(
            COMMAND make tools -j
            WORKING_DIRECTORY "${_nxdk}"
            RESULT_VARIABLE _make_tools
        )
        if(NOT _make_tools EQUAL 0)
            message(FATAL_ERROR "Failed to build nxdk tools")
        endif()
    endif()

    if(EXISTS "${_nxdk}/tools/cxbe/cxbe" AND NOT EXISTS "${_nxdk}/bin/cxbe")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${_nxdk}/tools/cxbe/cxbe" "${_nxdk}/bin/cxbe")
    endif()

    if(EXISTS "${_nxdk}/tools/extract-xiso/build/extract-xiso" AND NOT EXISTS "${_nxdk}/bin/extract-xiso")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy "${_nxdk}/tools/extract-xiso/build/extract-xiso" "${_nxdk}/bin/extract-xiso")
    endif()

    add_library(nxdk INTERFACE)
endfunction()
