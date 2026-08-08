#pragma once

#include "../core/vm_types.hpp"

class VM {
  private:
    static inline std::unordered_map<uint16_t, uint32_t> activeDispatchCounts;

    static inline BlockResult executeByteCode(VMThread *thread);

    static void stepThread(VMThread &thread);

  public:
    static void greenFlagClicked();
    static void stopAllClicked();

    static void runThreads(float delta);

    static inline std::vector<VMThread> threads;

    static bool threadsWithDispatchIdAreRunning(uint16_t dispatchId);

    static void createThread(uint32_t instanceId, uint32_t defId, uint32_t bytecodeOffset,
                             bool restartThread = false, uint16_t dispatchId = 0);

    static void killAllThreadsOfInstance(uint32_t instanceId);

    static void dispatchEventInSprite(uint16_t hatType, uint32_t eventParamId, uint32_t instanceId, bool restartThread = false, uint16_t dispatchId = 0);
    static void dispatchEvent(uint16_t hatType, uint32_t eventParamId, bool restartThread = false, uint16_t dispatchId = 0);
};