if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl3.cmake")
target_link_libraries(renderer_interface INTERFACE SDL3::SDL3)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl3_ttf.cmake")
target_link_libraries(renderer_interface INTERFACE SDL3_ttf::SDL3_ttf)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl3_gfx.cmake")
target_link_libraries(renderer_interface INTERFACE SDL3_gfx::SDL3_gfx)

set(SE_WINDOWING_VALID_OPTIONS "sdl3")
set(SE_AUDIO_ENGINE_DEFAULT "sdl3")
