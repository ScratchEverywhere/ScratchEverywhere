function(_dep_system_tsf)
endfunction()

function(_dep_source_tsf)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME tsf
		GITHUB_REPOSITORY ScratchEverywhere/TinySoundFont
		GIT_TAG main
	)

	add_library(tsf INTERFACE)
	target_include_directories(tsf INTERFACE ${tsf_SOURCE_DIR})
endfunction()
