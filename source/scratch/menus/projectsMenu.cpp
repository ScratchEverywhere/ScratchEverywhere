#include "projectsMenu.hpp"
#include "menuManager.hpp"
#include "menus/components.hpp"
#include "os.hpp"
#include <cmath>
#include <filesystem>
#include <memory>

ProjectsMenu::ProjectsMenu() {
    components::projectHoverData.clear();

    missingIcon = std::make_unique<Image>("gfx/menu/noicon.svg");

    for (const auto &entry : std::filesystem::directory_iterator(OS::getScratchFolderLocation())) {
        if (entry.is_directory() || entry.path().extension() != ".sb3") continue;
        projects.push_back({.name = entry.path().stem().string(), .path = entry.path().string()});
    }
}

void ProjectsMenu::render() {
    constexpr unsigned int maxColumns = 6;
    const float itemWidth = 150 * menuManager->scale;
    const unsigned int gap = 10 * menuManager->scale;
    const uint16_t padding = 15 * menuManager->scale;

    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.padding = {padding, padding, padding, padding},
			.childGap = static_cast<uint16_t>(10 * menuManager->scale),
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.backgroundColor = {115, 75, 115, 255},
		.cornerRadius = {15 * menuManager->scale, 0, 15 * menuManager->scale, 0},
		.clip = { .horizontal = true, .vertical = true, .childOffset = Clay_GetScrollOffset() },
	}) {
		CLAY_TEXT(CLAY_STRING("Projects"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(24 * menuManager->scale)}));

		unsigned int columns = (Clay_GetElementData(CLAY_ID("main")).boundingBox.width + gap - 2 * padding) / (itemWidth + gap);
		if (columns > maxColumns) columns = maxColumns;
		const unsigned int rows = std::ceil(projects.size() / static_cast<float>(columns));

		for (unsigned int i = 0; i < rows; i++) {
			CLAY(CLAY_IDI("projects-row", i), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(static_cast<float>(columns * (itemWidth + gap) - gap)) },
					.childGap = static_cast<uint16_t>(10 * menuManager->scale),
					.layoutDirection = CLAY_LEFT_TO_RIGHT
				}
			}) {
				for (unsigned int j = 0; j < columns; j++) {
					if (i * columns + j >= projects.size()) continue;
					components::renderProjectListItem(projects[i * columns + j], MenuManager::getImageData(missingIcon.get()), i * columns + j, CLAY_SIZING_FIXED(itemWidth), 0, menuManager); // TODO: Implement text scrolling to see the full name, maybe only when hovered/selected?
				}
			}
		}
	}
    // clang-format on
}
