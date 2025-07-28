#include <iostream>
#ifdef __3DS__
#include <3ds.h>
#endif
#pragma once

class MemoryTracker {
  private:
    static size_t totalAllocated;
    static size_t peakUsage;
    static size_t allocationCount;
    const static size_t old3ds_maxRamUsage = 50331648;  // 48 MB
    const static size_t new3ds_maxRamUsage = 100663296; // 96 MB
    const static size_t wiiu_maxRamUsage = 805306368;   // 768 MB
    const static size_t pc_maxRamUsage = 1073741824;    // 1 GB

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
        return pc_maxRamUsage;
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
void writeToFile(std::string message, std::string filePath);
} // namespace Log