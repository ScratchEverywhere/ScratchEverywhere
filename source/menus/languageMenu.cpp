#include "languageMenu.hpp"
#include "components.hpp"
#include "input.hpp"
#include "menuManager.hpp"
#include "translation.hpp"
#include <log.hpp>

LanguageMenu::LanguageMenu(void *userdata) {
    langs = TranslationManager::getLanguages();
    clayLangNames.reserve(langs.size());
    for (const auto &lang : langs) {
        clayLangNames.push_back({false, static_cast<int32_t>(lang.name.length()), nullptr});
        void *chars = malloc(clayLangNames.back().length);
        memcpy(chars, lang.name.c_str(), clayLangNames.back().length);
        clayLangNames.back().chars = static_cast<char *>(chars);
    }

    const auto maybe = createImageFromFile("gfx/menu/indicator.svg", false);
    if (!maybe.has_value()) {
        Log::logError("Failed to load indicator image: " + maybe.error());
    }
    indicator = maybe.value(); // TODO: Error handling
}

LanguageMenu::~LanguageMenu() {
    for (const auto &name : clayLangNames) {
        free(const_cast<char *>(name.chars));
    }
}

void changeLanguage(MenuManager *menuManager, unsigned int id) {
    TranslationManager::loadLanguage(TranslationManager::getLanguages()[id].key);
    nlohmann::json json = SettingsManager::getConfigSettings();
    json["Language"] = TranslationManager::getLoadedLanguage().key;
    SettingsManager::saveConfigSettings(json);
    menuManager->queueBack();
}

void LanguageMenu::render() {
    static Timer frameTimer;
    if (Input::isButtonJustPressed("B") && menuManager->canChangeMenus) {
        menuManager->back();
        return;
    }

    bool selectedMoved = false;
    if (Input::isButtonJustPressed("dpadDown") || Input::isButtonJustPressed("LeftStickDown")) {
        if (selected == -1) selected = 0;
        else if (selected != langs.size() - 1) selected++;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("dpadUp") || Input::isButtonJustPressed("LeftStickUp")) {
        if (selected == -1) selected = 0;
        else if (selected != 0) selected--;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("A") && selected != -1) {
        changeLanguage(menuManager, selected);
    }

    std::optional<Clay_BoundingBox> selectedBoundingBox = std::nullopt;
    if (selectedMoved && selected != -1)
        selectedBoundingBox = Clay_GetElementData(CLAY_IDI("lang", selected)).boundingBox;

    const float scrollOffset = getScrollOffset(frameTimer, selectedBoundingBox);

    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.childGap = static_cast<uint16_t>(5 * menuManager->scale),
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = {115, 75, 115, 255},
		.cornerRadius = {15 * menuManager->scale, 0, 15 * menuManager->scale, 0},
		.clip = {.horizontal = false, .vertical = true, .childOffset = {.y = scrollOffset}}
	}) {
		CLAY_TEXT(CLAY_STRING("Select Language"), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(24 * menuManager->scale) }));

		const uint16_t fontSize = 16 * menuManager->scale;
		const uint16_t hPadding = 10 * menuManager->scale;
		const uint16_t vPadding = 5 * menuManager->scale;

		for (const auto &lang : langs) {
			CLAY(CLAY_IDI("lang", lang.id), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
					.padding = {hPadding, hPadding, vPadding, vPadding}
				},
				.backgroundColor = {90, 60, 90, 255},
				.cornerRadius = {5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale}
			}) {
				struct HoverData {
					LanguageMenu *menu;
					TranslationManager::LanguageInfo lang;
				};

				Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
					const auto hoverData = reinterpret_cast<LanguageMenu*>(userdata);
					if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
						changeLanguage(hoverData->menuManager, id.offset);
					}
				}, this);

				CLAY_TEXT(clayLangNames[lang.id], CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = fontSize }));
			}
		}

		if (selected != -1) {
			CLAY(CLAY_ID_LOCAL("indicator"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(15 * menuManager->scale) }
				},
				.aspectRatio = {1},
				.image = {indicator.get()},
				.floating = {
					.offset = { .x = 15 * menuManager->scale },
					.parentId = CLAY_IDI("lang", selected).id,
					.attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_CENTER },
					.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID
				}
			});
		}
	}
    // clang-format on
    frameTimer.start();
}
