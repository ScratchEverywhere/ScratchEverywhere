cmake_minimum_required(VERSION 3.16)

project(ScratchEverywhere VERSION 0.34 LANGUAGES CXX C)

# error C7555: use of designated initializers requires at least '/std:c++20'
if(MSVC)
	set(CMAKE_CXX_STANDARD 20)
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SE_AUDIO "Enables audio in SE!" ON)

# [SWITCH] Cloud variables might actually be possible with a newer version of libcurl but I don't really feel like dealing with that rn
# [VITA]   It should work with our custom curl package but it doesn't so I'm just going to disable it here.
if(NOT NINTENDO_SWITCH AND NOT PSP AND NOT VITA AND NOT WEBOS)
	option(SE_CLOUDVARS "Enable cloud variables." ON)
endif()

# This tbh should probably work on all of these platforms (except MAYBE PSP..) but i lowkey dont want to test it rn
if(NOT NINTENDO_SWITCH AND NOT PSP AND NOT VITA)
	option(SE_DOWNLOAD "Enable the ability to use the internet to download files, such as Text-To-Speech blocks." ON)
endif()

option(SE_MENU "Enables the SE! Main Menu." ON)

if(SE_MENU)
	option(SE_LOADSCREEN "Enables SE!'s load screen." ON)
endif()

list(APPEND SE_RENDERER_VALID_OPTIONS "headless" "sdl2")
if(NOT NINTENDO_SWITCH AND NOT PSP AND NOT VITA AND NOT WEBOS)
	list(APPEND SE_RENDERER_VALID_OPTIONS "sdl1" "sdl3" "opengl")
endif()

if("sdl3" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"sdl3"
		CACHE STRING "Select the desired renderer backend."
	)
elseif("sdl2" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"sdl2"
		CACHE STRING "Select the desired renderer backend."
	)
elseif("sdl1" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"sdl1"
		CACHE STRING "Select the desired renderer backend."
	)
elseif("opengl" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"opengl"
		CACHE STRING "Select the desired renderer backend."
	)
else()
	set(
		SE_RENDERER
		"headless"
		CACHE STRING "Select the desired renderer backend."
	)
endif()
if(NOT SE_RENDERER IN_LIST SE_RENDERER_VALID_OPTIONS)
    message(
        FATAL_ERROR
        "Invalid value for SE_RENDERER: '${SE_RENDERER}'. Must be one of: ${SE_RENDERER_VALID_OPTIONS}"
    )
endif()

list(APPEND SE_WINDOWING_VALID_OPTIONS "headless")
if(SE_RENDERER STREQUAL "sdl2" OR SE_RENDERER STREQUAL "opengl")
	list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl2")
endif()
if(NOT NINTENDO_SWITCH AND NOT PSP AND NOT VITA AND NOT WEBOS)
	if(SE_RENDERER STREQUAL "opengl")
		list(APPEND SE_WINDOWING_VALID_OPTIONS "glfw")
	endif()
	if(SE_RENDERER STREQUAL "sdl3" OR SE_RENDERER STREQUAL "opengl")
		list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl3")
	endif()
	if(SE_RENDERER STREQUAL "sdl1" OR SE_RENDERER STREQUAL "opengl")
		list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl1")
	endif()
endif()

if("glfw" IN_LIST SE_WINDOWING_VALID_OPTIONS)
	set(
		SE_WINDOWING
		"glfw"
		CACHE STRING "Select the desired windowing backend."
	)
elseif("sdl3" IN_LIST SE_WINDOWING_VALID_OPTIONS)
	set(
		SE_WINDOWING
		"sdl3"
		CACHE STRING "Select the desired windowing backend."
	)
elseif("sdl2" IN_LIST SE_WINDOWING_VALID_OPTIONS)
	set(
		SE_WINDOWING
		"sdl2"
		CACHE STRING "Select the desired windowing backend."
	)
elseif("sdl1" IN_LIST SE_WINDOWING_VALID_OPTIONS)
	set(
		SE_WINDOWING
		"sdl1"
		CACHE STRING "Select the desired windowing backend."
	)
else()
	set(
		SE_WINDOWING
		"headless"
		CACHE STRING "Select the desired windowing backend."
	)
endif()
if(NOT SE_WINDOWING IN_LIST SE_WINDOWING_VALID_OPTIONS)
    message(
        FATAL_ERROR
        "Invalid value for SE_WINDOWING: '${SE_WINDOWING}'. Must be one of: ${SE_WINDOWING_VALID_OPTIONS}"
    )
endif()

list(APPEND SE_AUDIO_ENGINE_VALID_OPTIONS "headless" "sdl2")
if(NOT NINTENDO_SWITCH AND NOT PSP AND NOT VITA AND NOT WEBOS)
	list(APPEND SE_AUDIO_ENGINE_VALID_OPTIONS "sdl1" "sdl3")
endif()

if(SE_RENDERER STREQUAL "sdl3" OR SE_WINDOWING STREQUAL "sdl3")
	set(SE_AUDIO_ENGINE_DEFAULT "sdl3")
elseif(SE_RENDERER STREQUAL "sdl2" OR SE_WINDOWING STREQUAL "sdl2" OR SE_WINDOWING STREQUAL "glfw")
	set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
elseif(SE_RENDERER STREQUAL "sdl1" OR SE_WINDOWING STREQUAL "sdl1")
	set(SE_AUDIO_ENGINE_DEFAULT "sdl1")
else()
	set(SE_AUDIO_ENGINE_DEFAULT "headless")
endif()

if(SE_AUDIO_ENGINE_DEFAULT IN_LIST SE_AUDIO_ENGINE_VALID_OPTIONS)
    set(
        SE_AUDIO_ENGINE
        ${SE_AUDIO_ENGINE_DEFAULT}
        CACHE STRING "Select the desired audio backend."
    )
else()
    set(
        SE_AUDIO_ENGINE
        "headless"
        CACHE STRING "Select the desired audio backend."
    )
endif()

if(NOT SE_AUDIO_ENGINE IN_LIST SE_AUDIO_ENGINE_VALID_OPTIONS)
    message(
        FATAL_ERROR
        "Invalid value for SE_AUDIO_ENGINE: '${SE_AUDIO_ENGINE}'. Must be one of: ${SE_AUDIO_ENGINE_VALID_OPTIONS}"
    )
endif()

if(SE_RENDERER STREQUAL "sdl1" OR NINTENDO_SWITCH OR PSP OR VITA OR WEBOS)
	option(SE_SYSTEM "Use system libraries instead of CPM" ON)
else()
	option(SE_SYSTEM "Use system libraries instead of CPM" OFF)
endif()

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

if(NOT NINTENDO_SWITCH AND NOT VITA AND NOT WEBOS)
	option(SE_CMAKERC "Embed assets to executable with CMakeRC instead of loading from the working directory" ON)
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Fixes CMakeRC

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries as static")
set(CMAKE_FIND_LIBRARY_CUSTOM_LIB_DIRS OFF)

# Copy graphics files to romfs
if(SE_MENU)
	file(GLOB_RECURSE GFXFILES
	    "${CMAKE_SOURCE_DIR}/gfx/menu/*"
	    "${CMAKE_SOURCE_DIR}/gfx/ingame/*"
	)
else()
	file(GLOB_RECURSE GFXFILES
	    "${CMAKE_SOURCE_DIR}/gfx/menu/LibSansN.ttf"
	    "${CMAKE_SOURCE_DIR}/gfx/ingame/*"
	)
endif()

foreach(file IN LISTS GFXFILES)
	file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")

    set(DEST_PATH "${CMAKE_SOURCE_DIR}/romfs/gfx/${REL_PATH}")

    get_filename_component(DEST_DIR "${DEST_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${DEST_DIR}")

    configure_file("${file}" "${DEST_PATH}" COPYONLY)
endforeach()

if(SE_SYSTEM)
	find_package(PkgConfig REQUIRED)
endif()

# They're single header so I just use CPM no matter what
CPMAddPackage(
	NAME nlohmann_json
	GITHUB_REPOSITORY nlohmann/json
	VERSION 3.12.0
)
CPMAddPackage(
	NAME poipole807_ryuJS
	GITHUB_REPOSITORY poipole807/ryuJS
	VERSION 3.0
)
CPMAddPackage(
  NAME nanosvg
  GITHUB_REPOSITORY memononen/nanosvg
  GIT_TAG master
)
CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master
)


include(FetchContent)
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.1.0/miniz-3.1.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniz)

if(SE_RENDERER STREQUAL "sdl1")
	if(SE_SYSTEM)
		pkg_check_modules(SDL REQUIRED sdl>=1.2.0)
		pkg_check_modules(SDL_IMAGE REQUIRED SDL_image>=1.2.0)
		pkg_check_modules(SDL_TTF REQUIRED SDL_ttf>=1.2.0)
		pkg_check_modules(SDL_GFX REQUIRED SDL_gfx>=1.0.0)
	else()
		error("SDL1 is currently not supported without system libraries")
	endif()
	CPMAddPackage(
		NAME nanosvg
		GITHUB_REPOSITORY memononen/nanosvg
		GIT_TAG 5cefd9847949af6df13f65027fd43af5a7513633
	)
elseif(SE_RENDERER STREQUAL "sdl2")
	if(SE_SYSTEM)
		pkg_check_modules(SDL2 REQUIRED sdl2>=2.0.0)
		pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf>=2.0.0)

		if(NOT WEBOS)
			pkg_check_modules(SDL2_GFX REQUIRED SDL2_gfx>=1.0.0)
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
			NAME SDL2_ttf
			VERSION 2.24.0
			GITHUB_REPOSITORY libsdl-org/SDL_ttf
			GIT_TAG release-2.24.0
			OPTIONS "SDL2TTF_VENDORED ON"
		)
	endif()

	if(NOT SE_SYSTEM OR WEBOS)
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
		if(WEBOS)
			target_link_libraries(SDL2_gfx PUBLIC ${SDL2_LIBRARIES})
			target_include_directories(SDL2_gfx PRIVATE ${SDL2_INCLUDE_DIRS})
		else()
			target_link_libraries(SDL2_gfx PUBLIC
				SDL2::SDL2
			)
		endif()
	endif()
elseif(SE_RENDERER STREQUAL "sdl3")
	if(SE_SYSTEM)
		pkg_check_modules(SDL3 REQUIRED sdl3>=3.0.0)
		pkg_check_modules(SDL3_IMAGE REQUIRED sdl3-image>=3.0.0)
		pkg_check_modules(SDL3_TTF REQUIRED sdl3-ttf>=3.0.0)
		pkg_check_modules(SDL3_GFX REQUIRED sdl3-gfx>=1.0.0)
	else()
		CPMAddPackage(
			NAME SDL3
			VERSION 3.2.26
			GITHUB_REPOSITORY libsdl-org/SDL
			GIT_TAG main # Fixes SDL3_Mixer, we're wating for the 3.4.0 release
			OPTIONS "SDL_TEST_LIBRARY OFF"
		)
		CPMAddPackage(
			NAME SDL3_image
			VERSION 3.2.4
			GITHUB_REPOSITORY libsdl-org/SDL_image
			GIT_TAG release-3.2.4
		)
		CPMAddPackage(
			NAME SDL3_ttf
			VERSION 3.2.2
			GITHUB_REPOSITORY libsdl-org/SDL_ttf
			GIT_TAG release-3.2.2
			OPTIONS "SDLTTF_VENDORED ON"
		)
		CPMAddPackage(
			NAME SDL3_gfx
			VERSION 1.0.1
			GITHUB_REPOSITORY sabdul-khabir/SDL3_gfx
		)
	endif()
elseif(SE_RENDERER STREQUAL "opengl")
	option(SE_OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)

	find_package(OpenGL REQUIRED)
	if(SE_OPENGL_ES)
		find_package(OpenGL REQUIRED COMPONENTS GLES2)
	endif()
endif()

if(SE_WINDOWING STREQUAL "glfw")
	if(SE_SYSTEM)
		find_package(glfw3 REQUIRED)
	else()
		CPMAddPackage(
			NAME glfw
			GITHUB_REPOSITORY glfw/glfw
			GIT_TAG 3.4
			OPTIONS "GLFW_BUILD_DOCS OFF" "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_EXAMPLES OFF"
		)
	endif()
elseif(SE_WINDOWING STREQUAL "sdl1")
	if(SE_SYSTEM)
		pkg_check_modules(SDL REQUIRED sdl>=1.2.0)
	else()
		error("SDL1 is currently not supported without system libraries")
	endif()
elseif(SE_WINDOWING STREQUAL "sdl2")
	if(SE_SYSTEM)
		pkg_check_modules(SDL2 REQUIRED sdl2>=2.0.0)
	else()
		if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2)
			if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CROSSCOMPILING)
				set(SDL2_OPTIONS "SDL_COCOA OFF" "SDL_RENDER_METAL OFF" "SDL_OPENGL ON" "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF")
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
		endif()
	endif()
elseif(SE_WINDOWING STREQUAL "sdl3")
	if(SE_SYSTEM)
		pkg_check_modules(SDL3 REQUIRED sdl3>=3.0.0)
	else()
		if(NOT TARGET SDL3::SDL3)
			CPMAddPackage(
				NAME SDL3
				VERSION 3.2.26
				GITHUB_REPOSITORY libsdl-org/SDL
				GIT_TAG main
				OPTIONS "SDL_TEST_LIBRARY OFF"
			)
		endif()
	endif()
endif()

if(SE_AUDIO)
	if(SE_AUDIO_ENGINE STREQUAL "sdl1")
		if(SE_SYSTEM)
			pkg_check_modules(SDL_MIXER REQUIRED SDL_mixer>=1.2.0)
		else()
			message(FATAL_ERROR "SDL1 Audio is currently not supported without system libraries")
		endif()
	elseif(SE_AUDIO_ENGINE STREQUAL "sdl2")
		if(NOT SE_SYSTEM OR WEBOS)
			if(NOT TARGET SDL2::SDL2 AND NOT TARGET SDL2)
				if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CROSSCOMPILING)
					set(SDL2_OPTIONS "SDL_COCOA OFF" "SDL_RENDER_METAL OFF" "SDL_OPENGL ON" "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF")
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
			endif()

			CPMAddPackage(
				NAME SDL2_mixer
				VERSION 2.8.1
				GITHUB_REPOSITORY libsdl-org/SDL_mixer
				GIT_TAG release-2.8.1
				OPTIONS "SDL2MIXER_VORBIS STB" "SDL2MIXER_MP3 MINIMP3" "SDL2MIXER_FLAC DRFLAC" "SDL2MIXER_MOD OFF" "SDL2MIXER_OPUS OFF" "SDL2MIXER_FLUIDSYNTH OFF" "SDL2MIXER_WAVPACK OFF" "SDL2MIXER_MIDI OFF"
			)

			if(WEBOS)
				target_link_libraries(SDL2_mixer PUBLIC ${SDL2_LIBRARIES})
				target_include_directories(SDL2_mixer PRIVATE ${SDL2_INCLUDE_DIRS})
			endif()
		else()
			pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer>=2.0.0)
		endif()
	elseif(SE_AUDIO_ENGINE STREQUAL "sdl3")
		if(SE_SYSTEM)
			pkg_check_modules(SDL3_MIXER REQUIRED sdl3-mixer>=2.0.0)
		else()
			if(NOT TARGET SDL3::SDL3)
				CPMAddPackage(
					NAME SDL3
					VERSION 3.2.26
					GITHUB_REPOSITORY libsdl-org/SDL
					GIT_TAG main # Fixes SDL3_Mixer, we're wating for the 3.4.0 release
					OPTIONS "SDL_TEST_LIBRARY OFF"
				)
			endif()
			CPMAddPackage(
				NAME SDL3_mixer
				VERSION 3
				GITHUB_REPOSITORY libsdl-org/SDL_mixer
				GIT_TAG main # TODO: pin version
			)
		endif()
	endif()
endif()

if(SE_CLOUDVARS OR SE_DOWNLOAD)
	# Curl should always be built from source because most platforms don't build it with WebSockets enabled
	# (except on Haiku we can't build from source because some sread/swrite macro thing though Haiku's curl already supports WebSockets anyway soooooooo)
	if(NOT HAIKU)
		set(CURL_OPTIONS
			"BUILD_CURL_EXE OFF"
			"BUILD_TESTING OFF"
			"BUILD_EXAMPLES OFF"
			"ENABLE_MANUAL OFF"
			"CURL_DISABLE_INSTALL ON"
			"ENABLE_ARES OFF"
			"USE_LIBIDN2 OFF"
			"ENABLE_WEBSOCKETS ON"
			"CURL_DISABLE_WEBSOCKETS OFF"
			"CURL_DISABLE_LDAP ON"
			"CURL_USE_LIBSSH2 OFF"
		)
		if(WEBOS)
			list(APPEND CURL_OPTIONS "CURL_USE_OPENSSL OFF")
			list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS OFF")
			list(APPEND CURL_OPTIONS "CURL_USE_LIBPSL OFF")
		elseif(APPLE)
			list(APPEND CURL_OPTIONS "CURL_USE_OPENSSL OFF")
			list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS ON")
			list(APPEND CURL_OPTIONS "CURL_USE_LIBPSL OFF")
		else()
			list(APPEND CURL_OPTIONS "CURL_USE_OPENSSL OFF")
			list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS ON")
			list(APPEND CURL_OPTIONS "CURL_USE_LIBPSL ON")
		endif()
		if(WIN32 OR WEBOS)
			list(APPEND CURL_OPTIONS "CURL_ENABLE_SSL OFF")
		endif()

		CPMAddPackage(
			NAME curl
			GIT_REPOSITORY "https://github.com/curl/curl.git"
			GIT_TAG "curl-8_17_0"
			VERSION 8.17.0
			OPTIONS ${CURL_OPTIONS}
		)
	else()
		find_package(CURL REQUIRED)
	endif()
	if(SE_CLOUDVARS)
		CPMAddPackage("gh:ScratchEverywhere/mistpp@0.3.4")
	endif()
endif()

set(SOURCES "source" "source/runtime" "source/runtime/blocks" "source/renderers/${SE_RENDERER}" "source/windowing/${SE_WINDOWING}" "source/audio/${SE_AUDIO_ENGINE}")

if(SE_MENU)
	list(APPEND SOURCES source/menus)
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
if(WIN32)
	list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/gfx/windows/resource.rc")
endif()

add_executable(scratch-everywhere WIN32 ${SOURCE_FILES})
if(PSP)
	set_target_properties(scratch-everywhere PROPERTIES OUTPUT_NAME scratch-psp)
elseif(NINTENDO_SWITCH)
	set_target_properties(scratch-everywhere PROPERTIES OUTPUT_NAME scratch-nx)
elseif(VITA)
	set_target_properties(scratch-everywhere PROPERTIES OUTPUT_NAME scratch-vita)
	target_compile_definitions(scratch-everywhere PRIVATE VITA)
elseif(WEBOS)
	set_target_properties(scratch-everywhere PROPERTIES OUTPUT_NAME scratch-webos)
	target_compile_definitions(scratch-everywhere PRIVATE WEBOS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_CROSSCOMPILING AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
	# set_target_properties(scratch-everywhere PROPERTIES
	# 	MACOSX_BUNDLE True
	# 	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/gfx/macOS/Info.plist
	# 	OUTPUT_NAME "scratch-macOS"
	# )
	# add_custom_command(TARGET scratch-everywhere POST_BUILD
	# 	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/gfx/macOS/Icon.icns "$<TARGET_BUNDLE_DIR:scratch-everywhere>/Contents/Resources/Icon.icns")
endif()

if(SE_CMAKERC)
	include(FetchContent)
	FetchContent_Declare(
	  cmrc
	  URL https://github.com/vector-of-bool/cmrc/archive/refs/tags/2.0.1.tar.gz
	  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	)
	FetchContent_MakeAvailable(cmrc)
	include(${cmrc_SOURCE_DIR}/CMakeRC.cmake)

	file(GLOB_RECURSE ROMFS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/romfs/*")
	cmrc_add_resource_library(romfs ALIAS romfs::rc WHENCE "${CMAKE_CURRENT_SOURCE_DIR}/romfs" ${ROMFS_FILES})
	target_link_libraries(scratch-everywhere PRIVATE romfs::rc)
	target_compile_definitions(scratch-everywhere PRIVATE USE_CMAKERC)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT MSVC AND NOT APPLE)
	target_compile_options(scratch-everywhere PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_link_libraries(scratch-everywhere PRIVATE -fsanitize=address)
endif()

if(NOT BUILD_SHARED_LIBS AND MINGW)
	target_link_options(scratch-everywhere PRIVATE "-static-libgcc" "-static-libstdc++")
endif()

if(SE_RENDERER STREQUAL "sdl3")
	set(SDL_LINK_LIBS)

	if(NOT SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS SDL3::SDL3)
		target_include_directories(scratch-everywhere PRIVATE ${SDL3_gfx_SOURCE_DIR})
	endif()

	if(SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS ${SDL3_LIBRARIES} ${SDL3_IMAGE_LIBRARIES} ${SDL3_TTF_LIBRARIES} ${SDL3_GFX_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL3_INCLUDE_DIRS} ${SDL3_IMAGE_INCLUDE_DIRS} ${SDL3_TTF_INCLUDE_DIRS} ${SDL3_GFX_INCLUDE_DIRS})
	elseif(BUILD_SHARED_LIBS)
		list(APPEND SDL_LINK_LIBS SDL3_image::SDL3_image)
		list(APPEND SDL_LINK_LIBS SDL3_ttf::SDL3_ttf)
		list(APPEND SDL_LINK_LIBS SDL3_gfx_Shared)
	else()
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL3_image::SDL3_image-static>,
			SDL3_image::SDL3_image-static,
			SDL3_image::SDL3_image>)
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL3_ttf::SDL3_ttf-static>,
			SDL3_ttf::SDL3_ttf-static,
			SDL3_ttf::SDL3_ttf>)
		list(APPEND SDL_LINK_LIBS SDL3_gfx_Static)
	endif()

	target_link_libraries(scratch-everywhere PRIVATE ${SDL_LINK_LIBS})
elseif(SE_RENDERER STREQUAL "sdl2")
	set(SDL_LINK_LIBS)

	if(SE_SYSTEM AND NOT WEBOS)
		list(APPEND SDL_LINK_LIBS ${SDL2_GFX_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL2_GFX_INCLUDE_DIRS})
	else()
		list(APPEND SDL_LINK_LIBS SDL2_gfx)
	endif()

	if (NOT SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS SDL2::SDL2)
	endif()

	if(SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS ${SDL2_LIBRARIES} ${SDL2_TTF_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})

	elseif(BUILD_SHARED_LIBS)
		list(APPEND SDL_LINK_LIBS SDL2_ttf::SDL2_ttf)

	else()
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf-static>,
			SDL2_ttf::SDL2_ttf-static,
			SDL2_ttf::SDL2_ttf>)
	endif()

	if (WIN32)
		list(APPEND SDL_LINK_LIBS SDL2::SDL2main)
	endif()

	target_link_libraries(scratch-everywhere PRIVATE ${SDL_LINK_LIBS})
elseif(SE_RENDERER STREQUAL "sdl1")
	set(SDL_LINK_LIBS)

	if(SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS ${SDL_GFX_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL_GFX_INCLUDE_DIRS})
	else()
		list(APPEND SDL_LINK_LIBS SDL_gfx SDL::SDL)
	endif()

	if(SE_SYSTEM)
		list(APPEND SDL_LINK_LIBS ${SDL_LIBRARIES} ${SDL_IMAGE_LIBRARIES} ${SDL_TTF_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL_INCLUDE_DIRS} ${SDL_IMAGE_INCLUDE_DIRS} ${SDL_TTF_INCLUDE_DIRS})

	elseif(BUILD_SHARED_LIBS)
		list(APPEND SDL_LINK_LIBS SDL_image::SDL_image)
		list(APPEND SDL_LINK_LIBS SDL_ttf::SDL_ttf)

	else()
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL_image::SDL_image-static>,
			SDL_image::SDL_image-static,
			SDL_image::SDL_image>)
		list(APPEND SDL_LINK_LIBS $<IF:$<TARGET_EXISTS:SDL_ttf::SDL_ttf-static>,
			SDL_ttf::SDL_ttf-static,
			SDL_ttf::SDL_ttf>)
	endif()

	target_link_libraries(scratch-everywhere PRIVATE ${SDL_LINK_LIBS})
	target_include_directories(scratch-everywhere PRIVATE ${nanosvg_SOURCE_DIR}/src)
elseif(SE_RENDERER STREQUAL "opengl")
	target_link_libraries(scratch-everywhere PRIVATE OpenGL::GL)
	if(SE_OPENGL_ES)
		target_link_libraries(scratch-everywhere PRIVATE OpenGL::GLES2)
		target_compile_definitions(scratch-everywhere PRIVATE USE_GLES)
	endif()
	target_include_directories(scratch-everywhere PRIVATE include)
endif()

if(SE_WINDOWING STREQUAL "sdl1")
	if(SE_SYSTEM)
		target_link_libraries(scratch-everywhere PRIVATE ${SDL_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL_INCLUDE_DIRS})
	else()
		target_link_libraries(scratch-everywhere PRIVATE SDL::SDL)
	endif()
	target_compile_definitions(scratch-everywhere PRIVATE WINDOWING_SDL1)
elseif(SE_WINDOWING STREQUAL "sdl2")
	if(SE_SYSTEM)
		target_link_libraries(scratch-everywhere PRIVATE ${SDL2_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL2_INCLUDE_DIRS})
	else()
		target_link_libraries(scratch-everywhere PRIVATE SDL2::SDL2)
		if(WIN32)
			target_link_libraries(scratch-everywhere PRIVATE SDL2::SDL2main)
		endif()
	endif()
    target_compile_definitions(scratch-everywhere PRIVATE WINDOWING_SDL2)
elseif(SE_WINDOWING STREQUAL "sdl3")
	if(SE_SYSTEM)
		target_link_libraries(scratch-everywhere PRIVATE ${SDL3_LIBRARIES})
		target_include_directories(scratch-everywhere PRIVATE ${SDL3_INCLUDE_DIRS})
	else()
		target_link_libraries(scratch-everywhere PRIVATE SDL3::SDL3)
	endif()
	target_compile_definitions(scratch-everywhere PRIVATE WINDOWING_SDL3)
elseif(SE_WINDOWING STREQUAL "glfw")
	target_link_libraries(scratch-everywhere PRIVATE glfw)
	target_compile_definitions(scratch-everywhere PRIVATE WINDOWING_GLFW)
endif()

if(SE_SYSTEM)
	target_compile_definitions(scratch-everywhere PRIVATE SYSTEM_LIBS)
endif()

if(NOT NINTENDO_SWITCH AND NOT PSP AND NOT VITA AND NOT WEBOS)
	target_compile_definitions(scratch-everywhere PRIVATE __PC__)
endif()

if(SE_RENDERER STREQUAL "sdl1")
	target_compile_definitions(scratch-everywhere PRIVATE RENDERER_SDL1)
elseif(SE_RENDERER STREQUAL "sdl2")
	target_compile_definitions(scratch-everywhere PRIVATE RENDERER_SDL2)
elseif(SE_RENDERER STREQUAL "sdl3")
	target_compile_definitions(scratch-everywhere PRIVATE RENDERER_SDL3)
elseif(SE_RENDERER STREQUAL "opengl")
	target_compile_definitions(scratch-everywhere PRIVATE RENDERER_OPENGL)
elseif(SE_RENDERER STREQUAL "headless")
	target_compile_definitions(scratch-everywhere PRIVATE RENDERER_HEADLESS)
endif()

if(SE_AUDIO)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_AUDIO)
endif()
if(SE_MENU)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_MENU)
endif()
if(SE_LOADSCREEN)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_LOADSCREEN)
endif()
if(SE_DOWNLOAD)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_DOWNLOAD)
	if(NOT SE_CLOUDVARS)
		target_link_libraries(scratch-everywhere PRIVATE curl)
	endif()
endif()
if(SE_CLOUDVARS)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_CLOUDVARS)
	target_link_libraries(scratch-everywhere PRIVATE mist++)
endif()


if(NOT PSP AND NOT WEBOS)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_TOUCH)
endif()
if(NOT PSP AND NOT VITA AND NOT NINTENDO_SWITCH)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_MOUSE)
endif()
if(NOT PSP AND NOT VITA AND NOT NINTENDO_SWITCH)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_KEYBOARD)
endif()
if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CROSSCOMPILING) ) # joystick doesn't work on MacOS with current Dockerfile
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_CONTROLLER)
endif()


if(SE_AUDIO)
	if(SE_AUDIO_ENGINE STREQUAL "sdl1")
		if(SE_SYSTEM)
			target_link_libraries(scratch-everywhere PRIVATE ${SDL_MIXER_LIBRARIES})
			target_include_directories(scratch-everywhere PRIVATE ${SDL_MIXER_INCLUDE_DIRS})
		else()
			if(BUILD_SHARED_LIBS)
				target_link_libraries(scratch-everywhere PRIVATE SDL_mixer::SDL_mixer)
			else()
				target_link_libraries(scratch-everywhere PRIVATE $<IF:$<TARGET_EXISTS:SDL_mixer::SDL_mixer-static>,SDL_mixer::SDL_mixer-static,SDL_mixer::SDL_mixer>)
			endif()
		endif()
	elseif(SE_AUDIO_ENGINE STREQUAL "sdl2")
		if(SE_SYSTEM AND NOT WEBOS)
			target_link_libraries(scratch-everywhere PRIVATE ${SDL2_MIXER_LIBRARIES})
			target_include_directories(scratch-everywhere PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
		else()
			target_link_libraries(scratch-everywhere PRIVATE SDL2::SDL2)
			if(WEBOS)
				target_link_libraries(scratch-everywhere PRIVATE SDL2_mixer)
			elseif(BUILD_SHARED_LIBS)
				target_link_libraries(scratch-everywhere PRIVATE SDL2_mixer::SDL2_mixer)
			else()
				target_link_libraries(scratch-everywhere PRIVATE $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer-static>,SDL2_mixer::SDL2_mixer-static,SDL2_mixer::SDL2_mixer>)
			endif()
		endif()
	elseif(SE_AUDIO_ENGINE STREQUAL "sdl3")
		if(SE_SYSTEM)
			target_link_libraries(scratch-everywhere PRIVATE ${SDL3_LIBRARIES})
			target_include_directories(scratch-everywhere PRIVATE ${SDL3_INCLUDE_DIRS})
			target_link_libraries(scratch-everywhere PRIVATE ${SDL3_MIXER_LIBRARIES})
			target_include_directories(scratch-everywhere PRIVATE ${SDL3_MIXER_INCLUDE_DIRS})
		else()
			target_link_libraries(scratch-everywhere PRIVATE SDL3::SDL3)
			if(BUILD_SHARED_LIBS)
				target_link_libraries(scratch-everywhere PRIVATE SDL3_mixer::SDL3_mixer)
			else()
				target_link_libraries(scratch-everywhere PRIVATE $<IF:$<TARGET_EXISTS:SDL3_mixer::SDL3_mixer-static>,SDL3_mixer::SDL3_mixer-static,SDL3_mixer::SDL3_mixer>)
			endif()
		endif()
	endif()
endif()

target_link_libraries(scratch-everywhere PRIVATE nlohmann_json::nlohmann_json)
add_library(ryuJS STATIC
  ${poipole807_ryuJS_SOURCE_DIR}/ryu/d2s.c
  ${poipole807_ryuJS_SOURCE_DIR}/ryu/d2s.h
)
target_link_libraries(scratch-everywhere PRIVATE ryuJS)
target_include_directories(ryuJS PUBLIC
  ${poipole807_ryuJS_SOURCE_DIR}
)
target_include_directories(scratch-everywhere PRIVATE ${nanosvg_SOURCE_DIR}/src)
target_include_directories(scratch-everywhere PRIVATE ${stb_SOURCE_DIR})
target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})

if(PSP)
    create_pbp_file(
        TARGET scratch-everywhere
        ICON_PATH ${CMAKE_SOURCE_DIR}/gfx/psp/ICON0.png
        BACKGROUND_PATH NULL
        PREVIEW_PATH ${CMAKE_SOURCE_DIR}/gfx/psp/PIC1.png
        TITLE scratch-everywhere
        VERSION ${CMAKE_PROJECT_VERSION}
    )
	# package the EBOOT.pbp
	add_custom_command(TARGET scratch-everywhere POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/scratch-psp-bundle"
	    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/EBOOT.PBP" "${CMAKE_BINARY_DIR}/scratch-psp-bundle/EBOOT.PBP"
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/scratch-psp.zip" --format=zip -- "${CMAKE_BINARY_DIR}/scratch-psp-bundle"
	)
elseif(NINTENDO_SWITCH)
	nx_generate_nacp(OUTPUT scratch-nx.nacp NAME "Scratch Everywhere!" AUTHOR "NateXS and Grady Link" VERSION ${CMAKE_PROJECT_VERSION})
	nx_create_nro(scratch-everywhere OUTPUT scratch-nx.nro ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs ICON "${CMAKE_SOURCE_DIR}/gfx/wiiu/switch-icon.jpg" NACP scratch-nx.nacp)
elseif(VITA)
	include("${VITASDK}/share/vita.cmake" REQUIRED)

	if(EXISTS ${CMAKE_SOURCE_DIR}/romfs/project.sb3)
		set(PROJ_FILE FILE ${CMAKE_SOURCE_DIR}/romfs/project.sb3 project.sb3)
	else()
		set(PROJ_FILE "")
	endif()

	set(VITA_GFXFILES)
	foreach(file IN LISTS GFXFILES)
		file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")
		set(DEST_PATH "gfx/${REL_PATH}")
		list(APPEND VITA_GFXFILES FILE "${file}" "${DEST_PATH}")
	endforeach()

	vita_create_self(scratch-vita.self scratch-everywhere)
	vita_create_vpk(scratch-vita.vpk "NTXS00053" scratch-vita.self
		VERSION "01.00"
		NAME "Scratch Everywhere!"
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/icon0.png sce_sys/icon0.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
		${VITA_GFXFILES}
		${PROJ_FILE}
	)
elseif(WEBOS)
	# toolchain path: ~/arm-webos-linux-gnueabi_sdk-buildroot/share/buildroot/toolchainfile.cmake
	target_link_options(scratch-everywhere PRIVATE "-static-libstdc++" "-static-libgcc")
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
	install(TARGETS scratch-everywhere RUNTIME DESTINATION .)
    install(FILES ${CMAKE_SOURCE_DIR}/gfx/webos/appinfo.json ${CMAKE_SOURCE_DIR}/gfx/webos/icon.png ${CMAKE_SOURCE_DIR}/gfx/webos/splash.png DESTINATION .)

    if(EXISTS ${CMAKE_SOURCE_DIR}/romfs/project.sb3)
        install(FILES "${CMAKE_SOURCE_DIR}/romfs/project.sb3" DESTINATION .)
    endif()

    foreach(file IN LISTS GFXFILES)
        file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")
        get_filename_component(DEST_DIR "${REL_PATH}" DIRECTORY)
        install(FILES "${file}" DESTINATION "gfx/${DEST_DIR}")
    endforeach()

    set(CPACK_GENERATOR "External")
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_SOURCE_DIR}/AresPackage.cmake")
    set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
    set(CPACK_MONOLITHIC_INSTALL TRUE)

    include(CPack)
endif()
