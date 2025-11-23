#include "blockUtils.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "sprite.hpp"

namespace blocks::events {
SCRATCH_BLOCK_NOP(event, whenflagclicked)

SCRATCH_BLOCK_NOP(event, whenbackdropswitchesto)

SCRATCH_BLOCK(event, broadcast) {
    broadcastQueue.push_back(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, broadcastandwait) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -10;
        BlockExecutor::addToRepeatQueue(sprite, &block);
        block.broadcastsRun = BlockExecutor::runBroadcast(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
    }

    bool shouldEnd = true;
    for (auto &[blockPtr, spritePtr] : block.broadcastsRun) {
        if (spritePtr->toDelete) continue;
        if (!spritePtr->blockChains[blockPtr->blockChainID].blocksToRepeat.empty()) {
            shouldEnd = false;
            break;
        }
    }

    if (!shouldEnd) return BlockResult::RETURN;

    block.repeatTimes = -1;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, whenkeypressed) {
    for (std::string button : Input::inputButtons) {
        if (Scratch::getFieldValue(block, "KEY_OPTION") == button) {
            return BlockResult::CONTINUE;
        }
    }
    return BlockResult::RETURN;
}
} // namespace blocks::events
