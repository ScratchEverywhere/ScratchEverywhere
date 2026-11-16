if(NOT TARGET nxaudio)
    include(FetchContent)
    set(NXAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        nxdk_audio
        GIT_REPOSITORY https://github.com/Ryzee119/nxdk-audio.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(nxdk_audio)
endif()

target_link_libraries(audio_interface INTERFACE nxaudio)
if(DEFINED nxdk_audio_SOURCE_DIR)
    target_include_directories(audio_interface INTERFACE ${nxdk_audio_SOURCE_DIR}/include)
endif()
