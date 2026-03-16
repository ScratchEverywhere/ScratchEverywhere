function(_dep_system_expected-lite)
	find_package(expected-lite CONFIG QUIET)
	add_library(deps::expected-lite ALIAS nonstd::expected-lite)
endfunction()

function(_dep_source_expected-lite)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME expected-lite
		GITHUB_REPOSITORY nonstd-lite/expected-lite
		VERSION 0.10.0
	)
	add_library(deps::expected-lite ALIAS nonstd::expected-lite)
endfunction()
