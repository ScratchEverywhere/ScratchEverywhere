function(_recipe_nxaudio_source)
    if(TARGET nxaudio)
        return()
    endif()
    include(FetchContent)
    set(NXAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        nxdk_audio
        GIT_REPOSITORY https://github.com/Ryzee119/nxdk-audio.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(nxdk_audio)
    if(NOT TARGET deps::nxaudio AND TARGET nxaudio)
        add_library(deps::nxaudio ALIAS nxaudio)
    endif()
endfunction()
