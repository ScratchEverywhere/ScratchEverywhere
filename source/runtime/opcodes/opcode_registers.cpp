#include "opcode_registers.hpp"

#include "../compiler/compiler_context.hpp"
#include "../core/vm_types.hpp"
#include "../entity_manager.hpp"

bool ParserRegistry::registerParser(const std::string &scratchName, ParserHandler handler) {
    getParserMap()[scratchName] = handler;
    return true;
}

CompileResult ParserRegistry::compileStandard(CompilerContext &ctx, uint16_t opcode, const std::vector<std::string> &inputs, Purity purity) {
    std::vector<CompileResult> results;
    bool allConstant = (purity == Purity::Pure);

    for (const auto &inputName : inputs) {
        CompileResult res = ctx.compileInput(inputName);
        if (!res.isConstant) {
            allConstant = false;
        }
        results.push_back(std::move(res));
    }

    if (allConstant) {
        VMThread dummyThread;
        for (const auto &input : results) {
            dummyThread.stack.push_back(input.constantValue);
        }
        BlockResult result = OpcodeRegistry::getJumpTable()[opcode](&dummyThread);
        if (result != BlockResult::YIELD_SAME) {
            Value foldedResult = dummyThread.stack.back();
            return CompileResult::Constant(foldedResult);
        }
    }

    BytecodeChunk combinedChunk;

    for (CompileResult &res : results) {
        if (res.isConstant) {
            combinedChunk.emitPushConstant(res.constantValue);
        } else {
            combinedChunk.append(std::move(res.chunk));
        }
    }

    combinedChunk.emitOpcode(opcode);
    return CompileResult::Dynamic(std::move(combinedChunk));
}

bool OpcodeRegistry::registerBlock(uint16_t opcode, BlockHandler handler) {
    getJumpTable()[opcode] = handler;
    return true;
}

BlockResult OpcodeRegistry::executeByteCode(VMThread *thread) {
    uint32_t currentPc = thread->pc;
    Log::log("Execute " + std::to_string(thread->pc));
    uint16_t opcode = EntityManager::blueprints[thread->defId].bytecode[currentPc];
    thread->pc++;
    BlockResult r = getJumpTable()[opcode](thread);
    if (r == BlockResult::YIELD_SAME) thread->pc = currentPc;
    return r;
}