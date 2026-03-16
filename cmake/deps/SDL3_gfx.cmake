function(_dep_system_SDL3_gfx)
	pkg_check_modules(SDL3_gfx IMPORTED_TARGET SDL3_gfx>=1.0.0)
endfunction()

function(_dep_source_SDL3_gfx)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3_gfx
		VERSION 1.0.1
		GITHUB_REPOSITORY sabdul-khabir/SDL3_gfx
	)

	if(TARGET SDL3_gfx_Static)
		add_library(SDL3_gfx::SDL3_gfx ALIAS SDL3_gfx_Static)
	elseif(TARGET SDL3_gfx_Shared)
		add_library(SDL3_gfx::SDL3_gfx ALIAS SDL3_gfx_Shared)
	endif()
endfunction()
