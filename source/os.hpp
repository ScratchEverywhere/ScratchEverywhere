#pragma once
#include <chrono>
#include <filesystem>
#include <iostream>
#include <nonstd/expected.hpp>
#include <stdexcept>
#include <string>
#ifdef __3DS__
#include <3ds.h>
#elif defined(WII)
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#elif defined(__NDS__)
#include <nds.h>
#elif defined(__WIIU__)
#include <whb/sdcard.h>
#elif defined(__PS2__)
#include <kernel.h>
#endif

#if defined(_WIN32)
#include <shlobj.h>
#include <windows.h>
#elif defined(__HAIKU__)
#include <FindDirectory.h>
#include <Path.h>
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace Log {
void log(std::string message, bool printToScreen = true);
void logWarning(std::string message, bool printToScreen = true);
void logError(std::string message, bool printToScreen = true);
void writeToFile(std::string message);
void deleteLogFile();
} // namespace Log

class Timer {
  private:
#if defined(__NDS__) || defined(WII) || defined(__PS4__)
    uint64_t startTime;
#else
    std::chrono::high_resolution_clock::time_point startTime;
#endif

  public:
    Timer(const bool autoStart = true);
    /**
     * Starts the clock.
     */
    void start();
    /**
     * Gets the amount of time passed in milliseconds.
     * @return time passed (in ms)
     */
    int getTimeMs();

    double getTimeMsDouble();
    /**
     * Checks if enough time, in milliseconds, has passed since the timer started.
     * @return True if enough time has passed, False otherwise.
     */
    bool hasElapsed(int ms);
    /**
     * Checks if enough time, in milliseconds, has passed since the timer started, and automatically restarts if true.
     * @return True if enough time has passed, False otherwise.
     */
    bool hasElapsedAndRestart(int ms);
};

namespace OS {

extern bool toExit;

/**
 * Function to detect whether the platform is a DSi.
 * @return `true` on DSi, `false` everywhere else.
 */
inline bool isDSi() {
#ifdef __NDS__
    return isDSiMode();
#endif
    return false;
}

inline std::string getFilesystemRootPrefix() {
#ifdef __WIIU__
    return std::string(WHBGetSdCardMountPath());
#elif defined(__SWITCH__)
    return "";
#elif defined(WII)
    return "";
#elif defined(GAMECUBE)
    return "";
#elif defined(VITA)
    return "ux0:";
#elif defined(__PS2__)
    return "mc0:/";
#elif defined(__PS4__)
    return "";
#elif defined(__3DS__)
    return "sdmc:";
#elif defined(__EMSCRIPTEN__)
    return "";
#elif defined(__NDS__)
    return isDSi() ? "sd:" : "fat:";
#elif defined(_WIN32) || defined(_WIN64)
    return std::filesystem::path(std::getenv("SystemDrive")).string();
#else
    return "";
#endif
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
 * @return The location of the RomFS. On Switch, Wii U, and 3DS, this is `romfs:/`. everywhere else will be an empty string.
 */
inline std::string getRomFSLocation() {
#if defined(__WIIU__) || defined(__SWITCH__) || defined(__3DS__)
    return "romfs:/";
#elif defined(__EMSCRIPTEN__)
    return "/romfs/";
#elif defined(__PS4__)
    return "/app0/";
#else
    return "";
#endif
}

/**
 * Get the current platform that's running the app.
 * @return The string of the current platform. `3DS`, `Wii`, etc.
 */
inline std::string getPlatform() {
#if defined(__3DS__)
    return "3DS";
#elif defined(__WIIU__)
    return "Wii U";
#elif defined(__PC__)
    return "PC";
#elif defined(GAMECUBE)
    return "GameCube";
#elif defined(WII)
    return "Wii";
#elif defined(__SWITCH__)
    return "Switch";
#elif defined(VITA)
    return "Vita";
#elif defined(__NDS__)
    return "DS";
#elif defined(__EMSCRIPTEN__)
    return "WASM";
#elif defined(__PS4__)
    return "PS4";
#elif defined(__PSP__)
    return "PSP";
#elif defined(WEBOS)
    return "webOS TV";
#elif defined(__PS2__)
    return "PS2";
#else
    return "Unknown";
#endif
}

/**
 * Function to detect whether the platform is a New 3DS.
 * @return `true` on New 3DS, `false` everywhere else.
 */
inline bool isNew3DS() {
#ifdef __3DS__
    bool out = false;
    APT_CheckNew3DS(&out);
    return out;
#endif
    return false;
}

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

/**
 * Create a directory.
 * @param path The path of the directory to create.
 */
nonstd::expected<void, std::string> createDirectory(const std::string &path);

/**
 * Renames/moves a file.
 * @param originalPath The original/current path of the file.
 * @param newPath The path to move/rename the file to.
 */
void renameFile(const std::string &originalPath, const std::string &newPath);

/**
 * Remove a directory recursively.
 * @param path The path of the directory to remove.
 */
nonstd::expected<void, std::string> removeDirectory(const std::string &path);

/**
 * Check if a file exists.
 * @param path The path of the file to check.
 * @return `true` if the file exists, `false` otherwise.
 */
bool fileExists(const std::string &path);

/**
 * Get the parent path of a given path.
 * @param path The path of the file or directory.
 * @return The parent path of the given path.
 */
std::string parentPath(const std::string &path);
}; // namespace OS
