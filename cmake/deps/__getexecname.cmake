function(_dep_system___getexecname)
	find_path(__GETEXECNAME_INCLUDE_DIR 
		NAMES __getexecname/internal.h __getexecname/internal.cpp
		PATHS /usr/include /usr/include/__getexecname
	)

	if(__GETEXECNAME_INCLUDE_DIR)
		add_library(__getexecname STATIC ${__GETEXECNAME_INCLUDE_DIR}/__getexecname/internal.cpp)
		target_include_directories(__getexecname PUBLIC ${__GETEXECNAME_INCLUDE_DIR})
	endif()
endfunction()

function(_dep_source___getexecname)
	if(NOT DEFINED __getexecname_SOURCE_DIR)
		include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

		CPMAddPackage(
			NAME __getexecname
			GITHUB_REPOSITORY samuelvenable/__getexecname
			GIT_TAG main
		)
	endif()

	add_library(__getexecname STATIC ${__getexecname_SOURCE_DIR}/__getexecname/internal.cpp)
	target_include_directories(__getexecname PUBLIC ${__getexecname_SOURCE_DIR})
endfunction()
