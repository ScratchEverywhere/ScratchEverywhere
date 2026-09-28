#ifndef LIBRETRO
#include "image.hpp"
#include "translation.hpp"

#if (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS) && !defined(LIBRETRO)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
#include "hasdeps.hpp"
#include <libdlgmod/libdlgmod.h>
#if !defined(USE_LIBDLGMOD)
#define USE_LIBDLGMOD
#endif
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <sys/stat.h>
#endif

#include <log.hpp>
#ifdef ENABLE_MENU
#include <menus/mainMenu.hpp>
#endif
#include <cstdlib>
#include <inspector.hpp>
#include <menus/mainMenu.hpp>
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
    Render::deInit();
    OS::deinit();
}

static bool initApp() {
    return Scratch::initializeRuntime();
}

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
#ifdef ENABLE_INSPECTOR
        Inspector::processCommands();
#endif
    }
#endif
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

bool in_path = true;
bool hasdeps() {
    return in_path;
}

#if defined(WINDOWING_SDL1) || defined(WINDOWING_SDL2)
#include <SDL.h>

extern "C" int main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
#if defined(USE_LIBDLGMOD) && ((defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS) && !defined(LIBRETRO)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4)))
    in_path = false;
    const char *path = std::getenv("PATH");
    if (path && path[0] != '\0') {

        const char *ptr = std::getenv("XDG_CURRENT_DESKTOP");
        if (ptr && ptr[0] != '\0') {

            std::string str = ptr;
            std::transform(str.begin(), str.end(), str.begin(), ::toupper);

            bool is_qt = (str.find("KDE") != std::string::npos || str.find("TDE") != std::string::npos || 
                str.find("LXQT") != std::string::npos || str.find("RAZOR") != std::string::npos || 
                str.find("CUTEFISH") != std::string::npos || str.find("DEEPIN") != std::string::npos || 
                str.find("DDE") != std::string::npos || str.find("UKUI") != std::string::npos || 
                str.find("LUMINA") != std::string::npos || str.find("QT") != std::string::npos);

            if (is_qt) {
                struct stat st;
                std::string buf;

                std::string cpp_path(path);
                std::stringstream ss(cpp_path);

                char resolved_path[PATH_MAX];
                std::string cmd = "kdialog";

                while (std::getline(ss, buf, ':')) {
                    if (realpath((buf + std::string("/") + cmd).c_str(), resolved_path) && !stat(resolved_path, &st) && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                        // Expected dialog CLI executable exists in path!
                        widget_set_system("KDialog");
                           in_path = true;
                        break;
                    }
                }
                if (!in_path) {
                    struct stat st;
                    std::string buf;

                    std::string cpp_path(path);
                    std::stringstream ss(cpp_path);

                    char resolved_path[PATH_MAX];
                    std::string cmd = "zenity";

                    while (std::getline(ss, buf, ':')) {
                        if (realpath((buf + std::string("/") + cmd).c_str(), resolved_path) && !stat(resolved_path, &st) && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                            // Expected dialog CLI executable exists in path!
                            widget_set_system("Zenity");
                               in_path = true;
                            break;
                        }
                    }
                }
            } else {
                struct stat st;
                std::string buf;

                std::string cpp_path(path);
                std::stringstream ss(cpp_path);

                char resolved_path[PATH_MAX];
                std::string cmd = "zenity";

                while (std::getline(ss, buf, ':')) {
                    if (realpath((buf + std::string("/") + cmd).c_str(), resolved_path) && !stat(resolved_path, &st) && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                        // Expected dialog CLI executable exists in path!
                        widget_set_system("Zenity");
                           in_path = true;
                        break;
                    }
                }
                if (!in_path) {
                    struct stat st;
                    std::string buf;

                    std::string cpp_path(path);
                    std::stringstream ss(cpp_path);

                    char resolved_path[PATH_MAX];
                    std::string cmd = "kdialog";

                    while (std::getline(ss, buf, ':')) {
                        if (realpath((buf + std::string("/") + cmd).c_str(), resolved_path) && !stat(resolved_path, &st) && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
                            // Expected dialog CLI executable exists in path!
                            widget_set_system("KDialog");
                               in_path = true;
                            break;
                        }
                    }
                }
            }
        }
    }
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
