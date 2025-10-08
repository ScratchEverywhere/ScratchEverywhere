#include "interpret.hpp"
#include "os.hpp"
#include "scratch/extensions/format.hpp"
#include "scratch/menus/mainMenu.hpp"
#include "scratch/render.hpp"
#include <fstream>

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

    // Temporary testing code for custom extensions
    std::ifstream instream("see-example.see", std::ios::in | std::ios::binary);
    auto extensionData = extensions::parse(instream);
    if (!extensionData.has_value()) Log::logError("Error when parsing extension: " + extensionData.error());
    else Log::log(extensionData.value().name);
    instream.close();

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
