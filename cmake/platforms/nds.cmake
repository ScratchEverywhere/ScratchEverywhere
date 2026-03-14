set(SE_CACHING_DEFAULT OFF)
set(SE_HAS_THREADS OFF)

macro(package_platform)
	nds_create_rom(scratch-everywhere
		NAME "Scratch Everywhere!"
		SUBTITLE "Scratch 3 Games on your DS!"
		AUTHOR "NateXS"
		ICON "${CMAKE_CURRENT_SOURCE_DIR}/gfx/nds/icon.bmp"
		NITROFS "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
	)
endmacro()
