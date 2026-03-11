#include "image.hpp"
#ifdef ENABLE_MENU
#include <menus/mainMenu.hpp>
#endif
#include <cstdlib>
#include <menus/mainMenu.hpp>
#include <render.hpp>
#include <runtime.hpp>
#include <unzip.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef RENDERER_SDL2
#include <SDL2/SDL.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten_browser_file.h>
#endif

#ifdef PLAYDATE
#include <pdcpp/pdnewlib.h>
#endif

static void exitApp() {
    Render::deInit();
}

static bool initApp() {
    Log::deleteLogFile();
    Render::debugMode = true;
    if (!Render::Init()) {
        return false;
    }
    return true;
}

#ifdef PLAYDATE
PlaydateAPI *pd = nullptr;

static int pdUpdate(void *userdata) {
    Scratch::stepScratchProject(); // TODO: handle exiting
    return 1;
}

extern "C" {
int eventHandler(PlaydateAPI *pdIn, PDSystemEvent event, uint32_t arg) {
    eventHandler_pdnewlib(pdIn, event, arg);

    pd = pdIn;

    if (event == kEventInit) {
        initApp(); // TODO: error handling
        srand(time(NULL));

        Unzip::load(); // TODO: error handling and menu
        Scratch::initializeScratchProject();

        pd->display->setRefreshRate(Scratch::turbo ? 0 : Scratch::FPS);
        pd->system->setUpdateCallback(pdUpdate, nullptr);
    } else if (event == kEventTerminate) {
        exitApp();
    }

    return 0;
}
}
#else

bool activateMainMenu() {
#ifdef ENABLE_MENU
    MainMenu *menu = new MainMenu();
    if (Unzip::filePath.empty()) MenuManager::changeMenu(menu);

    while (Render::appShouldRun()) {
        MenuManager::render();

        if (MenuManager::isProjectLoaded != 0) {
            if (MenuManager::isProjectLoaded == -1) return false;
            MenuManager::isProjectLoaded = 0;
            return true;
        }

#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
#endif
    }
#endif
    return false;
}

void mainLoop() {
    Scratch::startScratchProject();
    if (Scratch::nextProject) {
        Log::log(Unzip::filePath);
        if (!Unzip::load()) {
            if (Unzip::projectOpened == -3) { // main menu
                if (!activateMainMenu()) {
                    exitApp();
                    exit(0);
                }
            } else {
                exitApp();
                exit(0);
            }
        }

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

int main(int argc, char **argv) {
    if (!initApp()) {
        exitApp();
        return 1;
    }

    srand(time(NULL));

    if (argc > 1) {
#if defined(__EMSCRIPTEN__)
        while (!OS::fileExists("/romfs/project.sb3")) {
            if (!Render::appShouldRun()) {
                exitApp();
                exit(0);
            }
            emscripten_sleep(0);
        }
#elif defined(__PC__)
        Unzip::filePath = std::string(argv[1]);
#else
#endif
    }

    if (!Unzip::load()) {
        if (Unzip::projectOpened == -3) {
#ifdef __EMSCRIPTEN__
            bool uploadComplete = false;
            emscripten_browser_file::upload(".sb3", [](std::string const &filename, std::string const &mime_type, std::string_view buffer, void *userdata) {
                *(bool *)userdata = true;
                if (!OS::fileExists(OS::getScratchFolderLocation())) OS::createDirectory(OS::getScratchFolderLocation());
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
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (1)
        mainLoop();
#endif
    exitApp();
    return 0;
}
#endif
