set(SE_DEFAULT_OUTPUT_NAME "scratch-psp")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT OFF)
set(SE_CMAKERC_DEFAULT ON)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM "psp")

macro(package_platform)
    create_pbp_file(
        TARGET scratch-everywhere
        ICON_PATH ${CMAKE_SOURCE_DIR}/gfx/psp/ICON0.png
        BACKGROUND_PATH NULL
        PREVIEW_PATH ${CMAKE_SOURCE_DIR}/gfx/psp/PIC1.png
        TITLE scratch-everywhere
		VERSION "${SE_APP_VERSION}"
    )
	# package the EBOOT.pbp
	add_custom_command(TARGET scratch-everywhere POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/EBOOT.PBP" "${CMAKE_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle/EBOOT.PBP"
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/${SE_OUTPUT_NAME}.zip" --format=zip -- "${CMAKE_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle"
	)
endmacro()
