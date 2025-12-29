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
#include <utility>
#include <vector>

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>

extern std::unique_ptr<MistConnection> cloudConnection;
#endif

Timer BlockExecutor::timer;
int BlockExecutor::dragPositionOffsetX;
int BlockExecutor::dragPositionOffsetY;

std::unordered_map<std::string, std::function<BlockResult(Block &, Sprite *, bool *, bool)>> &BlockExecutor::getHandlers() {
    static std::unordered_map<std::string, std::function<BlockResult(Block &, Sprite *, bool *, bool)>> handlers;
    return handlers;
}

std::unordered_map<std::string, std::function<Value(Block &, Sprite *)>> &BlockExecutor::getValueHandlers() {
    static std::unordered_map<std::string, std::function<Value(Block &, Sprite *)>> valueHandlers;
    return valueHandlers;
}

std::vector<Block *> BlockExecutor::runBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::vector<Block *> ranBlocks;
    Block *currentBlock = &block;

    if (!sprite || sprite->toDelete) return ranBlocks;

    while (currentBlock && currentBlock->id != "null") {
        ranBlocks.push_back(currentBlock);
        BlockResult result = executeBlock(*currentBlock, sprite, withoutScreenRefresh, fromRepeat);

        if (result == BlockResult::RETURN) return ranBlocks;

        if (currentBlock->next.empty()) break;
        currentBlock = &sprite->blocks[currentBlock->next];
        fromRepeat = false;
    }

    return ranBlocks;
}

BlockResult BlockExecutor::executeBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    auto &h = getHandlers();
    const auto iterator = h.find(block.opcode);
    if (iterator != h.end()) return iterator->second(block, sprite, withoutScreenRefresh, fromRepeat);

    Log::logWarning("Unknown block: " + block.opcode);

    return BlockResult::CONTINUE;
}

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
        for (auto &[id, data] : currentSprite->blocks) {
            // TODO: Add a way to register these with macros
            if (data.opcode == "event_whenkeypressed") {
                std::string key = Scratch::getFieldValue(data, "KEY_OPTION");
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 13))
                    executor.runBlock(data, currentSprite);
            } else if (data.opcode == "makeymakey_whenMakeyKeyPressed") {
                std::string key = Input::convertToKey(Scratch::getInputValue(data, "KEY", currentSprite), true);
                if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && Input::keyHeldDuration.find(key)->second > 0)
                    executor.runBlock(data, currentSprite);
            }
        }
    }
    BlockExecutor::runAllBlocksByOpcode("makeymakey_whenCodePressed");
}

void BlockExecutor::doSpriteClicking() {
    if (Input::mousePointer.isPressed) {
        Input::mousePointer.heldFrames++;
        bool hasClicked = false;
        for (auto &sprite : Scratch::sprites) {
            if (!sprite->visible) continue;

            // click a sprite
            if (sprite->shouldDoSpriteClick) {
                if (Input::mousePointer.heldFrames < 2 && Scratch::isColliding("mouse", sprite)) {

                    // run all "when this sprite clicked" blocks in the sprite
                    hasClicked = true;
                    for (auto &[id, data] : sprite->blocks) {
                        if (data.opcode == "event_whenthisspriteclicked") {
                            executor.runBlock(data, sprite);
                        }
                    }
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

void BlockExecutor::runRepeatBlocks() {
    bool withoutRefresh = false;

    // repeat ONLY the block most recently added to the repeat chain,,,
    std::vector<Sprite *> sprToRun = Scratch::sprites;
    for (auto &sprite : sprToRun) {
        for (auto &[id, blockChain] : sprite->blockChains) {
            const auto &repeatList = blockChain.blocksToRepeat;
            if (repeatList.empty()) continue;

            const std::string toRepeat = repeatList.back();
            if (toRepeat.empty()) continue;

            Block *const toRun = &sprite->blocks[toRepeat];
            if (toRun != nullptr) executor.runBlock(*toRun, sprite, &withoutRefresh, true);
        }
    }
    // delete sprites ready for deletion
    std::vector<Sprite *> toDelete;
    for (auto &spr : Scratch::sprites) {
        if (!spr->toDelete) continue;
        toDelete.push_back(spr);
    }
    for (auto *spr : toDelete) {
        delete spr;
        Scratch::sprites.erase(std::remove(Scratch::sprites.begin(), Scratch::sprites.end(), spr),
                               Scratch::sprites.end());
    }
}

void BlockExecutor::runRepeatsWithoutRefresh(Sprite *sprite, std::string blockChainID) {
    bool withoutRefresh = true;
    if (sprite->blockChains.find(blockChainID) == sprite->blockChains.end()) return;

    while (!sprite->blockChains[blockChainID].blocksToRepeat.empty()) {
        const std::string toRepeat = sprite->blockChains[blockChainID].blocksToRepeat.back();
        Block *toRun = Scratch::findBlock(toRepeat, sprite);
        if (toRun != nullptr)
            executor.runBlock(*toRun, sprite, &withoutRefresh, true);
    }
}

BlockResult BlockExecutor::runCustomBlock(Sprite *sprite, Block &block, Block *callerBlock, bool *withoutScreenRefresh) {
    for (auto &[id, data] : sprite->customBlocks) {
        if (id == block.customBlockId) {
            // Set up argument values
            for (std::string arg : data.argumentIds) {
                data.argumentValues[arg] = block.parsedInputs->find(arg) == block.parsedInputs->end() ? Value(0) : Scratch::getInputValue(block, arg, sprite);
            }

            // Get the parent of the prototype block (the definition containing all blocks)
            Block *customBlockDefinition = &sprite->blocks[sprite->blocks[data.blockId].parent];

            callerBlock->customBlockPtr = customBlockDefinition;

            bool localWithoutRefresh = data.runWithoutScreenRefresh;

            // If the parent chain is running without refresh, force this one to also run without refresh
            if (!localWithoutRefresh && withoutScreenRefresh != nullptr) localWithoutRefresh = *withoutScreenRefresh;

            // std::cout << "RWSR = " << localWithoutRefresh << std::endl;

            // Execute the custom block definition
            executor.runBlock(*customBlockDefinition, sprite, &localWithoutRefresh, false);

            if (localWithoutRefresh) BlockExecutor::runRepeatsWithoutRefresh(sprite, customBlockDefinition->blockChainID);

            break;
        }
    }

    if (block.customBlockId == "\u200B\u200Blog\u200B\u200B %s") Log::log("[PROJECT] " + Scratch::getInputValue(block, "arg0", sprite).asString());
    if (block.customBlockId == "\u200B\u200Bwarn\u200B\u200B %s") Log::logWarning("[PROJECT] " + Scratch::getInputValue(block, "arg0", sprite).asString());
    if (block.customBlockId == "\u200B\u200Berror\u200B\u200B %s") Log::logError("[PROJECT] " + Scratch::getInputValue(block, "arg0", sprite).asString());
    if (block.customBlockId == "\u200B\u200Bopen\u200B\u200B %s .sb3") {
        Log::log("Open next Project with Block");
        Scratch::nextProject = true;
        Unzip::filePath = Scratch::getInputValue(block, "arg0", sprite).asString();
        if (Unzip::filePath.rfind("sd:", 0) == 0) {
            const std::string drivePrefix = OS::getFilesystemRootPrefix();
            Unzip::filePath.replace(0, 3, drivePrefix);
        } else if (Unzip::filePath.rfind("romfs:", 0) == 0) {
            const std::string drivePrefix = OS::getRomFSLocation();
            Unzip::filePath.replace(0, 6, drivePrefix);
        } else {
            Unzip::filePath = Unzip::filePath;
        }

        if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
            Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
        }
        if (!OS::fileExists(Unzip::filePath + "/project.json"))
            Unzip::filePath = Unzip::filePath + ".sb3";

        Scratch::dataNextProject = Value();
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }
    if (block.customBlockId == "\u200B\u200Bopen\u200B\u200B %s .sb3 with data %s") {
        Log::log("Open next Project with Block and data");
        Scratch::nextProject = true;
        Unzip::filePath = Scratch::getInputValue(block, "arg0", sprite).asString();
        // if filepath contains sd:/ at the beginning and only at the beginning, replace it with sdmc:/
        if (Unzip::filePath.rfind("sd:", 0) == 0) {
            const std::string drivePrefix = OS::getFilesystemRootPrefix();
            Unzip::filePath.replace(0, 3, drivePrefix);
        } else if (Unzip::filePath.rfind("romfs:", 0) == 0) {
            const std::string drivePrefix = OS::getRomFSLocation();
            Unzip::filePath.replace(0, 6, drivePrefix);
        } else {
            Unzip::filePath = Unzip::filePath;
        }
        if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
            Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
        }
        if (!OS::fileExists(Unzip::filePath + "/project.json"))
            Unzip::filePath = Unzip::filePath + ".sb3";

        Scratch::dataNextProject = Scratch::getInputValue(block, "arg1", sprite);
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }

    return BlockResult::CONTINUE;
}

void BlockExecutor::runCloneStarts() {
    while (!Scratch::cloneQueue.empty()) {
        Sprite *cloningSprite = Scratch::cloneQueue.front();
        Scratch::cloneQueue.erase(Scratch::cloneQueue.begin());
        for (Sprite *sprite : Scratch::sprites) {
            if (cloningSprite != sprite) continue;
            for (auto &[id, data] : cloningSprite->blocks) {
                if (data.opcode == "control_start_as_clone") executor.runBlock(data, sprite);
            }
        }
    }
}

std::vector<std::pair<Block *, Sprite *>> BlockExecutor::runBroadcasts() {
    std::vector<std::pair<Block *, Sprite *>> blocksToRun;

    while (!Scratch::broadcastQueue.empty()) {
        const std::string currentBroadcast = Scratch::broadcastQueue.front();
        Scratch::broadcastQueue.erase(Scratch::broadcastQueue.begin());
        const auto results = runBroadcast(currentBroadcast);
        blocksToRun.insert(blocksToRun.end(), results.begin(), results.end());
    }

    return blocksToRun;
}

std::vector<std::pair<Block *, Sprite *>> BlockExecutor::runBroadcast(std::string broadcastToRun) {
    std::vector<std::pair<Block *, Sprite *>> blocksToRun;

    // find all matching "when I receive" blocks
    std::vector<Sprite *> sprToRun = Scratch::sprites;
    for (auto *currentSprite : sprToRun) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.opcode == "event_whenbroadcastreceived" &&
                Scratch::getFieldValue(block, "BROADCAST_OPTION") == broadcastToRun) {
                blocksToRun.insert(blocksToRun.begin(), {&block, currentSprite});
            }
        }
    }

    // run each matching block
    for (auto &[blockPtr, spritePtr] : blocksToRun)
        executor.runBlock(*blockPtr, spritePtr);

    return blocksToRun;
}

void BlockExecutor::runAllBlocksByOpcode(std::string opcodeToFind) {
    // std::cout << "Running all " << opcodeToFind << " blocks." << "\n";
    std::vector<Sprite *> sprToRun = Scratch::sprites;
    for (Sprite *currentSprite : sprToRun) {
        for (auto &[id, data] : currentSprite->blocks) {
            if (data.opcode != opcodeToFind) continue;

            executor.runBlock(data, currentSprite);
        }
    }
}

Value BlockExecutor::getBlockValue(Block &block, Sprite *sprite) {
    auto &vh = getValueHandlers();
    const auto iterator = vh.find(block.opcode);
    if (iterator != vh.end()) return iterator->second(block, sprite);

    Log::logWarning("Unknown block: " + block.opcode);

    return Value();
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

void BlockExecutor::updateMonitors() {
    for (auto &var : Render::visibleVariables) {
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
                try {
                    Block newBlock;
                    newBlock.opcode = var.opcode;
                    for (const auto &[paramName, paramValue] : var.parameters) {
                        ParsedField parsedField;
                        parsedField.value = Math::removeQuotations(paramValue);
                        (*newBlock.parsedFields)[paramName] = parsedField;
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
                    var.value = executor.getBlockValue(newBlock, sprite);
                } catch (...) {
                    var.value = Value("Unknown...");
                }
            }
        }
    }
}

Value BlockExecutor::getVariableValue(std::string variableId, Sprite *sprite) {
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
    for (const auto &currentSprite : Scratch::sprites) {
        if (currentSprite->isStage) {
            const auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) return globalIt->second.value;
        }
    }

    // Check global lists
    for (const auto &currentSprite : Scratch::sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->lists.find(variableId);
            if (globalIt == currentSprite->lists.end()) continue;
            std::string result;
            std::string seperator = "";
            for (const auto &item : globalIt->second.items) {
                if (item.asString().size() > 1 || !item.isString()) {
                    seperator = " ";
                    break;
                }
            }
            for (const auto &item : globalIt->second.items) {
                result += item.asString() + seperator;
            }
            if (!result.empty() && !seperator.empty()) result.pop_back();
            return Value(result);
        }
    }

    return Value();
}

#ifdef ENABLE_CLOUDVARS
void BlockExecutor::handleCloudVariableChange(const std::string &name, const std::string &value) {
    for (const auto &currentSprite : Scratch::sprites) {
        if (currentSprite->isStage) {
            for (auto it = currentSprite->variables.begin(); it != currentSprite->variables.end(); ++it) {
                if (it->second.name != name) continue;
                it->second.value = Value(value);
                return;
            }
        }
    }
}
#endif

Value BlockExecutor::getCustomBlockValue(std::string valueName, Sprite *sprite, Block block) {
    // get the parent prototype block
    Block *const definitionBlock = Scratch::getBlockParent(&block, sprite);
    const Block *prototypeBlock = Scratch::findBlock(Scratch::getInputValue(*definitionBlock, "custom_block", sprite).asString(), sprite);

    for (auto &[custId, custBlock] : sprite->customBlocks) {
        // variable must be in the same custom block
        if (prototypeBlock != nullptr && custBlock.blockId != prototypeBlock->id) continue;

        const auto it = std::find(custBlock.argumentNames.begin(), custBlock.argumentNames.end(), valueName);

        if (it == custBlock.argumentNames.end()) continue;

        const size_t index = std::distance(custBlock.argumentNames.begin(), it);

        if (index < custBlock.argumentIds.size()) {
            const std::string argumentId = custBlock.argumentIds[index];

            const auto valueIt = custBlock.argumentValues.find(argumentId);
            if (valueIt != custBlock.argumentValues.end()) {
                return valueIt->second;
                continue;
            }

            Log::logWarning("Argument ID found, but no value exists for it.");
            continue;
        }
        Log::logWarning("Index out of bounds for argumentIds!");
    }
    return Value();
}

void BlockExecutor::addToRepeatQueue(Sprite *sprite, Block *block) {
    auto &repeatList = sprite->blockChains[block->blockChainID].blocksToRepeat;
    if (std::find(repeatList.begin(), repeatList.end(), block->id) == repeatList.end()) {
        block->isRepeating = true;
        repeatList.push_back(block->id);
    }
}

void BlockExecutor::removeFromRepeatQueue(Sprite *sprite, Block *block) {
    auto it = sprite->blockChains.find(block->blockChainID);
    if (it == sprite->blockChains.end()) return;

    auto &blocksToRepeat = it->second.blocksToRepeat;
    if (!blocksToRepeat.empty()) {
        block->isRepeating = false;
        block->repeatTimes = -1;
        blocksToRepeat.pop_back();
    }
}

bool BlockExecutor::hasActiveRepeats(Sprite *sprite, std::string blockChainID) {
    if (sprite->toDelete) return false;
    if (sprite->blockChains.find(blockChainID) != sprite->blockChains.end() && !sprite->blockChains[blockChainID].blocksToRepeat.empty()) return true;
    return false;
}
