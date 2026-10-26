#
# CMake platform file for PS3 PPU processor
#

cmake_minimum_required(VERSION 3.6)

if(DEFINED ENV{PS3DEV})
    set(PS3DEV $ENV{PS3DEV})
elseif(NOT DEFINED PS3DEV)
    set(PS3DEV "/usr/local/ps3dev")
endif()

if(DEFINED ENV{PSL1GHT})
    set(PSL1GHT $ENV{PSL1GHT})
elseif(NOT DEFINED PSL1GHT)
    set(PSL1GHT "${PS3DEV}")
endif()

set(CMAKE_PREFIX_PATH
    "${PS3DEV}/ppu"
    "${PS3DEV}/ppu/lib/cmake"
    "${PS3DEV}/portlibs/ppu"
    "${PS3DEV}/portlibs/ppu/lib/cmake"
    ${CMAKE_PREFIX_PATH}
)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR powerpc64)

set(CMAKE_C_COMPILER "${PS3DEV}/ppu/bin/ppu-gcc")
set(CMAKE_CXX_COMPILER "${PS3DEV}/ppu/bin/ppu-g++")

set(PPU_CFLAGS "-mhard-float -fmodulo-sched -ffunction-sections -fdata-sections -I${PSL1GHT}/ppu/include -I${PSL1GHT}/portlibs/ppu/include -I${PSL1GHT}/portlibs/ppu/include/SDL2 -I${PSL1GHT}/ppu/include/simdmath -D__PS3__")
set(PPU_CXXFLAGS "-D_GLIBCXX11_USE_C99_STDIO=1 -D_GLIBCXX11_USE_C99_STDLIB=1 ${PPU_CFLAGS}")
set(PPU_LDFLAGS "-mhard-float -fmodulo-sched -Wl,--gc-sections -L${PSL1GHT}/ppu/lib -L${PS3DEV}/portlibs/ppu/lib -lm -lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2")

set(CMAKE_INSTALL_PREFIX ${PS3DEV}/portlibs/ppu)

set(CMAKE_FIND_ROOT_PATH ${PS3DEV} ${PS3DEV}/ppu ${PS3DEV}/portlibs/ppu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT ${PPU_CFLAGS})
set(CMAKE_CXX_FLAGS_INIT ${PPU_CXXFLAGS})
set(CMAKE_EXE_LINKER_FLAGS_INIT ${PPU_LDFLAGS})
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(PS3 TRUE)
set(PLATFORM_PS3 TRUE)
set(PPU TRUE)