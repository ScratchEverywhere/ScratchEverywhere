#include "events.hpp"
#include "../input.hpp"

BlockResult EventBlocks::flagClicked(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::broadcast(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    broadcastQueue.push_back(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::broadcastAndWait(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    if (block.repeatTimes == -1) {
        block.repeatTimes = -10;
        BlockExecutor::addToRepeatQueue(sprite, &block);
        broadcastQueue.push_back(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
        block.broadcastsRun = BlockExecutor::runBroadcasts();
    }

    bool shouldEnd = true;
    for (auto &[blockPtr, spritePtr] : block.broadcastsRun) {
        if (!spritePtr->blockChains[blockPtr->blockChainID].blocksToRepeat.empty()) {
            shouldEnd = false;
        }
    }

    if (!shouldEnd) return BlockResult::RETURN;

    block.repeatTimes = -1;
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::whenKeyPressed(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    for (std::string button : Input::inputButtons) {
        if (block.fields.at("KEY_OPTION")[0] == button) {
            return BlockResult::CONTINUE;
        }
    }
    return BlockResult::RETURN;
}