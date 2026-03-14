if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl2.cmake")
target_link_libraries(windowing_interface INTERFACE SDL2::SDL2)
