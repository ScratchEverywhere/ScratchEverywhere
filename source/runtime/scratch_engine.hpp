#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace ScratchEngine {
bool initializeRuntime();

void initializeScratchProject();

// i dont like that pair
std::pair<bool, bool> stepScratchProject();

bool startScratchProject();

void cleanupScratchProject();

void executeKeyHats();
inline uint32_t fnv1a_32(const std::string &str) {
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<uint32_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

void doSpriteClicking();
}; // namespace ScratchEngine