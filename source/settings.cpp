#include "settings.hpp"
#include "os.hpp"
#include <fstream>
#include <nlohmann/json_fwd.hpp>

void SettingsManager::migrate() {
    auto potentialError = OS::createDirectory(OS::getConfigFolderLocation());
    if (!potentialError.has_value()) {
        Log::logError("Could not make config directory: " + potentialError.error());
        return;
    }

    if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation() && OS::fileExists(OS::getScratchFolderLocation() + "Settings.json")) {
        OS::renameFile(OS::getScratchFolderLocation() + "Settings.json", OS::getConfigFolderLocation() + "Settings.json");
    }
}

nlohmann::json SettingsManager::getConfigSettings() {
    migrate();

    nlohmann::json json;

    std::ifstream file(OS::getConfigFolderLocation() + "Settings.json");
    if (!file.good()) {
        Log::logWarning("Failed to open Config file: " + OS::getConfigFolderLocation() + "Settings.json");
        return json;
    }

    file >> json;
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

    file >> json;
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
