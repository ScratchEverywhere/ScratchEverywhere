if(TARGET SDL3_gfx::SDL3_gfx)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(SDL3_gfx IMPORTED_TARGET SDL3_gfx>=1.0.0)
	if(SDL3_gfx_FOUND)
		add_library(SDL3_gfx::SDL3_gfx ALIAS PkgConfig::SDL3_gfx)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT SDL3_gfx_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME SDL3_gfx
		VERSION 1.0.1
		GITHUB_REPOSITORY sabdul-khabir/SDL3_gfx
	)

	if(TARGET SDL3_gfx_Static)
		add_library(SDL3_gfx::SDL3_gfx ALIAS SDL3_gfx_Static)
	elseif(TARGET SDL3_gfx_Shared)
		add_library(SDL3_gfx::SDL3_gfx ALIAS SDL3_gfx_Shared)
	endif()
endif()

if(NOT TARGET SDL3_gfx::SDL3_gfx)
	message(
		FATAL_ERROR
		"Failed to get SDL3_gfx."
	)
endif()
