#pragma once
#include <chrono>
#include <iostream>
#ifdef __3DS__
#include <3ds.h>
#endif
#ifdef WII
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#endif

namespace Log {
void log(std::string message, bool printToScreen = true);
void logWarning(std::string message, bool printToScreen = true);
void logError(std::string message, bool printToScreen = true);
void writeToFile(std::string message);
} // namespace Log

class Timer {
  private:
#if defined(__NDS__) || defined(WII) || defined(__PS4__)
    uint64_t startTime;
#else
    std::chrono::high_resolution_clock::time_point startTime;
#endif

  public:
    Timer();
    /**
     * Starts the clock.
     */
    void start();
    /**
     * Gets the amount of time passed in milliseconds.
     * @return time passed (in ms)
     */
    int getTimeMs();
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

class OS {
  public:
    /**
     * Gets the drive prefix for the current platform.
     * @return The string of the current platform's drive prefix. (e.g. "sdmc:" on 3DS)
     **/
    static std::string getFilesystemRootPrefix();

    /**
     * Gets the location of the current device's Scratch data folder.
     * This is where the user should put their .sb3 Scratch projects.
     * This is also where all the extra data of the app lies (custom controls data, etc).
     * @return The string of the current device's Scratch data folder.
     */
    static std::string getScratchFolderLocation();

    /**
     * Gets the location of the `RomFS`, the embedded filesystem within the executable.
     * This function should be used whenever you need to load an asset from say, the `gfx` folder.
     * @return The location of the RomFS. On OGC, Switch, Wii U, and 3DS, this is `romfs:/`. everywhere else will be an empty string.
     */
    static std::string getRomFSLocation();
    /**
     * Get the current platform that's running the app.
     * @return The string of the current platform. `3DS`, `Wii`, etc.
     */
    static std::string getPlatform();
    /**
     * Function to detect whether the platform is a New 3DS.
     * @return `true` on New 3DS, `false` everywhere else.
     */
    static bool isNew3DS();

    /**
     * Function to detect whether the platform is a DSi.
     * @return `true` on DSi, `false` everywhere else.
     */
    static bool isDSi();

    /**
     * Create a directory.
     * @param path The path of the directory to create.
     */
    static void createDirectory(const std::string& path);

    /**
     * Remove a directory recursively.
     * @param path The path of the directory to remove.
     */
    static void removeDirectory(const std::string& path);

    /**
     * Check if a file exists.
     * @param path The path of the file to check.
     * @return `true` if the file exists, `false` otherwise.
     */
    static bool fileExists(const std::string& path);

    /**
     * Get the parent path of a given path.
     * @param path The path of the file or directory.
     * @return The parent path of the given path.
     */
    static std::string parentPath(const std::string& path);
};
