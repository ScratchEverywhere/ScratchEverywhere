if(TARGET SDL_gfx::SDL_gfx)
	return()
endif()

if(SE_DEPS STREQUAL "source")
	message(
		FATAL_ERROR
		"SDL_gfx doesn't support building from source."
	)
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL_gfx REQUIRED SDL_gfx>=1.0.0)
	if(SDL_gfx_FOUND)
		add_library(SDL_gfx::SDL_gfx ALIAS PkgConfig::SDL_gfx)
	endif()
endif()

if(NOT TARGET SDL_gfx::SDL_gfx)
	message(
		FATAL_ERROR
		"Failed to get SDL_gfx."
	)
endif()
