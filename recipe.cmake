function(_recipe_ScratchEverywhere_source)
	set(SE_TAG "1.2")
	if(CL_REQ_VERSION)
		set(SE_TAG "${CL_REQ_VERSION}")
	endif()


	cl_import_source(
		NAME ScratchEverywhere
		URL https://github.com/ScratchEverywhere/ScratchEverywhere/archive/refs/tags/${SE_TAG}.tar.gz
		OPTIONS "SE_LIBRARY ON"
	)

	add_library(deps::ScratchEverywhere ALIAS scratch-everywhere)
endfunction()
