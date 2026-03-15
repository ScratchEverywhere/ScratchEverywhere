if(TARGET SDL3_mixer::SDL3_mixer)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL3_mixer IMPORTED_TARGET SDL3_mixer>=3.0.0)
	if(SDL3_mixer_FOUND)
		add_library(SDL3_mixer::SDL3_mixer ALIAS PkgConfig::SDL3_mixer)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL3_mixer_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3_mixer
		VERSION 3
		GITHUB_REPOSITORY libsdl-org/SDL_mixer
		GIT_TAG main # TODO: pin version
		OPTIONS "SDLMIXER_TESTS OFF" "SDLMIXER_EXAMPLES OFF" "SDLMIXER_MP3_MPG123_SHARED OFF" "SDLMIXER_MP3_DRMP3 OFF" "SDLMIXER_MP3_MPG123 ON" "CMAKE_DISABLE_FIND_PACKAGE_Opus ON" "CMAKE_DISABLE_FIND_PACKAGE_OpusFile ON" "CMAKE_DISABLE_FIND_PACKAGE_Ogg ON" "CMAKE_DISABLE_FIND_PACKAGE_Vorbis ON" "CMAKE_DISABLE_FIND_PACKAGE_FLAC ON" "CMAKE_DISABLE_FIND_PACKAGE_gme ON" "CMAKE_DISABLE_FIND_PACKAGE_libxmp ON" "SDLMIXER_MIDI OFF"
	)

	if(NOT TARGET SDL3_mixer::SDL3_mixer AND TARGET SDL3_mixer::SDL3_mixer-static)
		add_library(SDL3_mixer::SDL3_mixer ALIAS SDL3_mixer::SDL3_mixer-static)
	endif()
endif()

if(NOT TARGET SDL3_mixer::SDL3_mixer)
	message(
		FATAL_ERROR
		"Failed to get SDL3_mixer."
	)
endif()
