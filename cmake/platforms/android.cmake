set(SE_DEFAULT_OUTPUT_NAME "scratch") # libscratch.so
set(SE_LIBRARY ON)

set(SE_RENDERER_VALID_OPTIONS "opengl_core" "sdl2" "sdl3")
set(SE_WINDOWING_ENGINE_VALID_OPTIONS "sdl2" "sdl3")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2" "sdl3")
set(SE_DEPS_VALID_OPTIONS "source" "fallback" "system")
set(SE_LUA_BACKEND_VALID_OPTIONS "fallback" "lua51")

set(SE_CACHING_DEFAULT ON)
set(SE_CMAKERC_DEFAULT OFF)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)
set(SE_DECTALK ON)

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD TRUE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM_DEFINITIONS "__ANDROID__")
set(SE_PLATFORM "android")

macro(package_platform)
    # Link against logging library
    target_link_libraries(se-interface INTERFACE log)

    # armeabi-v7a requires cpufeatures library
    include(AndroidNdkModules)
    android_ndk_import_module_cpufeatures()

    set(SDL2_JAVA_SOURCE app/src/main/java/org/libsdl/app/)
    if(DEFINED SDL2_SOURCE_DIR)
        file(COPY ${SDL2_SOURCE_DIR}/android-project/${SDL2_JAVA_SOURCE}
            DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/android/${SDL2_JAVA_SOURCE})
    else()
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/")
            message(
                FATAL_ERROR
                "The SDL java glue was not found, please add it at android/${SDL2_JAVA_SOURCE}"
            )
        endif()
    endif()
endmacro()