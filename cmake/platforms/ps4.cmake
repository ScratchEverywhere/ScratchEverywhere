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
	# System libraries
	target_link_libraries(se-interface INTERFACE
		SceNet SceNetCtl SceSystemService SceUserService)

	# Convert Mesa dynamic libraries into prx
	set(SCE_MODULE "")
	set(MESA_LIBS "libEGL.so.1;libGLESv2.so.2;libvulkan.so.1;libgallium-26.2.0.so")
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/sce_module")
	foreach(MESA_LIB IN LISTS MESA_LIBS)
		add_custom_command(TARGET scratch-everywhere POST_BUILD
			COMMAND ${ELF2SELF} "${PS4_HOMEBREW_SDK}/target/usr/lib/${MESA_LIB}"
				"${CMAKE_CURRENT_BINARY_DIR}/sce_module/${MESA_LIB}"
			COMMENT "Converting ${MESA_LIB} to prx"
			VERBATIM
		)
		list(APPEND SCE_MODULE "sce_module/${MESA_LIB}")
	endforeach()

	# Get sce_sys files to append
	set(SCE_SYS "")
	file(GLOB GFX_FILES "${CMAKE_CURRENT_SOURCE_DIR}/gfx/ps4/*")
	foreach(GFX_FILE ${GFX_FILES})
		get_filename_component(FILENAME "${GFX_FILE}" NAME)
		list(APPEND SCE_SYS "sce_sys/${FILENAME}")
	endforeach()

	# The create-gp4 tool needs relative paths
	if(SE_BUNDLE)
		set(ROMFS_FILES "")
	else()
		file(GLOB_RECURSE ROMFS_FILES RELATIVE
			"${CMAKE_CURRENT_SOURCE_DIR}/romfs" "${CMAKE_CURRENT_SOURCE_DIR}/romfs/*")
	endif()

	# More info about param.sfo at https://github.com/red-prig/fpPS4/wiki/PARAM.SFO
	set(PKG_CONTENT_ID "IV0001-${SE_APP_TITLEID}_00-SCRATCHPS4000000")
	add_custom_command(TARGET scratch-everywhere POST_BUILD
		# Copy ps4 gfx to sce_sys
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/gfx/ps4"
			"${CMAKE_CURRENT_BINARY_DIR}/sce_sys"
		# Copy romfs contents to build folder
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/romfs/"
			"${CMAKE_CURRENT_BINARY_DIR}"
		# Convert ELF to SELF
		COMMAND ${ELF2SELF} $<TARGET_FILE:scratch-everywhere>
			"${CMAKE_CURRENT_BINARY_DIR}/eboot.bin"
		# Generate param.sfo
		COMMAND ${MKSFO} "${CMAKE_CURRENT_BINARY_DIR}/sce_sys/param.sfo" -i APP_TYPE=1
			-s4 FORMAT=obs -s4 CATEGORY=gd -s128 TITLE=${SE_APP_NAME}
			-s12 TITLE_ID=${SE_APP_TITLEID} -s8 APP_VER=${SE_APP_VERSION}
			-s8 VERSION=${SE_APP_VERSION} -s48 CONTENT_ID=${PKG_CONTENT_ID}
			-i ATTRIBUTE=16384 -i DOWNLOAD_DATA_SIZE=0 -i SYSTEM_VER=0
		# Generate .gp4 with/without romfs
		COMMAND ${MKGP4} -c ${PKG_CONTENT_ID}
			-o "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.gp4"
			eboot.bin sce_sys/param.sfo ${SCE_SYS} ${SCE_MODULE} ${ROMFS_FILES}
		# Package content
		COMMAND ${MKPKG} -o "${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.pkg"
			"${CMAKE_CURRENT_BINARY_DIR}/${SE_OUTPUT_NAME}.gp4"
		COMMENT "Packaging ${SE_OUTPUT_NAME}.pkg..."
		VERBATIM
	)
endmacro()
