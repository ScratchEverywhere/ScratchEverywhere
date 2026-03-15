set(SE_DEFAULT_OUTPUT_NAME "scratch-wiiu")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT ON)
set(SE_CMAKERC_DEFAULT ON)

set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)
set(SE_SYSTEM_CURL ON)

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

macro(package_platform)
	wut_create_rpx(scratch-everywhere)
	wut_create_wuhb(scratch-everywhere
		NAME "Scratch Everywhere!"
		AUTHOR "NateXS and Grady Link"
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

	add_custom_target(package
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu-bundle/"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu.rpx" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu-bundle/scratch-wii.rpx"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu.wuhb" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu-bundle/scratch-wiiu.wuhb"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/meta.xml" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu-bundle/meta.xml"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wiiu/hbl-icon.png" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu-bundle/icon.png"
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu.zip" --format=zip -- "${CMAKE_CURRENT_BINARY_DIR}/scratch-wiiu-bundle"
		DEPENDS scratch-everywhere
		COMMENT "Packaging..."
	)
endmacro()
