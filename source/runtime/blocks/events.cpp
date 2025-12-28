#include "blockUtils.hpp"
#include <input.hpp>
#include <sprite.hpp>

SCRATCH_BLOCK_NOP(event, whenflagclicked)

SCRATCH_BLOCK_NOP(event, whenbackdropswitchesto)

SCRATCH_BLOCK(event, broadcast) {
    Scratch::broadcastQueue.push_back(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(event, broadcastandwait) {
    if (!fromRepeat) {
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

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_NOP(event, whenkeypressed)

SCRATCH_BLOCK_NOP(event, whenbroadcastreceived)
