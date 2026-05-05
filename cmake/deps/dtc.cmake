function(_dep_system_dtc)
endfunction()

function(_dep_source_dtc)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME dtc
		GITHUB_REPOSITORY dectalk/dectalkmini
		GIT_TAG dectalk-develop
		OPTIONS DECTALKMINI_NO_FILESYSTEM DECTALKMINI_NO_CHARSET
	)
endfunction()
