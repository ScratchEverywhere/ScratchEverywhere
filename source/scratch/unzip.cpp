#include "unzip.hpp"
#include "menus/loading.hpp"
#include "render.hpp"
#ifdef __3DS__
#include <3ds.h>
#elif defined(SDL_BUILD)
#include "SDL2/SDL.h"
#endif

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

            // Main menu
            Log::logWarning("No sb3 project, trying Main Menu.");
            projectType = UNEMBEDDED;
            if (filePath == "") {
                Log::log("Activating main menu...");
                return -1;
            } else {
                // SD card Project
                Log::logWarning("Main Menu already done, loading SD card project.");
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
            }
        }
    }
    return 1;
}

int projectLoaderThread(void *data) {
    Unzip::openScratchProject(NULL);
    return 0;
}

void loadInitialImages() {
    Unzip::loadingState = "Loading images";
    int sprIndex = 1;
    if (projectType == UNZIPPED) {
        for (auto &currentSprite : sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(sprites.size());
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].fullName);
            sprIndex++;
        }
    } else {
        for (auto &currentSprite : sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(sprites.size());
            Image::loadImageFromSB3(&Unzip::zipArchive, currentSprite->costumes[currentSprite->currentCostume].fullName);
            sprIndex++;
        }
    }
}

bool Unzip::load() {

    Unzip::threadFinished = false;
    Unzip::projectOpened = 0;

#ifdef ENABLE_LOADSCREEN

#ifdef __3DS__ // create 3DS thread for loading screen
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
        loading.render();
    }
    threadJoin(projectThread, U64_MAX);
    threadFree(projectThread);
    if (Unzip::projectOpened != 1) {
        loading.cleanup();
        return false;
    }
    loading.cleanup();
    osSetSpeedupEnable(false);

#else // create SDL2 thread for loading screen

    SDL_Thread *thread = SDL_CreateThread(projectLoaderThread, "LoadingScreen", nullptr);

    if (thread != NULL && thread != nullptr) {
        SDL_DetachThread(thread);

        Loading loading;
        loading.init();

        while (!Unzip::threadFinished) {
            loading.render();
        }
        loading.cleanup();
    } else Unzip::openScratchProject(NULL);

    if (Unzip::projectOpened != 1)
        return false;
#endif

#else // non-threaded loading
    Unzip::openScratchProject(NULL);
    if (Unzip::projectOpened != 1)
        return false;
#endif

    loadInitialImages();
    return true;
}
