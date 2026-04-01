add_library(native-extension SHARED main.cpp)
target_include_directories(native-extension PRIVATE ${ABSOLUTE_SOURCES}) # I'm testing this pre-library so uh i just have to use the source dirs as include dirs
target_link_libraries(native-extension PRIVATE ryuJS)
se_add_dependency(native-extension miniz)
se_add_dependency(native-extension expected-lite)
