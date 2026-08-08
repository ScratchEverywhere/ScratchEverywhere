#include "vm.hpp"
#include "../core/audio_manager.hpp"
#include "../opcodes/opcode_registers.hpp"
#include "engine_state.hpp"
#include "entity_manager.hpp"
#include "timer.hpp"

bool VM::threadsWithDispatchIdAreRunning(uint16_t dispatchId) {
    auto it = activeDispatchCounts.find(dispatchId);
    return it != activeDispatchCounts.end() && it->second > 0;
}

void VM::greenFlagClicked() {
    activeDispatchCounts.clear();
    threads.clear();

    dispatchEvent(static_cast<uint16_t>(HatType::FLAG_CLICKED), 0, false, 0);
}

void VM::stopAllClicked() {
    activeDispatchCounts.clear();
    threads.clear();
}

void VM::createThread(uint32_t instanceId, uint32_t defId, uint32_t bytecodeOffset, bool restartThread, uint16_t dispatchId) {
    for (auto &thread : threads) {
        if (thread.instanceId == instanceId && thread.bytecodeOffset == bytecodeOffset && thread.state != ThreadState::DEAD) {
            if (restartThread) {
                thread.defId = defId;
                thread.pc = bytecodeOffset;
                thread.state = ThreadState::NEW;
                thread.stack.clear();
                thread.callStack.clear();
                thread.sleepTimer = 0.0f;
                thread.waitHandle = DISPATCH_ID_NONE;
                thread.spawnDispatchId = dispatchId;
                if (dispatchId != DISPATCH_ID_NONE) {
                    activeDispatchCounts[dispatchId]++;
                }
            }
            return;
        }
    }

    auto &thread = threads.emplace_back();
    thread.instanceId = instanceId;
    thread.defId = defId;
    thread.bytecodeOffset = bytecodeOffset;
    thread.pc = bytecodeOffset;
    thread.state = ThreadState::NEW;
    thread.sleepTimer = 0.0f;
    thread.spawnDispatchId = dispatchId;
    thread.waitHandle = DISPATCH_ID_NONE;
}

void VM::stepThread(VMThread &t) {
    uint32_t stepCounter = 0;
    Timer execTimer(false);
    bool timerStarted = false;

    while (t.state == ThreadState::RUNNING) {
        BlockResult res = OpcodeRegistry::executeByteCode(&t);

        if (res == BlockResult::YIELD_SAME) {
            break;
        }

        if (res == BlockResult::YIELD_NEXT) {
            if (!t.isWarp) {
                break;
            }

            stepCounter++;

            if (EngineState::warpTimer && (stepCounter & 500) == 0) {
                if (!timerStarted) {
                    execTimer.start();
                    timerStarted = true;
                } else if (execTimer.getTimeMs() >= 500.0f) {
                    break;
                }
            }
        }
    }
}

void VM::runThreads(float delta) {
    for (auto &t : threads) {
        switch (t.state) {
        case ThreadState::NEW:
            t.state = ThreadState::RUNNING;
            break;
        case ThreadState::WAITING_FOR_TIME:
            t.sleepTimer -= delta;
            if (t.sleepTimer <= 0.0f) t.state = ThreadState::RUNNING;
            continue;
        case ThreadState::WAITING_FOR_BROADCAST:
            if (!threadsWithDispatchIdAreRunning(t.waitHandle))
                t.state = ThreadState::RUNNING;
            continue;
        case ThreadState::WAITING_FOR_SOUND:
            if (!AudioManager::isPlaybackActive(t.waitHandle))
                t.state = ThreadState::RUNNING;
            continue;
        case ThreadState::DEAD:
            continue;
        default:
            break;
        }
        if (t.state != ThreadState::RUNNING) continue;
        stepThread(t);
    }

    threads.erase(std::remove_if(threads.begin(), threads.end(),
                                 [](const VMThread &t) { return t.state == ThreadState::DEAD; }),
                  threads.end());
}

void VM::killAllThreadsOfInstance(uint32_t instanceId) {
    for (auto &t : threads) {
        if (t.instanceId == instanceId) {
            t.state = ThreadState::DEAD;
        }
    }
}

void VM::dispatchEventInSprite(uint16_t hatType, uint32_t eventParamId, uint32_t instanceId, bool restartThread, uint16_t dispatchId) {
    if (instanceId >= EntityManager::activeInstances.size() || !EntityManager::activeInstances[instanceId])
        return;

    const uint32_t defId = EntityManager::blueprintIds[instanceId];
    const auto &blueprint = EntityManager::blueprints[defId];

    for (const auto &listener : blueprint.hatListeners) {
        if (listener.hatType == hatType && listener.eventParamId == eventParamId) {
            createThread(instanceId, defId, listener.bytecodeOffset, restartThread, dispatchId);
        }
    }
}

void VM::dispatchEvent(uint16_t hatType, uint32_t eventParamId, bool restartThread, uint16_t dispatchId) {
    struct Match {
        uint32_t defId;
        uint32_t bytecodeOffset;
    };
    thread_local std::vector<Match> matches;
    matches.clear();

    const auto &blueprints = EntityManager::blueprints;
    for (uint32_t defId = 0; defId < blueprints.size(); ++defId) {
        for (const auto &listener : blueprints[defId].hatListeners) {
            if (listener.hatType == hatType && listener.eventParamId == eventParamId) {
                matches.push_back({defId, listener.bytecodeOffset});
            }
        }
    }

    if (matches.empty()) return;

    const uint32_t totalInstances = EntityManager::activeInstances.size();
    for (uint32_t instanceId = 0; instanceId < totalInstances; ++instanceId) {
        if (!EntityManager::activeInstances[instanceId]) continue;

        const uint32_t defId = EntityManager::blueprintIds[instanceId];

        for (const auto &match : matches) {
            if (match.defId == defId) {
                createThread(instanceId, defId, match.bytecodeOffset, restartThread, dispatchId);
            }
        }
    }
}