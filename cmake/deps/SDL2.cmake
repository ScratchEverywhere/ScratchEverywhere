if(EMSCRIPTEN)
	add_library(SDL2 INTERFACE)
	target_compile_options(SDL2 INTERFACE "-sUSE_SDL=2")
	target_link_options(SDL2 INTERFACE "-sUSE_SDL=2")
endif()

function(_dep_system_SDL2)
	if(NOT CMAKE_CROSSCOMPILING)
		find_package(SDL2 QUIET)
	endif()
	if(NOT SDL2_FOUND)
		pkg_check_modules(SDL2 IMPORTED_TARGET sdl2>=2.0.0)
	elseif(SDL2_FOUND AND NOT TARGET SDL2::SDL2) # workaround for older SDL2 versions
		add_library(SDL2::SDL2 INTERFACE IMPORTED)
	    target_include_directories(SDL2::SDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
	    separate_arguments(SDL2_LIBRARIES_LIST NATIVE_COMMAND "${SDL2_LIBRARIES}")
	    target_link_options(SDL2::SDL2 INTERFACE ${SDL2_LIBRARIES_LIST})
	endif()
endfunction()

function(_dep_source_SDL2)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CROSSCOMPILING)
		set(SDL2_OPTIONS "SDL_COCOA ON" "SDL_RENDER_METAL OFF" "SDL_OPENGL ON" "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF") # TODO: Fix compiler-rt issues
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.11)
		set(SDL2_OPTIONS "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF")
	elseif(UBUNTU_TOUCH)
		set(SDL2_OPTIONS "SDL_X11 OFF" "SDL_VIDEO_DRIVER_X11 OFF" "SDL_VIDEO_DRIVER_MIR ON")
	else()
		set(SDL2_OPTIONS "")
	endif()
	if(UBUNTU_TOUCH)
		set(SDL2_VERSION "2.0.9")
	else()
		set(SDL2_VERSION "2.32.10")
	endif()
	CPMAddPackage(
		NAME SDL2
		VERSION ${SDL2_VERSION}
		GITHUB_REPOSITORY libsdl-org/SDL
		GIT_TAG release-${SDL2_VERSION}
		OPTIONS ${SDL2_OPTIONS}
	)
	if(TARGET SDL2 AND NOT TARGET SDL2::SDL2) # for older SDL2
	    add_library(SDL2::SDL2 ALIAS SDL2)
	endif()
endfunction()
