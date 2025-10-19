#include "mainMenu.hpp"
#include "menuManager.hpp"

Clay_RenderCommandArray MainMenu::render() {
    Clay_BeginLayout();
    // clang-format off
    CLAY(CLAY_ID("outer"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.layoutDirection = CLAY_LEFT_TO_RIGHT
		},
		.backgroundColor = {117, 77, 117, 255}
	}) {
        CLAY_TEXT(CLAY_STRING("Test!"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = MenuManager::FONT_ID_BODY_16, .fontSize = 16}));
    };
    // clang-format on
    return Clay_EndLayout();
}
