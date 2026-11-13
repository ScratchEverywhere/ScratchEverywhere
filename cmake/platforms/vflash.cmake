set(SE_DEFAULT_OUTPUT_NAME "scratch-vflash")

enable_language(ASM)
set(VFLASH_OPT_LEVEL "-Os" CACHE STRING "Compiler optimization level (-O1, -O2, -O3, -Os)")
find_package(VFlash REQUIRED)

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_RENDERER "sdl2" CACHE STRING "Select the desired renderer backend." FORCE)
set(SE_WINDOWING "sdl2" CACHE STRING "Select the desired windowing backend." FORCE)
set(SE_AUDIO_ENGINE "sdl2" CACHE STRING "Select the desired audio backend." FORCE)

set(SE_CACHING_DEFAULT OFF)
set(SE_BUNDLE_DEFAULT OFF)

set(SE_ALLOW_BUNDLE OFF)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD OFF)
set(SE_ALLOW_DISCORD_RPC OFF)
set(SE_ALLOW_NATIVE_EXTENSIONS OFF)

set(SE_HAS_THREADS OFF)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_DECTALK_DEFAULT OFF)
set(SE_MENU OFF CACHE BOOL "Enables the SE! Main Menu." FORCE)
set(SE_LOADSCREEN OFF CACHE BOOL "Enables SE!'s load screen." FORCE)

set(SE_PLATFORM_DEFINITIONS "__VFLASH__" "VFLASH")
set(SE_PLATFORM "vflash")

option(VFLASH_ENABLE_MMU "Enable ARMv5 MMU identity map at startup" OFF)
set(SE_VFLASH_PROJECT "" CACHE PATH "Path to unpacked Scratch project directory or .sb3 file to stage on the V.Flash ISO9660 disc")

macro(package_platform)
    set(_vflash_assets_dir "${CMAKE_BINARY_DIR}/vflash_assets")
    file(MAKE_DIRECTORY "${_vflash_assets_dir}")

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/romfs/gfx")
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/romfs/gfx" DESTINATION "${_vflash_assets_dir}")
    endif()

    if(SE_VFLASH_PROJECT AND EXISTS "${SE_VFLASH_PROJECT}")
        if(IS_DIRECTORY "${SE_VFLASH_PROJECT}")
            file(MAKE_DIRECTORY "${_vflash_assets_dir}/project")
            file(COPY "${SE_VFLASH_PROJECT}/" DESTINATION "${_vflash_assets_dir}/project")
        else()
            configure_file("${SE_VFLASH_PROJECT}" "${_vflash_assets_dir}/project.sb3" COPYONLY)
        endif()
    elseif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/romfs/project")
        file(MAKE_DIRECTORY "${_vflash_assets_dir}/project")
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/romfs/project/" DESTINATION "${_vflash_assets_dir}/project")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb3")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb3" "${_vflash_assets_dir}/project.sb3" COPYONLY)
    endif()

    vflash_add_disc_with_assets(vflash_disc_image scratch-everywhere vflash "${_vflash_assets_dir}")
endmacro()
