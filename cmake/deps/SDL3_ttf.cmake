function(_dep_system_SDL3_ttf)
	pkg_check_modules(SDL3_ttf IMPORTED_TARGET SDL3_ttf>=3.0.0)
endfunction()
	
function(_dep_source_SDL3_ttf)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3_ttf
		VERSION 3.2.2
		GITHUB_REPOSITORY libsdl-org/SDL_ttf
		GIT_TAG release-3.2.2
		OPTIONS "SDLTTF_VENDORED ON"
	)
endfunction()
