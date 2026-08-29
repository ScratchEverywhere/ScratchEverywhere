if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

cl_add_dep(renderer_interface SDL3)
cl_add_dep(renderer_interface SDL3_ttf)

set(SE_WINDOWING_VALID_OPTIONS "sdl3")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl3")
endif()
