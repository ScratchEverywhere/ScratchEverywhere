#include "settingsMenu.hpp"
#include "audiostack.hpp"
#include "filesystem.hpp"
#include "image.hpp"
#include "menuManager.hpp"
#include "translation.hpp"
#include <clay.h>
#include <cstring>
#include <fstream>
#include <input.hpp>
#include <regex>
#include <runtime.hpp>
#include <settings.hpp>

void SettingsMenu::init(const std::string &title) {
    for (const auto &[setting, translationKey] : translationNames) {
        names[setting] = TranslationManager::getTranslation(translationKey);

        clayIds[setting] = {false, static_cast<int32_t>(("setting-" + setting).length()), nullptr};
        void *chars = malloc(clayIds[setting].length);
        memcpy(chars, ("setting-" + setting).c_str(), clayIds[setting].length);
        clayIds[setting].chars = static_cast<char *>(chars);

        hoverData.insert({setting, {settings, setting, animationTimers[setting]}});
    }

    indicator = createImageFromFile("gfx/menu/indicator.svg", false).value(); // TODO: Error handling

    const std::string translatedTitle = TranslationManager::getTranslation(title);
    this->title = {false, static_cast<int32_t>(translatedTitle.length()), nullptr};
    void *chars = malloc(translatedTitle.length());
    memcpy(chars, translatedTitle.c_str(), translatedTitle.length());
    this->title.chars = static_cast<char *>(chars);
}

SettingsMenu::~SettingsMenu() {
    free(const_cast<char *>(title.chars));

    for (const auto &[setting, _] : names) {
        free(const_cast<char *>(clayIds[setting].chars));
    }
}

void SettingsMenu::renderSlider(const std::string &setting) {
    renderOrder.push_back(setting);

    auto &hd = hoverData.at(setting);

    const int value = settings[setting].get<int>();

    const uint16_t fontSize = 16 * menuManager->scale;
    const float width = 120 * menuManager->scale;
    const float height = 25 * menuManager->scale;
    const uint16_t borderWidth = 3 * menuManager->scale;
    const uint16_t padding = 5 * menuManager->scale;
    const float knobHeight = height - padding * 2;
    const uint16_t knobBorderWidth = 2 * menuManager->scale;
    const float maxOffset = (width - (knobHeight * 1.6)) * (static_cast<float>(value) / 100.0f);

    const float t = std::min(animationTimers[setting].getTimeMs(), static_cast<uint64_t>(animationDuration)) / static_cast<float>(animationDuration);
    float offset;
    if (!hd.justPressed && startTimer.getTimeMs() > animationDuration) offset = std::lerp(hd.lastOffset, maxOffset, t);
    else offset = maxOffset;

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
		CLAY(CLAY_IDI_LOCAL("bar",renderOrder.size()), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height) },
				.padding = { padding, padding, padding, padding }
			},
			.backgroundColor = { 255, 150, 35, 255 },
			.cornerRadius = { height / 2, height / 2, height / 2, height / 2 },
			.clip = { .horizontal = true, .childOffset = { .x = offset } },
			.border = { .color = { 200, 100, 0, 255 }, .width = { borderWidth, borderWidth, borderWidth, borderWidth } },
		}) {
			CLAY(CLAY_IDI_LOCAL("dial",renderOrder.size()), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .height = CLAY_SIZING_FIXED(knobHeight) }
				},
				.backgroundColor = { 225, 225, 220, 255 },
				.cornerRadius = { knobHeight / 2, knobHeight / 2, knobHeight / 2, knobHeight / 2 },
				.aspectRatio = { 1 },
				.border = { .color = { 220, 120, 5}, .width = { knobBorderWidth, knobBorderWidth, knobBorderWidth, knobBorderWidth } },
			});

            Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const auto hoverData = reinterpret_cast<Settings_HoverData*>(userdata);
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				hoverData->animationTimer.start();
				hoverData->justPressed = true;
			} else hoverData->justPressed = false;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED) {
                hoverData->pointerPos[0] = pointerData.position.x;
                hoverData->pointerPos[1] = pointerData.position.y;
                hoverData->pressed = true;
			} else hoverData->pressed = false;
		}, &hoverData.at(setting));
		} 
        hd.valueText = std::to_string(value);
        
        CLAY_TEXT(( (Clay_String){false, static_cast<int32_t>(hd.valueText.length()), hd.valueText.c_str()}),
                    CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255},.fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));

        
        Clay_ElementData data = Clay_GetElementData(CLAY_IDI_LOCAL("bar",renderOrder.size()));

        if (hd.pressed) {
			const float lefty = data.boundingBox.x + 10 * menuManager->scale;
			const float righty = data.boundingBox.x + data.boundingBox.width - 10 * menuManager->scale; 
            float val = std::clamp(static_cast<float>((hd.pointerPos[0] - lefty)) / (righty - lefty), 0.0f, 1.0f);

            hd.settings[hd.key] = static_cast<int>(100 * val);
        }
		if (hd.justPressed) {
			hd.lastOffset = offset;
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

    const float t = std::min(animationTimers[setting].getTimeMs(), static_cast<uint64_t>(animationDuration)) / static_cast<float>(animationDuration);
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
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const auto hoverData = *(const Settings_HoverData*)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				hoverData.settings[hoverData.key] = !hoverData.settings[hoverData.key];
				hoverData.animationTimer.start();
			}
		}, &hoverData.at(setting));

		CLAY_TEXT(((Clay_String){ false, static_cast<int32_t>(names.at(setting).length()), names.at(setting).c_str() }), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));
		CLAY(CLAY_IDI_LOCAL("toggle",renderOrder.size()), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_FIXED(height * 2), .height = CLAY_SIZING_FIXED(height) },
				.padding = { padding, padding, padding, padding }
			},
			.backgroundColor = { 255, 150, 35, 255 },
			.cornerRadius = { height / 2, height / 2, height / 2, height / 2 },
			.clip = { .horizontal = true, .childOffset = { .x = offset } },
			.border = { .color = { 200, 100, 0, 255 }, .width = { borderWidth, borderWidth, borderWidth, borderWidth } },
		}) {
			CLAY(CLAY_IDI_LOCAL("knob",renderOrder.size()), (Clay_ElementDeclaration){
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
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const auto hoverData = *(const Settings_HoverData*)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				const std::string newContent = Input::openSoftwareKeyboard(hoverData.settings[hoverData.key].get<std::string>().c_str());
				if (std::regex_match(newContent, std::regex("(?=.*[A-Za-z0-9_])[A-Za-z0-9_ ]+"))) hoverData.settings[hoverData.key] = newContent;
			}
		}, &hoverData.at(setting));

		CLAY_TEXT(((Clay_String){ false, static_cast<int32_t>(names.at(setting).length()), names.at(setting).c_str() }), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
	}
    // clang-format on
}

void SettingsMenu::renderButton(const std::string &setting) {
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
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const auto hoverData = reinterpret_cast<Settings_HoverData*>(userdata);
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				hoverData->justPressed = true;
			} else hoverData->justPressed = false;
			if(pointerData.state == CLAY_POINTER_DATA_PRESSED) {
				hoverData->pressed = true;
			} else hoverData->pressed = false;
		}, &hoverData.at(setting));

		CLAY_TEXT(((Clay_String){ false, static_cast<int32_t>(names.at(setting).length()), names.at(setting).c_str() }), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
	}
    // clang-format on
}

bool SettingsMenu::isButtonPressed(const std::string &buttonName) {
    if (hoverData.find(buttonName) == hoverData.end()) return false;

    return hoverData.at(buttonName).pressed;
}

bool SettingsMenu::isButtonJustPressed(const std::string &buttonName) {
    if (hoverData.find(buttonName) == hoverData.end()) return false;

    return hoverData.at(buttonName).justPressed;
}

void SettingsMenu::renderSettings() {}

void SettingsMenu::render() {
    static Timer frameTimer;
    const uint16_t padding = 15 * menuManager->scale;
    bool selectedMoved = false;

    if (Input::isButtonJustPressed("dpadDown") || Input::isButtonJustPressed("LeftStickDown")) {
        if (selected == -1) selected = 0;
        else if (selected != renderOrder.size() - 1) selected++;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("dpadUp") || Input::isButtonJustPressed("LeftStickUp")) {
        if (selected == -1) selected = 0;
        else if (selected != 0) selected--;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("A") && selected != -1) {
        if (hoverData.find(renderOrder[selected]) != hoverData.end()) hoverData.at(renderOrder[selected]).justPressed = true;
        if (settings[renderOrder[selected]].is_boolean()) settings[renderOrder[selected]] = !settings[renderOrder[selected]];
        else if (settings[renderOrder[selected]].is_string()) {
            const std::string newContent = Input::openSoftwareKeyboard(settings[renderOrder[selected]].get<std::string>().c_str());
            if (std::regex_match(newContent, std::regex("(?=.*[A-Za-z0-9_])[A-Za-z0-9_ ]+"))) settings[renderOrder[selected]] = newContent;
        }

        animationTimers[renderOrder[selected]].start();
    } else if ((Input::isButtonJustPressed("dpadRight") || (Input::isButtonJustPressed("LeftStickRight")) || Input::keyHeldDuration[Input::inputControls["dpadRight"]] > 30 || Input::keyHeldDuration[Input::inputControls["LeftStickRight"]] > 30) && selected != -1) {
        if (settings[renderOrder[selected]].is_number_integer()) {
            const int oldVal = settings[renderOrder[selected]].get<int>();
            settings[renderOrder[selected]] = std::clamp(oldVal + 1, 0, 100);
        }
    } else if ((Input::isButtonJustPressed("dpadLeft") || (Input::isButtonJustPressed("LeftStickLeft")) || Input::keyHeldDuration[Input::inputControls["dpadLeft"]] > 30 || Input::keyHeldDuration[Input::inputControls["LeftStickLeft"]] > 30) && selected != -1) {
        if (settings[renderOrder[selected]].is_number_integer()) {
            const int oldVal = settings[renderOrder[selected]].get<int>();
            settings[renderOrder[selected]] = std::clamp(oldVal - 1, 0, 100);
        }
    }

    std::optional<Clay_BoundingBox> selectedBoundingBox = std::nullopt;
    if (selectedMoved && selected != -1) selectedBoundingBox = Clay_GetElementData(CLAY_SID(clayIds[renderOrder[selected]])).boundingBox;

    const float scrollOffset = getScrollOffset(frameTimer, selectedBoundingBox);

    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
			.padding = {padding, padding, padding, padding},
			.childGap = static_cast<uint16_t>(15 * menuManager->scale),
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.backgroundColor = { 115, 75, 115, 255 },
		.cornerRadius = { 15 * menuManager->scale, 0, 15 * menuManager->scale, 0 },
		.clip = { .horizontal = false, .vertical = true, .childOffset = { .y = scrollOffset} },
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
				.image = {indicator.get()},
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
    frameTimer.start();
}

GlobalSettingsMenu::GlobalSettingsMenu(void *userdata) {
    SettingsManager::migrate();

    settings = SettingsManager::getConfigSettings();

    if (!settings.contains("useCustomUsername")) settings["useCustomUsername"] = false;
    if (!settings.contains("customUsername")) settings["customUsername"] = "";

    if (!settings.contains("UseProjectsPath")) settings["UseProjectsPath"] = false;
    if (!settings.contains("ProjectsPath")) settings["ProjectsPath"] = "";

    if (!settings.contains("musicVolume")) settings["musicVolume"] = static_cast<int>(100);

    if (!settings.contains("UseDectalk")) settings["UseDectalk"] = false;

    SettingsMenu::init("ui.settings.global");
}

GlobalSettingsMenu::~GlobalSettingsMenu() {
    std::ofstream out(OS::getConfigFolderLocation() + "Settings.json");
    out << settings.dump(4);
    out.close();
}

void GlobalSettingsMenu::renderSettings() {
    renderSlider("musicVolume");
    static float oldMusicVolume;
    if (oldMusicVolume != settings["musicVolume"]) {
#ifdef __NDS__
        constexpr const char *songName = "gfx/nds/mm_ds.wav";
#else
        constexpr const char *songName = "gfx/menu/mm_splash.ogg";
#endif
        if (Mixer::isSoundPlaying(songName)) {
            Mixer::setSoundVolume(songName, settings.value("musicVolume", 100));
        }
    }
    oldMusicVolume = settings["musicVolume"];

    renderToggle("useCustomUsername");
    if (settings["useCustomUsername"]) renderInputButton("customUsername");

    renderToggle("UseProjectsPath");
    if (settings["UseProjectsPath"]) renderInputButton("ProjectsPath");

    renderButton("clearCache");
    if (isButtonJustPressed("clearCache")) {
        FileSystem::removeDirectory(OS::getScratchFolderLocation() + "cache/");
        FileSystem::createDirectory(OS::getScratchFolderLocation() + "cache/");
    }

    renderButton("language");
    if (isButtonJustPressed("language")) {
        menuManager->queueChangeMenu(MenuID::LanguageMenu);
    }

    renderToggle("useDectalk");
}
