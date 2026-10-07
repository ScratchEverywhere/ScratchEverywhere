#include "settings.hpp"
#include <filesystem.hpp>
#include <fstream>
#include <log.hpp>
#include <os.hpp>

#ifdef __MSDOS__
#include <sys/stat.h>
#endif

// static const std::string settings = "Config.json";
static const std::string settings_names[2] = {
    "Settings",
    "Config"
};

static const std::string extenstions[2] = {
    ".json",
    ".jso"
};

#if defined(__MSDOS__)
    static const std::string settings = settings_names[1] + extenstions[1];
    static const std::string extension = extenstions[1];
#else
    static const std::string settings = settings_names[0] + extenstions[0];
    static const std::string extension = extenstions[0];
#endif

void SettingsManager::migrate() {
    auto potentialError = FileSystem::createDirectory(OS::getConfigFolderLocation());
    if (!potentialError.has_value()) {
        Log::logCritical("Could not make config directory: " + potentialError.error(), false);
        return;
    }

    if (OS::getScratchFolderLocation() != OS::getConfigFolderLocation() && FileSystem::fileExists(OS::getScratchFolderLocation() + settings)) {
        FileSystem::renameFile(OS::getScratchFolderLocation() + settings, OS::getConfigFolderLocation() + settings);
    }
}

nlohmann::json SettingsManager::getConfigSettings() {
    migrate();

    nlohmann::json json = nlohmann::json::object();
    std::string path;

    path = OS::getConfigFolderLocation() + settings;

    std::ifstream file;
    file.open(path);
    if (file.eof()) {
        Log::logError("FILE EOF ERROR");
    } if (file.bad()) {
        Log::logError("FILE BAD ERROR");
    } if (file.fail()) {
        Log::logError("FILE FAIL ERROR");
    } if (!file.is_open()) {
        Log::logError("FILE NOT OPEN");
    }

    if (!file.good()) {
        Log::logWarning("Failed to open Config file: " + path);
        return json;
    }

    file >> json;
    file.close();

    return json;
}

void SettingsManager::saveConfigSettings(const nlohmann::json &json) {
    std::ofstream outFile(OS::getConfigFolderLocation() + settings);
    outFile << json.dump(4);
    outFile.close();
}

nlohmann::json SettingsManager::getProjectSettings(const std::string &projectName) {
    nlohmann::json json = nlohmann::json::object();

    std::ifstream file(OS::getScratchFolderLocation() + projectName + ".sb3" + extension);
    if (!file.good()) {
        Log::logWarning("Failed to open project config file: " + OS::getScratchFolderLocation() + projectName + ".sb3" + extension);
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
    std::ofstream outFile(OS::getScratchFolderLocation() + projectName + ".sb3" + extension);
    outFile << json.dump(4);
    outFile.close();
}
