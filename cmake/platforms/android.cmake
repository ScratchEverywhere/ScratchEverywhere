set(SE_DEFAULT_OUTPUT_NAME "scratch") # libscratch.so

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "source" "fallback" "system")

set(SE_CACHING_DEFAULT ON)
set(SE_CMAKERC_DEFAULT OFF)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)
set(SE_FORCE_CLOUDVARS_SOURCE_CURL ON)
set(SE_FORCE_SOURCE_SDL2 ON)

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD TRUE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_PLATFORM_DEFINITIONS "__ANDROID__")
set(SE_PLATFORM "android")

macro(package_platform)
    # armeabi-v7a requires cpufeatures library
    include(AndroidNdkModules)
    android_ndk_import_module_cpufeatures()

    set(SDL2_JAVA_SOURCE app/src/main/java/org/libsdl/app/)
    file(COPY ${SDL2_SOURCE_DIR}/android-project/${SDL2_JAVA_SOURCE}
        DESTINATION ${PROJECT_ROOT}/android/${SDL2_JAVA_SOURCE})
endmacro()