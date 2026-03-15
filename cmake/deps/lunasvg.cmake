if(TARGET lunasvg::lunansvg)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(lunasvg CONFIG QUIET)
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET lunasvg::lunasvg) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/plutovg.cmake") # For the fixes we apply
	CPMAddPackage(
		NAME lunasvg
		GITHUB_REPOSITORY sammycage/lunasvg
		VERSION 3.5.0
		OPTIONS "LUNASVG_BUILD_EXAMPLES OFF" "plutovg_FOUND ON"
	)
endif()

if(NOT TARGET lunasvg::lunasvg)
	message(
		FATAL_ERROR
		"Failed to get LunaSVG."
	)
endif()
