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
    Log::log("ParserRegistry::compileStandard - Starting to compile standard opcode " + std::to_string(opcode));
    for (const auto &inputName : inputs) {
        Log::log("ParserRegistry::compileStandard - Compiling input " + inputName);
        CompileResult res = ctx.compileInput(inputName);
        if (!res.isConstant) {
            Log::log("ParserRegistry::compileStandard - Input " + inputName + " is not constant");
            allConstant = false;
        } else {
            Log::log("ParserRegistry::compileStandard - Input " + inputName + " is constant: " + res.constantValue.asString());
        }
        results.push_back(std::move(res));
    }

    if (allConstant) {
        Log::log("ParserRegistry::compileStandard - All inputs are constant");
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
            Log::log("ParserRegistry::compileStandard - Emitting push constant");
            combinedChunk.emitPushConstant(res.constantValue);
        } else {
            Log::log("ParserRegistry::compileStandard - Emitting chunk");
            combinedChunk.append(std::move(res.chunk));
        }
    }

    combinedChunk.emitOpcode(opcode);
    Log::log("ParserRegistry::compileStandard - Emitting opcode " + std::to_string(opcode));
    return CompileResult::Dynamic(std::move(combinedChunk));
}

bool OpcodeRegistry::registerBlock(uint16_t opcode, BlockHandler handler) {
    getJumpTable()[opcode] = handler;
    return true;
}

BlockResult OpcodeRegistry::executeByteCode(VMThread *thread) {
    uint32_t currentPc = thread->pc;
    uint16_t opcode = EntityManager::blueprints[thread->defId].bytecode[currentPc];
    thread->pc++;
    BlockResult r = getJumpTable()[opcode](thread);
    if (r == BlockResult::YIELD_SAME) thread->pc = currentPc;
    return r;
}