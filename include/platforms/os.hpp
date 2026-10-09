#pragma once
#include <se_export.hpp>
#include <settings.hpp>
#include <string>

namespace OS {

extern bool toExit;
extern bool loadedSettings;
extern std::string *customProjectsPath;

/**
 * Initialize platform specific stuffs
 */
SE_EXPORT bool init();

/**
 * Deinit platform specific stuffs
 */
SE_EXPORT void deinit();

/**
 * @return `true` if the platform is a "pro" version of the platform, e.g: New 3DS, DSi
 */
SE_EXPORT bool isEnhancedPlatform();

/**
 * @return "sdmc:", etc
 */
SE_EXPORT std::string getFilesystemRootPrefix();

/**
 * If a custom Scratch folder path is defined, this returns that path. Otherwise returns an empty string.
 */
inline std::string getCustomScratchFolderLocation() {
    if (!loadedSettings) {
        loadedSettings = true;

        nlohmann::json json = SettingsManager::getConfigSettings();

        if (json.contains("ProjectsPath") && json["ProjectsPath"].is_string() && json.contains("UseProjectsPath") && json["UseProjectsPath"].is_boolean() && json["UseProjectsPath"] == true) customProjectsPath = new std::string(json["ProjectsPath"].get<std::string>());
    }

    if (customProjectsPath != nullptr) return customProjectsPath->back() == '/' ? *customProjectsPath : *customProjectsPath + "/";
    else return "";
}

/**
 * Gets the location of the current OS's config folder.
 * This is where all settings (both global, and project settings) are stored.
 * @return The string of the current OS's config folder.
 */
SE_EXPORT std::string getConfigFolderLocation();

/**
 * Gets the location of the current device's Scratch data folder.
 * This is where the user should put their .sb3 Scratch projects.
 * @return The string of the current device's Scratch data folder.
 */
SE_EXPORT std::string getScratchFolderLocation();

/**
 * Gets the location of the `RomFS`, the embedded filesystem within the executable.
 * This function should be used whenever you need to load an asset from say, the `gfx` folder.
 * @return The location of the RomFS. e.g: Switch and 3DS will be `romfs:/`.
 */
SE_EXPORT std::string getRomFSLocation();

/**
 * Normalizes paths for use with CMakeRC embedded filesystem (CMRC).
 * Strips OS::getRomFSLocation() and leading drive/romfs prefixes, converting backslashes to slashes.
 */
inline std::string normalizeCMRCPath(const std::string &path) {
    std::string p = path;
    for (char &c : p) {
        if (c == '\\') c = '/';
    }

    std::string prefix = getRomFSLocation();
    for (char &c : prefix) {
        if (c == '\\') c = '/';
    }

    if (!prefix.empty() && p.compare(0, prefix.length(), prefix) == 0) {
        p = p.substr(prefix.length());
    }

    if (p.compare(0, 9, "D:/romfs/") == 0) p = p.substr(9);
    else if (p.compare(0, 8, "D:/romfs") == 0) p = p.substr(8);
    else if (p.compare(0, 6, "romfs/") == 0) p = p.substr(6);
    else if (p.compare(0, 5, "romfs") == 0) p = p.substr(5);

    while (!p.empty() && (p[0] == '/' || p[0] == '\\')) {
        p = p.substr(1);
    }
    return p;
}

/**
 * Get the current platform that's running the app.
 * @return The string of the current platform. `3DS`, `Wii`, etc.
 */
SE_EXPORT std::string getPlatform();

/**
 * Checks if the device is connected to the internet.
 */
SE_EXPORT bool isOnline();

/**
 * Initializes the internet.
 */
SE_EXPORT bool initWifi();

/**
 * De-Initializes the internet.
 */
SE_EXPORT void deInitWifi();

/**
 * Gets the device's nickname, or the user's custom username if set.
 */
SE_EXPORT std::string getUsername();
} // namespace OS

#ifdef __XBOX__
#include "xbox/xbox_iostream_injector.hpp"
#endif
