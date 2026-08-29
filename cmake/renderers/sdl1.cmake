if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

cl_add_dep(renderer_interface SDL)
cl_add_dep(renderer_interface SDL_ttf)
cl_add_dep(renderer_interface SDL_gfx)

set(SE_WINDOWING_VALID_OPTIONS "sdl1")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl1")
endif()
