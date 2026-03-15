if(TARGET SDL_mixer::SDL_mixer)
	return()
endif()

if(SE_DEPS STREQUAL "source")
	message(
		FATAL_ERROR
		"SDL_mixer doesn't support building from source."
	)
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL_mixer REQUIRED SDL_mixer>=1.2.0)
	if(SDL_mixer_FOUND)
		add_library(SDL_mixer::SDL_mixer ALIAS PkgConfig::SDL_mixer)
	endif()
endif()

if(NOT TARGET SDL_mixer::SDL_mixer)
	message(
		FATAL_ERROR
		"Failed to get SDL_mixer."
	)
endif()
