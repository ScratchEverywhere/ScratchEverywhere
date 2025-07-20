#include "../scratch/unzip.hpp"

#ifdef __WIIU__
#include <sstream>
#include <whb/sdcard.h>
#endif

volatile int Unzip::projectOpened;
volatile bool Unzip::threadFinished;
std::string Unzip::filePath = "";

int Unzip::openFile(std::ifstream *file) {
    std::cout << "Unzipping Scratch Project..." << std::endl;

    // load Scratch project into memory
    std::cout << "Loading SB3 into memory..." << std::endl;
    std::string filename = "project.sb3";
    std::string unzippedPath = "project/project.json";

#ifdef __WIIU__
    file->open("romfs:/" + unzippedPath, std::ios::binary | std::ios::ate);
#else
    file->open(unzippedPath, std::ios::binary | std::ios::ate);
#endif
    projectType = UNZIPPED;
    if (!(*file)) {
        std::cout << "No unzipped project, trying embedded." << std::endl;

#ifdef __WIIU__
        file->open("romfs:/" + filename, std::ios::binary | std::ios::ate);
#else
        file->open(filename, std::ios::binary | std::ios::ate);
#endif
        projectType = EMBEDDED;
#ifdef __WIIU__
        if (!(*file)) {
            std::ostringstream path;
            path << WHBGetSdCardMountPath() << "/wiiu/scratch-wiiu/" << filename;
            file->open(path.str(), std::ios::binary | std::ios::ate);
#endif
            if (!(*file)) {
                std::cerr << "Couldnt find file. jinkies." << std::endl;
                return 0;
            }
#ifdef __WIIU__
        }
#endif
    }
    return 1;
}

bool Unzip::load() {
    openScratchProject(NULL);
    if (Unzip::projectOpened == 1)
        return true;
    else return false;
}
