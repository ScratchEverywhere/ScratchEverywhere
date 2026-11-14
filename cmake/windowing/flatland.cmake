set(SE_FLATLAND_LOCAL_DIR "" CACHE PATH "Use this flatland checkout instead of Catalog's gh:gradylink/flatland.") # TEMP

enable_language(ASM)

if(SE_FLATLAND_LOCAL_DIR)
    set(FLATLAND_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    add_subdirectory("${SE_FLATLAND_LOCAL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/flatland-local" EXCLUDE_FROM_ALL)
    target_link_libraries(windowing_interface INTERFACE flatland)
else()
    cl_add_dep(windowing_interface gh:gradylink/flatland)
endif()
