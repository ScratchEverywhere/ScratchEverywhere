
if(NINTENDO_SWITCH)
	find_path(SWITCH_GLAD_INCLUDE_DIR glad/glad.h PATHS "$ENV{DEVKITPRO}/portlibs/switch/include" "/opt/devkitpro/portlibs/switch/include")
	find_library(SWITCH_GLAD_LIBRARY glad PATHS "$ENV{DEVKITPRO}/portlibs/switch/lib" "/opt/devkitpro/portlibs/switch/lib")
	if(NOT SWITCH_GLAD_INCLUDE_DIR OR NOT SWITCH_GLAD_LIBRARY)
		message(FATAL_ERROR "switch-glad not found. Install it with: (dkp-)pacman -S switch-glad")
	endif()
	target_include_directories(renderer_interface INTERFACE "${SWITCH_GLAD_INCLUDE_DIR}")

	set(_SE_SWITCH_MESA_DIR "")
	if(DEFINED SE_SWITCH_MESA_DIR AND NOT SE_SWITCH_MESA_DIR STREQUAL "")
		set(_SE_SWITCH_MESA_DIR "${SE_SWITCH_MESA_DIR}")
	elseif(DEFINED SWITCH_MESA_DIR AND NOT SWITCH_MESA_DIR STREQUAL "")
		set(_SE_SWITCH_MESA_DIR "${SWITCH_MESA_DIR}")
	elseif(DEFINED ENV{SE_SWITCH_MESA_DIR} AND NOT "$ENV{SE_SWITCH_MESA_DIR}" STREQUAL "")
		set(_SE_SWITCH_MESA_DIR "$ENV{SE_SWITCH_MESA_DIR}")
	elseif(DEFINED ENV{SWITCH_MESA_DIR} AND NOT "$ENV{SWITCH_MESA_DIR}" STREQUAL "")
		set(_SE_SWITCH_MESA_DIR "$ENV{SWITCH_MESA_DIR}")
	endif()

	set(_SE_VENDORED_VULKAN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/vulkan/switch")

	if(NOT _SE_SWITCH_MESA_DIR STREQUAL "")
		if(NOT EXISTS "${_SE_SWITCH_MESA_DIR}/include/vulkan/vulkan.h" AND NOT EXISTS "${_SE_SWITCH_MESA_DIR}/include/GL/gl.h")
			message(FATAL_ERROR "mesa-switch sdk not found at ${_SE_SWITCH_MESA_DIR} (expected include/ and lib/).")
		endif()
		target_include_directories(renderer_interface SYSTEM BEFORE INTERFACE "${_SE_SWITCH_MESA_DIR}/include")
		target_link_directories(renderer_interface INTERFACE "${_SE_SWITCH_MESA_DIR}/lib")
		
		target_link_libraries(renderer_interface INTERFACE
			"${SWITCH_GLAD_LIBRARY}"
			EGL GLESv2 glapi
			vulkan expat zstd
		)
		
		target_link_options(renderer_interface INTERFACE "LINKER:--allow-multiple-definition")
		target_compile_definitions(renderer_interface INTERFACE SE_SWITCH_MESA SE_VULKAN)
	elseif(EXISTS "${_SE_VENDORED_VULKAN_DIR}/lib/libvulkan.a" AND EXISTS "${_SE_VENDORED_VULKAN_DIR}/include/vulkan/vulkan.h")
		target_include_directories(renderer_interface SYSTEM BEFORE INTERFACE "${_SE_VENDORED_VULKAN_DIR}/include")
		target_link_directories(renderer_interface INTERFACE "${_SE_VENDORED_VULKAN_DIR}/lib")
		target_link_libraries(renderer_interface INTERFACE
			"${SWITCH_GLAD_LIBRARY}" EGL glapi drm_nouveau
			vulkan expat zstd
		)
		target_link_options(renderer_interface INTERFACE "LINKER:--allow-multiple-definition")
		target_compile_definitions(renderer_interface INTERFACE SE_VULKAN SE_VENDORED_VULKAN)
	else()
		target_link_libraries(renderer_interface INTERFACE "${SWITCH_GLAD_LIBRARY}" EGL glapi drm_nouveau)
	endif()
elseif(PS4)
	target_include_directories(renderer_interface INTERFACE "${PS4_HOMEBREW_SDK}/target/usr/include")
	target_link_libraries(renderer_interface INTERFACE EGL GLESv2)
elseif(EMSCRIPTEN)
	target_link_options(renderer_interface INTERFACE "-sMIN_WEBGL_VERSION=2" "-sMAX_WEBGL_VERSION=2" "-sFULL_ES3=1")
else()
	cl_add_dep(renderer_interface GLAD)
endif()

cl_add_dep(renderer_interface stb_truetype)

if(NINTENDO_SWITCH OR PS4 OR EMSCRIPTEN)
	set(SE_WINDOWING_VALID_OPTIONS "sdl2")

	if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
		set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
	endif()
elseif(NOT LIBRETRO)
	list(APPEND SE_WINDOWING_VALID_OPTIONS "sdl2" "sdl1" "sdl3" "glfw")

	if(WIN32)
		list(APPEND SE_WINDOWING_VALID_OPTIONS "win32")
		target_link_libraries(renderer_interface INTERFACE opengl32)
	endif()

	if(NOT DEFINED SE_AUDIO_ENGINE_DEFAULT)
		set(SE_AUDIO_ENGINE_DEFAULT "sdl2")
	endif()
endif()
