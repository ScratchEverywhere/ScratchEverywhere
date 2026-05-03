function(_dep_system_dr_libs)
	# TODO: Implement
endfunction()

function(_dep_source_dr_libs)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME dr_libs
		GITHUB_REPOSITORY mackron/dr_libs
		GIT_TAG master
		DOWNLOAD_ONLY TRUE
	)

	add_library(dr_libs INTERFACE)
	target_include_directories(dr_libs INTERFACE ${dr_libs_SOURCE_DIR})
endfunction()
