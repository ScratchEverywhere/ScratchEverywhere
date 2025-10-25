#include "mainMenu.hpp"
#include "components.hpp"
#include "menuManager.hpp"
#include "unzip.hpp"

std::string MainMenu::splash = "";

MainMenu::MainMenu() {
    logo = std::make_unique<Image>("gfx/menu/logo.svg");

    if (splash == "") splash = Unzip::getSplashText();
}

void MainMenu::render() {
    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = {115, 75, 115, 255},
		.cornerRadius = {15 * menuManager->scale, 0, 15 * menuManager->scale, 0}
	}) {
		if (logo) {
			CLAY(CLAY_ID("logo"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_PERCENT(0.5) },
					.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM }
				},
				.aspectRatio = { logo->getWidth() / static_cast<float>(logo->getHeight()) },
				.image = { MenuManager::getImageData(logo.get()) }
			});
			const Clay_String claySplash = {true, static_cast<int32_t>(splash.length()), splash.c_str()};
			constexpr uint16_t minFontSize = 18;
			uint16_t fontSize = Clay_GetElementData(CLAY_ID("logo")).boundingBox.width / 12.5;
			if (fontSize < minFontSize) fontSize = minFontSize;
			CLAY_TEXT(claySplash, CLAY_TEXT_CONFIG({.textColor = {255, 150, 35, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = fontSize }));
		}
	}
    // clang-format on
}
