function(_recipe_plutovg_system)
	if(CL_REQ_VERSION)
		find_package(plutovg ${CL_REQ_VERSION} CONFIG QUIET)
	else()
		find_package(plutovg 1.3.2 CONFIG QUIET) # 1.3.2 fixed an issue with the CMake config setup
	endif()
endfunction()

function(_recipe_plutovg_source)
	set(PLUTOVG_TAG "v1.3.3")
	if(CL_REQ_VERSION)
		set(PLUTOVG_TAG "v${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME plutovg
		URL https://github.com/sammycage/plutovg/archive/refs/tags/${PLUTOVG_TAG}.tar.gz
		OPTIONS "PLUTOVG_DISABLE_FONT_FACE_CACHE_LOAD" "ON" "PLUTOVG_BUILD_EXAMPLES" "OFF"
	)

	# Not all platforms need these but there's no harm in adding them since we don't need a multithreaded plutovg
	if(TARGET plutovg)
		target_compile_definitions(plutovg PRIVATE __STDC_NO_THREADS__ __STDC_NO_ATOMICS__)
		target_compile_options(plutovg PRIVATE -Wno-error=incompatible-pointer-types)
	endif()
endfunction()
