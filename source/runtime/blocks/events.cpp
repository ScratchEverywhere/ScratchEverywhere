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
    Scratch::newBroadcast = broadcast.asString();
    BlockExecutor::runAllBlocksByOpcode("event_whenbroadcastreceived");
    
    return BlockResult::CONTINUE_IMIDIATELY;
}

SCRATCH_BLOCK(event, broadcastandwait) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0 ) {
        Value broadcastValue;
        if (!Scratch::getInput(block, "BROADCAST_INPUT", thread, sprite, broadcastValue)) return BlockResult::REPEAT;
        Scratch::newBroadcast = broadcastValue.asString();
        
        std::vector<ScriptThread *> *out;
        BlockExecutor::runAllBlocksByOpcode("event_whenbroadcastreceived", out);
        state->threads = *out;
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
    std::string key = Scratch::getFieldValue(*block, "KEY_OPTION");
    if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && (Input::keyHeldDuration.find(key)->second == 1 || Input::keyHeldDuration.find(key)->second > 15 * (Scratch::FPS / 30.0f))) {
        return BlockResult::CONTINUE_IMIDIATELY;
    }
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(event, whenbroadcastreceived) {
    std::string key = Scratch::getFieldValue(*block, "KEY_OPTION");
    if (Scratch::newBroadcast == Scratch::getFieldValue(*block, "BROADCAST_OPTION")) {
        return BlockResult();
    }
    return BlockResult::RETURN;
}


SCRATCH_SHADOW_BLOCK(event_touchingobjectmenu, TOUCHINGOBJECTMENU)