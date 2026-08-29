function(_recipe_libcurl_toolchain)
	if(DEFINED NINTENDO_WIIU AND DEFINED DEVKITPRO)
		add_library(libcurl INTERFACE)
		target_link_libraries(libcurl INTERFACE curl mbedtls mbedx509 mbedcrypto z wut m)
		target_include_directories(libcurl INTERFACE "${DEVKITPRO}/portlibs/wiiu/include")
	endif()
endfunction()

function(_recipe_libcurl_system)
	if(NOT CMAKE_CROSSCOMPILING)
		if(SE_CLOUDVARS)
			find_package(CURL QUIET OPTIONAL_COMPONENTS WSS)
			if(TARGET CURL::libcurl AND (NOT SE_CLOUDVARS OR CURL_WSS_FOUND))
				get_target_property(IS_ALIAS CURL::libcurl ALIASED_TARGET)
				if(IS_ALIAS)
					add_library(deps::libcurl ALIAS ${IS_ALIAS})
				else()
					add_library(deps::libcurl ALIAS CURL::libcurl)
				endif()
				return()
			endif()
		else()
			find_package(CURL QUIET)
			if(TARGET CURL::libcurl)
				get_target_property(IS_ALIAS CURL::libcurl ALIASED_TARGET)
				if(IS_ALIAS)
					add_library(deps::libcurl ALIAS ${IS_ALIAS})
				else()
					add_library(deps::libcurl ALIAS CURL::libcurl)
				endif()
				return()
			endif()
		endif()
	endif()

	find_package(PkgConfig QUIET)
	if(NOT PkgConfig_FOUND)
		return()
	endif()

	if(SE_CLOUDVARS AND NOT CL_PACKAGE_MANAGER STREQUAL "pacman")
		return()
	endif()

	cl_format_pkgconfig_req("libcurl" "${CL_VERSION_REQ}" PKG_SPEC)
	if(CL_STATIC)
		pkg_check_modules(libcurl IMPORTED_TARGET GLOBAL "--static" ${PKG_SPEC})
	else()
		pkg_check_modules(libcurl IMPORTED_TARGET GLOBAL ${PKG_SPEC})
	endif()
endfunction()

function(_recipe_libcurl_package)
	if(CL_REQUIRE_STATIC) # Most package managers don't provide static libs.
		return()
	endif()

	if(SE_CLOUDVARS AND NOT CL_PACKAGE_MANAGER STREQUAL "pacman")
		return()
	endif()

	if(CL_PACKAGE_MANAGER STREQUAL "apt")
		set(CL_PACKAGE_NAME "libcurl4-openssl-dev" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "pacman")
		set(CL_PACKAGE_NAME "curl" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "brew")
		set(CL_PACKAGE_NAME "curl" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "yum")
		set(CL_PACKAGE_NAME "libcurl-devel" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "apk")
		set(CL_PACKAGE_NAME "curl-dev" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "zypper")
		set(CL_PACKAGE_NAME "libcurl-devel" PARENT_SCOPE)
	endif()
endfunction()

function(_recipe_libcurl_source)
	set(CURL_OPTIONS
		"BUILD_CURL_EXE" "OFF"
		"BUILD_TESTING" "OFF"
		"BUILD_EXAMPLES" "OFF"
		"ENABLE_MANUAL" "OFF"
		"CURL_DISABLE_INSTALL" "ON"
		"ENABLE_ARES" "OFF"
		"USE_LIBIDN2" "OFF"
		"CURL_DISABLE_WEBSOCKETS" "OFF"
		"CURL_DISABLE_LDAP" "ON"
		"CURL_DISABLE_NTLM" "ON"
	)
	if(CMAKE_CROSSCOMPILING AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Windows"))
		list(APPEND CURL_OPTIONS
			"CURL_USE_LIBSSH2" "OFF"
			"CURL_USE_LIBPSL" "OFF"
			"CURL_USE_OPENSSL" "OFF"
		)
	endif()
	if(WEBOS)
		list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS" "OFF")
	elseif(CMAKE_CROSSCOMPILING AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Windows"))
		list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS" "ON")
	endif()
	if(WIN32 OR WEBOS)
		list(APPEND CURL_OPTIONS "CURL_ENABLE_SSL" "OFF")
	endif()
	if(NINTENDO_WIIU)
		list(APPEND CURL_OPTIONS "ENABLE_THREADED_RESOLVER" "OFF" "ENABLE_IPV6" "OFF" "ENABLE_UNIX_SOCKETS" "OFF" "CURL_DISABLE_SOCKETPAIR" "ON")
	endif()

	set(CURL_TAG "curl-8_15_0")
	if(CL_REQ_VERSION)
		string(REPLACE "." "_" CURL_TAG_VER "${CL_REQ_VERSION}")
		set(CURL_TAG "curl-${CURL_TAG_VER}")
	endif()

	cl_import_source(
		NAME libcurl
		URL "https://github.com/curl/curl/archive/refs/tags/${CURL_TAG}.tar.gz"
		PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/libcurl.patch"
		OPTIONS ${CURL_OPTIONS}
	)

	if(TARGET CURL::libcurl)
		get_target_property(IS_ALIAS CURL::libcurl ALIASED_TARGET)
		if(IS_ALIAS)
			add_library(deps::libcurl ALIAS ${IS_ALIAS})
		else()
			add_library(deps::libcurl ALIAS CURL::libcurl)
		endif()
	endif()
endfunction()
