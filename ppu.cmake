#
# CMake platform file for PS3 PPU processor
#
# Based on the PS2 EE CMake File by:
# Copyright (C) 2009-2010 Mathias Lafeldt <misfire@debugon.org>
# Copyright (C) 2023 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Copyright (C) 2024 André Guilherme <andregui17@outlook.com>
# Copyright (C) 2024-Present PS2DEV Team
#

cmake_minimum_required(VERSION 3.6)

INCLUDE(CMakeForceCompiler)

if(DEFINED ENV{PS3DEV})
    SET(PS3DEV $ENV{PS3DEV})
else()
    message(FATAL_ERROR "The environment variable PS3DEV needs needs to be defined.")
endif()

if(DEFINED ENV{PSL1GHT})
    SET(PSL1GHT $ENV{PSL1GHT})
else()
    message(FATAL_ERROR "The environment variable PSL1GHT needs to be defined.")
endif()

set(CMAKE_PREFIX_PATH
    "$ENV{PS3DEV}/ppu"
    "$ENV{PS3DEV}/ppu/lib/cmake"
    "$ENV{PS3DEV}/portlibs/ppu"
    "$ENV{PS3DEV}/portlibs/ppu/lib/cmake"
    ${CMAKE_PREFIX_PATH}
)


SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_SYSTEM_PROCESSOR powerpc64)

SET(CMAKE_C_COMPILER ppu-gcc)
SET(CMAKE_CXX_COMPILER ppu-g++)

SET(PPU_CFLAGS "-mhard-float -fmodulo-sched -ffunction-sections -fdata-sections -I$ENV{PSL1GHT}/ppu/include -I$ENV{PSL1GHT}/portlibs/ppu/include -I$ENV{PSL1GHT}/portlibs/ppu/include/SDL2 -I$ENV{PSL1GHT}/ppu/include/simdmath -D__PS3__")
SET(PPU_CXXFLAGS "-D_GLIBCXX11_USE_C99_STDIO ${PPU_CFLAGS}")
SET(PPU_LDFLAGS "-mhard-float -fmodulo-sched -Wl,--gc-sections -L$ENV{PSL1GHT}/ppu/lib -L/usr/local/ps3dev/portlibs/ppu/lib -lm -lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2 -lio -laudio")

SET(CMAKE_INSTALL_PREFIX $ENV{PS3DEV}/portlibs/ppu)

SET(CMAKE_FIND_ROOT_PATH $ENV{PS3DEV} $ENV{PS3DEV}/ppu $ENV{PS3DEV}/portlibs/ppu)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)

SET(CMAKE_C_FLAGS_INIT ${PPU_CFLAGS})
SET(CMAKE_CXX_FLAGS_INIT ${PPU_CXXFLAGS})
SET(CMAKE_EXE_LINKER_FLAGS_INIT ${PPU_LDFLAGS})
SET(CMAKE_EXECUTABLE_SUFFIX "elf")
SET(CMAKE_EXECUTABLE_SUFFIX_ASM .elf)
SET(CMAKE_EXECUTABLE_SUFFIX_C .elf)
SET(CMAKE_EXECUTABLE_SUFFIX_CXX .elf)


SET(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "-nostartfiles -Wl,-r -Wl,-d")

SET($ENV{PKG_CONFIG_DIR} "")
SET($ENV{PKG_CONFIG_PATH} "")
SET($ENV{PKG_CONFIG_SYSROOT_DIR} "")
SET($ENV{PKG_CONFIG_LIBDIR} "$ENV{PS3DEV}/portlibs/ppu/lib/pkgconfig")

SET(PS3 TRUE)
SET(PLATFORM_PS3 TRUE)
SET(PPU TRUE)