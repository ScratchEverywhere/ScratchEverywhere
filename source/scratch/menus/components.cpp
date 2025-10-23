#include "components.hpp"
#include "../os.hpp"
#include "menus/menuManager.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

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
	}
    // clang-format on
}

std::vector<ProjectHoverData> projectHoverData;

void renderProjectListItem(const ProjectInfo &projectInfo, void *image, unsigned int i, Clay_SizingAxis width, float textScroll, MenuManager *menuManager) {
    static constexpr unsigned int padding = 10;

    // clang-format off
    CLAY(CLAY_IDI("project-list-item", i), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = width, .height = CLAY_SIZING_FIXED(60) },
			.padding = {padding, padding, padding, padding},
			.childGap = 5,
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		},
		.backgroundColor = {225, 225, 235, 255},
		.cornerRadius = {10, 10, 10, 10},
		.border = { .color = {90, 60, 90, 255}, .width = {5, 5, 5, 5} }
	}) {
		if (i <= projectHoverData.size()) projectHoverData.push_back({ menuManager, &projectInfo });
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, intptr_t userdata) {
			if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
			const auto projectHoverData = (const ProjectHoverData*)userdata;
			Log::log(std::to_string((uint64_t)projectHoverData->projectInfo));
			projectHoverData->menuManager->launchProject(projectHoverData->projectInfo->path);
		}, (intptr_t)&projectHoverData[i]);

		if (image) {
			CLAY(CLAY_IDI("project-list-item-img", i), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(Clay_GetElementData(CLAY_IDI("project-list-item", i)).boundingBox.height - 2 * padding) }
				},
				.cornerRadius = {5, 5, 5, 5}, // I don't think any renderers support this...
				.aspectRatio = {1},
				.image = {.imageData = image},
			});
		}
		CLAY(CLAY_IDI("project-list-item-text-wrapper", i), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_GROW(0) }
			},
			.clip = { .horizontal = true, .childOffset = {textScroll, 0} }
		}) {
			const Clay_String clayName = {false, static_cast<int32_t>(projectInfo.name.length()), projectInfo.name.c_str()};
			CLAY_TEXT(clayName, CLAY_TEXT_CONFIG({.textColor = {0, 0, 0, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = 12}));
		}
	}
    // clang-format on
}
} // namespace components
