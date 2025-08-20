#include "interpret.hpp"
#include "scratch/menus/mainMenu.hpp"
#include "scratch/render.hpp"
#include "scratch/unzip.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

// arm-none-eabi-addr2line -e Scratch.elf xxx
// ^ for debug purposes
#ifdef __OGC__
#include <SDL2/SDL.h>
#endif

static void exitApp() {
    Render::deInit();
}

static bool initApp() {
    return Render::Init();
}

int main(int argc, char **argv) {
    if (!initApp()) {
        exitApp();
        return 1;
    }

    if (!Unzip::load()) {

        if (Unzip::projectOpened == -3) { // main menu

            if (!MainMenu::activateMainMenu()) {
                exitApp();
                return 0;
            }

        } else {

            exitApp();
            return 0;
        }
    }

    while (Scratch::startScratchProject()) {
        if (!MainMenu::activateMainMenu()) {
            exitApp();
            return 0;
        }
    }
    exitApp();
    return 0;
}