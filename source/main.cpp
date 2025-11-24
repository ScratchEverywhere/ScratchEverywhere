#include <cstdlib>
#include <interpret.hpp>
#include <menus/mainMenu.hpp>
#include <render.hpp>
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
                return false;
            } else {
                MenuManager::isProjectLoaded = 0;
                return true;
            }
        }

#ifdef __EMSCRIPTEN__
        emscripten_sleep(0);
#endif
    }
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

#ifdef __EMSCRIPTEN__
    if (argc > 1) {
        while (!std::filesystem::exists("/romfs/project.sb3")) {
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
                if (!std::filesystem::exists(OS::getScratchFolderLocation())) std::filesystem::create_directory(OS::getScratchFolderLocation());
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
