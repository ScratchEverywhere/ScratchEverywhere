if("fallback" IN_LIST SE_LUA_BACKEND_VALID_OPTIONS)
	set(SE_LUA_BACKEND "fallback" CACHE STRING "The Lua backend to use for custom extensions.")
elseif("luajit" IN_LIST SE_LUA_BACKEND_VALID_OPTIONS)
	set(SE_LUA_BACKEND "luajit" CACHE STRING "The Lua backend to use for custom extensions.")
else()
	set(SE_LUA_BACKEND "lua51" CACHE STRING "The Lua backend to use for custom extensions.")
endif()
if(NOT SE_LUA_BACKEND IN_LIST SE_LUA_BACKEND_VALID_OPTIONS)
	message(
		FATAL_ERROR
		"Invalid value for SE_LUA_BACKEND: '${SE_LUA_BACKEND}'. Must be one of: ${SE_LUA_BACKEND_VALID_OPTIONS}"
	)
endif()

function(_recipe_lua51_system)
	if(SE_LUA_BACKEND STREQUAL "fallback" OR SE_LUA_BACKEND STREQUAL "luajit")
		find_package(PkgConfig QUIET)
		if(PkgConfig_FOUND)
			pkg_check_modules(luajit IMPORTED_TARGET luajit>=2.0.0)
		endif()
		if(TARGET PkgConfig::luajit)
			add_library(_lua51_system INTERFACE)
			target_link_libraries(_lua51_system INTERFACE PkgConfig::luajit)
			if(VITA) # Needed by the vita's libdl, idk why these aren't in the pkg-config stuff
				target_link_libraries(_lua51_system INTERFACE SceSblSsMgr_stub taihen_stub)
			endif()
			add_library(deps::lua51 ALIAS _lua51_system)

			set(SE_USED_LUA "luajit" CACHE INTERNAL "Resolved Lua backend used by lua51/sol2")
			return()
		endif()
	endif()
	if(SE_LUA_BACKEND STREQUAL "fallback" OR SE_LUA_BACKEND STREQUAL "lua51")
		find_package(PkgConfig QUIET)
		if(PkgConfig_FOUND)
			pkg_check_modules(lua51 IMPORTED_TARGET lua51)
		endif()
		if(TARGET PkgConfig::lua51)
			add_library(deps::lua51 ALIAS PkgConfig::lua51)
			set(SE_USED_LUA "lua51" CACHE INTERNAL "Resolved Lua backend used by lua51/sol2")
		endif()
	endif()
endfunction()

function(_recipe_lua51_source)
	if(SE_LUA_BACKEND STREQUAL "luajit")
		message(FATAL_ERROR "We don't support building LuaJIT from source yet.")
	else()
		cl_import_source(
			NAME lua51
			DOWNLOAD_ONLY
			URL "https://www.lua.org/ftp/lua-5.1.5.tar.gz"
		)

		file(GLOB lua_sources "${CL_SOURCE_DIR}/src/*.c")
		list(REMOVE_ITEM lua_sources "${CL_SOURCE_DIR}/src/lua.c" "${CL_SOURCE_DIR}/src/luac.c") # We only need the lib
		add_library(lua51 STATIC ${lua_sources})
		target_include_directories(lua51 SYSTEM PUBLIC $<BUILD_INTERFACE:${CL_SOURCE_DIR}/src>)

		set(SE_USED_LUA "lua51" CACHE INTERNAL "Resolved Lua backend used by lua51/sol2")
	endif()
endfunction()
