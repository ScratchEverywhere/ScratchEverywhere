if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

cl_add_dep(windowing_interface GLFW)
