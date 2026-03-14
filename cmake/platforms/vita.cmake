macro(package_platform)
	include("${VITASDK}/share/vita.cmake" REQUIRED)

	if(EXISTS ${CMAKE_SOURCE_DIR}/romfs/project.sb3)
		set(PROJ_FILE FILE ${CMAKE_SOURCE_DIR}/romfs/project.sb3 project.sb3)
	else()
		set(PROJ_FILE "")
	endif()

	set(VITA_GFXFILES)
	foreach(file IN LISTS GFXFILES)
		file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")
		set(DEST_PATH "gfx/${REL_PATH}")
		list(APPEND VITA_GFXFILES FILE "${file}" "${DEST_PATH}")
	endforeach()

	vita_create_self(scratch-vita.self scratch-everywhere)
	vita_create_vpk(scratch-vita.vpk "NTXS00053" scratch-vita.self
		VERSION "01.00"
		NAME "Scratch Everywhere!"
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/icon0.png sce_sys/icon0.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
		FILE ${CMAKE_SOURCE_DIR}/gfx/vita/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
		${VITA_GFXFILES}
		${PROJ_FILE}
	)
endmacro()
