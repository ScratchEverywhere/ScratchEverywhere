#include "mainMenu.hpp"
#include "components.hpp"

Clay_RenderCommandArray MainMenu::render() {
    Clay_BeginLayout();
    // clang-format off
    CLAY(CLAY_ID("outer"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.childGap = 10,
			.childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
			.layoutDirection = CLAY_LEFT_TO_RIGHT,
		},
		.backgroundColor = {117, 77, 117, 255}
	}) {
		sidebar.render();
        CLAY_TEXT(CLAY_STRING("Test!"), components::defaultTextConfig);
    };
    // clang-format on
    return Clay_EndLayout();
}
