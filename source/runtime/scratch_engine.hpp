#pragma once

#include <utility>

struct ScratchEngine {
    static bool initializeRuntime();

    static void initializeScratchProject();

    // i dont like that pair
    static std::pair<bool, bool> stepScratchProject(float delta);

    static bool startScratchProject();

    static void cleanupScratchProject();

    static void executeKeyHats();
    static void doSpriteClicking();
}; // namespace ScratchEngine