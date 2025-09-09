#include "../scratch/unzip.hpp"
#include "../scratch/menus/loading.hpp"
#include "render.hpp"
#include <3ds.h>

volatile int Unzip::projectOpened = 0;
std::string Unzip::loadingState = "";
volatile bool Unzip::threadFinished = false;
std::string Unzip::filePath = "";
mz_zip_archive Unzip::zipArchive;
std::vector<char> Unzip::zipBuffer;
bool Unzip::UnpackedInSD = false;
void *Unzip::trackedBufferPtr = nullptr;
size_t Unzip::trackedBufferSize = 0;
void *Unzip::trackedJsonPtr = nullptr;
size_t Unzip::trackedJsonSize = 0;

int Unzip::openFile(std::ifstream *file) {
    Log::log("Unzipping Scratch Project...");

    // load Scratch project into memory
    Log::log("Loading SB3 into memory...");
    const char *filename = "project.sb3";
    const char *unzippedPath = "romfs:/project/project.json";
    
    // first try embedded unzipped project
    file->open(unzippedPath, std::ios::binary | std::ios::ate);
    projectType = UNZIPPED;
    if (!(*file)) {
        Log::log("No unzipped project, trying embedded.");

        // try embedded zipped sb3
        file->open("romfs:/" + std::string(filename), std::ios::binary | std::ios::ate); // loads file from romfs
        projectType = EMBEDDED;
        if (!(*file)) {
            Log::log("No embedded Scratch project, trying SD card");
            projectType = UNEMBEDDED;

            // load main menu if not already
            if (filePath == "") return -1;

            // if main menu was loaded, load the selected file from main menu

            // check if normal Project
            if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4, filePath.size()) == ".sb3") {

                Log::log("Normal .sb3 project in SD card ");
                file->open(OS::getScratchFolderLocation() + filePath, std::ios::binary | std::ios::ate);
                if (!(*file)) {

                    Log::logError("Couldnt find file. jinkies.");
                    Log::logWarning(filePath);
                    return 0;
                }
            } else {
                projectType = UNZIPPED;
                Log::log("Unpacked .sb3 project in SD card");
                // check if Unpacked Project
                file->open(OS::getScratchFolderLocation() + filePath + "/project.json", std::ios::binary | std::ios::ate);
                if (!(*file)) {
                    Log::logError("Couldnt open Unpacked Scratch File");
                    Log::logWarning(filePath + "<");
                    return 0;
                }
                filePath = OS::getScratchFolderLocation() + filePath + "/";
                UnpackedInSD = true;
            }

        } else {
            filePath = "romfs:/" + std::string(filename);
        }
    }
    return 1;
}

bool Unzip::load() {

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
        false);

    if (!projectThread) {
        Unzip::threadFinished = true;
        Unzip::projectOpened = -3;
    }

    Loading loading;
    loading.init();

    while (!Unzip::threadFinished) {
#ifdef ENABLE_BUBBLES
        loading.render();
#endif
    }
    threadJoin(projectThread, U64_MAX);
    threadFree(projectThread);
    if (Unzip::projectOpened != 1) {
        loading.cleanup();
        return false;
    }
    loading.cleanup();
    // disable new 3ds clock speeds for a bit cus it crashes for some reason otherwise????
    osSetSpeedupEnable(false);
    return true;
}
