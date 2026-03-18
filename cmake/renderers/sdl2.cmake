if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(renderer_interface SDL2)
se_add_dependency(renderer_interface SDL2_ttf)
se_add_dependency(renderer_interface SDL2_gfx)

set(SE_WINDOWING_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
