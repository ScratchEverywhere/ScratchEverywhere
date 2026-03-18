function(_dep_system_lunasvg)
	find_package(lunasvg CONFIG QUIET)
endfunction()

function(_dep_source_lunasvg)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
	
	se_add_dependency(plutovg)

	CPMAddPackage(
		NAME lunasvg
		GITHUB_REPOSITORY sammycage/lunasvg
		VERSION 3.5.0
		OPTIONS "LUNASVG_BUILD_EXAMPLES OFF" "plutovg_FOUND ON"
	)
endfunction()
