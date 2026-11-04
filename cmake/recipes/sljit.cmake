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

  set(SE_SLJIT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sljit")
  set(SE_SLJIT_EXEC "")
  if(SE_PLATFORM STREQUAL "switch")
    set(SE_SLJIT_EXEC se_exec_switch.c)
    set(SE_SLJIT_EXEC_DEFINITIONS SE_SLJIT_DUAL_MAPPING SE_SLJIT_WX)
  elseif(SE_PLATFORM STREQUAL "wiiu")
    set(SE_SLJIT_EXEC se_exec_wiiu.c)
    set(SE_SLJIT_EXEC_DEFINITIONS SE_SLJIT_WX)
  elseif(SE_PLATFORM STREQUAL "3ds")
    set(SE_SLJIT_EXEC se_exec_3ds.c)
    set(SE_SLJIT_EXEC_DEFINITIONS SE_SLJIT_FLUSH)
  elseif(SE_PLATFORM STREQUAL "vita")
    set(SE_SLJIT_EXEC se_exec_vita.c)
    set(SE_SLJIT_EXEC_DEFINITIONS SE_SLJIT_WX SE_SLJIT_FLUSH)
  elseif(SE_PLATFORM STREQUAL "wii" OR SE_PLATFORM STREQUAL "gamecube")
    set(SE_SLJIT_EXEC se_exec_ogc.c)
    set(SE_SLJIT_EXEC_DEFINITIONS "")
  endif()
  target_include_directories(sljit PUBLIC "${SE_SLJIT_DIR}")
  target_compile_definitions(sljit PUBLIC SLJIT_HAVE_CONFIG_PRE=1)
  if(SE_SLJIT_EXEC)
    target_sources(sljit PRIVATE "${SE_SLJIT_DIR}/${SE_SLJIT_EXEC}")
    target_compile_definitions(sljit PUBLIC SE_SLJIT_CUSTOM_EXEC ${SE_SLJIT_EXEC_DEFINITIONS})
  endif()
  if(NOT SE_SLJIT_EXEC)
    set_target_properties(sljit PROPERTIES POSITION_INDEPENDENT_CODE ON)

    find_package(Threads QUIET)
    if(Threads_FOUND)
      target_link_libraries(sljit PUBLIC Threads::Threads)
    endif()
    if(CMAKE_DL_LIBS)
      target_link_libraries(sljit PUBLIC ${CMAKE_DL_LIBS})
    endif()
  endif()

  add_library(deps::sljit ALIAS sljit)
endfunction()
