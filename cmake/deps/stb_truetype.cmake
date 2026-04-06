function(_dep_system_stb_truetype)
	find_path(STB_TRUETYPE_INCLUDE_DIR 
		NAMES stb_truetype.h
		PATHS /usr/include /usr/include/stb
	)

	if(STB_TRUETYPE_INCLUDE_DIR)
		add_library(stb_truetype INTERFACE)
		target_include_directories(stb_truetype INTERFACE ${STB_TRUETYPE_INCLUDE_DIR})
	endif()
endfunction()

function(_dep_source_stb_truetype)
	if(NOT DEFINED stb_SOURCE_DIR)
		include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

		CPMAddPackage(
			NAME stb
			GITHUB_REPOSITORY nothings/stb
			GIT_TAG master
		)
	endif()

	add_library(stb_truetype INTERFACE)
	target_include_directories(stb_truetype INTERFACE ${stb_SOURCE_DIR})
endfunction()
