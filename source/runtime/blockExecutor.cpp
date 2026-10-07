#include "blockExecutor.hpp"
#include "collision.hpp"
#include "math.hpp"
#include "types.hpp"
#include <algorithm>
#include <cstddef>
#include <input.hpp>
#include <iterator>
#include <log.hpp>
#include <os.hpp>
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
}
#endif

ScriptThread *BlockExecutor::startThread(Sprite *sprite, Block *block, bool shouldRestart) {
    static uint64_t id = 0;

    size_t restartThreadIndex = -1;
    for (size_t i = 0; i < threads.size(); i++) {
        if (threads[i]->blockHat == block && sprite == threads[i]->sprite) {
            if (shouldRestart) {
                restartThreadIndex = i;
                break;
            } else return nullptr;
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
    newThread->parentThread = nullptr;
    newThread->finished = false;
    newThread->id = ++id;
    newThread->sprite = sprite;

    if (restartThreadIndex == -1) {
        threads.push_back(newThread);
    } else {
        auto &originalThread = threads[restartThreadIndex];
        originalThread->clear();
        Pools::threads.push_back(originalThread);
        threads.erase(threads.begin() + restartThreadIndex);
        threads.push_back(newThread);
    }

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

        var = runThread(*thread, *thread->sprite);

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
                               SpeechManager *speechManager = Render::getSpeechManager();
                               if (speechManager) speechManager->clearSpeech(s);
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

BlockResult BlockExecutor::runThread(ScriptThread &thread, Sprite &sprite) {
    if (thread.nextBlock == nullptr) return BlockResult::RETURN;
    BlockResult var = BlockResult::CONTINUE;
    Timer executionTimer(false);
    if (Scratch::warpTimer) executionTimer.start();
    Block *currentBlock = nullptr;
    unsigned int blocksSinceTimeCheck = 0;
    constexpr unsigned int timeCheckInterval = 64;
    do {
        currentBlock = thread.nextBlock;
        thread.nextBlock = currentBlock->nextBlock;

        switch (currentBlock->blockFunction.type) {
        case Type::Value: {
            var = currentBlock->blockFunction.func.value(currentBlock, &thread, &sprite, nullptr);
            break;
        }
        case Type::Number: {
            double blockOutDouble;
            var = currentBlock->blockFunction.func.number(currentBlock, &thread, &sprite, &blockOutDouble);
            break;
        }
        case Type::String: {
            std::string blockOutString;
            var = currentBlock->blockFunction.func.string(currentBlock, &thread, &sprite, &blockOutString);
            break;
        }
        case Type::Boolean: {
            bool blockOutBool;
            var = currentBlock->blockFunction.func.boolean(currentBlock, &thread, &sprite, &blockOutBool);
            break;
        }
        case Type::Color: {
            Color blockOutColor;
            var = currentBlock->blockFunction.func.color(currentBlock, &thread, &sprite, &blockOutColor);
            break;
        }
        }

        if (var == BlockResult::REPEAT) thread.nextBlock = currentBlock;
        else {
            Scratch::resetInput(currentBlock);
        }

        if (Scratch::warpTimer && thread.withoutScreenRefresh && ++blocksSinceTimeCheck >= timeCheckInterval) {
            blocksSinceTimeCheck = 0;
            if (executionTimer.getTimeMs() > 500) break;
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
        if (std::find(Input::inputKeys.begin(), Input::inputKeys.end(), key.first) == Input::inputKeys.end()) {
            Input::keyHeldDuration[key.first] = 0;
        } else {
            Input::keyHeldDuration[key.first]++;
        }
    }

    for (const auto &key : Input::inputKeys) {
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

namespace {
std::unordered_map<Block *, bool> whenGreaterThanPrevState;
}

void BlockExecutor::resetWhenGreaterThanHats() {
    whenGreaterThanPrevState.clear();
}

void BlockExecutor::executeWhenGreaterThanHats() {
    static ScriptThread conditionThread;

    for (Sprite *currentSprite : Scratch::sprites) {
        auto hatIt = currentSprite->hats.find("event_whengreaterthan");
        if (hatIt == currentSprite->hats.end()) continue;

        for (Block *block : hatIt->second) {
            std::string menu = Scratch::getFieldValue(*block, "WHENGREATERTHANMENU");
            std::transform(menu.begin(), menu.end(), menu.begin(), ::tolower);

            double current = 0;
            if (menu == "timer") current = BlockExecutor::timer.getTimeMs() / 1000.0;

            double threshold = 0;
            Scratch::getInputValueAs(block, "VALUE", &conditionThread, currentSprite, threshold);
            Scratch::resetInput(block, "VALUE");

            bool &wasTrue = whenGreaterThanPrevState[block];
            const bool nowTrue = current > threshold;
            if (nowTrue && !wasTrue) {
                BlockExecutor::startThread(currentSprite, block, true);
            }
            wasTrue = nowTrue;
        }
    }
}

void BlockExecutor::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;
        bool hasClicked = false;
        for (auto &sprite : Scratch::sprites) {
            if (!sprite->visible || sprite->ghostEffect == 100.0) continue;

            // click a sprite
            if (sprite->shouldDoSpriteClick) {
                bool colliding;
                if (Scratch::accurateCollision) colliding = collision::pointInSprite(sprite, Input::mousePointer.x, Input::mousePointer.y, true);
                else colliding = collision::pointInSpriteFast(sprite, Input::mousePointer.x, Input::mousePointer.y);
                if (Input::mousePointer.heldFrames < 2 && colliding) {

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
    const auto assignToVar = [&newValue](Variable &var) {
        std::visit([&newValue, &var](auto &&current) {
            using T = std::decay_t<decltype(current)>;
            if constexpr (std::is_same_v<T, double>) {
                var.value = newValue.asDouble();
            } else if constexpr (std::is_same_v<T, std::shared_ptr<const std::string>>) {
                if (newValue.isString()) var.value = newValue.getStringPtr();
                else var.value = std::make_shared<const std::string>(newValue.asString());
            } else if constexpr (std::is_same_v<T, bool>) {
                var.value = newValue.asBoolean();
            } else if constexpr (std::is_same_v<T, Value>) {
                var.value = newValue;
            }
        },
                   var.value);
    };

    // Set sprite variable
    if (sprite != nullptr) {
        const auto it = sprite->variables.find(variableId);
        if (it != sprite->variables.end()) {
            assignToVar(it->second);
            return;
        }
    }

    // Set global variable
    auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        assignToVar(globalIt->second);
#ifdef ENABLE_CLOUDVARS
        if (globalIt->second.cloud) {
            cloudConnection->set(globalIt->second.name, getVariableValueAs<std::string>(&globalIt->second));
        }
#endif
        return;
    }

    if (sprite != nullptr) {
        sprite->variables[variableId].value = newValue;
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
                var.value = BlockExecutor::getVariableValue(var.id, sprite);

                var.displayName = Math::removeQuotations(var.parameters["VARIABLE"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
            } else if (var.opcode == "data_listcontents") {
                var.displayName = Math::removeQuotations(var.parameters["LIST"]);
                if (!sprite->isStage) var.displayName = sprite->name + ": " + var.displayName;
                // Check lists
                auto listIt = sprite->lists.find(var.id);
                if (listIt != sprite->lists.end()) {
                    var.list = listIt->second.items;
                }

                // Check global lists
                auto globalIt = Scratch::stageSprite->lists.find(var.id);
                if (globalIt != Scratch::stageSprite->lists.end()) {
                    var.list = globalIt->second.items;
                }
            } else {
                Block newBlock;
                newBlock.opcode = var.opcode;
                newBlock.fields.reserve(var.parameters.size());
                newBlock.fieldMap.reserve(var.parameters.size());
                for (const auto &[paramName, paramValue] : var.parameters) {
                    ParsedField parsedField;
                    parsedField.value = Math::removeQuotations(paramValue);
                    newBlock.fields.push_back({paramName, parsedField});
                    newBlock.fieldMap[paramName] = &newBlock.fields.back().second;
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
                if (handlerIt != getHandlers().end()) {
                    handlerIt->second(&newBlock, thread, sprite, &var.value);
                } else {
                    Log::logWarning("[BlockExecutor] No handler found for monitor opcode: " + var.opcode);
                }
            }
        }
    }
}

Variable *BlockExecutor::getVariable(const std::string &variableId, Sprite *sprite) {
    // Check sprite variables
    if (sprite != nullptr) {
        const auto it = sprite->variables.find(variableId);
        if (it != sprite->variables.end()) return &it->second;
    }

    // Check global variables
    const auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        return &globalIt->second;
    }

    return nullptr;
}

template <typename T>
T BlockExecutor::getVariableValueAs(Variable *var) {
    if (!var) {
        if constexpr (std::is_same_v<T, Value>) return Value(0);
        else if constexpr (std::is_same_v<T, double>) return 0.0;
        else if constexpr (std::is_same_v<T, bool>) return false;
        else if constexpr (std::is_same_v<T, std::string>) return "0";
        else return Value(0).template get<T>();
    }

    return std::visit([](auto &&arg) -> T {
        using VarT = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Value>) {
            if constexpr (std::is_same_v<VarT, Value>) return arg;
            else return Value(arg);
        } else if constexpr (std::is_same_v<T, std::string> && std::is_same_v<VarT, std::shared_ptr<const std::string>>) {
            return *arg;
        } else if constexpr (std::is_same_v<VarT, T>) {
            return arg;
        } else if constexpr (std::is_same_v<VarT, Value>) {
            return arg.template get<T>();
        } else {
            return Value(arg).template get<T>();
        }
    },
                      var->value);
}

template <typename T>
T BlockExecutor::getVariableValueAs(const std::string &variableId, Sprite *sprite) {
    // Check sprite variables
    if (sprite != nullptr) {
        const auto it = sprite->variables.find(variableId);
        if (it != sprite->variables.end()) {
            return getVariableValueAs<T>(&it->second);
        }
    }

    // Check global variables
    const auto globalIt = Scratch::stageSprite->variables.find(variableId);
    if (globalIt != Scratch::stageSprite->variables.end()) {
        return getVariableValueAs<T>(&globalIt->second);
    }

    if (sprite != nullptr) {
        sprite->variables[variableId].value = Value(0);
        return getVariableValueAs<T>(&sprite->variables[variableId]);
    }

    return getVariableValueAs<T>(nullptr);
}

Value BlockExecutor::getVariableValue(const std::string &variableId, Sprite *sprite) {
    return getVariableValueAs<Value>(variableId, sprite);
}

#define GET_VARIABLE_VALUE_AS_TEMPLATE(T)                        \
    template T BlockExecutor::getVariableValueAs<T>(Variable *); \
    template T BlockExecutor::getVariableValueAs<T>(const std::string &, Sprite *)

GET_VARIABLE_VALUE_AS_TEMPLATE(Value);
GET_VARIABLE_VALUE_AS_TEMPLATE(double);
GET_VARIABLE_VALUE_AS_TEMPLATE(std::string);
GET_VARIABLE_VALUE_AS_TEMPLATE(bool);
GET_VARIABLE_VALUE_AS_TEMPLATE(Color);

Value BlockExecutor::getListValue(const std::string &listId, Sprite *sprite) {
    // Check sprite lists
    if (sprite != nullptr) {
        const auto listIt = sprite->lists.find(listId);
        if (listIt != sprite->lists.end()) {
            std::string result;
            std::string seperator = "";
            for (const auto &item : listIt->second.items) {
                if (!item.isString() || Math::utf16Length(item.get<std::string>()) > 1) {
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
    }

    // Check global lists
    auto globalListIt = Scratch::stageSprite->lists.find(listId);
    if (globalListIt != Scratch::stageSprite->lists.end()) {
        std::string result;
        std::string seperator = "";
        for (const auto &item : globalListIt->second.items) {
            if (!item.isString() || Math::utf16Length(item.get<std::string>()) > 1) {
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

    if (sprite != nullptr) {
        List newList;
        newList.id = listId;
        newList.items = {};
        sprite->lists[listId] = newList;
    }

    return Value("");
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
