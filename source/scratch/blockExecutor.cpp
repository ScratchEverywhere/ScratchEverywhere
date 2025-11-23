#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "os.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iterator>
#include <ratio>
#include <utility>
#include <vector>

#ifdef ENABLE_CLOUDVARS
#include <mist/mist.hpp>

extern std::unique_ptr<MistConnection> cloudConnection;
#endif

size_t blocksRun = 0;
Timer BlockExecutor::timer;

std::unordered_map<std::string, std::function<BlockResult(Block &, Sprite *, bool *, bool)>> BlockExecutor::handlers;
std::unordered_map<std::string, std::function<Value(Block &, Sprite *)>> BlockExecutor::valueHandlers;

std::vector<Block *> BlockExecutor::runBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::vector<Block *> ranBlocks;
    auto start = std::chrono::high_resolution_clock::now();
    Block *currentBlock = &block;

    bool localWithoutRefresh = false;
    if (!withoutScreenRefresh) {
        withoutScreenRefresh = &localWithoutRefresh;
    }

    if (!sprite || sprite->toDelete) {
        return ranBlocks;
    }

    while (currentBlock && currentBlock->id != "null") {
        blocksRun += 1;
        ranBlocks.push_back(currentBlock);
        BlockResult result = executeBlock(*currentBlock, sprite, withoutScreenRefresh, fromRepeat);

        if (result == BlockResult::RETURN) {
            return ranBlocks;
        }

        // runBroadcasts();

        // Move to next block
        if (!currentBlock->next.empty()) {
            currentBlock = &sprite->blocks[currentBlock->next];
        } else {
            break;
        }
    }

    // Timing measurement
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    if (duration.count() > 0) {
        // std::cout << " took " << duration.count() << " milliseconds!" << std::endl;
    }
    return ranBlocks;
}

BlockResult BlockExecutor::executeBlock(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    auto iterator = handlers.find(block.opcode);
    if (iterator != handlers.end()) {
        return iterator->second(block, sprite, withoutScreenRefresh, fromRepeat);
    }

    Log::logWarning("Unkown block: " + block.opcode);

    return BlockResult::CONTINUE;
}

void BlockExecutor::runRepeatBlocks() {
    blocksRun = 0;
    bool withoutRefresh = false;

    // repeat ONLY the block most recently added to the repeat chain,,,
    std::vector<Sprite *> sprToRun = sprites;
    for (auto &sprite : sprToRun) {
        for (auto &[id, blockChain] : sprite->blockChains) {
            auto &repeatList = blockChain.blocksToRepeat;
            if (!repeatList.empty()) {
                std::string toRepeat = repeatList.back();
                if (!toRepeat.empty()) {
                    Block *toRun = &sprite->blocks[toRepeat];
                    if (toRun != nullptr) {
                        executor.runBlock(*toRun, sprite, &withoutRefresh, true);
                    }
                }
            }
        }
    }
    // delete sprites ready for deletion

    for (auto &toDelete : sprites) {
        if (!toDelete->toDelete) continue;
        for (auto &[id, block] : toDelete->blocks) {
            for (std::string repeatID : toDelete->blockChains[block.blockChainID].blocksToRepeat) {
                Block *repeatBlock = findBlock(repeatID);
                if (repeatBlock) {
                    repeatBlock->repeatTimes = -1;
                }
            }
        }
        toDelete->isDeleted = true;
    }
    sprites.erase(std::remove_if(sprites.begin(), sprites.end(),
                                 [](Sprite *s) { return s->toDelete; }),
                  sprites.end());
}

void BlockExecutor::runRepeatsWithoutRefresh(Sprite *sprite, std::string blockChainID) {
    bool withoutRefresh = true;
    if (sprite->blockChains.find(blockChainID) != sprite->blockChains.end()) {
        while (!sprite->blockChains[blockChainID].blocksToRepeat.empty()) {
            std::string toRepeat = sprite->blockChains[blockChainID].blocksToRepeat.back();
            Block *toRun = findBlock(toRepeat);
            if (toRun != nullptr)
                executor.runBlock(*toRun, sprite, &withoutRefresh, true);
        }
    }
}

BlockResult BlockExecutor::runCustomBlock(Sprite *sprite, Block &block, Block *callerBlock, bool *withoutScreenRefresh) {
    for (auto &[id, data] : sprite->customBlocks) {
        if (id == block.customBlockId) {
            // Set up argument values
            for (std::string arg : data.argumentIds) {
                data.argumentValues[arg] = block.parsedInputs->find(arg) == block.parsedInputs->end() ? Value(0) : Scratch::getInputValue(block, arg, sprite);
            }

            // std::cout << "running custom block " << data.blockId << std::endl;

            // Get the parent of the prototype block (the definition containing all blocks)
            Block *customBlockDefinition = &sprite->blocks[sprite->blocks[data.blockId].parent];

            callerBlock->customBlockPtr = customBlockDefinition;

            bool localWithoutRefresh = data.runWithoutScreenRefresh;

            // If the parent chain is running without refresh, force this one to also run without refresh
            if (!localWithoutRefresh && withoutScreenRefresh != nullptr) {
                localWithoutRefresh = *withoutScreenRefresh;
            }

            // std::cout << "RWSR = " << localWithoutRefresh << std::endl;

            // Execute the custom block definition
            executor.runBlock(*customBlockDefinition, sprite, &localWithoutRefresh);

            if (localWithoutRefresh) {
                BlockExecutor::runRepeatsWithoutRefresh(sprite, customBlockDefinition->blockChainID);
            }

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
            std::string drivePrefix = OS::getFilesystemRootPrefix();
            Unzip::filePath.replace(0, 3, drivePrefix);
        } else {
            Unzip::filePath = Unzip::filePath;
        }

        if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
            Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
        }
        if (!std::filesystem::exists(Unzip::filePath + "/project.json"))
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
            std::string drivePrefix = OS::getFilesystemRootPrefix();
            Unzip::filePath.replace(0, 3, drivePrefix);
        } else {
            Unzip::filePath = Unzip::filePath;
        }
        if (Unzip::filePath.size() >= 1 && Unzip::filePath.back() == '/') {
            Unzip::filePath = Unzip::filePath.substr(0, Unzip::filePath.size() - 1);
        }
        if (!std::filesystem::exists(Unzip::filePath + "/project.json"))
            Unzip::filePath = Unzip::filePath + ".sb3";

        Scratch::dataNextProject = Scratch::getInputValue(block, "arg1", sprite);
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }
    return BlockResult::CONTINUE;
}

std::vector<std::pair<Block *, Sprite *>> BlockExecutor::runBroadcast(std::string broadcastToRun) {
    std::vector<std::pair<Block *, Sprite *>> blocksToRun;

    // find all matching "when I receive" blocks
    std::vector<Sprite *> sprToRun = sprites;
    for (auto *currentSprite : sprToRun) {
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.opcode == "event_whenbroadcastreceived" &&
                Scratch::getFieldValue(block, "BROADCAST_OPTION") == broadcastToRun) {
                blocksToRun.push_back({&block, currentSprite});
            }
        }
    }

    // run each matching block
    for (auto &[blockPtr, spritePtr] : blocksToRun) {
        executor.runBlock(*blockPtr, spritePtr);
    }

    return blocksToRun;
}

std::vector<std::pair<Block *, Sprite *>> BlockExecutor::runBroadcasts() {
    std::vector<std::pair<Block *, Sprite *>> blocksToRun;

    if (broadcastQueue.empty()) {
        return blocksToRun;
    }

    std::string currentBroadcast = broadcastQueue.front();
    broadcastQueue.erase(broadcastQueue.begin());

    auto results = runBroadcast(currentBroadcast);
    blocksToRun.insert(blocksToRun.end(), results.begin(), results.end());

    if (!broadcastQueue.empty()) {
        auto moreResults = runBroadcasts();
        blocksToRun.insert(blocksToRun.end(), moreResults.begin(), moreResults.end());
    }

    return blocksToRun;
}

std::vector<Block *> BlockExecutor::runAllBlocksByOpcode(std::string opcodeToFind) {
    // std::cout << "Running all " << opcodeToFind << " blocks." << "\n";
    std::vector<Block *> blocksRun;
    std::vector<Sprite *> sprToRun = sprites;
    for (Sprite *currentSprite : sprToRun) {
        for (auto &[id, data] : currentSprite->blocks) {
            if (data.opcode == opcodeToFind) {
                // runBlock(data,currentSprite);
                blocksRun.push_back(&data);
                executor.runBlock(data, currentSprite);
            }
        }
    }
    return blocksRun;
}

Value BlockExecutor::getBlockValue(Block &block, Sprite *sprite) {
    auto iterator = valueHandlers.find(block.opcode);
    if (iterator != valueHandlers.end()) {
        return iterator->second(block, sprite);
    }

    Log::logWarning("Unkown block: " + block.opcode);

    return Value();
}

void BlockExecutor::setVariableValue(const std::string &variableId, const Value &newValue, Sprite *sprite) {
    // Set sprite variable
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        it->second.value = newValue;
        return;
    }

    // Set global variable
    for (auto &currentSprite : sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) {
                globalIt->second.value = newValue;
#ifdef ENABLE_CLOUDVARS
                if (globalIt->second.cloud) cloudConnection->set(globalIt->second.name, globalIt->second.value.asString());
#endif
                return;
            }
        }
    }
}

Value BlockExecutor::getMonitorValue(Monitor &var) {
    Sprite *sprite = nullptr;
    for (auto &spr : sprites) {
        if (var.spriteName == "" && spr->isStage) {
            sprite = spr;
            break;
        }
        if (spr->name == var.spriteName && !spr->isClone) {
            sprite = spr;
            break;
        }
    }

    std::string monitorName = "";
    if (var.opcode == "data_variable") {
        var.value = BlockExecutor::getVariableValue(var.id, sprite);
        monitorName = Math::removeQuotations(var.parameters["VARIABLE"]);
    } else if (var.opcode == "data_listcontents") {
        monitorName = Math::removeQuotations(var.parameters["LIST"]);
        // Check lists
        auto listIt = sprite->lists.find(var.id);
        if (listIt != sprite->lists.end()) {
            std::string result;
            std::string seperator = "";
            for (const auto &item : listIt->second.items) {
                if (item.asString().size() > 1 || !item.isString()) {
                    seperator = "\n";
                    break;
                }
            }
            for (const auto &item : listIt->second.items) {
                result += item.asString() + seperator;
            }
            if (!result.empty() && !seperator.empty()) result.pop_back();
            Value val(result);
            var.value = val;
        }

        // Check global lists
        for (const auto &currentSprite : sprites) {
            if (currentSprite->isStage) {
                auto globalIt = currentSprite->lists.find(var.id);
                if (globalIt != currentSprite->lists.end()) {
                    std::string result;
                    std::string seperator = "";
                    for (const auto &item : globalIt->second.items) {
                        if (item.asString().size() > 1 || !item.isString()) {
                            seperator = "\n";
                            break;
                        }
                    }
                    for (const auto &item : globalIt->second.items) {
                        result += item.asString() + seperator;
                    }
                    if (!result.empty() && !seperator.empty()) result.pop_back();
                    Value val(result);
                    var.value = val;
                }
            }
        }
    } else {
        try {
            Block newBlock;
            newBlock.opcode = var.opcode;
            monitorName = var.opcode;
            var.value = executor.getBlockValue(newBlock, sprite);
        } catch (...) {
            var.value = Value("Unknown...");
        }
    }

    std::string renderText;
    if (var.mode != "large") {
        if (var.spriteName != "")
            renderText = var.spriteName + ": ";
        if (monitorName != "")
            renderText = renderText + monitorName + ": ";
    }
    renderText = renderText + var.value.asString();
    return Value(renderText);
}

Value BlockExecutor::getVariableValue(std::string variableId, Sprite *sprite) {
    // Check sprite variables
    auto it = sprite->variables.find(variableId);
    if (it != sprite->variables.end()) {
        return it->second.value;
    }

    // Check lists
    auto listIt = sprite->lists.find(variableId);
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
        Value val(result);
        return val;
    }

    // Check global variables
    for (const auto &currentSprite : sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->variables.find(variableId);
            if (globalIt != currentSprite->variables.end()) {
                return globalIt->second.value;
            }
        }
    }

    // Check global lists
    for (const auto &currentSprite : sprites) {
        if (currentSprite->isStage) {
            auto globalIt = currentSprite->lists.find(variableId);
            if (globalIt != currentSprite->lists.end()) {
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
                Value val(result);
                return val;
            }
        }
    }

    return Value();
}

#ifdef ENABLE_CLOUDVARS
void BlockExecutor::handleCloudVariableChange(const std::string &name, const std::string &value) {
    for (const auto &currentSprite : sprites) {
        if (currentSprite->isStage) {
            for (auto it = currentSprite->variables.begin(); it != currentSprite->variables.end(); ++it) {
                if (it->second.name == name) {
                    it->second.value = Value(value);
                    return;
                }
            }
        }
    }
}
#endif

Value BlockExecutor::getCustomBlockValue(std::string valueName, Sprite *sprite, Block block) {

    // get the parent prototype block
    Block *definitionBlock = getBlockParent(&block);
    Block *prototypeBlock = findBlock(Scratch::getInputValue(*definitionBlock, "custom_block", sprite).asString());

    for (auto &[custId, custBlock] : sprite->customBlocks) {

        // variable must be in the same custom block
        if (prototypeBlock != nullptr && custBlock.blockId != prototypeBlock->id) continue;

        auto it = std::find(custBlock.argumentNames.begin(), custBlock.argumentNames.end(), valueName);

        if (it != custBlock.argumentNames.end()) {
            size_t index = std::distance(custBlock.argumentNames.begin(), it);

            if (index < custBlock.argumentIds.size()) {
                std::string argumentId = custBlock.argumentIds[index];

                auto valueIt = custBlock.argumentValues.find(argumentId);
                if (valueIt != custBlock.argumentValues.end()) {
                    return valueIt->second;
                } else {
                    Log::logWarning("Argument ID found, but no value exists for it.");
                }
            } else {
                Log::logWarning("Index out of bounds for argumentIds!");
            }
        }
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
    if (it != sprite->blockChains.end()) {
        auto &blocksToRepeat = it->second.blocksToRepeat;
        if (!blocksToRepeat.empty()) {
            block->isRepeating = false;
            block->repeatTimes = -1;
            blocksToRepeat.pop_back();
        }
    }
}

bool BlockExecutor::hasActiveRepeats(Sprite *sprite, std::string blockChainID) {
    if (sprite->toDelete) return false;
    if (sprite->blockChains.find(blockChainID) != sprite->blockChains.end()) {
        if (!sprite->blockChains[blockChainID].blocksToRepeat.empty()) return true;
    }
    return false;
}
