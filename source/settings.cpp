#include "settings.hpp"
#include "os.hpp"
#include <fstream>

void SettingsManager::migrate() {
    OS::createDirectory(OS::getConfigFolderLocation());
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
    return std::move(json);
}

void SettingsManager::saveConfigSettings(const nlohmann::json &json) {
    std::ofstream outFile(OS::getConfigFolderLocation() + "Settings.json");
    outFile << json.dump(4);
    outFile.close();
}

nlohmann::json SettingsManager::getProjectSettings(const std::string &projectName) {
    nlohmann::json json;

    std::ifstream file(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    if (!file.good()) {
        Log::logWarning("Failed to open project config file: " + OS::getScratchFolderLocation() + projectName + ".sb3.json");
        return json;
    }

    file >> json;
    file.close();
    return std::move(json);
}

void SettingsManager::saveProjectSettings(const nlohmann::json &json, const std::string &projectName) {
    std::ofstream outFile(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    outFile << json.dump(4);
    outFile.close();
}