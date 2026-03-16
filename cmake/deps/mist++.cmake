function(_dep_system_mist++)
	pkg_check_modules(mist++ IMPORTED_TARGET mist++>=0.3.4)
endfunction()

function(_dep_source_mist++)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
	include("${CMAKE_CURRENT_LIST_DIR}/add_dependency.cmake")

	# Magic to get Mist++ to use our really weird custom linking stuff
	add_library(_libcurl INTERFACE)
	se_add_dependency(_libcurl libcurl)
	add_library(CURL::libcurl ALIAS _libcurl)

	CPMAddPackage("gh:ScratchEverywhere/mistpp@0.3.5")
endfunction()
