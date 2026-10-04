if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

target_link_libraries(renderer_interface INTERFACE gdi32)

set(SE_WINDOWING_VALID_OPTIONS "win32")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "headless")
endif()
