if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl3.cmake")
target_link_libraries(audio_interface INTERFACE SDL3::SDL3)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/sdl3_mixer.cmake")
target_link_libraries(audio_interface INTERFACE SDL3_mixer::SDL3_mixer)
