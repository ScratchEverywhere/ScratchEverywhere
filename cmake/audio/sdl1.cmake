if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl.cmake")
target_link_libraries(audio_interface INTERFACE SDL::SDL)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl_mixer.cmake")
target_link_libraries(audio_interface INTERFACE SDL_mixer::SDL_mixer)
