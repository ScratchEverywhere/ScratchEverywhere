cmake_minimum_required(VERSION 3.16)

project(ScratchEverywhere VERSION 0.28 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SE_SYSTEM "Use system libraries instead of CPM" OFF)
option(SE_AUDIO "Enables audio in SE!" ON)
option(SE_CLOUDVARS "Enable cloud variables." ON)
option(SE_HEADLESS "Makes SE! use a headless renderer instead of SDL2. This will override the SE_AUDIO setting." OFF)
option(SE_LOADSCREEN "Enables SE!'s load screen." ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
	message(STATUS "Downloading CPM.cmake...")
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
	file(DOWNLOAD "https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake"
		 "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake"
		 STATUS status LOG log_content)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
		message(FATAL_ERROR "Failed to download CPM.cmake: ${status}\n${log_content}")
	endif()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Fixes CMakeRC
include(FetchContent)
FetchContent_Declare(
  cmrc
  URL https://github.com/vector-of-bool/cmrc/archive/refs/tags/2.0.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(cmrc)
include(${cmrc_SOURCE_DIR}/CMakeRC.cmake)   

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries as static")
set(CMAKE_FIND_LIBRARY_CUSTOM_LIB_DIRS OFF)

# Copy graphics files to romfs
file(GLOB_RECURSE GFXFILES "${CMAKE_SOURCE_DIR}/gfx/*")
foreach(file IN LISTS GFXFILES)
	file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")

    set(DEST_PATH "${CMAKE_SOURCE_DIR}/romfs/gfx/${REL_PATH}")

    get_filename_component(DEST_DIR "${DEST_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${DEST_DIR}")

    configure_file("${file}" "${DEST_PATH}" COPYONLY)
endforeach()

file(GLOB_RECURSE ROMFS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/romfs/*")
cmrc_add_resource_library(romfs ALIAS romfs::rc WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/romfs" ${ROMFS_FILES})

# They're single header so I just use CPM no matter what
CPMAddPackage(
	NAME nlohmann_json
	GITHUB_REPOSITORY nlohmann/json
	VERSION 3.12.0
)
include(FetchContent)
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.1.0/miniz-3.1.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniz)

if(NOT SE_HEADLESS)
	if(SE_SYSTEM)
		find_package(PkgConfig REQUIRED)

		pkg_check_modules(SDL2 REQUIRED sdl2>=2.0.0)
		pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image>=2.0.0)
		pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf>=2.0.0)
		pkg_check_modules(SDL2_GFX REQUIRED SDL2_gfx>=1.0.0)

		if (SE_AUDIO)
			pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer>=2.0.0)
		endif()
	else()
		if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CROSSCOMPILING)
			message(STATUS "Test!")
			set(SDL2_OPTIONS "SDL_COCOA OFF" "SDL_RENDER_METAL OFF" "SDL_OPENGL ON" "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF") # TODO: Fix compiler-rt issues
		else()
			set(SDL2_OPTIONS "")
		endif()
		CPMAddPackage(
			NAME SDL2
			VERSION 2.32.10
			GITHUB_REPOSITORY libsdl-org/SDL
			GIT_TAG release-2.32.10
			OPTIONS ${SDL2_OPTIONS}
		)
		CPMAddPackage(
			NAME SDL2_image
			VERSION 2.8.8
			GITHUB_REPOSITORY libsdl-org/SDL_image
			GIT_TAG release-2.8.8
		)
		CPMAddPackage(
			NAME SDL2_ttf
			VERSION 2.24.0
			GITHUB_REPOSITORY libsdl-org/SDL_ttf
			GIT_TAG release-2.24.0
			OPTIONS "SDL2TTF_VENDORED ON"
		)
		CPMAddPackage(
			NAME SDL2_gfx
			VERSION 1.0.4
			GITHUB_REPOSITORY giroletm/SDL2_gfx
			GIT_TAG release-1.0.4
			DOWNLOAD_ONLY TRUE # SDL2_gfx doesn't have a CMakeLists.txt
		)
		file(GLOB SDL2_GFX_SOURCES
			"${SDL2_gfx_SOURCE_DIR}/SDL2_rotozoom.c"
			"${SDL2_gfx_SOURCE_DIR}/SDL2_framerate.c"
			"${SDL2_gfx_SOURCE_DIR}/SDL2_imageFilter.c"
			"${SDL2_gfx_SOURCE_DIR}/SDL2_gfxPrimitives.c"
			"${SDL2_gfx_SOURCE_DIR}/SDL2_bgi.c"
		)
		add_library(SDL2_gfx STATIC ${SDL2_GFX_SOURCES})
		target_include_directories(SDL2_gfx PUBLIC
			$<BUILD_INTERFACE:${SDL2_gfx_SOURCE_DIR}>
		)
		target_link_libraries(SDL2_gfx PUBLIC 
			SDL2::SDL2
		)
		if(SE_AUDIO)
			CPMAddPackage(
				NAME SDL2_mixer
				VERSION 2.8.1
				GITHUB_REPOSITORY libsdl-org/SDL_mixer
				GIT_TAG release-2.8.1
				OPTIONS "SDL2MIXER_VORBIS STB" "SDL2MIXER_MP3 MINIMP3" "SDL2MIXER_FLAC DRFLAC" "SDL2MIXER_MOD OFF" "SDL2MIXER_OPUS OFF" "SDL2MIXER_FLUIDSYNTH OFF" "SDL2MIXER_WAVPACK OFF" "SDL2MIXER_MIDI OFF"
			)
		endif()
	endif()
endif()

if(SE_CLOUDVARS)
	# Curl should always be built from source because most platforms don't build it with WebSockets enabled
	set(CURL_OPTIONS
		"BUILD_CURL_EXE OFF"
		"BUILD_TESTING OFF"
		"BUILD_EXAMPLES OFF"
		"ENABLE_MANUAL OFF"
		"CURL_DISABLE_INSTALL ON"
		"ENABLE_ARES OFF"
		"USE_LIBIDN2 OFF"
		"CURL_DISABLE_LDAP ON"
		"CURL_USE_LIBSSH2 OFF"
		"CURL_USE_OPENSSL OFF"
		"CURL_USE_MBEDTLS ON"
	)
	if(WIN32)
		list(APPEND CURL_OPTIONS "CURL_ENABLE_SSL OFF")
	endif()

	CPMAddPackage(
		NAME curl
		GIT_REPOSITORY "https://github.com/curl/curl.git"
		GIT_TAG "curl-8_16_0"
		VERSION 8.16.0
		OPTIONS ${CURL_OPTIONS}
	)
	CPMAddPackage("gh:ScratchEverywhere/mistpp@0.3.1")
endif()

if(SE_HEADLESS)
	set(SOURCES source source/scratch source/scratch/blocks source/scratch/menus source/headless)
else()
	set(SOURCES source source/scratch source/scratch/blocks source/scratch/menus source/sdl source/sdl/audio)
endif()

set(SOURCE_FILES)
foreach(DIR IN LISTS SOURCES)
	file(GLOB DIR_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/${DIR}/*.cc
	)
	list(APPEND SOURCE_FILES ${DIR_SOURCES})
endforeach()
list(APPEND SOURCE_FILES "${miniz_SOURCE_DIR}/miniz.c")

add_executable(scratch-everywhere ${SOURCE_FILES})

if(CMAKE_BUILD_TYPE MATCHES "Debug")
	target_compile_options(scratch-everywhere PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_link_libraries(scratch-everywhere PRIVATE -fsanitize=address)
endif()

if(NOT BUILD_SHARED_LIBS AND WIN32)
	target_link_options(scratch-everywhere PRIVATE "-static-libgcc" "-static-libstdc++")
endif()

if (NOT SE_HEADLESS)
	set(SDL_LINK_LIBS)

	if(SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS ${SDL2_GFX_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL2_GFX_INCLUDE_DIRS})
	else()
		list(APPEND SDL_LINK_LIBS SDL2_gfx SDL2::SDL2)
	endif()

	if(SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${SDL2_TTF_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

		if (SE_AUDIO)
			list(APPEND SDL_LINK_LIBS ${SDL2_MIXER_LIBRARIES})
			target_include_directories(scratch-everywhere PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
		endif()
	elseif(BUILD_SHARED_LIBS)
		list(APPEND SDL_LINK_LIBS SDL2_image::SDL2_image)
		list(APPEND SDL_LINK_LIBS SDL2_ttf::SDL2_ttf)

		if(SE_AUDIO)	
			list(APPEND SDL_LINK_LIBS SDL2_mixer::SDL2_mixer)
		endif()
	else()
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image-static>,
			SDL2_image::SDL2_image-static,
			SDL2_image::SDL2_image>)
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf-static>,
			SDL2_ttf::SDL2_ttf-static,
			SDL2_ttf::SDL2_ttf>)
		if(SE_AUDIO)
			list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer-static>,
				SDL2_mixer::SDL2_mixer-static,
				SDL2_mixer::SDL2_mixer>)
		endif()
	endif()

	if (WIN32)
		list(APPEND SDL_LINK_LIBS SDL2::SDL2main)
	endif()

	target_link_libraries(scratch-everywhere PRIVATE ${SDL_LINK_LIBS})
endif()

target_compile_definitions(scratch-everywhere PRIVATE __PC__)

if(NOT SE_HEADLESS)
	target_compile_definitions(scratch-everywhere PRIVATE SDL_BUILD)
endif()

if(SE_AUDIO)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_AUDIO)
endif()
if(SE_LOADSCREEN)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_LOADSCREEN)
endif()
if(SE_CLOUDVARS)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_CLOUDVARS)
	target_link_libraries(scratch-everywhere PRIVATE mist++)
endif()

target_link_libraries(scratch-everywhere PRIVATE nlohmann_json::nlohmann_json romfs::rc)
target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})

if(PSP)
    create_pbp_file(
        TARGET scratch-everywhere
        ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gfx/psp/ICON0.png
        BACKGROUND_PATH NULL
        PREVIEW_PATH ${CMAKE_CURRENT_SOURCE_DIR}/gfx/psp/PIC1.png
        TITLE scratch-everywhere
        VERSION ${PROJECT_VERSION}
    )
	# package the EBOOT.pbp
	add_custom_command(TARGET scratch-everywhere POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/scratch-psp"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:scratch-everywhere>" "${CMAKE_BINARY_DIR}/scratch-psp/EBOOT.PBP"
    COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/scratch-psp.zip" --format=zip -- "${CMAKE_BINARY_DIR}/scratch-psp"
)
endif()
