
cl_add_dep(renderer_interface SDL)
cl_add_dep(renderer_interface SDL_gfx)
cl_add_dep(renderer_interface stb_truetype)

list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl1")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl1")
endif()
