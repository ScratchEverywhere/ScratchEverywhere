#include "scratch/blockExecutor.hpp"
#include "scratch/input.hpp"
#include "scratch/render.hpp"
#include "scratch/unzip.hpp"
#include <chrono>
#include <thread>

#ifdef __WIIU__
#include <coreinit/foreground.h>
#include <coreinit/thread.h>
#include <proc_ui/procui.h>
#endif

// arm-none-eabi-addr2line -e Scratch.elf xxx
// ^ for debug purposes

static void exitApp() {
    Render::deInit();
}

static bool initApp() {
    return Render::Init();
}

int main(int argc, char **argv) {
#ifdef __WIIU__
    ProcUIInit(OSSavesDone_ReadyToRelease);
#endif

    if (!initApp()) {
        exitApp();
        return 1;
    }

    // this is for the FPS
    std::chrono::_V2::system_clock::time_point startTime = std::chrono::high_resolution_clock::now();
    std::chrono::_V2::system_clock::time_point endTime = std::chrono::high_resolution_clock::now();
    // this is for frametime check
    std::chrono::_V2::system_clock::time_point frameStartTime = std::chrono::high_resolution_clock::now();
    std::chrono::_V2::system_clock::time_point frameEndTime = std::chrono::high_resolution_clock::now();

    if (!Unzip::load()) {

        if (Unzip::projectOpened == -3) { // main menu

            MainMenu menu;
            bool isLoaded = false;
            while (!isLoaded) {

                menu.render();
                if ((!menu.hasProjects && menu.shouldExit) || !Render::appShouldRun()) {
                    exitApp();
                    return 0;
                }

                if (Unzip::filePath != "") {
                    menu.cleanup();
                    if (!Unzip::load()) {
                        exitApp();
                        return 0;
                    }
                    isLoaded = true;
                }
            }
            if (!Render::appShouldRun()) {
                menu.cleanup();
                exitApp();
                return 0;
            }

        } else {

            exitApp();
            return 0;
        }
    }

    BlockExecutor::runAllBlocksByOpcode(Block::EVENT_WHENFLAGCLICKED);
    BlockExecutor::timer = std::chrono::high_resolution_clock::now();

    while (Render::appShouldRun()) {
#ifdef __WIIU__
        switch (ProcUIProcessMessages(true)) {
        case PROCUI_STATUS_IN_FOREGROUND:
#endif

            endTime = std::chrono::high_resolution_clock::now();
            if (endTime - startTime >= std::chrono::milliseconds(1000 / Scratch::FPS)) {
                startTime = std::chrono::high_resolution_clock::now();
                frameStartTime = std::chrono::high_resolution_clock::now();

                Input::getInput();
                BlockExecutor::runRepeatBlocks();
                Render::renderSprites();

                frameEndTime = std::chrono::high_resolution_clock::now();
                auto frameDuration = frameEndTime - frameStartTime;
                // std::cout << "\x1b[17;1HFrame time: " << frameDuration.count() << " ms" << std::endl;
                // std::cout << "\x1b[18;1HSprites: " << sprites.size() << std::endl;
            }
#ifdef __WIIU__
            break;
        case PROCUI_STATUS_RELEASE_FOREGROUND:
            ProcUIDrawDoneRelease();
            break;
        case PROCUI_STATUS_IN_BACKGROUND:
            OSSleepTicks(OSMillisecondsToTicks(20));
            break;
        case PROCUI_STATUS_EXITING:
            toExit = true;
            break;
        }
#endif
        if (toExit) {
            break;
        }
    }

    exitApp();
    return 0;
}
