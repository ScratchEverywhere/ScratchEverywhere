#include "blockUtils.hpp"
#include "runtime.hpp"
#include "runtime/blockExecutor.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <iostream>
#include <math.hpp>
#include <os.hpp>
#include <ostream>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(control, if) {
    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();
    const auto it = block.parsedInputs->find("SUBSTACK");
    Block *const subBlock = it == block.parsedInputs->end() ? nullptr : &sprite->blocks[it->second.blockId];
    std::vector<std::string> blockID;
    Block *repeatBlock;

    if (!fromRepeat) {
        BlockExecutor::addToRepeatQueue(sprite, &block);
    } else {
        goto end;
    }

    if (!condition || subBlock == nullptr) goto end;
    executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    blockID = sprite->blockChains[block.blockChainID].blocksToRepeat;
    if (blockID.empty()) return BlockResult::RETURN;
    repeatBlock = &sprite->blocks[blockID.back()];
    if (!repeatBlock || repeatBlock != &block) return BlockResult::RETURN;

end:
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, if_else) {
    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();
    const std::string key = condition ? "SUBSTACK" : "SUBSTACK2";
    const auto it = block.parsedInputs->find(key);
    std::vector<std::string> blockID;
    Block *repeatBlock;

    if (!fromRepeat) {
        BlockExecutor::addToRepeatQueue(sprite, &block);
    } else {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    if (it == block.parsedInputs->end()) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }
    Block *const subBlock = &sprite->blocks[it->second.blockId];
    if (subBlock == nullptr) goto end;
    executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    blockID = sprite->blockChains[block.blockChainID].blocksToRepeat;
    if (blockID.empty()) return BlockResult::RETURN;
    repeatBlock = &sprite->blocks[blockID.back()];
    if (!repeatBlock || repeatBlock != &block) return BlockResult::RETURN;

end:
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, create_clone_of) {
    if (Scratch::cloneCount >= Scratch::maxClones) return BlockResult::CONTINUE;
    const Value inputValue = Scratch::getInputValue(block, "CLONE_OPTION", sprite);

    Sprite *clonedSprite = new Sprite();
    Sprite *targetSprite = nullptr;
    if (inputValue.asString() == "_myself_") {
        targetSprite = sprite;
    } else {
        for (Sprite *currentSprite : Scratch::sprites) {
            if (!currentSprite->isClone && !currentSprite->isStage && currentSprite->name == inputValue.asString()) {
                targetSprite = currentSprite;
                break;
            }
        }
    }

    if (targetSprite == nullptr) {
        delete clonedSprite;
        return BlockResult::CONTINUE;
    }

    *clonedSprite = *targetSprite;
    clonedSprite->blockChains.clear();
    clonedSprite->isClone = true;
    clonedSprite->toDelete = false;
    clonedSprite->renderInfo.forceUpdate = true;
    clonedSprite->id = Math::generateRandomString(15);

    BlockExecutor::linkPointers(clonedSprite);

    const int sourceIndex = (Scratch::sprites.size() - 1) - targetSprite->layer;
    auto it = Scratch::sprites.insert(Scratch::sprites.begin() + sourceIndex + 1, clonedSprite);

    for (size_t i = 0; i < sourceIndex + 1; i++) {
        Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
    }

    Scratch::cloneQueue.push_back(*it);
    Scratch::cloneCount++;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, delete_this_clone) {
    if (sprite->isClone) {
        sprite->toDelete = true;
        Scratch::cloneCount--;
        for (auto &[id, chain] : sprite->blockChains) {
            chain.blocksToRepeat.clear();
        }
        return BlockResult::RETURN;
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, stop) {
    const std::string stopType = Scratch::getFieldValue(block, "STOP_OPTION");
    if (stopType == "all") {
        Scratch::stopClicked();
        return BlockResult::RETURN;
    }
    if (stopType == "this script") {
        sprite->blockChains[block.blockChainID].blocksToRepeat.clear();
        return BlockResult::RETURN;
    }

    if (stopType == "other scripts in sprite") {
        for (auto &[id, chain] : sprite->blockChains) {
            if (id == block.blockChainID) continue;
            chain.blocksToRepeat.clear();
        }
        for (Sound sound : sprite->sounds)
            SoundPlayer::stopSound(sound.fullName);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

SCRATCH_BLOCK_NOP(control, start_as_clone)

SCRATCH_BLOCK(control, wait) {
    if (!fromRepeat) {
        const Value duration = Scratch::getInputValue(block, "DURATION", sprite);
        block.waitDuration = duration.asDouble() * 1000;

        block.waitTimer.start();
        BlockExecutor::addToRepeatQueue(sprite, &block);

        return BlockResult::RETURN;
    }

    if (!block.waitTimer.hasElapsed(block.waitDuration)) return BlockResult::RETURN;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, wait_until) {
    if (!fromRepeat) {
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool conditionMet = conditionValue.asBoolean();

    if (!conditionMet) return BlockResult::RETURN;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, repeat) {
    if (!fromRepeat) {
        block.repeatTimes = std::round(Scratch::getInputValue(block, "TIMES", sprite).asDouble());
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (block.repeatTimes <= 0) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }
    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *const subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    }

    // Countdown
    block.repeatTimes--;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, while) {
    if (!fromRepeat) {
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();

    if (!condition) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it == block.parsedInputs->end()) return BlockResult::RETURN;

    const std::string &blockId = it->second.blockId;
    const auto blockIt = sprite->blocks.find(blockId);
    if (blockIt != sprite->blocks.end()) {
        Block *subBlock = &blockIt->second;
        executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    } else {
        Log::logError("Invalid blockId: " + blockId);
    }

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, repeat_until) {
    if (!fromRepeat) {
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();

    if (condition) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);

        return BlockResult::CONTINUE;
    }

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it == block.parsedInputs->end()) return BlockResult::RETURN;

    const std::string &blockId = it->second.blockId;
    auto blockIt = sprite->blocks.find(blockId);
    if (blockIt != sprite->blocks.end()) {
        Block *subBlock = &blockIt->second;
        executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    } else {
        Log::logError("Invalid blockId: " + blockId);
    }

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, forever) {
    if (!fromRepeat) {
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *const subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    }
    return BlockResult::RETURN;
}

SCRATCH_REPORTER_BLOCK(control, get_counter) {
    return Value(Scratch::counter);
}

SCRATCH_BLOCK(control, incr_counter) {
    Scratch::counter++;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, clear_counter) {
    Scratch::counter = 0;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, for_each) {
    double upperBound = Scratch::getInputValue(block, "VALUE", sprite).asDouble();
    if (!fromRepeat) {
        block.repeatTimes = 0;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (block.repeatTimes >= upperBound) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    BlockExecutor::setVariableValue(Scratch::getFieldId(block, "VARIABLE"), Value(block.repeatTimes + 1), sprite);

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, false);
    }

    block.repeatTimes++;
    return BlockResult::RETURN;
}
