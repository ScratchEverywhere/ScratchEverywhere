set(SE_DEFAULT_OUTPUT_NAME "scratch-everywhere")

set(SE_RENDERER_VALID_OPTIONS "sdl1" "sdl2" "sdl3" "opengl")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl1" "sdl2" "sdl3")
set(SE_DEPS_VALID_OPTIONS "source" "fallback" "system")

set(SE_CACHING_DEFAULT ON)
set(SE_CMAKERC_DEFAULT ON)

set(SE_ALLOW_CMAKERC ON)
set(SE_ALLOW_CLOUDVARS ON)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_PLATFORM_DEFINITIONS "__PC__")
set(SE_PLATFORM "pc")

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH TRUE)
set(SE_HAS_MOUSE TRUE)
set(SE_HAS_KEYBOARD TRUE)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND (CMAKE_CROSSCOMPILING OR CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.11)) # joystick doesn't work on MacOS with current Dockerfile
	set(SE_HAS_CONTROLLER FALSE)
else()
	set(SE_HAS_CONTROLLER TRUE)
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT CMAKE_CROSSCOMPILING)
	set(SE_ASAN ON)
endif()

if(NOT BUILD_SHARED_LIBS AND MINGW)
	set(SE_STATIC_LIBGCC TRUE)
	set(SE_STATIC_LIBSTDCPP TRUE)
endif()

include(CheckSymbolExists)
check_symbol_exists(dlopen "dlfcn.h" SE_ALLOW_NATIVE_EXTENSIONS)

add_library(threads_interface INTERFACE)
find_package(Threads REQUIRED)
target_link_libraries(threads_interface INTERFACE Threads::Threads)
