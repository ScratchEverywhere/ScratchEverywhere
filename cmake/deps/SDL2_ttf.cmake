if(EMSCRIPTEN)
	add_library(SDL2_ttf INTERFACE)
	target_compile_options(SDL2_ttf INTERFACE "-sUSE_SDL_TTF=2")
	target_link_options(SDL2_ttf INTERFACE "-sUSE_SDL_TTF=2")
endif()

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
	if(UBUNTU_TOUCH)
		set(SDL2TTF_VERSION "2.0.15")
	else()
		set(SDL2TTF_VERSION "2.24.0")
	endif()

	CPMAddPackage(
		NAME SDL2_ttf
		VERSION ${SDL2TTF_VERSION}
		GITHUB_REPOSITORY libsdl-org/SDL_ttf
		GIT_TAG release-${SDL2TTF_VERSION}
		OPTIONS "SDL2TTF_VENDORED ON"
	)

	if(TARGET SDL2_ttf) # for older sdl2_ttf
		target_include_directories(SDL2_ttf INTERFACE
			"$<BUILD_INTERFACE:${SDL2_ttf_SOURCE_DIR}/>"
		)
	endif()
endfunction()
