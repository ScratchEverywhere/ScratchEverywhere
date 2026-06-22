if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

target_link_libraries(windowing_interface INTERFACE citro2d citro3d)
