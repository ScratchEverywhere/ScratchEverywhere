if(TARGET SDL2_ttf::SDL2_ttf)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
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
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL2_ttf_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL2_ttf
		VERSION 2.24.0
		GITHUB_REPOSITORY libsdl-org/SDL_ttf
		GIT_TAG release-2.24.0
		OPTIONS "SDL2TTF_VENDORED ON"
	)
endif()

if(NOT TARGET SDL2_ttf::SDL2_ttf AND TARGET SDL2_ttf::SDL2_ttf-static)
	add_library(SDL2_ttf::SDL2_ttf ALIAS SDL2_ttf::SDL2_ttf-static)
endif()

if(NOT TARGET SDL2_ttf::SDL2_ttf)
	message(
		FATAL_ERROR
		"Failed to get SDL2_ttf."
	)
endif()
