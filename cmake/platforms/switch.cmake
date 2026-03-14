macro(package_platform)
	nx_generate_nacp(OUTPUT scratch-nx.nacp NAME "Scratch Everywhere!" AUTHOR "NateXS and Grady Link" VERSION ${CMAKE_PROJECT_VERSION})
	nx_create_nro(scratch-everywhere OUTPUT scratch-nx.nro ROMFS ${CMAKE_CURRENT_SOURCE_DIR}/romfs ICON "${CMAKE_SOURCE_DIR}/gfx/wiiu/switch-icon.jpg" NACP scratch-nx.nacp)
endmacro()
