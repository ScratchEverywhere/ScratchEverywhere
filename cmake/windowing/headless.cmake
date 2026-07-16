if(TARGET windowing_interface)
    return()
endif()
add_library(windowing_interface INTERFACE)

if (APPLE)
	find_library(CLANG_RT_OSX libclang_rt.osx.a PATHS "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang" "/Library/Developer/CommandLineTools/usr/lib/clang" PATH_SUFFIXES "lib/darwin")
    target_link_libraries(windowing_interface INTERFACE "${CLANG_RT_OSX}")
endif()
