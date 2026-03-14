if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl2.cmake")
target_link_libraries(audio_interface INTERFACE SDL2::SDL2)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl2_mixer.cmake")
target_link_libraries(audio_interface INTERFACE SDL2_mixer::SDL2_mixer)
