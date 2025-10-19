#include "mainMenu.hpp"
#include "menus/menu.hpp"
#include "os.hpp"
#include <clay_renderer_SDL2.c>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>

#include "menuManager.hpp"

// TODO: 3ds support
#ifdef SDL_BUILD
#include "../../sdl/render.hpp"
#endif

std::unique_ptr<Menu> MenuManager::createMenu(MenuID id) {
    switch (id) {
    case MenuID::MainMenu:
        return std::make_unique<MainMenu>();
    default:
        return nullptr;
    }
}

void MenuManager::changeMenu(MenuID id) {
    if (id == MenuID::None) return;
    if (currentMenuID != MenuID::None) history.push(currentMenuID);
    currentMenuID = id;
    currentMenu = std::move(createMenu(id));
}

void MenuManager::back() {
    if (history.empty()) return;
    currentMenu = std::move(createMenu(history.top()));
    currentMenuID = history.top();
    history.pop();
}

MenuManager::MenuManager() {
    uint64_t totalMemorySize = Clay_MinMemorySize();
    clayMemory = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(clayMemory, {static_cast<float>(windowWidth), static_cast<float>(windowHeight)}, {[](Clay_ErrorData errorData) {
        Log::logError(std::string("[CLAY] ") + errorData.errorText.chars);
    }});

#ifdef SDL_BUILD
    fonts[FONT_ID_BODY_16] = {.fontId = FONT_ID_BODY_16, .font = TTF_OpenFont((OS::getRomFSLocation() + "gfx/menu/RedditSansFudge-Regular.ttf").c_str(), 16)};
    if (!fonts[FONT_ID_BODY_16].font) {
        Log::logError("Failed to load menu font.");
        shouldQuit = true;
        return;
    }
    Clay_SetMeasureTextFunction(SDL2_MeasureText, &fonts);
#endif
}

MenuManager::~MenuManager() {
#ifdef SDL_BUILD
    if (fonts[FONT_ID_BODY_16].font) TTF_CloseFont(fonts[FONT_ID_BODY_16].font);
#endif
    free(clayMemory.memory);
}

void MenuManager::render() {
    Clay_SetLayoutDimensions({(float)windowWidth, (float)windowHeight});

#ifdef SDL_BUILD
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (currentMenuID != MenuID::None) Clay_SDL2_Render(renderer, currentMenu->render(), fonts);
    SDL_RenderPresent(renderer);
#endif
}

void MenuManager::handleInput(float scrollX, float scrollY, float mouseX, float mouseY, bool mouseDown) {
    static Timer frameTimer;

    Clay_SetPointerState({mouseX, mouseY}, mouseDown);
    Clay_UpdateScrollContainers(true, {scrollX, scrollY}, frameTimer.getTimeMs() / 1000.0f);

    frameTimer.start(); // Restart and start are the same so we just use start
}
