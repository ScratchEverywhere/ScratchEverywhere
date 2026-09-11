if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

# SDL3 is only used for audio here (not windowing/rendering), so trim every
# other subsystem out of a from-source build to keep it lean.
cl_add_dep(audio_interface SDL3
	SOURCE_OPTIONS
		"SDL_RENDER" "OFF" "SDL_VIDEO" "OFF" "SDL_COCOA" "OFF" "SDL_JOYSTICK" "OFF" "SDL_HAPTIC" "OFF"
		"SDL_VIVANTE" "OFF" "SDL_OFFSCREEN" "OFF" "SDL_HIDAPI" "OFF" "SDL_UNIX_CONSOLE_BUILD" "ON"
)
