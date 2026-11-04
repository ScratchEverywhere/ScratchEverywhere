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
	if(XBOX)
		return()
	endif()
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
	endif()

	set(_lua_version "5.1.5")
	set(_lua_sha256 "2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333")
	set(_lua_archive "${CMAKE_BINARY_DIR}/_deps_cache/lua-${_lua_version}.tar.gz")
	set(_lua_extract "${CMAKE_BINARY_DIR}/_deps_cache/lua-${_lua_version}")

	# Needed since lua.org is unstable
	set(_lua_mirrors
		${SE_LUA51_URL}
		"https://www.lua.org/ftp/lua-${_lua_version}.tar.gz"
		"https://sources.voidlinux.org/lua51-${_lua_version}/lua-${_lua_version}.tar.gz"
		"https://fossies.org/linux/misc/lua-${_lua_version}.tar.gz"
	)

	if(NOT EXISTS "${_lua_extract}/lua-${_lua_version}/src/lapi.c")
		file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps_cache")

		set(_lua_ok FALSE)
		if(EXISTS "${_lua_archive}")
			file(SHA256 "${_lua_archive}" _existing_hash)
			if(_existing_hash STREQUAL _lua_sha256)
				set(_lua_ok TRUE)
			endif()
		endif()

		if(NOT _lua_ok)
			foreach(_attempt RANGE 1 2)
				foreach(_url IN LISTS _lua_mirrors)
					if(NOT _url)
						continue()
					endif()
					message(STATUS "[lua51] Downloading ${_url} (attempt ${_attempt})")
					file(DOWNLOAD "${_url}" "${_lua_archive}"
						EXPECTED_HASH SHA256=${_lua_sha256}
						TIMEOUT 60
						INACTIVITY_TIMEOUT 20
						TLS_VERIFY ON
						STATUS _dl_status
					)
					list(GET _dl_status 0 _dl_code)
					if(_dl_code EQUAL 0)
						set(_lua_ok TRUE)
						break()
					endif()
					list(GET _dl_status 1 _dl_msg)
					message(WARNING "[lua51] Mirror failed: ${_url}: ${_dl_msg}")
					file(REMOVE "${_lua_archive}")
				endforeach()
				if(_lua_ok)
					break()
				endif()
			endforeach()
		endif()

		if(NOT _lua_ok)
			message(FATAL_ERROR
				"[lua51] Could not download Lua ${_lua_version} from any mirror. "
				"Install the system package (liblua5.1-0-dev / lua51 pkg-config) "
				"or pass -DSE_LUA51_URL=<url or local path to lua-${_lua_version}.tar.gz>.")
		endif()

		file(REMOVE_RECURSE "${_lua_extract}")
		file(ARCHIVE_EXTRACT INPUT "${_lua_archive}" DESTINATION "${_lua_extract}")
	endif()

	set(_lua_src_dir "${_lua_extract}/lua-${_lua_version}")

	file(GLOB lua_sources "${_lua_src_dir}/src/*.c")
	list(REMOVE_ITEM lua_sources "${_lua_src_dir}/src/lua.c" "${_lua_src_dir}/src/luac.c") # We only need the lib
	add_library(lua51 STATIC ${lua_sources})
	target_include_directories(lua51 SYSTEM PUBLIC
		$<BUILD_INTERFACE:${_lua_src_dir}/src>
		$<BUILD_INTERFACE:${_lua_src_dir}/etc>
	)

	if(XBOX)
		target_compile_definitions(lua51 PUBLIC LUA_ANSI)
	endif()

	set(SE_USED_LUA "lua51" CACHE INTERNAL "Resolved Lua backend used by lua51/sol2")
endfunction()
