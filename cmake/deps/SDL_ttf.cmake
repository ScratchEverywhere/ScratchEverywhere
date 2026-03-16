function(_dep_system_SDL_ttf)
	pkg_check_modules(SDL_ttf IMPORTED_TARGET SDL_ttf>=1.2.0)
endfunction()

function(_dep_source_SDL_ttf)
	message(
		FATAL_ERROR
		"SDL_ttf doesn't support building from source."
	)
endfunction()
