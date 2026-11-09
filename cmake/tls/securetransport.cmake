target_link_libraries(se-interface INTERFACE "-framework Security" "-framework CoreFoundation")
target_compile_definitions(se-interface INTERFACE SE_TLS_SECURETRANSPORT)
