cmake_minimum_required(VERSION 3.16)

project(ScratchEverywhere VERSION 0.37 LANGUAGES CXX C)

# error C7555: use of designated initializers requires at least '/std:c++20'
if(MSVC)
	set(CMAKE_CXX_STANDARD 20)
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if(NOT CMAKE_CROSSCOMPILING OR CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/pc.cmake")
elseif(NINTENDO_SWITCH)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/switch.cmake")
elseif(PSP)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/psp.cmake")
elseif(VITA)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/vita.cmake")
elseif(NINTENDO_DS)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/nds.cmake")
elseif(EMSCRIPTEN)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/wasm.cmake")
elseif(NINTENDO_GAMECUBE)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/gamecube.cmake")
elseif(NINTENDO_WII)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/wii.cmake")
elseif(PS4)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/ps4.cmake")
elseif(NINTENDO_3DS)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/3ds.cmake")
elseif(NINTENDO_WIIU)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/wiiu.cmake")
endif()

if("fallback" IN_LIST SE_DEPS_VALID_OPTIONS)
	set(SE_DEPS "fallback" CACHE STRING "The method used to gather dependencies.")
elseif("source" IN_LIST SE_DEPS_VALID_OPTIONS)
	set(SE_DEPS "source" CACHE STRING "The method used to gather dependencies.")
else()
	set(SE_DEPS "system" CACHE STRING "The method used to gather dependencies.")
endif()
if(NOT SE_DEPS IN_LIST SE_DEPS_VALID_OPTIONS)
	message(
		FATAL_ERROR
		"Invalid value for SE_DEPS: '${SE_DEPS}'. Must be one of: ${SE_DEPS_VALID_OPTIONS}"
	)
endif()

set(SE_OUTPUT_NAME "${SE_DEFAULT_OUTPUT_NAME}" CACHE STRING "File name of the outputed executable")

option(SE_AUDIO "Enables audio in SE!" ON)

option(SE_CACHING "Enables pointer caching in Block, Monitor, and ParsedInput structs. This improves performance but at the cost of slightly longer load times and more RAM usage." ${SE_CACHING_DEFAULT})

if(SE_ALLOW_CLOUDVARS)
	option(SE_CLOUDVARS "Enable cloud variables." ON)
endif()

# This tbh should probably work on all of these platforms (except MAYBE PSP..) but i lowkey dont want to test it rn
if(SE_ALLOW_DOWNLOAD)
	option(SE_DOWNLOAD "Enable the ability to use the internet to download files, such as Text-To-Speech blocks." ON)
endif()

option(SE_MENU "Enables the SE! Main Menu." ON)

#  disable menu if a project is in RomFS
if (EXISTS "${CMAKE_SOURCE_DIR}/romfs/project" OR EXISTS "${CMAKE_SOURCE_DIR}/romfs/project.sb3")
	set(SE_MENU OFF)
	set(SE_LOADSCREEN OFF)
endif()

if(SE_MENU AND SE_HAS_THREADS)
	option(SE_LOADSCREEN "Enables SE!'s load screen." ON)
endif()

option(SE_SVG "Enable support for SVG images." ON)
option(SE_BITMAP "Enable support for Bitmap images." ON)

list(APPEND SE_RENDERER_VALID_OPTIONS "headless")

if("sdl2" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"sdl2"
		CACHE STRING "Select the desired renderer backend."
	)
elseif("sdl3" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"sdl3"
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
elseif("gl2d" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"gl2d"
		CACHE STRING "Select the desired renderer backend."
	)
elseif("citro2d" IN_LIST SE_RENDERER_VALID_OPTIONS)
	set(
		SE_RENDERER
		"citro2d"
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/renderers/${SE_RENDERER}.cmake")

list(APPEND SE_WINDOWING_VALID_OPTIONS "headless")

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
elseif("nds" IN_LIST SE_WINDOWING_VALID_OPTIONS)
	set(
		SE_WINDOWING
		"nds"
		CACHE STRING "Select the desired windowing backend."
	)
elseif("3ds" IN_LIST SE_WINDOWING_VALID_OPTIONS)
	set(
		SE_WINDOWING
		"3ds"
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/windowing/${SE_WINDOWING}.cmake")

list(APPEND SE_AUDIO_ENGINE_VALID_OPTIONS "headless")

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

if(SE_AUDIO)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/audio/${SE_AUDIO_ENGINE}.cmake")
endif()

option(SE_CMAKERC "Embed assets to executable with CMakeRC instead of loading from the working directory" ${SE_CMAKERC_DEFAULT})

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

if(NOT SE_SVG)
	list(FILTER GFXFILES EXCLUDE REGEX ".*/gfx/ingame/fonts/.*")
endif()

foreach(file IN LISTS GFXFILES)
	file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")

    set(DEST_PATH "${CMAKE_SOURCE_DIR}/romfs/gfx/${REL_PATH}")

    get_filename_component(DEST_DIR "${DEST_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${DEST_DIR}")

    configure_file("${file}" "${DEST_PATH}" COPYONLY)
endforeach()

# They're single header so I just use CPM no matter what
CPMAddPackage(
	NAME expected-lite
	GITHUB_REPOSITORY nonstd-lite/expected-lite
	VERSION 0.10.0
	DOWNLOAD_ONLY TRUE
)
add_library(expected-lite INTERFACE)
add_library(nonstd::expected-lite ALIAS expected-lite)
target_include_directories(expected-lite INTERFACE "$<BUILD_INTERFACE:${expected-lite_SOURCE_DIR}>/include")

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
if(SE_BITMAP)
	CPMAddPackage(
	NAME stb
	GITHUB_REPOSITORY nothings/stb
	GIT_TAG master
	)
endif()

if(SE_SVG)
	CPMAddPackage(
		NAME plutovg
		GITHUB_REPOSITORY sammycage/plutovg
		VERSION 1.3.0 # Version expected by LunaSVG
		OPTIONS "PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD ON" "PLUTOVG_BUILD_EXAMPLES OFF"
	)
	# Not all platforms need these but there's no harm in adding them since we don't need a multithreaded plutovg
	target_compile_definitions(plutovg PRIVATE __STDC_NO_THREADS__ __STDC_NO_ATOMICS__)
	target_compile_options(plutovg PRIVATE -Wno-error=incompatible-pointer-types)

	CPMAddPackage(
		NAME lunasvg
		GITHUB_REPOSITORY sammycage/lunasvg
		VERSION 3.5.0
		OPTIONS "LUNASVG_BUILD_EXAMPLES OFF" "plutovg_FOUND ON"
	)
endif()

include(FetchContent)
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.1.0/miniz-3.1.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniz)

if(SE_CLOUDVARS OR SE_DOWNLOAD)
	if(NOT SE_SYSTEM_CURL)
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
			"CURL_USE_LIBPSL OFF"
			"CURL_USE_OPENSSL OFF"
		)
		if(WEBOS)
			list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS OFF")
		else()
			list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS ON")
		endif()
		if(WIN32 OR WEBOS)
			list(APPEND CURL_OPTIONS "CURL_ENABLE_SSL OFF")
		endif()

		CPMAddPackage(
			NAME curl
			GIT_REPOSITORY "https://github.com/curl/curl.git"
			GIT_TAG "curl-8_15_0"
			VERSION 8.15.0
			PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/3ds-curl.patch"
			OPTIONS ${CURL_OPTIONS}
		)
	else()
		include(PkgConfig REQUIRED)
		pkg_check_modules(libcurl REQUIRED IMPORTED_TARGET GLOBAL libcurl>=8.4.0)
		add_library(curl ALIAS PkgConfig::libcurl)
		add_library(CURL::libcurl ALIAS PkgConfig::libcurl)
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
set_target_properties(scratch-everywhere PROPERTIES OUTPUT_NAME "${SE_OUTPUT_NAME}")
if(DEFINED SE_PLATFORM_DEFINTIONS)
	target_compile_definitions(scratch-everywhere PRIVATE ${SE_PLATFORM_DEFINITIONS})
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

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT MSVC AND NOT APPLE AND NOT NINTENDO_DS AND NOT EMSCRIPTEN AND NOT NINTENDO_GAMECUBE AND NOT NINTENDO_WII AND NOT NINTENDO_WIIU AND NOT NINTENDO_3DS AND NOT PS4)
	target_compile_options(scratch-everywhere PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_link_libraries(scratch-everywhere PRIVATE -fsanitize=address)
endif()

if(NOT BUILD_SHARED_LIBS AND MINGW)
	target_link_options(scratch-everywhere PRIVATE "-static-libgcc" "-static-libstdc++")
endif()

string(TOUPPER "RENDERER_${SE_RENDERER}" RENDERER_DEFINITION)
string(TOUPPER "WINDOWING_${SE_WINDOWING}" WINDOWING_DEFINITION)
target_compile_definitions(scratch-everywhere PRIVATE ${RENDERER_DEFINITION} ${WINDOWING_DEFINITION})

target_link_libraries(scratch-everywhere PRIVATE renderer_interface)
target_link_libraries(scratch-everywhere PRIVATE windowing_interface)
if(SE_AUDIO)
	target_link_libraries(scratch-everywhere PRIVATE audio_interface)
endif()

if(SE_CACHING)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_CACHING)
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
	target_link_libraries(scratch-everywhere PRIVATE curl)
endif()
if(SE_CLOUDVARS)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_CLOUDVARS)
	target_link_libraries(scratch-everywhere PRIVATE mist++)
endif()

if(SE_BITMAP)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_BITMAP)
endif()

if(SE_SVG)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_SVG)
endif()

if(SE_HAS_TOUCH)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_TOUCH)
endif()
if(SE_HAS_MOUSE)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_MOUSE)
endif()
if(SE_HAS_KEYBOARD)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_KEYBOARD)
endif()
if(SE_HAS_CONTROLLER)
	target_compile_definitions(scratch-everywhere PRIVATE PLATFORM_HAS_CONTROLLER)
endif()


target_link_libraries(scratch-everywhere PRIVATE nonstd::expected-lite nlohmann_json::nlohmann_json)
add_library(ryuJS STATIC
  ${poipole807_ryuJS_SOURCE_DIR}/ryu/d2s.c
  ${poipole807_ryuJS_SOURCE_DIR}/ryu/d2s.h
)
target_link_libraries(scratch-everywhere PRIVATE ryuJS)
target_include_directories(ryuJS PUBLIC
  ${poipole807_ryuJS_SOURCE_DIR}
)

if(SE_SVG)
	target_link_libraries(scratch-everywhere PRIVATE lunasvg::lunasvg)
endif()

if(SE_BITMAP)
	target_include_directories(scratch-everywhere PRIVATE ${stb_SOURCE_DIR})
endif()

target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})

# TODO: Move to cmake/platforms/wasm.cmake
if(EMSCRIPTEN)
	target_include_directories(scratch-everywhere PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/wasm)
	set_target_properties(scratch-everywhere PROPERTIES SUFFIX ".html")
	target_link_options(scratch-everywhere PRIVATE "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/romfs@/romfs" "--use-preload-plugins" "-sALLOW_MEMORY_GROWTH=1" "-sASYNCIFY" "-sEXPORTED_RUNTIME_METHODS=[ccall,HEAPU8,FS]" "-sEXPORTED_FUNCTIONS=[_main,_malloc,_free]" "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wasm/shell.html" "-sNO_DISABLE_EXCEPTION_CATCHING")
endif()

if(COMMAND package_platform)
	package_platform()
endif()
