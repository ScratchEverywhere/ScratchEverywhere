set(SE_DEFAULT_OUTPUT_NAME "scratch-gba")

set(SE_RENDERER_VALID_OPTIONS "gl2d") #will set this to correct renderer eventually
set(SE_AUDIO_ENGINE_VALID_OPTIONS "gba")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT OFF)

set(SE_ALLOW_CMAKERC OFF)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_PLATFORM_DEFINITIONS "__GBA__" "ARM7")
set(SE_PLATFORM "gba")

set(SE_HAS_THREADS OFF)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

macro(package_platform)
	nds_create_rom(scratch-everywhere
		NAME "${SE_APP_NAME}"
		SUBTITLE "${SE_APP_DESCRIPTION}"
		AUTHOR "${SE_APP_AUTHOR}"
	GBAROMFS "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
	)
endmacro()
