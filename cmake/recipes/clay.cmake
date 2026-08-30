function(_recipe_clay_source)
	cl_import_source(
		NAME clay
		DOWNLOAD_ONLY
		REPO https://github.com/nicbarker/clay.git
		REF e6cc369 # We're in between clay versions
	)

	if(NOT EXISTS "${CL_SOURCE_DIR}/clay.h")
		_catalog_log(FATAL_ERROR "clay: clay.h not found under ${CL_SOURCE_DIR}")
	endif()

	add_library(clay INTERFACE)
	target_include_directories(clay INTERFACE $<BUILD_INTERFACE:${CL_SOURCE_DIR}>)
endfunction()
