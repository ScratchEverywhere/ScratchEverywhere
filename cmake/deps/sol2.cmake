include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(lua51)

function(_dep_system_sol2)
	find_package(sol2 CONFIG QUIET)
endfunction()

function(_dep_source_sol2)
	CPMAddPackage(
	  NAME sol2
	  GITHUB_REPOSITORY ThePhD/sol2
	  VERSION 3.3.0
	  GIT_TAG develop # Fixes sol::optional error
	  OPTIONS "SOL2_ENABLE_INSTALL OFF"
	)
	add_library(deps::sol2 INTERFACE IMPORTED)
    target_link_libraries(deps::sol2 INTERFACE sol2 deps::lua51)
endfunction()
