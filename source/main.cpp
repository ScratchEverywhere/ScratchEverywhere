#include "emscripten/emscripten.h"
#include "interpret.hpp"
#include "scratch/menus/mainMenu.hpp"
#include "scratch/render.hpp"
#include "scratch/unzip.hpp"
#include <cstdlib>

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten_browser_file.h>
#endif

static void exitApp() {
    Render::deInit();
}

static bool initApp() {
    return Render::Init();
}

bool activateMainMenu() {
    MainMenu *menu = new MainMenu();
    MenuManager::changeMenu(menu);

    while (Render::appShouldRun()) {
        MenuManager::render();

        if (MenuManager::isProjectLoaded != 0) {
            if (MenuManager::isProjectLoaded == -1) {
                exitApp();
                return false;
            } else {
                MenuManager::isProjectLoaded = 0;
                break;
            }
        }

#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
#endif
    }
    return true;
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
    } else {
        Unzip::filePath = "";
        Scratch::nextProject = false;
        Scratch::dataNextProject = Value();
        if (toExit || !activateMainMenu()) {
            exitApp();
            exit(0);
        }
    }
}

int main(int argc, char **argv) {
    if (!initApp()) {
        exitApp();
        return 1;
    }

    srand(time(NULL));

#ifdef SDL_BUILD
    emscripten_sleep(1500); // Ummm, this makes it so it has time to load the project from the url, not hacky at all, trust me bro.
#endif

    if (!Unzip::load()) {
        if (Unzip::projectOpened == -3) {
#ifdef __EMSCRIPTEN__
            bool uploadComplete = false;
            emscripten_browser_file::upload(".sb3", [](std::string const &filename, std::string const &mime_type, std::string_view buffer, void *userdata) {
                *(bool *)userdata = true;
                if (!std::filesystem::exists(OS::getScratchFolderLocation())) std::filesystem::create_directory(OS::getScratchFolderLocation());
                std::ofstream f(OS::getScratchFolderLocation() + filename);
                f << buffer;
                f.close();
                Unzip::filePath = filename;
                Unzip::load(); // TODO: Error handling
            },
                                            &uploadComplete);
            while (Render::appShouldRun() && !uploadComplete)
                emscripten_sleep(0);
#else
            if (!activateMainMenu()) return 0;
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
