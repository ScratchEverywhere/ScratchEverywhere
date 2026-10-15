if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

if(VITA)
    cl_add_dep(renderer_interface SDL3 OPTIONS
        "CMAKE_POLICY_DEFAULT_CMP0077" "NEW"
        "CMAKE_POLICY_DEFAULT_CMP0069" "NEW"
        "VIDEO_VITA_PIB" "ON"
    )
else()
    cl_add_dep(renderer_interface SDL3)
endif()
cl_add_dep(renderer_interface stb_truetype)

set(SE_WINDOWING_VALID_OPTIONS "sdl3")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl3")
endif()
