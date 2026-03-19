if(EMSCRIPTEN)
	add_library(SDL2_mixer INTERFACE)
	target_compile_options(SDL2_mixer INTERFACE "-sUSE_SDL_MIXER=2" "-sSDL_MIXER_FORMATS='[\"ogg\",\"mp3\"]'")
	target_link_options(SDL2_mixer INTERFACE "-sUSE_SDL_MIXER=2" "-sSDL_MIXER_FORMATS='[\"ogg\",\"mp3\"]'")
endif()

function(_dep_system_SDL2_mixer)
	pkg_check_modules(SDL2_mixer IMPORTED_TARGET SDL2_mixer>=2.0.0)
endfunction()

function(_dep_source_SDL2_mixer)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL2_mixer
		VERSION 2.8.1
		GITHUB_REPOSITORY libsdl-org/SDL_mixer
		GIT_TAG release-2.8.1
		OPTIONS "SDL2MIXER_VORBIS STB" "SDL2MIXER_MP3 MINIMP3" "SDL2MIXER_FLAC DRFLAC" "SDL2MIXER_MOD OFF" "SDL2MIXER_OPUS OFF" "SDL2MIXER_FLUIDSYNTH OFF" "SDL2MIXER_WAVPACK OFF" "SDL2MIXER_MIDI OFF"
	)
endfunction()
