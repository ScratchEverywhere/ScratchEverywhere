#include "components.hpp"
#include "../os.hpp"
#include "image.hpp"
#include "input.hpp"
#include "menus/menuManager.hpp"
#include <algorithm>
#include <cmath>
#include <log.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef RENDERER_SDL2
#include <renderers/sdl2/clay_renderer.hpp>
#include <renderers/sdl2/render.hpp>

#ifdef PLATFORM_HAS_CONTROLLER
extern SDL_GameController *controller;
#endif
#endif

#ifdef RENDERER_SDL3
#include <renderers/sdl3/clay_renderer.hpp>
#include <renderers/sdl3/render.hpp>

#ifdef PLATFORM_HAS_CONTROLLER
extern SDL_Gamepad *controller;
#endif
#endif

#ifdef RENDERER_SDL1
#include <renderers/sdl1/clay_renderer.hpp>
#include <renderers/sdl1/render.hpp>

// TODO: Controller stuff
#endif

namespace components {
#if defined(RENDERER_SDL2) || defined(RENDERER_SDL3) || defined(RENDERER_SDL1)
SDL_Font fonts[2] = {};
#elif defined(RENDERER_CITRO2D)
std::map<unsigned int, C2D_Font> fonts;
#endif

uint16_t FONT_ID_BODY_16 = 0;
uint16_t FONT_ID_BODY_BOLD_48 = 1;

const std::array<std::string, 3> Sidebar::tabs = {"home", "projects", "settings"};

Sidebar::Sidebar() {
    nextTabImage = getControllerImage("shoulderR");
    previousTabImage = getControllerImage("shoulderL");

    for (const auto &tab : tabs) {
        const auto maybe = createImageFromFile(OS::getRomFSLocation() + "gfx/menu/" + tab + ".svg", false);
        if (!maybe.has_value()) {
            Log::logError("Failed to load tab image: " + maybe.error());
        }
        images[tab] = maybe.value(); // TODO: Error handling
        hoverDatas[tab] = new HoverData{menuManager, tab};
    }
}

Sidebar::~Sidebar() {
    for (auto &[_, hoverData] : hoverDatas) {
        delete hoverData;
    }
}

static MenuID tabToMenuID(const std::string tab) {
    if (tab == "home") return MenuID::MainMenu;
    if (tab == "projects") return MenuID::ProjectsMenu;
    if (tab == "settings") return MenuID::GlobalSettingsMenu;
    return MenuID::None;
}

constexpr Clay_Color unfocusedTabColor = {90, 60, 90, 255};
constexpr Clay_Color focusedTabColor = {115, 75, 115, 255};

void Sidebar::renderItem(const std::string tab) {
    const std::string id = "sidebar_" + tab;
    Clay_String clayId = (Clay_String){false, static_cast<int32_t>(id.length()), id.c_str()};

    const std::string imageId = "sidebar_img_" + tab;
    Clay_String clayImageId = (Clay_String){false, static_cast<int32_t>(imageId.length()), imageId.c_str()};

    if (selected != tab && tabToMenuID(tab) == menuManager->currentMenuID) {
        unSelectedTab = selected;
        animationTimer.start();
        selected = tab;
    }

    const float t = std::min(animationTimer.getTimeMs(), static_cast<uint64_t>(animationDuration)) / static_cast<float>(animationDuration);

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
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const auto hoverData = (const HoverData*)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME && hoverData->menuManager->currentMenuID != tabToMenuID(hoverData->tab) && hoverData->menuManager->canChangeMenus) hoverData->menuManager->changeMenu(tabToMenuID(hoverData->tab));
		}, hoverDatas[tab]);

		if (images.contains(tab) && images[tab]) CLAY(CLAY_SID(clayImageId), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_PERCENT(0.5) }
			},
			.aspectRatio = { 1 },
			.image = { .imageData = images[tab].get() }	
		});
	}
    // clang-format on
}

void Sidebar::render() {
    if (menuManager->canChangeMenus && (Input::isButtonJustPressed("shoulderR") || Input::isButtonJustPressed("RT"))) menuManager->changeMenu(tabToMenuID(tabs[(std::distance(tabs.begin(), std::find(tabs.begin(), tabs.end(), selected)) + 1) % tabs.size()]));
    if (menuManager->canChangeMenus && (Input::isButtonJustPressed("shoulderL") || Input::isButtonJustPressed("LT"))) {
        int newTab = std::distance(tabs.begin(), std::find(tabs.begin(), tabs.end(), selected)) - 1;
        if (newTab < 0) newTab = tabs.size() - 1;
        menuManager->changeMenu(tabToMenuID(tabs[newTab]));
    }

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
#if (defined(RENDERER_SDL2) || defined(RENDERER_SDL3)) && defined(PLATFORM_HAS_CONTROLLER)
		if (controller != nullptr && menuManager->scale >= 1.5) {
#else
		if (menuManager->scale >= 1.5) {
#endif
			CLAY(CLAY_ID_LOCAL("previous-hint"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0) }
				},
				.aspectRatio = {1},
				.image = { previousTabImage.get() }
			});
		}
		CLAY(CLAY_ID_LOCAL("tabs"), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
				.childGap = static_cast<uint16_t>(10 * menuManager->scale),
				.layoutDirection = CLAY_TOP_TO_BOTTOM
			}
		}) {
			for (const auto &tab : tabs) renderItem(tab);
		}
#if (defined(RENDERER_SDL2) || defined(RENDERER_SDL3)) && defined(PLATFORM_HAS_CONTROLLER)
		if (controller != nullptr && menuManager->scale >= 1.5) {
#else
		if (menuManager->scale >= 1.5) {
#endif
			CLAY(CLAY_ID_LOCAL("next-hint"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0) }
				},
				.aspectRatio = {1},
				.image = { nextTabImage.get() }
			});
		}
	}
    // clang-format on
}

struct ProjectScrollData {
    enum class ScrollState {
        StartPause,
        Forward,
        EndPause,
        Backward
    };

    ScrollState state = ScrollState::StartPause;
    Timer timer;
    float currentScroll = 0.0f;
};

void renderProjectListItem(const ProjectInfo &projectInfo, std::shared_ptr<Image> image, unsigned int i, Clay_SizingAxis width, MenuManager *menuManager, bool selected) {
    static std::unordered_map<unsigned int, ProjectHoverData> hoverDatas;
    static std::unordered_map<unsigned int, ProjectScrollData> scrollDatas;

    hoverDatas[i] = ProjectHoverData{menuManager, &projectInfo};
    auto &scrollData = scrollDatas[i];

    const uint16_t padding = 10 * menuManager->scale;
    selected = selected || Clay_PointerOver(CLAY_IDI("project-list-item", i));
    Clay_Color bgColor = {225, 225, 235, 255};
    Clay_Color borderColor = {90, 60, 90, 255};
    if (selected) {
        bgColor = {255, 215, 255, 255};
        borderColor = {190, 125, 190, 255};
    }

    if (!selected) {
        if (scrollData.state != ProjectScrollData::ScrollState::StartPause || scrollData.currentScroll != 0.0f) {
            scrollData.state = ProjectScrollData::ScrollState::StartPause;
            scrollData.currentScroll = 0.0f;
            scrollData.timer.start();
        }
    } else {
        Clay_ScrollContainerData textWrapperData = Clay_GetScrollContainerData(CLAY_IDI("project-list-item-text-wrapper", i));

        float maxScroll = 0.0f;
        if (textWrapperData.found) {
            maxScroll = textWrapperData.contentDimensions.width - textWrapperData.scrollContainerDimensions.width;
        }

        if (maxScroll > 0.0f) {
            const float scrollSpeed = 45.0f * menuManager->scale;
            constexpr unsigned int pauseDurationMs = 750;

            switch (scrollData.state) {
            case ProjectScrollData::ScrollState::StartPause:
                scrollData.currentScroll = 0.0f;
                if (scrollData.timer.hasElapsed(pauseDurationMs)) {
                    scrollData.state = ProjectScrollData::ScrollState::Forward;
                    scrollData.timer.start();
                }
                break;

            case ProjectScrollData::ScrollState::Forward: {
                float elapsedSec = scrollData.timer.getTimeMs() / 1000.0f;
                scrollData.currentScroll = -(elapsedSec * scrollSpeed);

                if (-scrollData.currentScroll >= maxScroll) {
                    scrollData.currentScroll = -maxScroll;
                    scrollData.state = ProjectScrollData::ScrollState::EndPause;
                    scrollData.timer.start();
                }
                break;
            }

            case ProjectScrollData::ScrollState::EndPause:
                scrollData.currentScroll = -maxScroll;
                if (scrollData.timer.hasElapsed(pauseDurationMs)) {
                    scrollData.state = ProjectScrollData::ScrollState::Backward;
                    scrollData.timer.start();
                }
                break;

            case ProjectScrollData::ScrollState::Backward: {
                float elapsedSec = scrollData.timer.getTimeMs() / 1000.0f;
                scrollData.currentScroll = -maxScroll + (elapsedSec * scrollSpeed);

                if (scrollData.currentScroll >= 0.0f) {
                    scrollData.currentScroll = 0.0f;
                    scrollData.state = ProjectScrollData::ScrollState::StartPause;
                    scrollData.timer.start();
                }
                break;
            }
            }
        } else {
            scrollData.currentScroll = 0.0f;
        }
    }

    // clang-format off
    CLAY(CLAY_IDI("project-list-item", i), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = width, .height = CLAY_SIZING_FIXED(60 * menuManager->scale) },
			.padding = {padding, padding, padding, padding},
			.childGap = 5,
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		},
		.backgroundColor = bgColor,
		.cornerRadius = {10 * menuManager->scale, 10 * menuManager->scale, 10 * menuManager->scale, 10 * menuManager->scale},
		.border = { .color = borderColor, .width = {static_cast<uint16_t>(5 * menuManager->scale), static_cast<uint16_t>(5 * menuManager->scale), static_cast<uint16_t>(5 * menuManager->scale), static_cast<uint16_t>(5 * menuManager->scale)} },
		.transition = {
			.handler = Clay_EaseOut,
			.duration = 0.01f,
			.properties = (Clay_TransitionProperty)(CLAY_TRANSITION_PROPERTY_BACKGROUND_COLOR | CLAY_TRANSITION_PROPERTY_BORDER_COLOR),
		}
	}) {
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const auto projectHoverData = (const ProjectHoverData*)userdata;
			if (pointerData.state != CLAY_POINTER_DATA_PRESSED_THIS_FRAME) return;
			projectHoverData->menuManager->launchProject(projectHoverData->projectInfo->path);
		}, &hoverDatas[i]);

		if (image) {
			CLAY(CLAY_IDI("project-list-item-img", i), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(Clay_GetElementData(CLAY_IDI("project-list-item", i)).boundingBox.height - 2 * padding) }
				},
				.aspectRatio = {1},
				.image = {.imageData = image.get()},
			});
		}
		CLAY(CLAY_IDI("project-list-item-text-wrapper", i), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_GROW(0) }
			},
			.clip = { .horizontal = true, .childOffset = {scrollData.currentScroll, 0} }
		}) {
			static constexpr uint16_t minFontSize = 16;
			uint16_t fontSize = 12 * menuManager->scale;
			if (fontSize < minFontSize) fontSize = minFontSize;
			const Clay_String clayName = {false, static_cast<int32_t>(projectInfo.displayName.length()), projectInfo.displayName.c_str()};
			CLAY_TEXT(clayName, CLAY_TEXT_CONFIG({.textColor = {0, 0, 0, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));
		}
	}
    // clang-format on
}

std::shared_ptr<Image> getControllerImage(const std::string button) {
#ifdef PLATFORM_CONTROLLER_TYPE
    const std::string controllerType = PLATFORM_CONTROLLER_TYPE;
#else
#if defined(RENDERER_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
    static std::string controllerType;
    if (controller == nullptr) controllerType = "xbox"; // Default to Xbox because it's the most common.
    else switch (SDL_GameControllerGetType(controller)) {
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5:
            controllerType = "playstation";
            break;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
            controllerType = "switch";
            break;
        default: // Default to Xbox because it's the most common.
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE:
            controllerType = "xbox";
            break;
        }
#elif defined(RENDERER_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
    static std::string controllerType;
    if (controller == nullptr) controllerType = "xbox"; // Default to Xbox because it's the most common.
    else switch (SDL_GetGamepadType(controller)) {
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5:
            controllerType = "playstation";
            break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
            controllerType = "switch";
            break;
        default: // Default to Xbox because it's the most common.
        case SDL_GAMEPAD_TYPE_XBOX360:
        case SDL_GAMEPAD_TYPE_XBOXONE:
            controllerType = "xbox";
            break;
        }
#else
    static const std::string controllerType = "xbox"; // Default to Xbox because it's the most common.
#endif
#endif

    std::unordered_map<std::string, std::string> buttonMap;
    if (controllerType == "wiiu" || controllerType == "switch")
        buttonMap = {
            {"dpadUp", "dpad_up_outline"},
            {"dpadDown", "dpad_down_outline"},
            {"dpadLeft", "dpad_left_outline"},
            {"dpadRight", "dpad_right_outline"},
            {"A", "button_a_outline"},
            {"B", "button_b_outline"},
            {"X", "button_x_outline"},
            {"Y", "button_y_outline"},
            {"shoulderR", "button_r_outline"},
            {"shoulderL", "button_l_outline"},
            {"start", "button_plus_outline"},
            {"select", "button_minus_outline"},
            {"LeftStickRight", "stick_l_right"},
            {"LeftStickDown", "stick_l_down"},
            {"LeftStickLeft", "stick_l_left"},
            {"LeftStickUp", "stick_l_up"},
            {"LeftStickPressed", "stick_l"},
            {"RightStickRight", "stick_r_right"},
            {"RightStickDown", "stick_r_down"},
            {"RightStickLeft", "stick_r_left"},
            {"RightStickUp", "stick_r_up"},
            {"RightStickPressed", "stick_r"},
            {"LT", "button_zl_outline"},
            {"RT", "button_zr_outline"}};
    else if (controllerType == "gamecube")
        buttonMap = {
            {"dpadUp", "dpad_up_outline"},
            {"dpadDown", "dpad_down_outline"},
            {"dpadLeft", "dpad_left_outline"},
            {"dpadRight", "dpad_right_outline"},
            {"A", "button_a_outline"},
            {"B", "button_b_outline"},
            {"X", "button_x_outline"},
            {"Y", "button_y_outline"},
            {"shoulderR", "button_r_outline"},
            {"shoulderL", "button_l_outline"},
            {"start", "button_start_outline"},
            {"select", "button_z_outline"},
            {"LeftStickRight", "stick_right"},
            {"LeftStickDown", "stick_down"},
            {"LeftStickLeft", "stick_left"},
            {"LeftStickUp", "stick_up"},
            {"LeftStickPressed", "stick"},
            {"RightStickRight", "stick_c_right"},
            {"RightStickDown", "stick_c_down"},
            {"RightStickLeft", "stick_c_left"},
            {"RightStickUp", "stick_c_up"},
            {"RightStickPressed", "stick_c"},
            {"LT", "button_z_outline"},
            {"RT", "button_z_outline"}};
    else if (controllerType == "wii") // For now just assumes nunchuck
        buttonMap = {
            {"dpadUp", "dpad_up_outline"},
            {"dpadDown", "dpad_down_outline"},
            {"dpadLeft", "dpad_left_outline"},
            {"dpadRight", "dpad_right_outline"},
            {"A", "button_a_outline"},
            {"B", "button_b_outline"},
            {"X", "button_z_outline"},
            {"Y", "button_c_outline"},
            {"shoulderR", "button_r_outline"},
            {"shoulderL", "button_l_outline"},
            {"start", "button_plus_outline"},
            {"select", "button_minus_outline"},
            {"LeftStickRight", "stick_l_right"},
            {"LeftStickDown", "stick_l_down"},
            {"LeftStickLeft", "stick_l_left"},
            {"LeftStickUp", "stick_l_up"},
            {"LeftStickPressed", "stick_l"},
            {"RightStickRight", "stick_r_right"},
            {"RightStickDown", "stick_r_down"},
            {"RightStickLeft", "stick_r_left"},
            {"RightStickUp", "stick_r_up"},
            {"RightStickPressed", "stick_r"},
            {"LT", "button_zl_outline"},
            {"RT", "button_zr_outline"}};
    else if (controllerType == "playstation")
        buttonMap = {
            {"dpadUp", "dpad_up_outline"},
            {"dpadDown", "dpad_down_outline"},
            {"dpadLeft", "dpad_left_outline"},
            {"dpadRight", "dpad_right_outline"},
            {"A", "button_cross_outline"},
            {"B", "button_circle_outline"},
            {"X", "button_square_outline"},
            {"Y", "button_triangle_outline"},
            {"shoulderR", "trigger_r1_outline"},
            {"shoulderL", "trigger_l1_outline"},
            {"start", ""}, // ummm, idk what to do for these
            {"select", ""},
            {"LeftStickRight", "stick_l_right"},
            {"LeftStickDown", "stick_l_down"},
            {"LeftStickLeft", "stick_l_left"},
            {"LeftStickUp", "stick_l_up"},
            {"LeftStickPressed", "stick_l"},
            {"RightStickRight", "stick_r_right"},
            {"RightStickDown", "stick_r_down"},
            {"RightStickLeft", "stick_r_left"},
            {"RightStickUp", "stick_r_up"},
            {"RightStickPressed", "stick_r"},
            {"LT", "trigger_l2_outline"},
            {"RT", "trigger_r2_outline"}};
    else if (controllerType == "xbox")
        buttonMap = {
            {"dpadUp", "dpad_up_outline"},
            {"dpadDown", "dpad_down_outline"},
            {"dpadLeft", "dpad_left_outline"},
            {"dpadRight", "dpad_right_outline"},
            {"A", "button_a_outline"},
            {"B", "button_b_outline"},
            {"X", "button_x_outline"},
            {"Y", "button_y_outline"},
            {"shoulderR", "rb_outline"},
            {"shoulderL", "lb_outline"},
            {"start", "button_start_outline"},
            {"select", "button_back_outline"},
            {"LeftStickRight", "stick_l_right"},
            {"LeftStickDown", "stick_l_down"},
            {"LeftStickLeft", "stick_l_left"},
            {"LeftStickUp", "stick_l_up"},
            {"LeftStickPressed", "stick_l"},
            {"RightStickRight", "stick_r_right"},
            {"RightStickDown", "stick_r_down"},
            {"RightStickLeft", "stick_r_left"},
            {"RightStickUp", "stick_r_up"},
            {"RightStickPressed", "stick_r"},
            {"LT", "trigger_lt_outline"},
            {"RT", "trigger_rt_outline"}};

    const auto maybe = createImageFromFile(OS::getRomFSLocation() + "gfx/menu/controller/" + controllerType + "/" + controllerType + "_" + buttonMap[button] + ".svg", false);
    if (!maybe.has_value()) {
        Log::logError("Failed to load controller image: " + maybe.error());
    }
    return maybe.value(); // TODO: Error handling
}
} // namespace components
