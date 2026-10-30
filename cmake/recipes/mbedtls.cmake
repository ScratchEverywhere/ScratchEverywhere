function(_recipe_mbedtls_system)
    if(CMAKE_CROSSCOMPILING)
        return()
    endif()

    if(CL_REQ_VERSION)
        find_package(MbedTLS ${CL_REQ_VERSION} CONFIG QUIET)
    else()
        find_package(MbedTLS CONFIG QUIET)
    endif()

    if(TARGET MbedTLS::mbedtls)
        get_target_property(_MBEDTLS_ALIASED MbedTLS::mbedtls ALIASED_TARGET)
        if(_MBEDTLS_ALIASED)
            add_library(deps::mbedtls ALIAS ${_MBEDTLS_ALIASED})
        else()
            add_library(deps::mbedtls ALIAS MbedTLS::mbedtls)
        endif()
        return()
    endif()

    cl_format_pkgconfig_req("mbedtls" "${CL_VERSION_REQ}" PKG_SPEC)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(mbedtls IMPORTED_TARGET GLOBAL ${PKG_SPEC})
        if(mbedtls_FOUND AND TARGET PkgConfig::mbedtls)
            add_library(deps::mbedtls ALIAS PkgConfig::mbedtls)
        endif()
    endif()
endfunction()

function(_recipe_mbedtls_package)
    if(CL_PACKAGE_MANAGER STREQUAL "apt")
        set(CL_PACKAGE_NAME "libmbedtls-dev" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "pacman")
        set(CL_PACKAGE_NAME "mbedtls" PARENT_SCOPE)
    elseif(CL_PACKAGE_MANAGER STREQUAL "brew")
        set(CL_PACKAGE_NAME "mbedtls" PARENT_SCOPE)
    endif()
endfunction()

function(_recipe_mbedtls_source)
    set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)

    set(MBEDTLS_OPTIONS
        "ENABLE_PROGRAMS" "OFF"
        "ENABLE_TESTING" "OFF"
        "BUILD_SHARED_LIBS" "OFF"
        "MBEDTLS_FATAL_WARNINGS" "OFF"
        "INSTALL_MBEDTLS_HEADERS" "ON"
    )

    cl_import_source(
        NAME mbedtls
        URL "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2"
        PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/mbedtls.patch"
        OPTIONS ${MBEDTLS_OPTIONS}
    )

    if(TARGET mbedtls)
        if(NOT TARGET deps::mbedtls)
            add_library(deps::mbedtls ALIAS mbedtls)
        endif()
        target_include_directories(mbedtls INTERFACE "${CL_SOURCE_DIR}/include")
        target_include_directories(mbedx509 INTERFACE "${CL_SOURCE_DIR}/include")
        target_include_directories(mbedcrypto INTERFACE "${CL_SOURCE_DIR}/include")
        set(MBEDTLS_INCLUDE_DIR "${CL_SOURCE_DIR}/include" CACHE PATH "mbedtls include dir" FORCE)
        set(MBEDTLS_INCLUDE_DIR "${CL_SOURCE_DIR}/include" PARENT_SCOPE)
        set(MBEDTLS_LIBRARY "mbedtls" CACHE STRING "mbedtls library" FORCE)
        set(MBEDTLS_LIBRARY "mbedtls" PARENT_SCOPE)
        set(MBEDX509_LIBRARY "mbedx509" CACHE STRING "mbedx509 library" FORCE)
        set(MBEDX509_LIBRARY "mbedx509" PARENT_SCOPE)
        set(MBEDCRYPTO_LIBRARY "mbedcrypto" CACHE STRING "mbedcrypto library" FORCE)
        set(MBEDCRYPTO_LIBRARY "mbedcrypto" PARENT_SCOPE)
    endif()
endfunction()
