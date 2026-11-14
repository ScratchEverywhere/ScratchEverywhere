set(SE_DEFAULT_OUTPUT_NAME "scratch-ps4")

set(SE_RENDERER_VALID_OPTIONS "opengl_core" "sdl2")
set(SE_WINDOWING_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT ON)
set(SE_BUNDLE_DEFAULT OFF)

set(SE_ALLOW_BUNDLE ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_HAS_THREADS ON)
set(SE_LOADSCREEN ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM_DEFINITIONS "__PS4__")
set(SE_PLATFORM "ps4")

macro(package_platform)
	# Link system libraries
	target_link_libraries(se-interface INTERFACE SceNetCtl)

	# Temporarily copy ps4 gfx to source dir
	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/gfx/ps4/"
		DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/sce_sys")

	# Format version in a way it is acceptable by the PS4
	string(REPLACE "." ";" VERSION_LIST "${SE_APP_VERSION}")
	list(LENGTH VERSION_LIST VERSION_LENGTH)
	list(GET VERSION_LIST 0 VERSION_MAJOR)
	list(GET VERSION_LIST 1	VERSION_MINOR)
	string(LENGTH ${VERSION_MAJOR} MAJOR_LENGTH)
	string(LENGTH ${VERSION_MINOR} MINOR_LENGTH)
	if(MAJOR_LENGTH EQUAL 1)
		set(VERSION_MAJOR "0${VERSION_MAJOR}")
	endif()
	if(MINOR_LENGTH EQUAL 1 AND VERSION_LENGTH LESS 3)
		set(VERSION_MINOR "${VERSION_MINOR}0")
	endif()
	if(VERSION_LENGTH GREATER 2)
	    list(GET VERSION_LIST 2	VERSION_PATCH)
	    if(NOT VERSION_PATCH STREQUAL "")
		    set(VERSION_MINOR "${VERSION_MINOR}${VERSION_PATCH}")
		endif()
	endif()

	# More info about param.sfo at https://github.com/red-prig/fpPS4/wiki/PARAM.SFO
	set(PKG_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}")
	set(PKG_CONTENT_ID "IV0000-${SE_APP_TITLEID}_00-SCRATCHPS4000000")

	orbis_add_pkg(scratch-everywhere
		TITLE ${SE_APP_NAME}
		TITLE_ID ${SE_APP_TITLEID}
		CONTENT_ID ${PKG_CONTENT_ID}
		VERSION ${PKG_VERSION}
		ATTRIBUTE 16384
		ASSETS sce_sys romfs
		MODULES libEGL.so.1 libGLESv2.so.2 libvulkan.so.1 libgallium-26.2.0.so
	)

	# Rename pkg to output name and delete sce_sys
	add_custom_command(TARGET scratch-everywhere_pkg POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E rename
			"${CMAKE_CURRENT_BINARY_DIR}/${PKG_CONTENT_ID}.pkg"
			"${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.pkg"
		COMMAND ${CMAKE_COMMAND} -E remove_directory sce_sys
		COMMENT "Renaming and cleaning up..."
	)
endmacro()
