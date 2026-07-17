function(_dep_system_SDL3)
	pkg_check_modules(SDL3 IMPORTED_TARGET sdl3>=3.0.0)
endfunction()

function(_dep_source_SDL3)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3
		VERSION 3.2.26
		GITHUB_REPOSITORY libsdl-org/SDL
		GIT_TAG main # Fixes SDL3_Mixer, we're wating for the 3.4.0 release
		OPTIONS "SDL_TEST_LIBRARY OFF"
	)
endfunction()

# compiler
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT WEBOS)
	target_compile_definitions(SDL3 PUBLIC USE_SDL3_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
	target_compile_definitions(SDL3 PUBLIC USE_SDL3_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
	target_compile_definitions(SDL3 PUBLIC USE_SDL3_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
	target_compile_definitions(SDL3 PUBLIC USE_SDL3_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
	target_compile_definitions(SDL3 PUBLIC USE_SDL3_POLLEVENT)
elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
	target_compile_definitions(SDL3 PUBLIC USE_SDL3_POLLEVENT)
endif()
