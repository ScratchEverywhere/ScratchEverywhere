include("${CMAKE_SOURCE_DIR}/cmake/platforms/pc.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

set(SE_RENDERER_VALID_OPTIONS "opengl")
set(SE_WINDOWING_VALID_OPTIONS "retroarch")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "retroarch")

set(SE_WINDOWING "retroarch")
set(SE_AUDIO_ENGINE "retroarch")
set(SE_PLATFORM "retraorch")

list(APPEND SE_PLATFORM_DEFINITIONS "RETROARCH")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
