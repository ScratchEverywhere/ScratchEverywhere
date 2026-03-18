if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(renderer_interface SDL)
se_add_dependency(renderer_interface SDL_ttf)
se_add_dependency(renderer_interface SDL_gfx)

set(SE_WINDOWING_VALID_OPTIONS "sdl1")
set(SE_AUDIO_ENGINE_DEFAULT "sdl1")
