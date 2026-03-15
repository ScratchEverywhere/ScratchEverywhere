if(TARGET nlohmann_json::nlohmann_json)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(nlohmann_json QUIET)
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET nlohmann_json::nlohmann_json) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME nlohmann_json
		GITHUB_REPOSITORY nlohmann/json
		VERSION 3.12.0
	)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
	message(
		FATAL_ERROR
		"Failed to get nlohmann_json."
	)
endif()
