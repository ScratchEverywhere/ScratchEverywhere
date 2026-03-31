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

std::unordered_map<std::string, BlockFunc> &BlockExecutor::getHandlers() {
    static std::unordered_map<std::string, BlockFunc> handlers;
    return handlers;
}

/* Should be implemented, but i dont want to do it right now
#ifdef ENABLE_CACHING
void BlockExecutor::linkPointers(Sprite *sprite) {
    auto &h = getHandlers();
    auto &vh = getValueHandlers();

    for (auto &block : sprite->blocks) {

        auto it = h.find(block.opcode);
        if (it != h.end()) {
            block.handler = it->second;
        } else {
            auto vit = vh.find(block.opcode);
            if (vit != vh.end()) block.valueHandler = vit->second;
        }

        for (auto &[id, input] : *block.parsedInputs) {
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

        auto variableId = Scratch::getFieldId(block, "VARIABLE");
        if (variableId != "") {
            auto it = sprite->variables.find(variableId);
            if (it != sprite->variables.end()) {
                block.variable = &it->second;
                continue;
            }

            auto globalIt = Scratch::stageSprite->variables.find(variableId);
            if (globalIt != Scratch::stageSprite->variables.end()) {
                block.variable = &globalIt->second;
                continue;
            }

            block.variable = nullptr;
        }

        auto listId = Scratch::getFieldId(block, "LIST");
        if (listId != "") {
            auto it = sprite->lists.find(listId);
            if (it != sprite->lists.end()) {
                block.list = &it->second;
                continue;
            }

            auto globalIt = Scratch::stageSprite->lists.find(listId);
            if (globalIt != Scratch::stageSprite->lists.end()) {
                block.list = &globalIt->second;
                continue;
            }

            block.list = nullptr;
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
*/

ScriptThread *BlockExecutor::startThread(Sprite *sprite, Block *block) {
    static uint64_t id = 0;
    for (auto thread : sprite->threads) {
        if (thread->blockHat == block) {
            thread->clear();
            thread->nextBlock = block;
            thread->finished = false;
            return thread;
        }
    }
    for (auto thread : sprite->pendingThreads) {
        if (thread->blockHat == block) {
            thread->clear();
            thread->nextBlock = block;
            thread->finished = false;
            return thread;
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
    sprite->pendingThreads.push_back(newThread);
    return newThread;
}

void BlockExecutor::runThreads() {
    if (!Scratch::pendingSprites.empty()) {
        for (auto &sprite : Scratch::pendingSprites) {
            Scratch::addCloneBehind(sprite.second, sprite.first);
        }
        Scratch::pendingSprites.clear();
    }

    for (auto &sprite : Scratch::sprites) {
        if (!sprite->pendingThreads.empty()) {
            sprite->threads.insert(sprite->threads.end(), sprite->pendingThreads.begin(), sprite->pendingThreads.end());
            sprite->pendingThreads.clear();
        }
        size_t i = sprite->threads.size();
        while (i > 0) {
            i--;
            ScriptThread *thread = sprite->threads[i];
            BlockResult var;

            if (thread->finished) {
                thread->clear();
                Pools::threads.push_back(thread);
                sprite->threads.erase(sprite->threads.begin() + i);
                continue;
            }
            var = runThread(*thread, *sprite, nullptr);
            if (Scratch::shouldStop) return;
        }
        if (stopClicked) break;
    }
    Scratch::sprites.erase(
        std::remove_if(Scratch::sprites.begin(), Scratch::sprites.end(),
                       [](Sprite *s) {
                           if (s->toDelete) {
                               sortSprites = true;
                               delete s;
                               return true;
                           }
                           return false;
                       }),
        Scratch::sprites.end());

    if (sortSprites) {
        Scratch::sortSprites();
        sortSprites = false;
    }
    if (stopClicked) {
        Scratch::stopClicked();
    }
}

BlockResult BlockExecutor::runThread(ScriptThread &thread, Sprite &sprite, Value *outValue) {
    if (thread.nextBlock == nullptr) return BlockResult::RETURN;
    BlockResult var = BlockResult::CONTINUE;
    unsigned int executedBlocks = 0;
    unsigned int executionCount = 0;
    Block *currentBlock = nullptr;
    do {
        currentBlock = thread.nextBlock;
        thread.nextBlock = currentBlock->nextBlock;

        var = currentBlock->blockFunction(currentBlock, &thread, &sprite, outValue);
        if (var == BlockResult::REPEAT) thread.nextBlock = currentBlock;
        else {
            // if (thread.nextBlock != nullptr) Scratch::resetInput(thread.nextBlock); // just for safety
            Scratch::resetInput(currentBlock);
        }

        executionCount++;
        if (thread.withoutScreenRefresh && var != BlockResult::CONTINUE_IMIDIATELY) executedBlocks++;

        // TODO: re-add (currently makes execution slow, maybe could use a Timer instead?)
        // if (thread.withoutScreenRefresh && executionCount >= Scratch::withoutScreenRefreshLimit) break;
        // if (!thread.withoutScreenRefresh && executionCount >= 1024) break;

    } while ((var == BlockResult::CONTINUE_IMIDIATELY || (var == BlockResult::CONTINUE && (!currentBlock->isEndBlock || thread.withoutScreenRefresh))) && !thread.finished && thread.nextBlock != nullptr && !Scratch::shouldStop);
    if (currentBlock == nullptr || (var != BlockResult::REPEAT && currentBlock->nextBlock == nullptr)) thread.finished = true;
    return var;
}

// old thingy
/*BlockResult BlockExecutor::runBlock(Block *block, ScriptThread &thread, Sprite &sprite, Value *outValue) {
    bool finished = true;
    BlockResult executedBlock;
    do {
        bool finished = true;
        for (auto &input : block->inputs) {
            if (input.second.needed && !input.second.calculated) {

                if (Scratch::getInput(block, input.first, &thread, &sprite, input.second.value)) {
                    input.second.calculated = true;
                    input.second.needed = false;
                } else {
                    finished = false;
                }
                if (Scratch::shouldStop) return BlockResult::CONTINUE;
            }
            if (!finished) return BlockResult::REPEAT;
        }

    } while ((executedBlock = block->blockFunction(block, &thread, &sprite, outValue)) == BlockResult::GET_INPUTS);
    return executedBlock;
}*/

void BlockExecutor::runAllBlocksByOpcode(const std::string &opcode, std::vector<ScriptThread *> *out) {
    for (auto &sprite : Scratch::sprites) {
        if (sprite->hats[opcode].empty()) continue;
        for (auto &hat : sprite->hats[opcode]) {
            ScriptThread *thread = BlockExecutor::startThread(sprite, hat);
            if (out) out->push_back(thread);
        }
    }
}
void BlockExecutor::runAllBlocksByOpcodeInSprite(const std::string &opcode, Sprite *sprite, std::vector<ScriptThread *> *out) {
    if (sprite->hats[opcode].empty()) return;
    for (auto &hat : sprite->hats[opcode]) {
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

    const std::vector<Sprite *> sprToRun = Scratch::sprites;
    for (Sprite *currentSprite : sprToRun) {
        if (!currentSprite->hats["event_whenkeypressed"].empty()) {
            for (Block *block : currentSprite->hats["event_whenkeypressed"]) {
                std::string key = Scratch::getFieldValue(*block, "KEY_OPTION");
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 15 * (Scratch::FPS / 30.0f))) {
                    BlockExecutor::startThread(currentSprite, block);
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

/*
void BlockExecutor::setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite, Block *block) {
#ifdef ENABLE_CACHING
    if (block != nullptr && block->variable != nullptr) {
        block->variable->value = newValue;
#ifdef ENABLE_CLOUDVARS
        if (block->variable->cloud) cloudConnection->set(block->variable->name, block->variable->value.asString());
#endif
        return;
    }
#endif

    // Set sprite variable
    const auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
#ifdef ENABLE_CACHING
        if (block != nullptr && block->variable == nullptr) block->variable = &it->second;
#endif
        it->second.value = newValue;
        return;
    }

    auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
#ifdef ENABLE_CACHING
        if (block != nullptr && block->variable == nullptr) block->variable = &globalIt->second;
#endif
        globalIt->second.value = newValue;
#ifdef ENABLE_CLOUDVARS
        if (globalIt->second.cloud) cloudConnection->set(globalIt->second.name, globalIt->second.value.asString());
#endif
        return;
    }
}
*/

// that was the old variant without caching, with caching would need a rewrite to work properly again
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

// same goes for this one, rewrite would be needed for caching (BUT I have ideas on how to do it, so it shouldnt be too bad, just not my priority right now)
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
                var.value = BlockExecutor::getVariableValue(var.id, sprite);
                var.displayName = Math::removeQuotations(var.parameters["VARIABLE"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
            } else if (var.opcode == "data_listcontents") {
                var.displayName = Math::removeQuotations(var.parameters["LIST"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;

                // Check lists
                auto listIt = sprite->lists.find(var.id);
                if (listIt != sprite->lists.end())
                    var.list = listIt->second.items;

                // Check global lists
                auto globalIt = Scratch::stageSprite->lists.find(var.id);
                if (globalIt != Scratch::stageSprite->lists.end())
                    var.list = globalIt->second.items;
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
    // #ifdef ENABLE_CACHING
    //     if (block != nullptr && block->variable != nullptr) return block->variable->value;
    // #endif

    // Check sprite variables
    const auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        // #ifdef ENABLE_CACHING
        //         if (block != nullptr && block->variable == nullptr) block->variable = &it->second;
        // #endif
        return it->second.value;
    }

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
        // #ifdef ENABLE_CACHING
        //         if (block != nullptr && block->variable == nullptr) block->variable = &globalIt->second;
        // #endif
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