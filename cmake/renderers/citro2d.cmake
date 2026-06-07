if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

target_link_libraries(renderer_interface INTERFACE citro2d citro3d)

set(SE_WINDOWING_VALID_OPTIONS "3ds")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl3")
endif()
