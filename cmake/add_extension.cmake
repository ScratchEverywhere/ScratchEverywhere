include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

function(add_extension TARGET)
	add_library(${TARGET} SHARED ${ARGN})

	if(APPLE)
		target_link_options(${TARGET} PRIVATE -undefined dynamic_lookup)
	endif()

	target_include_directories(${TARGET} PRIVATE ${ABSOLUTE_SOURCES}) # Once we add the library build target this will be better.
	target_link_libraries(${TARGET} PRIVATE ryuJS)
	se_add_dependency(${TARGET} miniz)
	se_add_dependency(${TARGET} expected-lite)
	se_add_dependency(${TARGET} nlohmann_json)
endfunction()
