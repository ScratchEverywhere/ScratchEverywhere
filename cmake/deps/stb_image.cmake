function(_dep_system_stb_image)
	find_path(STB_IMAGE_INCLUDE_DIR 
		NAMES stb_image.h
		PATHS /usr/include /usr/include/stb
	)

	if(STB_IMAGE_INCLUDE_DIR)
		add_library(stb_image INTERFACE)
		target_include_directories(stb_image INTERFACE ${STB_IMAGE_INCLUDE_DIR})
	endif()
endfunction()

function(_dep_source_stb_image)
	if(NOT DEFINED stb_SOURCE_DIR)
		include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

		CPMAddPackage(
			NAME stb
			GITHUB_REPOSITORY nothings/stb
			GIT_TAG master
		)
	endif()

	add_library(stb_image INTERFACE)
	target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})
endfunction()
