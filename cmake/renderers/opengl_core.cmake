if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

# find_package(OpenGL REQUIRED)
# target_link_libraries(renderer_interface INTERFACE OpenGL::GL)

cl_add_dep(renderer_interface GLAD)
cl_add_dep(renderer_interface stb_truetype)

if(NOT LIBRETRO)
	set(SE_WINDOWING_VALID_OPTIONS "sdl2" "sdl1" "sdl3" "glfw" "win32")

	if(WIN32)
		target_link_libraries(renderer_interface INTERFACE opengl32)
	endif()

	if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
		if(WIN32)
			set(SE_AUDIO_ENGINE_DEFAULT "winmm")
		else()
			set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
		endif()
	endif()
endif()
