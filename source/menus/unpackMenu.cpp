#include "unpackMenu.hpp"
#include "menuManager.hpp"
#include <clay.h>
#include <cstring>
#include <fstream>
#include <input.hpp>
#include <runtime.hpp>
#include <settings.hpp>

static bool threadFinished = false;

void UnpackMenu::unpack(UnpackParams params) {
    if (!params.deletingProject) {
        if (Unzip::extractProject(OS::getScratchFolderLocation() + params.projectName + ".sb3", OS::getScratchFolderLocation() + params.projectName)) {
            addToJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json", params.projectName);
            nlohmann::json settings = SettingsManager::getProjectSettings(params.projectName);
            settings["unpackedExists"] = true;
            SettingsManager::saveProjectSettings(settings, params.projectName);
        }
    } else {
        if (Unzip::deleteProjectFolder(OS::getScratchFolderLocation() + params.projectName)) {
            removeFromJsonArray(OS::getScratchFolderLocation() + "UnpackedGames.json", params.projectName);
            nlohmann::json settings = SettingsManager::getProjectSettings(params.projectName);
            settings["unpackedExists"] = false;
            SettingsManager::saveProjectSettings(settings, params.projectName);
        }
    }
    threadFinished = true;
}

UnpackMenu::UnpackMenu(void *userdata, const std::string &title) {
    // there's definitely better ways of doing this..
    UnpackParams *paramsPtr = static_cast<UnpackParams *>(userdata);
    UnpackParams params = {.projectName = paramsPtr->projectName, .deletingProject = paramsPtr->deletingProject};
    delete paramsPtr;

    projectName = params.projectName;
    deletingProject = params.deletingProject;

    this->title = {false, static_cast<int32_t>(title.length()), nullptr};
    void *chars = malloc(title.length());
    memcpy(chars, title.c_str(), title.length());
    this->title.chars = static_cast<char *>(chars);

    thread = std::thread(unpack, params);
    if (!thread.joinable()) {
        unpack(params);
    } else thread.detach();
}

UnpackMenu::~UnpackMenu() {
    free(const_cast<char *>(title.chars));
}

void UnpackMenu::addToJsonArray(const std::string &filePath, const std::string &value) {
    nlohmann::json j;

    std::ifstream inFile(filePath);
    if (inFile) {
        inFile >> j;
    }
    inFile.close();

    if (!j.contains("items") || !j["items"].is_array()) {
        j["items"] = nlohmann::json::array();
    }

    j["items"].push_back(value);

    OS::createDirectory(OS::parentPath(filePath));

    std::ofstream outFile(filePath);
    if (!outFile) {
        Log::logError("Failed to write JSON file: " + filePath);
        return;
    }
    outFile << j.dump(2);
    outFile.close();
}

std::vector<std::string> UnpackMenu::getJsonArray(const std::string &filePath) {
    std::vector<std::string> result;
    std::ifstream inFile(filePath);
    if (!inFile) return result;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        for (const auto &el : j["items"]) {
            result.push_back(el.get<std::string>());
        }
    }
    return result;
}

void UnpackMenu::removeFromJsonArray(const std::string &filePath, const std::string &value) {
    std::ifstream inFile(filePath);
    if (!inFile) return;

    nlohmann::json j;
    inFile >> j;
    inFile.close();

    if (j.contains("items") && j["items"].is_array()) {
        auto &arr = j["items"];
        arr.erase(std::remove(arr.begin(), arr.end(), value), arr.end());
    }

    std::ofstream outFile(filePath);
    if (!outFile) return;
    outFile << j.dump(2);
    outFile.close();
}

void UnpackMenu::render() {
    menuManager->canChangeMenus = false;

    const uint16_t padding = 15 * menuManager->scale;

    // clang-format off
	CLAY(CLAY_ID("main"), (Clay_ElementDeclaration){
		.layout = {
			.sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
			.padding = {padding, padding, padding, padding},
			.childGap = static_cast<uint16_t>(15 * menuManager->scale),
			.layoutDirection = CLAY_TOP_TO_BOTTOM
		},
		.backgroundColor = { 115, 75, 115, 255 },
		.cornerRadius = { 15 * menuManager->scale, 0, 15 * menuManager->scale, 0 }
	}) {
		CLAY(CLAY_ID_LOCAL("title-wrapper"), (Clay_ElementDeclaration){
			.layout = {
				.sizing = { .width = CLAY_SIZING_GROW(0) },
				.childAlignment = { .x = CLAY_ALIGN_X_CENTER }
			}
		}) {
			CLAY_TEXT(title, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(24 * menuManager->scale) }));
		}

        CLAY(CLAY_ID_LOCAL("texty"),(Clay_ElementDeclaration){}) {
            Clay_String str = {false, static_cast<int>(text.size()),text.c_str()};
            CLAY_TEXT(str, CLAY_TEXT_CONFIG({ .textColor = {255, 255, 255, 255}, .fontId = components::FONT_ID_BODY_BOLD_48, .fontSize = static_cast<uint16_t>(24 * menuManager->scale) }));
        }

	}
    // clang-format on

    if (threadFinished) {
        menuManager->canChangeMenus = true;
        menuManager->queueChangeMenu(MenuID::ProjectsMenu);
        return;
    }
}
