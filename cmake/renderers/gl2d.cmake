if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

set(SE_WINDOWING_VALID_OPTIONS "nds")
set(SE_AUDIO_ENGINE_DEFAULT "nds")
