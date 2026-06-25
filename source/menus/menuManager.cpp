#include "menuManager.hpp"
#include "../unzip.hpp"
#include "components.hpp"
#include "confirmationMenu.hpp"
#include "controlsMenu.hpp"
#include "input.hpp"
#include "languageMenu.hpp"
#include "loading.hpp"
#include "log.hpp"
#include "mainMenu.hpp"
#include "menu.hpp"
#include "projectSettingsMenu.hpp"
#include "projectsMenu.hpp"
#include "settingsMenu.hpp"
#include "timer.hpp"
#include "unpackMenu.hpp"
#include "window.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <render.hpp>
#include <string>
#include <utility>

#ifdef RENDERER_SDL2
#include <renderers/sdl2/clay_renderer.hpp>
#include <renderers/sdl2/render.hpp>
#elif defined(RENDERER_SDL3)
#include <renderers/sdl3/clay_renderer.hpp>
#include <renderers/sdl3/render.hpp>
#elif defined(RENDERER_SDL1)
#include <renderers/sdl1/clay_renderer.hpp>
#include <renderers/sdl1/render.hpp>
#elif defined(RENDERER_CITRO2D)
#include <citro2d.h>
#include <citro3d.h>
#include <renderers/citro2d/clay_renderer.hpp>

extern C3D_RenderTarget *bottomScreen;
extern C3D_RenderTarget *topScreen;
#elif defined(RENDERER_GL2D)
#include <gl2d.h>
#include <nds.h>
#include <renderers/gl2d/clay_renderer.hpp>
#elif defined(RENDERER_OPENGL)
#include <GL/gl.h>
#include <renderers/opengl/clay_renderer.hpp>
#endif

Clay_Arena MenuManager::clayMemory;

std::unique_ptr<Menu> MenuManager::createMenu(MenuID id, void *userdata) {
    switch (id) {
    case MenuID::MainMenu:
        return std::make_unique<MainMenu>(userdata);
    case MenuID::ProjectsMenu:
        return std::make_unique<ProjectsMenu>(userdata);
    case MenuID::GlobalSettingsMenu:
        return std::make_unique<GlobalSettingsMenu>(userdata);
    case MenuID::ProjectSettingsMenu:
        return std::make_unique<ProjectSettingsMenu>(userdata);
    case MenuID::ControlsMenu:
        return std::make_unique<ControlsMenu>(userdata);
    case MenuID::UnpackMenu:
        return std::make_unique<UnpackMenu>(userdata);
    case MenuID::LanguageMenu:
        return std::make_unique<LanguageMenu>(userdata);
    case MenuID::LoadingMenu:
        return std::make_unique<LoadingMenu>(userdata);
    case MenuID::ConfirmationMenu:
        return std::make_unique<ConfirmationMenu>(userdata);
    default:
        return nullptr;
    }
}

void MenuManager::queueChangeMenu(MenuID id, void *userdata) {
    menuQueue = {id, userdata};
}

void MenuManager::changeMenu(MenuID id, void *userdata) {
    if (id == MenuID::None) return;
    if (currentMenuID != MenuID::None) history.push(currentMenuID);
    currentMenuID = id;
    currentMenu = std::move(createMenu(id, userdata));
    currentMenu->menuManager = this;
}

bool MenuManager::launchProject(const std::string path) {
    Unzip::filePath = path;
    if (!Unzip::load()) {
        Log::logError("Failed to load project '" + path + "', closing app.");
        return false;
    }
    return true;
}

void MenuManager::back(void *userdata) {
    if (history.empty()) return;
    currentMenu = std::move(createMenu(history.top(), userdata));
    currentMenu->menuManager = this;
    currentMenuID = history.top();
    history.pop();
}

void MenuManager::queueBack(void *userdata) {
    if (history.empty()) return;
    backQueued = true;
    backUserdata = userdata;
}

MenuManager::MenuManager() {
    Input::applyControls();
    sidebar.menuManager = this;
}

void MenuManager::initClay() {
    // Clay_MinMemorySize() allocates ~6MB of RAM by default...
    // limiting these makes it allocate much much less
    // GL2D can only do ~100 draw calls per frame anyway so it's chill
#ifdef __NDS__
    Clay_SetMaxElementCount(100);
    Clay_SetMaxMeasureTextCacheWordCount(30);
#endif

    uint64_t clayMinMemory = Clay_MinMemorySize();
    clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayMinMemory, malloc(clayMinMemory));
    Clay_Initialize(clayMemory, {static_cast<float>(Render::getWidth()), static_cast<float>(Render::getHeight())}, {[](Clay_ErrorData errorData) {
                        Log::logError(std::string("[CLAY] ") + errorData.errorText.chars);
                    }});

#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3) || defined(RENDERER_SDL1)
#ifdef RENDERER_SDL3
#define TTF_GetError SDL_GetError
#endif

    components::fonts[components::FONT_ID_BODY_16] = {.fontId = components::FONT_ID_BODY_16, .font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/RedditSansFudge-Regular.ttf").c_str(), 16)};
    if (!components::fonts[components::FONT_ID_BODY_16].font) Log::logError(std::string("Failed to load menu font: ") + TTF_GetError());

    components::fonts[components::FONT_ID_BODY_BOLD_48] = {.fontId = components::FONT_ID_BODY_BOLD_48, .font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/RedditSansFudge-Bold.ttf").c_str(), 48)};
    if (!components::fonts[components::FONT_ID_BODY_BOLD_48].font) Log::logError(std::string("Failed to load bold menu font: ") + TTF_GetError());

    Clay_SetMeasureTextFunction(SDL_MeasureText, &components::fonts);

#ifdef RENDERER_SDL3
#undef TTF_GetError
#endif
#elif defined(RENDERER_CITRO2D)
    C2D_Font font = C2D_FontLoad("romfs:/gfx/menu/RedditSansFudge-Regular.bcfnt");
    unsigned int fontId = Clay_Citro2D_RegisterFont(font);
    if (fontId == Clay_Citro2D_FONT_INVALID) Log::logError("Failed to load menu font.");
    else {
        components::fonts[fontId] = font;
        components::FONT_ID_BODY_16 = fontId;
    }

    font = C2D_FontLoad("romfs:/gfx/menu/RedditSansFudge-Bold.bcfnt");
    fontId = Clay_Citro2D_RegisterFont(font);
    if (fontId == Clay_Citro2D_FONT_INVALID) Log::logError("Failed to load bold menu font.");
    else {
        components::fonts[fontId] = font;
        components::FONT_ID_BODY_BOLD_48 = fontId;
    }

    Clay_SetMeasureTextFunction(Clay_Citro2D_MeasureText, nullptr);
#elif defined(RENDERER_GL2D)
    // ubuntu bold is the default font
    if (!Clay_GL2D_RegisterFont("")) {
        Log::logError("Failed to load menu font.");
    }
    if (!Clay_GL2D_RegisterFont("")) {
        Log::logError("Failed to load bold menu font.");
    }
    Clay_SetMeasureTextFunction(Clay_GL2D_MeasureText, nullptr);
#elif defined(RENDERER_OPENGL)
    if (!Clay_OpenGL_RegisterFont("gfx/menu/RedditSansFudge-Regular")) {
        Log::logError("Failed to load menu font.");
    }
    if (!Clay_OpenGL_RegisterFont("gfx/menu/RedditSansFudge-Bold")) {
        Log::logError("Failed to load bold menu font.");
    }
    Clay_SetMeasureTextFunction(Clay_OpenGL_MeasureText, nullptr);
#endif
}

void MenuManager::freeClay() {
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3) || defined(RENDERER_SDL1)
    if (components::fonts[components::FONT_ID_BODY_16].font) TTF_CloseFont(components::fonts[components::FONT_ID_BODY_16].font);
    if (components::fonts[components::FONT_ID_BODY_BOLD_48].font) TTF_CloseFont(components::fonts[components::FONT_ID_BODY_BOLD_48].font);
#elif defined(RENDERER_CITRO2D)
    for (auto &font : components::fonts)
        C2D_FontFree(font.second);
#endif
    free(clayMemory.memory);
}

void MenuManager::render() {
    static Timer deltaTimer;

    static constexpr float maxScale = 2;

    if (menuQueue.first != MenuID::None) {
        if (Input::mousePointer.isPressed) {
            waitForRelease = true;
        }

        Clay_SetPointerState({-9999, -9999}, false);
        changeMenu(menuQueue.first, menuQueue.second);
        menuQueue = {MenuID::None, nullptr};
    } else if (backQueued) {
        if (Input::mousePointer.isPressed) {
            waitForRelease = true;
        }

        Clay_SetPointerState({-9999, -9999}, false);
        back(backUserdata);
        backQueued = false;
    }

#ifdef RENDERER_CITRO2D
    constexpr unsigned int windowWidth = 320;
    constexpr unsigned int windowHeight = 240;
#elif defined(RENDERER_GL2D)
    constexpr unsigned int windowWidth = 256;
    constexpr unsigned int windowHeight = 192;
#else
    const int windowWidth = Render::getWidth();
    const int windowHeight = Render::getHeight();
#endif
    Clay_SetLayoutDimensions({static_cast<float>(windowWidth), static_cast<float>(windowHeight)});
#ifdef RENDERER_GL2D
    scale = 0.75f;
#else
    scale = std::sqrt(windowWidth * windowWidth + windowHeight * windowHeight) / 600.0f;
    if (scale > maxScale) scale = maxScale;
#endif

#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    SDL_SetRenderDrawColor(renderer, 66, 44, 66, 255);
    SDL_RenderClear(renderer);
#elif defined(RENDERER_SDL1)
    SDL_Surface *window = static_cast<SDL_Surface *>(Render::getRenderer());

    Uint32 clearColor = SDL_MapRGBA(window->format, 66, 44, 66, 255);
    SDL_FillRect(window, NULL, clearColor);
#elif defined(RENDERER_CITRO2D)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(bottomScreen, C2D_Color32(66, 44, 66, 255));
    C2D_TargetClear(topScreen, C2D_Color32(66, 44, 66, 255));
    C2D_SceneBegin(topScreen);
    C2D_SceneBegin(bottomScreen);
#elif defined(RENDERER_GL2D)
    glBegin2D();
    int r5 = 66 >> 3;
    int g5 = 44 >> 3;
    int b5 = 66 >> 3;
    glClearColor(r5, g5, b5, 31);
    lcdMainOnBottom();
#elif defined(RENDERER_OPENGL)
    glClearColor(66.0f / 255.0f, 44.0f / 255.0f, 66.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif
    Clay_BeginLayout();
    // clang-format off
	CLAY(CLAY_ID("outer"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	}) {
		if (currentMenuID != MenuID::LoadingMenu && currentMenuID != MenuID::ConfirmationMenu) {
			sidebar.render();
		}
		currentMenu->render();
	}

    // clang-format on
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    Clay_SDL_Render(renderer, Clay_EndLayout(deltaTimer.getTimeMsDouble() / 1000.0f), reinterpret_cast<SDL_Font *>(components::fonts));
    SDL_RenderPresent(renderer);
#elif defined(RENDERER_SDL1)
    Clay_SDL_Render(window, Clay_EndLayout(deltaTimer.getTimeMsDouble() / 1000.0f), reinterpret_cast<SDL_Font *>(components::fonts));

    if (SDL_Flip(window) < 0) {
        Log::logError(std::string("Failed to flip screen buffer: ") + SDL_GetError());
    }
#elif defined(RENDERER_CITRO2D)
    Clay_Citro2D_Render(bottomScreen, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}, Clay_EndLayout(deltaTimer.getTimeMsDouble() / 1000.0f));
    C3D_FrameEnd(0);
#elif defined(RENDERER_GL2D)
    Clay_GL2D_Render({static_cast<float>(windowWidth), static_cast<float>(windowHeight)}, Clay_EndLayout(deltaTimer.getTimeMsDouble() / 1000.0f));
    glEnd2D();
    glFlush(GL_TRANS_MANUALSORT);
#elif defined(RENDERER_OPENGL)
    Clay_Dimensions dimensions = {static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    Clay_RenderCommandArray renderCommands = Clay_EndLayout(deltaTimer.getTimeMsDouble() / 1000.0f);

    Clay_OpenGL_Render(dimensions, renderCommands);

    if (globalWindow) globalWindow->swapBuffers();
#endif

    deltaTimer.start();
}

void MenuManager::onResize() {
    if (currentMenu) currentMenu->onResize();
}

void MenuManager::handleInput(float mouseX, float mouseY, bool mouseDown) {
    static Timer frameTimer;

    if (waitForRelease) {
        if (!mouseDown) {
            waitForRelease = false;
        }

        Clay_SetPointerState({-9999, -9999}, false);
        return;
    }

    Clay_SetPointerState({mouseX, mouseY}, mouseDown);
    Clay_UpdateScrollContainers(true, {Input::scrollDelta[0], Input::scrollDelta[1]}, frameTimer.getTimeMs() / 1000.0f);

    frameTimer.start(); // Restart and start are the same so we just use start
}
