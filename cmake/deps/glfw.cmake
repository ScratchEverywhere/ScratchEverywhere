if(TARGET glfw)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_package(glfw3 OPTIONAL)
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET glfw) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME glfw
		GITHUB_REPOSITORY glfw/glfw
		VERSION 3.4
		GIT_TAG 3.4
		OPTIONS "GLFW_BUILD_DOCS OFF" "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_EXAMPLES OFF"
	)
endif()

if(NOT TARGET glfw)
	message(
		FATAL_ERROR
		"Failed to get GLFW."
	)
endif()
