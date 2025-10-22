#include "projectsMenu.hpp"
#include "menus/components.hpp"
#include "os.hpp"
#include <cmath>
#include <filesystem>

#ifdef SDL_BUILD
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#endif

static constexpr unsigned int itemWidth = 150;
static constexpr unsigned int gap = 10;
static constexpr unsigned int padding = 15;
static constexpr unsigned int maxColumns = 6;

ProjectsMenu::ProjectsMenu() {
#ifdef SDL_BUILD
    missingIcon = IMG_Load((OS::getRomFSLocation() + "gfx/menu/noicon.svg").c_str());
    if (!missingIcon) Log::logError("Failed to load missing icon image.");
#endif

    for (const auto &entry : std::filesystem::directory_iterator(OS::getScratchFolderLocation())) {
        if (entry.is_directory() || entry.path().extension() != ".sb3") continue;
        projects.push_back({.name = entry.path().stem().string(), .path = entry.path().string()});
    }
}

ProjectsMenu::~ProjectsMenu() {
#ifdef SDL_BUILD
    if (missingIcon) SDL_FreeSurface(missingIcon);
#endif
}

void ProjectsMenu::render() {
    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)},
			.padding = {padding, padding, padding, padding},
			.childGap = 10,
			.childAlignment = { .x = CLAY_ALIGN_X_CENTER },
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.backgroundColor = {115, 75, 115, 255},
		.cornerRadius = {15, 0, 15, 0},
		.clip = { .horizontal = true, .vertical = true, .childOffset = Clay_GetScrollOffset() },
	}) {
		CLAY_TEXT(CLAY_STRING("Projects"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = 24}));

		unsigned int columns = (Clay_GetElementData(CLAY_ID("main")).boundingBox.width + gap - 2 * padding) / (itemWidth + gap);
		if (columns > maxColumns) columns = maxColumns;
		const unsigned int rows = std::ceil(projects.size() / static_cast<float>(columns));

		for (unsigned int i = 0; i < rows; i++) {
			CLAY(CLAY_IDI("projects-row", i), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_FIXED(static_cast<float>(columns * (itemWidth + gap) - gap)) },
					.childGap = 10,
					.layoutDirection = CLAY_LEFT_TO_RIGHT
				}
			}) {
				for (unsigned int j = 0; j < columns; j++) {
					if (i * columns + j >= projects.size()) continue;
					components::renderProjectListItem(projects[i * columns + j], missingIcon, i * columns + j, CLAY_SIZING_FIXED(itemWidth), 0); // TODO: Implement text scrolling to see the full name, maybe only when hovered/selected?
				}
			}
		}
	}
    // clang-format on
}
