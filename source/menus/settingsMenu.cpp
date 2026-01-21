#include "settingsMenu.hpp"
#include "menuManager.hpp"
#include <clay.h>
#include <cstring>
#include <fstream>
#include <input.hpp>
#include <regex>
#include <runtime.hpp>
#include <settings.hpp>

void SettingsMenu::init(const std::string &title) {
    for (const auto &[setting, _] : settings.items()) {
        clayIds[setting] = {false, static_cast<int32_t>(("setting-" + setting).length()), nullptr};
        void *chars = malloc(clayIds[setting].length);
        memcpy(chars, ("setting-" + setting).c_str(), clayIds[setting].length);
        clayIds[setting].chars = static_cast<char *>(chars);

        hoverData.insert({setting, {settings, setting, animationTimers[setting]}});
    }

    indicator = std::make_unique<Image>("gfx/menu/indicator.svg");

    this->title = {false, static_cast<int32_t>(title.length()), nullptr};
    void *chars = malloc(title.length());
    memcpy(chars, title.c_str(), title.length());
    this->title.chars = static_cast<char *>(chars);
}

SettingsMenu::~SettingsMenu() {
    free(const_cast<char *>(title.chars));

    for (const auto &[setting, _] : settings.items()) {
        free(const_cast<char *>(clayIds[setting].chars));
    }
}

void SettingsMenu::renderSlider(const std::string &setting) {
    renderOrder.push_back(setting);

    const int value = settings[setting].get<int>();

    const uint16_t fontSize = 16 * menuManager->scale;
    const float width = 120 * menuManager->scale;
    const float height = 25 * menuManager->scale;
    const uint16_t borderWidth = 3 * menuManager->scale;
    const uint16_t padding = 5 * menuManager->scale;
    const float knobHeight = height - padding * 2;
    const uint16_t knobBorderWidth = 2 * menuManager->scale;

    float offset = (width - (knobHeight * 1.5)) * (static_cast<float>(value) / 100.0f);

    // clang-format off
    CLAY(CLAY_SID(clayIds[setting]), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
			.childGap = static_cast<uint16_t>(10 * menuManager->scale),
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		}
	}) {
		

		CLAY_TEXT(((Clay_String){ false, static_cast<int32_t>(names.at(setting).length()), names.at(setting).c_str() }), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));
		CLAY(CLAY_ID_LOCAL("bar"), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height) },
				.padding = { padding, padding, padding, padding }
			},
			.backgroundColor = { 255, 150, 35, 255 },
			.cornerRadius = { height / 2, height / 2, height / 2, height / 2 },
			.clip = { .horizontal = true, .childOffset = { .x = offset } },
			.border = { .color = { 200, 100, 0, 255 }, .width = { borderWidth, borderWidth, borderWidth, borderWidth } },
		}) {
			CLAY(CLAY_ID_LOCAL("dial"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .height = CLAY_SIZING_FIXED(knobHeight) }
				},
				.backgroundColor = { 225, 225, 220, 255 },
				.cornerRadius = { knobHeight / 2, knobHeight / 2, knobHeight / 2, knobHeight / 2 },
				.aspectRatio = { 1 },
				.border = { .color = { 220, 120, 5}, .width = { knobBorderWidth, knobBorderWidth, knobBorderWidth, knobBorderWidth } },
			});

            Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, intptr_t userdata) {
			const auto hoverData = reinterpret_cast<Settings_HoverData*>(userdata);
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED) {
                hoverData->pointerPos[0] = pointerData.position.x;
                hoverData->pointerPos[1] = pointerData.position.y;
                hoverData->pressed = true;
			} else hoverData->pressed = false;
		}, (intptr_t)&hoverData.at(setting));
		} 
        auto &hd = hoverData.at(setting);
        hd.valueText = std::to_string(value);
        
        CLAY_TEXT(( (Clay_String){false, static_cast<int32_t>(hd.valueText.length()), hd.valueText.c_str()}),
                    CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255},.fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));

        
        Clay_ElementData data = Clay_GetElementData(CLAY_ID_LOCAL("bar"));

        if (hd.pressed && hd.pointerPos[0] >= data.boundingBox.x && hd.pointerPos[0] <= data.boundingBox.x + data.boundingBox.width &&
            hd.pointerPos[1] >= data.boundingBox.y && hd.pointerPos[1] <= data.boundingBox.y + data.boundingBox.height) {

            float val = std::clamp(static_cast<float>((hd.pointerPos[0] - data.boundingBox.x)) / ((data.boundingBox.x + data.boundingBox.width - 10) - data.boundingBox.x), 0.0f, 1.0f);

            hd.settings[hd.key] = static_cast<int>(100 * val);
        }
	}
    // clang-format on
}

void SettingsMenu::renderToggle(const std::string &setting) {
    renderOrder.push_back(setting);

    const uint16_t fontSize = 16 * menuManager->scale;
    const float height = 25 * menuManager->scale;
    const uint16_t borderWidth = 3 * menuManager->scale;
    const uint16_t padding = 5 * menuManager->scale;
    const float knobHeight = height - padding * 2;
    const uint16_t knobBorderWidth = 2 * menuManager->scale;

    const float t = std::min(animationTimers[setting].getTimeMs(), static_cast<int>(animationDuration)) / static_cast<float>(animationDuration);
    float offset;
    if (startTimer.getTimeMs() > animationDuration) offset = settings[setting] ? std::lerp(0, height, t) : std::lerp(height, 0, t);
    else offset = settings[setting] ? height : 0;

    // clang-format off
    CLAY(CLAY_SID(clayIds[setting]), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) },
			.childGap = static_cast<uint16_t>(10 * menuManager->scale),
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		}
	}) {
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, intptr_t userdata) {
			const auto hoverData = *(const Settings_HoverData*)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				hoverData.settings[hoverData.key] = !hoverData.settings[hoverData.key];
				hoverData.animationTimer.start();
			}
		}, (intptr_t)&hoverData.at(setting));

		CLAY_TEXT(((Clay_String){ false, static_cast<int32_t>(names.at(setting).length()), names.at(setting).c_str() }), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));
		CLAY(CLAY_ID_LOCAL("toggle"), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_FIXED(height * 2), .height = CLAY_SIZING_FIXED(height) },
				.padding = { padding, padding, padding, padding }
			},
			.backgroundColor = { 255, 150, 35, 255 },
			.cornerRadius = { height / 2, height / 2, height / 2, height / 2 },
			.clip = { .horizontal = true, .childOffset = { .x = offset } },
			.border = { .color = { 200, 100, 0, 255 }, .width = { borderWidth, borderWidth, borderWidth, borderWidth } },
		}) {
			CLAY(CLAY_ID_LOCAL("knob"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .height = CLAY_SIZING_FIXED(knobHeight) }
				},
				.backgroundColor = { 225, 225, 220, 255 },
				.cornerRadius = { knobHeight / 2, knobHeight / 2, knobHeight / 2, knobHeight / 2 },
				.aspectRatio = { 1 },
				.border = { .color = { 220, 120, 5 }, .width = { knobBorderWidth, knobBorderWidth, knobBorderWidth, knobBorderWidth } },
			});
		}
	}
    // clang-format on
}

void SettingsMenu::renderInputButton(const std::string &setting) {
    renderOrder.push_back(setting);

    const uint16_t hPadding = 10 * menuManager->scale;
    const uint16_t vPadding = 5 * menuManager->scale;

    // clang-format off
    CLAY(CLAY_SID(clayIds[setting]), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
			.padding = {hPadding, hPadding, vPadding, vPadding}
		},
		.backgroundColor = {90, 60, 90, 255},
		.cornerRadius = {5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale}
	}) {
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, intptr_t userdata) {
			const auto hoverData = *(const Settings_HoverData*)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				const std::string newContent = Input::openSoftwareKeyboard(hoverData.settings[hoverData.key].get<std::string>().c_str());
				if (std::regex_match(newContent, std::regex("(?=.*[A-Za-z0-9_])[A-Za-z0-9_ ]+"))) hoverData.settings[hoverData.key] = newContent;
			}
		}, (intptr_t)&hoverData.at(setting));

		CLAY_TEXT(((Clay_String){ false, static_cast<int32_t>(names.at(setting).length()), names.at(setting).c_str() }), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
	}
    // clang-format on
}

void SettingsMenu::renderSettings() {}

// TODO: Steal scrolling from projects menu
void SettingsMenu::render() {
    const uint16_t padding = 15 * menuManager->scale;

    if (Input::isButtonJustPressed("dpadDown") || Input::isButtonJustPressed("LeftStickDown")) {
        if (selected == -1) selected = 0;
        else if (selected != renderOrder.size() - 1) selected++;
    } else if (Input::isButtonJustPressed("dpadUp") || Input::isButtonJustPressed("LeftStickUp")) {
        if (selected == -1) selected = 0;
        else if (selected != 0) selected--;
    } else if (Input::isButtonJustPressed("A") && selected != -1) {
        if (settings[renderOrder[selected]].is_boolean()) settings[renderOrder[selected]] = !settings[renderOrder[selected]];
        else if (settings[renderOrder[selected]].is_string()) {
            const std::string newContent = Input::openSoftwareKeyboard(settings[renderOrder[selected]].get<std::string>().c_str());
            if (std::regex_match(newContent, std::regex("(?=.*[A-Za-z0-9_])[A-Za-z0-9_ ]+"))) settings[renderOrder[selected]] = newContent;
        }

        animationTimers[renderOrder[selected]].start();
    }

    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
			.padding = {padding, padding, padding, padding},
			.childGap = static_cast<uint16_t>(15 * menuManager->scale),
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.backgroundColor = { 115, 75, 115, 255 },
		.cornerRadius = { 15 * menuManager->scale, 0, 15 * menuManager->scale, 0 }
	}) {
		CLAY(CLAY_ID_LOCAL("title-wrapper"), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_GROW(0) },
				.childAlignment = { .x = CLAY_ALIGN_X_CENTER }
			}
		}) {
			CLAY_TEXT(title, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(24 * menuManager->scale) }));
		}

		renderOrder.clear();
		renderSettings();

		if (selected > static_cast<int>(renderOrder.size()) - 1) selected = renderOrder.size() - 1;

		if (selected != -1) {
			CLAY(CLAY_ID_LOCAL("indicator"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(15 * menuManager->scale) }
				},
				.aspectRatio = {1},
				.image = {MenuManager::getImageData(indicator.get())},
				.floating = {
					.offset = { .x = 15 * menuManager->scale },
					.parentId = CLAY_SID(clayIds[renderOrder[selected]]).id,
					.attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_CENTER },
					.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID
				}
			});
		}
	}
    // clang-format on
}

GlobalSettingsMenu::GlobalSettingsMenu(void *userdata) {
    SettingsManager::migrate();

    settings = SettingsManager::getConfigSettings();

    if (!settings.contains("useCustomUsername")) settings["useCustomUsername"] = false;
    if (!settings.contains("customUsername")) settings["customUsername"] = "";

    if (!settings.contains("UseProjectsPath")) settings["UseProjectsPath"] = false;
    if (!settings.contains("ProjectsPath")) settings["ProjectsPath"] = "";

    if (!settings.contains("musicVolume")) settings["musicVolume"] = static_cast<int>(100);

    SettingsMenu::init();
}

GlobalSettingsMenu::~GlobalSettingsMenu() {
    std::ofstream out(OS::getConfigFolderLocation() + "Settings.json");
    out << settings.dump(4);
    out.close();
}

void GlobalSettingsMenu::renderSettings() {
    renderSlider("musicVolume");

    renderToggle("useCustomUsername");
    if (settings["useCustomUsername"]) renderInputButton("customUsername");

    renderToggle("UseProjectsPath");
    if (settings["UseProjectsPath"]) renderInputButton("ProjectsPath");
}
