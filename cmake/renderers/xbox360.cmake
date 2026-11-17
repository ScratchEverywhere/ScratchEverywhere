# Xbox 360 (xsdk) native hardware renderer
cl_add_dep(renderer_interface stb_truetype)

list(APPEND SE_WINDOWING_VALID_OPTIONS "xbox360")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
    set(SE_AUDIO_ENGINE_DEFAULT "xbox360")
endif()
