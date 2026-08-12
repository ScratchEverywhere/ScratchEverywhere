#!/bin/sh
cd "${0%/*}";
if [ -d "build-sdl2" ]; then
	rm -fr build-sdl2;
fi;
if [ ! -d "build-sdl2" ]; then
	mkdir build-sdl2;
fi;
if [ -d "build-sdl2" ]; then
	cd build-sdl2; 
	cmake .. -DCMAKE_BUILD_TYPE=Release -DSE_RENDERER="sdl2" -DSE_WINDOWING="sdl2" -DSE_AUDIO_ENGINE="sdl2" -DSE_USE_LIBRARY_BUILD=0 && make;
	cmake .. -DCMAKE_BUILD_TYPE=Release -DSE_RENDERER="sdl2" -DSE_WINDOWING="sdl2" -DSE_AUDIO_ENGINE="sdl2" -DSE_USE_LIBRARY_BUILD=1 && make;
	if [ `uname -s` = "Darwin" ]; then
    	if sw_vers | grep -q "macOS\|Mac OS X"; then
			if [ -f "scratch-pc" ]; then
        		install_name_tool -delete_rpath "`brew --prefix`/lib" scratch-pc;
        		install_name_tool -add_rpath @executable_path scratch-pc;
        		install_name_tool -add_rpath @loader_path scratch-pc;
        		install_name_tool -add_rpath @executable_path/../Frameworks scratch-pc;
        		install_name_tool -add_rpath @executable_path/../Resources scratch-pc;
        		install_name_tool -add_rpath . scratch-pc;
        		install_name_tool -id @rpath/libscratch.dylib scratch-pc;
        		install_name_tool -change "`brew --prefix sdl2_ttf`/lib/libSDL2_ttf-2.0.0.dylib" @rpath/libsdl2_ttf.dylib scratch-pc;
        		install_name_tool -change "`brew --prefix sdl2`/lib/libSDL2-2.0.0.dylib" @rpath/libsdl2.dylib scratch-pc;
			fi;
			if [ -f "libscratch.dylib" ]; then
        		install_name_tool -delete_rpath "`brew --prefix`/lib" libscratch.dylib;
        		install_name_tool -add_rpath @executable_path libscratch.dylib;
        		install_name_tool -add_rpath @loader_path libscratch.dylib;
        		install_name_tool -add_rpath @executable_path/../Frameworks libscratch.dylib;
        		install_name_tool -add_rpath @executable_path/../Resources libscratch.dylib;
        		install_name_tool -add_rpath . libscratch.dylib;
        		install_name_tool -id @rpath/libscratch.dylib libscratch.dylib;
        		install_name_tool -change "`brew --prefix sdl2_ttf`/lib/libSDL2_ttf-2.0.0.dylib" @rpath/libsdl2_ttf.dylib libscratch.dylib;
        		install_name_tool -change "`brew --prefix sdl2`/lib/libSDL2-2.0.0.dylib" @rpath/libsdl2.dylib libscratch.dylib;
			fi;
		fi;
    fi;
fi;
