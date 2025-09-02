#include "events.hpp"
#include "../input.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "sprite.hpp"

BlockResult EventBlocks::flagClicked(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::whenBackdropSwitchesTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::broadcast(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    broadcastQueue.push_back(Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString());
    return BlockResult::CONTINUE;
}

BlockResult EventBlocks::broadcastAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

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

BlockResult EventBlocks::whenKeyPressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    for (std::string button : Input::inputButtons) {
        if (Scratch::getFieldValue(block, "KEY_OPTION") == button) {
            return BlockResult::CONTINUE;
        }
    }
    return BlockResult::RETURN;
}