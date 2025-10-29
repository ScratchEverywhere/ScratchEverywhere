#include "interpret.hpp"
#include "scratch/menus/menuManager.hpp"
#include "scratch/render.hpp"
#include "scratch/unzip.hpp"
#include <cstdlib>
#include <memory>

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
    MenuManager::freeClay();
    Render::deInit();
}

static bool initApp() {
    const bool result = Render::Init();
    MenuManager::initClay();
    return result;
}

bool activateMainMenu() {
    MenuManager menuManager;

    menuManager.changeMenu(MenuID::MainMenu);

    while (Render::appShouldRun(&menuManager)) {
        menuManager.render();
        if (Unzip::projectOpened >= 0) break;

#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
#endif
    }
    return true;
}

void mainLoop() {
    Scratch::startScratchProject();
    if (Scratch::nextProject) {
        Log::log("Loading: " + Unzip::filePath);
        if (!Unzip::load()) {
            if (Unzip::projectOpened == -3) { // main menu
                Unzip::filePath = "";
                Unzip::projectOpened = -67; // I have no idea what the correct number.
                Scratch::nextProject = false;
                Scratch::dataNextProject = Value();
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
        Unzip::projectOpened = -67; // I have no idea what the correct number.
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

#ifdef __EMSCRIPTEN__
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
