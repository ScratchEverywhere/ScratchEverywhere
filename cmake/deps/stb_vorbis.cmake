function(_dep_system_stb_vorbis)
	find_path(STB_VORBIS_INCLUDE_DIR 
		NAMES stb_vorbis.h stb_vorbis.c
		PATHS /usr/include /usr/include/stb
	)

	if(STB_VORBIS_INCLUDE_DIR)
		add_library(stb_vorbis STATIC ${STB_VORBIS_INCLUDE_DIR}/stb_vorbis.c)
		target_include_directories(stb_vorbis PUBLIC ${STB_VORBIS_INCLUDE_DIR})
	endif()
endfunction()

function(_dep_source_stb_vorbis)
	if(NOT DEFINED stb_SOURCE_DIR)
		include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

		CPMAddPackage(
			NAME stb
			GITHUB_REPOSITORY nothings/stb
			GIT_TAG master
		)
	endif()

	add_library(stb_vorbis STATIC ${stb_SOURCE_DIR}/stb_vorbis.c)
	target_include_directories(stb_vorbis PUBLIC ${stb_SOURCE_DIR})
endfunction()
