include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(plutovg)

function(_dep_system_plutosvg)
	find_package(plutosvg QUIET)
endfunction()

function(_dep_source_plutosvg)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME plutosvg
		GITHUB_REPOSITORY sammycage/plutosvg
		VERSION 0.0.7
		OPTIONS "PLUTOSVG_BUILD_EXAMPLES OFF" "PLUTOSVG_ENABLE_FREETYPE ON" # Currently only used for SDL3_ttf source so we need freetype integration, the issue is that it will always use system freetype which should pretty much always be ok but in the future we might need to do some patching
	)
endfunction()
