if(TARGET miniz::miniz)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(miniz CONFIG QUIET)
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET miniz::miniz) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME miniz
		GITHUB_REPOSITORY richgel999/miniz
		VERSION 3.1.1
		GIT_TAG 3.1.1
	)
	add_library(miniz::miniz ALIAS miniz)
endif()

if(NOT TARGET miniz::miniz)
	message(
		FATAL_ERROR
		"Failed to get Miniz."
	)
endif()
