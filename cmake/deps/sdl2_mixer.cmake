if(TARGET SDL2_mixer::SDL2_mixer)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(SDL2_mixer REQUIRED IMPORTED_TARGET SDL2_mixer>=2.0.0)
		if(SDL2_mixer_FOUND)
			add_library(SDL2_mixer::SDL2_mixer ALIAS PkgConfig::SDL2_mixer)
		endif()
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL2_mixer_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL2_mixer
		VERSION 2.8.1
		GITHUB_REPOSITORY libsdl-org/SDL_mixer
		GIT_TAG release-2.8.1
		OPTIONS "SDL2MIXER_VORBIS STB" "SDL2MIXER_MP3 MINIMP3" "SDL2MIXER_FLAC DRFLAC" "SDL2MIXER_MOD OFF" "SDL2MIXER_OPUS OFF" "SDL2MIXER_FLUIDSYNTH OFF" "SDL2MIXER_WAVPACK OFF" "SDL2MIXER_MIDI OFF"
	)

	if(NOT TARGET SDL2_mixer::SDL2_mixer AND TARGET SDL2_mixer::SDL2_mixer-static)
		add_library(SDL2_mixer::SDL2_mixer ALIAS SDL2_mixer::SDL2_mixer-static)
	endif()
endif()

if(NOT TARGET SDL2_mixer::SDL2_mixer)
	message(
		FATAL_ERROR
		"Failed to get SDL2_mixer."
	)
endif()
