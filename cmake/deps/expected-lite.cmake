if(TARGET nonstd::expected-lite)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(expected-lite CONFIG QUIET)
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET nonstd::expected-lite) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME expected-lite
		GITHUB_REPOSITORY nonstd-lite/expected-lite
		VERSION 0.10.0
	)
endif()

if(NOT TARGET nonstd::expected-lite)
	message(
		FATAL_ERROR
		"Failed to get expected-lite."
	)
endif()
