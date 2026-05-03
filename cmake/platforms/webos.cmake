set(SE_DEFAULT_OUTPUT_NAME "scratch-webos")

set(SE_RENDERER_VALID_OPTIONS "sdl2")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl2")
set(SE_DEPS_VALID_OPTIONS "fallback")

set(SE_CACHING_DEFAULT ON)

set(SE_ALLOW_CMAKERC OFF)
set(SE_ALLOW_CLOUDVARS OFF)
set(SE_ALLOW_DOWNLOAD ON)

set(SE_PLATFORM_DEFINITIONS "WEBOS")
set(SE_PLATFORM "pc") # i dont *think* webOS would need a different implementation?

set(SE_HAS_THREADS ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE TRUE)
set(SE_HAS_KEYBOARD TRUE)
set(SE_HAS_CONTROLLER TRUE)

macro(package_platform)
	# toolchain path: ~/arm-webos-linux-gnueabi_sdk-buildroot/share/buildroot/toolchainfile.cmake
	target_link_options(scratch-everywhere PRIVATE "-static-libstdc++" "-static-libgcc")
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
	install(TARGETS scratch-everywhere RUNTIME DESTINATION .)

	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gfx/webos/appinfo.json.in" "${CMAKE_CURRENT_BINARY_DIR}/appinfo.json" @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/appinfo.json ${CMAKE_CURRENT_SOURCE_DIR}/gfx/webos/icon.png ${CMAKE_CURRENT_SOURCE_DIR}/gfx/webos/splash.png DESTINATION .)

	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb3)
		install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/romfs/project.sb3" DESTINATION .)
    endif()

    foreach(file IN LISTS GFXFILES)
		file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/gfx" "${file}")
        get_filename_component(DEST_DIR "${REL_PATH}" DIRECTORY)
        install(FILES "${file}" DESTINATION "gfx/${DEST_DIR}")
    endforeach()

    set(CPACK_GENERATOR "External")
	set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpack/AresPackage.cmake")
    set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
    set(CPACK_MONOLITHIC_INSTALL TRUE)

    include(CPack)
endmacro()
