function(_dep_system_SDL_mixer)
	pkg_check_modules(SDL_mixer IMPORTED_TARGET SDL_mixer>=1.2.0)
endfunction()

function(_dep_source_SDL_mixer)
	message(
		FATAL_ERROR
		"SDL_mixer doesn't support building from source."
	)
endfunction()
