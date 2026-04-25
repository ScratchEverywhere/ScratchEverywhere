set(SE_DEFAULT_OUTPUT_NAME "scratch-wiiu")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT ON)
set(SE_CMAKERC_DEFAULT ON)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_PKGCONF_CURL ON)

set(SE_PLATFORM_DEFINITIONS "__WIIU__")
set(SE_PLATFORM "wiiu")

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

macro(package_platform)
	wut_create_rpx(scratch-everywhere)
	wut_create_wuhb(scratch-everywhere
		NAME "${SE_APP_NAME}"
		AUTHOR "${SE_APP_AUTHOR}"
		ICON "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wiiu/icon.png"
		TVSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wiiu/tv-splash.png"
		DRCSPLASH "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wiiu/drc-splash.png"
	)

	execute_process(
		COMMAND date "+%Y%m%d%H%M%S"
		OUTPUT_VARIABLE WIIU_RELEASE_DATE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gfx/wiiu/meta.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/meta.xml" @ONLY)

	add_custom_target(package_wiiu
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle/"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.rpx" "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle/${SE_OUTPUT_NAME}.rpx"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.wuhb" "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle/${SE_OUTPUT_NAME}.wuhb"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/meta.xml" "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle/meta.xml"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wiiu/hbl-icon.png" "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle/icon.png"
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.zip" --format=zip -- "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}-bundle"
		DEPENDS scratch-everywhere
		COMMENT "Packaging..."
	)
endmacro()
