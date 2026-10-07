if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

# SDL2 is only used for audio here (not windowing/rendering), so trim every
# other subsystem out of a from-source build to keep it lean.
cl_add_dep(audio_interface SDL2
	SOURCE_OPTIONS
		"SDL_COCOA" "OFF" "SDL_RENDER_METAL" "OFF" "SDL_OPENGL" "OFF" "SDL_JOYSTICK" "OFF"
		"SDL_HAPTIC" "OFF" "SDL_RPI" "OFF" "SDL_X11" "OFF" "SDL_WAYLAND" "OFF" "SDL_DIRECTX" "OFF"
		"SDL_KMSDRM" "OFF" "SDL_VULKAN" "OFF" "SDL_VIVANTE" "OFF" "SDL_RENDER_D3D" "OFF"
		"SDL_OFFSCREEN" "OFF" "SDL_HIDAPI" "OFF" "SDL_UNIX_CONSOLE_BUILD" "ON"
)
