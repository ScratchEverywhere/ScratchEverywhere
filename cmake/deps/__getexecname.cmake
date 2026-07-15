set(SE_FORCE_SOURCE___getexecname TRUE)
function(_dep_source___getexecname)
	if(NOT DEFINED __getexecname_SOURCE_DIR)
		include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
		CPMAddPackage(
			NAME __getexecname
			GITHUB_REPOSITORY samuelvenable/__getexecname
			GIT_TAG main
		)
	endif()
	add_library(__getexecname STATIC "${__getexecname_SOURCE_DIR}/__getbasepath/internal.cpp")
	target_include_directories(__getexecname PUBLIC "${__getexecname_SOURCE_DIR}")
endfunction()
