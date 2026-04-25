set(SE_DEFAULT_OUTPUT_NAME "scratch-3ds")

set(SE_RENDERER_VALID_OPTIONS "citro2d")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2" "sdl3")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT OFF)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_FORCE_CLOUDVARS_SOURCE_CURL ON)

set(SE_PLATFORM_DEFINITIONS "__3DS__")

set(SE_PLATFORM "3ds")

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

option(SE_BUILD_CIA "Whether or not to build a .cia output." ON)
set(SE_BANNERTOOL bannertool CACHE PATH "Path to bannertool executable")
set(SE_MAKEROM makerom CACHE PATH "Path to makerom executable")
set(SE_RAM 72 CACHE STRING "The amount of RAM to make available to SE! (MB)")

macro(package_platform)

	file(GLOB_RECURSE TTF_FILES "${CMAKE_CURRENT_SOURCE_DIR}/romfs/gfx/menu/*.ttf")
	foreach(TTF_FILE IN LISTS TTF_FILES)
		string(REGEX REPLACE "\\.ttf$" ".bcfnt" BCFNT_OUTPUT "${TTF_FILE}")
        add_custom_command(TARGET scratch-everywhere POST_BUILD
            COMMAND mkbcfnt -o "${BCFNT_OUTPUT}" "${TTF_FILE}"
            DEPENDS "${TTF_FILE}"
            VERBATIM
        )
    endforeach()

	ctr_generate_smdh(OUTPUT "${SE_OUTPUT_NAME}.smdh" NAME "${SE_APP_NAME}" AUTHOR "${SE_APP_AUTHOR}" ICON "${CMAKE_CURRENT_SOURCE_DIR}/gfx/icon.png" DESCRIPTION "${SE_APP_DESCRIPTION}")
	ctr_create_3dsx(scratch-everywhere ROMFS "${CMAKE_CURRENT_SOURCE_DIR}/romfs" SMDH "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.smdh")

	if(SE_BUILD_CIA)
		set(3DS_ROMFS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/romfs")
		configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gfx/3ds/makerom.rsf.in" "${CMAKE_CURRENT_BINARY_DIR}/makerom.rsf") # For setting the amount of ram
		add_custom_command(TARGET scratch-everywhere POST_BUILD
			COMMAND ${SE_BANNERTOOL} makebanner -i "${CMAKE_CURRENT_SOURCE_DIR}/gfx/3ds/banner.png" -a "${CMAKE_CURRENT_SOURCE_DIR}/gfx/3ds/banner.wav" -o "${CMAKE_CURRENT_BINARY_DIR}/banner.bnr"
			COMMAND ${SE_BANNERTOOL} makesmdh -s "${SE_APP_NAME}" -l "${SE_APP_DESCRIPTION}" -p "${SE_APP_AUTHOR}" -i "${CMAKE_CURRENT_SOURCE_DIR}/gfx/icon.png" -o "${CMAKE_CURRENT_BINARY_DIR}/icon.smdh"
			COMMAND	${SE_MAKEROM} -f "cia" -o "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.cia" -elf "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.elf" -rsf "${CMAKE_CURRENT_BINARY_DIR}/makerom.rsf" -banner "${CMAKE_CURRENT_BINARY_DIR}/banner.bnr" -icon "${CMAKE_CURRENT_BINARY_DIR}/icon.smdh" -target t -exefslogo
			COMMENT "Building ${SE_OUTPUT_NAME}.cia..."
		)
	endif()
endmacro()
