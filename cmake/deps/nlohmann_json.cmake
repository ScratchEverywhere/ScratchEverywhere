function(_dep_system_nlohmann_json)
	find_package(nlohmann_json QUIET)
endfunction()

function(_dep_source_nlohmann_json)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME nlohmann_json
		GITHUB_REPOSITORY nlohmann/json
		VERSION 3.12.0
	)
endfunction()
