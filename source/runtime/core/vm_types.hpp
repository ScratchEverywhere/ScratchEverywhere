#pragma once

#include "../data/blueprint.hpp"
#include "../data/entity_components.hpp"
#include "blueprint.hpp"
#include <cstdint>
#include <vector>

#include "value.hpp"

enum class ThreadState : uint8_t {
    NEW,
    RUNNING,
    WAITING_FOR_TIME,
    WAITING_FOR_SOUND,
    WAITING_FOR_BROADCAST,
    DEAD,
};

enum class BlockResult : uint8_t {
    CONTINUE,
    YIELD_NEXT,
    YIELD_SAME,
    YIELD = YIELD_NEXT
};
// TurboWarp stalls at ~10.000 calls (~5.000 with returns) and vanilla Scratch straight-up dies
// Capping it here so we don't crash. If a script reaches this deep, it just bails out with an empty return value
// (Yeah, this can waste 80 KB of stack space per thread, but hey, it stays alive!)
#define MAX_CALL_DEPTH 10000

struct CallFrame {
    uint32_t returnPC = 0;
    uint32_t stackBase = 0;
    uint16_t argCount = 0;
    bool previousWarpState = false;
};

#define DISPATCH_ID_NONE 0

struct GlideInfo {
    float startX = 0.0f, startY = 0.0f;
    float endX = 0.0f, endY = 0.0f;
    float durationSecs = 0.0f;
};

struct VMThread {
    VMThread() = default;
    ~VMThread() {
        if (glideInfo) delete glideInfo;
    }
    VMThread(const VMThread &other) = delete;
    VMThread &operator=(const VMThread &other) = delete;
    VMThread(VMThread &&other) = default;
    VMThread &operator=(VMThread &&other) = default;

    uint32_t pc = 0;
    uint32_t bytecodeOffset = 0;
    SpriteTransform *transform = nullptr;
    RenderInfo *renderInfo = nullptr;
    TargetDefinition *definition = nullptr;
    uint32_t instanceId;
    uint16_t defId;

    double sleepTimer = 0.0f;
    uint16_t spawnDispatchId = DISPATCH_ID_NONE;
    uint32_t waitHandle = DISPATCH_ID_NONE;

    std::vector<CallFrame> callStack;
    std::vector<Value> stack;

    ThreadState state = ThreadState::NEW;
    bool isWarp = false;

    bool isGliding = false;
    GlideInfo *glideInfo = nullptr;
};
