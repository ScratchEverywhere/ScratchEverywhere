function(_se_finalize_sol2 SOL2_UPSTREAM_TARGET)
	add_library(_sol2_target INTERFACE IMPORTED)
	target_link_libraries(_sol2_target INTERFACE ${SOL2_UPSTREAM_TARGET} deps::lua51)
	target_compile_definitions(_sol2_target INTERFACE SOL_ALL_SAFETIES_ON=1 SOL_LUA_VERSION=501)
	if(NINTENDO_WIIU)
		target_compile_definitions(_sol2_target INTERFACE SOL_NO_THREAD_LOCAL=1)
	endif()
	if(SE_USED_LUA STREQUAL "luajit")
		target_compile_definitions(_sol2_target INTERFACE SOL_USE_LUAJIT=1)
	endif()
	add_library(deps::sol2 ALIAS _sol2_target)
endfunction()

function(_recipe_sol2_system)
	find_package(sol2 CONFIG QUIET)
	if(TARGET sol2::sol2)
		_se_finalize_sol2(sol2::sol2)
	endif()
endfunction()

function(_recipe_sol2_source)
	set(SOL2_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

	cl_import_source(
		NAME sol2
		DOWNLOAD_ONLY
		REPO https://github.com/ThePhD/sol2.git
		REF develop # Fixes sol::optional error
	)

	add_subdirectory("${CL_SOURCE_DIR}" "${CL_SOURCE_DIR}-build")

	if(TARGET sol2)
		_se_finalize_sol2(sol2)
	endif()
endfunction()
