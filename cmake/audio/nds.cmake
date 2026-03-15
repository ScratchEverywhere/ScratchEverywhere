if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/maxmod.cmake")
target_link_libraries(audio_interface INTERFACE maxmod)
