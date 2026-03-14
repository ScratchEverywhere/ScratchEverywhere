set(SE_CACHING_DEFAULT OFF)

macro(package_platform)
    create_pbp_file(
        TARGET scratch-everywhere
        ICON_PATH ${CMAKE_SOURCE_DIR}/gfx/psp/ICON0.png
        BACKGROUND_PATH NULL
        PREVIEW_PATH ${CMAKE_SOURCE_DIR}/gfx/psp/PIC1.png
        TITLE scratch-everywhere
        VERSION ${CMAKE_PROJECT_VERSION}
    )
	# package the EBOOT.pbp
	add_custom_command(TARGET scratch-everywhere POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/scratch-psp-bundle"
	    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_BINARY_DIR}/EBOOT.PBP" "${CMAKE_BINARY_DIR}/scratch-psp-bundle/EBOOT.PBP"
		COMMAND ${CMAKE_COMMAND} -E tar "cfv" "${CMAKE_BINARY_DIR}/scratch-psp.zip" --format=zip -- "${CMAKE_BINARY_DIR}/scratch-psp-bundle"
	)
endmacro()
