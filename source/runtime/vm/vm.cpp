#include "vm.hpp"
#include "../core/audio_manager.hpp"
#include "../opcodes/opcode_registers.hpp"
#include "../systems/sprite_system.hpp"
#include "blueprint.hpp"
#include "engine_state.hpp"
#include "entity_manager.hpp"
#include "log.hpp"
#include "timer.hpp"
#include <cstdint>

bool VM::threadsWithDispatchIdAreRunning(uint16_t dispatchId) {
    auto it = activeDispatchCounts.find(dispatchId);
    return it != activeDispatchCounts.end() && it->second > 0;
}

void VM::createNewThreads() {
    for (auto &info : newThreads) {
        if (info.restartThread) {
            for (auto &thread : threads) {
                if (thread.instanceId == info.instanceId && thread.bytecodeOffset == info.bytecodeOffset && thread.state != ThreadState::DEAD) {
                    thread.defId = info.defId;
                    thread.pc = info.bytecodeOffset;
                    thread.state = ThreadState::NEW;
                    thread.stack.clear();
                    thread.callStack.clear();
                    thread.sleepTimer = 0.0f;
                    thread.waitHandle = DISPATCH_ID_NONE;
                    thread.spawnDispatchId = info.dispatchId;
                    thread.transform = &EntityManager::transforms[info.instanceId];
                    thread.renderInfo = &EntityManager::renderInfo[info.instanceId];
                    thread.definition = &EntityManager::blueprints[info.defId];
                    if (info.dispatchId != DISPATCH_ID_NONE) {
                        activeDispatchCounts[info.dispatchId]++;
                    }
                    break;
                }
            }
        } else {
            auto &thread = threads.emplace_back();
            thread.instanceId = info.instanceId;
            thread.defId = info.defId;
            thread.bytecodeOffset = info.bytecodeOffset;
            thread.pc = info.bytecodeOffset;
            thread.state = ThreadState::NEW;
            thread.sleepTimer = 0.0f;
            thread.spawnDispatchId = info.dispatchId;
            thread.waitHandle = DISPATCH_ID_NONE;
            thread.transform = &EntityManager::transforms[info.instanceId];
            thread.renderInfo = &EntityManager::renderInfo[info.instanceId];
            thread.definition = &EntityManager::blueprints[info.defId];
        }
    }
    newThreads.clear();
}

void VM::greenFlagClicked() {
    activeDispatchCounts.clear();
    EngineState::timer.start();
    threads.clear();

    dispatchEvent(static_cast<uint16_t>(HatType::FLAG_CLICKED), 0, false, 0);
    for (uint16_t blueprintIndex = 0; blueprintIndex < EntityManager::blueprints.size(); ++blueprintIndex) {
        const auto &blueprint = EntityManager::blueprints[blueprintIndex];
        for (const auto &listener : blueprint.hatListeners) {
            if (listener.hatType == static_cast<uint16_t>(HatType::GREATER_THAN)) {
                for (uint32_t instanceId = 0; instanceId < EntityManager::activeInstances.size(); ++instanceId) {
                    if (!EntityManager::activeInstances[instanceId]) continue;
                    if (EntityManager::blueprintIds[instanceId] == blueprintIndex) {
                        addThread(instanceId, blueprintIndex, listener.bytecodeOffset,
                                  true, DISPATCH_ID_NONE);
                    }
                }
            }
        }
    }
    EntityManager::dirtyPointer = true;
}

void VM::stopAllClicked() {
    activeDispatchCounts.clear();
    threads.clear();
}

void VM::addThread(uint32_t instanceId, uint32_t defId, uint32_t bytecodeOffset, bool restartThread, uint16_t dispatchId) {
    newThreads.push_back({instanceId, defId, bytecodeOffset, restartThread, dispatchId});
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

            if (EngineState::warpTimer && (stepCounter % 1024) == 0) {
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
    if (EntityManager::dirtyPointer) {
        EntityManager::stageBlueprint = &EntityManager::blueprints[EntityManager::stageSprite];
    }
    for (auto &t : threads) {
        if (EntityManager::dirtyPointer) {
            t.transform = &EntityManager::transforms[t.instanceId];
            t.renderInfo = &EntityManager::renderInfo[t.instanceId];
            t.definition = &EntityManager::blueprints[t.defId];
        }
        switch (t.state) {
        case ThreadState::NEW:
            t.state = ThreadState::RUNNING;
            break;
        case ThreadState::WAITING_FOR_TIME:
            t.sleepTimer -= delta;
            if (t.sleepTimer <= 0.0f) t.state = ThreadState::RUNNING;
            if (t.isGliding) {
                float elapsed = t.glideInfo->durationSecs - t.sleepTimer;
                float progress = std::clamp(elapsed / t.glideInfo->durationSecs, 0.0f, 1.0f);
                float x = t.glideInfo->startX + (t.glideInfo->endX - t.glideInfo->startX) * progress;
                float y = t.glideInfo->startY + (t.glideInfo->endY - t.glideInfo->startY) * progress;
                SpriteSystem::gotoXY(t.instanceId, x, y);
            }
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
    EntityManager::dirtyPointer = false;
    createNewThreads();
    threads.erase(std::remove_if(threads.begin(), threads.end(),
                                 [](const VMThread &t) {
                                     if (t.state == ThreadState::DEAD && t.spawnDispatchId != DISPATCH_ID_NONE) {
                                         auto it = activeDispatchCounts.find(t.spawnDispatchId);
                                         if (it != activeDispatchCounts.end()) {
                                             if (--it->second == 0) {
                                                 activeDispatchCounts.erase(it);
                                             }
                                         }
                                     }
                                     return t.state == ThreadState::DEAD;
                                 }),
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

    const uint16_t defId = EntityManager::blueprintIds[instanceId];
    const auto &blueprint = EntityManager::blueprints[defId];

    for (const auto &listener : blueprint.hatListeners) {
        if (listener.hatType == hatType && listener.eventParamId == eventParamId) {
            addThread(instanceId, defId, listener.bytecodeOffset, restartThread, dispatchId);
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
        if (!EntityManager::activeInstances[instanceId]) {
            continue;
        };

        const uint16_t defId = EntityManager::blueprintIds[instanceId];

        for (const auto &match : matches) {
            if (match.defId == defId) {
                addThread(instanceId, defId, match.bytecodeOffset, restartThread, dispatchId);
            }
        }
    }
}