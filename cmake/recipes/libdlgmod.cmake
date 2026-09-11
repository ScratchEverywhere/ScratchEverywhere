# Always built from source (no system/package equivalent) - matches the old
# SE_FORCE_SOURCE_libdlgmod behavior.
function(_recipe_libdlgmod_source)
	set(LIBDLGMOD_REF "main")
	if(CL_REQ_VERSION)
		set(LIBDLGMOD_REF "${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME libdlgmod
		DOWNLOAD_ONLY
		REPO https://github.com/samuelvenable/libdlgmod.git
		REF ${LIBDLGMOD_REF}
	)

	set(LIBDLGMOD_DIR "${CL_SOURCE_DIR}/libdlgmod")
	if(NOT EXISTS "${LIBDLGMOD_DIR}")
		_catalog_log(FATAL_ERROR "libdlgmod: expected sources not found under ${LIBDLGMOD_DIR}")
	endif()

	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		add_library(libdlgmod STATIC "${LIBDLGMOD_DIR}/win32/libdlgmod.cpp" "${LIBDLGMOD_DIR}/general/apiprocess/process.cpp" "${LIBDLGMOD_DIR}/general/xprocess.cpp")
		target_compile_definitions(libdlgmod PUBLIC PROCESS_GUIWINDOW_IMPL NULLIFY_STDERR)
		set_target_properties(libdlgmod PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE TRUE)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		add_library(libdlgmod STATIC "${LIBDLGMOD_DIR}/macos/libdlgmod.mm")
		target_compile_definitions(libdlgmod PUBLIC PROCESS_GUIWINDOW_IMPL NULLIFY_STDERR)
		set_target_properties(libdlgmod PROPERTIES OBJCXX_STANDARD 17 OBJCXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE TRUE)
	elseif(CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD|DragonFly|NetBSD|OpenBSD|SunOS)$" AND NOT ANDROID AND NOT WEBOS)
		add_library(libdlgmod STATIC "${LIBDLGMOD_DIR}/xlib/libdlgmod.cpp" "${LIBDLGMOD_DIR}/general/apiprocess/process.cpp" "${LIBDLGMOD_DIR}/general/xprocess.cpp" "${LIBDLGMOD_DIR}/general/lodepng.cpp")
		target_compile_definitions(libdlgmod PUBLIC PROCESS_GUIWINDOW_IMPL NULLIFY_STDERR)
		set_target_properties(libdlgmod PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE TRUE)
	else()
		return()
	endif()

	target_include_directories(libdlgmod PUBLIC $<BUILD_INTERFACE:${LIBDLGMOD_DIR}/general> $<BUILD_INTERFACE:${CL_SOURCE_DIR}>)

	# Believe it or not all these if-platform checks are actually necessary; do not ask me why; it makes just as little sense to me as you.
	# I tried having USE_SDL_POLLEVENT, USE_SDL2_POLLEVENT, and USE_SDL3_POLLEVENT defined on platforms besides these and everything broke.
	if(DEFINED SE_WINDOWING AND (CMAKE_SYSTEM_NAME MATCHES "^(Linux|FreeBSD|DragonFly|NetBSD|OpenBSD|SunOS)$") AND NOT ANDROID AND NOT WEBOS)
		if(SE_WINDOWING STREQUAL "sdl1")
			cl_add_dep(libdlgmod SDL)
			target_compile_definitions(libdlgmod PUBLIC USE_SDL_POLLEVENT)
		elseif(SE_WINDOWING STREQUAL "sdl2")
			cl_add_dep(libdlgmod SDL2)
			target_compile_definitions(libdlgmod PUBLIC USE_SDL2_POLLEVENT)
		elseif(SE_WINDOWING STREQUAL "sdl3")
			cl_add_dep(libdlgmod SDL3)
			target_compile_definitions(libdlgmod PUBLIC USE_SDL3_POLLEVENT)
		endif()
	endif()

	# linker
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		target_link_libraries(libdlgmod PUBLIC ntdll gdiplus comctl32 shlwapi comdlg32 ole32 oleaut32 uuid)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
		target_link_libraries(libdlgmod PUBLIC "-framework AppKit" "-framework UniformTypeIdentifiers")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		target_link_libraries(libdlgmod PUBLIC X11 pthread)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
		target_include_directories(libdlgmod PUBLIC "/usr/local/include")
		target_link_directories(libdlgmod PUBLIC "/usr/local/lib")
		target_link_libraries(libdlgmod PUBLIC X11 kvm pthread)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
		target_include_directories(libdlgmod PUBLIC "/usr/X11R7/include")
		target_link_directories(libdlgmod PUBLIC "/usr/X11R7/lib")
		target_link_libraries(libdlgmod PUBLIC X11 kvm pthread)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
		target_include_directories(libdlgmod PUBLIC "/usr/X11R6/include")
		target_link_directories(libdlgmod PUBLIC "/usr/X11R6/lib")
		target_link_libraries(libdlgmod PUBLIC X11 kvm pthread)
	elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
		target_link_libraries(libdlgmod PUBLIC X11 kvm proc pthread)
	endif()

	set_target_properties(libdlgmod PROPERTIES PREFIX "")
	add_library(deps::libdlgmod ALIAS libdlgmod)
endfunction()
