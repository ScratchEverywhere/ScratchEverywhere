include("${CMAKE_SOURCE_DIR}/cmake/platforms/pc.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

set(SE_RENDERER_VALID_OPTIONS "opengl")
set(SE_WINDOWING_VALID_OPTIONS "libretro")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "libretro")

set(SE_WINDOWING "libretro")
set(SE_AUDIO_ENGINE "libretro")
set(SE_AUDIO_ENGINE_DEFAULT "libretro")
set(SE_PLATFORM "libretro")

set(SE_CMAKERC_DEFAULT ON)
set(SE_ALLOW_CMAKERC ON)

list(APPEND SE_PLATFORM_DEFINITIONS "LIBRETRO")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
