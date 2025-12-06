#include "unzip.hpp"
#include "image.hpp"
#include <fstream>
#ifdef __3DS__
#include <3ds.h>
#elif defined(RENDERER_SDL1)
#include "SDL/SDL.h"
#elif defined(RENDERER_SDL2)
#include "SDL2/SDL.h"
#elif defined(RENDERER_SDL3)
#include "SDL3/SDL.h"
#endif

#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>
#include <sstream>

CMRC_DECLARE(romfs);
#endif

#ifdef ENABLE_LOADSCREEN
#include "menus/loading.hpp"
#endif

volatile int Unzip::projectOpened = 0;
std::string Unzip::loadingState = "";
volatile bool Unzip::threadFinished = false;
std::string Unzip::filePath = "";
mz_zip_archive Unzip::zipArchive;
std::vector<char> Unzip::zipBuffer;
bool Unzip::UnpackedInSD = false;

int Unzip::openFile(std::istream *&file) {
    Log::log("Unzipping Scratch project...");

    // load Scratch project into memory
    Log::log("Loading SB3 into memory...");
    std::string embeddedFilename = "project.sb3";
    std::string unzippedPath = "project/project.json";

    embeddedFilename = OS::getRomFSLocation() + embeddedFilename;
    unzippedPath = OS::getRomFSLocation() + unzippedPath;

#ifdef USE_CMAKERC
    const auto &fs = cmrc::romfs::get_filesystem();
#endif

    // Unzipped Project in romfs:/
#ifdef USE_CMAKERC
    if (fs.exists(unzippedPath)) {
        const auto &romfsFile = fs.open(unzippedPath);
        const std::string_view content(romfsFile.begin(), romfsFile.size());
        file = new std::istringstream(std::string(content));
    }
#else
    file = new std::ifstream(unzippedPath, std::ios::binary | std::ios::ate);
#endif
    projectType = UNZIPPED;
    if (file != nullptr && *file) return 1;
    // .sb3 Project in romfs:/
    Log::logWarning("No unzipped project, trying embedded.");
    projectType = EMBEDDED;
#ifdef USE_CMAKERC
    if (fs.exists(embeddedFilename)) {
        const auto &romfsFile = fs.open(embeddedFilename);
        const std::string_view content(romfsFile.begin(), romfsFile.size());
        file = new std::istringstream(std::string(content));
        file->seekg(0, std::ios::end);
    }
#else
    file = new std::ifstream(embeddedFilename, std::ios::binary | std::ios::ate);
#endif
    if (file != nullptr && *file) return 1;
    // Main menu
    Log::logWarning("No sb3 project, trying Main Menu.");
    projectType = UNEMBEDDED;
    if (filePath == "") {
        Log::log("Activating main menu...");
        return -1;
    }
    // SD card Project
    Log::logWarning("Main Menu already done, loading SD card project.");
    // check if normal Project
    if (filePath.size() >= 4 && filePath.substr(filePath.size() - 4, filePath.size()) == ".sb3") {
        Log::log("Normal .sb3 project in SD card ");
        file = new std::ifstream(filePath, std::ios::binary | std::ios::ate);
        if (!(*file)) {
            Log::logError("Couldnt find file. jinkies.");
            Log::logWarning(filePath);
            return 0;
        }

        return 1;
    }
    projectType = UNZIPPED;
    Log::log("Unpacked .sb3 project in SD card");
    // check if Unpacked Project
    file = new std::ifstream(filePath + "/project.json", std::ios::binary | std::ios::ate);
    if (file == nullptr || !(*file)) {
        Log::logError("Couldnt open Unpacked Scratch File");
        Log::logWarning(filePath);
        return 0;
    }
    filePath = filePath + "/";
    UnpackedInSD = true;

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
            Image::loadImageFromFile(currentSprite->costumes[currentSprite->currentCostume].fullName, currentSprite);
            sprIndex++;
        }
    } else {
        for (auto &currentSprite : sprites) {
            if (!currentSprite->visible || currentSprite->ghostEffect == 100) continue;
            Unzip::loadingState = "Loading image " + std::to_string(sprIndex) + " / " + std::to_string(sprites.size());
            Image::loadImageFromSB3(&Unzip::zipArchive, currentSprite->costumes[currentSprite->currentCostume].fullName, currentSprite);
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

#elif defined(RENDERER_SDL1) | defined(RENDERER_SDL2) || defined(RENDERER_SDL3) // create SDL thread for loading screen
#ifdef RENDERER_SDL1
    SDL_Thread *thread = SDL_CreateThread(projectLoaderThread, nullptr);
#elif defined(RENDERER_SDL2)
    SDL_Thread *thread = SDL_CreateThreadWithStackSize(projectLoaderThread, "LoadingScreen", 0x15000, nullptr);
#else
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER, (void *)projectLoaderThread);
    SDL_SetStringProperty(props, SDL_PROP_THREAD_CREATE_NAME_STRING, "LoadingScreen");
    SDL_SetNumberProperty(props, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER, 0x15000);
    SDL_SetPointerProperty(props, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, nullptr);
    SDL_Thread *thread = SDL_CreateThreadWithProperties(props);
    SDL_DestroyProperties(props);
#endif

    if (thread != NULL && thread != nullptr) {

        Loading loading;
        loading.init();

        while (!Unzip::threadFinished) {
            loading.render();
        }
        SDL_WaitThread(thread, nullptr);
        loading.cleanup();
    } else Unzip::openScratchProject(NULL);

    if (Unzip::projectOpened != 1)
        return false;
#endif
#else

    // non-threaded loading
    Unzip::openScratchProject(NULL);
    if (Unzip::projectOpened != 1)
        return false;
#endif

    loadInitialImages();
    return true;
}
