function(_dep_system_miniz)
	find_package(miniz CONFIG QUIET)
endfunction()

function(_dep_source_miniz)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME miniz
		GITHUB_REPOSITORY richgel999/miniz
		VERSION 3.1.1
		GIT_TAG 3.1.1
		OPTIONS "BUILD_TESTS OFF" "BUILD_EXAMPLES OFF"
	)
endfunction()
