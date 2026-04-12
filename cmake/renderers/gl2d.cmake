if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(renderer_interface stb_truetype)

set(SE_WINDOWING_VALID_OPTIONS "nds")
set(SE_AUDIO_ENGINE_DEFAULT "nds")
