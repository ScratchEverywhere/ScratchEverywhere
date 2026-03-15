if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

option(SE_OPENGL_ES "Use OpenGL ES instead of OpenGL" OFF)

if(SE_OPENGL_ES)
	find_package(OpenGL REQUIRED COMPONENTS GLES2)
	target_link_libraries(renderer_interface INTERFACE OpenGL::GLES2)
	target_compile_definitions(renderer_interface INTERFACE USE_GLES)
else()
	find_package(OpenGL REQUIRED)
	target_link_libraries(renderer_interface INTERFACE OpenGL::GL)
endif()

set(SE_WINDOWING_VALID_OPTIONS "sdl2" "sdl1" "sdl3" "glfw")
set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
