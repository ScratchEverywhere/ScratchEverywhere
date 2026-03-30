#include "blockUtils.hpp"
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(procedures, call) {
    BlockState *state = thread->getState(block);

    if (state->completedSteps == -2) {
    executeBlock:
        BlockResult result = BlockExecutor::runThread(*state->threads[0], *sprite, nullptr);
        if (result == BlockResult::RETURN || state->threads[0]->finished) {
            if (outValue) *outValue = state->threads[0]->returnValue;
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        return BlockResult::REPEAT;
    }

    if (state->completedSteps == 0) {
        if (block->MyBlockDefinitionID == nullptr ||
            block->MyBlockDefinitionID->blockFunction == nullptr) {
            return BlockResult::CONTINUE;
        }

        ScriptThread *newThread;
        if (!Pools::threads.empty()) {
            newThread = Pools::threads.back();
            Pools::threads.pop_back();
        } else {
            newThread = new ScriptThread();
        }
        newThread->blockHat = block->MyBlockDefinitionID;
        newThread->nextBlock = block->MyBlockDefinitionID;
        newThread->withoutScreenRefresh = block->MyBlockWithoutScreenRefresh;
        newThread->finished = false;
        newThread->returnValue = Value();
        newThread->MyBlocksVariablen.clear();
        state->threads.push_back(newThread);
        state->completedSteps = 1;
    }

    while ((size_t)(state->completedSteps - 1) < block->argumentIDs.size()) {
        int argIdx = state->completedSteps - 1;
        Value argVal;
        if (!Scratch::getInput(block, block->argumentIDs[argIdx], thread, sprite, argVal))
            return BlockResult::REPEAT;
        state->threads[0]->MyBlocksVariablen[block->argumentIDs[argIdx]] = argVal;
        state->completedSteps++;
    }

    state->completedSteps = -2;
    goto executeBlock;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(procedures, prototype) {
    for (size_t i = 0; i < block->argumentIDs.size(); i++) {
        const std::string &argId = block->argumentIDs[i];
        const std::string &argName = (i < block->argumentNames.size())
                                         ? block->argumentNames[i]
                                         : argId;

        auto it = thread->MyBlocksVariablen.find(argId);
        if (it != thread->MyBlocksVariablen.end()) {
            thread->MyBlocksVariablen[argName] = std::move(it->second);

            if (argName != argId)
                thread->MyBlocksVariablen.erase(argId);
        } else {
            thread->MyBlocksVariablen[argName] = (i < block->argumentDefaults.size())
                                                     ? block->argumentDefaults[i]
                                                     : Value("");
        }
    }
    return BlockResult::CONTINUE_IMIDIATELY;
}

BlockResult block_procedures_return_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue);
static uint8_t block_procedures_return_reg_ =
    (BlockExecutor::getHandlers()["procedures_return"] = block_procedures_return_, 0);
BlockResult block_procedures_return_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue) {
    Value returnVal;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, returnVal))
        return BlockResult::REPEAT;

    thread->returnValue = returnVal;
    thread->finished = true;
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(argument, reporter_string_number) {
    std::string name = Scratch::getFieldValue(*block, "VALUE");
    auto it = thread->MyBlocksVariablen.find(name);
    if (outValue)
        *outValue = (it != thread->MyBlocksVariablen.end()) ? it->second : Value("");
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(argument, reporter_boolean) {
    std::string name = Scratch::getFieldValue(*block, "VALUE");
    auto it = thread->MyBlocksVariablen.find(name);
    if (outValue)
        *outValue = (it != thread->MyBlocksVariablen.end()) ? it->second : Value(false);
    return BlockResult::CONTINUE;
}
