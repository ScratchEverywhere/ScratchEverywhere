#include "blockExecutor.hpp"
#include "parser.hpp"
#include "runtime.hpp"
#include "types.hpp"
#include <algorithm>
#include <memory>

namespace {

BytecodeStackKind stackKindForType(Type type) {
    switch (type) {
    case Type::Number:
        return BytecodeStackKind::Number;
    case Type::String:
        return BytecodeStackKind::String;
    case Type::Boolean:
        return BytecodeStackKind::Boolean;
    case Type::Color:
        return BytecodeStackKind::Color;
    case Type::Value:
    default:
        return BytecodeStackKind::Value;
    }
}

struct BytecodeCompilerState {
    Sprite *sprite = nullptr;
    BytecodeChunk chunk;
    BytecodeLinkTable link;

    std::unordered_map<std::string, BytecodeIndex> opcodeSlotMap;
    std::unordered_map<std::string, BytecodeIndex> variableSlotMap;
    std::unordered_map<std::string, BytecodeIndex> listSlotMap;
    std::unordered_map<Block *, BytecodeIndex> escapeBlockMap;

    const std::vector<std::string> *currentArgumentNames = nullptr;

    BytecodeIndex emit(BytecodeOp op, BytecodeIndex a = invalidBytecodeIndex, BytecodeIndex b = invalidBytecodeIndex) {
        chunk.instructions.push_back(BytecodeInstruction{op, a, b});
        return static_cast<BytecodeIndex>(chunk.instructions.size() - 1);
    }

    BytecodeIndex opcodeSlot(const std::string &opcode, const std::vector<BytecodeInputSignature> &signature) {
        auto it = opcodeSlotMap.find(opcode);
        if (it != opcodeSlotMap.end()) return it->second;

        BytecodeIndex slot = static_cast<BytecodeIndex>(chunk.opcodeSlots.size());
        chunk.opcodeSlots.push_back(chunk.internString(opcode));

        auto &handlers = BlockExecutor::getHandlers();
        auto handlerIt = handlers.find(opcode);
        link.opcodes.push_back(handlerIt != handlers.end() ? &handlerIt->second : nullptr);

        auto &positional = BlockExecutor::getPositionalHandlers();
        auto positionalIt = positional.find(opcode);
        link.positional.push_back(positionalIt != positional.end() ? positionalIt->second : BytecodePositionalFn{});

        chunk.paramKindsStart.push_back(static_cast<BytecodeIndex>(chunk.paramKinds.size()));
        for (const auto &sig : signature)
            chunk.paramKinds.push_back(sig.kind);
        chunk.paramKindsCount.push_back(static_cast<BytecodeIndex>(signature.size()));

        opcodeSlotMap[opcode] = slot;
        return slot;
    }

    BytecodeIndex variableSlot(const std::string &variableId) {
        auto it = variableSlotMap.find(variableId);
        if (it != variableSlotMap.end()) return it->second;

        BytecodeIndex slot = static_cast<BytecodeIndex>(chunk.variableSlots.size());
        chunk.variableSlots.push_back(chunk.internString(variableId));
        link.variables.push_back(BlockExecutor::getVariable(variableId, sprite));
        variableSlotMap[variableId] = slot;
        return slot;
    }

    BytecodeIndex listSlot(const std::string &listId) {
        auto it = listSlotMap.find(listId);
        if (it != listSlotMap.end()) return it->second;

        BytecodeIndex slot = static_cast<BytecodeIndex>(chunk.listSlots.size());
        chunk.listSlots.push_back(chunk.internString(listId));

        List *list = nullptr;
        auto listIt = sprite->lists.find(listId);
        if (listIt != sprite->lists.end()) {
            list = &listIt->second;
        } else if (Scratch::stageSprite != nullptr) {
            auto globalIt = Scratch::stageSprite->lists.find(listId);
            if (globalIt != Scratch::stageSprite->lists.end()) list = &globalIt->second;
        }
        link.lists.push_back(list);
        listSlotMap[listId] = slot;
        return slot;
    }

    BytecodeIndex escapeSlot(Block *block) {
        auto it = escapeBlockMap.find(block);
        if (it != escapeBlockMap.end()) return it->second;

        BytecodeIndex slot = static_cast<BytecodeIndex>(link.escapeBlocks.size());
        link.escapeBlocks.push_back(block);
        escapeBlockMap[block] = slot;
        return slot;
    }
};

void convertIfNeeded(BytecodeCompilerState &cs, BytecodeStackKind from, BytecodeStackKind to) {
    if (from == to) return;

    if (from != BytecodeStackKind::Value) {
        switch (from) {
        case BytecodeStackKind::Number:
            cs.emit(BytecodeOp::ConvertNumberToValue);
            break;
        case BytecodeStackKind::String:
            cs.emit(BytecodeOp::ConvertStringToValue);
            break;
        case BytecodeStackKind::Boolean:
            cs.emit(BytecodeOp::ConvertBooleanToValue);
            break;
        case BytecodeStackKind::Color:
            cs.emit(BytecodeOp::ConvertColorToValue);
            break;
        default:
            break;
        }
    }

    if (to != BytecodeStackKind::Value) {
        switch (to) {
        case BytecodeStackKind::Number:
            cs.emit(BytecodeOp::ConvertValueToNumber);
            break;
        case BytecodeStackKind::String:
            cs.emit(BytecodeOp::ConvertValueToString);
            break;
        case BytecodeStackKind::Boolean:
            cs.emit(BytecodeOp::ConvertValueToBoolean);
            break;
        case BytecodeStackKind::Color:
            cs.emit(BytecodeOp::ConvertValueToColor);
            break;
        default:
            break;
        }
    }
}

void compileConstant(BytecodeCompilerState &cs, const Value &value, BytecodeStackKind wanted) {
    switch (wanted) {
    case BytecodeStackKind::Number: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(cs.chunk.numberConstants.size());
        cs.chunk.numberConstants.push_back(value.get<double>());
        cs.emit(BytecodeOp::PushConstNumber, idx);
        return;
    }
    case BytecodeStackKind::String: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(cs.chunk.stringConstants.size());
        cs.chunk.stringConstants.push_back(value.get<std::string>());
        cs.emit(BytecodeOp::PushConstString, idx);
        return;
    }
    case BytecodeStackKind::Boolean: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(cs.chunk.boolConstants.size());
        cs.chunk.boolConstants.push_back(value.get<bool>());
        cs.emit(BytecodeOp::PushConstBoolean, idx);
        return;
    }
    case BytecodeStackKind::Color: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(cs.chunk.colorConstants.size());
        cs.chunk.colorConstants.push_back(value.asColor());
        cs.emit(BytecodeOp::PushConstColor, idx);
        return;
    }
    case BytecodeStackKind::Value: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(cs.chunk.constants.size());
        cs.chunk.constants.push_back(value);
        cs.emit(BytecodeOp::PushConstValue, idx);
        return;
    }
    }
}

void compileDefaultConst(BytecodeCompilerState &cs, BytecodeStackKind wanted) {
    switch (wanted) {
    case BytecodeStackKind::Number:
        compileConstant(cs, Value(0.0), wanted);
        return;
    case BytecodeStackKind::String:
        compileConstant(cs, Value(std::string()), wanted);
        return;
    case BytecodeStackKind::Boolean:
        compileConstant(cs, Value(false), wanted);
        return;
    case BytecodeStackKind::Color:
        compileConstant(cs, Value(Color{}), wanted);
        return;
    case BytecodeStackKind::Value:
        compileConstant(cs, Value(0.0), wanted);
        return;
    }
}

void emitPushVar(BytecodeCompilerState &cs, BytecodeIndex slot, BytecodeStackKind wanted) {
    switch (wanted) {
    case BytecodeStackKind::Number:
        cs.emit(BytecodeOp::PushVarNumber, slot);
        return;
    case BytecodeStackKind::String:
        cs.emit(BytecodeOp::PushVarString, slot);
        return;
    case BytecodeStackKind::Boolean:
        cs.emit(BytecodeOp::PushVarBoolean, slot);
        return;
    case BytecodeStackKind::Color:
        cs.emit(BytecodeOp::PushVarColor, slot);
        return;
    case BytecodeStackKind::Value:
        cs.emit(BytecodeOp::PushVarValue, slot);
        return;
    }
}

void emitCallNative(BytecodeCompilerState &cs, BytecodeIndex opSlot, BytecodeIndex argCount, BytecodeStackKind returnKind) {
    switch (returnKind) {
    case BytecodeStackKind::Number:
        cs.emit(BytecodeOp::CallNativeNumber, opSlot, argCount);
        return;
    case BytecodeStackKind::String:
        cs.emit(BytecodeOp::CallNativeString, opSlot, argCount);
        return;
    case BytecodeStackKind::Boolean:
        cs.emit(BytecodeOp::CallNativeBoolean, opSlot, argCount);
        return;
    case BytecodeStackKind::Color:
        cs.emit(BytecodeOp::CallNativeColor, opSlot, argCount);
        return;
    case BytecodeStackKind::Value:
        cs.emit(BytecodeOp::CallNativeValue, opSlot, argCount);
        return;
    }
}

void compileReporter(BytecodeCompilerState &cs, Block *block, BytecodeStackKind wanted);

void compileOperand(BytecodeCompilerState &cs, const ParsedInput *input, BytecodeStackKind wanted) {
    if (input == nullptr) {
        compileDefaultConst(cs, wanted);
        return;
    }

    switch (input->inputType) {
    case ParsedInput::VALUE:
        compileConstant(cs, input->value, wanted);
        return;

    case ParsedInput::VARIABLE:
        if (input->list) {
            BytecodeIndex slot = cs.listSlot(input->variableId);
            cs.emit(BytecodeOp::PushList, slot);
            convertIfNeeded(cs, BytecodeStackKind::Value, wanted);
        } else {
            emitPushVar(cs, cs.variableSlot(input->variableId), wanted);
        }
        return;

    case ParsedInput::BLOCK:
        if (input->block == nullptr) {
            compileDefaultConst(cs, wanted);
            return;
        }
        compileReporter(cs, input->block, wanted);
        return;
    }
}

bool tryCompileArgumentReporter(BytecodeCompilerState &cs, Block *block, BytecodeStackKind wanted) {
    if (cs.currentArgumentNames == nullptr) return false;
    if (block->opcode != "argument_reporter_string_number" && block->opcode != "argument_reporter_boolean") return false;

    std::string argName = Scratch::getFieldValue(*block, "VALUE");
    const auto &names = *cs.currentArgumentNames;
    auto it = std::find(names.begin(), names.end(), argName);
    if (it == names.end()) return false;

    BytecodeIndex argSlot = static_cast<BytecodeIndex>(std::distance(names.begin(), it));
    cs.emit(BytecodeOp::PushArg, argSlot);
    convertIfNeeded(cs, BytecodeStackKind::Value, wanted);
    return true;
}

void compileReporter(BytecodeCompilerState &cs, Block *block, BytecodeStackKind wanted) {
    if (tryCompileArgumentReporter(cs, block, wanted)) return;

    const std::string &opcode = block->opcode;
    const auto &signatures = BlockExecutor::getInputSignatures();
    auto sigIt = signatures.find(opcode);

    if (sigIt == signatures.end()) {
        BytecodeIndex slot = cs.escapeSlot(block);
        cs.emit(BytecodeOp::CallLegacyReporter, slot);
        convertIfNeeded(cs, BytecodeStackKind::Value, wanted);
        return;
    }

    for (const auto &sig : sigIt->second) {
        compileOperand(cs, Scratch::getInput(block, sig.name), sig.kind);
    }

    BytecodeIndex opSlot = cs.opcodeSlot(opcode, sigIt->second);
    BytecodeStackKind nativeKind = stackKindForType(block->blockFunction.type);
    emitCallNative(cs, opSlot, static_cast<BytecodeIndex>(sigIt->second.size()), nativeKind);

    convertIfNeeded(cs, nativeKind, wanted);
}

bool tryCompileVariableStatement(BytecodeCompilerState &cs, Block *block, BytecodeIndex &outFirstInstr) {
    if (block->opcode != "data_setvariableto" && block->opcode != "data_changevariableby") return false;

    const std::string &varId = Scratch::getFieldId(*block, "VARIABLE");
    Variable *var = BlockExecutor::getVariable(varId, cs.sprite);
    if (var == nullptr) return false;

    BytecodeStackKind varKind;
    if (std::holds_alternative<double>(var->value)) varKind = BytecodeStackKind::Number;
    else if (std::holds_alternative<std::shared_ptr<const std::string>>(var->value)) varKind = BytecodeStackKind::String;
    else if (std::holds_alternative<bool>(var->value)) varKind = BytecodeStackKind::Boolean;
    else varKind = BytecodeStackKind::Value;

    BytecodeIndex firstInstr = static_cast<BytecodeIndex>(cs.chunk.instructions.size());
    BytecodeIndex slot = cs.variableSlot(varId);

    if (block->opcode == "data_changevariableby") {
        if (varKind != BytecodeStackKind::Number) return false;

        compileOperand(cs, Scratch::getInput(block, "VALUE"), BytecodeStackKind::Number);
        cs.emit(BytecodeOp::ChangeVarNumber, slot);
        outFirstInstr = firstInstr;
        return true;
    }

    compileOperand(cs, Scratch::getInput(block, "VALUE"), varKind);
    switch (varKind) {
    case BytecodeStackKind::Number:
        cs.emit(BytecodeOp::StoreVarNumber, slot);
        break;
    case BytecodeStackKind::String:
        cs.emit(BytecodeOp::StoreVarString, slot);
        break;
    case BytecodeStackKind::Boolean:
        cs.emit(BytecodeOp::StoreVarBoolean, slot);
        break;
    case BytecodeStackKind::Color:
        cs.emit(BytecodeOp::StoreVarColor, slot);
        break;
    case BytecodeStackKind::Value:
        cs.emit(BytecodeOp::StoreVarValue, slot);
        break;
    }
    outFirstInstr = firstInstr;
    return true;
}

BytecodeIndex compileStatementChain(BytecodeCompilerState &cs, Block *start) {
    if (start == nullptr) {
        return cs.emit(BytecodeOp::Halt);
    }

    const auto &signatures = BlockExecutor::getInputSignatures();
    BytecodeIndex firstInstruction = invalidBytecodeIndex;
    Block *block = start;

    while (block != nullptr) {
        BytecodeIndex varInstr;
        if (tryCompileVariableStatement(cs, block, varInstr)) {
            if (firstInstruction == invalidBytecodeIndex) firstInstruction = varInstr;
            block = block->nextBlock;
            continue;
        }

        auto sigIt = signatures.find(block->opcode);
        if (sigIt == signatures.end()) {
            BytecodeIndex slot = cs.escapeSlot(block);
            BytecodeIndex idx = cs.emit(BytecodeOp::EscapeToLegacyThread, slot);
            if (firstInstruction == invalidBytecodeIndex) firstInstruction = idx;
            return firstInstruction;
        }

        for (const auto &sig : sigIt->second) {
            compileOperand(cs, Scratch::getInput(block, sig.name), sig.kind);
        }

        BytecodeIndex opSlot = cs.opcodeSlot(block->opcode, sigIt->second);
        BytecodeIndex instrIdx = cs.emit(BytecodeOp::CallNative, opSlot, static_cast<BytecodeIndex>(sigIt->second.size()));

        if (firstInstruction == invalidBytecodeIndex) firstInstruction = instrIdx;
        block = block->nextBlock;
    }

    cs.emit(BytecodeOp::Halt);
    return firstInstruction;
}

} // namespace

void Parser::compileBytecode(Sprite *sprite) {
    BytecodeCompilerState cs;
    cs.sprite = sprite;

    for (const auto &[opcode, hatBlocks] : sprite->hats) {
        for (Block *hat : hatBlocks) {
            if (hat == nullptr) continue;

            BytecodeScript script;
            script.opcodeIndex = cs.chunk.internString(opcode);
            script.entryInstruction = compileStatementChain(cs, hat);
            cs.chunk.scripts.push_back(script);
        }
    }

    for (const auto &[proccode, definitionBlock] : sprite->customHatBlock) {
        if (definitionBlock == nullptr) continue;

        BytecodeProcedure proc;
        proc.procCodeIndex = cs.chunk.internString(proccode);
        proc.argCount = static_cast<BytecodeIndex>(definitionBlock->argumentIDs.size());
        proc.runWithoutScreenRefresh = definitionBlock->MyBlockWithoutScreenRefresh;

        proc.argumentNamesStart = static_cast<BytecodeIndex>(cs.chunk.strings.size());
        for (size_t i = 0; i < definitionBlock->argumentIDs.size(); i++) {
            const std::string &name = (i < definitionBlock->argumentNames.size())
                                          ? definitionBlock->argumentNames[i]
                                          : definitionBlock->argumentIDs[i];
            cs.chunk.strings.push_back(name);
        }
        proc.argumentNamesCount = static_cast<BytecodeIndex>(definitionBlock->argumentIDs.size());

        cs.currentArgumentNames = &definitionBlock->argumentNames;
        proc.entryInstruction = compileStatementChain(cs, definitionBlock->nextBlock);
        cs.currentArgumentNames = nullptr;

        cs.chunk.procedures.push_back(proc);
    }

    sprite->bytecode = std::make_unique<BytecodeChunk>(std::move(cs.chunk));
    sprite->bytecodeLink = std::make_unique<BytecodeLinkTable>(std::move(cs.link));
}
