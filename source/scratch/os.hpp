#include <chrono>
#include <iostream>
#ifdef __3DS__
#include <3ds.h>
#endif
#ifdef __OGC__
#include <ogc/lwp_watchdog.h>
#include <ogc/system.h>
#endif
#pragma once

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

    // ---- Max VRAM values (just an estimate based on how many images i can load before i cant anymore) ----
    const static size_t old3ds_maxVRAMUsage = 30000000;   // ~30 MB
    const static size_t new3ds_maxVRAMUsage = 30000000;   // ~30 MB
    const static size_t wiiu_maxVRAMUsage = 67108864;     // 64 MB
    const static size_t wii_maxVRAMUsage = 44040192;      // 42 MB
    const static size_t gamecube_maxVRAMUsage = 11010048; // ~10 MB
    const static size_t pc_maxVRAMUsage = 134217728;      // 128 MB

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
        return pc_maxVRAMUsage;
    }

    // Raw allocation tracking
    static void allocate(size_t size) {
        totalAllocated += size;
        allocationCount++;

        if (totalAllocated > peakUsage) {
            peakUsage = totalAllocated;
        }
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

    static void deallocate(size_t size) {
        totalAllocated -= size;
        allocationCount--;
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
void writeToFile(std::string message, std::string filePath);
} // namespace Log

class Timer {
  private:
#ifdef __OGC__
    u64 startTime;
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
    static std::string getScratchFolderLocation();
};