#include <cstdlib>
#include <input.hpp>
#include <memory>
#include <menuManager.hpp>
#ifndef LIBRETRO
#include "image.hpp"
#include "translation.hpp"
#include <cstdlib>
#include <inspector.hpp>
#include <log.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <unzip.hpp>

#ifdef ENABLE_AUDIO
#include <audio.hpp>
#endif

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten_browser_file.h>
#include <filesystem.hpp>
#endif

static void exitApp() {
    MenuManager::freeClay();
    Render::deInit();
    OS::deinit();
}

static bool initApp() {
    const bool result = Scratch::initializeRuntime();
    MenuManager::initClay();
    return result;
}

bool activateMainMenu() {
    MenuManager menuManager;
    Render::menuManager = &menuManager;

    menuManager.changeMenu(MenuID::MainMenu);

    while (Render::appShouldRun()) {
        Input::getInput(&menuManager);

        menuManager.render();
        if (Unzip::projectOpened >= 0) {
            Render::menuManager = nullptr;
            return true;
        }

#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
#endif
#ifdef ENABLE_INSPECTOR
        Inspector::processCommands();
#endif
    }
    Render::menuManager = nullptr;
    return false;
}

void mainLoop() {
    Scratch::startScratchProject();

    if (Scratch::nextProject) {
        Log::log(Unzip::filePath);
        if (Unzip::load()) {
            goto skipCheck;
        }

        if (Unzip::projectOpened != -3) { // main menu
            exitApp();
            exit(0);
        }

        if (!activateMainMenu()) {
            exitApp();
            exit(0);
        }

    skipCheck:
        return;
    }

    Unzip::filePath = "";
    Scratch::nextProject = false;
    Scratch::dataNextProject = Value();
    if (OS::toExit || !activateMainMenu()) {
        exitApp();
        exit(0);
    }
}

#if defined(WINDOWING_SDL1) || defined(WINDOWING_SDL2)
#include <SDL.h>

extern "C" int main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
    if (!initApp()) {
        exitApp();
        return 1;
    }

    srand(time(NULL));

    bool enableInspector = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--inspector") {
            enableInspector = true;
        } else if (Unzip::filePath.empty()) {
#if defined(__PC__)
            Unzip::filePath = arg;
#endif
        }
    }

#ifdef ENABLE_INSPECTOR
    if (enableInspector) Inspector::init();
#endif

#if defined(__EMSCRIPTEN__)
    if (argc > 1) {
        while (!FileSystem::fileExists("/romfs/project.sb3")) {
            if (!Render::appShouldRun()) {
                exitApp();
                exit(0);
            }
            emscripten_sleep(0);
        }
    }
#endif

    if (!Unzip::load()) {
        if (Unzip::projectOpened == -3) {
#ifdef __EMSCRIPTEN__
            bool uploadComplete = false;
            emscripten_browser_file::upload(".sb3", [](std::string const &filename, std::string const &mime_type, std::string_view buffer, void *userdata) {
                *(bool *)userdata = true;
                if (!FileSystem::fileExists(OS::getScratchFolderLocation())) FileSystem::createDirectory(OS::getScratchFolderLocation());
                std::ofstream f(OS::getScratchFolderLocation() + filename);
                f << buffer;
                f.close();
                Unzip::filePath = OS::getScratchFolderLocation() + filename;
                Unzip::load(); // TODO: Error handling
            },
                                            &uploadComplete);
            while (Render::appShouldRun() && !uploadComplete)
                emscripten_sleep(0);
#else
            if (!activateMainMenu()) {
                exitApp();
                return 0;
            }
#endif
        } else {
            exitApp();
            return 0;
        }
        if (!activateMainMenu()) return 0;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (1)
        mainLoop();
#endif

    return 0;
}
#endif
