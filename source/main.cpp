#ifndef LIBRETRO
#include "image.hpp"
#include "translation.hpp"
#include <log.hpp>
#ifdef ENABLE_MENU
#include <menus/mainMenu.hpp>
#endif
#include <cstdlib>
#include <inspector.hpp>
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

#if defined(SE_USE_LIBRARY_BUILD)
static ScriptThread monitorDisplayThread;
#endif

#if !defined(SE_USE_LIBRARY_BUILD)
static void exitApp() {
#else
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) void scratch_everywhere_destroy() {
#else
extern "C" __attribute__((visibility("default"))) void scratch_everywhere_destroy() {
#endif
    Render::deInit();
    OS::deinit();
}
#if defined(_WIN32) || defined(_WIN64)
/**
 * I returned a string split by a colon delimiter character 
 * because I intend to use this in GameMaker as a GameMaker
 * extension. GameMaker extension functions can only return
 * double, const char *, char *, or void. If you don't like
 * this, please let me know before you change this behavior
 * -- "samuelvenable" a.k.a. "high on tantor" on github.com
 */
extern "C" __declspec(dllexport) const char *scratch_everywhere_step() {
#else
extern "C" __attribute__((visibility("default"))) const char *scratch_everywhere_step() {
#endif
	static char buffer[4];
	std::pair<bool, bool> result = stepScratchProject(monitorDisplayThread);
	bool first  = result.first  ? 1 : 0;
	bool second = result.second ? 1 : 0;
    snprintf(buffer, sizeof(buffer), "%s:%s", first, second);
	return static_cast<const char *>(buffer);
}
#endif

static bool initAppDone = false;
static bool initApp() {
	if (!initAppDone) {
		initAppDone = true;
    	return Scratch::initializeRuntime();
	}
	return initAppDone;
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

#if defined(ENABLE_MENU)
        if (Unzip::projectOpened != -3) { // main menu
            exitApp();
            exit(0);
        }
        if (!activateMainMenu()) {
            exitApp();
            exit(0);
        }
#endif

    skipCheck:
        return;
    }

    Unzip::filePath = "";
    Scratch::nextProject = false;
    Scratch::dataNextProject = Value();
#if defined(ENABLE_MENU)
    if (OS::toExit || !activateMainMenu()) {
#else
    if (OS::toExit) {
#endif
        exitApp();
        exit(0);
    }
}

#if !defined(SE_USE_LIBRARY_BUILD)
#if defined(WINDOWING_SDL1) || defined(WINDOWING_SDL2)
#include <SDL.h>
extern "C" int main(int argc, char **argv) {
#else
int main(int argc, char **argv) {
#endif
#else
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) void scratch_everywhere_create(const char *sb3) {
#else
extern "C" __attribute__((visibility("default"))) void scratch_everywhere_create(const char *sb3) {
#endif
#endif
    if (!initApp()) {
        exitApp();
#if !defined(SE_USE_LIBRARY_BUILD)
        return 1;
#endif
    }

    srand(time(nullptr));

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
#if defined(ENABLE_MENU) && !defined(SE_USE_LIBRARY_BUILD)
            if (!activateMainMenu()) {
                exitApp();
                return 0;
            }
#endif
#endif
        } else {
            exitApp();
#if !defined(SE_USE_LIBRARY_BUILD)
            return 0;
#endif
        }
    }

#if !defined(SE_USE_LIBRARY_BUILD)
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (1)
        mainLoop();
#endif
#else
	Unzip::filePath = sb3;
	Unzip::load();
	Scratch::startScratchProject();
#endif
#if !defined(SE_USE_LIBRARY_BUILD)
	exitApp();
    return 0;
#else
#endif
}
#endif
