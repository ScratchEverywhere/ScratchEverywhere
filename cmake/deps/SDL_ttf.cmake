if(NINTENDO_WII)
	add_library(_SDL_ttf INTERFACE)
	target_link_libraries(_SDL_ttf INTERFACE SDL_ttf freetype png bz2 z brotlidec brotlicommon)
	target_include_directories(_SDL_ttf INTERFACE ${DEVKITPRO}/portlibs/wii/include/SDL)
	add_library(deps::SDL_ttf ALIAS _SDL_ttf)
endif()

function(_dep_system_SDL_ttf)
	pkg_check_modules(SDL_ttf IMPORTED_TARGET SDL_ttf>=1.2.0)
endfunction()

function(_dep_source_SDL_ttf)
	message(
		FATAL_ERROR
		"SDL_ttf doesn't support building from source."
	)
endfunction()
