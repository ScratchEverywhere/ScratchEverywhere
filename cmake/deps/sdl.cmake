if(TARGET SDL::SDL)
	return()
endif()

if(SE_DEPS STREQUAL "source")
	message(
		FATAL_ERROR
		"SDL doesn't support building from source."
	)
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL IMPORTED_TARGET sdl>=1.2.0)
	if(SDL_FOUND)
		add_library(SDL::SDL ALIAS PkgConfig::SDL)
	endif()
endif()

if(NOT TARGET SDL::SDL)
	message(
		FATAL_ERROR
		"Failed to get SDL."
	)
endif()
