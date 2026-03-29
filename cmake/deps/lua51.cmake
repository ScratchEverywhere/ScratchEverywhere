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

function(_dep_system_lua51)
	if(SE_LUA_BACKEND STREQUAL "fallback" OR SE_LUA_BACKEND STREQUAL "luajit")
		pkg_check_modules(luajit IMPORTED_TARGET luajit>=2.0.0)
		if(TARGET PkgConfig::luajit)
			add_library(deps::lua51 ALIAS PkgConfig::luajit)
			set(SE_USED_LUA "luajit" PPARENT_SCOPE)
			return()
		endif()
	endif()
	if(SE_LUA_BACKEND STREQUAL "fallback" OR SE_LUA_BACKEND STREQUAL "lua51")
		pkg_check_modules(lua51 IMPORTED_TARGET lua51)
		if(TARGET PkgConfig::lua51)
			set(SE_USED_LUA "lua51" PARENT_SCOPE)
		endif()
	endif()
endfunction()

function(_dep_source_lua51)
	if(SE_LUA_BACKEND STREQUAL "luajit")
		message(FATAL_ERROR "We don't support building LuaJIT from source yet.")
		set(SE_USED_LUA "luajit")
	else()
		CPMAddPackage(
		  NAME lua51
		  GITHUB_REPOSITORY lua/lua
		  VERSION 5.1.1
		  DOWNLOAD_ONLY YES
		)

		file(GLOB lua_sources ${lua51_SOURCE_DIR}/*.c)
		list(REMOVE_ITEM lua_sources "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c") # We only need the lib
		add_library(lua51 STATIC ${lua_sources})
		target_include_directories(lua51 SYSTEM PUBLIC $<BUILD_INTERFACE:${lua51_SOURCE_DIR}>)

		set(SE_USED_LUA "lua51" PARENT_SCOPE)
	endif()
endfunction()
