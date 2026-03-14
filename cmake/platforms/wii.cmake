set(SE_CACHING_DEFAULT OFF)

macro(package_platform)
	ogc_create_dol(scratch-everywhere)

	execute_process(
		COMMAND date "+%Y%m%d%H%M%S"
		OUTPUT_VARIABLE WII_RELEASE_DATE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gfx/wii/meta.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/meta.xml" @ONLY)

	add_custom_command(TARGET scratch-everywhere POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii-bundle/apps/scratch-wii"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii.dol" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii-bundle/apps/scratch-wii/boot.dol"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/meta.xml" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii-bundle/apps/scratch-wii/meta.xml"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/gfx/wii/icon.png" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii-bundle/apps/scratch-wii/icon.png"
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii.zip" --format=zip -- "${CMAKE_CURRENT_BINARY_DIR}/scratch-wii-bundle/apps"
		COMMENT "Packaging..."
	)
endmacro()
