function(_dep_system_plutovg)
	find_package(plutovg 1.3.2 CONFIG QUIET) # 1.3.2 fixes an issue with the CMake config setup
endfunction()

function(_dep_source_plutovg)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME plutovg
		GITHUB_REPOSITORY sammycage/plutovg
		VERSION 1.3.2
		OPTIONS "PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD ON" "PLUTOVG_BUILD_EXAMPLES OFF"
	)
	# Not all platforms need these but there's no harm in adding them since we don't need a multithreaded plutovg
	target_compile_definitions(plutovg PRIVATE __STDC_NO_THREADS__ __STDC_NO_ATOMICS__)
	target_compile_options(plutovg PRIVATE -Wno-error=incompatible-pointer-types)
endfunction()
