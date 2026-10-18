function(_recipe_ScratchEverywhere_source)
	set(SE_REF "1.2.1")
	if(CL_VERSION_REQ)
		set(SE_REF "${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME ScratchEverywhere
		REPO https://github.com/ScratchEverywhere/ScratchEverywhere.git
		REF ${SE_REF}
		OPTIONS "SE_LIBRARY ON"
	)

	add_library(deps::ScratchEverywhere ALIAS scratch-everywhere)
endfunction()
