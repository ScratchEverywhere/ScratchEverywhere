#include "os.hpp"
#include "render.hpp"
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <malloc.h>
#include <ostream>
#include <string>
#ifdef __WIIU__
#include <sstream>
#include <whb/sdcard.h>
#endif
#ifdef WII
#include <gccore.h>
#endif
#ifdef __NDS__
#include <nds.h>
#endif
#ifdef __PS4__
#include <orbis/libkernel.h>
#endif

// PS4 implementation of logging
#ifdef __PS4__
char logBuffer[1024];

void Log::log(std::string message, bool printToScreen) {
    if (printToScreen) {
        snprintf(logBuffer, 1023, "<SE!> %s\n", message.c_str());
        sceKernelDebugOutText(0, logBuffer);
    }
}
void Log::logWarning(std::string message, bool printToScreen) {
    if (printToScreen) {
        snprintf(logBuffer, 1023, "<SE!> Warning: %s\n", message.c_str());
        sceKernelDebugOutText(0, logBuffer);
    }
}
void Log::logError(std::string message, bool printToScreen) {
    if (printToScreen) {
        snprintf(logBuffer, 1023, "<SE!> Error: %s\n", message.c_str());
        sceKernelDebugOutText(0, logBuffer);
    }
}
void Log::writeToFile(std::string message) {
}
#else
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
#endif

// Nintendo DS Timer implementation
#ifdef __NDS__
Timer::Timer() {
    start();
}
void Timer::start() {
    startTime = cpuGetTiming();
}
int Timer::getTimeMs() {
    uint64_t currentTime = cpuGetTiming();
    // CPU timing is in units based on the bus clock (33.513982 MHz)
    // Convert to milliseconds: (ticks * 1000) / BUS_CLOCK
    return static_cast<int>((currentTime - startTime) * 1000 / BUS_CLOCK);
}

// Wii's std::chrono support is still pretty bad
#elif defined(WII)

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

// std::chrono on PS4 updates slowly
#elif defined(__PS4__)

Timer::Timer() {
    start();
}

void Timer::start() {
    startTime = sceKernelReadTsc() * 1000;
}

int Timer::getTimeMs() {
    uint64_t currentTime = sceKernelReadTsc() * 1000;
    return static_cast<int>((currentTime - startTime) / sceKernelGetTscFrequency());
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

std::string OS::getFilesystemRootPrefix() {
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

std::string OS::getScratchFolderLocation() {
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

std::string OS::getRomFSLocation() {
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

bool OS::isNew3DS() {
#ifdef __3DS__
    bool out = false;
    APT_CheckNew3DS(&out);
    return out;
#endif
    return false;
}

bool OS::isDSi() {
#ifdef __NDS__
    return isDSiMode();
#endif
    return false;
}

bool OS::initWifi() {
#ifdef __3DS__
    u32 wifiEnabled = false;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled == AC_AP_TYPE_NONE) {
        int ret;
        uint32_t SOC_ALIGN = 0x1000;
        uint32_t SOC_BUFFERSIZE = 0x100000;
        uint32_t *SOC_buffer = NULL;

        SOC_buffer = (uint32_t *)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
        if (SOC_buffer == NULL) {
            Log::logError("memalign: failed to allocate");
        } else if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
            std::ostringstream err;
            err << "socInit: 0x" << std::hex << std::setw(8) << std::setfill('0') << ret;
            Log::logError(err.str());
        }
    }
#endif
    return true;
}

void OS::deInitWifi() {
#ifdef __3DS__
    u32 wifiEnabled = false;
    ACU_GetWifiStatus(&wifiEnabled);
    if (wifiEnabled != AC_AP_TYPE_NONE)
        socExit();
#endif
}