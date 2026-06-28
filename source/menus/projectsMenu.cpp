#include "projectsMenu.hpp"
#include "../input.hpp"
#include "filesystem.hpp"
#include "image.hpp"
#include "log.hpp"
#include "menuManager.hpp"
#include "menus/components.hpp"
#include "os.hpp"
#include "parser.hpp"
#include "runtime.hpp"
#include "thread.hpp"
#include "unzip.hpp"
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <render.hpp>
#include <string>

#ifdef RENDERER_CITRO2D
constexpr unsigned int windowWidth = 320;
constexpr unsigned int windowHeight = 240;
#endif

ProjectsMenu::ProjectsMenu(void *userdata) {
    const auto maybe = createImageFromFile(OS::getRomFSLocation() + "gfx/menu/noicon.svg", false);
    if (!maybe.has_value()) {
        Log::logError("Failed to load missing image: " + maybe.error());
    }
    missingIcon = maybe.value();

    const std::string path = OS::getScratchFolderLocation();

    if (FileSystem::fileExists(path)) {
        const auto maybe = FileSystem::listDirectory(path);
        if (!maybe.has_value()) {
            Log::logError("Failed to list projects: " + maybe.error());
        } else {
            {
                std::queue<components::ProjectInfo *> temp;
                backdropQueue.swap(temp);
            }

            for (const auto &entry : maybe.value()) {
                if ((entry.size() >= 4 && entry.compare(entry.size() - 4, 4, ".sb3")) && !(FileSystem::fileExists(path + entry + "/project.json") || (entry.back() == '/' && FileSystem::fileExists(path + entry + "project.json")))) continue;

                std::string projectName = entry;
                std::string displayName = entry;
                bool unpacked = false;
                if (projectName.size() >= 4 && projectName.compare(projectName.size() - 4, 4, ".sb3") == 0) {
                    projectName = projectName.substr(0, projectName.size() - 4);
                    displayName = projectName;
                } else {
                    unpacked = true;
                    displayName = "⚡ " + displayName + " ⚡";
                }

                projects.push_back({.name = projectName, .path = path + entry, .displayName = displayName, .unpacked = unpacked});
                projects.back().image = getProjectIcon(projects.back(), unpacked);
            }

#ifndef __PS4__
            backdropQueueMutex.lock();
            for (auto &project : projects) {
                if (project.image == nullptr) {
                    backdropQueue.push(&project);
                }
            }
            backdropQueueMutex.unlock();

            if (backdropIconThread.create(backdropLoader, this, 0x4000, 0, -1, "BackdropLoader")) {
                threadSpawned = true;
            }
#endif
        }
    }

    backdropQueueMutex.lock();
    doneLoading = true;
    backdropQueueMutex.unlock();

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

void ProjectsMenu::backdropLoader(void *userdata) {
    ProjectsMenu *menu = static_cast<ProjectsMenu *>(userdata);

    while (true) {
        components::ProjectInfo *project = nullptr;
        bool shouldExit = false;
        menu->backdropQueueMutex.lock();

        if (menu->forceExit) {
            shouldExit = true;
        } else if (!menu->backdropQueue.empty()) {
            project = menu->backdropQueue.front();
            menu->backdropQueue.pop();
        } else if (menu->doneLoading) {
            shouldExit = true;
        }

        menu->backdropQueueMutex.unlock();
        if (shouldExit) break;
        if (project == nullptr) {
            SE_Thread::sleep(50);
            continue;
        }

        nlohmann::json project_json;

        if (!project->unpacked) {
            Scratch::sb3InRam = true;
            auto &zip = Unzip::zipArchive;
            memset(&zip, 0, sizeof(zip));

            if (!mz_zip_reader_init_file(&zip, project->path.c_str(), 0)) {
                Log::logError("Could not find project: " + project->path);
                continue;
            }
            int fileIndex = mz_zip_reader_locate_file(&zip, "project.json", nullptr, 0);

            if (fileIndex < 0) {
                Log::logError("Could not find project.json: " + project->name);
                mz_zip_reader_end(&zip);
                continue;
            }

            size_t extractedSize = 0;
            void *data = mz_zip_reader_extract_to_heap(&zip, fileIndex, &extractedSize, 0);

            if (!data) {
                Log::logError("Could not extract project.json: " + project->name);
                mz_zip_reader_end(&zip);
                continue;
            }

            const char *begin = static_cast<const char *>(data);
            const char *end = begin + extractedSize;
            project_json = nlohmann::json::parse(begin, end);
            mz_free(data);
            mz_zip_reader_end(&zip);
        } else {
            const std::string &basePath = project->path.back() == '/' ? project->path : project->path + '/';

            Unzip::UnpackedInSD = true;
            Unzip::filePath = basePath;

            std::ifstream f(basePath + "project.json");
            if (!f.is_open()) {
                Log::logError("Could not open project.json: " + basePath + "project.json");
                continue;
            }
            f >> project_json;
        }

        for (const auto &target : project_json["targets"]) {
            if (!target.contains("isStage") || !target["isStage"].get<bool>()) continue;
            if (!target.contains("costumes") || target["costumes"].empty()) continue;
            for (const auto &costume : target["costumes"]) {
                if (!costume.contains("md5ext") || !costume.contains("name")) continue;
                if (costume["name"] != "__icon__") continue;

                menu->backdropQueueMutex.lock();
                menu->imagePaths[project] = costume["md5ext"];
                menu->backdropQueueMutex.unlock();
                break;
            }
            if (menu->imagePaths.contains(project)) break;
        }
    }
}

std::shared_ptr<Image> ProjectsMenu::getProjectIcon(components::ProjectInfo &project, bool unpacked) {
    static const std::array<std::string, 4> imageFormats = {".svg", ".png", ".jpg", ".jpeg"};

    // External File Check
    for (const auto &format : imageFormats) {
        const std::string path = OS::getScratchFolderLocation() + project.name + format;
        if (!FileSystem::fileExists(path)) continue;

        const auto maybe = createImageFromFile(path, false);
        if (!maybe.has_value()) {
            Log::logError("Error while loading project icon for '" + project.name + "': " + maybe.error());
            continue;
        }
        const auto &image = maybe.value();

        if (image->getWidth() != image->getHeight()) {
            Log::logError("Project icon dimensions must be square: " + path);
            continue;
        }

        return image;
    }

    return nullptr;
}

ProjectsMenu::~ProjectsMenu() {
    if (threadSpawned) {
        backdropQueueMutex.lock();
        forceExit = true;
        backdropQueueMutex.unlock();

        backdropIconThread.join();
    }

    if (noProjectsPath.chars != nullptr) free(const_cast<char *>(noProjectsPath.chars));
}

void ProjectsMenu::render() {
    backdropQueueMutex.lock();
    std::vector<components::ProjectInfo *> toRemove;
    for (auto &[project, path] : imagePaths) {
        std::shared_ptr<Image> image = nullptr;

        if (project->unpacked) {
            auto imageOrErr = createImageFromFile(path, true);
            if (!imageOrErr.has_value()) {
                Log::logError("Failed to load image: " + path);
                toRemove.push_back(project);
                continue;
            }
            image = imageOrErr.value();
        } else {
            auto imageOrErr = createImageFromZip(path, &Unzip::zipArchive);
            if (!imageOrErr.has_value()) {
                Log::logError("Failed to load image: " + path);
                toRemove.push_back(project);
                continue;
            }
            image = imageOrErr.value();
        }

        if (image->getWidth() != image->getHeight()) {
            Log::logError("Project icon dimensions must be square: " + project->name);
            imagePaths.erase(project);
            continue;
        }

        project->image = image;
        toRemove.push_back(project);
    }
    for (auto *project : toRemove) {
        imagePaths.erase(project);
    }
    backdropQueueMutex.unlock();

    static Timer frameTimer;

    constexpr unsigned int maxColumns = 6;
    const float itemWidth = 150 * menuManager->scale;
    const unsigned int gap = 10 * menuManager->scale;
    const uint16_t padding = 15 * menuManager->scale;

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

    std::optional<Clay_BoundingBox> selectedBoundingBox = std::nullopt;
    if (selectedMoved && !projects.empty() && selectedProject != -1)
        selectedBoundingBox = Clay_GetElementData(CLAY_IDI("project-list-item", selectedProject)).boundingBox;

    const float scrollOffset = getScrollOffset(frameTimer, selectedBoundingBox);

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
					const auto &projectData = projects[i * columns + j];
					components::renderProjectListItem(projectData, projectData.image != nullptr ? projectData.image : missingIcon, i * columns + j, CLAY_SIZING_FIXED(itemWidth), menuManager, selectedProject == i * columns + j);
				}
			}
		}
	}
    // clang-format on

    frameTimer.start();
}
