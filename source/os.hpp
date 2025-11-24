#pragma once
#include <chrono>
#include <string>

#ifdef __3DS__
#include <3ds.h>
#elif defined(WII)
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#elif defined(__WIIU__)
#include <sstream>
#include <whb/sdcard.h>
#elif defined(__NDS__)
#include <nds.h>
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
    static inline std::string getFilesystemRootPrefix() {
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
#elif defined(__PS4__)
        return "";
#elif defined(__3DS__)
        return "sdmc:";
#elif defined(__EMSCRIPTEN__)
        return "";
#elif defined(__NDS__)
        return isDSi() ? "sd:" : "fat:";
#else
        return "";
#endif
    }

    /**
     * Gets the location of the current device's Scratch data folder.
     * This is where the user should put their .sb3 Scratch projects.
     * This is also where all the extra data of the app lies (custom controls data, etc).
     * @return The string of the current device's Scratch data folder.
     */
    static inline std::string getScratchFolderLocation() {
        const std::string prefix = getFilesystemRootPrefix();
#ifdef __WIIU__
        return prefix + "/wiiu/scratch-wiiu/";
#elif defined(__SWITCH__)
        return "/switch/scratch-nx/";
#elif defined(WII)
        return "/apps/scratch-wii/";
#elif defined(GAMECUBE)
        return "/scratch-gamecube/";
#elif defined(VITA)
        return prefix + "data/scratch-vita/";
#elif defined(__PS4__)
        return "/data/scratch-ps4/";
#elif defined(__3DS__)
        return prefix + "/3ds/scratch-everywhere/";
#elif defined(__EMSCRIPTEN__)
        return "/scratch-everywhere/";
#elif defined(__NDS__)
        return prefix + "/scratch-ds/";
#else
        return "scratch-everywhere/";
#endif
    }

    /**
     * Gets the location of the `RomFS`, the embedded filesystem within the executable.
     * This function should be used whenever you need to load an asset from say, the `gfx` folder.
     * @return The location of the RomFS. On OGC, Switch, Wii U, and 3DS, this is `romfs:/`. everywhere else will be an empty string.
     */
    static inline std::string getRomFSLocation() {
#if defined(__WIIU__) || defined(__OGC__) || defined(__SWITCH__) || defined(__3DS__)
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
    static inline std::string getPlatform() {
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
#else
        return "Unknown";
#endif
    }

    /**
     * Function to detect whether the platform is a New 3DS.
     * @return `true` on New 3DS, `false` everywhere else.
     */
    static inline bool isNew3DS() {
#ifdef __3DS__
        bool out = false;
        APT_CheckNew3DS(&out);
        return out;
#endif
        return false;
    }

    /**
     * Function to detect whether the platform is a DSi.
     * @return `true` on DSi, `false` everywhere else.
     */
    static inline bool isDSi() {
#ifdef __NDS__
        return isDSiMode();
#endif
        return false;
    }
};
