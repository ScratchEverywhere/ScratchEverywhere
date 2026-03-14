macro(package_platform)
	add_custom_command(TARGET scratch-everywhere POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sce_module"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sce_sys/about"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${OPENORBIS}/samples/piglet/sce_module" "${CMAKE_CURRENT_BINARY_DIR}/sce_module"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/gfx/ps4" "${CMAKE_CURRENT_BINARY_DIR}/sce_sys"
		COMMAND ${CMAKE_COMMAND} -E copy "${OPENORBIS}/samples/piglet/sce_sys/about/right.sprx" "${CMAKE_CURRENT_BINARY_DIR}/sce_sys/about"
	)

	file(GLOB_RECURSE ROMFS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/romfs/*")

	add_self(scratch-everywhere)
	add_pkg(scratch-everywhere "NTXS10053" "Scratch Everywhere!" ${CMAKE_PROJECT_VERSION} ${ROMFS_FILES})
endmacro()
