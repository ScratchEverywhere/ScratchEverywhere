set(SE_DEFAULT_OUTPUT_NAME "scratch-ps4")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_CACHING_DEFAULT ON)

set(SE_ALLOW_CMAKERC OFF)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM "ps4")

macro(package_platform)
	add_custom_command(TARGET scratch-everywhere POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sce_module"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sce_sys/about"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${OPENORBIS}/samples/piglet/sce_module" "${CMAKE_CURRENT_BINARY_DIR}/sce_module"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/gfx/ps4" "${CMAKE_CURRENT_BINARY_DIR}/sce_sys"
		COMMAND ${CMAKE_COMMAND} -E copy "${OPENORBIS}/samples/piglet/sce_sys/about/right.sprx" "${CMAKE_CURRENT_BINARY_DIR}/sce_sys/about"
	)

	# The create-gp4 tool needs relative paths
	file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/romfs/" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
	file(GLOB_RECURSE ROMFS_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/romfs" "${CMAKE_CURRENT_SOURCE_DIR}/romfs/*")

	add_self(scratch-everywhere)
	add_pkg(scratch-everywhere "${SE_APP_TITLEID}" "${SE_APP_NAME}" "${SE_APP_VERSION}" ${ROMFS_FILES})
endmacro()
