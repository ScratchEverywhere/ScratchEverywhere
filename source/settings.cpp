#include "settings.hpp"
#include "os.hpp"
#include <fstream>

void SettingsManager::migrate() {
    try {
        OS::createDirectory(OS::getConfigFolderLocation());
    } catch (...) {
        Log::logError("Could not make config directory.");
        return;
    }
    if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation() && OS::fileExists(OS::getScratchFolderLocation() + "Settings.json")) {
        OS::renameFile(OS::getScratchFolderLocation() + "Settings.json", OS::getConfigFolderLocation() + "Settings.json");
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

        std::ofstream migrationOut(OS::getConfigFolderLocation() + "Settings.json");
        migrationOut << o.dump(4);
        migrationOut.close();
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
    nlohmann::json json;

    std::ifstream file(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    if (!file.good()) {
        Log::logWarning("Failed to open project config file: " + OS::getScratchFolderLocation() + projectName + ".sb3.json");
        return json;
    }

    file >> json;
    file.close();
    return json;
}

void SettingsManager::saveProjectSettings(const nlohmann::json &json, const std::string &projectName) {
    std::ofstream outFile(OS::getScratchFolderLocation() + projectName + ".sb3.json");
    outFile << json.dump(4);
    outFile.close();
}