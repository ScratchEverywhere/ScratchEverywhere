if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")
se_add_dependency(windowing_interface SDL)

if (APPLE)
	execute_process(COMMAND clang "--print-runtime-dir" OUTPUT_VARIABLE CLANG_COMMAND_OUTPUT OUTPUT_STRIP_TRAILING_WHITESPACE)
	target_link_libraries(windowing_interface PUBLIC "${CLANG_COMMAND_OUTPUT}/libclang_rt.osx.a")
endif()
