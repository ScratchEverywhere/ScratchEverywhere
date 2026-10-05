if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

target_link_libraries(renderer_interface INTERFACE winmm)
