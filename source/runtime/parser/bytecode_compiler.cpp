#include "bytecode_compiler.hpp"
#include "blockExecutor.hpp"
#include "parser.hpp"
#include "runtime.hpp"
#include "types.hpp"
#include <algorithm>
#include <memory>

std::unordered_map<std::string, BytecodeCompiler::CustomCompileFn> &BytecodeCompiler::getCustomCompilers() {
    static std::unordered_map<std::string, CustomCompileFn> compilers;
    return compilers;
}

void BytecodeCompiler::convertIfNeeded(BytecodeStackKind from, BytecodeStackKind to) {
    if (from == to) return;

    if (from != BytecodeStackKind::Value) {
        switch (from) {
        case BytecodeStackKind::Number:
            emit(BytecodeOp::ConvertNumberToValue);
            break;
        case BytecodeStackKind::String:
            emit(BytecodeOp::ConvertStringToValue);
            break;
        case BytecodeStackKind::Boolean:
            emit(BytecodeOp::ConvertBooleanToValue);
            break;
        case BytecodeStackKind::Color:
            emit(BytecodeOp::ConvertColorToValue);
            break;
        default:
            break;
        }
    }

    if (to != BytecodeStackKind::Value) {
        switch (to) {
        case BytecodeStackKind::Number:
            emit(BytecodeOp::ConvertValueToNumber);
            break;
        case BytecodeStackKind::String:
            emit(BytecodeOp::ConvertValueToString);
            break;
        case BytecodeStackKind::Boolean:
            emit(BytecodeOp::ConvertValueToBoolean);
            break;
        case BytecodeStackKind::Color:
            emit(BytecodeOp::ConvertValueToColor);
            break;
        default:
            break;
        }
    }
}

bool BytecodeCompiler::isStaticallyKind(const ParsedInput *input, BytecodeStackKind kind) {
    if (input == nullptr) return false;

    switch (input->inputType) {
    case ParsedInput::VALUE:
        switch (kind) {
        case BytecodeStackKind::Number:
            return input->value.isDouble();
        case BytecodeStackKind::String:
            return input->value.isString();
        case BytecodeStackKind::Boolean:
            return input->value.isBoolean();
        case BytecodeStackKind::Color:
            return input->value.isColor();
        default:
            return false;
        }

    case ParsedInput::VARIABLE: {
        if (input->list) return false;
        Variable *var = BlockExecutor::getVariable(input->variableId, sprite());
        if (var == nullptr) return false;
        switch (kind) {
        case BytecodeStackKind::Number:
            return std::holds_alternative<double>(var->value);
        case BytecodeStackKind::String:
            return std::holds_alternative<std::shared_ptr<const std::string>>(var->value);
        case BytecodeStackKind::Boolean:
            return std::holds_alternative<bool>(var->value);
        default:
            return false;
        }
    }

    case ParsedInput::BLOCK:
        if (input->block == nullptr) return false;
        switch (kind) {
        case BytecodeStackKind::Number:
            return input->block->blockFunction.type == Type::Number;
        case BytecodeStackKind::String:
            return input->block->blockFunction.type == Type::String;
        case BytecodeStackKind::Boolean:
            return input->block->blockFunction.type == Type::Boolean;
        case BytecodeStackKind::Color:
            return input->block->blockFunction.type == Type::Color;
        default:
            return false;
        }
    }
    return false;
}

BytecodeIndex BytecodeCompiler::emit(BytecodeOp op, BytecodeIndex a, BytecodeIndex b) {
    chunk_.instructions.push_back(BytecodeInstruction{op, a, b});
    return static_cast<BytecodeIndex>(chunk_.instructions.size() - 1);
}

void BytecodeCompiler::patchJumpTarget(BytecodeIndex jumpInstr, BytecodeIndex target) {
    chunk_.instructions[jumpInstr].a = target;
}

BytecodeIndex BytecodeCompiler::opcodeSlot(const std::string &opcode, const std::vector<BytecodeInputSignature> &signature) {
    auto it = opcodeSlotMap_.find(opcode);
    if (it != opcodeSlotMap_.end()) return it->second;

    BytecodeIndex slot = static_cast<BytecodeIndex>(chunk_.opcodeSlots.size());
    chunk_.opcodeSlots.push_back(chunk_.internString(opcode));

    auto &handlers = BlockExecutor::getHandlers();
    auto handlerIt = handlers.find(opcode);
    link_.opcodes.push_back(handlerIt != handlers.end() ? &handlerIt->second : nullptr);

    auto &positional = BlockExecutor::getPositionalHandlers();
    auto positionalIt = positional.find(opcode);
    link_.positional.push_back(positionalIt != positional.end() ? positionalIt->second : BytecodePositionalFn{});

    chunk_.paramKindsStart.push_back(static_cast<BytecodeIndex>(chunk_.paramKinds.size()));
    for (const auto &sig : signature) {
        chunk_.paramKinds.push_back(sig.kind);
        chunk_.paramNames.push_back(chunk_.internString(sig.name));
    }
    chunk_.paramKindsCount.push_back(static_cast<BytecodeIndex>(signature.size()));

    opcodeSlotMap_[opcode] = slot;
    return slot;
}

BytecodeIndex BytecodeCompiler::variableSlot(const std::string &variableId) {
    auto it = variableSlotMap_.find(variableId);
    if (it != variableSlotMap_.end()) return it->second;

    BytecodeIndex slot = static_cast<BytecodeIndex>(chunk_.variableSlots.size());
    chunk_.variableSlots.push_back(chunk_.internString(variableId));
    link_.variables.push_back(BlockExecutor::getVariable(variableId, sprite_));
    variableSlotMap_[variableId] = slot;
    return slot;
}

BytecodeIndex BytecodeCompiler::listSlot(const std::string &listId) {
    auto it = listSlotMap_.find(listId);
    if (it != listSlotMap_.end()) return it->second;

    BytecodeIndex slot = static_cast<BytecodeIndex>(chunk_.listSlots.size());
    chunk_.listSlots.push_back(chunk_.internString(listId));

    List *list = nullptr;
    auto listIt = sprite_->lists.find(listId);
    if (listIt != sprite_->lists.end()) {
        list = &listIt->second;
    } else if (Scratch::stageSprite != nullptr) {
        auto globalIt = Scratch::stageSprite->lists.find(listId);
        if (globalIt != Scratch::stageSprite->lists.end()) list = &globalIt->second;
    }
    link_.lists.push_back(list);
    listSlotMap_[listId] = slot;
    return slot;
}

BytecodeIndex BytecodeCompiler::registerProcedure(Block *definitionBlock, const std::string &proccode) {
    auto it = procedureSlotMap_.find(definitionBlock);
    if (it != procedureSlotMap_.end()) return it->second;

    BytecodeIndex slot = static_cast<BytecodeIndex>(chunk_.procedures.size());

    BytecodeProcedure proc;
    proc.procCodeIndex = chunk_.internString(proccode);
    proc.argCount = static_cast<BytecodeIndex>(definitionBlock->argumentIDs.size());
    proc.runWithoutScreenRefresh = definitionBlock->MyBlockWithoutScreenRefresh;

    proc.argumentNamesStart = static_cast<BytecodeIndex>(chunk_.strings.size());
    for (size_t i = 0; i < definitionBlock->argumentIDs.size(); i++) {
        const std::string &name = (i < definitionBlock->argumentNames.size())
                                      ? definitionBlock->argumentNames[i]
                                      : definitionBlock->argumentIDs[i];
        chunk_.strings.push_back(name);
    }
    proc.argumentNamesCount = static_cast<BytecodeIndex>(definitionBlock->argumentIDs.size());

    chunk_.procedures.push_back(proc);
    procedureSlotMap_[definitionBlock] = slot;
    return slot;
}

BytecodeIndex BytecodeCompiler::procedureSlot(Block *definitionBlock) const {
    auto it = procedureSlotMap_.find(definitionBlock);
    return it != procedureSlotMap_.end() ? it->second : invalidBytecodeIndex;
}

BytecodeIndex BytecodeCompiler::escapeSlot(Block *block) {
    auto it = escapeBlockMap_.find(block);
    if (it != escapeBlockMap_.end()) return it->second;

    BytecodeIndex slot = static_cast<BytecodeIndex>(link_.escapeBlocks.size());
    link_.escapeBlocks.push_back(block);
    chunk_.escapeBlockOrdinals.push_back(block->bytecodeOrdinal);
    escapeBlockMap_[block] = slot;
    return slot;
}

Block *BytecodeCompiler::substackBlock(Block *block, const std::string &inputName) {
    const ParsedInput *input = Scratch::getInput(block, inputName);
    return (input != nullptr) ? input->block : nullptr;
}

bool BytecodeCompiler::compileSubstack(Block *block, const std::string &inputName, Block *stopBlock, BytecodeIndex &outFirstInstr) {
    return compileBlockChain(substackBlock(block, inputName), stopBlock, outFirstInstr);
}

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

void compileConstant(BytecodeCompiler &compiler, const Value &value, BytecodeStackKind wanted) {
    switch (wanted) {
    case BytecodeStackKind::Number: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(compiler.chunk().numberConstants.size());
        compiler.chunk().numberConstants.push_back(value.get<double>());
        compiler.emit(BytecodeOp::PushConstNumber, idx);
        return;
    }
    case BytecodeStackKind::String: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(compiler.chunk().stringConstants.size());
        compiler.chunk().stringConstants.push_back(value.get<std::string>());
        compiler.emit(BytecodeOp::PushConstString, idx);
        return;
    }
    case BytecodeStackKind::Boolean: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(compiler.chunk().boolConstants.size());
        compiler.chunk().boolConstants.push_back(value.get<bool>());
        compiler.emit(BytecodeOp::PushConstBoolean, idx);
        return;
    }
    case BytecodeStackKind::Color: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(compiler.chunk().colorConstants.size());
        compiler.chunk().colorConstants.push_back(value.asColor());
        compiler.emit(BytecodeOp::PushConstColor, idx);
        return;
    }
    case BytecodeStackKind::Value: {
        BytecodeIndex idx = static_cast<BytecodeIndex>(compiler.chunk().constants.size());
        compiler.chunk().constants.push_back(value);
        compiler.emit(BytecodeOp::PushConstValue, idx);
        return;
    }
    }
}

void compileDefaultConst(BytecodeCompiler &compiler, BytecodeStackKind wanted) {
    switch (wanted) {
    case BytecodeStackKind::Number:
        compileConstant(compiler, Value(0.0), wanted);
        return;
    case BytecodeStackKind::String:
        compileConstant(compiler, Value(std::string()), wanted);
        return;
    case BytecodeStackKind::Boolean:
        compileConstant(compiler, Value(false), wanted);
        return;
    case BytecodeStackKind::Color:
        compileConstant(compiler, Value(Color{}), wanted);
        return;
    case BytecodeStackKind::Value:
        compileConstant(compiler, Value(0.0), wanted);
        return;
    }
}

void compileSignatureParam(BytecodeCompiler &compiler, Block *block, const std::string &name, BytecodeStackKind wanted) {
    const ParsedInput *input = Scratch::getInput(block, name);
    if (input == nullptr) {
        const auto field = block->fieldMap.find(name);
        if (field != block->fieldMap.end()) {
            compileConstant(compiler, Value(field->second->value), wanted);
            return;
        }
    }
    compiler.compileOperand(input, wanted);
}

void emitCallNative(BytecodeCompiler &compiler, BytecodeIndex opSlot, BytecodeIndex argCount, BytecodeStackKind returnKind) {
    switch (returnKind) {
    case BytecodeStackKind::Number:
        compiler.emit(BytecodeOp::CallNativeNumber, opSlot, argCount);
        return;
    case BytecodeStackKind::String:
        compiler.emit(BytecodeOp::CallNativeString, opSlot, argCount);
        return;
    case BytecodeStackKind::Boolean:
        compiler.emit(BytecodeOp::CallNativeBoolean, opSlot, argCount);
        return;
    case BytecodeStackKind::Color:
        compiler.emit(BytecodeOp::CallNativeColor, opSlot, argCount);
        return;
    case BytecodeStackKind::Value:
        compiler.emit(BytecodeOp::CallNativeValue, opSlot, argCount);
        return;
    }
}

void compileReporter(BytecodeCompiler &compiler, Block *block, BytecodeStackKind wanted);

bool tryCompileArgumentReporter(BytecodeCompiler &compiler, Block *block, BytecodeStackKind wanted) {
    if (compiler.currentArgumentNames() == nullptr) return false;
    if (block->opcode != "argument_reporter_string_number" && block->opcode != "argument_reporter_boolean") return false;

    std::string argName = Scratch::getFieldValue(*block, "VALUE");
    const auto &names = *compiler.currentArgumentNames();
    auto it = std::find(names.begin(), names.end(), argName);
    if (it == names.end()) return false;

    BytecodeIndex argSlot = static_cast<BytecodeIndex>(std::distance(names.begin(), it));
    compiler.emit(BytecodeOp::PushArg, argSlot);
    compiler.convertIfNeeded(BytecodeStackKind::Value, wanted);
    return true;
}

void compileReporter(BytecodeCompiler &compiler, Block *block, BytecodeStackKind wanted) {
    if (tryCompileArgumentReporter(compiler, block, wanted)) return;

    const std::string &opcode = block->opcode;

    auto &customCompilers = BytecodeCompiler::getCustomCompilers();
    auto customIt = customCompilers.find(opcode);
    if (customIt != customCompilers.end()) {
        BytecodeIndex customInstr;
        if (customIt->second(compiler, block, customInstr)) {
            compiler.convertIfNeeded(stackKindForType(block->blockFunction.type), wanted);
            return;
        }
    }

    const auto &signatures = BlockExecutor::getInputSignatures();
    auto sigIt = signatures.find(opcode);

    if (sigIt == signatures.end()) {
        BytecodeIndex slot = compiler.escapeSlot(block);
        compiler.emit(BytecodeOp::CallLegacyReporter, slot);
        compiler.convertIfNeeded(BytecodeStackKind::Value, wanted);
        return;
    }

    for (const auto &sig : sigIt->second) {
        compileSignatureParam(compiler, block, sig.name, sig.kind);
    }

    BytecodeIndex opSlot = compiler.opcodeSlot(opcode, sigIt->second);

    if (BlockExecutor::getBlockingBlocks().count(opcode) != 0) {
        compiler.emit(BytecodeOp::CallNativeBlockingValue, opSlot, static_cast<BytecodeIndex>(sigIt->second.size()));
        compiler.convertIfNeeded(BytecodeStackKind::Value, wanted);
        return;
    }

    BytecodeStackKind nativeKind = stackKindForType(block->blockFunction.type);
    emitCallNative(compiler, opSlot, static_cast<BytecodeIndex>(sigIt->second.size()), nativeKind);

    compiler.convertIfNeeded(nativeKind, wanted);
}

} // namespace

void BytecodeCompiler::emitPushVar(BytecodeIndex slot, BytecodeStackKind wanted) {
    switch (wanted) {
    case BytecodeStackKind::Number:
        emit(BytecodeOp::PushVarNumber, slot);
        return;
    case BytecodeStackKind::String:
        emit(BytecodeOp::PushVarString, slot);
        return;
    case BytecodeStackKind::Boolean:
        emit(BytecodeOp::PushVarBoolean, slot);
        return;
    case BytecodeStackKind::Color:
        emit(BytecodeOp::PushVarColor, slot);
        return;
    case BytecodeStackKind::Value:
        emit(BytecodeOp::PushVarValue, slot);
        return;
    }
}

void BytecodeCompiler::compileOperand(const ParsedInput *input, BytecodeStackKind wanted) {
    if (input == nullptr) {
        compileDefaultConst(*this, wanted);
        return;
    }

    switch (input->inputType) {
    case ParsedInput::VALUE:
        compileConstant(*this, input->value, wanted);
        return;

    case ParsedInput::VARIABLE:
        if (input->list) {
            BytecodeIndex slot = listSlot(input->variableId);
            emit(BytecodeOp::PushList, slot);
            convertIfNeeded(BytecodeStackKind::Value, wanted);
        } else {
            emitPushVar(variableSlot(input->variableId), wanted);
        }
        return;

    case ParsedInput::BLOCK:
        if (input->block == nullptr) {
            compileDefaultConst(*this, wanted);
            return;
        }
        {
            bool savedStatementContext = statementContext_;
            statementContext_ = false;
            compileReporter(*this, input->block, wanted);
            statementContext_ = savedStatementContext;
        }
        return;
    }
}

void BytecodeCompiler::compileInput(Block *block, const std::string &inputName, BytecodeStackKind wanted) {
    compileSignatureParam(*this, block, inputName, wanted);
}

BytecodeIndex BytecodeCompiler::compileConditionalJump(Block *block, const std::string &inputName, bool jumpIfTrue) {
    compileInput(block, inputName, BytecodeStackKind::Boolean);

    BytecodeInstruction &last = chunk_.instructions.back();
    if (last.op == BytecodeOp::NumEquals || last.op == BytecodeOp::NumLess || last.op == BytecodeOp::NumGreater) {
        const BytecodeCompareKind kind = last.op == BytecodeOp::NumEquals ? BytecodeCompareKind::Equals
                                         : last.op == BytecodeOp::NumLess ? BytecodeCompareKind::Less
                                                                          : BytecodeCompareKind::Greater;
        last.op = jumpIfTrue ? BytecodeOp::JumpIfNumCompareTrue : BytecodeOp::JumpIfNumCompareFalse;
        last.a = invalidBytecodeIndex;
        last.b = static_cast<BytecodeIndex>(kind);
        return static_cast<BytecodeIndex>(chunk_.instructions.size() - 1);
    }

    return emit(jumpIfTrue ? BytecodeOp::JumpIfTrue : BytecodeOp::JumpIfFalse);
}

bool BytecodeCompiler::compileBlockChain(Block *start, Block *stopBlock, BytecodeIndex &outFirstInstr) {
    BytecodeIndex firstInstruction = invalidBytecodeIndex;
    Block *block = start;

    while (block != nullptr && block != stopBlock) {
        auto &customCompilers = getCustomCompilers();
        auto customIt = customCompilers.find(block->opcode);
        if (customIt != customCompilers.end()) {
            bool savedStatementContext = statementContext_;
            statementContext_ = true;
            BytecodeIndex customInstr;
            bool ok = customIt->second(*this, block, customInstr);
            statementContext_ = savedStatementContext;

            if (ok) {
                if (firstInstruction == invalidBytecodeIndex) firstInstruction = customInstr;
                block = block->nextBlock;
                continue;
            }
        }

        const auto &signatures = BlockExecutor::getInputSignatures();
        auto sigIt = signatures.find(block->opcode);
        if (sigIt == signatures.end()) {
            BytecodeIndex slot = escapeSlot(block);
            BytecodeIndex idx = emit(BytecodeOp::EscapeToLegacyThread, slot);
            if (firstInstruction == invalidBytecodeIndex) firstInstruction = idx;
            outFirstInstr = firstInstruction;
            return false;
        }

        const BytecodeIndex blockStart = here();

        for (const auto &sig : sigIt->second) {
            compileSignatureParam(*this, block, sig.name, sig.kind);
        }

        BytecodeIndex opSlot = opcodeSlot(block->opcode, sigIt->second);
        emit(BlockExecutor::getBlockingBlocks().count(block->opcode) != 0 ? BytecodeOp::CallNativeBlocking : BytecodeOp::CallNative, opSlot, static_cast<BytecodeIndex>(sigIt->second.size()));

        if (firstInstruction == invalidBytecodeIndex) firstInstruction = blockStart;
        block = block->nextBlock;
    }

    outFirstInstr = firstInstruction;
    return true;
}

namespace {

BytecodeIndex compileStatementChain(BytecodeCompiler &compiler, Block *start) {
    if (start == nullptr) {
        return compiler.emit(BytecodeOp::Halt);
    }

    BytecodeIndex firstInstr;
    if (compiler.compileBlockChain(start, nullptr, firstInstr)) {
        compiler.emit(BytecodeOp::Halt);
    }
    return firstInstr;
}

} // namespace

void Parser::compileBytecode(Sprite *sprite) {
    BytecodeCompiler compiler(sprite);

    for (const auto &[proccode, definitionBlock] : sprite->customHatBlock) {
        if (definitionBlock == nullptr) continue;
        compiler.registerProcedure(definitionBlock, proccode);
    }

    for (const auto &[opcode, hatBlocks] : sprite->hats) {
        for (Block *hat : hatBlocks) {
            if (hat == nullptr) continue;

            BytecodeScript script;
            script.opcodeIndex = compiler.chunk().internString(opcode);
            script.entryInstruction = compileStatementChain(compiler, hat);
            script.blockOrdinal = hat->bytecodeOrdinal;
            compiler.chunk().scripts.push_back(script);
            compiler.link().scriptBlocks.push_back(hat);
        }
    }

    for (const auto &[proccode, definitionBlock] : sprite->customHatBlock) {
        if (definitionBlock == nullptr) continue;

        BytecodeIndex slot = compiler.procedureSlot(definitionBlock);

        compiler.setCurrentArgumentNames(&definitionBlock->argumentNames);
        BytecodeIndex entry = compileStatementChain(compiler, definitionBlock->nextBlock);
        compiler.setCurrentArgumentNames(nullptr);

        compiler.chunk().procedures[slot].entryInstruction = entry;
    }

    sprite->bytecode = std::make_unique<BytecodeChunk>(std::move(compiler.chunk()));
    sprite->bytecodeLink = std::make_unique<BytecodeLinkTable>(std::move(compiler.link()));
}
