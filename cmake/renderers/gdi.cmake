
target_link_libraries(renderer_interface INTERFACE gdi32)

list(APPEND SE_WINDOWING_VALID_OPTIONS "win32")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "winmm")
endif()
