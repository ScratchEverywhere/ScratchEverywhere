function(_dep_system_SDL)
	pkg_check_modules(SDL IMPORTED_TARGET sdl>=1.2.0)
endfunction()

function(_dep_source_SDL)
	message(
		FATAL_ERROR
		"SDL doesn't support building from source."
	)
endfunction()

# compiler
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT WEBOS)
	target_compile_definitions(SDL PUBLIC USE_SDL_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	target_compile_definitions(SDL PUBLIC USE_SDL_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
	target_compile_definitions(SDL PUBLIC USE_SDL_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	target_compile_definitions(SDL PUBLIC USE_SDL_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_compile_definitions(SDL PUBLIC USE_SDL_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
	target_compile_definitions(SDL PUBLIC USE_SDL_POLLEVENT)
endif()
