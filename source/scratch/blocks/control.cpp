#include "control.hpp"
#include "../audio.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "os.hpp"
#include "sprite.hpp"
#include "value.hpp"
#include <iostream>
#include <ostream>

BlockResult ControlBlocks::If(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    bool condition = conditionValue.asBoolean();

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    } else {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    if (condition) {
        auto it = block.parsedInputs->find("SUBSTACK");
        if (it != block.parsedInputs->end()) {
            Block *subBlock = &sprite->blocks[it->second.blockId];
            if (subBlock) {
                for (auto &ranBlock : executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat)) {
                    if (ranBlock->isRepeating) {
                        return BlockResult::RETURN;
                    } else if (ranBlock->stopScript) {
                        ranBlock->stopScript = false;
                        return BlockResult::RETURN;
                    }
                }
            }
        }
    }

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::ifElse(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    bool condition = conditionValue.asBoolean();

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    } else {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    std::string key = condition ? "SUBSTACK" : "SUBSTACK2";
    auto it = block.parsedInputs->find(key);
    if (it != block.parsedInputs->end()) {
        Block *subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) {
            for (auto &ranBlock : executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat)) {
                if (ranBlock->isRepeating) {
                    return BlockResult::RETURN;
                } else if (ranBlock->stopScript) {
                    ranBlock->stopScript = false;
                    return BlockResult::RETURN;
                }
            }
        }
    }

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::createCloneOf(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    // std::cout << "Trying " << std::endl;
    Value inputValue = Scratch::getInputValue(block, "CLONE_OPTION", sprite);

    Sprite *spriteToClone = getAvailableSprite();
    if (!spriteToClone) return BlockResult::CONTINUE;
    if (inputValue.asString() == "_myself_") {
        *spriteToClone = *sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->name == inputValue.asString() && !currentSprite->isClone) {
                *spriteToClone = *currentSprite;
            }
        }
    }
    spriteToClone->blockChains.clear();

    if (spriteToClone != nullptr && !spriteToClone->name.empty()) {
        spriteToClone->isClone = true;
        spriteToClone->isStage = false;
        spriteToClone->toDelete = false;
        spriteToClone->id = Math::generateRandomString(15);
        // Log::log("Cloned " + sprite->name);
        //  add clone to sprite list
        sprites.push_back(spriteToClone);
        Sprite *addedSprite = sprites.back();
        // Run "when I start as a clone" scripts for the clone
        for (Sprite *currentSprite : sprites) {
            if (currentSprite == addedSprite) {
                for (auto &[id, block] : currentSprite->blocks) {
                    if (block.opcode == "control_start_as_clone") {
                        // std::cout << "Running clone block " << block.id << std::endl;
                        executor.runBlock(block, currentSprite, withoutScreenRefresh, fromRepeat);
                    }
                }
            }
        }
    }
    Scratch::sortSprites();
    return BlockResult::CONTINUE;
}
BlockResult ControlBlocks::deleteThisClone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (sprite->isClone) {
        sprite->toDelete = true;
        return BlockResult::CONTINUE;
    }
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::stop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string stopType = Scratch::getFieldValue(block, "STOP_OPTION");
    if (stopType == "all") {
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }
    if (stopType == "this script") {
        for (std::string repeatID : sprite->blockChains[block.blockChainID].blocksToRepeat) {
            Block *repeatBlock = &sprite->blocks[repeatID];
            if (repeatBlock) {
                repeatBlock->repeatTimes = -1;
                repeatBlock->isRepeating = false;
            }
        }
        sprite->blockChains[block.blockChainID].blocksToRepeat.clear();
        block.stopScript = true;
        return BlockResult::RETURN;
    }

    if (stopType == "other scripts in sprite") {
        for (auto &[id, chain] : sprite->blockChains) {
            if (id == block.blockChainID) continue;
            for (std::string repeatID : chain.blocksToRepeat) {
                Block *repeatBlock = &sprite->blocks[repeatID];
                if (repeatBlock) {
                    repeatBlock->repeatTimes = -1;
                }
            }
            chain.blocksToRepeat.clear();
        }
        for (auto &[id, sound] : sprite->sounds) {
            SoundPlayer::stopSound(sound.fullName);
        }
        return BlockResult::CONTINUE;
    }
    return BlockResult::RETURN;
}

BlockResult ControlBlocks::startAsClone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::wait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        Value duration = Scratch::getInputValue(block, "DURATION", sprite);
        if (duration.isNumeric()) {
            block.waitDuration = duration.asDouble() * 1000; // convert to milliseconds
        } else {
            block.waitDuration = 0;
        }

        block.waitTimer.start();
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    block.repeatTimes -= 1;

    if (block.waitTimer.hasElapsed(block.waitDuration) && block.repeatTimes <= -4) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        Scratch::forceRedraw = true;
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

BlockResult ControlBlocks::waitUntil(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);

    bool conditionMet = conditionValue.asBoolean();

    if (conditionMet) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

BlockResult ControlBlocks::repeat(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = Scratch::getInputValue(block, "TIMES", sprite).asInt();
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (block.repeatTimes > 0) {
        auto it = block.parsedInputs->find("SUBSTACK");
        if (it != block.parsedInputs->end()) {
            Block *subBlock = &sprite->blocks[it->second.blockId];
            if (subBlock) {
                executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
            }
        }

        // Countdown
        block.repeatTimes -= 1;
        return BlockResult::RETURN;
    } else {
        block.repeatTimes = -1;
    }
    // std::cout << "done with repeat " << block.id << std::endl;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::While(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    bool condition = conditionValue.asBoolean();

    if (!condition) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        const std::string &blockId = it->second.blockId;
        auto blockIt = sprite->blocks.find(blockId);
        if (blockIt != sprite->blocks.end()) {
            Block *subBlock = &blockIt->second;
            executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
        } else {
            std::cerr << "Invalid blockId: " << blockId << std::endl;
        }
    }

    return BlockResult::RETURN;
}

BlockResult ControlBlocks::repeatUntil(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    bool condition = conditionValue.asBoolean();

    if (condition) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);

        return BlockResult::CONTINUE;
    }

    auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        const std::string &blockId = it->second.blockId;
        auto blockIt = sprite->blocks.find(blockId);
        if (blockIt != sprite->blocks.end()) {
            Block *subBlock = &blockIt->second;
            executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
        } else {
            std::cerr << "Invalid blockId: " << blockId << std::endl;
        }
    }

    // Continue the loop
    return BlockResult::RETURN;
}

BlockResult ControlBlocks::forever(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -3;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) {
            executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
        }
    }
    return BlockResult::RETURN;
}

Value ControlBlocks::getCounter(Block &block, Sprite *sprite) {
    return Value(Scratch::counter);
}

BlockResult ControlBlocks::incrementCounter(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Scratch::counter++;
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::clearCounter(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Scratch::counter = 0;
    return BlockResult::CONTINUE;
}

BlockResult ControlBlocks::forEach(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = Scratch::getInputValue(block, "VALUE", sprite).asInt();
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (block.repeatTimes > 0) {
        BlockExecutor::setVariableValue(Scratch::getFieldId(block, "VARIABLE"), Value(Scratch::getInputValue(block, "VALUE", sprite).asInt() - block.repeatTimes + 1), sprite);

        auto it = block.parsedInputs->find("SUBSTACK");
        if (it != block.parsedInputs->end()) {
            Block *subBlock = &sprite->blocks[it->second.blockId];
            if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
        }

        block.repeatTimes -= 1;
        return BlockResult::RETURN;
    }
    block.repeatTimes = -1;

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}
