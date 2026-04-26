#include "blockExecutor.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <input.hpp>
#include <iterator>
#include <os.hpp>
#include <ratio>
#include <render.hpp>
#include <runtime.hpp>
#include <speech_manager.hpp>
#include <string>
#include <utility>
#include <vector>

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>

extern std::unique_ptr<MistConnection> cloudConnection;
#endif

Timer BlockExecutor::timer;
int BlockExecutor::dragPositionOffsetX;
int BlockExecutor::dragPositionOffsetY;
bool BlockExecutor::sortSprites = false;
bool BlockExecutor::stopClicked = false;
std::vector<ScriptThread *> BlockExecutor::threads;

std::unordered_map<std::string, BlockFunc> &BlockExecutor::getHandlers() {
    static std::unordered_map<std::string, BlockFunc> handlers;
    return handlers;
}

#ifdef ENABLE_CACHING
void BlockExecutor::linkPointers(Sprite *sprite) {
    for (auto &[_, blocks] : sprite->hats) {
        for (auto &block : blocks) {
            for (auto &[id, input] : block->inputs) {
                if (input.inputType != ParsedInput::VARIABLE) continue;

                auto it = sprite->variables.find(input.variableId);
                if (it != sprite->variables.end()) {
                    input.variable = &it->second;
                    continue;
                }

                auto globalIt = Scratch::stageSprite->variables.find(input.variableId);
                if (globalIt != Scratch::stageSprite->variables.end()) {
                    input.variable = &globalIt->second;
                    continue;
                }

                input.variable = nullptr;
            }
        }
    }

    for (auto &[id, monitor] : Render::monitors) {
        if (monitor.opcode == "data_variable") {
            auto it = sprite->variables.find(monitor.id);
            if (it != sprite->variables.end()) {
                monitor.variablePtr = &it->second;
                continue;
            }

            auto globalIt = Scratch::stageSprite->variables.find(monitor.id);
            if (globalIt != Scratch::stageSprite->variables.end()) {
                monitor.variablePtr = &globalIt->second;
                continue;
            }

            monitor.variablePtr = nullptr;
            continue;
        }
        if (monitor.opcode == "data_listcontents") {
            auto it = sprite->lists.find(monitor.id);
            if (it != sprite->lists.end()) {
                monitor.listPtr = &it->second;
                continue;
            }

            auto globalIt = Scratch::stageSprite->lists.find(monitor.id);
            if (globalIt != Scratch::stageSprite->lists.end()) {
                monitor.listPtr = &globalIt->second;
                continue;
            }

            monitor.variablePtr = nullptr;
            continue;
        }
    }
}
#endif

ScriptThread *BlockExecutor::startThread(Sprite *sprite, Block *block, bool shouldRestart) {
    static uint64_t id = 0;
    for (auto thread : threads) {
        if (thread->blockHat == block && sprite == thread->sprite) {
            if (shouldRestart) thread->finished = true;
            else return nullptr;
        }
    }

    ScriptThread *newThread = nullptr;
    if (Pools::threads.empty()) newThread = new ScriptThread();
    else {
        newThread = Pools::threads.back();
        Pools::threads.pop_back();
    }
    newThread->blockHat = block;
    newThread->nextBlock = block;
    newThread->finished = false;
    newThread->id = ++id;
    newThread->sprite = sprite;
    threads.push_back(newThread);
    return newThread;
}

void BlockExecutor::runThreads() {
    size_t i = 0;
    while (i < threads.size()) {
        ScriptThread *thread = threads[i];
        BlockResult var;

        if (thread->finished) {
            thread->clear();
            Pools::threads.push_back(thread);
            threads.erase(threads.begin() + i);
            continue;
        }

        var = runThread(*thread, *thread->sprite, nullptr);

        if (Scratch::shouldStop) return;
        i++;
    }

    Scratch::sprites.erase(
        std::remove_if(Scratch::sprites.begin(), Scratch::sprites.end(),
                       [](Sprite *s) {
                           if (s->toDelete) {
                               for (auto &thread : threads) {
                                   if (thread->sprite == s) {
                                       thread->finished = true;
                                   }
                               }
                               sortSprites = true;
                               delete s;
                               return true;
                           }
                           return false;
                       }),
        Scratch::sprites.end());

    if (sortSprites) {
        for (unsigned int i = 0; i < Scratch::sprites.size(); i++) {
            Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
        }
        sortSprites = false;
    }
    if (stopClicked) {
        Scratch::stopClicked();
    }
}

BlockResult BlockExecutor::runThread(ScriptThread &thread, Sprite &sprite, Value *outValue) {
    if (thread.nextBlock == nullptr) return BlockResult::RETURN;
    BlockResult var = BlockResult::CONTINUE;
    Timer executionTimer(false);
    if (Scratch::warpTimer) executionTimer.start();
    Block *currentBlock = nullptr;
    do {
        currentBlock = thread.nextBlock;
        thread.nextBlock = currentBlock->nextBlock;

        var = currentBlock->blockFunction(currentBlock, &thread, &sprite, outValue);
        if (var == BlockResult::REPEAT) thread.nextBlock = currentBlock;
        else {
            Scratch::resetInput(currentBlock);
        }

        if (Scratch::warpTimer && thread.withoutScreenRefresh && executionTimer.getTimeMs() > 500) {
            break;
        }

    } while ((var == BlockResult::CONTINUE_IMMEDIATELY || (var == BlockResult::CONTINUE && (!currentBlock->isEndBlock || thread.withoutScreenRefresh))) && !thread.finished && thread.nextBlock != nullptr && !Scratch::shouldStop);
    if (currentBlock == nullptr || var == BlockResult::RETURN || (var != BlockResult::REPEAT && currentBlock->nextBlock == nullptr)) thread.finished = true;
    return var;
}

void BlockExecutor::runAllBlocksByOpcode(const std::string &opcode, std::vector<ScriptThread *> *out) {
    for (auto *sprite : Scratch::sprites) {
        runAllBlocksByOpcodeInSprite(opcode, sprite);
    }
}

void BlockExecutor::runAllBlocksByOpcodeInSprite(const std::string &opcode, Sprite *sprite, std::vector<ScriptThread *> *out) {
    if (sprite->hats[opcode].empty()) return;
    std::vector<Block *> tempHats(sprite->hats[opcode].begin(), sprite->hats[opcode].end());
    for (auto it = tempHats.rbegin(); it != tempHats.rend(); ++it) {
        auto &hat = *it;

        ScriptThread *thread = BlockExecutor::startThread(sprite, hat);
        if (out) out->push_back(thread);
    }
}

// ToDo: That could be optimized, but it works for now and i want to move on to other stuff
void BlockExecutor::executeKeyHats() {
    for (const auto &key : Input::keyHeldDuration) {
        if (std::find(Input::inputButtons.begin(), Input::inputButtons.end(), key.first) == Input::inputButtons.end()) {
            Input::keyHeldDuration[key.first] = 0;
        } else {
            Input::keyHeldDuration[key.first]++;
        }
    }

    for (const auto &key : Input::inputButtons) {
        if (Input::keyHeldDuration.find(key) == Input::keyHeldDuration.end()) Input::keyHeldDuration[key] = 1;

        if (key == "any" || Input::keyHeldDuration[key] != 1) continue;

        Input::codePressedBlockOpcodes.clear();
        std::string addKey = (key.find(' ') == std::string::npos) ? key : key.substr(0, key.find(' '));
        std::transform(addKey.begin(), addKey.end(), addKey.begin(), ::tolower);
        Input::inputBuffer.push_back(addKey);
        if (Input::inputBuffer.size() == 101) Input::inputBuffer.erase(Input::inputBuffer.begin());
    }

    for (Sprite *currentSprite : Scratch::sprites) {
        if (!currentSprite->hats["event_whenkeypressed"].empty()) {
            for (Block *block : currentSprite->hats["event_whenkeypressed"]) {
                std::string key = Scratch::getFieldValue(*block, "KEY_OPTION");
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 15 * (Scratch::FPS / 30.0f))) {
                    BlockExecutor::startThread(currentSprite, block, false);
                }
            }
        }
        BlockExecutor::runAllBlocksByOpcodeInSprite("makeymakey_whenMakeyKeyPressed", currentSprite);
    }
    BlockExecutor::runAllBlocksByOpcode("makeymakey_whenCodePressed");
}

void BlockExecutor::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;
        bool hasClicked = false;
        for (auto &sprite : Scratch::sprites) {
            if (!sprite->visible || sprite->ghostEffect == 100.0) continue;

            // click a sprite
            if (sprite->shouldDoSpriteClick) {
                if (Input::mousePointer.heldFrames < 2 && Scratch::isColliding("mouse", sprite)) {

                    // run all "when this sprite clicked" blocks in the sprite
                    hasClicked = true;
                    BlockExecutor::runAllBlocksByOpcodeInSprite("event_whenthisspriteclicked", sprite);
                    if (sprite->isStage) BlockExecutor::runAllBlocksByOpcodeInSprite("event_whenstageclicked", sprite);
                }
            }
            // start dragging a sprite
            if (Input::draggingSprite == nullptr && Input::mousePointer.heldFrames < 2 && sprite->draggable && Scratch::isColliding("mouse", sprite)) {
                Input::draggingSprite = sprite;
                dragPositionOffsetX = Input::mousePointer.x - sprite->xPosition;
                dragPositionOffsetY = Input::mousePointer.y - sprite->yPosition;
            }
            if (hasClicked) break;
        }
    } else {
        Input::mousePointer.heldFrames = 0;
    }

    // move a dragging sprite
    if (Input::draggingSprite == nullptr) return;

    if (Input::mousePointer.heldFrames == 0) {
        Input::draggingSprite = nullptr;
        return;
    }
    Input::draggingSprite->xPosition = Input::mousePointer.x - dragPositionOffsetX;
    Input::draggingSprite->yPosition = Input::mousePointer.y - dragPositionOffsetY;
}

void BlockExecutor::setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite) {
    // Set sprite variable
    const auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        it->second.value = newValue;
        return;
    }

    auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        globalIt->second.value = newValue;
#ifdef ENABLE_CLOUDVARS
        if (globalIt->second.cloud) cloudConnection->set(globalIt->second.name, globalIt->second.value.asString());
#endif
        return;
    }
}

void BlockExecutor::updateMonitors(ScriptThread *thread) {
    for (auto &[id, var] : Render::monitors) {
        if (var.visible) {

            Sprite *sprite = nullptr;
            for (auto &spr : Scratch::sprites) {
                if (var.spriteName == "" && spr->isStage) {
                    sprite = spr;
                    break;
                }
                if (spr->name == var.spriteName && !spr->isClone) {
                    sprite = spr;
                    break;
                }
            }

            if (var.opcode == "data_variable") {
#ifdef ENABLE_CACHING
                if (var.variablePtr != nullptr) var.value = var.variablePtr->value;
                else var.value = BlockExecutor::getVariableValue(var.id, sprite);
#else
                var.value = BlockExecutor::getVariableValue(var.id, sprite);
#endif

                var.displayName = Math::removeQuotations(var.parameters["VARIABLE"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
            } else if (var.opcode == "data_listcontents") {
                var.displayName = Math::removeQuotations(var.parameters["LIST"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;

#ifdef ENABLE_CACHING
                if (var.listPtr != nullptr) {
                    var.list = var.listPtr->items;
                } else {
#endif
                    // Check lists
                    auto listIt = sprite->lists.find(var.id);
                    if (listIt != sprite->lists.end()) {
                        var.list = listIt->second.items;
#ifdef ENABLE_CACHING
                        var.listPtr = &listIt->second;
#endif
                    }

                    // Check global lists
                    auto globalIt = Scratch::stageSprite->lists.find(var.id);
                    if (globalIt != Scratch::stageSprite->lists.end()) {
                        var.list = globalIt->second.items;
#ifdef ENABLE_CACHING
                        var.listPtr = &globalIt->second;
#endif
                    }
#ifdef ENABLE_CACHING
                }
#endif
            } else {
                Block newBlock;
                newBlock.opcode = var.opcode;
                for (const auto &[paramName, paramValue] : var.parameters) {
                    ParsedField parsedField;
                    parsedField.value = Math::removeQuotations(paramValue);
                    (newBlock.fields)[paramName] = parsedField;
                }
                if (var.opcode == "looks_costumenumbername")
                    var.displayName = var.spriteName + ": costume " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                else if (var.opcode == "looks_backdropnumbername")
                    var.displayName = "backdrop " + Scratch::getFieldValue(newBlock, "NUMBER_NAME");
                else if (var.opcode == "sensing_current")
                    var.displayName = std::string(MonitorDisplayNames::getCurrentMenuMonitorName(Scratch::getFieldValue(newBlock, "CURRENTMENU")));
                else {
                    auto spriteName = MonitorDisplayNames::getSpriteMonitorName(var.opcode);
                    if (spriteName != var.opcode) {
                        var.displayName = var.spriteName + ": " + std::string(spriteName);
                    } else {
                        auto simpleName = MonitorDisplayNames::getSimpleMonitorName(var.opcode);
                        var.displayName = simpleName != var.opcode ? std::string(simpleName) : var.opcode;
                    }
                }
                auto handlerIt = getHandlers().find(var.opcode);
                if (handlerIt != getHandlers().end() && handlerIt->second != nullptr) {
                    handlerIt->second(&newBlock, thread, sprite, &var.value);
                } else {
                    Log::logWarning("No handler found for monitor opcode: " + var.opcode);
                }
            }
        }
    }
}

Value BlockExecutor::getVariableValue(const std::string &variableId, Sprite *sprite) {
    // Check sprite variables
    const auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) return it->second.value;

    // Check lists
    const auto listIt = sprite->lists.find(variableId);
    if (listIt != sprite->lists.end()) {
        std::string result;
        std::string seperator = "";
        for (const auto &item : listIt->second.items) {
            if (item.asString().size() > 1 || !item.isString()) {
                seperator = " ";
                break;
            }
        }
        for (const auto &item : listIt->second.items) {
            result += item.asString() + seperator;
        }
        if (!result.empty() && !seperator.empty()) result.pop_back();
        return Value(result);
    }

    // Check global variables
    const auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        return globalIt->second.value;
    }

    // Check global lists
    auto globalListIt = Scratch::stageSprite->lists.find(variableId);
    if (globalListIt != Scratch::stageSprite->lists.end()) {
        std::string result;
        std::string seperator = "";
        for (const auto &item : globalListIt->second.items) {
            if (item.asString().size() > 1 || !item.isString()) {
                seperator = " ";
                break;
            }
        }
        for (const auto &item : globalListIt->second.items) {
            result += item.asString() + seperator;
        }
        if (!result.empty() && !seperator.empty()) result.pop_back();
        return Value(result);
    }

    return Value();
}

#ifdef ENABLE_CLOUDVARS
void BlockExecutor::handleCloudVariableChange(const std::string &name, const std::string &value) {
    for (auto it = Scratch::stageSprite->variables.begin(); it != Scratch::stageSprite->variables.end(); ++it) {
        if (it->second.name != name) continue;
        it->second.value = Value(value);
        return;
    }
}
#endif
