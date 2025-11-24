#include <chrono>
#include <fstream>
#include <iostream>
#include <os.hpp>
#include <ostream>
#include <render.hpp>
#include <string>
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
