function(_dep_system_mbedtls)
    find_package(mbedtls mbedx509 mbedcrypto QUIET)
endfunction()

function(_dep_source_mbedtls)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

    CPMAddPackage(
        NAME mbedtls
        GITHUB_REPOSITORY Mbed-TLS/mbedtls
        GIT_TAG mbedtls-3.6.0
        OPTIONS
        "ENABLE_PROGRAMS OFF"
        "ENABLE_TESTING OFF"
        "USE_SHARED_MBEDTLS_LIBRARY OFF"
        "MBEDTLS_TLS_ENABLED ON"
    )
endfunction()
