if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

cl_add_dep(windowing_interface SDL2)

if(WIN32 OR IOS)
	if(TARGET SDL2::SDL2main)
		target_link_libraries(windowing_interface INTERFACE SDL2::SDL2main)
	else()
		target_link_libraries(windowing_interface INTERFACE SDL2main)
	endif()
endif()
