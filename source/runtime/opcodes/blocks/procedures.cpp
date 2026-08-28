#include "../../core/vm_types.hpp"
#include "../entity_manager.hpp"
#include "../opcode_registers.hpp"

DEFINE_CUSTOM_PARSER("procedures_call", procCall) {
    const auto &mutation = ctx.blocksJson[ctx.currentBlock]["mutation"];
    std::string proccode = mutation["proccode"].get<std::string>();

    BytecodeChunk chunk;
    uint16_t argCount = 0;

    if (mutation.contains("argumentids")) {
        auto argIds = nlohmann::json::parse(mutation["argumentids"].get<std::string>());
        for (const auto &argIdJson : argIds) {
            std::string argId = argIdJson.get<std::string>();
            CompileResult arg = ctx.compileInput(argId);
            arg.isConstant ? chunk.emitPushConstant(arg.constantValue)
                           : chunk.append(std::move(arg.chunk));
            ++argCount;
        }
    }

    bool warp = false;
    if (mutation.contains("warp")) {
        const auto &warpVal = mutation["warp"];
        if (warpVal.is_boolean()) warp = warpVal.get<bool>();
        else if (warpVal.is_string()) warp = (warpVal.get<std::string>() == "true");
    }

    chunk.emitOpcode(warp ? static_cast<uint16_t>(Opcode::CALL_PROCEDURE_WOS)
                          : static_cast<uint16_t>(Opcode::CALL_PROCEDURE));

    chunk.emit16(argCount);

    size_t patchOffset = chunk.code.size();
    chunk.unresolvedProcedureCalls.push_back({proccode, patchOffset});
    chunk.emit32(0x00000000);

    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_EXECUTION(CALL_PROCEDURE) {
    auto &bc = thread->definition->bytecode;

    uint16_t argCount = bc[thread->pc++];

    uint32_t targetAddress = static_cast<uint32_t>(bc[thread->pc]) |
                             (static_cast<uint32_t>(bc[thread->pc + 1]) << 16);
    thread->pc += 2;

    if (thread->callStack.size() >= MAX_CALL_DEPTH)
        return BlockResult::CONTINUE;

    CallFrame &c = thread->callStack.emplace_back();
    c.returnPC = thread->pc;
    c.stackBase = static_cast<uint32_t>(thread->stack.size());
    c.argCount = argCount;
    c.previousWarpState = thread->isWarp;

    thread->pc = targetAddress;
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(CALL_PROCEDURE_WOS) {
    auto &bc = thread->definition->bytecode;

    uint16_t argCount = bc[thread->pc++];

    uint32_t targetAddress = static_cast<uint32_t>(bc[thread->pc]) |
                             (static_cast<uint32_t>(bc[thread->pc + 1]) << 16);
    thread->pc += 2;

    if (thread->callStack.size() >= MAX_CALL_DEPTH)
        return BlockResult::CONTINUE;

    CallFrame &c = thread->callStack.emplace_back();
    c.returnPC = thread->pc;
    c.stackBase = static_cast<uint32_t>(thread->stack.size());
    c.argCount = argCount;
    c.previousWarpState = thread->isWarp;

    thread->isWarp = true;
    thread->pc = targetAddress;
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(RETURN_WITH_VALUE) {
    CallFrame f = thread->callStack.back();
    thread->callStack.pop_back();
    Value result = thread->stack.back();

    thread->stack.resize(f.stackBase - f.argCount);

    thread->stack.push_back(result);
    thread->pc = f.returnPC;
    return BlockResult::CONTINUE;
}

DEFINE_EXECUTION(PUSH_PROC_ARG) {
    auto &bc = thread->definition->bytecode;
    uint16_t index = bc[thread->pc++];

    if (thread->callStack.empty()) {
        thread->stack.push_back(Value());
        return BlockResult::CONTINUE;
    }

    const CallFrame &frame = thread->callStack.back();
    uint32_t slotIndex = frame.stackBase - frame.argCount + index;

    if (slotIndex >= frame.stackBase || index >= frame.argCount) {
        thread->stack.push_back(Value());
        return BlockResult::CONTINUE;
    }

    thread->stack.push_back(thread->stack[slotIndex]);
    return BlockResult::CONTINUE;
}

static CompileResult compileArgReporter(CompilerContext &ctx) {
    const auto &block = ctx.blocksJson[ctx.currentBlock];

    std::string argName;
    if (block.contains("fields") && block["fields"].contains("VALUE") &&
        block["fields"]["VALUE"].is_array() && !block["fields"]["VALUE"].empty()) {
        argName = block["fields"]["VALUE"][0].get<std::string>();
    } else {
        return CompileResult::Constant(Value());
    }

    const ProcedureCompileInfo *info = ctx.getCurrentProcedureInfo();
    if (!info) return CompileResult::Constant(Value());

    uint16_t index = 0;
    bool found = false;

    for (uint16_t i = 0; i < static_cast<uint16_t>(info->argumentNamesInOrder.size()); ++i) {
        if (info->argumentNamesInOrder[i] == argName) {
            index = i;
            found = true;
            break;
        }
    }

    if (!found) {
        for (uint16_t i = 0; i < static_cast<uint16_t>(info->argumentIdsInOrder.size()); ++i) {
            const std::string &argId = info->argumentIdsInOrder[i];
            for (const auto &[bId, bData] : ctx.blocksJson.items()) {
                if (!bData.is_object() || !bData.contains("opcode")) continue;
                if (bData["opcode"].get<std::string>() != "procedures_prototype") continue;
                if (!bData.contains("inputs") || !bData["inputs"].contains(argId)) continue;

                const auto &inputData = bData["inputs"][argId];
                if (inputData.is_array() && inputData.size() >= 2 && inputData[1].is_string()) {
                    std::string shadowId = inputData[1].get<std::string>();
                    if (ctx.blocksJson.contains(shadowId)) {
                        const auto &shadowBlock = ctx.blocksJson[shadowId];
                        if (shadowBlock.contains("fields") && shadowBlock["fields"].contains("VALUE") &&
                            shadowBlock["fields"]["VALUE"].is_array() && !shadowBlock["fields"]["VALUE"].empty()) {
                            if (shadowBlock["fields"]["VALUE"][0].get<std::string>() == argName) {
                                index = i;
                                found = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (found) break;
        }
    }

    if (!found) return CompileResult::Constant(Value());

    BytecodeChunk chunk;
    chunk.emitOpcode(static_cast<uint16_t>(Opcode::PUSH_PROC_ARG));
    chunk.emit16(index);
    return CompileResult::Dynamic(std::move(chunk));
}

DEFINE_CUSTOM_PARSER("argument_reporter_string_number", argReporterStringNum) {
    return compileArgReporter(ctx);
}

DEFINE_CUSTOM_PARSER("argument_reporter_boolean", argReporterBoolean) {
    return compileArgReporter(ctx);
}