# IDK if glad has system support?
function(_dep_system_glad)
endfunction()

function(_dep_source_glad)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME glad
		GITHUB_REPOSITORY Dav1dde/glad
		VERSION 2.0.8
		DOWNLOAD_ONLY TRUE
	)
	add_subdirectory(${glad_SOURCE_DIR}/cmake ${glad_BINARY_DIR})

	glad_add_library(glad
		REPRODUCIBLE
		LOADER
		LANGUAGE C
		API gl:core=4.1
	)
endfunction()
