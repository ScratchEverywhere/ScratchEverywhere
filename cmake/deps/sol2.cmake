include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(lua51)
if(NOT TARGET lua51)
	get_target_property(IS_ALIAS deps::sol2 ALIASED_TARGET)
	if(IS_ALIAS)
		add_library(lua51 ALIAS ${IS_ALIAS})
	else()
		add_library(lua51 ALIAS deps::lua51)
	endif()
endif()

function(_dep_system_sol2)
	find_package(sol2 CONFIG QUIET)
endfunction()

function(_dep_system_sol2)
	CPMAddPackage(
	  NAME sol2
	  GITHUB_REPOSITORY ThePhD/sol2
	  VERSION 3.3.0
	  GIT_TAG develop
	  OPTIONS "SOL2_ENABLE_INSTALL OFF"
	)
	target_link_libraries(sol2 INTERFACE deps::lua51)
endfunction()
