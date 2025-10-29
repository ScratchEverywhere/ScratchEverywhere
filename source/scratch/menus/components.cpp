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
#elif defined(__3DS__)
std::map<unsigned int, C2D_Font> fonts;
#endif

uint16_t FONT_ID_BODY_16 = 0;
uint16_t FONT_ID_BODY_BOLD_48 = 1;

Sidebar::Sidebar() {
    for (const auto &tab : tabs)
        images[tab] = std::make_unique<Image>("gfx/menu/" + tab + ".svg");
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

    float height = 1.0f / tabs.size();
    height -= (1.0f / (tabs.size() * 3)) / tabs.size();
    if (selected == tab) {
        height += std::lerp(0, 1.0f / (tabs.size() * 3), t);
    } else if (unSelectedTab == tab) {
        height += std::lerp(1.0f / (tabs.size() * 3), 0, t);
    }

    // clang-format off
	CLAY(CLAY_SID(clayId), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(height) },
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
		},
		.backgroundColor = bgColor,
		.cornerRadius = {16 * menuManager->scale, 0, 16 * menuManager->scale, 0},
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
			.image = { .imageData = MenuManager::getImageData(images[tab].get()) }	
		});
	}
    // clang-format on
}

void Sidebar::render() {
    // clang-format off
	CLAY(CLAY_ID("sidebar"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIXED(60.0f * menuManager->scale), .height = CLAY_SIZING_GROW(0) },
			.padding = { static_cast<uint16_t>(8 * menuManager->scale), 0, static_cast<uint16_t>(15 * menuManager->scale), static_cast<uint16_t>(15 * menuManager->scale) },
			.childGap = static_cast<uint16_t>(10 * menuManager->scale),
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
    const uint16_t padding = 10 * menuManager->scale;

    // clang-format off
    CLAY(CLAY_IDI("project-list-item", i), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = width, .height = CLAY_SIZING_FIXED(60 * menuManager->scale) },
			.padding = {padding, padding, padding, padding},
			.childGap = 5,
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		},
		.backgroundColor = {225, 225, 235, 255},
		.cornerRadius = {10 * menuManager->scale, 10 * menuManager->scale, 10 * menuManager->scale, 10 * menuManager->scale},
		.border = { .color = {90, 60, 90, 255}, .width = {static_cast<uint16_t>(5 * menuManager->scale), static_cast<uint16_t>(5 * menuManager->scale), static_cast<uint16_t>(5 * menuManager->scale), static_cast<uint16_t>(5 * menuManager->scale)} }
	}) {
		if (i <= projectHoverData.size()) projectHoverData.push_back({ menuManager, &projectInfo });
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, intptr_t userdata) {
			if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
			const auto projectHoverData = (const ProjectHoverData*)userdata;
			projectHoverData->menuManager->launchProject(projectHoverData->projectInfo->path);
		}, (intptr_t)&projectHoverData[i]);

		if (image) {
			CLAY(CLAY_IDI("project-list-item-img", i), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(Clay_GetElementData(CLAY_IDI("project-list-item", i)).boundingBox.height - 2 * padding) }
				},
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
			static constexpr uint16_t minFontSize = 16;
			uint16_t fontSize = 12 * menuManager->scale;
			if (fontSize < minFontSize) fontSize = minFontSize;
			const Clay_String clayName = {false, static_cast<int32_t>(projectInfo.name.length()), projectInfo.name.c_str()};
			CLAY_TEXT(clayName, CLAY_TEXT_CONFIG({.textColor = {0, 0, 0, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));
		}
	}
    // clang-format on
}
} // namespace components
