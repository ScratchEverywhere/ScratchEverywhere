# Xbox (nxdk) renderer: SDL3 on top of the nxdk XGU/nv2a render backend.
# Kept separate from the generic sdl3 renderer so the console-specific tiling,
# pixel formats and memory handling don't leak into the baseline SDL3 code.
cl_add_dep(renderer_interface SDL3)
cl_add_dep(renderer_interface stb_truetype)

list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl3")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
    set(SE_AUDIO_ENGINE_DEFAULT "sdl3")
endif()
