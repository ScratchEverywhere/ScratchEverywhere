

if(VFLASH)
    target_link_libraries(windowing_interface INTERFACE SDL2)
else()
    cl_add_dep(windowing_interface SDL2)
endif()

if(WIN32)
	if(TARGET SDL2::SDL2main)
		target_link_libraries(windowing_interface INTERFACE SDL2::SDL2main)
	else()
		target_link_libraries(windowing_interface INTERFACE SDL2main)
	endif()
endif()
