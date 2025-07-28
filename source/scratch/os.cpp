#include "os.hpp"

size_t MemoryTracker::totalAllocated = 0;
size_t MemoryTracker::peakUsage = 0;
size_t MemoryTracker::allocationCount = 0;

void Log::log(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cout << message << std::endl;
}
void Log::logWarning(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cout << "Warning: " << message << std::endl;
}
void Log::logError(std::string message, bool printToScreen) {
    if (printToScreen)
        std::cerr << "Error: " << message << std::endl;
}
void Log::writeToFile(std::string message, std::string filePath) {
}