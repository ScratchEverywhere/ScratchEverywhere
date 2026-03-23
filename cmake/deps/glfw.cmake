function(_dep_system_glfw)
	find_package(glfw3 QUIET)
endfunction()

function(_dep_source_glfw)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME glfw
		GITHUB_REPOSITORY glfw/glfw
		VERSION 3.4
		GIT_TAG 3.4
		OPTIONS "GLFW_BUILD_DOCS OFF" "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_EXAMPLES OFF"
	)
endfunction()
