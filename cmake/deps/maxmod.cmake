function(_dep_system_maxmod)
	include(CheckIncludeFile)
	check_include_file(maxmod9.h HAVE_MAXMOD9_H)

	if(HAVE_MAXMOD9_H)
		add_library(maxmod INTERFACE)
		target_link_libraries(maxmod INTERFACE mm9)
	endif()
endfunction()

function(_dep_source_maxmod)
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
	target_include_directories(maxmod PUBLIC "${maxmode_SOURCE_DIR}/include")
endfunction()
