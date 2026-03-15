if(TARGET SDL2_gfx::SDL2_gfx)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL2_gfx IMPORTED_TARGET SDL2_gfx>=1.0.0)
	if(SDL2_gfx_FOUND)
		add_library(SDL2_gfx::SDL2_gfx ALIAS PkgConfig::SDL2_gfx)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL2_gfx_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

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
	add_library(SDL2_gfx::SDL2_gfx ALIAS SDL2_gfx)
	target_include_directories(SDL2_gfx PUBLIC
		$<BUILD_INTERFACE:${SDL2_gfx_SOURCE_DIR}>
	)
	target_link_libraries(SDL2_gfx PUBLIC
		SDL2::SDL2
	)
endif()

if(NOT TARGET SDL2_gfx::SDL2_gfx)
	message(
		FATAL_ERROR
		"Failed to get SDL2_gfx."
	)
endif()
