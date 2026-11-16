# Xbox (nxdk) NV2A direct hardware renderer
cl_add_dep(renderer_interface stb_truetype)

list(APPEND SE_WINDOWING_VALID_OPTIONS "xbox")

if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
    set(SE_AUDIO_ENGINE_DEFAULT "xbox")
endif()

if(XBOX)
    target_link_libraries(renderer_interface INTERFACE ${NXDK_DIR}/lib/libpbkit.lib)
endif()
