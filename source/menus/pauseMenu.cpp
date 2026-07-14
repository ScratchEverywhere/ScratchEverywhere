#include "pauseMenu.hpp"
#include "components.hpp"
#include "input.hpp"
#include "log.hpp"
#include "menu.hpp"
#include "menuManager.hpp"
#include "runtime.hpp"
#include "translation.hpp"
#include <runtime.hpp>

bool PauseMenu::shouldUnpause = false;

PauseMenu::PauseMenu(void *userdata) {
    shouldUnpause = false;

    const auto &addButtonText = [&](std::string translationKey) {
        const std::string &translated = TranslationManager::getTranslation(translationKey);
        buttonTexts.push_back({false, static_cast<int32_t>(translated.length()), nullptr});
        void *chars = malloc(buttonTexts.back().length);
        memcpy(chars, translated.c_str(), buttonTexts.back().length);
        buttonTexts.back().chars = static_cast<char *>(chars);
    };

    addButtonText("ui.pause.resume");
    addButtonText("ui.pause.flag");
    addButtonText("ui.pause.stop");
    addButtonText("ui.pause.turbo.enable");
    addButtonText("ui.pause.turbo.disable");
    addButtonText("ui.pause.exit");

    const auto maybe = createImageFromFile(OS::getRomFSLocation() + "gfx/menu/indicator.svg", false);
    if (!maybe.has_value()) {
        Log::logError("Failed to load indicator image: " + maybe.error());
    }
    indicator = maybe.value();
}

PauseMenu::~PauseMenu() {
    for (const auto &text : buttonTexts) {
        free(const_cast<char *>(text.chars));
    }
}

void PauseMenu::renderButton(unsigned int id, Clay_String text, void (*cb)()) {
    const uint16_t hPadding = 10 * menuManager->scale;
    const uint16_t vPadding = 5 * menuManager->scale;

    if (selected == id && Input::isButtonJustPressed("A")) cb();

    // clang-format off
	CLAY(CLAY_IDI("pause-button", id), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
			.padding = {hPadding, hPadding, vPadding, vPadding}
		},
		.backgroundColor = {90, 60, 90, 255},
		.cornerRadius = {5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale}
	}) {
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			void (*cb)() = (void (*)())userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				cb();
			}
		}, (void *)cb);

		CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
	}
    // clang-format on
}

void PauseMenu::render() {
    const uint16_t gap = 10 * menuManager->scale;
    constexpr unsigned int numButtons = 5;

    if (Input::isButtonJustPressed("dpadDown") || Input::isButtonJustPressed("LeftStickDown")) {
        if (selected == -1) selected = 0;
        else if (selected != numButtons - 1) selected++;
    } else if (Input::isButtonJustPressed("dpadUp") || Input::isButtonJustPressed("LeftStickUp")) {
        if (selected == -1) selected = 0;
        else if (selected != 0) selected--;
    }

    // clang-format off
    CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(250 * menuManager->scale), .height = CLAY_SIZING_GROW(0)},
			.padding = {gap, gap, gap, gap},
			.childGap = gap,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
		.backgroundColor = { 115, 75, 115, 255 },
		.floating = {
			.attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
			.attachTo = CLAY_ATTACH_TO_ROOT,
		}
    }) {
        // clang-format on
        renderButton(0, buttonTexts[0], []() {
            shouldUnpause = true;
        });
        renderButton(1, buttonTexts[1], []() {
            Scratch::greenFlagClicked();
            shouldUnpause = true;
        });
        renderButton(2, buttonTexts[2], []() {
            Scratch::stopClicked();
            shouldUnpause = true;
        });
        renderButton(3, buttonTexts[Scratch::turbo ? 4 : 3], []() {
            Scratch::turbo = !Scratch::turbo;
        });
        renderButton(4, buttonTexts[5], []() {
            Scratch::shouldStop = true;
            shouldUnpause = true;
        });

        if (selected != -1) {
            // clang-format off
			CLAY(CLAY_ID_LOCAL("indicator"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(15 * menuManager->scale) }
				},
				.aspectRatio = {1},
				.image = {indicator.get()},
				.floating = {
					.offset = { .x = 15 * menuManager->scale },
					.parentId = CLAY_IDI("pause-button", selected).id,
					.attachPoints = { .element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_CENTER },
					.attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID
				}
			});
            // clang-format on
        }
    }
}
