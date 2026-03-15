if(TARGET CURL::libcurl)
	return()
endif()

if(NINTENDO_WIIU)
	add_library(libcurl INTERFACE)
	add_library(CURL::libcurl ALIAS libcurl)
	target_link_libraries(libcurl INTERFACE curl mbedtls mbedx509 mbedcrypto z wut m)
endif()

if((SE_DEPS STREQUAL "system" OR SE_DEPS STREQUAL "fallback") AND NOT TARGET CURL::libcurl)
	if(SE_CLOUDVARS)
		if(NOT SE_FORCE_CLOUDVARS_SOURCE_CURL)
			find_package(CURL QUIET OPTIONAL_COMPONENTS WSS)

			if((NOT CURL_FOUND OR NOT CURL_WSS_FOUND) AND SE_PKGCONF_CURL AND PkgConfig_FOUND)
				pkg_check_modules(curl IMPORTED_TARGET GLOBAL libcurl>=8.4.0)
				if(curl_FOUND)
					add_library(CURL::libcurl ALIAS PkgConfig::curl)
				endif()
			endif()
		endif()
	elseif(PkgConfig_FOUND)
		pkg_check_modules(curl IMPORTED_TARGET GLOBAL libcurl>=8.4.0)
		if(curl_FOUND)
			add_library(CURL::libcurl ALIAS PkgConfig::curl)
		endif()
	endif()
endif()

if((SE_DEPS STREQUAL "fallback" AND NOT TARGET CURL::libcurl) OR SE_DEPS STREQUAL "source")
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
	)
	if(WEBOS)
		list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS OFF")
	else()
		list(APPEND CURL_OPTIONS "CURL_USE_MBEDTLS ON")
	endif()
	if(WIN32 OR WEBOS)
		list(APPEND CURL_OPTIONS "CURL_ENABLE_SSL OFF")
	endif()

	CPMAddPackage(
		NAME curl
		GIT_REPOSITORY "https://github.com/curl/curl.git"
		GIT_TAG "curl-8_15_0"
		VERSION 8.15.0
		PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/3ds-curl.patch"
		OPTIONS ${CURL_OPTIONS}
	)
endif()

if(NOT TARGET CURL::libcurl)
	message(
		FATAL_ERROR
		"Failed to get libcurl."
	)
endif()
