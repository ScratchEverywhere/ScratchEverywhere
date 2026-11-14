function(_recipe_ScratchEverywhere_system)
  find_program(SE_SYSTEM_EXE NAMES scratch-everywhere scratch-pc)
  if(SE_SYSTEM_EXE AND NOT TARGET ScratchEverywhere)
    add_executable(ScratchEverywhere IMPORTED GLOBAL)
    set_target_properties(ScratchEverywhere PROPERTIES IMPORTED_LOCATION "${SE_SYSTEM_EXE}")
  endif()
endfunction()

function(_recipe_ScratchEverywhere_prebuilt)
  if(CMAKE_CROSSCOMPILING)
    return()
  endif()

  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(SE_ASSET "scratch-linux")
    set(SE_INSTALLED_NAME "scratch-pc")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(SE_ASSET "scratch-macOS")
    set(SE_INSTALLED_NAME "scratch-pc")
  elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
    set(SE_ASSET "scratch-windows.exe")
    set(SE_INSTALLED_NAME "scratch-pc.exe")
  else()
    return()
  endif()

  set(SE_REF "1.3")
  if(CL_REQ_VERSION)
    set(SE_REF "${CL_REQ_VERSION}")
  endif()
  cl_set_version("${SE_REF}")

  _catalog_get_cache_dir(SE_CACHE)
  set(SE_DL_DIR "${SE_CACHE}/prebuilt_ScratchEverywhere_${SE_REF}")
  set(SE_DL_FILE "${SE_DL_DIR}/${SE_ASSET}")
  file(MAKE_DIRECTORY "${SE_DL_DIR}")

  _catalog_is_var_defined("REFRESH_ScratchEverywhere" SE_REFRESH)
  if(SE_REFRESH OR NOT EXISTS "${SE_DL_FILE}")
    file(REMOVE "${SE_DL_FILE}")
    _catalog_download_file(
      "https://github.com/ScratchEverywhere/ScratchEverywhere/releases/download/${SE_REF}/${SE_ASSET}"
      "${SE_DL_FILE}"
    )
  endif()
  if(NOT EXISTS "${SE_DL_FILE}")
    return()
  endif()
  file(CHMOD "${SE_DL_FILE}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  _catalog_get_var("INSTALL_PREFIX" SE_PREFIX)
  if(SE_PREFIX)
    _catalog_get_var("INSTALL_DESTDIR" SE_DESTDIR)
    set(SE_BIN_DIR "${SE_DESTDIR}${SE_PREFIX}/bin")
    file(MAKE_DIRECTORY "${SE_BIN_DIR}")
    file(REMOVE "${SE_BIN_DIR}/${SE_INSTALLED_NAME}")
    set(CMAKE_INSTALL_MANIFEST_FILES "")
    file(INSTALL "${SE_DL_FILE}" DESTINATION "${SE_BIN_DIR}" RENAME "${SE_INSTALLED_NAME}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    _catalog_append_install_log("ScratchEverywhere" "${CMAKE_INSTALL_MANIFEST_FILES}")
    get_property(SE_CURRENT_PACKAGE GLOBAL PROPERTY _CATALOG_CURRENT_PACKAGE)
    foreach(SE_TGT ScratchEverywhere ${SE_CURRENT_PACKAGE})
      if(NOT "${SE_TGT}" STREQUAL "" AND NOT TARGET ${SE_TGT})
        add_library(${SE_TGT} INTERFACE IMPORTED GLOBAL)
      endif()
    endforeach()
  elseif(NOT TARGET ScratchEverywhere)
    add_executable(ScratchEverywhere IMPORTED GLOBAL)
    set_target_properties(ScratchEverywhere PROPERTIES IMPORTED_LOCATION "${SE_DL_FILE}")
  endif()
endfunction()

function(_recipe_ScratchEverywhere_source)
  set(SE_REF "1.3")
  if(CL_REQ_VERSION)
    set(SE_REF "${CL_REQ_VERSION}")
  endif()
  cl_set_version("${SE_REF}")

  cl_import_source(
    NAME ScratchEverywhere
    REPO https://github.com/ScratchEverywhere/ScratchEverywhere.git
    REF ${SE_REF}
  )

  if(TARGET scratch-everywhere)
    add_executable(deps::ScratchEverywhere ALIAS scratch-everywhere)
  endif()
endfunction()

function(_recipe_ScratchEverywhere_latest)
  set(SE_REF "1.3")
  if(CL_REQ_VERSION)
    set(SE_REF "${CL_REQ_VERSION}")
  endif()
  cl_set_version("${SE_REF}")
endfunction()

function(_recipe_ScratchEverywhere_install PREFIX)
  file(GLOB_RECURSE SE_BUILT_EXE
    "${CL_SOURCE_BUILD_DIR}/scratch-pc"
    "${CL_SOURCE_BUILD_DIR}/scratch-pc.exe"
  )
  if(NOT SE_BUILT_EXE)
    message(FATAL_ERROR "now how tf did this happen")
  endif()
  list(GET SE_BUILT_EXE 0 SE_BUILT_EXE)
  cl_install_files("${SE_BUILT_EXE}" DESTINATION bin)
endfunction()
