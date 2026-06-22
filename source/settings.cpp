#include "settings.hpp"
#include <filesystem.hpp>
#include <fstream>
#include <log.hpp>
#include <os.hpp>

void SettingsManager::migrate() {
    auto potentialError = FileSystem::createDirectory(OS::getConfigFolderLocation());
    if (!potentialError.has_value()) {
        Log::logError("Could not make config directory: " + potentialError.error());
        return;
    }

    if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation() && FileSystem::fileExists(OS::getScratchFolderLocation() + "Settings.json")) {
        FileSystem::renameFile(OS::getScratchFolderLocation() + "Settings.json", OS::getConfigFolderLocation() + "Settings.json");
    }

    // Global Settings
    std::ifstream migrationIn(OS::getConfigFolderLocation() + "Settings.json");
    if (migrationIn.good()) {
        nlohmann::json i;

        migrationIn >> i;
        migrationIn.close();

        nlohmann::json o = i;

        if (i.contains("EnableUsername") && i["EnableUsername"].is_boolean()) {
            o["useCustomUsername"] = i["EnableUsername"];
            o.erase("EnableUsername");
        }

        if (i.contains("Username") && i["Username"].is_string()) {
            o["customUsername"] = i["Username"];
            o.erase("Username");
        }

        if (i.contains("UseDectalk") && i["UseDectalk"].is_boolean()) {
            o["useDectalk"] = i["UseDectalk"];
            o.erase("UseDectalk");
        }

        std::ofstream migrationOut(OS::getConfigFolderLocation() + "Settings.json");
        migrationOut << o.dump(4);
        migrationOut.close();
    }
}

nlohmann::json SettingsManager::getConfigSettings() {
    migrate();

    nlohmann::json json = nlohmann::json::object();

    std::ifstream file(OS::getConfigFolderLocation() + "Settings.json");
    if (!file.good()) {
        Log::logWarning("Failed to open Config file: " + OS::getConfigFolderLocation() + "Settings.json");
        return json;
    }

    try {
        file >> json;
    } catch (const nlohmann::json::parse_error &e) {
        Log::logError("Failed to parse project settings json: " + std::string(e.what()));
        file.close();
        return json;
    }

    file.close();
    return json;
}

void SettingsManager::saveConfigSettings(const nlohmann::json &json) {
    std::ofstream outFile(OS::getConfigFolderLocation() + "Settings.json");
    outFile << json.dump(4);
    outFile.close();
}

nlohmann::json SettingsManager::getProjectSettings(const std::string &projectName) {
    nlohmann::json json = nlohmann::json::object();

    std::ifstream file(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    if (!file.good()) {
        Log::logWarning("Failed to open project config file: " + OS::getScratchFolderLocation() + projectName + ".sb3.json");
        if (!json.contains("settings")) json["settings"] = nlohmann::json::object();
        return json;
    }

    try {
        file >> json;
    } catch (const nlohmann::json::parse_error &e) {
        Log::logError("Failed to parse project settings json: " + std::string(e.what()));
        file.close();
        return json;
    }

    file.close();

    if (!json.is_object()) json = nlohmann::json::object();
    if (!json.contains("settings")) json["settings"] = nlohmann::json::object();
    return json;
}

void SettingsManager::saveProjectSettings(const nlohmann::json &json, const std::string &projectName) {
    std::ofstream outFile(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    outFile << json.dump(4);
    outFile.close();
}

bool SettingsManager::isProjectUnpacked(const std::string &projectName) {
    nlohmann::json json;

    std::ifstream file(OS::getScratchFolderLocation() + "UnpackedGames.json");
    if (!file.good()) {
        return false;
    }

    try {
        file >> json;
    } catch (const nlohmann::json::parse_error &e) {
        Log::logError("Failed to parse project settings json: " + std::string(e.what()));
        file.close();
        return false;
    }

    file.close();

    if (json.contains("items") && json["items"].is_array()) {
        auto &items = json["items"];
        if (std::find(items.begin(), items.end(), projectName) != items.end()) {
            return true;
        }
    }

    return false;
}
