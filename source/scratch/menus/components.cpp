#include "components.hpp"
#include "../os.hpp"
#include "menus/menuManager.hpp"
#include <algorithm>
#include <cmath>

#ifdef SDL_BUILD
#include "../../sdl/render.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#endif

namespace components {
#ifdef SDL_BUILD
SDL2_Font fonts[2] = {};
#endif

Sidebar::Sidebar() {
    for (const auto &tab : tabs) {
#ifdef SDL_BUILD
        images[tab] = IMG_Load((OS::getRomFSLocation() + "gfx/menu/" + tab + ".svg").c_str());
        if (!images.contains(tab) || !images[tab]) Log::logError("Failed to load image for tab: " + tab);
#endif
    }
}

Sidebar::~Sidebar() {
    for (const auto &tab : tabs) {
#ifdef SDL_BUILD
        if (images.contains(tab) && images[tab]) SDL_FreeSurface(images[tab]);
#endif
    }
}

static MenuID tabToMenuID(const std::string tab) {
    if (tab == "home") return MenuID::MainMenu;
    if (tab == "projects") return MenuID::ProjectsMenu;
    return MenuID::None;
}

constexpr Clay_Color unfocusedTabColor = {90, 60, 90, 255};
constexpr Clay_Color focusedTabColor = {115, 75, 115, 255};

void Sidebar::renderItem(const std::string tab) {
    if (!hoverData.contains(tab)) hoverData[tab] = {menuManager, tab};

    const std::string id = "sidebar_" + tab;
    Clay_String clayId = (Clay_String){false, static_cast<int32_t>(id.length()), id.c_str()};

    const std::string imageId = "sidebar_img_" + tab;
    Clay_String clayImageId = (Clay_String){false, static_cast<int32_t>(imageId.length()), imageId.c_str()};

    if (selected != tab && tabToMenuID(tab) == menuManager->currentMenuID) {
        unSelectedTab = selected;
        animationTimer.start();
        selected = tab;
    }

    float t = std::min(animationTimer.getTimeMs(), static_cast<int>(animationDuration)) / static_cast<float>(animationDuration);

    Clay_Color bgColor = unfocusedTabColor;
    if (selected == tab) {
        bgColor.r = std::lerp(unfocusedTabColor.r, focusedTabColor.r, t);
        bgColor.g = std::lerp(unfocusedTabColor.g, focusedTabColor.g, t);
        bgColor.b = std::lerp(unfocusedTabColor.b, focusedTabColor.b, t);
    } else if (unSelectedTab == tab) {
        bgColor.r = std::lerp(focusedTabColor.r, unfocusedTabColor.r, t);
        bgColor.g = std::lerp(focusedTabColor.g, unfocusedTabColor.g, t);
        bgColor.b = std::lerp(focusedTabColor.b, unfocusedTabColor.b, t);
    }

    float height = 100;
    if (selected == tab) {
        height += std::lerp(0, 25, t);
    } else if (unSelectedTab == tab) {
        height += std::lerp(25, 0, t);
    }

    // clang-format off
	CLAY(CLAY_SID(clayId), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(height) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
		},
		.backgroundColor = bgColor,
		.cornerRadius = {16, 0, 16, 0},
	}) {
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, intptr_t userdata) {
			const auto hoverData = *(const HoverData*)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME && hoverData.menuManager->currentMenuID != tabToMenuID(hoverData.tab)) hoverData.menuManager->changeMenu(tabToMenuID(hoverData.tab));
		}, (intptr_t)&hoverData[tab]);

		if (images.contains(tab) && images[tab]) CLAY(CLAY_SID(clayImageId), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_PERCENT(0.5) }
			},
			.aspectRatio = { 1 },
			.image = { .imageData = images[tab] }	
		});
	}
    // clang-format on
}

void Sidebar::render() {
    // clang-format off
	CLAY(CLAY_ID("sidebar"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIXED(60), .height = CLAY_SIZING_GROW(0) },
			.padding = { 8, 0, 0, 0 },
			.childGap = 10,
			.childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
	}) {
		for (const auto &tab : tabs) renderItem(tab);

#ifdef SDL_BUILD
		bool windowFocused = (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
#elif __3DS__
		bool windowFocused == true;
#endif
	}
    // clang-format on
}
} // namespace components
