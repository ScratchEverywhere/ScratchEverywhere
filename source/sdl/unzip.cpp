#include "../scratch/unzip.hpp"
#include "interpret.hpp"
#include "miniz/miniz.h"
#include "os.hpp"
#include <cstddef>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#ifdef __WIIU__
#include <sstream>
#include <whb/sdcard.h>
#endif

volatile int Unzip::projectOpened;
volatile bool Unzip::threadFinished;
std::string Unzip::filePath = "";
std::string Unzip::loadingState = "";
mz_zip_archive Unzip::zipArchive;
std::vector<char> Unzip::zipBuffer;
bool Unzip::UnpackedInSD = false;
void *Unzip::trackedBufferPtr = nullptr;
size_t Unzip::trackedBufferSize = 0;
void *Unzip::trackedJsonPtr = nullptr;
size_t Unzip::trackedJsonSize = 0;

int Unzip::openFile(std::ifstream *file) {
    Log::log("Unzipping Scratch project...");

    // load Scratch project into memory
    Log::log("Loading SB3 into memory...");
    std::string embeddedFilename = "project.sb3";
    std::string unzippedPath = "project/project.json";

    embeddedFilename = OS::getRomFSLocation() + embeddedFilename;
    unzippedPath = OS::getRomFSLocation() + unzippedPath;

    // Unzipped Project in romfs:/
    file->open(unzippedPath, std::ios::binary | std::ios::ate);
    projectType = UNZIPPED;
    if (!(*file)) {

        // .sb3 Project in romfs:/
        Log::logWarning("No unzipped project, trying embedded.");
        projectType = EMBEDDED;
        file->open(embeddedFilename, std::ios::binary | std::ios::ate);
        if (!(*file)) {


            file->open(OS::getScratchFolderLocation() + "project.sb3", std::ios::binary | std::ios::ate);

            if (!(*file)) {

                // Main menu
                Log::logWarning("No sb3 project, trying Main Menu.");
                projectType = UNEMBEDDED;
                if (filePath == "") {
                    Log::log("Activating main menu...");
                    return -1;
                } else {
                    // SD card Project
                    Log::logWarning("Main Menu already done, loading SD card project.");
                    if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4, filePath.size()) == ".sb3") {
                        Log::log("Normal .sb3 project in SD card ");

                          file->open(OS::getScratchFolderLocation() + filePath, std::ios::binary | std::ios::ate);
                          if (!(*file)) {
                            Log::logError("Couldn't find file. jinkies.");
                            return 0;
                          }
                      
                      } else {
                          projectType = UNZIPPED;
                          Log::log("Unpacked .sb3 project in SD card");
                            // check if Unpacked Project
                          file->open(OS::getScratchFolderLocation() + filePath + "/project.json", std::ios::binary | std::ios::ate);
                          if (!(*file)) {
                              Log::logError("Couldnt open Unpacked Scratch File");
                              return 0;
                          }
                     }
                }
          }
    }
    return 1;
}

bool Unzip::load() {
    openScratchProject(NULL);
    if (Unzip::projectOpened == 1)
        return true;
    else return false;
}
