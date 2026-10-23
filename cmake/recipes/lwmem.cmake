function(_recipe_lwmem_source)
    set(LWMEM_REF "develop")
    if(CL_REQ_VERSION)
        set(LWMEM_REF "${CL_REQ_VERSION}")
    endif()

    cl_import_source(
        NAME lwmem
        DOWNLOAD_ONLY
        REPO https://github.com/MaJerle/lwmem.git
        REF ${LWMEM_REF}
    )

    set(LWMEM_ROOT "${CL_SOURCE_DIR}/lwmem")
    if(NOT EXISTS "${LWMEM_ROOT}/src/lwmem/lwmem.c")
        _catalog_log(FATAL_ERROR "lwmem: expected sources not found under ${LWMEM_ROOT}")
    endif()

    add_library(lwmem INTERFACE)
    target_sources(lwmem INTERFACE "${LWMEM_ROOT}/src/lwmem/lwmem.c")
    target_include_directories(lwmem INTERFACE "${LWMEM_ROOT}/src/include")

    # Fall back to upstream's template when the platform ships no options file
    if(NOT XBOX)
        set(LWMEM_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/lwmem_gen")
        configure_file("${LWMEM_ROOT}/src/include/lwmem/lwmem_opts_template.h" "${LWMEM_GEN_DIR}/lwmem_opts.h" COPYONLY)
        target_include_directories(lwmem INTERFACE "${LWMEM_GEN_DIR}")
    endif()

    if(NOT TARGET deps::lwmem)
        add_library(deps::lwmem ALIAS lwmem)
    endif()
endfunction()
