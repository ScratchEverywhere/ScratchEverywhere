include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/deps/add_dependency.cmake")

function(add_extension TARGET)
	add_library(${TARGET} SHARED ${ARGN})
	target_link_libraries(${TARGET} PRIVATE se-interface)

	if(APPLE)
		target_link_options(${TARGET} PRIVATE -undefined dynamic_lookup)
	endif()
endfunction()
