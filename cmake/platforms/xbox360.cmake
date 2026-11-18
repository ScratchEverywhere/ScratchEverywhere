# Xbox 360 (xsdk) native platform
set(SE_DEFAULT_OUTPUT_NAME "scratch-xbox360")
set(SE_PLATFORM "xbox360")
set(SE_PLATFORM_DEFINITIONS "__XBOX360__")
add_compile_definitions(__XBOX360__)

if(NOT XSDK OR NOT COMMAND xsdk_add_xex)
    message(FATAL_ERROR "Xbox 360 requires the xsdk CMake toolchain and runtime")
endif()

# Use optimized builds by default while allowing explicit debug configurations.
if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build configuration" FORCE)
endif()

set(SE_RENDERER "xbox360" CACHE STRING "Renderer backend")
set(SE_WINDOWING "xbox360" CACHE STRING "Window backend")
set(SE_RENDERER_VALID_OPTIONS "xbox360")
set(SE_WINDOWING_VALID_OPTIONS "xbox360")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "xbox360")
set(SE_AUDIO_ENGINE_DEFAULT "xbox360")
set(SE_ZIP_BACKEND_VALID_OPTIONS "miniz")

set(SE_DECTALK_DEFAULT ON)
set(SE_CACHING_DEFAULT ON)
set(SE_BUNDLE_DEFAULT OFF)
set(SE_ALLOW_BUNDLE OFF)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_HAS_TOUCH OFF)
set(SE_HAS_MOUSE OFF)
set(SE_HAS_KEYBOARD OFF)
set(SE_HAS_CONTROLLER ON)
set(SE_HAS_THREADS ON)

enable_language(ASM)

set(SE_MENU ON CACHE BOOL "Build application menus")
set(SE_SVG ON CACHE BOOL "Decode SVG application assets")
set(SE_AUDIO ON CACHE BOOL "Enable Xbox 360 native audio")
set(SE_JIT ON CACHE BOOL "Compile hot bytecode to machine code with SLJIT.")

macro(package_platform)
    target_compile_options(se-interface INTERFACE
        -ffunction-sections
        -fdata-sections
        -mcheck-fpr-alignment
    )
    target_link_options(scratch-everywhere PRIVATE -Wl,--gc-sections)

    xsdk_add_xex(scratch-everywhere NAME "Scratch Everywhere")
    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rename
            "$<TARGET_FILE_DIR:scratch-everywhere>/scratch-everywhere.xex"
            "$<TARGET_FILE_DIR:scratch-everywhere>/${SE_OUTPUT_NAME}.xex"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${XSDK_ROOT}/LICENSE"
            "$<TARGET_FILE_DIR:scratch-everywhere>/xsdk-LICENSE.txt"
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${XSDK_ROOT}/THIRD_PARTY_NOTICES.md"
            "$<TARGET_FILE_DIR:scratch-everywhere>/xsdk-THIRD_PARTY_NOTICES.md"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/romfs"
            "$<TARGET_FILE_DIR:scratch-everywhere>/romfs"
        COMMENT "Packaging Xbox 360 executable and resources"
        VERBATIM
    )
endmacro()
