#include "blockUtils.hpp"
#include <input.hpp>
#include <types.hpp>

SCRATCH_BLOCK(event, whenflagclicked) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, whenflagclicked);
SCRATCH_BYTECODE_BLOCK(event, whenflagclicked) {
    outFirstInstr = compiler.here();
    return true;
}

SCRATCH_BLOCK(event, whenbackdropswitchesto) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, whenbackdropswitchesto);
SCRATCH_BYTECODE_BLOCK(event, whenbackdropswitchesto) {
    outFirstInstr = compiler.here();
    return true;
}

SCRATCH_BLOCK(event, whenthisspriteclicked) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, whenthisspriteclicked);
SCRATCH_BYTECODE_BLOCK(event, whenthisspriteclicked) {
    outFirstInstr = compiler.here();
    return true;
}

SCRATCH_BLOCK(event, whenstageclicked) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, whenstageclicked);
SCRATCH_BYTECODE_BLOCK(event, whenstageclicked) {
    outFirstInstr = compiler.here();
    return true;
}

SCRATCH_BLOCK_POSITIONAL_S_VOID(event, broadcast) {
    BlockExecutor::startBroadcastHats(str);
}
SCRATCH_BLOCK(event, broadcast) {
    std::string broadcast;

    if (!Scratch::getInputValueAs(block, "BROADCAST_INPUT", thread, sprite, broadcast)) return BlockResult::REPEAT;
    block_event_broadcast_pos_(sprite, broadcast);

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, broadcast, {"BROADCAST_INPUT", BytecodeStackKind::String});

SCRATCH_BLOCK(event, broadcastandwait) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) {
        std::string broadcast;

        if (!Scratch::getInputValueAs(block, "BROADCAST_INPUT", thread, sprite, broadcast)) return BlockResult::REPEAT;
        state->threads = BlockExecutor::startBroadcastHats(broadcast);

        fprintf(stderr, "[BCWAIT] start '%s' threads=%zu ids=", broadcast.c_str(), state->threads.size());
        for (auto id : state->threads) fprintf(stderr, "%llu,", (unsigned long long)id);
        fprintf(stderr, "\n");

        state->completedSteps = 1;
        if (state->threads.empty()) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        return BlockResult::REPEAT;
    }

    for (auto &stateID : state->threads) {
        if (BlockExecutor::isThreadRunning(stateID)) return BlockResult::REPEAT;
    }
    fprintf(stderr, "[BCWAIT] done\n");
    thread->eraseState(block);
    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(event, broadcastandwait) {
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "BROADCAST_INPUT", BytecodeStackKind::String);
    compiler.emit(BytecodeOp::BroadcastAndWait);
    return true;
}

// FIXME: This is currently very poorly optimized. Please fix it, thank you.
SCRATCH_BLOCK(event, whenkeypressed) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, whenkeypressed);
SCRATCH_BYTECODE_BLOCK(event, whenkeypressed) {
    outFirstInstr = compiler.here();
    return true;
}

SCRATCH_BLOCK(event, whenbroadcastreceived) {
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(event, whenbroadcastreceived);
SCRATCH_BYTECODE_BLOCK(event, whenbroadcastreceived) {
    outFirstInstr = compiler.here();
    return true;
}

SCRATCH_SHADOW_BLOCK(event_touchingobjectmenu, TOUCHINGOBJECTMENU)
