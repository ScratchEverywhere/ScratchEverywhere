function(_dep_system_lua51)
	pkg_check_modules(lua51 IMPORTED_TARGET lua51)
endfunction()

function(_dep_source_lua51)
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
endfunction()
