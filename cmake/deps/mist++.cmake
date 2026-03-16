function(_dep_system_mist++)
	pkg_check_modules(mist++ IMPORTED_TARGET mist++>=0.3.4)
endfunction()

function(_dep_source_mist++)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

	se_add_dependency(libcurl)
	get_target_property(IS_ALIAS deps::libcurl ALIASED_TARGET)
	if(IS_ALIAS)
		add_library(CURL::libcurl ALIAS ${IS_ALIAS})
	else()
		add_library(CURL::libcurl ALIAS deps::libcurl)
	endif()

	CPMAddPackage("gh:ScratchEverywhere/mistpp@0.3.5")
endfunction()
