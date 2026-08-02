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
		DOWNLOAD_ONLY TRUE
	)

	add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
	target_include_directories(miniz PUBLIC ${miniz_SOURCE_DIR})
endfunction()
