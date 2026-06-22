#include "menuManager.hpp"
#include "../unzip.hpp"
#include "components.hpp"
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
#elif defined(RENDERER_CITRO2D)
#include <citro2d.h>
#include <citro3d.h>
#include <renderers/citro2d/clay_renderer.hpp>

extern C3D_RenderTarget *bottomScreen;
extern C3D_RenderTarget *topScreen;
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
    uint64_t clayMinMemory = Clay_MinMemorySize();
    clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayMinMemory, malloc(clayMinMemory));
    Clay_Initialize(clayMemory, {static_cast<float>(Render::getWidth()), static_cast<float>(Render::getHeight())}, {[](Clay_ErrorData errorData) {
                        Log::logError(std::string("[CLAY] ") + errorData.errorText.chars);
                    }});

#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
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
#endif
}

void MenuManager::freeClay() {
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
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
#else
    const int windowWidth = Render::getWidth();
    const int windowHeight = Render::getHeight();
#endif
    Clay_SetLayoutDimensions({static_cast<float>(windowWidth), static_cast<float>(windowHeight)});
    scale = std::sqrt(windowWidth * windowWidth + windowHeight * windowHeight) / 600.0f;
    if (scale > maxScale) scale = maxScale;

#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    SDL_SetRenderDrawColor(renderer, 66, 44, 66, 255);
    SDL_RenderClear(renderer);
#elif defined(RENDERER_CITRO2D)
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(bottomScreen, C2D_Color32(66, 44, 66, 255));
    C2D_TargetClear(topScreen, C2D_Color32(66, 44, 66, 255));
    C2D_SceneBegin(topScreen);
    C2D_SceneBegin(bottomScreen);
#endif
    Clay_BeginLayout();
    // clang-format off
	CLAY(CLAY_ID("outer"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
	}) {
		if (currentMenuID != MenuID::LoadingMenu) {
			sidebar.render();
		}
		currentMenu->render();
	}
    // clang-format on
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3)
    Clay_SDL_Render(renderer, Clay_EndLayout(deltaTimer.getTimeMs()), reinterpret_cast<SDL_Font *>(components::fonts));
    SDL_RenderPresent(renderer);
#elif defined(RENDERER_CITRO2D)
    Clay_Citro2D_Render(bottomScreen, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}, Clay_EndLayout());
    C3D_FrameEnd(0);
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
