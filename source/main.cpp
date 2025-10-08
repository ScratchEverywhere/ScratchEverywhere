#include "interpret.hpp"
#include "scratch/menus/mainMenu.hpp"
#include "scratch/render.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifdef SDL_BUILD
#include <SDL2/SDL.h>
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

            // -1 means project couldn't load
            if (MenuManager::isProjectLoaded == -1) {
                exitApp();
                return false;
            } else {
                MenuManager::isProjectLoaded = 0;
                break;
            }
        }
    }
    return true;
}

int main(int argc, char **argv) {
    if (!initApp()) {
        exitApp();
        return 1;
    }

    srand(time(NULL));

    if (!Unzip::load()) {

        if (Unzip::projectOpened == -3) { // main menu

            if (!activateMainMenu()) return 0;

        } else {
            exitApp();
            return 0;
        }
    }

    while (Scratch::startScratchProject()) {

        if (Scratch::nextProject) {
            Log::log(Unzip::filePath);
            if (!Unzip::load()) {

                if (Unzip::projectOpened == -3) { // main menu

                    if (!activateMainMenu()) break;

                } else {
                    exitApp();
                    break;
                }
            }
        } else {
            Unzip::filePath = "";
            Scratch::nextProject = false;
            Scratch::dataNextProject = Value();
            if (toExit || !activateMainMenu()) break;
        }
    }
    exitApp();
    return 0;
}
