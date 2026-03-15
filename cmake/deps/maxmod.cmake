if(TARGET maxmod)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	include(CheckIncludeFile)
	check_include_file(maxmod9.h HAVE_MAXMOD9_H)

	if(HAVE_MAXMOD9_H)
		add_library(maxmod INTERFACE)
		target_link_libraries(maxmod INTERFACE mm9)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET maxmod) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME maxmod
		GIT_REPOSITORY "https://codeberg.org/blocksds/maxmod"
		VERSION 1.18.1
		GIT_TAG v1.18.1-blocks
		DOWNLOAD_ONLY TRUE
	)
	file(GLOB MAXMOD_SOURCES "${maxmod_SOURCE_DIR}/source/ds/common" "${maxmod_SOURCE_DIR}/source/ds/arm9")
	add_library(maxmod STATIC ${MAXMOD_SOURCES})
	target_include_directories(maxmode PUBLIC "${maxmode_SOURCE_DIR}/include")
endif()

if(NOT TARGET maxmod)
	message(
		FATAL_ERROR
		"Failed to get maxmod."
	)
endif()
