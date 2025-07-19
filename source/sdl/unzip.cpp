#include "../scratch/unzip.hpp"

volatile int Unzip::projectOpened;
volatile bool Unzip::threadFinished;
std::string Unzip::filePath = "";

int Unzip::openFile(std::ifstream *file){
    std::cout<<"Unzipping Scratch Project..."<<std::endl;

    // load Scratch project into memory
    std::cout<<"Loading SB3 into memory..."<<std::endl;
    const char* filename = "project.sb3";
    const char* unzippedPath = "project/project.json";

    //first try embedded unzipped project
    file->open(unzippedPath, std::ios::binary | std::ios::ate);
    projectType = UNZIPPED;
    if(!(*file)){
        std::cerr<<"No unzipped project, trying embedded."<<std::endl;

        // try embedded zipped sb3
        file->open(std::string(filename), std::ios::binary | std::ios::ate); // loads file from romfs
        projectType = EMBEDDED;
        if (!(*file)){
            std::cerr<<"Couldnt find file. jinkies."<<std::endl;
            return 0;
        }
    }
    return 1;
}


bool Unzip::load(){
    openScratchProject(NULL);
    if(Unzip::projectOpened == 1)
    return true;
    else return false;
}