function(_nxdk_apply_patch DIR PATCH OUT_APPLIED)
    set(${OUT_APPLIED} FALSE PARENT_SCOPE)

    execute_process(
        COMMAND git apply --check "${PATCH}"
        WORKING_DIRECTORY "${DIR}"
        RESULT_VARIABLE _fwd OUTPUT_QUIET ERROR_QUIET
    )
    if(_fwd EQUAL 0)
        execute_process(
            COMMAND git apply "${PATCH}"
            WORKING_DIRECTORY "${DIR}"
            RESULT_VARIABLE _apply
        )
        if(NOT _apply EQUAL 0)
            message(FATAL_ERROR "Failed to apply ${PATCH} in ${DIR}")
        endif()
        message(STATUS "Applied ${PATCH}")
        set(${OUT_APPLIED} TRUE PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND git apply --reverse --check "${PATCH}"
        WORKING_DIRECTORY "${DIR}"
        RESULT_VARIABLE _rev OUTPUT_QUIET ERROR_QUIET
    )
    if(_rev EQUAL 0)
        message(STATUS "nxdk.patch already applied")
    else()
        message(FATAL_ERROR "${PATCH} neither applies nor is already applied in ${DIR}")
    endif()
endfunction()

function(_recipe_nxdk_toolchain)
    if(NOT XBOX)
        return()
    endif()

    set(_nxdk "$ENV{NXDK_DIR}")
    if(_nxdk STREQUAL "")
        message(FATAL_ERROR "NXDK_DIR is not set. Clone https://github.com/xboxDev/nxdk (commit 14d5ee97e73347c973f1f57b68b79ec08c9e77f2) with submodules and point NXDK_DIR at it.")
    endif()
    if(NOT EXISTS "${_nxdk}/bin/nxdk-cc")
        message(FATAL_ERROR "NXDK_DIR (${_nxdk}) does not look like an nxdk checkout")
    endif()

    set(_patched FALSE)
    set(_patch "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/nxdk.patch")
    if(EXISTS "${_patch}")
        _nxdk_apply_patch("${_nxdk}" "${_patch}" _patched)
    endif()

    if(_patched OR NOT EXISTS "${_nxdk}/lib/libnxdk.lib")
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

function(_recipe_nxdk_system)
endfunction()

function(_recipe_nxdk_package)
endfunction()

function(_recipe_nxdk_source)
    message(FATAL_ERROR "nxdk cannot be fetched automatically. Set NXDK_DIR to an nxdk checkout.")
endfunction()
