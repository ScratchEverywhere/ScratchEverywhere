#include "mainMenu.hpp"
#include "components.hpp"
#include "os.hpp"
#include "unzip.hpp"
#include <SDL2/SDL_image.h>

std::string MainMenu::splash = "";

MainMenu::MainMenu() {
    logo = IMG_Load((OS::getRomFSLocation() + "gfx/menu/logo.svg").c_str());
    if (!logo) Log::logError("Failed to load logo.");

    if (splash == "") splash = Unzip::getSplashText();
}

MainMenu::~MainMenu() {
    if (logo) SDL_FreeSurface(logo);
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
		.cornerRadius = {15, 0, 15, 0}
	}) {
		if (logo) {
			CLAY(CLAY_ID("logo"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_PERCENT(0.5) },
					.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_BOTTOM }
				},
				.aspectRatio = { logo->w / static_cast<float>(logo->h) },
				.image = { logo }
			});
			const Clay_String claySplash = {true, static_cast<int32_t>(splash.length()), splash.c_str()};
			CLAY_TEXT(claySplash, CLAY_TEXT_CONFIG({.textColor = {255, 150, 35, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(Clay_GetElementData(CLAY_ID("logo")).boundingBox.width / 12.5)}));
		}
	}
    // clang-format on
}
