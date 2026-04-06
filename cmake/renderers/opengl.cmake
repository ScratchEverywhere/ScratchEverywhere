if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

find_package(OpenGL REQUIRED)
target_link_libraries(renderer_interface INTERFACE OpenGL::GL)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(renderer_interface stb_truetype)

set(SE_WINDOWING_VALID_OPTIONS "sdl2" "sdl1" "sdl3" "glfw")
set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
