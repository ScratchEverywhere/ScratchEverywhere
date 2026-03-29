function(_dep_system_libcurl)
	if(SE_CLOUDVARS AND SE_FORCE_CLOUDVARS_SOURCE_CURL)
		return()
	endif()

	if(NINTENDO_WIIU)
		add_library(libcurl INTERFACE)
		target_link_libraries(libcurl INTERFACE curl mbedtls mbedx509 mbedcrypto z wut m)
		target_include_directories(libcurl INTERFACE "${DEVKITPRO}/portlibs/wiiu/include")
	endif()

	if(SE_CLOUDVARS)
		find_package(CURL QUIET OPTIONAL_COMPONENTS WSS)

		if((NOT CURL_FOUND OR NOT CURL_WSS_FOUND) AND SE_PKGCONF_CURL AND PkgConfig_FOUND)
			pkg_check_modules(libcurl IMPORTED_TARGET GLOBAL libcurl>=8.4.0)
		else()
			add_library(deps::libcurl ALIAS CURL::libcurl)
		endif()
	elseif(PkgConfig_FOUND)
		pkg_check_modules(libcurl IMPORTED_TARGET GLOBAL libcurl>=8.4.0)
	endif()
endfunction()

function(_dep_source_libcurl)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	set(CURL_OPTIONS
		"BUILD_CURL_EXE OFF"
		"BUILD_TESTING OFF"
		"BUILD_EXAMPLES OFF"
		"ENABLE_MANUAL OFF"
		"CURL_DISABLE_INSTALL ON"
		"ENABLE_ARES OFF"
		"USE_LIBIDN2 OFF"
		"CURL_DISABLE_WEBSOCKETS OFF"
		"CURL_DISABLE_LDAP ON"
		"CURL_USE_LIBSSH2 OFF"
		"CURL_USE_LIBPSL OFF"
		"CURL_USE_OPENSSL OFF"
		"CURL_DISABLE_NTLM ON"
	)
	if(WEBOS)
		list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS OFF")
	else()
		list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS ON")
	endif()
	if(WIN32 OR WEBOS)
		list(APPEND CURL_OPTIONS "CURL_ENABLE_SSL OFF")
	endif()
	if(NINTENDO_WIIU)
		list(APPEND CURL_OPTIONS "ENABLE_THREADED_RESOLVER OFF" "ENABLE_IPV6 OFF" "ENABLE_UNIX_SOCKETS OFF" "CURL_DISABLE_SOCKETPAIR ON")
	endif()

	CPMAddPackage(
		NAME curl
		GIT_REPOSITORY "https://github.com/curl/curl.git"
		GIT_TAG "curl-8_15_0"
		VERSION 8.15.0
		PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/libcurl.patch"
		OPTIONS ${CURL_OPTIONS}
	)

	get_target_property(IS_ALIAS CURL::libcurl ALIASED_TARGET)
	if(IS_ALIAS)
		add_library(deps::libcurl ALIAS ${IS_ALIAS})
	else()
		add_library(deps::libcurl ALIAS CURL::libcurl)
	endif()
endfunction()
