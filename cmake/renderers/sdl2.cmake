if(TARGET renderer_interface)
    return()
endif()
add_library(renderer_interface INTERFACE)

cl_add_dep(renderer_interface SDL2)
cl_add_dep(renderer_interface SDL2_ttf)

if(PSP AND TARGET PkgConfig::SDL2_ttf)
	get_target_property(_SDL2_TTF_LINK_OPTS PkgConfig::SDL2_ttf INTERFACE_LINK_OPTIONS)
	if(_SDL2_TTF_LINK_OPTS)
		list(REMOVE_ITEM _SDL2_TTF_LINK_OPTS "-pthread")
		set_target_properties(PkgConfig::SDL2_ttf PROPERTIES INTERFACE_LINK_OPTIONS "${_SDL2_TTF_LINK_OPTS}")
	endif()
endif()

set(SE_WINDOWING_VALID_OPTIONS "sdl2")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
	set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
endif()
