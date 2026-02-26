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
    std::string broadcastName = Scratch::getInputValue(block, "BROADCAST_INPUT", sprite).asString();

    if (!fromRepeat) {
        for (Sprite *spr : Scratch::sprites) {
            for (auto &[id, hat_block] : spr->blocks) {
                if (hat_block.opcode == "event_whenbroadcastreceived" && Scratch::getFieldValue(hat_block, "BROADCAST_OPTION") == broadcastName) {
                    Scratch::broadcastQueue.push_back(broadcastName);
                    BlockExecutor::addToRepeatQueue(sprite, &block);
                    return BlockResult::RETURN;
                }
            }
        }
        return BlockResult::CONTINUE;
    }

    if (block.broadcastsRun.empty()) {
        for (Sprite *spr : Scratch::sprites) {
            for (auto &[id, chain] : spr->blockChains) {
                if (chain.blocksToRepeat.empty()) continue;

                for (auto &chainBlock : chain.blockChain) {
                    if (chainBlock->opcode == "event_whenbroadcastreceived" && Scratch::getFieldValue(*chainBlock, "BROADCAST_OPTION") == broadcastName) {
                        block.broadcastsRun.push_back({chainBlock, spr});
                        break;
                    }
                }
            }
        }
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
    block.broadcastsRun.clear();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_NOP(event, whenkeypressed)

SCRATCH_BLOCK_NOP(event, whenbroadcastreceived)

SCRATCH_BLOCK_NOP(event, whenthisspriteclicked)
