if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(windowing_interface SDL)
