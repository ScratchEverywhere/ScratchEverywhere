#include "interpret.hpp"
#include "scratch/menus/mainMenu.hpp"
#include "scratch/render.hpp"
#include "scratch/unzip.hpp"

#ifdef __PC__
#include <dlfcn.h>
#elif defined(__WIIU__)
#include <coreinit/dynload.h>
#endif

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

    // Close Extension Libraries
    for (auto extension : extensions) {
#ifdef __PC__
        dlclose(extension.handle);
#elif defined(__WIIU__)
        OSDynLoad_Release(extension.module);
#endif
    }
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