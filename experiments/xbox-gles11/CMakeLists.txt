cmake_minimum_required(VERSION 3.20)
project(se_gles11_probe C)

if(NOT NXDK OR NOT EXISTS "${GLES11_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Use scripts/build-xbox-gles11-probe.sh")
endif()
set(NXDK_GLES11_WITH_GL4ES OFF CACHE BOOL "" FORCE)
add_subdirectory("${GLES11_SOURCE_DIR}" gles11)
add_executable(gles11-probe main.c)
target_link_libraries(gles11-probe PRIVATE GLESv1_CM
    "${NXDK_DIR}/lib/libpbkit.lib" "${NXDK_DIR}/lib/libnxdk_automount_d.lib")
target_link_options(gles11-probe PRIVATE "-include:_automount_d_drive")
add_custom_command(TARGET gles11-probe POST_BUILD
    COMMAND "${NXDK_DIR}/bin/cxbe" -OUT:${CMAKE_CURRENT_BINARY_DIR}/default.xbe
    -TITLE:GLES11-Probe $<TARGET_FILE:gles11-probe>)
