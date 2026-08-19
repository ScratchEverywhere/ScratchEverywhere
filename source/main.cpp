#ifndef LIBRETRO
#include "image.hpp"
#include "translation.hpp"
#include <log.hpp>
#ifdef ENABLE_MENU
#include <menus/mainMenu.hpp>
#endif
#include <string>
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
#if defined(_WIN32) || defined(_WIN64) || defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__) && !defined(WEBOS)) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || (defined(__sun) && defined(__SVR4))
#include <libdlgmod/libdlgmod.h>
#if !defined(USE_LIBDLGMOD)
#define USE_LIBDLGMOD
#endif
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__APPLE__)
#include <AppKit/AppKit.h>
#else
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif
#endif
static ScriptThread monitorDisplayThread;
#endif

static void exitApp() {
    Render::deInit();
    OS::deinit();
}

#if defined(SE_USE_LIBRARY_BUILD)
bool scratch_everywhere_is_blocking = false;
std::string scratch_everywhere_parent_window_string = "0";
#if defined(_WIN32) || defined(_WIN64)
WNDPROC OriginalWndProc = nullptr;
LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_CLOSE) {
            Scratch::cleanupScratchProject();
            Render::deInit();
            OS::deinit();
	        exit(0);
            return 0;
        }
        break;
    case WM_CLOSE:
        Scratch::cleanupScratchProject();
        Render::deInit();
        OS::deinit();
	    exit(0);
        return 0;
        break;
    }
    return CallWindowProc(OriginalWndProc, hwnd, msg, wParam, lParam);
}
#elif defined(__APPLE__) && defined(USE_LIBDLGMOD)
@interface WindowDelegate : NSObject <NSWindowDelegate>
@end
@implementation WindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    Scratch::cleanupScratchProject();
    Render::deInit();
    OS::deinit();
	exit(0);
	return YES;
}
@end
#endif
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) void scratch_everywhere_destroy() {
#else
extern "C" __attribute__((visibility("default"))) void scratch_everywhere_destroy() {
#endif
    Scratch::cleanupScratchProject();
    Render::deInit();
    OS::deinit();
	exit(0);
}
/**
 * I returned a string split by a colon delimiter character 
 * because I intend to use this in GameMaker as a GameMaker
 * extension. GameMaker extension functions can only return
 * double, char *, or void. If you want to change the types
 * here, please let me know before you change this behavior
 * -- "samuelvenable" a.k.a. "high on tantor" on github.com
 */
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) char *scratch_everywhere_step() {
#else
extern "C" __attribute__((visibility("default"))) char *scratch_everywhere_step() {
#endif
    static char buffer[4];
    std::pair<bool, bool> code = Scratch::stepScratchProject(monitorDisplayThread);
    const int first  = ((code.first)  ? 1 : 0);
    const int second = ((code.second) ? 1 : 0);
    snprintf(buffer, sizeof(buffer), "%d:%d", first, second);
    return (char *)buffer;
}
#if defined(USE_LIBDLGMOD)
#if !defined(_WIN32) && !defined(_WIN64) && !defined(__APPLE__)
static int XErrorHandlerImpl(Display *display, XErrorEvent *event) {
  return 0;
}
static int XIOErrorHandlerImpl(Display *display) {
  return 0;
}
#endif
#endif
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) double scratch_everywhere_get_is_blocking() {
#else
extern "C" __attribute__((visibility("default"))) double scratch_everywhere_get_is_blocking() {
#endif
	return scratch_everywhere_is_blocking;
}
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) void scratch_everywhere_set_is_blocking(double blocking) {
#else
extern "C" __attribute__((visibility("default"))) void scratch_everywhere_set_is_blocking(double blocking) {
#endif
	scratch_everywhere_is_blocking = (bool)(int)blocking;
}
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) char *scratch_everywhere_get_parent_window() {
#else
extern "C" __attribute__((visibility("default"))) char *scratch_everywhere_get_parent_window() {
#endif
	return (char *)scratch_everywhere_parent_window_string.c_str();
}
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) void scratch_everywhere_set_parent_window(char *window) {
#else
extern "C" __attribute__((visibility("default"))) void scratch_everywhere_set_parent_window(char *window) {
#endif
	scratch_everywhere_parent_window_string = window;
#if defined(USE_LIBDLGMOD)
#if defined(_WIN32) || defined(_WIN64)
	HWND scratch_everywhere_window = (HWND)(void *)strtoull(widget_get_owner(), nullptr, 10);
	HWND scratch_everywhere_parent_window = (HWND)(void *)strtoull(window, nullptr, 10);
    if (IsIconic(scratch_everywhere_parent_window)) ShowWindow(scratch_everywhere_parent_window, SW_RESTORE);
	SetWindowLongPtrW(scratch_everywhere_window, GWLP_HWNDPARENT, (LONG_PTR)(void *)scratch_everywhere_parent_window);
    SetWindowLongPtrW(scratch_everywhere_parent_window, GWL_STYLE, (GetWindowLongPtrW(scratch_everywhere_parent_window, GWL_STYLE) | WS_CLIPCHILDREN | WS_CLIPSIBLINGS) & ~(WS_THICKFRAME | WS_MAXIMIZEBOX));
	SetWindowLongPtrW(scratch_everywhere_window, GWL_STYLE, (GetWindowLongPtrW(scratch_everywhere_window, GWL_STYLE) | WS_CHILD) & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_POPUP));
	SetWindowLongPtrW(scratch_everywhere_window, GWL_EXSTYLE, GetWindowLongPtrW(scratch_everywhere_window, GWL_EXSTYLE) & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
    SetWindowPos(scratch_everywhere_parent_window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	SetWindowPos(scratch_everywhere_window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	RECT rect; GetClientRect(scratch_everywhere_parent_window, &rect); SetParent(scratch_everywhere_window, scratch_everywhere_parent_window);
    OriginalWndProc = (WNDPROC)SetWindowLongPtrW(scratch_everywhere_parent_window, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);
    MoveWindow(scratch_everywhere_window, 0, 0, rect.right, rect.bottom, TRUE);
#elif defined(__APPLE__)
	NSWindow *scratch_everywhere_window = (NSWindow *)(void *)strtoull(widget_get_owner(), nullptr, 10);
	NSWindow *scratch_everywhere_parent_window = (NSWindow *)(void *)strtoull(window, nullptr, 10);
	[scratch_everywhere_parent_window addChildWindow:scratch_everywhere_window ordered:NSWindowAbove];
	[scratch_everywhere_window setStyleMask:NSWindowStyleMaskBorderless]; NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown location:
	NSMakePoint(scratch_everywhere_window.frame.size.width / 2, scratch_everywhere_window.frame.size.height / 2) modifierFlags:0 timestamp:0 windowNumber:
	[scratch_everywhere_window windowNumber] context:nullptr eventNumber:0 clickCount:1 pressure:1.0]; [scratch_everywhere_window sendEvent:event];
	NSPoint origin = scratch_everywhere_parent_window.frame.origin; NSSize size = scratch_everywhere_parent_window.contentView.bounds.size; 
	[scratch_everywhere_window setFrame:NSMakeRect(origin.x, origin.y, size.width, size.height) display:YES animate:NO];
	[[scratch_everywhere_parent_window standardWindowButton:NSWindowZoomButton] setEnabled:NO];
	scratch_everywhere_parent_window.styleMask &= ~NSWindowStyleMaskResizable;
	WindowDelegate *delegate = [[WindowDelegate alloc] init];
	[scratch_everywhere_parent_window setDelegate:delegate];
#else
  	XSetErrorHandler(XErrorHandlerImpl); 
    XSetIOErrorHandler(XIOErrorHandlerImpl); Display *display = XOpenDisplay(nullptr);
	Window scratch_everywhere_window = (Window)strtoul(widget_get_owner(), nullptr, 10);
	Window scratch_everywhere_parent_window = (Window)strtoul(window, nullptr, 10);
	XSetTransientForHint(display, scratch_everywhere_window, scratch_everywhere_parent_window);
	XReparentWindow(display, scratch_everywhere_window, scratch_everywhere_parent_window, 0, 0);
	XWindowAttributes attr; XGetWindowAttributes(display, scratch_everywhere_parent_window, &attr);
	XResizeWindow(display, scratch_everywhere_window, attr.width, attr.height); XSizeHints *sh = 
    XAllocSizeHints(); sh->flags = PMinSize | PMaxSize; sh->min_width = sh->max_width = attr.width;
	sh->min_height = sh->max_height = attr.height; XSetWMNormalHints(display, 
    scratch_everywhere_parent_window, sh); XFree(sh); XCloseDisplay(display);
#endif
#endif
}
#endif

static bool initApp(int width, int height, bool resizable, std::string title) {
    return Scratch::initializeRuntime(width, height, resizable, title);
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

        if (Unzip::projectOpened != -3) {
            exitApp();
            exit(0);
        }

#if defined(ENABLE_MENU)
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
    if (!initApp(-1, -1, true, "Scratch Everywhere!")) {
#else
/**
 * I use double arguments for width / height instead of int 
 * because I intend to use this in GameMaker as a GameMaker
 * extension. GameMaker extension arguments are limited to: 
 * double, char *, or void. If you want to change the types
 * here, please let me know before you change this behavior
 * -- "samuelvenable" a.k.a. "high on tantor" on github.com
 */
#if defined(_WIN32) || defined(_WIN64)
extern "C" __declspec(dllexport) char *scratch_everywhere_create(char *sb3, double width, double height, char *title) {
#else
extern "C" __attribute__((visibility("default"))) char *scratch_everywhere_create(char *sb3, double width, double height, char *title) {
#endif
    if (!initApp((int)width, (int)height, false, title)) {
#endif
#if !defined(SE_USE_LIBRARY_BUILD)
        exitApp();
        return 1;
#endif
    }

    srand(time(nullptr));

    bool enableInspector = false;
#if !defined(SE_USE_LIBRARY_BUILD)
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
#endif

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
#if defined(ENABLE_MENU)
            if (!activateMainMenu()) {
                exitApp();
                return 0;
            }
#endif
#endif
        } else {
#if !defined(SE_USE_LIBRARY_BUILD)
            exitApp();
            return 0;
#endif
        }
    }

#if defined(SE_USE_LIBRARY_BUILD)
    Unzip::filePath = sb3;
    Unzip::load();
    Scratch::initializeScratchProject();
#if defined(USE_LIBDLGMOD)
	scratch_everywhere_set_parent_window((char *)scratch_everywhere_parent_window_string.c_str());
#endif
	if (scratch_everywhere_is_blocking) {
		while (true) {
			std::pair<bool, bool> code = Scratch::stepScratchProject(monitorDisplayThread);
    		if (!code.first) {
				Scratch::cleanupScratchProject();
				exitApp();
				exit(0);
				break;
			}
		}
	}
#if defined(USE_LIBDLGMOD)
	return (char *)widget_get_owner();
#else
	return (char *)"0";
#endif
#else
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (true)
        mainLoop();
#endif
    exitApp();
    return 0;
#endif
}
#endif
