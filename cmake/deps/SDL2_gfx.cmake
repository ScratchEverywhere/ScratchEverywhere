if(EMSCRIPTEN)
	add_library(SDL2_gfx INTERFACE)
	target_compile_options(SDL2_gfx INTERFACE "-sUSE_SDL_GFX=2")
	target_link_options(SDL2_gfx INTERFACE "-sUSE_SDL_GFX=2")
endif()

function(_dep_system_SDL2_gfx)
	pkg_check_modules(SDL2_gfx IMPORTED_TARGET SDL2_gfx>=1.0.0)
endfunction()

function(_dep_source_SDL2_gfx)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

	CPMAddPackage(
		NAME SDL2_gfx
		VERSION 1.0.4
		GITHUB_REPOSITORY giroletm/SDL2_gfx
		GIT_TAG release-1.0.4
		DOWNLOAD_ONLY TRUE # SDL2_gfx doesn't have a CMakeLists.txt
	)

	file(GLOB SDL2_GFX_SOURCES
		"${SDL2_gfx_SOURCE_DIR}/SDL2_rotozoom.c"
		"${SDL2_gfx_SOURCE_DIR}/SDL2_framerate.c"
		"${SDL2_gfx_SOURCE_DIR}/SDL2_imageFilter.c"
		"${SDL2_gfx_SOURCE_DIR}/SDL2_gfxPrimitives.c"
		"${SDL2_gfx_SOURCE_DIR}/SDL2_bgi.c"
	)
	add_library(SDL2_gfx STATIC ${SDL2_GFX_SOURCES})
	target_include_directories(SDL2_gfx PUBLIC
		$<BUILD_INTERFACE:${SDL2_gfx_SOURCE_DIR}>
	)
	se_add_dependency(SDL2_gfx SDL2)
endfunction()
