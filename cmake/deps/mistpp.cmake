if(TARGET mist++)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback" AND PkgConfig_FOUND)
	pkg_check_modules(mist++ IMPORTED_TARGET mist++>=0.3.4)
	if(mist++_FOUND)
		add_library(mist++ ALIAS PkgConfig::mist++)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT mist++_FOUND) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage("gh:ScratchEverywhere/mistpp@0.3.5")
endif()

if(NOT TARGET mist++)
	message(
		FATAL_ERROR
		"Failed to get Mist++."
	)
endif()
