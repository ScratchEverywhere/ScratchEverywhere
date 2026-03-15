if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl2.cmake")
target_link_libraries(renderer_interface INTERFACE SDL2::SDL2)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl2_ttf.cmake")
target_link_libraries(renderer_interface INTERFACE SDL2_ttf::SDL2_ttf)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl2_gfx.cmake")
target_link_libraries(renderer_interface INTERFACE SDL2_gfx::SDL2_gfx)

set(SE_WINDOWING_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
