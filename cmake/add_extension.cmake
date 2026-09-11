
function(add_extension TARGET)
	add_library(${TARGET} SHARED ${ARGN})
	set_target_properties(${TARGET} PROPERTIES PREFIX "")
	target_link_libraries(${TARGET} PRIVATE se-interface)

	if(APPLE)
		target_link_options(${TARGET} PRIVATE -undefined dynamic_lookup)
	endif()
endfunction()
