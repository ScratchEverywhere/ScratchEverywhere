#pragma once
#include <settings.hpp>
#include <string>

namespace OS {

extern bool toExit;
extern bool loadedSettings;
extern std::string *customProjectsPath;

/**
 * Initialize platform specific stuffs
 */
bool init();

/**
 * Deinit platform specific stuffs
 */
void deinit();

/**
 * @return `true` if the platform is a "pro" version of the platform, e.g: New 3DS, DSi
 */
bool isEnhancedPlatform();

/**
 * @return "sdmc:", etc
 */
std::string getFilesystemRootPrefix();

/**
 * If a custom Scratch folder path is defined, this returns that path. Otherwise returns an empty string.
 */
inline std::string getCustomScratchFolderLocation() {
    if (!loadedSettings) {
        loadedSettings = true;

        nlohmann::json json = SettingsManager::getConfigSettings();

        if (json.contains("ProjectsPath") && json["ProjectsPath"].is_string() && json.contains("UseProjectsPath") && json["UseProjectsPath"].is_boolean() && json["UseProjectsPath"] == true) customProjectsPath = new std::string(json["ProjectsPath"].get<std::string>());
    }

    if (customProjectsPath != nullptr) return *customProjectsPath;
    else return "";
}

/**
 * Gets the location of the current OS's config folder.
 * This is where all settings (both global, and project settings) are stored.
 * @return The string of the current OS's config folder.
 */
std::string getConfigFolderLocation();

/**
 * Gets the location of the current device's Scratch data folder.
 * This is where the user should put their .sb3 Scratch projects.
 * @return The string of the current device's Scratch data folder.
 */
std::string getScratchFolderLocation();

/**
 * Gets the location of the `RomFS`, the embedded filesystem within the executable.
 * This function should be used whenever you need to load an asset from say, the `gfx` folder.
 * @return The location of the RomFS. e.g: Switch and 3DS will be `romfs:/`.
 */
std::string getRomFSLocation();

/**
 * Get the current platform that's running the app.
 * @return The string of the current platform. `3DS`, `Wii`, etc.
 */
std::string getPlatform();

/**
 * Checks if the device is connected to the internet.
 */
bool isOnline();

/**
 * Initializes the internet.
 */
bool initWifi();

/**
 * De-Initializes the internet.
 */
void deInitWifi();

/**
 * Gets the device's nickname, or the user's custom username if set.
 */
std::string getUsername();
} // namespace OS