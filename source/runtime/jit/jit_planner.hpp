#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include "renderer.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <log.hpp>
#include <map>
#include <memory>
#include <set>
#include "ir/lir_caps.hpp"
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal {

inline bool isJump(BytecodeOp op) {
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

inline bool isCall(BytecodeOp op) {
    return op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue;
}

inline bool isTerminal(BytecodeOp op) {
    return op == BytecodeOp::Halt || op == BytecodeOp::Return || op == BytecodeOp::EscapeToLegacyThread;
}

enum class NativeShape {
    None,
    Add,
    Subtract,
    Multiply,
    Divide,
    NumberNumber,
    Number,
    SpriteNumber,
    And,
    Or,
    Not,
    BoolBool,
    BoolUnary,
    SpriteBoolean,
    VoidNone,
    VoidNumber,
    VoidNumberNumber,
    VoidColor,
    ValueBoolean,
    VoidValue,
    StringStringString,
    StringStringBool,
    StringNumber,
    NumberStringString,
    SpriteStringNumber,
    SpriteStringBool,
    SpriteString,
    VoidString,
    VoidStringNumber,
};

struct Planner {
    const BytecodeChunk &chunk;
    const BytecodeLinkTable &link;
    BytecodeAnalysis &analysis;
    size_t count;

    BytecodeIndex regionHead = invalidBytecodeIndex, regionTail = invalidBytecodeIndex;
    std::vector<uint8_t> supported;
    std::vector<uint8_t> needsExit;
    std::vector<int32_t> entryOf;
    std::vector<BytecodeIndex> entryPcs;
    std::unordered_map<uint8_t, uint32_t> loopBlockers;
    std::map<std::pair<int, uint8_t>, uint32_t> procedureBlockers;
    std::vector<uint8_t> visible;
    std::vector<std::vector<BytecodeIndex>> entryReads;

    static constexpr uint32_t minRegionInstructions = 40;

    Planner(const BytecodeChunk &c, const BytecodeLinkTable &l, BytecodeAnalysis &a)
        : chunk(c), link(l), analysis(a), count(c.instructions.size()) {}

    const std::string &opcodeName(BytecodeIndex slot) const {
        static const std::string empty;
        if (slot >= chunk.opcodeSlots.size() || chunk.opcodeSlots[slot] >= chunk.strings.size()) return empty;
        return chunk.strings[chunk.opcodeSlots[slot]];
    }

    NativeShape nativeShape(const BytecodeInstruction &instr) const {
        const BytecodeIndex slot = instr.a;
        if (slot >= chunk.paramKindsStart.size() || slot >= link.positional.size()) return NativeShape::None;
        const BytecodeIndex start = chunk.paramKindsStart[slot];
        const BytecodeIndex args = chunk.paramKindsCount[slot];
        const BytecodePositionalFn &fn = link.positional[slot];
        const auto kind = [&](BytecodeIndex i) { return chunk.paramKinds[start + i]; };
        using K = BytecodeStackKind;

        if (instr.op == BytecodeOp::CallNativeNumber) {
            if (args == 2 && kind(0) == K::Number && kind(1) == K::Number && fn.nn_n) {
                const std::string &name = opcodeName(slot);
                if (name == "operator_add") return NativeShape::Add;
                if (name == "operator_subtract") return NativeShape::Subtract;
                if (name == "operator_multiply") return NativeShape::Multiply;
                if (name == "operator_divide") return NativeShape::Divide;
                return NativeShape::NumberNumber;
            }
            if (args == 1 && kind(0) == K::Number && fn.n_n) return NativeShape::Number;
            if (args == 0 && fn.sprite_n) return NativeShape::SpriteNumber;
            if (args == 1 && kind(0) == K::String && fn.s_n) return NativeShape::StringNumber;
            if (args == 1 && kind(0) == K::String && fn.spriteS_n) return NativeShape::SpriteStringNumber;
        } else if (instr.op == BytecodeOp::CallNativeString) {
            if (args == 2 && kind(0) == K::String && kind(1) == K::String && fn.ss_s) return NativeShape::StringStringString;
            if (args == 2 && kind(0) == K::Number && kind(1) == K::String && fn.ns_s) return NativeShape::NumberStringString;
            if (args == 0 && fn.sprite_s) return NativeShape::SpriteString;
        } else if (instr.op == BytecodeOp::CallNativeBoolean) {
            if (args == 2 && kind(0) == K::Boolean && kind(1) == K::Boolean && fn.bb_b) {
                const std::string &name = opcodeName(slot);
                if (name == "operator_and") return NativeShape::And;
                if (name == "operator_or") return NativeShape::Or;
                return NativeShape::BoolBool;
            }
            if (args == 1 && kind(0) == K::Boolean && fn.b_b) {
                return opcodeName(slot) == "operator_not" ? NativeShape::Not : NativeShape::BoolUnary;
            }
            if (args == 0 && fn.sprite_b) return NativeShape::SpriteBoolean;
            if (args == 2 && kind(0) == K::String && kind(1) == K::String && fn.ss_b) return NativeShape::StringStringBool;
            if (args == 1 && kind(0) == K::Value && fn.v_b) return NativeShape::ValueBoolean;
            if (args == 1 && kind(0) == K::String && fn.spriteS_b) return NativeShape::SpriteStringBool;
        } else if (instr.op == BytecodeOp::CallNative && voidCallAllowed(opcodeName(slot))) {
            if (args == 1 && kind(0) == K::String && fn.s_void) return NativeShape::VoidString;
            if (args == 2 && kind(0) == K::String && kind(1) == K::Number && fn.sn_void) return NativeShape::VoidStringNumber;
            if (args == 0 && fn.voidFn) return NativeShape::VoidNone;
            if (args == 1 && kind(0) == K::Number && fn.n_void) return NativeShape::VoidNumber;
            if (args == 2 && kind(0) == K::Number && kind(1) == K::Number && fn.nn_void) return NativeShape::VoidNumberNumber;
            if (args == 1 && kind(0) == K::Value && fn.v_void) return NativeShape::VoidValue;
            if (args == 1 && kind(0) == K::Color && fn.c_void) return NativeShape::VoidColor;
        }
        return NativeShape::None;
    }

    static bool voidCallAllowed(const std::string &opcode) {
        for (const char *prefix : {"motion_", "looks_", "pen_", "music_", "sound_"}) {
            if (opcode.rfind(prefix, 0) == 0) return true;
        }
        return (opcode == "event_broadcast") || opcode == "control_incr_counter" || opcode == "control_clear_counter" || opcode == "sensing_resettimer";
    }

    bool followsConstant(BytecodeIndex pc) const {
        return pc > 0 && !analysis.isBlockStart(pc) && chunk.instructions[pc - 1].op == BytecodeOp::PushConstValue &&
               chunk.instructions[pc - 1].a < chunk.constants.size();
    }

    bool topValueIsNumber(BytecodeIndex pc) const {
        BytecodeAbstractState state;
        if (!analysis.stateBefore(pc, state) || state.values.empty()) return false;
        return state.values.back() == BytecodeValueType::Number;
    }

    static bool numericLike(BytecodeValueType type) {
        return type == BytecodeValueType::Number || type == BytecodeValueType::NumericString || type == BytecodeValueType::NumberLike;
    }

    bool topValuesAreNumeric(BytecodeIndex pc, size_t how_many) const {
        BytecodeAbstractState state;
        if (!analysis.stateBefore(pc, state) || state.values.size() < how_many) return false;
        for (size_t i = 0; i < how_many; i++) {
            if (!numericLike(state.values[state.values.size() - 1 - i])) return false;
        }
        return true;
    }

    bool consumedInBlock(BytecodeIndex pc, int64_t variable, int positionOffset = 0) const {
        BytecodeAbstractState pushed;
        if (!analysis.stateBefore(pc, pushed)) return false;
        if (static_cast<int>(pushed.values.size()) + positionOffset < 0) return false;
        const size_t position = pushed.values.size() + positionOffset;
        for (BytecodeIndex q = pc + 1; q < count; q++) {
            const BytecodeInstruction &between = chunk.instructions[q - 1];
            if (variable >= 0) {
                if (between.op == BytecodeOp::ItemOfList || (between.op == BytecodeOp::StoreVarValue && between.a == static_cast<BytecodeIndex>(variable))) {
                    BytecodeAbstractState before;
                    if (analysis.stateBefore(q - 1, before) && before.values.size() == position + 1) return true;
                }
                switch (between.op) {
                case BytecodeOp::StoreVarNumber:
                case BytecodeOp::StoreVarString:
                case BytecodeOp::StoreVarBoolean:
                case BytecodeOp::StoreVarColor:
                case BytecodeOp::StoreVarValue:
                case BytecodeOp::AppendVarString:
                case BytecodeOp::ChangeVarNumber:
                    if (between.a == static_cast<BytecodeIndex>(variable)) return false;
                    break;
                case BytecodeOp::ForEachCheck:
                    if (between.b == static_cast<BytecodeIndex>(variable)) return false;
                    break;
                default:
                    break;
                }
            }
            BytecodeAbstractState state;
            if (!analysis.stateBefore(q, state)) return false;
            if (state.values.size() <= position) return true;
            if (analysis.isBlockStart(q)) return false;
        }
        return false;
    }

    bool listValid(const BytecodeInstruction &instr) const { return instr.a < chunk.listSlots.size(); }

    bool stringConsumedInBlock(BytecodeIndex pc, int offset) const {
        BytecodeAbstractState before;
        if (!analysis.stateBefore(pc, before)) return false;
        const int position = static_cast<int>(before.strings) + offset;
        if (position < 0) return false;
        for (BytecodeIndex q = pc + 1; q < count; q++) {
            BytecodeAbstractState state;
            if (!analysis.stateBefore(q, state)) return false;
            if (static_cast<int>(state.strings) <= position) return true;
            if (analysis.isBlockStart(q)) return false;
        }
        return false;
    }

    bool variableBorrowable(BytecodeIndex pc) const {
        const BytecodeInstruction &push = chunk.instructions[pc];
        BytecodeAbstractState before;
        if (!analysis.stateBefore(pc, before)) return false;
        const int position = static_cast<int>(before.strings);
        for (BytecodeIndex q = pc + 1; q < count; q++) {
            BytecodeAbstractState state;
            if (!analysis.stateBefore(q, state)) return false;
            if (static_cast<int>(state.strings) <= position) return true;
            if (analysis.isBlockStart(q) || !supported[q]) return false;

            const BytecodeInstruction &instr = chunk.instructions[q];
            switch (instr.op) {
            case BytecodeOp::StoreVarNumber:
            case BytecodeOp::StoreVarString:
            case BytecodeOp::StoreVarValue:
            case BytecodeOp::AppendVarString:
            case BytecodeOp::ChangeVarNumber:
                if (instr.a == push.a) return false;
                break;
            case BytecodeOp::CallProcedure:
            case BytecodeOp::CallProcedureValue:
            case BytecodeOp::CallNative:
            case BytecodeOp::Yield:
            case BytecodeOp::ForEachCheck:
            case BytecodeOp::RepeatCheck:
                return false;
            default:
                break;
            }
        }
        return false;
    }

    static int stringArguments(NativeShape shape) {
        switch (shape) {
        case NativeShape::StringStringString:
        case NativeShape::StringStringBool:
            return 2;
        case NativeShape::StringNumber:
        case NativeShape::NumberStringString:
        case NativeShape::SpriteStringNumber:
        case NativeShape::SpriteStringBool:
        case NativeShape::VoidString:
        case NativeShape::VoidStringNumber:
            return 1;
        default:
            return 0;
        }
    }

    static bool leavesString(NativeShape shape) {
        return shape == NativeShape::StringStringString || shape == NativeShape::NumberStringString || shape == NativeShape::SpriteString;
    }

    static bool isNumericConstant(const Value &value) {
        return value.isDouble() || (value.isString() && BytecodeAnalysis::isCanonicalNumericString(value.asString()));
    }

    bool opSupported(BytecodeIndex pc) const {
        const BytecodeInstruction &instr = chunk.instructions[pc];
        const size_t variableCount = chunk.variableSlots.size();
        switch (instr.op) {
        case BytecodeOp::Nop:
        case BytecodeOp::PushConstNumber:
        case BytecodeOp::PushConstBoolean:
        case BytecodeOp::PopNumber:
        case BytecodeOp::PopBoolean:
        case BytecodeOp::ConvertNumberToValue:
        case BytecodeOp::ConvertBooleanToValue:
        case BytecodeOp::Jump:
        case BytecodeOp::JumpIfFalse:
        case BytecodeOp::JumpIfTrue:
        case BytecodeOp::NumEquals:
        case BytecodeOp::NumLess:
        case BytecodeOp::NumGreater:
        case BytecodeOp::JumpIfNumCompareFalse:
        case BytecodeOp::JumpIfNumCompareTrue:
        case BytecodeOp::MathOp:
        case BytecodeOp::RandomInt:
        case BytecodeOp::Yield:
            return true;
        case BytecodeOp::PushVarNumber:
        case BytecodeOp::StoreVarNumber:
        case BytecodeOp::ChangeVarNumber:
            return instr.a < variableCount;
        case BytecodeOp::RepeatCheck:
            return topValueIsNumber(pc);
        case BytecodeOp::ForEachCheck:
            return instr.b < variableCount && topValuesAreNumeric(pc, 2);
        case BytecodeOp::SetForEachBound:
            return topValuesAreNumeric(pc, 2);
        case BytecodeOp::PopValue:
            return true;
        case BytecodeOp::PushConstValue: {
            if (instr.a >= chunk.constants.size()) return false;
            if (chunk.constants[instr.a].isDouble()) return true;
            if (pc + 1 < count && !analysis.isBlockStart(pc + 1)) {
                const BytecodeOp next = chunk.instructions[pc + 1].op;
                if (next == BytecodeOp::ConvertValueToNumber || next == BytecodeOp::StoreVarValue) return true;
            }
            return consumedInBlock(pc, -1);
        }
        case BytecodeOp::PushVarValue:
            return instr.a < variableCount && consumedInBlock(pc, instr.a);

        case BytecodeOp::ItemOfList:
            return listValid(instr) && consumedInBlock(pc, -1, -1);
        case BytecodeOp::ReplaceInList:
        case BytecodeOp::DeleteFromList:
        case BytecodeOp::InsertInList:
            return listValid(instr);
        case BytecodeOp::AddToList:
        case BytecodeOp::ItemNumOfList:
        case BytecodeOp::ListContainsItem: {
            BytecodeAbstractState state;
            return listValid(instr) && analysis.stateBefore(pc, state) && !state.values.empty();
        }
        case BytecodeOp::LengthOfList:
        case BytecodeOp::DeleteAllOfList:
            return listValid(instr);

        case BytecodeOp::CallProcedure:
        case BytecodeOp::CallProcedureValue: {
            if (instr.a >= chunk.procedures.size()) return false;
            const BytecodeProcedure &proc = chunk.procedures[instr.a];
            BytecodeAbstractState state;
            return proc.argCount == instr.b && proc.entryInstruction < count && analysis.stateBefore(pc, state) && state.values.size() >= instr.b;
        }
        case BytecodeOp::Return: {
            BytecodeAbstractState state;
            return analysis.stateBefore(pc, state) && !state.values.empty();
        }
        case BytecodeOp::Halt:
            return true;
        case BytecodeOp::PushArg: {
            const int procedure = analysis.procedureAt(pc);
            return procedure >= 0 && instr.a < chunk.procedures[procedure].argCount && consumedInBlock(pc, -1);
        }
        case BytecodeOp::ValueEquals:
        case BytecodeOp::ValueLess:
        case BytecodeOp::ValueGreater:
        case BytecodeOp::ConvertValueToNumber:
        case BytecodeOp::ConvertValueToColor:
        case BytecodeOp::ConvertValueToBoolean:
        case BytecodeOp::Random:
        case BytecodeOp::CostumeNumberName:
            return true;
        case BytecodeOp::StoreVarBoolean:
            return instr.a < variableCount;
        case BytecodeOp::StoreVarValue:
            return instr.a < variableCount;
        case BytecodeOp::CallNative:
        case BytecodeOp::CallNativeNumber:
        case BytecodeOp::CallNativeString:
        case BytecodeOp::CallNativeBoolean: {
            const NativeShape shape = nativeShape(instr);
            if (shape == NativeShape::None) return false;
            return !leavesString(shape) || stringConsumedInBlock(pc, -stringArguments(shape));
        }

        case BytecodeOp::PushConstColor:
            return instr.a < chunk.colorConstants.size();
        case BytecodeOp::PushVarColor:
            return instr.a < chunk.variableSlots.size();
        case BytecodeOp::PushConstString:
            return instr.a < chunk.stringConstants.size() && stringConsumedInBlock(pc, 0);
        case BytecodeOp::PushVarString:
            return instr.a < variableCount && stringConsumedInBlock(pc, 0);
        case BytecodeOp::StoreVarString:
        case BytecodeOp::AppendVarString:
            return instr.a < variableCount;
        case BytecodeOp::PopString:
        case BytecodeOp::StrEquals:
        case BytecodeOp::StrLess:
        case BytecodeOp::StrGreater:
        case BytecodeOp::LengthOf:
            return true;
        case BytecodeOp::LengthOfVar:
            return instr.a < variableCount;
        case BytecodeOp::ConvertStringToValue:
            return consumedInBlock(pc, -1);
        case BytecodeOp::ConvertValueToString:
        case BytecodeOp::LetterOf:
            return stringConsumedInBlock(pc, 0);
        case BytecodeOp::LetterOfVar:
            return instr.a < variableCount && stringConsumedInBlock(pc, 0);
        default:
            return false;
        }
    }

    void plan(const std::vector<BytecodeIndex> &roots) {
        supported.assign(count, 0);
        needsExit.assign(count, 0);
        entryOf.assign(count, -1);

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (analysis.isReachable(pc)) supported[pc] = opSupported(pc) ? 1 : 0;
            if (regionHead != invalidBytecodeIndex && (pc < regionHead || pc > regionTail)) supported[pc] = 0;
        }
        limitPromotedVariables();

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (!isJump(instr.op) || instr.a >= count || instr.a > pc) continue;
            for (BytecodeIndex i = instr.a; i <= pc; i++) {
                if (analysis.isReachable(i) && !supported[i]) {
                    loopBlockers[static_cast<uint8_t>(chunk.instructions[i].op)]++;
                    procedureBlockers[{analysis.procedureAt(i), static_cast<uint8_t>(chunk.instructions[i].op)}]++;
                }
            }
        }

        for (bool changed = true; changed;) {
            changed = false;
            for (BytecodeIndex pc = 0; pc < count; pc++) {
                if (!supported[pc] || !isCall(chunk.instructions[pc].op)) continue;
                if (!supported[chunk.procedures[chunk.instructions[pc].a].entryInstruction]) {
                    supported[pc] = 0;
                    changed = true;
                }
            }
            for (BytecodeIndex pc = 0; pc < count; pc++) {
                const BytecodeInstruction &instr = chunk.instructions[pc];
                if (!isJump(instr.op) || instr.a >= count || instr.a > pc) continue;
                bool allSupported = true;
                for (BytecodeIndex i = instr.a; i <= pc; i++) {
                    if (analysis.isReachable(i) && !supported[i]) allSupported = false;
                }
                if (allSupported) continue;
                for (BytecodeIndex i = instr.a; i <= pc; i++) {
                    if (supported[i]) {
                        supported[i] = 0;
                        changed = true;
                    }
                }
            }
        }

        const auto successors = [&](BytecodeIndex pc, std::vector<BytecodeIndex> &out) {
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (isTerminal(instr.op)) return;
            if (isJump(instr.op) && instr.a < count) out.push_back(instr.a);
            if (instr.op != BytecodeOp::Jump && pc + 1 < count) out.push_back(pc + 1);
        };

        std::vector<BytecodeIndex> next;
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!analysis.isReachable(pc)) continue;
            next.clear();
            successors(pc, next);
            if (supported[pc]) {
                for (BytecodeIndex s : next) {
                    if (analysis.isReachable(s) && !supported[s]) needsExit[s] = 1;
                }
                const BytecodeInstruction &here = chunk.instructions[pc];
                if (isJump(here.op) && here.a <= pc && here.a < count && supported[here.a]) addEntry(here.a);
                if (chunk.instructions[pc].op == BytecodeOp::Yield && pc + 1 < count && supported[pc + 1]) addEntry(pc + 1);
                if (isCall(here.op) && pc + 1 < count && supported[pc + 1]) addEntry(pc + 1);
            } else {
                for (BytecodeIndex s : next) {
                    if (analysis.isReachable(s) && supported[s]) addEntry(s);
                }
            }
        }
        for (BytecodeIndex root : roots) {
            if (root < count && supported[root]) addEntry(root);
        }

        measureEntries();
    }

    void measureRegion(BytecodeIndex entry, uint32_t &instructions, bool &hasLoop, std::vector<BytecodeIndex> &reads) const {
        instructions = 0;
        hasLoop = false;
        std::vector<uint8_t> seen(count, 0);
        std::vector<BytecodeIndex> stack{entry};
        while (!stack.empty()) {
            const BytecodeIndex pc = stack.back();
            stack.pop_back();
            if (pc >= count || seen[pc] || !supported[pc]) continue;
            seen[pc] = 1;
            instructions++;
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (instr.op == BytecodeOp::PushVarNumber || instr.op == BytecodeOp::ChangeVarNumber || instr.op == BytecodeOp::PushVarValue) {
                if (std::find(reads.begin(), reads.end(), instr.a) == reads.end()) reads.push_back(instr.a);
            }
            if (isTerminal(instr.op)) continue;
            if (isJump(instr.op) && instr.a < count) {
                if (instr.a <= pc) hasLoop = true;
                stack.push_back(instr.a);
            }
            if (isCall(instr.op)) stack.push_back(chunk.procedures[instr.a].entryInstruction);
            if (instr.op != BytecodeOp::Jump) stack.push_back(pc + 1);
        }
    }

    int64_t promotedVariable(const BytecodeInstruction &instr) const {
        switch (instr.op) {
        case BytecodeOp::PushVarNumber:
        case BytecodeOp::StoreVarNumber:
        case BytecodeOp::ChangeVarNumber:
        case BytecodeOp::StoreVarValue:
        case BytecodeOp::PushVarValue:
        case BytecodeOp::PushVarString:
        case BytecodeOp::StoreVarString:
        case BytecodeOp::AppendVarString:
        case BytecodeOp::LetterOfVar:
        case BytecodeOp::LengthOfVar:
            return instr.a;
        case BytecodeOp::ForEachCheck:
            return instr.b;
        default:
            return -1;
        }
    }

    void limitPromotedVariables() {
        std::unordered_map<int64_t, uint32_t> uses;
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!supported[pc]) continue;
            const int64_t variable = promotedVariable(chunk.instructions[pc]);
            if (variable >= 0) uses[variable]++;
        }
        if (uses.size() <= maxPromoted) return;

        std::vector<int32_t> inLoop(count + 1, 0);
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (isJump(instr.op) && instr.a < count && instr.a <= pc) {
                inLoop[instr.a]++;
                inLoop[pc + 1]--;
            }
        }
        for (BytecodeIndex pc = 1; pc <= count; pc++)
            inLoop[pc] += inLoop[pc - 1];
        uses.clear();
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!supported[pc]) continue;
            const int64_t variable = promotedVariable(chunk.instructions[pc]);
            if (variable >= 0) uses[variable] += inLoop[pc] > 0 ? 16 : 1;
        }

        std::vector<std::pair<uint32_t, int64_t>> ranked;
        for (const auto &[variable, n] : uses)
            ranked.push_back({n, variable});
        std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        std::unordered_map<int64_t, bool> kept;
        for (size_t i = 0; i < maxPromoted; i++)
            kept[ranked[i].second] = true;

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!supported[pc]) continue;
            const int64_t variable = promotedVariable(chunk.instructions[pc]);
            if (variable >= 0 && !kept.count(variable)) supported[pc] = 0;
        }
    }

    void measureEntries() {
        entryReads.clear();
        visible.clear();
        for (BytecodeIndex pc : entryPcs) {
            uint32_t instructions;
            bool hasLoop;
            std::vector<BytecodeIndex> reads;
            measureRegion(pc, instructions, hasLoop, reads);
            entryReads.push_back(std::move(reads));
            visible.push_back(hasLoop || instructions >= minRegionInstructions ? 1 : 0);
        }
    }

    void addEntry(BytecodeIndex pc) {
        if (entryOf[pc] >= 0) return;
        entryOf[pc] = static_cast<int32_t>(entryPcs.size());
        entryPcs.push_back(pc);
    }
};

} // namespace BytecodeJitInternal

#endif
