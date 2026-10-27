#include "bytecode_analysis.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <sstream>
#include <variant>

namespace {

using VType = BytecodeValueType;
using State = BytecodeAbstractState;

VType join(VType a, VType b) {
    if (a == VType::Unreached) return b;
    if (b == VType::Unreached) return a;
    return a == b ? a : VType::Unknown;
}

VType typeOfKind(BytecodeStackKind kind) {
    switch (kind) {
    case BytecodeStackKind::Number:
        return VType::Number;
    case BytecodeStackKind::String:
        return VType::String;
    case BytecodeStackKind::Boolean:
        return VType::Boolean;
    case BytecodeStackKind::Color:
        return VType::Color;
    case BytecodeStackKind::Value:
        break;
    }
    return VType::Unknown;
}

const char *typeName(VType t) {
    switch (t) {
    case VType::Unreached:
        return "unreached";
    case VType::Number:
        return "number";
    case VType::String:
        return "string";
    case VType::Boolean:
        return "boolean";
    case VType::Color:
        return "color";
    case VType::Unknown:
        break;
    }
    return "unknown";
}

bool hasPositional(const BytecodePositionalFn &fn) {
    return fn.nn_n || fn.n_n || fn.b_b || fn.bb_b || fn.ss_s || fn.ss_b || fn.s_n || fn.ns_s ||
           fn.voidFn || fn.n_void || fn.nn_void || fn.s_void || fn.sn_void || fn.c_void ||
           fn.spriteS_n || fn.spriteS_b || fn.sprite_n || fn.sprite_b || fn.sprite_s ||
           fn.v_void || fn.v_b || fn.spriteSS_v;
}

uint32_t *typedCount(State &s, BytecodeStackKind kind) {
    switch (kind) {
    case BytecodeStackKind::Number:
        return &s.numbers;
    case BytecodeStackKind::String:
        return &s.strings;
    case BytecodeStackKind::Boolean:
        return &s.booleans;
    case BytecodeStackKind::Color:
        return &s.colors;
    case BytecodeStackKind::Value:
        break;
    }
    return nullptr;
}

bool pop(State &s, BytecodeStackKind kind, VType *type = nullptr) {
    if (kind == BytecodeStackKind::Value) {
        if (s.values.empty()) return false;
        if (type) *type = s.values.back();
        s.values.pop_back();
        return true;
    }
    uint32_t *count = typedCount(s, kind);
    if (*count == 0) return false;
    --*count;
    if (type) *type = typeOfKind(kind);
    return true;
}

void push(State &s, BytecodeStackKind kind, VType type = VType::Unknown) {
    if (kind == BytecodeStackKind::Value) {
        s.values.push_back(type);
        return;
    }
    ++*typedCount(s, kind);
}

bool isBlockEnd(BytecodeOp op) {
    switch (op) {
    case BytecodeOp::Jump:
    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue:
    case BytecodeOp::JumpIfNumCompareFalse:
    case BytecodeOp::JumpIfNumCompareTrue:
    case BytecodeOp::RepeatCheck:
    case BytecodeOp::ForEachCheck:
    case BytecodeOp::Halt:
    case BytecodeOp::Return:
    case BytecodeOp::EscapeToLegacyThread:
        return true;
    default:
        return false;
    }
}

bool hasJumpTarget(BytecodeOp op) {
    switch (op) {
    case BytecodeOp::Jump:
    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue:
    case BytecodeOp::JumpIfNumCompareFalse:
    case BytecodeOp::JumpIfNumCompareTrue:
    case BytecodeOp::RepeatCheck:
    case BytecodeOp::ForEachCheck:
        return true;
    default:
        return false;
    }
}

bool isTerminal(BytecodeOp op) {
    return op == BytecodeOp::Halt || op == BytecodeOp::Return || op == BytecodeOp::EscapeToLegacyThread;
}

bool isUnconditionalJump(BytecodeOp op) { return op == BytecodeOp::Jump; }

} // namespace

BytecodeAnalysis::BytecodeAnalysis(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysisPolicy policy)
    : chunk_(chunk), link_(link), policy_(policy) {}

void BytecodeAnalysis::declareVariables() {
    declaredVariableTypes_.assign(chunk_.variableSlots.size(), VType::Unknown);
    for (size_t i = 0; i < declaredVariableTypes_.size() && i < link_.variables.size(); i++) {
        const Variable *var = link_.variables[i];
        if (var == nullptr) continue;
        switch (var->value.index()) {
        case 1:
            declaredVariableTypes_[i] = VType::Number;
            break;
        case 2:
            declaredVariableTypes_[i] = VType::String;
            break;
        case 3:
            declaredVariableTypes_[i] = VType::Boolean;
            break;
        default:
            break;
        }
    }
}

void BytecodeAnalysis::buildBlocks() {
    const size_t count = chunk_.instructions.size();
    std::vector<bool> leader(count + 1, false);

    for (const BytecodeScript &script : chunk_.scripts) {
        if (script.entryInstruction < count) leader[script.entryInstruction] = true;
    }
    for (const BytecodeProcedure &proc : chunk_.procedures) {
        if (proc.entryInstruction < count) leader[proc.entryInstruction] = true;
    }
    for (size_t pc = 0; pc < count; pc++) {
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        if (hasJumpTarget(instr.op) && instr.a < count) leader[instr.a] = true;
        if (isBlockEnd(instr.op)) leader[pc + 1] = true;
    }
    if (count > 0) leader[0] = true;

    blockOfInstruction_.assign(count, 0);
    blocks_.clear();
    for (size_t pc = 0; pc < count; pc++) {
        if (leader[pc]) {
            if (!blocks_.empty()) blocks_.back().end = static_cast<BytecodeIndex>(pc);
            Block block;
            block.start = static_cast<BytecodeIndex>(pc);
            blocks_.push_back(std::move(block));
        }
        blockOfInstruction_[pc] = static_cast<uint32_t>(blocks_.size() - 1);
    }
    if (!blocks_.empty()) blocks_.back().end = static_cast<BytecodeIndex>(count);
}

void BytecodeAnalysis::forgetVariables(State &state) const {
    state.variables = declaredVariableTypes_;
}

bool BytecodeAnalysis::step(State &s, const BytecodeInstruction &instr, bool warp, std::string &error) const {
    using K = BytecodeStackKind;
    const BytecodeOp op = instr.op;

    const auto fail = [&](const char *what) {
        error = what;
        return false;
    };
    const auto varType = [&](BytecodeIndex slot) {
        return slot < s.variables.size() ? s.variables[slot] : VType::Unknown;
    };
    const auto setVar = [&](BytecodeIndex slot, VType type) {
        if (slot < s.variables.size()) s.variables[slot] = type;
    };
    const auto popN = [&](K kind, int n) {
        for (int i = 0; i < n; i++) {
            if (!pop(s, kind)) return false;
        }
        return true;
    };

    switch (op) {
    case BytecodeOp::Nop:
    case BytecodeOp::Jump:
    case BytecodeOp::ShowListMonitor:
    case BytecodeOp::HideListMonitor:
    case BytecodeOp::ShowVariableMonitor:
    case BytecodeOp::HideVariableMonitor:
    case BytecodeOp::GoToFrontBack:
    case BytecodeOp::SetRotationStyle:
    case BytecodeOp::SetDragMode:
    case BytecodeOp::DeleteAllOfList:
        return true;

    case BytecodeOp::PushConstNumber:
        push(s, K::Number);
        return true;
    case BytecodeOp::PushConstString:
        push(s, K::String);
        return true;
    case BytecodeOp::PushConstBoolean:
        push(s, K::Boolean);
        return true;
    case BytecodeOp::PushConstColor:
        push(s, K::Color);
        return true;
    case BytecodeOp::PushConstValue: {
        VType type = VType::Unknown;
        if (instr.a < chunk_.constants.size()) {
            const Value &constant = chunk_.constants[instr.a];
            if (constant.isDouble()) type = VType::Number;
            else if (constant.isString()) type = VType::String;
            else if (constant.isBoolean()) type = VType::Boolean;
            else if (constant.isColor()) type = VType::Color;
        }
        push(s, K::Value, type);
        return true;
    }

    case BytecodeOp::PushVarNumber:
        push(s, K::Number);
        return true;
    case BytecodeOp::PushVarString:
        push(s, K::String);
        return true;
    case BytecodeOp::PushVarBoolean:
        push(s, K::Boolean);
        return true;
    case BytecodeOp::PushVarColor:
        push(s, K::Color);
        return true;
    case BytecodeOp::PushVarValue: {
        VType type = varType(instr.a);
        push(s, K::Value, type == VType::Unreached ? VType::Unknown : type);
        return true;
    }
    case BytecodeOp::PushList:
    case BytecodeOp::PushArg:
        push(s, K::Value);
        return true;

    case BytecodeOp::StoreVarNumber:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        setVar(instr.a, VType::Number);
        return true;
    case BytecodeOp::StoreVarString:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        setVar(instr.a, VType::String);
        return true;
    case BytecodeOp::StoreVarBoolean:
        if (!pop(s, K::Boolean)) return fail("stack underflow (boolean)");
        setVar(instr.a, VType::Boolean);
        return true;
    case BytecodeOp::StoreVarColor:
        if (!pop(s, K::Color)) return fail("stack underflow (color)");
        setVar(instr.a, VType::Color);
        return true;
    case BytecodeOp::StoreVarValue: {
        VType type;
        if (!pop(s, K::Value, &type)) return fail("stack underflow (value)");
        setVar(instr.a, type);
        return true;
    }
    case BytecodeOp::AppendVarString:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        setVar(instr.a, VType::String);
        return true;
    case BytecodeOp::ChangeVarNumber:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        setVar(instr.a, VType::Number);
        return true;

    case BytecodeOp::AddToList:
    case BytecodeOp::DeleteFromList:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        return true;
    case BytecodeOp::InsertInList:
    case BytecodeOp::ReplaceInList:
        if (!popN(K::Value, 2)) return fail("stack underflow (value)");
        return true;
    case BytecodeOp::ItemOfList:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Value);
        return true;
    case BytecodeOp::ItemNumOfList:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Number);
        return true;
    case BytecodeOp::LengthOfList:
        push(s, K::Number);
        return true;
    case BytecodeOp::ListContainsItem:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Boolean);
        return true;

    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean:
    case BytecodeOp::CallNativeColor:
    case BytecodeOp::CallNativeValue:
    case BytecodeOp::CallNativeBlocking:
    case BytecodeOp::CallNativeBlockingValue: {
        const BytecodeIndex slot = instr.a;
        if (slot >= chunk_.paramKindsStart.size() || slot >= chunk_.paramKindsCount.size()) return fail("bad opcode slot");
        const BytecodeIndex start = chunk_.paramKindsStart[slot];
        const BytecodeIndex count = chunk_.paramKindsCount[slot];
        if (start + count > chunk_.paramKinds.size()) return fail("bad parameter kinds");
        for (BytecodeIndex i = count; i-- > 0;) {
            if (!pop(s, chunk_.paramKinds[start + i])) return fail("stack underflow (native argument)");
        }

        switch (op) {
        case BytecodeOp::CallNativeNumber:
            push(s, K::Number);
            break;
        case BytecodeOp::CallNativeString:
            push(s, K::String);
            break;
        case BytecodeOp::CallNativeBoolean:
            push(s, K::Boolean);
            break;
        case BytecodeOp::CallNativeColor:
            push(s, K::Color);
            break;
        case BytecodeOp::CallNativeValue:
        case BytecodeOp::CallNativeBlockingValue:
            push(s, K::Value);
            break;
        default:
            break;
        }

        const bool blocking = op == BytecodeOp::CallNativeBlocking || op == BytecodeOp::CallNativeBlockingValue;
        const bool positional = slot < link_.positional.size() && hasPositional(link_.positional[slot]);
        const bool pure = !blocking && (positional ? !policy_.positionalCallsForget : !policy_.genericNativeCallsForget);
        if (!pure) forgetVariables(s);
        return true;
    }

    case BytecodeOp::CallProcedure:
    case BytecodeOp::CallProcedureValue:
        for (BytecodeIndex i = 0; i < instr.b; i++) {
            if (!pop(s, K::Value)) return fail("stack underflow (procedure argument)");
        }
        if (op == BytecodeOp::CallProcedureValue) push(s, K::Value);
        forgetVariables(s);
        return true;
    case BytecodeOp::Return:
        if (!pop(s, K::Value)) return fail("stack underflow (return value)");
        return true;

    case BytecodeOp::CallLegacyReporter:
        push(s, K::Value);
        forgetVariables(s);
        return true;
    case BytecodeOp::CallLegacyStatement:
        forgetVariables(s);
        return true;
    case BytecodeOp::EscapeToLegacyThread:
        return true;

    case BytecodeOp::ConvertNumberToValue:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        push(s, K::Value, VType::Number);
        return true;
    case BytecodeOp::ConvertStringToValue:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        push(s, K::Value, VType::String);
        return true;
    case BytecodeOp::ConvertBooleanToValue:
        if (!pop(s, K::Boolean)) return fail("stack underflow (boolean)");
        push(s, K::Value, VType::Boolean);
        return true;
    case BytecodeOp::ConvertColorToValue:
        if (!pop(s, K::Color)) return fail("stack underflow (color)");
        push(s, K::Value, VType::Color);
        return true;
    case BytecodeOp::ConvertValueToNumber:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Number);
        return true;
    case BytecodeOp::ConvertValueToString:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::String);
        return true;
    case BytecodeOp::ConvertValueToBoolean:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Boolean);
        return true;
    case BytecodeOp::ConvertValueToColor:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Color);
        return true;

    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue:
        if (!pop(s, K::Boolean)) return fail("stack underflow (boolean)");
        return true;
    case BytecodeOp::JumpIfNumCompareFalse:
    case BytecodeOp::JumpIfNumCompareTrue:
        if (!popN(K::Number, 2)) return fail("stack underflow (number)");
        return true;

    // These two describe the path that stays in the loop; the exit edge is handled by the caller.
    case BytecodeOp::RepeatCheck:
        if (s.values.empty()) return fail("stack underflow (repeat counter)");
        s.values.back() = VType::Number;
        return true;
    case BytecodeOp::ForEachCheck:
        if (s.values.size() < 2) return fail("stack underflow (for each counter)");
        s.values.back() = VType::Number;
        setVar(instr.b, VType::Number);
        return true;

    case BytecodeOp::Wait:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        forgetVariables(s);
        return true;
    case BytecodeOp::BroadcastAndWait:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        forgetVariables(s);
        return true;
    case BytecodeOp::PlaySoundUntilDone:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        forgetVariables(s);
        return true;

    case BytecodeOp::RandomInt:
        if (!popN(K::Number, 2)) return fail("stack underflow (number)");
        push(s, K::Number);
        return true;
    case BytecodeOp::Random:
        if (!popN(K::Value, 2)) return fail("stack underflow (value)");
        push(s, K::Number);
        return true;
    case BytecodeOp::LetterOf:
        if (!pop(s, K::Value) || !pop(s, K::Number)) return fail("stack underflow (letter of)");
        push(s, K::String);
        return true;
    case BytecodeOp::LengthOf:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        push(s, K::Number);
        return true;
    case BytecodeOp::LetterOfVar:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        push(s, K::String);
        return true;
    case BytecodeOp::LengthOfVar:
        push(s, K::Number);
        return true;
    case BytecodeOp::MathOp:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        push(s, K::Number);
        return true;

    case BytecodeOp::ValueEquals:
    case BytecodeOp::ValueLess:
    case BytecodeOp::ValueGreater:
        if (!popN(K::Value, 2)) return fail("stack underflow (value)");
        push(s, K::Boolean);
        return true;
    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater:
        if (!popN(K::Number, 2)) return fail("stack underflow (number)");
        push(s, K::Boolean);
        return true;
    case BytecodeOp::StrEquals:
    case BytecodeOp::StrLess:
    case BytecodeOp::StrGreater:
        if (!popN(K::String, 2)) return fail("stack underflow (string)");
        push(s, K::Boolean);
        return true;

    case BytecodeOp::SetEffectTo:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        return true;
    case BytecodeOp::ChangeEffectBy:
    case BytecodeOp::GoForwardBackwardLayers:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        return true;
    case BytecodeOp::CostumeNumberName:
    case BytecodeOp::BackdropNumberName:
        push(s, K::Value, instr.b == 0 ? VType::String : VType::Number);
        return true;
    case BytecodeOp::CurrentDateTime:
        push(s, K::Number);
        return true;

    case BytecodeOp::PopNumber:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        return true;
    case BytecodeOp::PopString:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        return true;
    case BytecodeOp::PopBoolean:
        if (!pop(s, K::Boolean)) return fail("stack underflow (boolean)");
        return true;
    case BytecodeOp::PopColor:
        if (!pop(s, K::Color)) return fail("stack underflow (color)");
        return true;
    case BytecodeOp::PopValue:
        if (!pop(s, K::Value)) return fail("stack underflow (value)");
        return true;

    case BytecodeOp::Yield:
        if (!warp || policy_.warpYieldForgets) forgetVariables(s);
        return true;
    case BytecodeOp::Halt:
        return true;
    }

    return fail("unhandled opcode");
}

bool BytecodeAnalysis::merge(Block &target, const State &incoming, int entry, std::string &error) {
    if (!target.reached) {
        target.reached = true;
        target.entry = entry;
        target.in = incoming;
        return true;
    }

    State &current = target.in;
    if (current.numbers != incoming.numbers || current.strings != incoming.strings ||
        current.booleans != incoming.booleans || current.colors != incoming.colors ||
        current.values.size() != incoming.values.size()) {
        error = "stack heights differ where paths merge at instruction " + std::to_string(target.start);
        return false;
    }

    bool changed = false;
    for (size_t i = 0; i < current.values.size(); i++) {
        const VType joined = join(current.values[i], incoming.values[i]);
        if (joined != current.values[i]) {
            current.values[i] = joined;
            changed = true;
        }
    }
    for (size_t i = 0; i < current.variables.size() && i < incoming.variables.size(); i++) {
        const VType joined = join(current.variables[i], incoming.variables[i]);
        if (joined != current.variables[i]) {
            current.variables[i] = joined;
            changed = true;
        }
    }
    return changed;
}

bool BytecodeAnalysis::run() {
    buildBlocks();
    declareVariables();
    entries_.clear();
    stats_ = {};
    stats_.instructions = static_cast<uint32_t>(chunk_.instructions.size());
    stats_.blocks = static_cast<uint32_t>(blocks_.size());

    const size_t count = chunk_.instructions.size();
    std::vector<uint32_t> worklist;
    std::vector<bool> queued(blocks_.size(), false);

    const auto addEntry = [&](BytecodeIndex entryPc, bool isProcedure, BytecodeIndex index, bool warp, uint32_t argCount) {
        BytecodeEntryResult result;
        result.entryInstruction = entryPc;
        result.isProcedure = isProcedure;
        result.index = index;
        result.warp = warp;
        if (entryPc >= count) {
            result.ok = false;
            result.error = "entry instruction out of range";
            entries_.push_back(std::move(result));
            return;
        }

        State state;
        state.variables = declaredVariableTypes_;
        state.values.assign(argCount, VType::Unknown); // procedure arguments are already on the value stack
        const int entry = static_cast<int>(entries_.size());
        entries_.push_back(std::move(result));

        Block &block = blocks_[blockOfInstruction_[entryPc]];
        block.warp = warp;
        std::string error;
        if (merge(block, state, entry, error) && !queued[blockOfInstruction_[entryPc]]) {
            queued[blockOfInstruction_[entryPc]] = true;
            worklist.push_back(blockOfInstruction_[entryPc]);
        }
    };

    for (size_t i = 0; i < chunk_.scripts.size(); i++) {
        addEntry(chunk_.scripts[i].entryInstruction, false, static_cast<BytecodeIndex>(i), false, 0);
    }
    for (size_t i = 0; i < chunk_.procedures.size(); i++) {
        const BytecodeProcedure &proc = chunk_.procedures[i];
        addEntry(proc.entryInstruction, true, static_cast<BytecodeIndex>(i), proc.runWithoutScreenRefresh, proc.argCount);
    }

    const auto fail = [&](int entry, const std::string &message) {
        BytecodeEntryResult &result = entries_[entry];
        if (result.ok) {
            result.ok = false;
            result.error = message;
        }
    };

    const auto propagate = [&](int entry, bool warp, BytecodeIndex targetPc, const State &state) {
        if (targetPc >= count) {
            fail(entry, "jump target out of range");
            return;
        }
        const uint32_t targetBlock = blockOfInstruction_[targetPc];
        Block &target = blocks_[targetBlock];
        target.warp = warp;
        std::string error;
        const bool changed = merge(target, state, entry, error);
        if (!error.empty()) {
            fail(entry, error);
            return;
        }
        if (changed && !queued[targetBlock]) {
            queued[targetBlock] = true;
            worklist.push_back(targetBlock);
        }
    };

    while (!worklist.empty()) {
        const uint32_t blockIndex = worklist.back();
        worklist.pop_back();
        queued[blockIndex] = false;

        const Block &block = blocks_[blockIndex];
        const int entry = block.entry;
        const bool warp = block.warp;
        if (!entries_[entry].ok) continue;

        State state = block.in;
        State preLast;
        std::string error;
        bool good = true;
        for (BytecodeIndex pc = block.start; pc < block.end; pc++) {
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            const bool last = pc + 1 == block.end;
            if (last && (instr.op == BytecodeOp::RepeatCheck || instr.op == BytecodeOp::ForEachCheck)) preLast = state;
            if (!step(state, instr, warp, error)) {
                fail(entry, error + " at instruction " + std::to_string(pc));
                good = false;
                break;
            }
        }
        if (!good) continue;

        const BytecodeIndex lastPc = block.end - 1;
        const BytecodeInstruction &lastInstr = chunk_.instructions[lastPc];

        if (isTerminal(lastInstr.op)) continue;

        if (isUnconditionalJump(lastInstr.op)) {
            propagate(entry, warp, lastInstr.a, state);
            continue;
        }

        if (hasJumpTarget(lastInstr.op)) {
            State taken = state;
            if (lastInstr.op == BytecodeOp::RepeatCheck) {
                taken = preLast;
                taken.values.pop_back();
            } else if (lastInstr.op == BytecodeOp::ForEachCheck) {
                taken = preLast;
                taken.values.pop_back();
                taken.values.pop_back();
            }
            propagate(entry, warp, lastInstr.a, taken);
        }

        if (block.end >= count) {
            fail(entry, "execution runs off the end of the chunk");
            continue;
        }
        propagate(entry, warp, block.end, state);
    }

    for (const Block &block : blocks_) {
        if (!block.reached || block.entry < 0) continue;
        entries_[block.entry].reachableInstructions += block.end - block.start;
    }

    countStats();

    bool allOk = true;
    for (const BytecodeEntryResult &entry : entries_)
        allOk = allOk && entry.ok;
    return allOk;
}

void BytecodeAnalysis::countStats() {
    for (const Block &block : blocks_) {
        if (!block.reached || block.entry < 0 || !entries_[block.entry].ok) continue;
        stats_.reachable += block.end - block.start;

        State state = block.in;
        std::string error;
        for (BytecodeIndex pc = block.start; pc < block.end; pc++) {
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            switch (instr.op) {
            case BytecodeOp::PushVarValue: {
                stats_.pushVarValue++;
                const VType t = instr.a < state.variables.size() ? state.variables[instr.a] : VType::Unknown;
                if (t != VType::Unknown && t != VType::Unreached) stats_.pushVarValueKnown++;
                break;
            }
            case BytecodeOp::StoreVarValue:
                stats_.storeVarValue++;
                if (!state.values.empty() && state.values.back() != VType::Unknown) stats_.storeVarValueKnown++;
                break;
            case BytecodeOp::ConvertValueToNumber:
            case BytecodeOp::ConvertValueToString:
            case BytecodeOp::ConvertValueToBoolean:
            case BytecodeOp::ConvertValueToColor: {
                stats_.convertValueToTyped++;
                if (state.values.empty() || state.values.back() == VType::Unknown) break;
                stats_.convertValueToTypedKnown++;
                const VType target = instr.op == BytecodeOp::ConvertValueToNumber    ? VType::Number
                                     : instr.op == BytecodeOp::ConvertValueToString  ? VType::String
                                     : instr.op == BytecodeOp::ConvertValueToBoolean ? VType::Boolean
                                                                                     : VType::Color;
                if (state.values.back() == target) stats_.convertValueToTypedIdentity++;
                break;
            }
            case BytecodeOp::PushArg:
                stats_.pushArg++;
                break;
            case BytecodeOp::PushVarNumber:
            case BytecodeOp::PushVarString:
            case BytecodeOp::PushVarBoolean:
            case BytecodeOp::ChangeVarNumber: {
                if (instr.a >= declaredVariableTypes_.size() || declaredVariableTypes_[instr.a] != VType::Unknown) break;
                stats_.demotedAccesses++;
                const VType wanted = instr.op == BytecodeOp::PushVarString    ? VType::String
                                     : instr.op == BytecodeOp::PushVarBoolean ? VType::Boolean
                                                                              : VType::Number;
                if (instr.a < state.variables.size() && state.variables[instr.a] == wanted) stats_.demotedAccessesKnown++;
                break;
            }
            default:
                break;
            }
            if (instr.op == BytecodeOp::StoreVarValue && instr.a < declaredVariableTypes_.size() && declaredVariableTypes_[instr.a] == VType::Unknown) {
                stats_.demotedAccesses++;
                if (!state.values.empty() && state.values.back() != VType::Unknown) stats_.demotedAccessesKnown++;
            }
            if (!step(state, instr, block.warp, error)) break;
        }
    }
}

BytecodeValueType BytecodeAnalysis::variableTypeBefore(BytecodeIndex pc, BytecodeIndex variableSlot) const {
    if (pc >= chunk_.instructions.size()) return VType::Unreached;
    const Block &block = blocks_[blockOfInstruction_[pc]];
    if (!block.reached) return VType::Unreached;

    State state = block.in;
    std::string error;
    for (BytecodeIndex i = block.start; i < pc; i++) {
        if (!step(state, chunk_.instructions[i], block.warp, error)) return VType::Unreached;
    }
    return variableSlot < state.variables.size() ? state.variables[variableSlot] : VType::Unknown;
}

std::string BytecodeAnalysis::report() const {
    std::ostringstream out;
    uint32_t okCount = 0;
    for (const BytecodeEntryResult &entry : entries_) {
        if (entry.ok) okCount++;
    }

    out << "bytecode analysis: " << entries_.size() << " entries (" << chunk_.scripts.size() << " scripts, "
        << chunk_.procedures.size() << " procedures), " << okCount << " verified";
    if (okCount != entries_.size()) out << ", " << entries_.size() - okCount << " FAILED";
    out << "\n  " << stats_.instructions << " instructions in " << stats_.blocks << " blocks, " << stats_.reachable << " reachable";

    for (const BytecodeEntryResult &entry : entries_) {
        if (entry.ok) continue;
        out << "\n  FAILED " << (entry.isProcedure ? "procedure " : "script ") << entry.index << " (entry " << entry.entryInstruction << "): " << entry.error;
    }

    const auto pct = [](uint32_t part, uint32_t whole) {
        return whole == 0 ? std::string("-") : std::to_string(part * 100 / whole) + "%";
    };
    out << "\n  PushVarValue:         " << stats_.pushVarValueKnown << "/" << stats_.pushVarValue << " have a proven type (" << pct(stats_.pushVarValueKnown, stats_.pushVarValue) << ")";
    out << "\n  StoreVarValue:        " << stats_.storeVarValueKnown << "/" << stats_.storeVarValue << " store a value of proven type (" << pct(stats_.storeVarValueKnown, stats_.storeVarValue) << ")";
    out << "\n  ConvertValueTo*:      " << stats_.convertValueToTypedKnown << "/" << stats_.convertValueToTyped << " unbox a value of proven type (" << pct(stats_.convertValueToTypedKnown, stats_.convertValueToTyped) << "), "
        << stats_.convertValueToTypedIdentity << " already the target type";
    out << "\n  PushArg:              " << stats_.pushArg << " (arguments are untyped)";
    out << "\n  demoted variables:    " << stats_.demotedAccessesKnown << "/" << stats_.demotedAccesses << " accesses have a proven type (" << pct(stats_.demotedAccessesKnown, stats_.demotedAccesses) << ")";
    return out.str();
}
