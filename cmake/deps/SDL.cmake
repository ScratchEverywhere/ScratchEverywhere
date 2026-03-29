function(_dep_system_SDL)
	pkg_check_modules(SDL IMPORTED_TARGET sdl>=1.2.0)
endfunction()

function(_dep_source_SDL)
	message(
		FATAL_ERROR
		"SDL doesn't support building from source."
	)
endfunction()
