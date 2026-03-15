if(TARGET stb)
	return()
endif()

if(SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback")
	find_path(STB_IMAGE_INCLUDE_DIR 
		NAMES stb_image.h
		PATHS /usr/include /usr/include/stb
	)

	find_path(STB_TRUETYPE_INCLUDE_DIR 
		NAMES stb_truetype.h
		PATHS /usr/include /usr/include/stb
	)

	if(STB_IMAGE_INCLUDE_DIR AND STB_TRUETYPE_INCLUDE_DIR)
		add_library(stb INTERFACE)
		target_include_directories(stb INTERFACE 
			${STB_IMAGE_INCLUDE_DIR} 
			${STB_TRUETYPE_INCLUDE_DIR}
		)
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET stb) OR SE_DEPS STREQUAL "source")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME stb
		GITHUB_REPOSITORY nothings/stb
		GIT_TAG master
	)
	add_library(stb INTERFACE)
	target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

if(NOT TARGET stb)
	message(
		FATAL_ERROR
		"Failed to get STB."
	)
endif()
