if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

cl_add_dep(renderer_interface SDL2)
cl_add_dep(renderer_interface SDL2_ttf)

set(SE_WINDOWING_VALID_OPTIONS "sdl2")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
endif()
