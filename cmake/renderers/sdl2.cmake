
if(VITA)
    cl_add_dep(renderer_interface SDL2 OPTIONS
        "CMAKE_POLICY_DEFAULT_CMP0077" "NEW"
        "CMAKE_POLICY_DEFAULT_CMP0069" "NEW"
        "VIDEO_VITA_PIB" "ON"
    )
elseif(PS3)
    cl_add_dep(renderer_interface SDL2)
else()
    cl_add_dep(renderer_interface SDL2)
endif()
cl_add_dep(renderer_interface stb_truetype)

list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl2")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
endif()
