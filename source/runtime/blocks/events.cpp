#include "blockUtils.hpp"
#include <input.hpp>
#include <sprite.hpp>

SCRATCH_BLOCK(event, whenflagclicked) {
    thread->finished = false;
    return BlockResult::CONTINUE_IMIDIATELY;
}

SCRATCH_BLOCK(event, whenbackdropswitchesto) {
    if (Scratch::getFieldValue(*block, "BACKDROP") == Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name) {
        return BlockResult::CONTINUE_IMIDIATELY;
    }
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(event, whenthisspriteclicked) {
    return BlockResult::CONTINUE_IMIDIATELY;
}

SCRATCH_BLOCK(event, broadcast) {
    Value broadcast;
    if (!Scratch::getInput(block, "BROADCAST_INPUT", thread, sprite, broadcast)) return BlockResult::REPEAT;
    std::string broadcastStr = broadcast.asString();
    
    for (auto &spr : Scratch::sprites) {
        if (spr->hats["event_whenbroadcastreceived"].empty()) continue;
        for (Block *hat : spr->hats["event_whenbroadcastreceived"]) {
            if (Scratch::getFieldValue(*hat, "BROADCAST_OPTION") == broadcastStr) {
                BlockExecutor::startThread(spr, hat);
            }
        }
    }
    
    return BlockResult::CONTINUE_IMIDIATELY;
}

SCRATCH_BLOCK(event, broadcastandwait) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0 ) {
        Value broadcastValue;
        if (!Scratch::getInput(block, "BROADCAST_INPUT", thread, sprite, broadcastValue)) return BlockResult::REPEAT;
        std::string broadcastStr = broadcastValue.asString();
        
        for (auto &spr : Scratch::sprites) {
            if (spr->hats["event_whenbroadcastreceived"].empty()) continue;
            for (Block *hat : spr->hats["event_whenbroadcastreceived"]) {
                if (Scratch::getFieldValue(*hat, "BROADCAST_OPTION") == broadcastStr) {
                    state->threads.push_back(BlockExecutor::startThread(spr, hat));
                }
            }
        }
        
        state->completedSteps = 1;
        if (state->threads.empty()) {
            thread->eraseState(block);
            return BlockResult::CONTINUE_IMIDIATELY;
        }
        return BlockResult::REPEAT;
    }
    for (Sprite *spr : Scratch::sprites) {
        for (auto &t : state->threads) {
            if (!t->finished) return BlockResult::REPEAT;
        }
    }
    thread->eraseState(block);
    return BlockResult::CONTINUE_IMIDIATELY;
}
// TODO: This is currently very poorly optimized. Please fix it, thank you.
SCRATCH_BLOCK(event, whenkeypressed) {
    return BlockResult::CONTINUE_IMIDIATELY;
}

SCRATCH_BLOCK(event, whenbroadcastreceived) {
    return BlockResult::CONTINUE_IMIDIATELY;
}


SCRATCH_SHADOW_BLOCK(event_touchingobjectmenu, TOUCHINGOBJECTMENU)