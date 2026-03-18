function(_dep_system_SDL_gfx)
	pkg_check_modules(SDL_gfx IMPORTED_TARGET SDL_gfx>=1.0.0)
endfunction()

function(_dep_source_SDL_gfx)
	message(
		FATAL_ERROR
		"SDL_gfx doesn't support building from source."
	)
endfunction()
