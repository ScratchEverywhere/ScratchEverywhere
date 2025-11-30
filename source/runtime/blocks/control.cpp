#include "blockUtils.hpp"
#include <audio.hpp>
#include <blockExecutor.hpp>
#include <interpret.hpp>
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

    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    } else {
        goto end;
    }

    if (!condition || subBlock == nullptr) goto end;
    for (auto &ranBlock : executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat)) {
        if (ranBlock->isRepeating) return BlockResult::RETURN;
    }

end:
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, if_else) {
    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();

    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    } else {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    const std::string key = condition ? "SUBSTACK" : "SUBSTACK2";
    const auto it = block.parsedInputs->find(key);
    if (it == block.parsedInputs->end()) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }
    Block *const subBlock = &sprite->blocks[it->second.blockId];
    if (subBlock == nullptr) goto end;
    for (auto &ranBlock : executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat)) {
        if (ranBlock->isRepeating) return BlockResult::RETURN;
    }

end:
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, create_clone_of) {
    const Value inputValue = Scratch::getInputValue(block, "CLONE_OPTION", sprite);

    Sprite *const spriteToClone = getAvailableSprite();
    if (!spriteToClone) return BlockResult::CONTINUE;
    if (inputValue.asString() == "_myself_") {
        *spriteToClone = *sprite;
    } else {
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->name == inputValue.asString() && !currentSprite->isClone) *spriteToClone = *currentSprite;
        }
    }
    spriteToClone->blockChains.clear();

    if (spriteToClone == nullptr || spriteToClone->name.empty()) return BlockResult::CONTINUE;

    spriteToClone->isClone = true;
    spriteToClone->isStage = false;
    spriteToClone->toDelete = false;
    spriteToClone->id = Math::generateRandomString(15);
    sprites.push_back(spriteToClone);
    Sprite *addedSprite = sprites.back();

    for (Sprite *currentSprite : sprites) {
        if (currentSprite != addedSprite) continue;
        for (auto &[id, block] : currentSprite->blocks) {
            if (block.opcode == "control_start_as_clone") executor.runBlock(block, currentSprite, withoutScreenRefresh, fromRepeat);
        }
    }

    Scratch::sortSprites();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, delete_this_clone) {
    if (sprite->isClone) sprite->toDelete = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, stop) {
    const std::string stopType = Scratch::getFieldValue(block, "STOP_OPTION");
    if (stopType == "all") {
        Scratch::shouldStop = true;
        return BlockResult::RETURN;
    }
    if (stopType == "this script") {
        for (std::string repeatID : sprite->blockChains[block.blockChainID].blocksToRepeat) {
            Block *const repeatBlock = &sprite->blocks[repeatID];
            if (repeatBlock) repeatBlock->repeatTimes = -1;
        }

        sprite->blockChains[block.blockChainID].blocksToRepeat.clear();
        return BlockResult::RETURN;
    }

    if (stopType == "other scripts in sprite") {
        for (auto &[id, chain] : sprite->blockChains) {
            if (id == block.blockChainID) continue;
            for (std::string repeatID : chain.blocksToRepeat) {
                Block *repeatBlock = &sprite->blocks[repeatID];
                if (repeatBlock) repeatBlock->repeatTimes = -1;
            }
            chain.blocksToRepeat.clear();
        }
        for (auto &[id, sound] : sprite->sounds)
            SoundPlayer::stopSound(sound.fullName);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

SCRATCH_BLOCK_NOP(control, start_as_clone)

SCRATCH_BLOCK(control, wait) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        const Value duration = Scratch::getInputValue(block, "DURATION", sprite);
        block.waitDuration = duration.isNumeric() ? duration.asDouble() * 1000 : 0;

        block.waitTimer.start();
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    block.repeatTimes -= 1;

    if (!block.waitTimer.hasElapsed(block.waitDuration) || block.repeatTimes > -4) return BlockResult::RETURN;
    block.repeatTimes = -1;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, wait_until) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -4;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);

    const bool conditionMet = conditionValue.asBoolean();

    if (!conditionMet) return BlockResult::RETURN;
    block.repeatTimes = -1;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(control, repeat) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = Scratch::getInputValue(block, "TIMES", sprite).asInt();
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (block.repeatTimes <= 0) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }
    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *const subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
    }

    // Countdown
    block.repeatTimes -= 1;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, while) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();

    if (!condition) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it == block.parsedInputs->end()) return BlockResult::RETURN;

    const std::string &blockId = it->second.blockId;
    const auto blockIt = sprite->blocks.find(blockId);
    if (blockIt != sprite->blocks.end()) {
        Block *subBlock = &blockIt->second;
        executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
    } else {
        Log::logError("Invalid blockId: " + blockId);
    }

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, repeat_until) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const Value conditionValue = Scratch::getInputValue(block, "CONDITION", sprite);
    const bool condition = conditionValue.asBoolean();

    if (condition) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);

        return BlockResult::CONTINUE;
    }

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it == block.parsedInputs->end()) return BlockResult::RETURN;

    const std::string &blockId = it->second.blockId;
    auto blockIt = sprite->blocks.find(blockId);
    if (blockIt != sprite->blocks.end()) {
        Block *subBlock = &blockIt->second;
        executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
    } else {
        Log::logError("Invalid blockId: " + blockId);
    }

    return BlockResult::RETURN;
}

SCRATCH_BLOCK(control, forever) {
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -3;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *const subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
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
    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = Scratch::getInputValue(block, "VALUE", sprite).asInt();
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (block.repeatTimes <= 0) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    BlockExecutor::setVariableValue(Scratch::getFieldId(block, "VARIABLE"), Value(Scratch::getInputValue(block, "VALUE", sprite).asInt() - block.repeatTimes + 1), sprite);

    const auto it = block.parsedInputs->find("SUBSTACK");
    if (it != block.parsedInputs->end()) {
        Block *subBlock = &sprite->blocks[it->second.blockId];
        if (subBlock) executor.runBlock(*subBlock, sprite, withoutScreenRefresh, fromRepeat);
    }

    block.repeatTimes -= 1;
    return BlockResult::RETURN;
}
