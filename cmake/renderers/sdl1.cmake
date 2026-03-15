if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl.cmake")
target_link_libraries(renderer_interface INTERFACE SDL::SDL)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl_ttf.cmake")
target_link_libraries(renderer_interface INTERFACE SDL_ttf::SDL_ttf)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl_gfx.cmake")
target_link_libraries(renderer_interface INTERFACE SDL_gfx::SDL_gfx)

set(SE_WINDOWING_VALID_OPTIONS "sdl1")
set(SE_AUDIO_ENGINE_DEFAULT "sdl1")
