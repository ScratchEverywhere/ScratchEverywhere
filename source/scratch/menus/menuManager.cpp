#include "menuManager.hpp"
#include "../unzip.hpp"
#include "components.hpp"
#include "mainMenu.hpp"
#include "menu.hpp"
#include "os.hpp"
#include "projectsMenu.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#ifdef SDL_BUILD
#include "../../sdl/render.hpp"
#include <clay_renderer_SDL2.c>
#elif defined(__3DS__)
#include "../../3ds/image.hpp"
#include <citro2d.h>
#include <citro3d.h>
#include <clay3ds.h>

constexpr unsigned int windowWidth = 320;
constexpr unsigned int windowHeight = 240;

extern C3D_RenderTarget *bottomScreen;
extern C3D_RenderTarget *topScreen;
#endif

Clay_Arena MenuManager::clayMemory;

std::unique_ptr<Menu> MenuManager::createMenu(MenuID id) {
    switch (id) {
    case MenuID::MainMenu:
        return std::make_unique<MainMenu>();
    case MenuID::ProjectsMenu:
        return std::make_unique<ProjectsMenu>();
    default:
        return nullptr;
    }
}

void MenuManager::changeMenu(MenuID id) {
    if (id == MenuID::None) return;
    if (currentMenuID != MenuID::None) history.push(currentMenuID);
    currentMenuID = id;
    currentMenu = std::move(createMenu(id));
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

void MenuManager::back() {
    if (history.empty()) return;
    currentMenu = std::move(createMenu(history.top()));
    currentMenu->menuManager = this;
    currentMenuID = history.top();
    history.pop();
}

MenuManager::MenuManager() {
    sidebar.menuManager = this;
}

void MenuManager::initClay() {
    uint64_t clayMinMemory = Clay_MinMemorySize();
    clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayMinMemory, malloc(clayMinMemory));
    Clay_Initialize(clayMemory, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}, {[](Clay_ErrorData errorData) {
                        Log::logError(std::string("[CLAY] ") + errorData.errorText.chars);
                    }});

#ifdef SDL_BUILD
    components::fonts[components::FONT_ID_BODY_16] = {.fontId = components::FONT_ID_BODY_16, .font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/RedditSansFudge-Regular.ttf").c_str(), 16)};
    if (!components::fonts[components::FONT_ID_BODY_16].font) Log::logError(std::string("Failed to load menu font: ") + TTF_GetError());

    components::fonts[components::FONT_ID_BODY_BOLD_48] = {.fontId = components::FONT_ID_BODY_BOLD_48, .font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/RedditSansFudge-Bold.ttf").c_str(), 48)};
    if (!components::fonts[components::FONT_ID_BODY_BOLD_48].font) Log::logError(std::string("Failed to load bold menu font: ") + TTF_GetError());

    Clay_SetMeasureTextFunction(SDL2_MeasureText, &components::fonts);
#elif defined(__3DS__)
    C2D_Font font = C2D_FontLoad("romfs:/gfx/menu/RedditSansFudge-Regular.bcfnt");
    unsigned int fontId = Clay3DS_RegisterFont(font);
    if (fontId == Clay3DS_FONT_INVALID) Log::logError("Failed to load menu font.");
    else {
        components::fonts[fontId] = font;
        components::FONT_ID_BODY_16 = fontId;
    }

    font = C2D_FontLoad("romfs:/gfx/menu/RedditSansFudge-Bold.bcfnt");
    fontId = Clay3DS_RegisterFont(font);
    if (fontId == Clay3DS_FONT_INVALID) Log::logError("Failed to load bold menu font.");
    else {
        components::fonts[fontId] = font;
        components::FONT_ID_BODY_BOLD_48 = fontId;
    }

    Clay_SetMeasureTextFunction(Clay3DS_MeasureText, nullptr);
#endif
}

void MenuManager::freeClay() {
#ifdef SDL_BUILD
    if (components::fonts[components::FONT_ID_BODY_16].font) TTF_CloseFont(components::fonts[components::FONT_ID_BODY_16].font);
    if (components::fonts[components::FONT_ID_BODY_BOLD_48].font) TTF_CloseFont(components::fonts[components::FONT_ID_BODY_BOLD_48].font);
#elif defined(__3DS__)
    for (auto &font : components::fonts)
        C2D_FontFree(font.second);
#endif
    free(clayMemory.memory);
}

void MenuManager::render() {
    static constexpr float maxScale = 2;

#ifdef SDL_BUILD
    SDL_GetWindowSizeInPixels(window, &windowWidth, &windowHeight);
#endif
    Clay_SetLayoutDimensions({static_cast<float>(windowWidth), static_cast<float>(windowHeight)});
    scale = std::sqrt(windowWidth * windowWidth + windowHeight * windowHeight) / 600.0f;
    if (scale > maxScale) scale = maxScale;

#ifdef SDL_BUILD
    SDL_SetRenderDrawColor(renderer, 66, 44, 66, 255);
    SDL_RenderClear(renderer);
#elif defined(__3DS__)
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
		sidebar.render();
		currentMenu->render();
	}
    // clang-format on
#ifdef SDL_BUILD
    Clay_SDL2_Render(renderer, Clay_EndLayout(), components::fonts);
    SDL_RenderPresent(renderer);
#elif defined(__3DS__)
    Clay3DS_Render(bottomScreen, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}, Clay_EndLayout());
    C3D_FrameEnd(0);
#endif
}

void MenuManager::handleInput(float scrollX, float scrollY, float mouseX, float mouseY, bool mouseDown) {
    static Timer frameTimer;

    Clay_SetPointerState({mouseX, mouseY}, mouseDown);
    Clay_UpdateScrollContainers(true, {scrollX, scrollY}, frameTimer.getTimeMs() / 1000.0f);

    frameTimer.start(); // Restart and start are the same so we just use start
}
