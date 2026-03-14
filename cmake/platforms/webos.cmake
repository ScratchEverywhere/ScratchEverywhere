macro(package_platform)
	# toolchain path: ~/arm-webos-linux-gnueabi_sdk-buildroot/share/buildroot/toolchainfile.cmake
	target_link_options(scratch-everywhere PRIVATE "-static-libstdc++" "-static-libgcc")
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON)
	install(TARGETS scratch-everywhere RUNTIME DESTINATION .)
    install(FILES ${CMAKE_SOURCE_DIR}/gfx/webos/appinfo.json ${CMAKE_SOURCE_DIR}/gfx/webos/icon.png ${CMAKE_SOURCE_DIR}/gfx/webos/splash.png DESTINATION .)

    if(EXISTS ${CMAKE_SOURCE_DIR}/romfs/project.sb3)
        install(FILES "${CMAKE_SOURCE_DIR}/romfs/project.sb3" DESTINATION .)
    endif()

    foreach(file IN LISTS GFXFILES)
        file(RELATIVE_PATH REL_PATH "${CMAKE_SOURCE_DIR}/gfx" "${file}")
        get_filename_component(DEST_DIR "${REL_PATH}" DIRECTORY)
        install(FILES "${file}" DESTINATION "gfx/${DEST_DIR}")
    endforeach()

    set(CPACK_GENERATOR "External")
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT "${CMAKE_SOURCE_DIR}/AresPackage.cmake")
    set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
    set(CPACK_MONOLITHIC_INSTALL TRUE)

    include(CPack)
endmacro()
