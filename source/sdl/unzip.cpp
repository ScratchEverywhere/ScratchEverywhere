#include "../scratch/unzip.hpp"

#ifdef __WIIU__
#include <sstream>
#include <whb/sdcard.h>
#endif

volatile int Unzip::projectOpened;
volatile bool Unzip::threadFinished;
std::string Unzip::filePath = "";
mz_zip_archive Unzip::zipArchive;
std::vector<char> Unzip::zipBuffer;

int Unzip::openFile(std::ifstream *file) {
    Log::log("Unzipping Scratch project...");

    // load Scratch project into memory
    Log::log("Loading SB3 into memory...");
    std::string filename = "project.sb3";
    std::string unzippedPath = "project/project.json";

#if defined(__WIIU__) || defined(__OGC__)
    file->open("romfs:/" + unzippedPath, std::ios::binary | std::ios::ate);
#else
    file->open(unzippedPath, std::ios::binary | std::ios::ate);
#endif
    projectType = UNZIPPED;
    if (!(*file)) {
        Log::logWarning("No unzipped project, trying embedded.");

#if defined(__WIIU__) || defined(__OGC__)
        file->open("romfs:/" + filename, std::ios::binary | std::ios::ate);
#else
        file->open(filePath, std::ios::binary | std::ios::ate);
#endif
        projectType = EMBEDDED;
#ifdef __WIIU__
        if (!(*file)) {
            std::ostringstream path;
            path << WHBGetSdCardMountPath() << "/wiiu/scratch-wiiu/" << filePath;
            file->open(path.str(), std::ios::binary | std::ios::ate);
#endif
            if (!(*file)) {
                projectType = UNEMBEDDED;
                // if main menu hasn't been loaded yet, load it
                if (filePath == "") {
                    Log::log("Activating main menu...");
                    return -1;
                } else {

                    file->open(filePath, std::ios::binary | std::ios::ate);
                    if (!(*file)) {
                        Log::logError("Couldn't find file. jinkies.");
                        return 0;
                    }
                }
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
