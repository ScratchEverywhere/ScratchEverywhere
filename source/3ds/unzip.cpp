#include "../scratch/unzip.hpp"
#include <3ds.h>
#include "render.hpp"

volatile int Unzip::projectOpened = 0;
volatile bool Unzip::threadFinished = false;
std::string Unzip::filePath = "";

int Unzip::openFile(std::ifstream *file){
    std::cout<<"Unzipping Scratch Project..."<<std::endl;

    // load Scratch project into memory
    std::cout<<"Loading SB3 into memory..."<<std::endl;
    const char* filename = "project.sb3";
    const char* unzippedPath = "romfs:/project/project.json";

    //first try embedded unzipped project
    file->open(unzippedPath, std::ios::binary | std::ios::ate);
    projectType = UNZIPPED;
    if(!(*file)){
        std::cerr<<"No unzipped project, trying embedded."<<std::endl;

        // try embedded zipped sb3
        file->open("romfs:/"+std::string(filename), std::ios::binary | std::ios::ate); // loads file from romfs
        projectType = EMBEDDED;
        if (!(*file)){
            std::cerr<<"No embedded Scratch project, trying SD card"<<std::endl;

            if(filePath == "") return -1;

            // then try SD card location
            file->open(filePath, std::ios::binary | std::ios::ate); // loads file from location of executable
            projectType = UNEMBEDDED;
            if (!(*file)){
                std::cerr<<"Couldnt find file. jinkies." << std::endl;
                return 0;
            }
        }
        else{
            filePath = "romfs:/"+std::string(filename);
        }
    }
    return 1;
}

bool Unzip::load(){

    Unzip::threadFinished = false;
	Unzip::projectOpened = 0;

    s32 mainPrio = 0;
    svcGetThreadPriority(&mainPrio, CUR_THREAD_HANDLE);

	Thread projectThread = threadCreate(
        Unzip::openScratchProject,
        NULL,
        0x4000,
        mainPrio + 1,
        -1,
        false
    );

    if(!projectThread) {
		Unzip::threadFinished = true;
		Unzip::projectOpened = -3;
    }

    LoadingScreen loading;
	loading.init();
  
    while(!Unzip::threadFinished){
		loading.renderLoadingScreen();
		gspWaitForVBlank();
    }
	threadJoin(projectThread, U64_MAX);
    threadFree(projectThread);
	if(Unzip::projectOpened != 1){
		loading.cleanup();
		return false;
	}
	loading.cleanup();
    // disable new 3ds clock speeds for a bit cus it crashes for some reason otherwise????
	osSetSpeedupEnable(false);
    return true;
}
