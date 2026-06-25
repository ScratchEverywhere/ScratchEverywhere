#include "confirmationMenu.hpp"
#include "components.hpp"
#include "menu.hpp"
#include "menuManager.hpp"
#include "translation.hpp"

bool ConfirmationMenu::accepted;
bool ConfirmationMenu::chosen;

ConfirmationMenu::ConfirmationMenu(void *userdata) {
    text = *(std::string *)userdata;
    acceptString = TranslationManager::getTranslation("ui.confirm.accept");
    cancelString = TranslationManager::getTranslation("ui.confirm.cancel");

    ConfirmationMenu::chosen = false;
}

void ConfirmationMenu::renderButton(bool isAccept) {
    const uint16_t hPadding = 10 * menuManager->scale;
    const uint16_t vPadding = 5 * menuManager->scale;

    // clang-format off
	CLAY(CLAY_SID_LOCAL(isAccept ? CLAY_STRING("accept") : CLAY_STRING("cancel")), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
			.padding = {hPadding, hPadding, vPadding, vPadding}
		},
		.backgroundColor = {90, 60, 90, 255},
		.cornerRadius = {5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale, 5 * menuManager->scale}
	}) {
		Clay_OnHover([](Clay_ElementId id, Clay_PointerData pointerData, void *userdata) {
			const bool isAccept = (bool)userdata;
			if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
				ConfirmationMenu::accepted = isAccept;
				ConfirmationMenu::chosen = true;
			}
		}, (void *)isAccept);

		const Clay_String clayText = {true, static_cast<int32_t>((isAccept ? acceptString : cancelString).length()), (isAccept ? acceptString : cancelString).c_str()};
		CLAY_TEXT(clayText, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
	}
    // clang-format on
}

void ConfirmationMenu::render() {
    // clang-format off
    CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
        .layout = {
            .sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.childGap = static_cast<uint16_t>(15 * menuManager->scale),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
		.backgroundColor = { 115, 75, 115, 255 }
    }) {
		const Clay_String clayText = {true, static_cast<int32_t>(text.length()), text.c_str()};

		CLAY_TEXT(clayText, CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));

		CLAY(CLAY_ID_LOCAL("buttons"), (Clay_ElementDeclaration){
			.layout = {
				.childGap = static_cast<uint16_t>(10 * menuManager->scale),
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			}
		}) {
			renderButton(true);
			renderButton(false);
		}
    }
    // clang-format on
}
