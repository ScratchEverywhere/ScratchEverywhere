if(TARGET SDL2::SDL2)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(SDL2 IMPORTED_TARGET sdl2>=2.0.0)
		if(SDL2_FOUND)
			add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
		endif()
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL2_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CROSSCOMPILING)
		set(SDL2_OPTIONS "SDL_COCOA OFF" "SDL_RENDER_METAL OFF" "SDL_OPENGL ON" "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF") # TODO: Fix compiler-rt issues
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.11)
		set(SDL2_OPTIONS "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF")
	else()
		set(SDL2_OPTIONS "")
	endif()
	CPMAddPackage(
		NAME SDL2
		VERSION 2.32.10
		GITHUB_REPOSITORY libsdl-org/SDL
		GIT_TAG release-2.32.10
		OPTIONS ${SDL2_OPTIONS}
	)
endif()

if(NOT TARGET SDL2::SDL2)
	message(
		FATAL_ERROR
		"Failed to get SDL2."
	)
endif()
