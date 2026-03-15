if(TARGET SDL3::SDL3)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL3 IMPORTED_TARGET sdl3>=3.0.0)
	if(SDL3_FOUND)
		add_library(SDL3::SDL3 ALIAS PkgConfig::SDL3)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL3_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3
		VERSION 3.2.26
		GITHUB_REPOSITORY libsdl-org/SDL
		GIT_TAG main # Fixes SDL3_Mixer, we're wating for the 3.4.0 release
		OPTIONS "SDL_TEST_LIBRARY OFF"
	)
endif()

if(NOT TARGET SDL3::SDL3)
	message(
		FATAL_ERROR
		"Failed to get SDL3."
	)
endif()
