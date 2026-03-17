function(_dep_system_SDL3_ttf)
	pkg_check_modules(SDL3_ttf IMPORTED_TARGET SDL3_ttf>=3.0.0)
endfunction()
	
function(_dep_source_SDL3_ttf)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

	se_add_dependency(plutosvg)

	CPMAddPackage(
		NAME SDL3_ttf
		VERSION 3.2.2
		GITHUB_REPOSITORY libsdl-org/SDL_ttf
		GIT_TAG release-3.2.2
		OPTIONS "SDLTTF_HARFBUZZ_VENDORED ON" "SDLTTF_FREETYPE_VENDORED OFF" "SDLTTF_PLUTOSVG_VENDORED OFF" "plutosvg_FOUND TRUE" # See plutosvg.cmake for why freetype isn't being vendored atm
	)
endfunction()
