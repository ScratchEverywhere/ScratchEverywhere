if(TARGET SDL_ttf::SDL_ttf)
	return()
endif()

if(SE_DEPS STREQUAL "source")
	message(
		FATAL_ERROR
		"SDL_ttf doesn't support building from source."
	)
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL_ttf REQUIRED SDL_ttf>=1.2.0)
	if(SDL_ttf_FOUND)
		add_library(SDL_ttf::SDL_ttf ALIAS PkgConfig::SDL_ttf)
	endif()
endif()

if(NOT TARGET SDL_ttf::SDL_ttf)
	message(
		FATAL_ERROR
		"Failed to get SDL_ttf."
	)
endif()
