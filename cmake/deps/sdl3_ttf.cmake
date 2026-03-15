if(TARGET SDL3_ttf::SDL3_ttf)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL3_ttf IMPORTED_TARGET SDL3_ttf>=3.0.0)
	if(SDL3_mixer_FOUND)
		add_library(SDL3_ttf::SDL3_ttf ALIAS PkgConfig::SDL3_ttf)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL3_ttf_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3_ttf
		VERSION 3.2.2
		GITHUB_REPOSITORY libsdl-org/SDL_ttf
		GIT_TAG release-3.2.2
		OPTIONS "SDLTTF_VENDORED ON"
	)

	if(NOT TARGET SDL3_ttf::SDL3_ttf AND TARGET SDL3_ttf::SDL3_ttf-static)
		add_library(SDL3_ttf::SDL3_ttf ALIAS SDL3_ttf::SDL3_ttf-static)
	endif()
endif()

if(NOT TARGET SDL3_ttf::SDL3_ttf)
	message(
		FATAL_ERROR
		"Failed to get SDL3_ttf."
	)
endif()
