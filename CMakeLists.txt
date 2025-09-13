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
	option(SE_ENABLE_BUBBLES "Enables the bubbles loading screen" ON)
endif()
if(NINTENDO_3DS)
	option(SE_BANNERTOOL "Path to bannertool executable" bannertool)
	option(SE_MAKEROM "Path to makerom executable" makerom)
	set(SE_RAM 72 CACHE STRING "The amount of RAM to make available to SE! (MB)")
endif()
if(NOT (NINTENDO_GAMECUBE OR VITA OR NINTENDO_SWITCH OR NINTENDO_WII))
	option(SE_CLOUDVARS "Enable cloud variables." ON) # On by default now that you know longer need to globally install special dependencies
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

if(SE_CLOUDVARS)
	if(NINTENDO_3DS)
		include(ExternalProject)

		set(CURL_INSTALL_DIR ${CMAKE_BINARY_DIR}/curl-install)
		set(CURL_SOURCE_DIR ${CMAKE_BINARY_DIR}/curl-src)

		ExternalProject_Add(
			curl
			URL https://curl.haxx.se/download/curl-8.4.0.tar.xz
			URL_HASH SHA256=16c62a9c4af0f703d28bda6d7bbf37ba47055ad3414d70dec63e2e6336f2a82d
			SOURCE_DIR ${CURL_SOURCE_DIR}
			PATCH_COMMAND patch -Np1 -i "${CMAKE_SOURCE_DIR}/cmake/curl-3ds.patch"
			CONFIGURE_COMMAND bash -c ". ${DEVKITPRO}/3dsvars.sh && <SOURCE_DIR>/configure CFLAGS=\"$CFLAGS\" CPPFLAGS=\"$CPPFLAGS\" LIBS=\"-lctru\" --host=arm-none-eabi --disable-shared --enable-static --disable-ipv6 --disable-unix-sockets --disable-threaded-resolver --disable-manual --disable-pthreads --disable-socketpair --disable-ntlm-wb --with-mbedtls=${DEVKITPRO}/portlibs/3ds --with-ca-bundle=sdmc:/config/ssl/cacert.pem"
			BUILD_COMMAND bash -c ". ${DEVKITPRO}/3dsvars.sh && $(MAKE) -C lib"
			INSTALL_COMMAND ""
			BUILD_IN_SOURCE 1
		)

		add_library(libcurl STATIC IMPORTED)
		set_target_properties(libcurl PROPERTIES
			IMPORTED_LOCATION ${CURL_INSTALL_DIR}/usr/local/lib/libcurl.a
			INTERFACE_INCLUDE_DIRECTORIES ${CURL_INSTALL_DIR}/usr/local/include
		)

		add_dependencies(libcurl curl)
	elseif(NINTENDO_WIIU)
		include(ExternalProject)

		set(CURL_INSTALL_DIR ${CMAKE_BINARY_DIR}/curl-install)
		set(CURL_SOURCE_DIR ${CMAKE_BINARY_DIR}/curl-src)

		ExternalProject_Add(
			curl
			URL https://curl.se/download/curl-8.7.1.tar.xz
			URL_HASH SHA256=6fea2aac6a4610fbd0400afb0bcddbe7258a64c63f1f68e5855ebc0c659710cd
			SOURCE_DIR ${CURL_SOURCE_DIR}
			CONFIGURE_COMMAND bash -c ". ${DEVKITPRO}/wiiuvars.sh && <SOURCE_DIR>/configure --host=powerpc-eabi --disable-shared --enable-static --disable-ipv6 --disable-unix-sockets --disable-threaded-resolver --disable-manual --disable-pthreads --disable-socketpair --disable-ntlm-wb --with-mbedtls=${DEVKITPRO}/portlibs/wiiu --with-ca-path=/vol/storage_mlc01/sys/title/0005001b/10054000/content/scerts"
			BUILD_COMMAND bash -c ". ${DEVKITPRO}/wiiuvars.sh && $(MAKE) -C lib"
			INSTALL_COMMAND ""
			BUILD_IN_SOURCE 1
		)

		add_library(libcurl STATIC IMPORTED)
		set_target_properties(libcurl PROPERTIES
			IMPORTED_LOCATION ${CURL_INSTALL_DIR}/usr/local/lib/libcurl.a
			INTERFACE_INCLUDE_DIRECTORIES ${CURL_INSTALL_DIR}/usr/local/include
		)

		add_dependencies(libcurl curl)
	else()
		CPMAddPackage(
			NAME curl
			GIT_REPOSITORY "https://github.com/curl/curl.git"
			GIT_TAG "curl-8_15_0"
			VERSION 8.15.0
			OPTIONS BUILD_CURL_EXE OFF BUILD_TESTING OFF BUILD_EXAMPLES OFF
		)
	endif()
	CPMAddPackage("gh:ScratchEverywhere/mistpp@0.3.1")
endif()

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

if(NINTENDO_3DS)
	set(TARGET_NAME "scratch-3ds")
elseif(NINTENDO_WIIU)
	set(TARGET_NAME "scratch-wiiu")
elseif(NINTENDO_SWITCH)
	set(TARGET_NAME "scratch-nx")
elseif(NINTENDO_WII)
	set(TARGET_NAME "scratch-wii")
elseif(NINTENDO_GAMECUBE)
	set(TARGET_NAME "scratch-gamecube")
elseif(VITA)
	set(TARGET_NAME "scratch-vita")
else()
	set(TARGET_NAME "scratch-pc")
endif()

add_executable(${TARGET_NAME} ${SOURCE_FILES})

# Copy graphics files to romfs
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
	romfs_add(${TARGET_NAME} "romfs")
elseif(NINTENDO_WII)
	include("${DEVKITPRO}/portlibs/wii/share/romfs-ogc.cmake" REQUIRED)
	romfs_add(${TARGET_NAME} "romfs")
elseif(NINTENDO_GAMECUBE)
	include("${DEVKITPRO}/portlibs/gamecube/share/romfs-ogc.cmake" REQUIRED)
	romfs_add(${TARGET_NAME} "romfs")
endif()

if(NOT NINTENDO_3DS)
	target_include_directories(${TARGET_NAME} PRIVATE
		${SDL2_INCLUDE_DIRS}
	    ${SDL2_IMAGE_INCLUDE_DIRS}
	    ${SDL2_TTF_INCLUDE_DIRS}
	)

	target_link_libraries(${TARGET_NAME}
	    ${SDL2_LIBRARIES}
		${SDL2_IMAGE_LIBRARIES}
	    ${SDL2_TTF_LIBRARIES}
	)

	if(SE_ENABLE_AUDIO)
		target_include_directories(${TARGET_NAME} PRIVATE ${SDL2_MIXER_INCLUDE_DIRS})
		target_link_libraries(${TARGET_NAME} ${SDL2_MIXER_LIBRARIES})
	endif()
endif()

# TODO: add cloud variable support

if(NINTENDO_WIIU OR NINTENDO_SWITCH OR NINTENDO_WII OR NINTENDO_GAMECUBE)
	target_compile_definitions(${TARGET_NAME} PRIVATE SDL_BUILD)
elseif(VITA)
	target_compile_definitions(${TARGET_NAME} PRIVATE VITA SDL_BUILD)
elseif(NOT NINTENDO_3DS)
	target_compile_definitions(${TARGET_NAME} PRIVATE __PC__ SDL_BUILD)
endif()

if(NINTENDO_WII)
	target_compile_definitions(${TARGET_NAME} PRIVATE WII)
elseif(NINTENDO_GAMECUBE)
	target_compile_definitions(${TARGET_NAME} PRIVATE GAMECUBE)
endif()

if(SE_ENABLE_AUDIO)
	target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_AUDIO)
endif()
if(SE_ENABLE_BUBBLES)
	target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_BUBBLES)
endif()
if(SE_CLOUDVARS)
	target_compile_definitions(${TARGET_NAME} PRIVATE ENABLE_CLOUDVARS)
	target_link_libraries(${TARGET_NAME} mist++)
endif()

target_link_libraries(${TARGET_NAME} nlohmann_json::nlohmann_json)


if(NINTENDO_3DS)
	target_link_libraries(${TARGET_NAME} nanosvg citro2d citro3d)
	if (SE_ENABLE_AUDIO)
		target_link_libraries(${TARGET_NAME} SDL2::SDL2 SDL2_mixer::SDL2_mixer-static)
	endif()
endif()

if(NINTENDO_3DS)
	target_include_directories(${TARGET_NAME} PRIVATE ${SOURCES} ${miniz_SOURCE_DIR} "${nanosvg_SOURCE_DIR}/src" ${stb_SOURCE_DIR})
else()
	target_include_directories(${TARGET_NAME} PRIVATE ${SOURCES} ${miniz_SOURCE_DIR})
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
	wut_create_rpx(${TARGET_NAME})
	wut_create_wuhb(${TARGET_NAME}
		NAME "Scratch Everywhere!"
		AUTHOR "NateXS and Grady Link"
		ICON "gfx/wiiu/icon.png"
		TVSPLASH "gfx/wiiu/tv-splash.png"
		DRCSPLASH "gfx/wiiu/drc-splash.png"
	)
	file(GENERATE
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}/meta.xml"
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
		TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		ARGS "${CMAKE_BINARY_DIR}/${TARGET_NAME}.wuhb" "${CMAKE_BINARY_DIR}/${TARGET_NAME}/${TARGET_NAME}.wuhb"
		COMMENT "Packaging '${TARGET_NAME}.wuhb'..."
	)
	add_custom_command(
		TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		ARGS "${CMAKE_BINARY_DIR}/${TARGET_NAME}.rpx" "${CMAKE_BINARY_DIR}/${TARGET_NAME}/${TARGET_NAME}.rpx"
		COMMENT "Packaging '${TARGET_NAME}.rpx'..."
	)
	configure_file("${CMAKE_SOURCE_DIR}/gfx/wiiu/hbl-icon.png" "${CMAKE_BINARY_DIR}/${TARGET_NAME}/icon.png" COPYONLY)
elseif(NINTENDO_3DS)
	ctr_generate_smdh(OUTPUT ${TARGET_NAME}.smdh NAME ${APP_NAME} AUTHOR "NateXS" ICON "${CMAKE_SOURCE_DIR}/gfx/icon.png" DESCRIPTION ${APP_DESCRIPTION})
	ctr_create_3dsx(${TARGET_NAME} ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs SMDH ${TARGET_NAME}.smdh)

	# CIA
	add_custom_command(
		TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${SE_BANNERTOOL}
		ARGS makebanner -i "${CMAKE_SOURCE_DIR}/gfx/3ds/banner.png" -a "${CMAKE_SOURCE_DIR}/gfx/3ds/banner.wav" -o "banner.bnr"
		COMMENT "Building banner.bnr..."
	)
	add_custom_command(
		TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${SE_BANNERTOOL}
		ARGS makesmdh -s "${APP_NAME}" -l "${APP_DESCRIPTION}" -p "NateXS" -i "${CMAKE_SOURCE_DIR}/gfx/icon.png" -o "icon.smdh"
		COMMENT "Building icon.smdh..."
	)
	configure_file("${CMAKE_SOURCE_DIR}/gfx/3ds/makerom.rsf.in" "${CMAKE_BINARY_DIR}/makerom.rsf") # For setting the amount of ram
	add_custom_command(
		TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${SE_MAKEROM}
		ARGS -f "cia" -o "${TARGET_NAME}.cia" -elf "${TARGET_NAME}.elf" -rsf "${CMAKE_BINARY_DIR}/makerom.rsf" -banner "banner.bnr" -icon icon.smdh -target t -exefslogo
		COMMENT "Building ${TARGET_NAME}.cia"
	)
elseif(NINTENDO_SWITCH)
	nx_generate_nacp(OUTPUT ${TARGET_NAME}.nacp NAME ${APP_NAME} AUTHOR "NateXS and Grady Link" VERSION ${CMAKE_PROJECT_VERSION})
	nx_create_nro(${TARGET_NAME} ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs ICON "${CMAKE_SOURCE_DIR}/gfx/wiiu/icon.png" NACP ${TARGET_NAME}.nacp)
elseif(NINTENDO_WII)
	file(GENERATE
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}/meta.xml"
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

	ogc_create_dol(${TARGET_NAME})
	add_custom_command(
		TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy
		ARGS "${CMAKE_BINARY_DIR}/${TARGET_NAME}.dol" "${CMAKE_BINARY_DIR}/${TARGET_NAME}/boot.dol"
		COMMENT "Packaging '${TARGET_NAME}.dol'..."
	)
	configure_file("${CMAKE_SOURCE_DIR}/gfx/wii/icon.png" "${CMAKE_BINARY_DIR}/${TARGET_NAME}/icon.png" COPYONLY)
elseif(NINTENDO_GAMECUBE)
	ogc_create_dol(${TARGET_NAME})
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

	vita_create_self(${TARGET_NAME}.self ${TARGET_NAME})
	vita_create_vpk(${TARGET_NAME}.vpk ${VITA_TITLEID} ${TARGET_NAME}.self
		VERSION "01.00"
		NAME ${APP_NAME}
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/icon0.png sce_sys/icon0.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
		${VITA_GFXFILES}
		${PROJ_FILE}
	)
endif()
