if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

set(SE_SVG OFF)
set(SE_BITMAP OFF)

set(SE_WINDOWING_VALID_OPTIONS "headless")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "headless")
endif()
