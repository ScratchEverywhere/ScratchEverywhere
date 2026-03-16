function(_dep_system_SDL2_ttf)
	if(NOT CMAKE_CROSSCOMPILING)
		find_package(SDL2_ttf QUIET)
	endif()
	if(NOT SDL2_ttf_FOUND)
		pkg_check_modules(SDL2_ttf QUIET SDL2_ttf>=2.0.0)
		if(SDL2_ttf_FOUND)
			add_library(SDL2_ttf::SDL2_ttf INTERFACE IMPORTED)
			set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${SDL2_ttf_INCLUDE_DIRS}"
				INTERFACE_LINK_LIBRARIES "${SDL2_ttf_LIBRARIES}"
			)
		endif()
	endif()
endfunction()

function(_dep_source_SDL2_ttf)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL2_ttf
		VERSION 2.24.0
		GITHUB_REPOSITORY libsdl-org/SDL_ttf
		GIT_TAG release-2.24.0
		OPTIONS "SDL2TTF_VENDORED ON"
	)
endfunction()
