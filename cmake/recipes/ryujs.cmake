function(_recipe_ryuJS_source)
	set(RYUJS_TAG "v3.0")
	if(CL_REQ_VERSION)
		set(RYUJS_TAG "v${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME ryujs
		DOWNLOAD_ONLY
		URL "https://github.com/poipole807/ryuJS/archive/refs/tags/${RYUJS_TAG}.tar.gz"
	)

	add_library(ryuJS STATIC
		"${CL_SOURCE_DIR}/ryu/d2s.c"
		"${CL_SOURCE_DIR}/ryu/d2s.h"
	)
	target_include_directories(ryuJS PUBLIC $<BUILD_INTERFACE:${CL_SOURCE_DIR}>)
	add_library(deps::ryuJS ALIAS ryuJS)
endfunction()
