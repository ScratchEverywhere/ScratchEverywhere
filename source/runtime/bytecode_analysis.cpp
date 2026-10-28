#include "bytecode_analysis.hpp"
#include "math.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <variant>

namespace {

using VType = BytecodeValueType;
using State = BytecodeAbstractState;

bool isNumberLike(VType t) {
    return t == VType::Number || t == VType::NumericString || t == VType::NumberLike;
}

VType join(VType a, VType b) {
    if (a == VType::Unreached) return b;
    if (b == VType::Unreached) return a;
    if (a == b) return a;
    if (isNumberLike(a) && isNumberLike(b)) return VType::NumberLike;
    if ((a == VType::String && b == VType::NumericString) || (a == VType::NumericString && b == VType::String)) return VType::String;
    return VType::Unknown;
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
    case VType::NumericString:
        return "numeric string";
    case VType::NumberLike:
        return "number-like";
    case VType::Unknown:
        break;
    }
    return "unknown";
}

bool isCanonicalNumber(const Value &value) {
    return value.isString() && BytecodeAnalysis::isCanonicalNumericString(value.asString());
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

BytecodeAnalysis::BytecodeAnalysis(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysisPolicy policy, std::vector<BytecodeValueType> variableSeed)
    : chunk_(chunk), link_(link), policy_(policy), seed_(std::move(variableSeed)) {}

bool BytecodeAnalysis::isCanonicalNumericString(const std::string &text) {
    if (text.empty() || text.find_first_of("eE") != std::string::npos) return false;
    const auto parsed = Math::parseNumber(text);
    return parsed.has_value() && std::isfinite(*parsed) && Math::toString(*parsed) == text;
}

uint8_t BytecodeAnalysis::typeBit(BytecodeValueType type) {
    switch (type) {
    case VType::Number:
        return 1;
    case VType::String:
        return 2;
    case VType::Boolean:
        return 4;
    case VType::Color:
        return 8;
    case VType::Unknown:
        return 16;
    case VType::NumericString:
        return 32;
    case VType::NumberLike:
        return 1 | 32;
    case VType::Unreached:
        break;
    }
    return 0;
}

BytecodeValueType BytecodeAnalysis::collapse(uint8_t bits) {
    const uint8_t number = 1, string = 2, boolean = 4, color = 8, unknown = 16, numericString = 32;
    if (bits & unknown) return VType::Unknown;
    if (bits & string) {
        return (bits & ~(string | numericString)) == 0 ? VType::String : VType::Unknown;
    }
    switch (bits) {
    case number:
        return VType::Number;
    case numericString:
        return VType::NumericString;
    case number | numericString:
        return VType::NumberLike;
    case boolean:
        return VType::Boolean;
    case color:
        return VType::Color;
    default:
        return VType::Unknown;
    }
}

void BytecodeAnalysis::recordWrite(BytecodeIndex slot, VType type) const {
    if (slot < written_.size()) written_[slot] |= typeBit(type);
}

void BytecodeAnalysis::declareVariables() {
    std::vector<VType> &declaredVariableTypes_ = declared_;
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
    if (seed_.size() != declared_.size()) seed_ = declared_;
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
    state.variables = seed_;
}

bool BytecodeAnalysis::step(State &s, const BytecodeInstruction &instr, bool warp, int procedure, std::string &error) const {
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
            else if (constant.isString()) type = isCanonicalNumber(constant) ? VType::NumericString : VType::String;
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
        push(s, K::Value);
        return true;
    case BytecodeOp::PushArg: {
        VType type = VType::Unknown;
        if (policy_.interprocedural && procedure >= 0 && static_cast<size_t>(procedure) < params_.size() && instr.a < params_[procedure].size()) {
            type = params_[procedure][instr.a];
        }
        push(s, K::Value, type);
        return true;
    }

    case BytecodeOp::StoreVarNumber:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        setVar(instr.a, VType::Number);
        recordWrite(instr.a, VType::Number);
        return true;
    case BytecodeOp::StoreVarString:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        setVar(instr.a, VType::String);
        recordWrite(instr.a, VType::String);
        return true;
    case BytecodeOp::StoreVarBoolean:
        if (!pop(s, K::Boolean)) return fail("stack underflow (boolean)");
        setVar(instr.a, VType::Boolean);
        recordWrite(instr.a, VType::Boolean);
        return true;
    case BytecodeOp::StoreVarColor:
        if (!pop(s, K::Color)) return fail("stack underflow (color)");
        setVar(instr.a, VType::Color);
        recordWrite(instr.a, VType::Color);
        return true;
    case BytecodeOp::StoreVarValue: {
        VType type;
        if (!pop(s, K::Value, &type)) return fail("stack underflow (value)");
        setVar(instr.a, type);
        recordWrite(instr.a, type);
        return true;
    }
    case BytecodeOp::AppendVarString:
        if (!pop(s, K::String)) return fail("stack underflow (string)");
        setVar(instr.a, VType::String);
        recordWrite(instr.a, VType::String);
        return true;
    case BytecodeOp::ChangeVarNumber:
        if (!pop(s, K::Number)) return fail("stack underflow (number)");
        setVar(instr.a, VType::Number);
        recordWrite(instr.a, VType::Number);
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
    case BytecodeOp::CallProcedureValue: {
        if (instr.a >= chunk_.procedures.size()) return fail("bad procedure index");
        const size_t argCount = instr.b;
        if (s.values.size() < argCount) return fail("stack underflow (procedure argument)");

        if (policy_.interprocedural) {
            const bool shapeMatches = chunk_.procedures[instr.a].argCount == argCount && instr.a < nextParams_.size() && nextParams_[instr.a].size() == argCount;
            for (size_t i = 0; i < argCount; i++) {
                const VType argType = s.values[s.values.size() - argCount + i];
                if (shapeMatches) nextParams_[instr.a][i] = join(nextParams_[instr.a][i], argType);
            }
            if (instr.a < callSeen_.size()) callSeen_[instr.a] = true;
            if (!shapeMatches && instr.a < nextParams_.size()) {
                for (VType &param : nextParams_[instr.a])
                    param = VType::Unknown;
            }
        }
        for (size_t i = 0; i < argCount; i++)
            s.values.pop_back();

        if (op == BytecodeOp::CallProcedureValue) {
            push(s, K::Value, policy_.interprocedural && instr.a < returns_.size() ? returns_[instr.a] : VType::Unknown);
        }

        const bool recursionYields = policy_.interprocedural && procedure >= 0 && instr.a < procedureReaches_.size() &&
                                     !chunk_.procedures[procedure].runWithoutScreenRefresh && !chunk_.procedures[instr.a].runWithoutScreenRefresh &&
                                     (static_cast<int>(instr.a) == procedure || procedureReaches_[instr.a][procedure]);
        if (!policy_.interprocedural || instr.a >= procedureSuspends_.size() || procedureSuspends_[instr.a] || recursionYields) {
            forgetVariables(s);
        } else {
            const std::vector<bool> &writes = procedureWrites_[instr.a];
            for (size_t slot = 0; slot < writes.size() && slot < s.variables.size(); slot++) {
                if (writes[slot]) s.variables[slot] = seed_[slot];
            }
        }
        return true;
    }
    case BytecodeOp::Return: {
        VType type;
        if (!pop(s, K::Value, &type)) return fail("stack underflow (return value)");
        if (procedure >= 0 && static_cast<size_t>(procedure) < nextReturns_.size()) nextReturns_[procedure] = join(nextReturns_[procedure], type);
        return true;
    }

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

    case BytecodeOp::RepeatCheck:
        if (s.values.empty()) return fail("stack underflow (repeat counter)");
        s.values.back() = VType::Number;
        return true;
    case BytecodeOp::ForEachCheck:
        if (s.values.size() < 2) return fail("stack underflow (for each counter)");
        s.values.back() = VType::Number;
        setVar(instr.b, VType::Number);
        recordWrite(instr.b, VType::Number);
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
        if (procedure >= 0 && static_cast<size_t>(procedure) < nextReturns_.size()) nextReturns_[procedure] = join(nextReturns_[procedure], VType::String);
        return true;
    }

    return fail("unhandled opcode");
}

bool BytecodeAnalysis::merge(Block &target, const State &incoming, int entry, std::string &error) {
    if (!target.reached) {
        target.reached = true;
        target.entry = entry;
        target.procedure = entries_[entry].isProcedure ? static_cast<int>(entries_[entry].index) : -1;
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

void BytecodeAnalysis::runDataflow() {
    entries_.clear();
    for (Block &b : blocks_) {
        b.reached = false;
        b.entry = -1;
        b.procedure = -1;
        b.warp = false;
        b.in = {};
    }

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
        state.variables = seed_;
        state.values.assign(argCount, VType::Unknown);
        if (isProcedure && policy_.interprocedural && index < params_.size() && params_[index].size() == argCount) {
            for (uint32_t i = 0; i < argCount; i++) {
                state.values[i] = callSeen_[index] ? params_[index][i] : VType::Unknown;
            }
        }
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
            if (!step(state, instr, warp, block.procedure, error)) {
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
}

void BytecodeAnalysis::computeProcedureEffects() {
    const size_t procedureCount = chunk_.procedures.size();
    const size_t slotCount = chunk_.variableSlots.size();
    procedureSuspends_.assign(procedureCount, false);
    procedureWrites_.assign(procedureCount, std::vector<bool>(slotCount, false));
    std::vector<std::vector<bool>> callees(procedureCount, std::vector<bool>(procedureCount, false));

    for (size_t p = 0; p < procedureCount; p++) {
        const BytecodeIndex entryPc = chunk_.procedures[p].entryInstruction;
        if (entryPc >= chunk_.instructions.size()) {
            procedureSuspends_[p] = true;
            continue;
        }
        const bool warp = chunk_.procedures[p].runWithoutScreenRefresh;

        std::vector<bool> seen(blocks_.size(), false);
        std::vector<uint32_t> stack{blockOfInstruction_[entryPc]};
        while (!stack.empty()) {
            const uint32_t b = stack.back();
            stack.pop_back();
            if (seen[b]) continue;
            seen[b] = true;
            const Block &block = blocks_[b];

            for (BytecodeIndex pc = block.start; pc < block.end; pc++) {
                const BytecodeInstruction &instr = chunk_.instructions[pc];
                switch (instr.op) {
                case BytecodeOp::StoreVarNumber:
                case BytecodeOp::StoreVarString:
                case BytecodeOp::StoreVarBoolean:
                case BytecodeOp::StoreVarColor:
                case BytecodeOp::StoreVarValue:
                case BytecodeOp::AppendVarString:
                case BytecodeOp::ChangeVarNumber:
                    if (instr.a < slotCount) procedureWrites_[p][instr.a] = true;
                    break;
                case BytecodeOp::ForEachCheck:
                    if (instr.b < slotCount) procedureWrites_[p][instr.b] = true;
                    break;
                case BytecodeOp::Wait:
                case BytecodeOp::BroadcastAndWait:
                case BytecodeOp::PlaySoundUntilDone:
                case BytecodeOp::CallNativeBlocking:
                case BytecodeOp::CallNativeBlockingValue:
                case BytecodeOp::CallLegacyReporter:
                case BytecodeOp::CallLegacyStatement:
                case BytecodeOp::EscapeToLegacyThread:
                    procedureSuspends_[p] = true;
                    break;
                case BytecodeOp::Yield:
                    if (!warp || policy_.warpYieldForgets) procedureSuspends_[p] = true;
                    break;
                case BytecodeOp::CallNative:
                case BytecodeOp::CallNativeNumber:
                case BytecodeOp::CallNativeString:
                case BytecodeOp::CallNativeBoolean:
                case BytecodeOp::CallNativeColor:
                case BytecodeOp::CallNativeValue: {
                    const bool positional = instr.a < link_.positional.size() && hasPositional(link_.positional[instr.a]);
                    if (positional ? policy_.positionalCallsForget : policy_.genericNativeCallsForget) procedureSuspends_[p] = true;
                    break;
                }
                case BytecodeOp::CallProcedure:
                case BytecodeOp::CallProcedureValue:
                    if (instr.a < procedureCount) callees[p][instr.a] = true;
                    else procedureSuspends_[p] = true;
                    break;
                default:
                    break;
                }
            }

            if (block.end == 0 || block.end > chunk_.instructions.size()) continue;
            const BytecodeInstruction &last = chunk_.instructions[block.end - 1];
            if (isTerminal(last.op)) continue;
            if (hasJumpTarget(last.op) && last.a < chunk_.instructions.size()) stack.push_back(blockOfInstruction_[last.a]);
            if (!isUnconditionalJump(last.op) && block.end < chunk_.instructions.size()) stack.push_back(blockOfInstruction_[block.end]);
        }
    }

    procedureReaches_ = callees;
    for (size_t k = 0; k < procedureCount; k++) {
        for (size_t i = 0; i < procedureCount; i++) {
            if (!procedureReaches_[i][k]) continue;
            for (size_t j = 0; j < procedureCount; j++) {
                if (procedureReaches_[k][j]) procedureReaches_[i][j] = true;
            }
        }
    }
    for (size_t p = 0; p < procedureCount; p++) {
        for (size_t q = 0; q < procedureCount; q++) {
            if (!callees[p][q]) continue;
            if (!chunk_.procedures[p].runWithoutScreenRefresh && !chunk_.procedures[q].runWithoutScreenRefresh && (p == q || procedureReaches_[q][p])) {
                procedureSuspends_[p] = true;
            }
        }
    }

    for (bool changed = true; changed;) {
        changed = false;
        for (size_t p = 0; p < procedureCount; p++) {
            for (size_t q = 0; q < procedureCount; q++) {
                if (!callees[p][q]) continue;
                if (procedureSuspends_[q] && !procedureSuspends_[p]) {
                    procedureSuspends_[p] = true;
                    changed = true;
                }
                for (size_t slot = 0; slot < slotCount; slot++) {
                    if (procedureWrites_[q][slot] && !procedureWrites_[p][slot]) {
                        procedureWrites_[p][slot] = true;
                        changed = true;
                    }
                }
            }
        }
    }
}

void BytecodeAnalysis::computeEscapeTaint() {
    const size_t slotCount = chunk_.variableSlots.size();
    const size_t count = chunk_.instructions.size();
    escapeTaint_.assign(slotCount, false);

    const auto scan = [&](BytecodeIndex entryPc) {
        if (entryPc >= count) return;
        std::vector<bool> writes(slotCount, false);
        std::vector<BytecodeIndex> callees, escapes;

        std::vector<bool> seen(blocks_.size(), false);
        std::vector<uint32_t> stack{blockOfInstruction_[entryPc]};
        while (!stack.empty()) {
            const uint32_t b = stack.back();
            stack.pop_back();
            if (seen[b]) continue;
            seen[b] = true;
            const Block &block = blocks_[b];

            for (BytecodeIndex pc = block.start; pc < block.end; pc++) {
                const BytecodeInstruction &instr = chunk_.instructions[pc];
                switch (instr.op) {
                case BytecodeOp::StoreVarNumber:
                case BytecodeOp::StoreVarString:
                case BytecodeOp::StoreVarBoolean:
                case BytecodeOp::StoreVarColor:
                case BytecodeOp::StoreVarValue:
                case BytecodeOp::AppendVarString:
                case BytecodeOp::ChangeVarNumber:
                    if (instr.a < slotCount) writes[instr.a] = true;
                    break;
                case BytecodeOp::ForEachCheck:
                    if (instr.b < slotCount) writes[instr.b] = true;
                    break;
                case BytecodeOp::CallProcedure:
                case BytecodeOp::CallProcedureValue:
                    callees.push_back(instr.a);
                    break;
                case BytecodeOp::CallLegacyReporter:
                case BytecodeOp::CallLegacyStatement:
                case BytecodeOp::EscapeToLegacyThread:
                    escapes.push_back(instr.a);
                    break;
                default:
                    break;
                }
            }

            if (block.end == 0 || block.end > count) continue;
            const BytecodeInstruction &last = chunk_.instructions[block.end - 1];
            if (isTerminal(last.op)) continue;
            if (hasJumpTarget(last.op) && last.a < count) stack.push_back(blockOfInstruction_[last.a]);
            if (!isUnconditionalJump(last.op) && block.end < count) stack.push_back(blockOfInstruction_[block.end]);
        }

        if (escapes.empty()) return;

        for (size_t slot = 0; slot < slotCount; slot++) {
            if (writes[slot]) escapeTaint_[slot] = true;
        }
        for (BytecodeIndex callee : callees) {
            if (callee >= procedureWrites_.size()) continue;
            for (size_t slot = 0; slot < slotCount; slot++) {
                if (procedureWrites_[callee][slot]) escapeTaint_[slot] = true;
            }
        }
        for (BytecodeIndex escape : escapes) {
            if (escape >= chunk_.escapeWriteStart.size() || escape >= chunk_.escapeWriteCount.size()) {
                escapeTaint_.assign(slotCount, true);
                return;
            }
            for (BytecodeIndex i = 0; i < chunk_.escapeWriteCount[escape]; i++) {
                const BytecodeIndex slot = chunk_.escapeWrittenVariables[chunk_.escapeWriteStart[escape] + i];
                if (slot < slotCount) escapeTaint_[slot] = true;
            }
        }
    };

    for (const BytecodeScript &script : chunk_.scripts)
        scan(script.entryInstruction);
    for (const BytecodeProcedure &proc : chunk_.procedures)
        scan(proc.entryInstruction);
}

bool BytecodeAnalysis::run() {
    buildBlocks();
    declareVariables();
    computeProcedureEffects();
    computeEscapeTaint();
    stats_ = {};
    stats_.instructions = static_cast<uint32_t>(chunk_.instructions.size());
    stats_.blocks = static_cast<uint32_t>(blocks_.size());

    const size_t procedureCount = chunk_.procedures.size();
    params_.assign(procedureCount, {});
    returns_.assign(procedureCount, VType::Unreached);
    callSeen_.assign(procedureCount, false);
    for (size_t p = 0; p < procedureCount; p++)
        params_[p].assign(chunk_.procedures[p].argCount, VType::Unreached);

    constexpr int maxRounds = 16;
    for (int round = 0;; round++) {
        nextParams_.assign(procedureCount, {});
        for (size_t p = 0; p < procedureCount; p++)
            nextParams_[p].assign(chunk_.procedures[p].argCount, VType::Unreached);
        nextReturns_.assign(procedureCount, VType::Unreached);
        callSeen_.assign(procedureCount, false);
        written_.assign(chunk_.variableSlots.size(), 0);

        runDataflow();

        bool changed = false;
        for (size_t p = 0; p < procedureCount; p++) {
            for (size_t i = 0; i < params_[p].size(); i++) {
                const VType joined = join(params_[p][i], nextParams_[p][i]);
                if (joined != params_[p][i]) {
                    params_[p][i] = joined;
                    changed = true;
                }
            }
            const VType joined = join(returns_[p], nextReturns_[p]);
            if (joined != returns_[p]) {
                returns_[p] = joined;
                changed = true;
            }
        }
        if (!changed) break;
        if (round + 1 >= maxRounds) {
            for (auto &params : params_)
                std::fill(params.begin(), params.end(), VType::Unknown);
            std::fill(returns_.begin(), returns_.end(), VType::Unknown);
            nextParams_.assign(procedureCount, {});
            for (size_t p = 0; p < procedureCount; p++)
                nextParams_[p].assign(chunk_.procedures[p].argCount, VType::Unreached);
            nextReturns_.assign(procedureCount, VType::Unreached);
            callSeen_.assign(procedureCount, false);
            written_.assign(chunk_.variableSlots.size(), 0);
            runDataflow();
            break;
        }
    }

    for (size_t slot = 0; slot < escapeTaint_.size() && slot < written_.size(); slot++) {
        if (escapeTaint_[slot] && declared_[slot] == VType::Unknown) written_[slot] |= typeBit(VType::Unknown);
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
    readFacts_.assign(chunk_.instructions.size(), VType::Unreached);
    for (const Block &block : blocks_) {
        if (!block.reached || block.entry < 0 || !entries_[block.entry].ok) continue;
        stats_.reachable += block.end - block.start;

        State state = block.in;
        std::string error;
        for (BytecodeIndex pc = block.start; pc < block.end; pc++) {
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            switch (instr.op) {
            case BytecodeOp::PushVarNumber:
            case BytecodeOp::PushVarString:
            case BytecodeOp::PushVarBoolean:
            case BytecodeOp::PushVarColor:
            case BytecodeOp::PushVarValue:
            case BytecodeOp::ChangeVarNumber:
            case BytecodeOp::LetterOfVar:
            case BytecodeOp::LengthOfVar:
                if (instr.a < state.variables.size()) readFacts_[pc] = state.variables[instr.a];
                break;
            default:
                break;
            }
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
                if (block.procedure >= 0 && policy_.interprocedural && instr.a < params_[block.procedure].size()) {
                    const VType t = params_[block.procedure][instr.a];
                    if (t != VType::Unknown && t != VType::Unreached) stats_.pushArgKnown++;
                }
                break;
            case BytecodeOp::CallProcedure:
            case BytecodeOp::CallProcedureValue:
                stats_.procedureCalls++;
                if (policy_.interprocedural && instr.a < procedureSuspends_.size() && !procedureSuspends_[instr.a]) stats_.procedureCallsKeepingFacts++;
                if (instr.op == BytecodeOp::CallProcedureValue) {
                    stats_.procedureValueCalls++;
                    if (policy_.interprocedural && instr.a < returns_.size() && returns_[instr.a] != VType::Unknown && returns_[instr.a] != VType::Unreached) stats_.procedureValueCallsTyped++;
                }
                break;
            case BytecodeOp::PushVarNumber:
            case BytecodeOp::PushVarString:
            case BytecodeOp::PushVarBoolean:
            case BytecodeOp::ChangeVarNumber: {
                if (instr.a >= declared_.size() || declared_[instr.a] != VType::Unknown) break;
                stats_.demotedAccesses++;
                const VType wanted = instr.op == BytecodeOp::PushVarString    ? VType::String
                                     : instr.op == BytecodeOp::PushVarBoolean ? VType::Boolean
                                                                              : VType::Number;
                if (instr.a < state.variables.size()) {
                    const VType fact = state.variables[instr.a];
                    const bool compatible = wanted == VType::Number   ? isNumberLike(fact)
                                            : wanted == VType::String ? (fact == VType::String || isNumberLike(fact))
                                                                      : fact == wanted;
                    if (compatible) stats_.demotedAccessesKnown++;
                }
                break;
            }
            default:
                break;
            }
            if (instr.op == BytecodeOp::StoreVarValue && instr.a < declared_.size() && declared_[instr.a] == VType::Unknown) {
                stats_.demotedAccesses++;
                if (!state.values.empty() && state.values.back() != VType::Unknown) stats_.demotedAccessesKnown++;
            }
            if (!step(state, instr, block.warp, block.procedure, error)) break;
        }
    }
}

bool BytecodeAnalysis::isReachable(BytecodeIndex pc) const {
    if (pc >= chunk_.instructions.size()) return false;
    const Block &block = blocks_[blockOfInstruction_[pc]];
    return block.reached && block.entry >= 0 && entries_[block.entry].ok;
}

bool BytecodeAnalysis::isBlockStart(BytecodeIndex pc) const {
    return pc < chunk_.instructions.size() && blocks_[blockOfInstruction_[pc]].start == pc;
}

bool BytecodeAnalysis::stateBefore(BytecodeIndex pc, BytecodeAbstractState &out) const {
    if (!isReachable(pc)) return false;
    const Block &block = blocks_[blockOfInstruction_[pc]];
    out = block.in;
    std::string error;
    for (BytecodeIndex i = block.start; i < pc; i++) {
        if (!step(out, chunk_.instructions[i], block.warp, block.procedure, error)) return false;
    }
    return true;
}

BytecodeValueType BytecodeAnalysis::variableTypeBefore(BytecodeIndex pc, BytecodeIndex variableSlot) const {
    if (pc >= chunk_.instructions.size()) return VType::Unreached;
    const Block &block = blocks_[blockOfInstruction_[pc]];
    if (!block.reached) return VType::Unreached;

    State state = block.in;
    std::string error;
    for (BytecodeIndex i = block.start; i < pc; i++) {
        if (!step(state, chunk_.instructions[i], block.warp, block.procedure, error)) return VType::Unreached;
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

BytecodeProgramAnalysis::BytecodeProgramAnalysis(std::vector<Input> inputs, Environment environment, BytecodeAnalysisPolicy policy)
    : inputs_(std::move(inputs)), environment_(std::move(environment)), policy_(policy) {}

namespace {

uint8_t valueTypeBit(const Value &value) {
    if (isCanonicalNumber(value)) return BytecodeAnalysis::typeBit(BytecodeValueType::NumericString);
    if (value.isDouble()) return BytecodeAnalysis::typeBit(BytecodeValueType::Number);
    if (value.isString()) return BytecodeAnalysis::typeBit(BytecodeValueType::String);
    if (value.isBoolean()) return BytecodeAnalysis::typeBit(BytecodeValueType::Boolean);
    if (value.isColor()) return BytecodeAnalysis::typeBit(BytecodeValueType::Color);
    return BytecodeAnalysis::typeBit(BytecodeValueType::Unknown);
}

const std::string &variableIdOf(const BytecodeChunk &chunk, size_t slot) {
    static const std::string empty;
    if (slot >= chunk.variableSlots.size() || chunk.variableSlots[slot] >= chunk.strings.size()) return empty;
    return chunk.strings[chunk.variableSlots[slot]];
}

} // namespace

bool BytecodeProgramAnalysis::run() {
    const uint8_t unknownBit = BytecodeAnalysis::typeBit(BytecodeValueType::Unknown);
    variableTypes_.clear();

    for (const Input &input : inputs_) {
        const BytecodeChunk &chunk = *input.chunk;
        for (size_t slot = 0; slot < chunk.variableSlots.size(); slot++) {
            const std::string &id = variableIdOf(chunk, slot);
            const Variable *var = slot < input.link->variables.size() ? input.link->variables[slot] : nullptr;

            uint8_t bits = 0;
            if (var == nullptr) {
                bits = unknownBit;
            } else {
                switch (var->value.index()) {
                case 1:
                    bits = BytecodeAnalysis::typeBit(BytecodeValueType::Number);
                    break;
                case 2:
                    bits = BytecodeAnalysis::typeBit(BytecodeValueType::String);
                    break;
                case 3:
                    bits = BytecodeAnalysis::typeBit(BytecodeValueType::Boolean);
                    break;
                default:
                    if (const Value *initial = std::get_if<Value>(&var->value)) bits = valueTypeBit(*initial);
                    else bits = unknownBit;
                    if (environment_.customExtensions) bits |= unknownBit;
                    if (environment_.sliderVariables.count(id) != 0) bits |= BytecodeAnalysis::typeBit(BytecodeValueType::Number);
                    break;
                }
#ifdef ENABLE_CLOUDVARS
                if (var->cloud) bits |= unknownBit;
#endif
            }
            variableTypes_[id] |= bits;
        }
    }

    constexpr int maxRounds = 32;
    bool allOk = true;
    for (rounds_ = 1;; rounds_++) {
        analyses_.clear();
        allOk = true;
        for (const Input &input : inputs_) {
            const BytecodeChunk &chunk = *input.chunk;
            std::vector<BytecodeValueType> seed(chunk.variableSlots.size(), BytecodeValueType::Unknown);
            for (size_t slot = 0; slot < seed.size(); slot++) {
                seed[slot] = BytecodeAnalysis::collapse(variableTypes_[variableIdOf(chunk, slot)]);
            }
            auto analysis = std::make_unique<BytecodeAnalysis>(chunk, *input.link, policy_, std::move(seed));
            allOk = analysis->run() && allOk;
            analyses_.push_back(std::move(analysis));
        }

        if (!allOk) {
            for (auto &entry : variableTypes_)
                entry.second = unknownBit;
            if (rounds_ > maxRounds) break;
        }

        bool changed = false;
        for (size_t i = 0; i < inputs_.size(); i++) {
            const BytecodeChunk &chunk = *inputs_[i].chunk;
            const std::vector<uint8_t> &written = analyses_[i]->writtenTypes();
            for (size_t slot = 0; slot < written.size(); slot++) {
                uint8_t &bits = variableTypes_[variableIdOf(chunk, slot)];
                if ((bits | written[slot]) != bits) {
                    bits |= written[slot];
                    changed = true;
                }
            }
        }
        if (!changed) break;
        if (rounds_ >= maxRounds) {
            for (auto &entry : variableTypes_)
                entry.second = unknownBit;
        }
    }
    return allOk;
}

BytecodeAnalysisStats BytecodeProgramAnalysis::totalStats() const {
    BytecodeAnalysisStats total;
    for (const auto &analysis : analyses_) {
        const BytecodeAnalysisStats &s = analysis->stats();
        total.instructions += s.instructions;
        total.reachable += s.reachable;
        total.blocks += s.blocks;
        total.pushVarValue += s.pushVarValue;
        total.pushVarValueKnown += s.pushVarValueKnown;
        total.storeVarValue += s.storeVarValue;
        total.storeVarValueKnown += s.storeVarValueKnown;
        total.convertValueToTyped += s.convertValueToTyped;
        total.convertValueToTypedKnown += s.convertValueToTypedKnown;
        total.convertValueToTypedIdentity += s.convertValueToTypedIdentity;
        total.pushArg += s.pushArg;
        total.pushArgKnown += s.pushArgKnown;
        total.demotedAccesses += s.demotedAccesses;
        total.demotedAccessesKnown += s.demotedAccessesKnown;
        total.procedureCalls += s.procedureCalls;
        total.procedureCallsKeepingFacts += s.procedureCallsKeepingFacts;
        total.procedureValueCalls += s.procedureValueCalls;
        total.procedureValueCallsTyped += s.procedureValueCallsTyped;
    }
    return total;
}

std::string BytecodeProgramAnalysis::variableReport() const {
    uint32_t single[7] = {0, 0, 0, 0, 0, 0, 0};
    uint32_t mixed = 0;
    std::ostringstream mixedList;
    for (const auto &[id, bits] : variableTypes_) {
        const BytecodeValueType type = BytecodeAnalysis::collapse(bits);
        if (type == BytecodeValueType::Unknown) {
            mixed++;
            mixedList << "\n    " << id << ":";
            for (BytecodeValueType t : {BytecodeValueType::Number, BytecodeValueType::String, BytecodeValueType::Boolean, BytecodeValueType::Color, BytecodeValueType::Unknown}) {
                if (bits & BytecodeAnalysis::typeBit(t)) mixedList << " " << (t == BytecodeValueType::Unknown ? "other" : typeName(t));
            }
        } else {
            single[static_cast<int>(type) - 1]++;
        }
    }
    std::ostringstream out;
    out << variableTypes_.size() << " variables: " << single[0] << " number, " << single[1] << " string, " << single[2] << " boolean, "
        << single[3] << " color, " << single[4] << " numeric string, " << single[5] << " number-like, " << mixed << " with several types or unknown writers (" << rounds_ << " rounds)";
    out << mixedList.str();
    return out.str();
}
