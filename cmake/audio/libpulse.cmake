if(TARGET audio_interface)
    return()
endif()
add_library(audio_interface INTERFACE)

cl_add_dep(audio_interface libpulse PREFER_SHARED)
cl_add_dep(audio_interface libpulse-simple PREFER_SHARED)
