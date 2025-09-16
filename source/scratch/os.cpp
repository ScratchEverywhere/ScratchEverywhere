#include "os.hpp"
#include "render.hpp"
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#ifdef __OGC__
#include <gccore.h>
#endif
#ifdef __WIIU__
#include <sstream>
#include <whb/sdcard.h>
#endif

size_t MemoryTracker::totalAllocated = 0;
size_t MemoryTracker::peakUsage = 0;
size_t MemoryTracker::allocationCount = 0;
size_t MemoryTracker::totalVRAMAllocated = 0;

void Log::log(std::string message, bool printToScreen) {
    if (printToScreen) std::cout << message << std::endl;
    writeToFile(message);
}

void Log::logWarning(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cout << "\x1b[1;33m" << "Warning: " << message << "\x1b[0m" << std::endl;
    writeToFile("Warning: " + message);
}

void Log::logError(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cerr << "\x1b[1;31m" << "Error: " << message << "\x1b[0m" << std::endl;

    writeToFile("Error: " + message);
}
void Log::writeToFile(std::string message) {
    if (Render::debugMode) {
        std::string filePath = OS::getScratchFolderLocation() + "log.txt";
        std::ofstream logFile;
        logFile.open(filePath, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        } else {
            std::cerr << "Could not open log file: " << filePath << std::endl;
        }
    }
}

// Wii and Gamecube Timer implementation
#ifdef __OGC__

Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = gettick();
}

int Timer::getTimeMs() {
    u64 currentTime = gettick();
    return ticks_to_millisecs(currentTime - startTime);
}

// everyone else...
#else
Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = std::chrono::high_resolution_clock::now();
}

int Timer::getTimeMs() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
    return static_cast<int>(duration.count());
}

#endif

bool Timer::hasElapsed(int milliseconds) {
    return getTimeMs() >= milliseconds;
}

bool Timer::hasElapsedAndRestart(int milliseconds) {
    if (hasElapsed(milliseconds)) {
        start();
        return true;
    }
    return false;
}

std::string OS::getScratchFolderLocation() {
#ifdef __WIIU__
    return std::string(WHBGetSdCardMountPath()) + "/wiiu/scratch-wiiu/";
#elif defined(__SWITCH__)
    return "/switch/scratch-nx/";
#elif defined(WII)
    return "/apps/scratch-wii/";
#elif defined(GAMECUBE)
    return "carda:/scratch-gamecube/";
#elif defined(VITA)
    return "ux0:data/scratch-vita/";
#elif defined(__3DS__)
    return "sdmc:/3ds/scratch-everywhere/";
#else
    return "scratch-everywhere/";
#endif
}

std::string OS::getRomFSLocation() {
#if defined(__WIIU__) || defined(__OGC__) || defined(__SWITCH__) || defined(__3DS__)
    return "romfs:/";
#else
    return "";
#endif
}

std::string OS::getPlatform() {
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
#else
    return "Unknown";
#endif
}

bool OS::isNew3DS() {
#ifdef __3DS__
    bool out = false;
    APT_CheckNew3DS(&out);
    return out;
#endif
    return false;
}
