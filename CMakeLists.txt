cmake_minimum_required(VERSION 3.16)

project(Scratch-Everywhere VERSION 0.20 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")
    message(STATUS "Downloading CPM.cmake...")
    file(DOWNLOAD "https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake"
         "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake"
         STATUS status LOG log_content)
    if(status)
        message(FATAL_ERROR "Failed to download CPM.cmake: ${status}\n${log_content}")
    endif()
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

option(SE_ENABLE_AUDIO "Enables audio in SE!" ON)
if(NINTENDO_3DS OR NINTENDO_WII OR NINTENDO_GAMECUBE)
	option(SE_ENABLE_BUBBLES "Enables the bubbles loading screen." ON)
endif()

if(NINTENDO_3DS)
	if(SE_ENABLE_AUDIO)
		CPMAddPackage(
			NAME SDL2
			GITHUB_REPOSITORY libsdl-org/SDL
			GIT_TAG release-2.32.8
			VERSION 2.32.8
		)
		CPMAddPackage(
			NAME SDL2_mixer
			GITHUB_REPOSITORY libsdl-org/SDL_mixer
			GIT_TAG release-2.8.1
			VERSION 2.8.1
			OPTIONS "SDL2MIXER_VORBIS STB" "SDL2MIXER_MP3 MINIMP3" "SDL2MIXER_FLAC DRFLAC" "SDL2MIXER_MOD OFF" "SDL2MIXER_OPUS OFF" "SDL2MIXER_FLUIDSYNTH OFF" "SDL2MIXER_WAVPACK OFF" "SDL2MIXER_MIDI OFF"
		)
	endif()
	CPMAddPackage(
		NAME nanosvg
		GITHUB_REPOSITORY memononen/nanosvg
		GIT_TAG HEAD # TODO: pin commit
	)
	CPMAddPackage(
		NAME stb
		GITHUB_REPOSITORY nothings/stb
		GIT_TAG master # TODO: pin commit
		DOWNLOAD_ONLY YES
	)
else()
	find_package(PkgConfig REQUIRED)

	pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
	pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image>=2.0.0)
	pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf>=2.0.0)
	if(SE_ENABLE_AUDIO)
		pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer>=2.0.0)
	endif()
endif()

CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.12.0
)

include(FetchContent)
FetchContent_Declare(
    miniz
    URL https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip
)
FetchContent_MakeAvailable(miniz)

if(NINTENDO_3DS)
	set(SOURCES source source/scratch source/scratch/blocks source/scratch/menus source/3ds source/sdl/audio)
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

file(GLOB_RECURSE GFXFILES "${CMAKE_SOURCE_DIR}/gfx/*")
foreach(file IN LISTS GFXFILES)
    file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")

    set(DEST_PATH "${CMAKE_SOURCE_DIR}/romfs/gfx/${REL_PATH}")

    get_filename_component(DEST_DIR "${DEST_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${DEST_DIR}")

    configure_file("${file}" "${DEST_PATH}" COPYONLY)
endforeach()

if(NINTENDO_WII OR NINTENDO_GAMECUBE)
	set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if(NINTENDO_WIIU)
	include("${DEVKITPRO}/portlibs/wiiu/share/romfs-wiiu.cmake" REQUIRED)
	romfs_add(scratch-everywhere "romfs")
elseif(NINTENDO_WII)
	include("${DEVKITPRO}/portlibs/wii/share/romfs-ogc.cmake" REQUIRED)
	romfs_add(scratch-everywhere "romfs")
elseif(NINTENDO_GAMECUBE)
	include("${DEVKITPRO}/portlibs/gamecube/share/romfs-ogc.cmake" REQUIRED)
	romfs_add(scratch-everywhere "romfs")
endif()

if(NOT NINTENDO_3DS)
	target_include_directories(scratch-everywhere PRIVATE
		${SDL2_INCLUDE_DIRS}
	    ${SDL2_IMAGE_INCLUDE_DIRS}
	    ${SDL2_TTF_INCLUDE_DIRS}
	)

	target_link_libraries(scratch-everywhere
	    ${SDL2_LIBRARIES}
		${SDL2_IMAGE_LIBRARIES}
	    ${SDL2_TTF_LIBRARIES}
	)

	if(SE_ENABLE_AUDIO)
		target_include_directories(scratch-everywhere PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
		target_link_libraries(scratch-everywhere ${SDL2_MIXER_LIBRARIES})
	endif()
endif()

# TODO: add cloud variable support

if(NINTENDO_WIIU OR NINTENDO_SWITCH OR NINTENDO_WII OR NINTENDO_GAMECUBE)
	target_compile_definitions(scratch-everywhere PRIVATE SDL_BUILD)
elseif(VITA)
	target_compile_definitions(scratch-everywhere PRIVATE VITA SDL_BUILD)
elseif(NOT NINTENDO_3DS)
	target_compile_definitions(scratch-everywhere PRIVATE __PC__ SDL_BUILD)
endif()

if(SE_ENABLE_AUDIO)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_AUDIO)
endif()
if (SE_ENABLE_BUBBLES)
	target_compile_definitions(scratch-everywhere PRIVATE ENABLE_BUBBLES)
endif()

target_link_libraries(scratch-everywhere nlohmann_json::nlohmann_json)


if(NINTENDO_3DS)
	target_link_libraries(scratch-everywhere nanosvg citro2d citro3d)
	if (SE_ENABLE_AUDIO)
		target_link_libraries(scratch-everywhere SDL2::SDL2 SDL2_mixer::SDL2_mixer-static)
	endif()
endif()

if(NINTENDO_3DS)
	target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR} "${nanosvg_SOURCE_DIR}/src" ${stb_SOURCE_DIR})
else()
	target_include_directories(scratch-everywhere PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})
endif()

set(APP_NAME "Scratch Everywhere!")
set(APP_DESCRIPTION "Play Scratch Games anywhere!")

set(VITA_TITLEID "NTXS00053")

set(BUILD_TIMESTAMP "")
execute_process(
    COMMAND date +"%Y%m%d%H%M%S"
    OUTPUT_VARIABLE BUILD_TIMESTAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(REPLACE "\"" "" BUILD_TIMESTAMP "${BUILD_TIMESTAMP}")

if (NINTENDO_WIIU)
	wut_create_rpx(scratch-everywhere)
	wut_create_wuhb(scratch-everywhere
		NAME "Scratch Everywhere!"
		AUTHOR "NateXS and Grady Link"
		ICON "gfx/wiiu/icon.png"
		TVSPLASH "gfx/wiiu/tv-splash.png"
		DRCSPLASH "gfx/wiiu/drc-splash.png"
	)
	file(GENERATE
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scratch-everywhere/meta.xml"
		CONTENT "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>
<app version=\"1\">
    <name>${APP_NAME}</name>
	<coder>NateXS and Grady Link</coder>
	<version>${CMAKE_PROJECT_VERSION}</version>
	<release_date>${BUILD_TIMESTAMP}</release_date>
	<short_description>${APP_DESCRIPTION}</short_description>
    <long_description>${APP_DESCRIPTION}</long_description>
</app>"
	)
	add_custom_command(
		TARGET scratch-everywhere
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		ARGS "${CMAKE_BINARY_DIR}/scratch-everywhere.wuhb" "${CMAKE_BINARY_DIR}/scratch-everywhere/scratch-everywhere.wuhb"
		COMMENT "Packaging 'scratch-everywhere.wuhb'..."
	)
	add_custom_command(
		TARGET scratch-everywhere
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		ARGS "${CMAKE_BINARY_DIR}/scratch-everywhere.rpx" "${CMAKE_BINARY_DIR}/scratch-everywhere/scratch-everywhere.rpx"
		COMMENT "Packaging 'scratch-everywhere.rpx'..."
	)
	configure_file("${CMAKE_SOURCE_DIR}/gfx/wiiu/hbl-icon.png" "${CMAKE_BINARY_DIR}/scratch-everywhere/icon.png" COPYONLY)
elseif(NINTENDO_3DS)
	ctr_generate_smdh(OUTPUT scratch-everywhere.smdh NAME ${APP_NAME} AUTHOR "NateXS" ICON "${CMAKE_SOURCE_DIR}/gfx/icon.png" DESCRIPTION ${APP_DESCRIPTION})
	ctr_create_3dsx(scratch-everywhere ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs SMDH scratch-everywhere.smdh)
elseif(NINTENDO_SWITCH)
	nx_generate_nacp(OUTPUT scratch-everywhere.nacp NAME ${APP_NAME} AUTHOR "NateXS and Grady Link" VERSION ${CMAKE_PROJECT_VERSION})
	nx_create_nro(scratch-everywhere ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs ICON "gfx/wiiu/icon.png" NACP scratch-everywhere.nacp)
elseif(NINTENDO_WII)
	file(GENERATE
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/scratch-everywhere/meta.xml"
		CONTENT "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>
<app version=\"1\">
    <name>${APP_NAME}</name>
	<coder>NateXS</coder>
	<version>${CMAKE_PROJECT_VERSION}</version>
	<release_date>${BUILD_TIMESTAMP}</release_date>
	<short_description>${APP_DESCRIPTION}</short_description>
    <long_description>${APP_DESCRIPTION}</long_description>
</app>"
	)

	ogc_create_dol(scratch-everywhere)
	add_custom_command(
		TARGET scratch-everywhere
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		ARGS "${CMAKE_BINARY_DIR}/scratch-everywhere.dol" "${CMAKE_BINARY_DIR}/scratch-everywhere/boot.dol"
		COMMENT "Packaging 'scratch-everywhere.dol'..."
	)
	configure_file("${CMAKE_SOURCE_DIR}/gfx/wii/icon.png" "${CMAKE_BINARY_DIR}/scratch-everywhere/icon.png" COPYONLY)
elseif(NINTENDO_GAMECUBE)
	ogc_create_dol(scratch-everywhere)
elseif(VITA)
	include("${VITASDK}/share/vita.cmake" REQUIRED)

	set(VITA_GFXFILES)
	foreach(file IN LISTS GFXFILES)
		file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")
		set(DEST_PATH "gfx/${REL_PATH}")
		list(APPEND VITA_GFXFILES FILE "${file}" "${DEST_PATH}")
	endforeach()

	vita_create_self(scratch-everywhere.self scratch-everywhere)
	vita_create_vpk(scratch-everywhere.vpk ${VITA_TITLEID} scratch-everywhere.self
		VERSION "01.00"
		NAME ${APP_NAME}
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/icon0.png sce_sys/icon0.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
		${VITA_GFXFILES}
	)
endif()
