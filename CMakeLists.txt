cmake_minimum_required(VERSION 3.16)

project(ScratchEverywhere VERSION 0.38.1 LANGUAGES CXX C)

# error C7555: use of designated initializers requires at least '/std:c++20'
if(MSVC)
	set(CMAKE_CXX_STANDARD 20)
else()
	set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5) # Fixes CMakeRC and a few other things

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build all libraries as static")

include(CMakeDependentOption)

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

find_package(PkgConfig QUIET)

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
elseif(PS2)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/ps2.cmake")
elseif(PS4)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/ps4.cmake")
elseif(NINTENDO_3DS)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/3ds.cmake")
elseif(NINTENDO_WIIU)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/wiiu.cmake")
elseif(WEBOS)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platforms/webos.cmake")
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

cmake_dependent_option(SE_CLOUDVARS "Enable cloud variables." ON "SE_ALLOW_CLOUDVARS" OFF)

# This tbh should probably work on all of these platforms (except MAYBE PSP..) but i lowkey dont want to test it rn
cmake_dependent_option(SE_DOWNLOAD "Enable the ability to use the internet to download files, such as Text-To-Speech blocks." ON "SE_ALLOW_DOWNLOAD" OFF)

option(SE_MENU "Enables the SE! Main Menu." ON)

#  disable menu if a project is in RomFS
if(EXISTS "${CMAKE_SOURCE_DIR}/romfs/project" OR EXISTS "${CMAKE_SOURCE_DIR}/romfs/project.sb3")
	set(SE_MENU OFF)
	set(SE_LOADSCREEN OFF)
endif()

cmake_dependent_option(SE_LOADSCREEN "Enables SE!'s load screen." ON "SE_MENU;SE_HAS_THREADS" OFF)

option(SE_SVG "Enable support for SVG images." ON)
option(SE_BITMAP "Enable support for Bitmap images." ON)

function(set_preferred_cache_var var_name priority_list valid_options_list doc_string)
	if(DEFINED ${var_name} AND NOT "${${var_name}}" STREQUAL "")
		set(selected_value "${${var_name}}")
	else()
		set(selected_value "")
        foreach(option ${priority_list})
            if(option IN_LIST valid_options_list)
                set(selected_value "${option}")
                break()
            endif()
        endforeach()
	endif()

	if(NOT selected_value IN_LIST valid_options_list)
        message(
			FATAL_ERROR 
            "Invalid value for ${var_name}: '${selected_value}'. Must be one of: ${valid_options_list}"
        )
    endif()

	set(${var_name} "${selected_value}" CACHE STRING "${doc_string}" FORCE)
    set_property(CACHE ${var_name} PROPERTY STRINGS ${valid_options_list})
endfunction()

list(APPEND SE_RENDERER_VALID_OPTIONS "headless")
if(NOT DEFINED SE_RENDERER_PRIORITY)
	set(SE_RENDERER_PRIORITY "sdl2" "sdl3" "opengl" "sdl1" "citro2d" "gl2d" "headless")
endif()
set_preferred_cache_var(SE_RENDERER "${SE_RENDERER_PRIORITY}" "${SE_RENDERER_VALID_OPTIONS}" "Select the desired renderer backend.")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/renderers/${SE_RENDERER}.cmake")

set(SE_WINDOWING_PRIORITY "glfw" "sdl3" "sdl2" "sdl1" "3ds" "nds" "headless")
set_preferred_cache_var(SE_WINDOWING "${SE_WINDOWING_PRIORITY}" "${SE_WINDOWING_VALID_OPTIONS}" "Select the desired windowing backend.")
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

if(NOT DEFINED SE_CMAKERC_DEFAULT)
	set(SE_CMAKERC_DEFAULT OFF)
endif()
cmake_dependent_option(SE_CMAKERC "Embed assets to executable with CMakeRC instead of loading from the working directory" ${SE_CMAKERC_DEFAULT} "SE_ALLOW_CMAKERC" OFF)

set(CMAKE_FIND_LIBRARY_CUSTOM_LIB_DIRS OFF)

# Copy graphics files to romfs
if(SE_MENU)
	file(GLOB_RECURSE GFXFILES
	    "${CMAKE_SOURCE_DIR}/gfx/menu/*"
	    "${CMAKE_SOURCE_DIR}/gfx/ingame/*"
	)
else()
	file(GLOB_RECURSE GFXFILES
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

set(PLATFORM_DIRS ${SE_PLATFORM})
list(TRANSFORM PLATFORM_DIRS PREPEND "source/platforms/")
set(SOURCES "source" "source/runtime" "source/runtime/blocks" "source/platforms" ${PLATFORM_DIRS} "source/renderers/${SE_RENDERER}" "source/windowing/${SE_WINDOWING}" "source/audio/${SE_AUDIO_ENGINE}")

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
if(WIN32)
	list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/gfx/windows/resource.rc")
endif()

add_executable(scratch-everywhere WIN32 ${SOURCE_FILES})
target_include_directories(scratch-everywhere PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external-include)
set_target_properties(scratch-everywhere PROPERTIES OUTPUT_NAME "${SE_OUTPUT_NAME}")
if(DEFINED SE_PLATFORM_DEFINITIONS)
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

if(SE_ASAN)
	target_compile_options(scratch-everywhere PRIVATE -fsanitize=address -fno-omit-frame-pointer)
	target_link_libraries(scratch-everywhere PRIVATE -fsanitize=address)
endif()

if(SE_STATIC_LIBGCC)
	target_link_options(scratch-everywhere PRIVATE "-static-libgcc")
endif()
if(SE_STATIC_LIBSTDCPP)
	target_link_options(scratch-everywhere PRIVATE "-static-libstdc++")
endif()

if(NOT BUILD_SHARED_LIBS AND MINGW)
	target_link_options(scratch-everywhere PRIVATE "-static" "-static-libgcc" "-static-libstdc++")
endif()

string(TOUPPER "RENDERER_${SE_RENDERER}" RENDERER_DEFINITION)
string(TOUPPER "WINDOWING_${SE_WINDOWING}" WINDOWING_DEFINITION)
target_compile_definitions(scratch-everywhere PRIVATE ${RENDERER_DEFINITION} ${WINDOWING_DEFINITION})

if(TARGET threads_interface)
	target_link_libraries(scratch-everywhere PRIVATE threads_interface)
endif()

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
	se_add_dependency(scratch-everywhere libcurl)
endif()
if(SE_CLOUDVARS)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_CLOUDVARS)
	se_add_dependency(scratch-everywhere mist++)
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

CPMAddPackage(
	NAME poipole807_ryuJS
	GITHUB_REPOSITORY poipole807/ryuJS
	VERSION 3.0
)
add_library(ryuJS STATIC
  ${poipole807_ryuJS_SOURCE_DIR}/ryu/d2s.c
  ${poipole807_ryuJS_SOURCE_DIR}/ryu/d2s.h
)
target_include_directories(ryuJS PUBLIC
  ${poipole807_ryuJS_SOURCE_DIR}
)
target_link_libraries(scratch-everywhere PRIVATE ryuJS)

se_add_dependency(scratch-everywhere nlohmann_json)
se_add_dependency(scratch-everywhere miniz)
se_add_dependency(scratch-everywhere expected-lite)

if(SE_SVG)
	se_add_dependency(scratch-everywhere lunasvg)
endif()

if(SE_BITMAP)
	se_add_dependency(scratch-everywhere stb)
endif()

target_include_directories(scratch-everywhere PRIVATE ${SOURCES})

# TODO: Move to cmake/platforms/wasm.cmake
if(EMSCRIPTEN)
	set_target_properties(scratch-everywhere PROPERTIES SUFFIX ".html")
	target_link_options(scratch-everywhere PRIVATE "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/romfs@/romfs" "--use-preload-plugins" "-sALLOW_MEMORY_GROWTH=1" "-sASYNCIFY" "-sEXPORTED_RUNTIME_METHODS=[ccall,HEAPU8,FS]" "-sEXPORTED_FUNCTIONS=[_main,_malloc,_free]" "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wasm/shell.html" "-sNO_DISABLE_EXCEPTION_CATCHING")
endif()

if(COMMAND package_platform)
	package_platform()
endif()
