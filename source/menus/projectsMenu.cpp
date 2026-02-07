#include "projectsMenu.hpp"
#include "../input.hpp"
#include "menuManager.hpp"
#include "menus/components.hpp"
#include "os.hpp"
#include "settingsMenu.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <render.hpp>
#include <string>

#ifdef RENDERER_SDL2
#include <renderers/sdl2/render.hpp>
#else
constexpr unsigned int windowWidth = 320;
constexpr unsigned int windowHeight = 240;
#endif

ProjectsMenu::ProjectsMenu(void *userdata) {
    missingIcon = std::make_unique<Image>("gfx/menu/noicon.svg");

    const std::string path = OS::getScratchFolderLocation();

    if (OS::fileExists(path)) {
        for (const auto &entry : std::filesystem::directory_iterator(path)) {
            if (entry.path().extension() != ".sb3" && !(entry.is_directory() && std::filesystem::is_regular_file(entry.path() / "project.json"))) continue;
            projects.push_back({.name = entry.path().stem().string(), .path = entry.path().string()});
        }
    }

    const std::string noProjectsPathString = "You can put projects in: " + OS::getScratchFolderLocation();
    void *mem = malloc(noProjectsPathString.length());
    if (mem == nullptr) {
        Log::logError("Failed to allocate memory for no projects string.");
        noProjectsPath = {false, 0, nullptr};
        return;
    }
    memcpy(mem, noProjectsPathString.c_str(), noProjectsPathString.length());
    noProjectsPath = {false, static_cast<int32_t>(noProjectsPathString.length()), static_cast<const char *>(mem)};
}

ProjectsMenu::~ProjectsMenu() {
    if (noProjectsPath.chars != nullptr) free(const_cast<char *>(noProjectsPath.chars));
}

void ProjectsMenu::render() {
    static Timer frameTimer;

    constexpr unsigned int maxColumns = 6;
    const float itemWidth = 150 * menuManager->scale;
    const unsigned int gap = 10 * menuManager->scale;
    const uint16_t padding = 15 * menuManager->scale;
    const int windowWidth = Render::getWidth();
    const int windowHeight = Render::getHeight();

    unsigned int columns = (Clay_GetElementData(CLAY_ID("main")).boundingBox.width + gap - 2 * padding) / (itemWidth + gap);
    if (columns > maxColumns) columns = maxColumns;
    else if (columns <= 0) columns = 1;
    const unsigned int rows = std::ceil(projects.size() / static_cast<float>(columns));

    bool selectedMoved = false;
    if (Input::isButtonJustPressed("dpadRight") || Input::isButtonJustPressed("LeftStickRight")) {
        if (selectedProject == -1) selectedProject = 0;
        else if (selectedProject != projects.size() - 1) selectedProject++;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("dpadLeft") || Input::isButtonJustPressed("LeftStickLeft")) {
        if (selectedProject == -1) selectedProject = 0;
        else if (selectedProject != 0) selectedProject--;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("dpadDown") || Input::isButtonJustPressed("LeftStickDown")) {
        if (selectedProject == -1) selectedProject = 0;
        else {
            selectedProject += columns;
            if (selectedProject > projects.size() - 1) selectedProject = projects.size() - 1;
        }
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("dpadUp") || Input::isButtonJustPressed("LeftStickUp")) {
        if (selectedProject == -1) selectedProject = 0;
        else if (selectedProject >= columns) selectedProject -= columns;
        selectedMoved = true;
    } else if (Input::isButtonJustPressed("A") && selectedProject >= 0) menuManager->launchProject(projects[selectedProject].path);
    else if (Input::isButtonJustPressed("X") && selectedProject >= 0) {
        menuManager->queueChangeMenu(MenuID::ProjectSettingsMenu, const_cast<void *>(static_cast<const void *>(projects[selectedProject].name.c_str())));
        return;
    }
    if (selectedProject == 0 && projects.empty()) selectedProject = -1;

    if (!projects.empty()) {
        const Clay_ScrollContainerData scrollContainerData = Clay_GetScrollContainerData(CLAY_ID("main"));
        float maxScrollPosition = scrollContainerData.contentDimensions.height - scrollContainerData.scrollContainerDimensions.height;
        if (maxScrollPosition < 0) maxScrollPosition = 0;

        scrollOffset += Input::scrollDelta[1] * frameTimer.getTimeMs();
        if (dragging) {
            if (Input::mousePointer.isPressed) {
                scrollOffset -= Input::mousePointer.y - lastDragPosition[1];
                lastDragPosition = {static_cast<float>(Input::mousePointer.x), static_cast<float>(Input::mousePointer.y)};
            } else {
                dragging = false;
            }
        } else if (Input::mousePointer.isPressed) {
            dragging = true;
            lastDragPosition = {static_cast<float>(Input::mousePointer.x), static_cast<float>(Input::mousePointer.y)};
        }

        const Clay_BoundingBox selectedBoundingBox = Clay_GetElementData(CLAY_IDI("project-list-item", selectedProject)).boundingBox;
        const unsigned int selectedRow = std::floor(selectedProject / columns);
        if (selectedMoved) {
            if (selectedBoundingBox.y < 0) {
                if (selectedRow <= 1) scrollOffset = 0;
                else scrollOffset = -((padding * 2) + (60 * menuManager->scale * selectedRow) + (10 * menuManager->scale * (selectedRow - 2)) + (24 * menuManager->scale + 10 * menuManager->scale));
            } else if (selectedBoundingBox.y + selectedBoundingBox.height > windowHeight) {
                if (selectedRow == rows - 1) scrollOffset = -maxScrollPosition;
                else scrollOffset = -((padding * 2) + (60 * menuManager->scale * (selectedRow + 1)) + (10 * menuManager->scale * (selectedRow - 1)) + (24 * menuManager->scale + 10 * menuManager->scale)) + windowHeight;
            }
        }

        if (scrollOffset > 0) scrollOffset = 0;
        if (scrollOffset < -maxScrollPosition) scrollOffset = -maxScrollPosition;
    }

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
		.clip = { .horizontal = true, .vertical = true, .childOffset = { .y = scrollOffset} },
	}) {
		if (projects.empty()) {
			CLAY(CLAY_ID("no-projects-wrapper"), (Clay_ElementDeclaration){
				.layout = {
					.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
					.childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
					.layoutDirection = CLAY_TOP_TO_BOTTOM
				}
			}) {
				CLAY_TEXT(CLAY_STRING("You don't seem to have any projects."), CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
				CLAY_TEXT(noProjectsPath, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_16, .fontSize = static_cast<uint16_t>(16 * menuManager->scale) }));
			}
			continue; // The CLAY macro actually just makes a for loop so this just prevents the project row containers from rendering.
		}

		CLAY_TEXT(CLAY_STRING("Projects"), CLAY_TEXT_CONFIG({.textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(24 * menuManager->scale)}));

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
					components::renderProjectListItem(projects[i * columns + j], MenuManager::getImageData(missingIcon.get()), i * columns + j, CLAY_SIZING_FIXED(itemWidth), 0, menuManager, selectedProject == i * columns + j); // TODO: Implement text scrolling to see the full name, maybe only when hovered/selected?
				}
			}
		}
	}
    // clang-format on

    frameTimer.start();
}
