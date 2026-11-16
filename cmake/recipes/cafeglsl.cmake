function(_cafeglsl_expose)
	if(NOT TARGET cafeglsl::cafeglsl)
		return()
	endif()

	add_library(cafeglsl INTERFACE)
	target_link_libraries(cafeglsl INTERFACE cafeglsl::cafeglsl)
	add_library(deps::cafeglsl ALIAS cafeglsl)
endfunction()

function(_recipe_cafeglsl_system)
	if(NOT NINTENDO_WIIU)
		return()
	endif()

	find_package(cafeglsl CONFIG QUIET GLOBAL)
	_cafeglsl_expose()
endfunction()

function(_recipe_cafeglsl_prebuilt)
	if(NOT NINTENDO_WIIU)
		return()
	endif()

	set(CAFEGLSL_VERSION "1.0.0-rc1")
	if(CAFEGLSL_RELEASE)
		set(CAFEGLSL_VERSION "${CAFEGLSL_RELEASE}")
	elseif(CL_REQ_VERSION)
		if(NOT CL_REQ_VERSION MATCHES "^[0-9]+(\\.[0-9]+)*$")
			_catalog_log(FATAL_ERROR "cafeglsl: cannot use version '${CL_REQ_VERSION}'. Pass a plain release number such as 1.1.0, or set CAFEGLSL_RELEASE=<release name> for a pre-release.")
		endif()
		set(CAFEGLSL_VERSION "${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME cafeglsl
		DOWNLOAD_ONLY
		URL "https://github.com/Exzap/CafeGLSL/releases/download/v${CAFEGLSL_VERSION}/cafeglsl-${CAFEGLSL_VERSION}-wiiu-ppc.tar.xz"
	)

	find_package(cafeglsl CONFIG QUIET GLOBAL NO_DEFAULT_PATH PATHS "${CL_SOURCE_DIR}")
	_cafeglsl_expose()
endfunction()
