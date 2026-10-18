function(_recipe_ScratchEverywhere_source)
	set(SE_REF "1.2.1")
	if(CL_REQ_VERSION)
		set(SE_REF "${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME ScratchEverywhere
		URL https://github.com/ScratchEverywhere/ScratchEverywhere.git
		REF ${SE_REF}
		OPTIONS "SE_LIBRARY ON"
	)

	add_library(deps::ScratchEverywhere ALIAS scratch-everywhere)
endfunction()
