#include "blockUtils.hpp"
#include <types.hpp>
#include <value.hpp>

SCRATCH_BLOCK(procedures, call) {
    BlockState *state = thread->getState(block);

    if (state->completedSteps == -2) {
    executeBlock:
        BlockResult result = BlockExecutor::runThread(*state->myBlockThread, *sprite);
        if (result == BlockResult::RETURN || state->myBlockThread->finished) {
            if (outValue) *outValue = state->myBlockThread->returnValue;

            state->myBlockThread->clear();

            // delete instead of putting into thread pool, since otherwise it would just grow each time a call is made
            delete state->myBlockThread;
            state->myBlockThread = nullptr;

            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        return BlockResult::REPEAT;
    }

    if (state->completedSteps == 0) {
        if (block->MyBlockDefinitionID == nullptr) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        const bool isRecursive = thread->isRecursiveProcedureCall(block->MyBlockDefinitionID);

        ScriptThread *newThread;
        if (!Pools::threads.empty()) {
            newThread = Pools::threads.back();
            Pools::threads.pop_back();
        } else {
            newThread = new ScriptThread();
        }
        newThread->blockHat = block->MyBlockDefinitionID;
        newThread->nextBlock = block->MyBlockDefinitionID;
        newThread->parentThread = thread;
        newThread->withoutScreenRefresh = !thread->withoutScreenRefresh ? block->MyBlockWithoutScreenRefresh : true;
        newThread->finished = false;
        newThread->returnValue = Value();
        newThread->MyBlocksVariablen.clear();

        newThread->callStack = thread->callStack;
        newThread->callStack.push_back(block->MyBlockDefinitionID);

        state->myBlockThread = newThread;
        state->completedSteps = 1;
        Scratch::resetInput(block);

        if (isRecursive && !thread->withoutScreenRefresh && !block->MyBlockWithoutScreenRefresh) {
            return BlockResult::REPEAT;
        }
    }

    while ((size_t)(state->completedSteps - 1) < block->argumentIDs.size()) {
        int argIdx = state->completedSteps - 1;
        const std::string &argId = block->argumentIDs[argIdx];

        Value argVal;
        if (block->inputMap.find(argId) == block->inputMap.end() &&
            block->MyBlockDefinitionID != nullptr &&
            (size_t)argIdx < block->MyBlockDefinitionID->argumentDefaults.size()) {
            argVal = block->MyBlockDefinitionID->argumentDefaults[argIdx];
        } else if (!Scratch::getInputValue(block, argId, thread, sprite, argVal)) {
            return BlockResult::REPEAT;
        }
        state->myBlockThread->MyBlocksVariablen[argId] = argVal;
        state->completedSteps++;
    }

    state->completedSteps = -2;
    goto executeBlock;
    return BlockResult::REPEAT;
}

SCRATCH_BYTECODE_BLOCK(procedures, call) {
    outFirstInstr = compiler.here();

    if (!block->bytecodeCallOpcode.empty()) {
        auto &sigMap = BlockExecutor::getInputSignatures();
        auto sigIt = sigMap.find(block->bytecodeCallOpcode);
        if (sigIt != sigMap.end()) {
            for (const auto &s : sigIt->second)
                compiler.compileInput(block, s.name, s.kind);
            BytecodeIndex opSlot = compiler.opcodeSlot(block->bytecodeCallOpcode, sigIt->second);
            compiler.emit(BytecodeOp::CallNative, opSlot,
                          static_cast<BytecodeIndex>(sigIt->second.size()));
        }
        return true;
    }

    if (block->MyBlockDefinitionID == nullptr) return true;
    BytecodeIndex procSlot = compiler.procedureSlot(block->MyBlockDefinitionID);
    if (procSlot == invalidBytecodeIndex) return true;

    for (size_t i = 0; i < block->argumentIDs.size(); i++) {
        const std::string &argId = block->argumentIDs[i];
        const ParsedInput *input = Scratch::getInput(block, argId);

        if (input == nullptr) {
            Value defaultVal;
            if (i < block->MyBlockDefinitionID->argumentDefaults.size())
                defaultVal = block->MyBlockDefinitionID->argumentDefaults[i];

            BytecodeIndex idx = static_cast<BytecodeIndex>(compiler.chunk().constants.size());
            compiler.chunk().constants.push_back(defaultVal);
            compiler.emit(BytecodeOp::PushConstValue, idx);
        } else {
            compiler.compileOperand(input, BytecodeStackKind::Value);
        }
    }

    BytecodeIndex argCount = static_cast<BytecodeIndex>(block->argumentIDs.size());
    compiler.emit(compiler.inStatementContext() ? BytecodeOp::CallProcedure : BytecodeOp::CallProcedureValue, procSlot, argCount);
    return true;
}

SCRATCH_BLOCK(procedures, prototype) {
    for (size_t i = 0; i < block->argumentIDs.size(); i++) {
        const std::string &argId = block->argumentIDs[i];
        const std::string &argName = (i < block->argumentNames.size())
                                         ? block->argumentNames[i]
                                         : argId;

        auto it = thread->MyBlocksVariablen.find(argId);
        if (it != thread->MyBlocksVariablen.end()) {
            if (argName != argId) {
                Value renamedValue = std::move(it->second);
                thread->MyBlocksVariablen.erase(argId);
                thread->MyBlocksVariablen[argName] = std::move(renamedValue);
            }
        } else {
            thread->MyBlocksVariablen[argName] = (i < block->argumentDefaults.size())
                                                     ? block->argumentDefaults[i]
                                                     : Value();
        }
    }
    return BlockResult::CONTINUE_IMMEDIATELY;
}

BlockResult block_procedures_return_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue);
static uint8_t block_procedures_return_reg_ =
    (BlockExecutor::getHandlers()["procedures_return"] = BlockFunc(block_procedures_return_), 0);
BlockResult block_procedures_return_(Block *block, ScriptThread *thread, Sprite *sprite, Value *outValue) {
    Value returnVal;
    if (!Scratch::getInputValue(block, "VALUE", thread, sprite, returnVal))
        return BlockResult::REPEAT;

    thread->returnValue = returnVal;
    thread->finished = true;
    return BlockResult::RETURN;
}
SCRATCH_BYTECODE_BLOCK(procedures, return) {
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "VALUE", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::Return);
    return true;
}

SCRATCH_BLOCK(argument, reporter_string_number) {
    std::string name = Scratch::getFieldValue(*block, "VALUE");
    auto it = thread->MyBlocksVariablen.find(name);
    if (outValue)
        *outValue = (it != thread->MyBlocksVariablen.end()) ? it->second : Value(0);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(argument, reporter_boolean) {
    std::string name = Scratch::getFieldValue(*block, "VALUE");
    auto it = thread->MyBlocksVariablen.find(name);
    if (outValue)
        *outValue = (it != thread->MyBlocksVariablen.end()) ? it->second : Value(0);
    return BlockResult::CONTINUE;
}
