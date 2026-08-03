#ifndef LIBRETRO
#include "image.hpp"
#include "translation.hpp"
#include <log.hpp>
#if defined(__EMSCRIPTEN__) && defined(SE_USE_LIBRARY_BUILD)
#undef SE_USE_LIBRARY_BUILD
#endif
#if defined(ENABLE_MENU) && defined(SE_USE_LIBRARY_BUILD)
#undef ENABLE_MENU
#endif
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
#if !defined(SE_USE_LIBRARY_BUILD)
        exit(0);
#endif
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
extern "C" __declspec(dllexport) void scratch_everywhere(const char *sb3) {
#else
extern "C" __attribute__((visibility("default"))) void scratch_everywhere(const char *sb3) {
#endif
#if defined(_WIN32) || defined(_WIN64)
	auto widen = [](std::string str) {
		if (str.empty()) {
			return std::wstring(L"");
		}
		std::size_t wchar_count = str.size() + 1;
		std::vector<wchar_t> buf(wchar_count);
		wchar_count = (std::size_t)MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buf.data(), (int)wchar_count);
		if (!wchar_count) { 
			return std::wstring(L"");
		}
		return std::wstring{buf.data(), wchar_count};
	}
    int argc = 2;
    wchar_t **argv = (wchar_t **)malloc(argc * sizeof(wchar_t *));
    if (argv) {
        for (int i = 0; i < argc; i++) {
			std::wstring wsb3 = widen(sb3);
            const wchar_t *tmp = (!i) ? L"(null)" : wsb3.c_str();
            argv[i] = (wchar_t *)malloc((strlen(tmp) + 1) * sizeof(wchar_t));
            if (argv[i]) {
                wcscpy(argv[i], tmp);
            }
        }
    }
#else
    int argc = 2;
    char **argv = (char **)malloc(argc * sizeof(char *));
    if (argv) {
        for (int i = 0; i < argc; i++) {
            const char *tmp = (!i) ? "(null)" : sb3;
            argv[i] = (char *)malloc((strlen(tmp) + 1) * sizeof(char));
            if (argv[i]) {
            	strcpy(argv[i], tmp);
            }
        }
    }
#endif
#endif
    if (!initApp()) {
        exitApp();
#if !defined(SE_USE_LIBRARY_BUILD)
        return 1;
#else
        for (int i = 0; i < argc; i++) {
            free(argv[i]); 
        }
        free(argv);
		exit(0);
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
#else
            for (int i = 0; i < argc; i++) {
                free(argv[i]); 
            }
            free(argv);
			exit(0);
#endif
        }
    }
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (1)
        mainLoop();
#endif
#if !defined(SE_USE_LIBRARY_BUILD)
	exitApp();
    return 0;
#else
    for (int i = 0; i < argc; i++) {
        free(argv[i]); 
    }
    free(argv);
	exit(0);
#endif
}
#endif
