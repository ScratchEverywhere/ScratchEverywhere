if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(audio_interface SDL2)
