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

class MemoryTracker {
  private:
    static size_t totalAllocated;
    static size_t totalVRAMAllocated;
    static size_t peakUsage;
    static size_t allocationCount;

    // ---- Max RAM values (max usable ram minus a slight safety margin) ----
    const static size_t old3ds_maxRamUsage = 50331648;   // 48 MB
    const static size_t new3ds_maxRamUsage = 100663296;  // 96 MB
    const static size_t wiiu_maxRamUsage = 805306368;    // 768 MB
    const static size_t wii_maxRamUsage = 86900736;      // 83 MB
    const static size_t gamecube_maxRamUsage = 23068672; // 22 MB
    const static size_t pc_maxRamUsage = 1073741824;     // 1 GB
    const static size_t vita_maxRamUsage = 335544320;    // 320 MB
    const static size_t ps4_maxRamUsage = 1073741824;     // 1 GB

    // ---- Max VRAM values (just an estimate based on how many images i can load before i cant anymore) ----
    const static size_t old3ds_maxVRAMUsage = 30000000;   // ~30 MB
    const static size_t new3ds_maxVRAMUsage = 30000000;   // ~30 MB
    const static size_t wiiu_maxVRAMUsage = 67108864;     // 64 MB
    const static size_t wii_maxVRAMUsage = 44040192;      // 42 MB
    const static size_t gamecube_maxVRAMUsage = 11010048; // ~10 MB
    const static size_t pc_maxVRAMUsage = 134217728;      // 128 MB
    const static size_t vita_maxVRAMUsage = 100663296;    // 96 MB
    const static size_t ps4_maxVRAMUsage = 134217728;      // 128 MB

  public:
    static size_t getMaxRamUsage() {
#ifdef __3DS__
        bool isNew3DS = false;
        APT_CheckNew3DS(&isNew3DS);
        if (isNew3DS)
            return new3ds_maxRamUsage;
        else
            return old3ds_maxRamUsage;
#endif
#ifdef __WIIU__
        return wiiu_maxRamUsage;
#endif
#ifdef WII
        return wii_maxRamUsage;
#endif
#ifdef GAMECUBE
        return gamecube_maxRamUsage;
#endif
#ifdef VITA
        return vita_maxRamUsage;
#endif
#ifdef __PS4__
        return ps4_maxRamUsage;
#endif
        return pc_maxRamUsage;
    }

    static size_t getMaxVRAMUsage() {
#ifdef __3DS__
        bool isNew3DS = false;
        APT_CheckNew3DS(&isNew3DS);
        if (isNew3DS)
            return new3ds_maxVRAMUsage;
        else
            return old3ds_maxVRAMUsage;
#endif
#ifdef __WIIU__
        return wiiu_maxVRAMUsage;
#endif
#ifdef WII
        return wii_maxVRAMUsage;
#endif
#ifdef GAMECUBE
        return gamecube_maxVRAMUsage;
#endif
#ifdef VITA
        return vita_maxVRAMUsage;
#endif
#ifdef __PS4__
        return ps4_maxVRAMUsage;
#endif
        return pc_maxVRAMUsage;
    }

    // Raw allocation tracking
    static void *allocate(size_t size) {
        void *ptr = malloc(size);
        if (ptr) {
            totalAllocated += size;
            allocationCount++;

            if (totalAllocated > peakUsage) {
                peakUsage = totalAllocated;
            }
        }
        return ptr;
    }

    static void allocateVRAM(size_t size) {
        totalVRAMAllocated += size;
    }
    static void deallocateVRAM(size_t size) {
        totalVRAMAllocated -= size;
    }

    static size_t getVRAMUsage() {
        return totalVRAMAllocated;
    }

    static void deallocate(void *ptr, size_t size) {
        if (ptr) {
            totalAllocated -= size;
            allocationCount--;
            free(ptr);
        }
    }

    // Template versions for type safety
    template <typename T>
    static T *allocate(size_t count = 1) {
        size_t size = count * sizeof(T);
        T *ptr = static_cast<T *>(malloc(size));
        if (ptr) {
            totalAllocated += size;
            allocationCount++;

            if (totalAllocated > peakUsage) {
                peakUsage = totalAllocated;
            }
        }
        return ptr;
    }

    template <typename T>
    static void deallocate(T *ptr, size_t count = 1) {
        if (ptr) {
            size_t size = count * sizeof(T);
            totalAllocated -= size;
            allocationCount--;
            free(ptr);
        }
    }

    static size_t getCurrentUsage() {
        return totalAllocated;
    }

    static size_t getPeakUsage() {
        return peakUsage;
    }

    static size_t getAllocationCount() {
        return allocationCount;
    }
};

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
};
