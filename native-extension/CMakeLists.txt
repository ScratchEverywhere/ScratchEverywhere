if(APPLE)
	add_library(native-extension MODULE main.cpp)
	target_link_options(native-extension PRIVATE -undefined dynamic_lookup)
	set_target_properties(native-extension PROPERTIES BUNDLE TRUE)
else()
	add_library(native-extension SHARED main.cpp)
endif()

target_include_directories(native-extension PRIVATE ${ABSOLUTE_SOURCES}) # I'm testing this pre-library so uh i just have to use the source dirs as include dirs
target_link_libraries(native-extension PRIVATE ryuJS)
se_add_dependency(native-extension miniz)
se_add_dependency(native-extension expected-lite)
se_add_dependency(native-extension nlohmann_json)
