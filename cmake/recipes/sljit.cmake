function(_recipe_sljit_system)
  find_path(SLJIT_INCLUDE_DIR NAMES sljitLir.h PATH_SUFFIXES sljit)
  find_library(SLJIT_LIBRARY NAMES sljit)
  if(SLJIT_INCLUDE_DIR AND SLJIT_LIBRARY)
    add_library(_sljit_system UNKNOWN IMPORTED GLOBAL)
    set_target_properties(_sljit_system PROPERTIES
      IMPORTED_LOCATION "${SLJIT_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SLJIT_INCLUDE_DIR}"
    )
    add_library(deps::sljit ALIAS _sljit_system)
  endif()
endfunction()

function(_recipe_sljit_source)
  set(SLJIT_REF "39c508d1b8fbae332d60c7450db20236913089da")
  if(CL_REQ_VERSION)
    set(SLJIT_REF "${CL_REQ_VERSION}")
  endif()

  cl_import_source(
    NAME sljit
    DOWNLOAD_ONLY
    REPO https://github.com/zherczeg/sljit.git
    REF ${SLJIT_REF}
    PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/sljit.patch"
  )
  if(CL_INSTALL_PREFIX)
    return()
  endif()

  add_library(sljit STATIC "${CL_SOURCE_DIR}/sljit_src/sljitLir.c")
  target_include_directories(sljit PUBLIC $<BUILD_INTERFACE:${CL_SOURCE_DIR}/sljit_src>)
  set_target_properties(sljit PROPERTIES POSITION_INDEPENDENT_CODE ON)
  find_package(Threads QUIET)
  if(Threads_FOUND)
    target_link_libraries(sljit PUBLIC Threads::Threads)
  endif()
  if(CMAKE_DL_LIBS)
    target_link_libraries(sljit PUBLIC ${CMAKE_DL_LIBS})
  endif()

  add_library(deps::sljit ALIAS sljit)
endfunction()
