#include "projectsMenu.hpp"
#include "menus/components.hpp"

void ProjectsMenu::render() {
    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = {115, 75, 115, 255},
		.cornerRadius = {15, 0, 15, 0}
	}) {
		CLAY_TEXT(CLAY_STRING("Very definitly real and complete projects screen."), DEFAULT_TEXT_CONFIG);
	}
    // clang-format on
}
