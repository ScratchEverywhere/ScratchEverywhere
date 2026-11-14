set(SE_DEFAULT_OUTPUT_NAME "scratch-vflash")

enable_language(ASM)
set(VFLASH_OPT_LEVEL "-Os" CACHE STRING "Compiler optimization level (-O1, -O2, -O3, -Os)")
find_package(VFlash REQUIRED)

set(SE_RENDERER_VALID_OPTIONS "vflash")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "vflash")
set(SE_DEPS_VALID_OPTIONS "fallback" "system")

set(SE_RENDERER "vflash" CACHE STRING "Select the desired renderer backend." FORCE)
set(SE_WINDOWING "sdl2" CACHE STRING "Select the desired windowing backend." FORCE)
set(SE_AUDIO_ENGINE "vflash" CACHE STRING "Select the desired audio backend." FORCE)

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

macro(vflash_create_executable)
    set(_vflash_block_files
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/ScratchEverywhere.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/control.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/data.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/events.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/logs.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/looks.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/motion.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/operators.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/pen.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/procedures.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/sensing.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blocks/sound.cpp"
    )
    # Keep block handlers in an OBJECT library so static SCRATCH_BLOCK registrars are
    # never dropped by the linker, while keeping the rest of core in a STATIC archive
    # so unused features are dead-stripped under the 4 MiB ELF RAM budget.
    add_library(scratch-everywhere-blocks OBJECT ${_vflash_block_files})
    target_link_libraries(scratch-everywhere-blocks PRIVATE se-interface SDL2)

    add_library(scratch-everywhere-core STATIC ${SOURCE_FILES})
    target_include_directories(se-interface INTERFACE ${VFLASH_INCLUDE_DIRS})
    target_compile_definitions(se-interface INTERFACE STBI_NO_THREAD_LOCALS)
    target_link_libraries(scratch-everywhere-core PRIVATE se-interface SDL2)
    target_compile_options(se-interface INTERFACE
        -Oz
        -mcpu=arm926ej-s -marm -mfloat-abi=soft -mlittle-endian -mabi=aapcs -fno-short-enums
        -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
        -Wno-psabi
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
    )

    vflash_add_program(scratch-everywhere firmware-disc.ld
        "${CMAKE_CURRENT_SOURCE_DIR}/source/platforms/vflash/entry.c"
    )
    if(VFLASH_ENABLE_MMU)
        target_sources(scratch-everywhere PRIVATE
            "${VFLASH_SDK_DIR}/src/platform/mmu.c"
            "${VFLASH_SDK_DIR}/src/platform/mmu_arm926.S"
        )
        target_compile_definitions(scratch-everywhere PRIVATE VFLASH_ENABLE_MMU=1)
        target_compile_definitions(se-interface INTERFACE VFLASH_ENABLE_MMU=1)
    endif()

    target_link_libraries(se-interface INTERFACE stdc++_nano m c_nano gcc)
    target_link_options(scratch-everywhere PRIVATE
        -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc
        -Wl,--wrap=_malloc_r -Wl,--wrap=_free_r -Wl,--wrap=_calloc_r -Wl,--wrap=_realloc_r
    )
    target_link_libraries(scratch-everywhere PRIVATE
        scratch-everywhere-blocks scratch-everywhere-core se-interface SDL2
        -Wl,--start-group stdc++_nano m c_nano gcc -Wl,--end-group
    )
endmacro()

macro(package_platform)
    if(TARGET plutovg)
        target_compile_options(plutovg PRIVATE -Os -fno-unwind-tables -fno-asynchronous-unwind-tables)
        target_link_libraries(plutovg INTERFACE -Wl,--start-group stdc++_nano m c_nano gcc -Wl,--end-group)
    endif()
    if(TARGET lunasvg)
        target_compile_options(lunasvg PRIVATE -Os -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables)
        target_link_libraries(lunasvg INTERFACE -Wl,--start-group stdc++_nano m c_nano gcc -Wl,--end-group)
    endif()
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
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb2")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb2" "${_vflash_assets_dir}/project.sb3" COPYONLY)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb" "${_vflash_assets_dir}/project.sb3" COPYONLY)
    endif()

    vflash_add_disc_with_assets(vflash_disc_image scratch-everywhere vflash "${_vflash_assets_dir}")
endmacro()
