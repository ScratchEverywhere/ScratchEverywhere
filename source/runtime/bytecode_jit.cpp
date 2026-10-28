#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <log.hpp>
#include <sljitLir.h>
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal {

constexpr uint32_t kMaxSlots = 224;
constexpr uint32_t kMaxPromoted = 48;
constexpr uint32_t kPollInterval = 64;

struct EntryDescriptor {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<uint8_t> valueIsNumber;
    std::vector<uint32_t> loadVariables;
};

struct ValueEntry {
    enum Kind : uint8_t { Opaque,
                          Number,
                          Constant };
    Kind kind = Opaque;
    uint32_t constant = 0;
};

struct ExitDescriptor {
    uint32_t pc = 0;
    uint32_t result = 0;
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<ValueEntry> values;
};

struct Code;

struct Context {
    BytecodeThread *thread = nullptr;
    const BytecodeChunk *chunk = nullptr;
    const Code *code = nullptr;
    void *sprite = nullptr;
    Timer *timer = nullptr;
    bool *timerStarted = nullptr;
    uint32_t numberBase = 0;
    uint32_t booleanBase = 0;
    uint32_t valueBase = 0;
    uint32_t poll = 0;
    uint64_t warp = 0;
    int64_t dirty[kMaxPromoted];
    double slots[kMaxSlots];
};

struct Code {
    void *machine = nullptr;
    sljit_sw (*fn)(Context *, sljit_sw) = nullptr;

    std::vector<EntryDescriptor> entries;
    std::deque<ExitDescriptor> exits;
    std::vector<void *> entryTable;
    std::vector<uint32_t> promoted;
    uint32_t slotBooleans = 0;
    uint32_t slotValues = 0;
    uint32_t slotVariables = 0;

    ~Code() {
        if (machine != nullptr) sljit_free_code(machine, nullptr);
    }
};

} // namespace BytecodeJitInternal

struct BytecodeJitCode : BytecodeJitInternal::Code {};

namespace {

using namespace BytecodeJitInternal;

Variable *variableAt(const BytecodeThread &thread, BytecodeIndex slot) {
    if (thread.link == nullptr || slot >= thread.link->variables.size()) return nullptr;
    return thread.link->variables[slot];
}

Variable *resolveOrCreateVariable(BytecodeThread &thread, BytecodeIndex slot) {
    Variable *var = variableAt(thread, slot);
    if (var != nullptr) return var;
    if (thread.sprite == nullptr || thread.link == nullptr || thread.chunk == nullptr) return nullptr;
    if (slot >= thread.chunk->variableSlots.size() || thread.chunk->variableSlots[slot] >= thread.chunk->strings.size()) return nullptr;

    const std::string &id = thread.chunk->strings[thread.chunk->variableSlots[slot]];
    Variable &created = thread.sprite->variables[id];
    created.id = id;
    created.name = id;
    const_cast<BytecodeLinkTable *>(thread.link)->variables[slot] = &created;
    return &created;
}

int64_t bitsOf(double d) {
    int64_t bits;
    std::memcpy(&bits, &d, sizeof bits);
    return bits;
}

double fromBits(int64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof d);
    return d;
}

void SLJIT_FUNC jitEnter(Context *ctx, sljit_sw entryId) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;
    const EntryDescriptor &entry = code.entries[entryId];

    if (thread.numberStack.size() < entry.numbers || thread.boolStack.size() < entry.booleans || thread.valueStack.size() < entry.valueIsNumber.size()) {
        Log::logCritical("JIT: the thread's stacks are shallower than the analysis of the bytecode says", true);
        std::abort();
    }

    ctx->numberBase = static_cast<uint32_t>(thread.numberStack.size()) - entry.numbers;
    for (uint32_t i = 0; i < entry.numbers; i++)
        ctx->slots[i] = thread.numberStack[ctx->numberBase + i];

    ctx->booleanBase = static_cast<uint32_t>(thread.boolStack.size()) - entry.booleans;
    for (uint32_t i = 0; i < entry.booleans; i++)
        ctx->slots[code.slotBooleans + i] = fromBits(thread.boolStack[ctx->booleanBase + i] ? 1 : 0);

    ctx->valueBase = static_cast<uint32_t>(thread.valueStack.size()) - static_cast<uint32_t>(entry.valueIsNumber.size());
    for (uint32_t i = 0; i < entry.valueIsNumber.size(); i++) {
        if (entry.valueIsNumber[i]) ctx->slots[code.slotValues + i] = thread.valueStack[ctx->valueBase + i].get<double>();
    }

    std::memset(ctx->dirty, 0, code.promoted.size() * sizeof(int64_t));
    for (uint32_t k : entry.loadVariables) {
        ctx->slots[code.slotVariables + k] = BlockExecutor::getVariableValueAs<double>(variableAt(thread, code.promoted[k]));
    }

    ctx->warp = thread.withoutScreenRefresh ? 1 : 0;
    ctx->poll = kPollInterval;
}

sljit_sw SLJIT_FUNC jitExit(Context *ctx, const ExitDescriptor *exit) {
    BytecodeThread &thread = *ctx->thread;
    const Code &code = *ctx->code;

    thread.numberStack.resize(ctx->numberBase + exit->numbers);
    for (uint32_t i = 0; i < exit->numbers; i++)
        thread.numberStack[ctx->numberBase + i] = ctx->slots[i];

    thread.boolStack.resize(ctx->booleanBase + exit->booleans);
    for (uint32_t i = 0; i < exit->booleans; i++)
        thread.boolStack[ctx->booleanBase + i] = bitsOf(ctx->slots[code.slotBooleans + i]) != 0;

    thread.valueStack.resize(ctx->valueBase + exit->values.size());
    for (uint32_t i = 0; i < exit->values.size(); i++) {
        const ValueEntry &entry = exit->values[i];
        if (entry.kind == ValueEntry::Number) thread.valueStack[ctx->valueBase + i] = Value(ctx->slots[code.slotValues + i]);
        else if (entry.kind == ValueEntry::Constant) thread.valueStack[ctx->valueBase + i] = ctx->chunk->constants[entry.constant];
    }

    for (uint32_t k = 0; k < code.promoted.size(); k++) {
        const int64_t kind = ctx->dirty[k];
        if (kind == 0) continue;
        Variable *var = resolveOrCreateVariable(thread, code.promoted[k]);
        if (var == nullptr) continue;
        const double value = ctx->slots[code.slotVariables + k];
        if (kind == 1) var->value = value;
        else if (kind == 2) var->value = Value(value);
        else var->value = ctx->chunk->constants[kind - 3];
    }

    thread.pc = exit->pc;
    return static_cast<sljit_sw>(exit->result);
}

double SLJIT_FUNC jitRepeatRemaining(double counter) {
    if (std::isnan(counter)) counter = 0.0;
    return std::round(counter) - 1.0;
}

sljit_sw SLJIT_FUNC jitCompareUnordered(double a, double b, sljit_sw kind) {
    const bool nan1 = std::isnan(a), nan2 = std::isnan(b);
    switch (static_cast<BytecodeCompareKind>(kind)) {
    case BytecodeCompareKind::Equals:
        return nan1 && nan2;
    case BytecodeCompareKind::Less:
        return nan2 && !nan1;
    default:
        return nan1 && !nan2;
    }
}

double SLJIT_FUNC jitMathOp(double value, sljit_sw op) {
    return bytecodeMathOpApply(static_cast<BytecodeMathOp>(op), value);
}

sljit_sw SLJIT_FUNC jitPreempt(Context *ctx) {
    if (!(Scratch::warpTimer && ctx->thread->withoutScreenRefresh)) return 0;
    if (!*ctx->timerStarted) {
        ctx->timer->start();
        *ctx->timerStarted = true;
        return 0;
    }
    return ctx->timer->getTimeMs() > 500 ? 1 : 0;
}

bool isJump(BytecodeOp op) {
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
};

struct Planner {
    const BytecodeChunk &chunk;
    const BytecodeLinkTable &link;
    BytecodeAnalysis &analysis;
    size_t count;

    std::vector<uint8_t> supported;
    std::vector<uint8_t> needsExit;
    std::vector<int32_t> entryOf;
    std::vector<BytecodeIndex> entryPcs;
    std::unordered_map<uint8_t, uint32_t> loopBlockers;
    std::vector<std::vector<BytecodeIndex>> entryReads;

    static constexpr uint32_t kMinRegionInstructions = 40;

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
        }
        return NativeShape::None;
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
        case BytecodeOp::Jump:
        case BytecodeOp::JumpIfFalse:
        case BytecodeOp::JumpIfTrue:
        case BytecodeOp::NumEquals:
        case BytecodeOp::NumLess:
        case BytecodeOp::NumGreater:
        case BytecodeOp::JumpIfNumCompareFalse:
        case BytecodeOp::JumpIfNumCompareTrue:
        case BytecodeOp::MathOp:
        case BytecodeOp::Yield:
            return true;
        case BytecodeOp::PushVarNumber:
        case BytecodeOp::StoreVarNumber:
        case BytecodeOp::ChangeVarNumber:
            return instr.a < variableCount;
        case BytecodeOp::PopValue:
        case BytecodeOp::RepeatCheck:
            return topValueIsNumber(pc);
        case BytecodeOp::PushConstValue: {
            if (pc + 1 >= count || analysis.isBlockStart(pc + 1)) return false;
            const BytecodeOp next = chunk.instructions[pc + 1].op;
            return (next == BytecodeOp::ConvertValueToNumber || next == BytecodeOp::StoreVarValue) && instr.a < chunk.constants.size();
        }
        case BytecodeOp::ConvertValueToNumber:
            return followsConstant(pc) || topValueIsNumber(pc);
        case BytecodeOp::StoreVarValue:
            return instr.a < variableCount && (followsConstant(pc) || topValueIsNumber(pc));
        case BytecodeOp::CallNativeNumber:
        case BytecodeOp::CallNativeBoolean:
            return nativeShape(instr) != NativeShape::None;
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
        }

        for (BytecodeIndex pc = 0; pc < count; pc++) {
            const BytecodeInstruction &instr = chunk.instructions[pc];
            if (!isJump(instr.op) || instr.a >= count || instr.a > pc) continue;
            for (BytecodeIndex i = instr.a; i <= pc; i++) {
                if (analysis.isReachable(i) && !supported[i]) loopBlockers[static_cast<uint8_t>(chunk.instructions[i].op)]++;
            }
        }

        for (bool changed = true; changed;) {
            changed = false;
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
            } else {
                for (BytecodeIndex s : next) {
                    if (analysis.isReachable(s) && supported[s]) addEntry(s);
                }
            }
        }
        for (BytecodeIndex root : roots) {
            if (root < count && supported[root]) addEntry(root);
        }

        keepWorthwhileEntries();
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
            if (instr.op == BytecodeOp::PushVarNumber || instr.op == BytecodeOp::ChangeVarNumber) {
                if (std::find(reads.begin(), reads.end(), instr.a) == reads.end()) reads.push_back(instr.a);
            }
            if (isTerminal(instr.op)) continue;
            if (isJump(instr.op) && instr.a < count) {
                if (instr.a <= pc) hasLoop = true;
                stack.push_back(instr.a);
            }
            if (instr.op != BytecodeOp::Jump) stack.push_back(pc + 1);
        }
    }

    void keepWorthwhileEntries() {
        std::vector<BytecodeIndex> kept;
        std::vector<std::vector<BytecodeIndex>> keptReads;
        for (BytecodeIndex pc : entryPcs) {
            uint32_t instructions;
            bool hasLoop;
            std::vector<BytecodeIndex> reads;
            measureRegion(pc, instructions, hasLoop, reads);
            if (!hasLoop && instructions < kMinRegionInstructions) continue;
            kept.push_back(pc);
            keptReads.push_back(std::move(reads));
        }
        std::fill(entryOf.begin(), entryOf.end(), -1);
        for (size_t i = 0; i < kept.size(); i++)
            entryOf[kept[i]] = static_cast<int32_t>(i);
        entryPcs = std::move(kept);
        entryReads = std::move(keptReads);
    }

    void addEntry(BytecodeIndex pc) {
        if (entryOf[pc] >= 0) return;
        entryOf[pc] = static_cast<int32_t>(entryPcs.size());
        entryPcs.push_back(pc);
    }
};

struct AbsValue {
    ValueEntry::Kind kind = ValueEntry::Opaque;
    uint32_t constant = 0;
};

struct AbsState {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<AbsValue> values;
};

class Emitter {
  public:
    Emitter(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, Planner &planner, Code &code)
        : chunk_(chunk), link_(link), analysis_(analysis), plan_(planner), code_(code) {}

    bool emit();

  private:
    const BytecodeChunk &chunk_;
    const BytecodeLinkTable &link_;
    BytecodeAnalysis &analysis_;
    Planner &plan_;
    Code &code_;

    sljit_compiler *c_ = nullptr;
    std::vector<sljit_label *> labels_;
    std::vector<std::pair<sljit_jump *, BytecodeIndex>> pending_;
    std::vector<sljit_jump *> exitJumps_;
    std::vector<std::function<void()>> cold_;
    std::vector<int32_t> promotedIndex_;

    AbsState state_;
    uint32_t maxNumbers_ = 0, maxBooleans_ = 0, maxValues_ = 0;

    static constexpr sljit_sw slotsOffset() { return static_cast<sljit_sw>(offsetof(Context, slots)); }
    static sljit_sw slotOffset(uint32_t slot) { return slotsOffset() + static_cast<sljit_sw>(slot) * 8; }
    static sljit_sw dirtyOffset(uint32_t k) { return static_cast<sljit_sw>(offsetof(Context, dirty)) + static_cast<sljit_sw>(k) * 8; }

    uint32_t numberSlot(uint32_t depth) const { return depth; }
    uint32_t booleanSlot(uint32_t depth) const { return code_.slotBooleans + depth; }
    uint32_t valueSlot(uint32_t depth) const { return code_.slotValues + depth; }
    uint32_t variableSlot(uint32_t k) const { return code_.slotVariables + k; }

    void loadF(sljit_s32 reg, uint32_t slot) { sljit_emit_fop1(c_, SLJIT_MOV_F64, reg, 0, SLJIT_MEM1(SLJIT_S0), slotOffset(slot)); }
    void storeF(uint32_t slot, sljit_s32 reg) { sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), reg, 0); }
    void loadW(sljit_s32 reg, uint32_t slot) { sljit_emit_op1(c_, SLJIT_MOV, reg, 0, SLJIT_MEM1(SLJIT_S0), slotOffset(slot)); }
    void storeW(uint32_t slot, sljit_s32 reg) { sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), reg, 0); }
    void storeImmW(uint32_t slot, sljit_sw value) { sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), SLJIT_IMM, value); }
    void storeImmF(uint32_t slot, double value) {
        sljit_emit_fset64(c_, SLJIT_FR0, value);
        storeF(slot, SLJIT_FR0);
    }

    void nanToZero(sljit_s32 reg) {
        sljit_jump *notNan = sljit_emit_fcmp(c_, SLJIT_ORDERED_EQUAL, reg, 0, reg, 0);
        sljit_emit_fset64(c_, reg, 0.0);
        sljit_set_label(notNan, sljit_emit_label(c_));
    }

    void callHelper(void *fn, sljit_s32 argTypes) { sljit_emit_icall(c_, SLJIT_CALL, argTypes, SLJIT_IMM, reinterpret_cast<sljit_sw>(fn)); }

    void resetStateFromAnalysis(BytecodeIndex pc);
    AbsState stateAfterPop(const AbsState &s) const { return s; }
    void emitExitAt(BytecodeIndex pc, uint32_t result, const AbsState &state);
    void emitJumpTo(sljit_jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge);
    void emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget);
    void emitInstruction(BytecodeIndex pc);
    void emitCompareValue(const BytecodeInstruction &instr);
    void emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc);
    void emitNative(const BytecodeInstruction &instr, NativeShape shape);
    ExitDescriptor &makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state);
    uint32_t promoted(BytecodeIndex variable) const { return static_cast<uint32_t>(promotedIndex_[variable]); }
    double constantNumber(BytecodeIndex index) const { return chunk_.constants[index].get<double>(); }
};

void Emitter::resetStateFromAnalysis(BytecodeIndex pc) {
    BytecodeAbstractState s;
    analysis_.stateBefore(pc, s);
    state_.numbers = s.numbers;
    state_.booleans = s.booleans;
    state_.values.clear();
    for (BytecodeValueType type : s.values) {
        AbsValue v;
        v.kind = type == BytecodeValueType::Number ? ValueEntry::Number : ValueEntry::Opaque;
        state_.values.push_back(v);
    }
}

ExitDescriptor &Emitter::makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state) {
    ExitDescriptor &d = code_.exits.emplace_back();
    d.pc = pc;
    d.result = result;
    d.numbers = state.numbers;
    d.booleans = state.booleans;
    for (const AbsValue &v : state.values) {
        ValueEntry e;
        e.kind = v.kind;
        e.constant = v.constant;
        d.values.push_back(e);
    }
    return d;
}

void Emitter::emitExitAt(BytecodeIndex pc, uint32_t result, const AbsState &state) {
    ExitDescriptor &d = makeExit(pc, result, state);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&d));
    exitJumps_.push_back(sljit_emit_jump(c_, SLJIT_JUMP));
}

void Emitter::emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget) {
    sljit_emit_op2(c_, SLJIT_SUB32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, poll), SLJIT_IMM, 1);
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_MEM1(SLJIT_S0), offsetof(Context, poll), SLJIT_R0, 0);
    sljit_jump *proceed = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_MEM1(SLJIT_S0), offsetof(Context, poll), SLJIT_IMM, kPollInterval);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&jitPreempt), SLJIT_ARGS1(W, P));
    sljit_jump *keepGoing = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    emitExitAt(target, static_cast<uint32_t>(BytecodeJit::Result::Yield), stateAtTarget);
    sljit_label *go = sljit_emit_label(c_);
    sljit_set_label(proceed, go);
    sljit_set_label(keepGoing, go);
    pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), target});
}

void Emitter::emitJumpTo(sljit_jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge) {
    if (!backEdge) {
        pending_.push_back({jump, target});
        return;
    }
    cold_.push_back([this, jump, target, stateAtTarget]() {
        sljit_set_label(jump, sljit_emit_label(c_));
        emitBackEdgeThenJump(target, stateAtTarget);
    });
}

void Emitter::emitCompareValue(const BytecodeInstruction &instr) {
    const BytecodeCompareKind kind = instr.op == BytecodeOp::NumEquals ? BytecodeCompareKind::Equals
                                     : instr.op == BytecodeOp::NumLess ? BytecodeCompareKind::Less
                                                                       : BytecodeCompareKind::Greater;
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    const uint32_t out = booleanSlot(state_.booleans);
    loadF(SLJIT_FR0, a);
    loadF(SLJIT_FR1, b);
    sljit_jump *unordered = sljit_emit_fcmp(c_, SLJIT_UNORDERED, SLJIT_FR0, 0, SLJIT_FR1, 0);

    const sljit_s32 setFlag = kind == BytecodeCompareKind::Equals ? SLJIT_SET_F_EQUAL : kind == BytecodeCompareKind::Less ? SLJIT_SET_F_LESS
                                                                                                                          : SLJIT_SET_F_GREATER;
    const sljit_s32 flag = kind == BytecodeCompareKind::Equals ? SLJIT_F_EQUAL : kind == BytecodeCompareKind::Less ? SLJIT_F_LESS
                                                                                                                   : SLJIT_F_GREATER;
    sljit_emit_fop1(c_, SLJIT_CMP_F64 | setFlag, SLJIT_FR0, 0, SLJIT_FR1, 0);
    sljit_emit_op_flags(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(out), flag);
    sljit_label *done = sljit_emit_label(c_);

    cold_.push_back([this, unordered, done, kind, out]() {
        sljit_set_label(unordered, sljit_emit_label(c_));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(kind));
        callHelper(reinterpret_cast<void *>(&jitCompareUnordered), SLJIT_ARGS3(W, F64, F64, W));
        storeW(out, SLJIT_R0);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
}

void Emitter::emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc) {
    const BytecodeCompareKind kind = static_cast<BytecodeCompareKind>(instr.b);
    const bool jumpIfTrue = instr.op == BytecodeOp::JumpIfNumCompareTrue;
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    state_.numbers -= 2;
    const AbsState atTarget = state_;
    const bool backEdge = instr.a <= pc;

    loadF(SLJIT_FR0, a);
    loadF(SLJIT_FR1, b);
    sljit_jump *unordered = sljit_emit_fcmp(c_, SLJIT_UNORDERED, SLJIT_FR0, 0, SLJIT_FR1, 0);

    sljit_s32 type;
    switch (kind) {
    case BytecodeCompareKind::Equals:
        type = jumpIfTrue ? SLJIT_ORDERED_EQUAL : SLJIT_ORDERED_NOT_EQUAL;
        break;
    case BytecodeCompareKind::Less:
        type = jumpIfTrue ? SLJIT_ORDERED_LESS : SLJIT_ORDERED_GREATER_EQUAL;
        break;
    default:
        type = jumpIfTrue ? SLJIT_ORDERED_GREATER : SLJIT_ORDERED_LESS_EQUAL;
        break;
    }
    emitJumpTo(sljit_emit_fcmp(c_, type, SLJIT_FR0, 0, SLJIT_FR1, 0), instr.a, atTarget, backEdge);
    sljit_label *fall = sljit_emit_label(c_);

    cold_.push_back([this, unordered, fall, kind, jumpIfTrue, atTarget, backEdge, target = instr.a]() {
        sljit_set_label(unordered, sljit_emit_label(c_));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(kind));
        callHelper(reinterpret_cast<void *>(&jitCompareUnordered), SLJIT_ARGS3(W, F64, F64, W));
        sljit_jump *no = sljit_emit_cmp(c_, jumpIfTrue ? SLJIT_EQUAL : SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        if (backEdge) {
            emitBackEdgeThenJump(target, atTarget);
        } else {
            pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), target});
        }
        sljit_set_label(no, fall);
    });
}

void Emitter::emitNative(const BytecodeInstruction &instr, NativeShape shape) {
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    switch (shape) {
    case NativeShape::Add:
    case NativeShape::Subtract:
    case NativeShape::Multiply:
    case NativeShape::Divide: {
        const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        const sljit_s32 op = shape == NativeShape::Add ? SLJIT_ADD_F64 : shape == NativeShape::Subtract ? SLJIT_SUB_F64
                                                                     : shape == NativeShape::Multiply   ? SLJIT_MUL_F64
                                                                                                        : SLJIT_DIV_F64;
        sljit_emit_fop2(c_, op, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(a, SLJIT_FR0);
        state_.numbers -= 1;
        break;
    }
    case NativeShape::NumberNumber: {
        const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), SLJIT_ARGS2(F64, F64, F64));
        storeF(a, SLJIT_FR0);
        state_.numbers -= 1;
        break;
    }
    case NativeShape::Number: {
        const uint32_t a = numberSlot(state_.numbers - 1);
        loadF(SLJIT_FR0, a);
        callHelper(reinterpret_cast<void *>(fn.n_n), SLJIT_ARGS1(F64, F64));
        storeF(a, SLJIT_FR0);
        break;
    }
    case NativeShape::SpriteNumber: {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.sprite_n), SLJIT_ARGS1(F64, P));
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers += 1;
        break;
    }
    case NativeShape::And:
    case NativeShape::Or: {
        const uint32_t a = booleanSlot(state_.booleans - 2), b = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        loadW(SLJIT_R1, b);
        sljit_emit_op2(c_, shape == NativeShape::And ? SLJIT_AND : SLJIT_OR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R1, 0);
        storeW(a, SLJIT_R0);
        state_.booleans -= 1;
        break;
    }
    case NativeShape::Not: {
        const uint32_t a = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        sljit_emit_op2(c_, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
        storeW(a, SLJIT_R0);
        break;
    }
    case NativeShape::BoolBool: {
        const uint32_t a = booleanSlot(state_.booleans - 2), b = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        loadW(SLJIT_R1, b);
        callHelper(reinterpret_cast<void *>(fn.bb_b), SLJIT_ARGS2(32, 32, 32));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R0, 0);
        storeW(a, SLJIT_R0);
        state_.booleans -= 1;
        break;
    }
    case NativeShape::BoolUnary: {
        const uint32_t a = booleanSlot(state_.booleans - 1);
        loadW(SLJIT_R0, a);
        callHelper(reinterpret_cast<void *>(fn.b_b), SLJIT_ARGS1(32, 32));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R0, 0);
        storeW(a, SLJIT_R0);
        break;
    }
    case NativeShape::SpriteBoolean: {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.sprite_b), SLJIT_ARGS1(32, P));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_R0, 0);
        storeW(booleanSlot(state_.booleans), SLJIT_R0);
        state_.booleans += 1;
        break;
    }
    case NativeShape::None:
        break;
    }
}

void Emitter::emitInstruction(BytecodeIndex pc) {
    const BytecodeInstruction &instr = chunk_.instructions[pc];
    switch (instr.op) {
    case BytecodeOp::Nop:
        return;

    case BytecodeOp::PushConstNumber:
        storeImmF(numberSlot(state_.numbers), chunk_.numberConstants[instr.a]);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    case BytecodeOp::PushConstBoolean:
        storeImmW(booleanSlot(state_.booleans), chunk_.boolConstants[instr.a] ? 1 : 0);
        state_.booleans++;
        return;
    case BytecodeOp::PopNumber:
        state_.numbers--;
        return;
    case BytecodeOp::PopBoolean:
        state_.booleans--;
        return;
    case BytecodeOp::PopValue:
        state_.values.pop_back();
        return;

    case BytecodeOp::PushVarNumber: {
        loadF(SLJIT_FR0, variableSlot(promoted(instr.a)));
        nanToZero(SLJIT_FR0);
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers++;
        return;
    }
    case BytecodeOp::StoreVarNumber: {
        const uint32_t k = promoted(instr.a);
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        storeF(variableSlot(k), SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.numbers--;
        return;
    }
    case BytecodeOp::ChangeVarNumber: {
        const uint32_t k = promoted(instr.a);
        loadF(SLJIT_FR0, variableSlot(k));
        nanToZero(SLJIT_FR0);
        loadF(SLJIT_FR1, numberSlot(state_.numbers - 1));
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(variableSlot(k), SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.numbers--;
        return;
    }

    case BytecodeOp::PushConstValue: {
        AbsValue v;
        v.kind = ValueEntry::Constant;
        v.constant = instr.a;
        state_.values.push_back(v);
        return;
    }
    case BytecodeOp::ConvertNumberToValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        storeF(valueSlot(depth), SLJIT_FR0);
        AbsValue v;
        v.kind = ValueEntry::Number;
        state_.values.push_back(v);
        state_.numbers--;
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }
    case BytecodeOp::ConvertValueToNumber: {
        const AbsValue top = state_.values.back();
        state_.values.pop_back();
        if (top.kind == ValueEntry::Constant) {
            storeImmF(numberSlot(state_.numbers), constantNumber(top.constant));
        } else {
            loadF(SLJIT_FR0, valueSlot(static_cast<uint32_t>(state_.values.size())));
            nanToZero(SLJIT_FR0);
            storeF(numberSlot(state_.numbers), SLJIT_FR0);
        }
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }
    case BytecodeOp::StoreVarValue: {
        const uint32_t k = promoted(instr.a);
        const AbsValue top = state_.values.back();
        state_.values.pop_back();
        if (top.kind == ValueEntry::Constant) {
            storeImmF(variableSlot(k), constantNumber(top.constant));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 3 + static_cast<sljit_sw>(top.constant));
        } else {
            loadF(SLJIT_FR0, valueSlot(static_cast<uint32_t>(state_.values.size())));
            storeF(variableSlot(k), SLJIT_FR0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 2);
        }
        return;
    }

    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater:
        emitCompareValue(instr);
        state_.numbers -= 2;
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;

    case BytecodeOp::JumpIfNumCompareFalse:
    case BytecodeOp::JumpIfNumCompareTrue:
        emitCompareJump(instr, pc);
        return;

    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue: {
        loadW(SLJIT_R0, booleanSlot(state_.booleans - 1));
        state_.booleans--;
        const bool backEdge = instr.a <= pc;
        sljit_jump *j = sljit_emit_cmp(c_, instr.op == BytecodeOp::JumpIfTrue ? SLJIT_NOT_EQUAL : SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        emitJumpTo(j, instr.a, state_, backEdge);
        return;
    }
    case BytecodeOp::Jump:
        if (instr.a <= pc) emitBackEdgeThenJump(instr.a, state_);
        else pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), instr.a});
        return;

    case BytecodeOp::RepeatCheck: {
        const uint32_t counter = valueSlot(static_cast<uint32_t>(state_.values.size()) - 1);
        loadF(SLJIT_FR0, counter);
        callHelper(reinterpret_cast<void *>(&jitRepeatRemaining), SLJIT_ARGS1(F64, F64));
        sljit_emit_fset64(c_, SLJIT_FR1, 0.0);
        AbsState exited = state_;
        exited.values.pop_back();
        sljit_jump *done = sljit_emit_fcmp(c_, SLJIT_ORDERED_LESS, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(counter, SLJIT_FR0);
        emitJumpTo(done, instr.a, exited, instr.a <= pc);
        return;
    }

    case BytecodeOp::MathOp:
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(instr.b));
        callHelper(reinterpret_cast<void *>(&jitMathOp), SLJIT_ARGS2(F64, F64, W));
        storeF(numberSlot(state_.numbers - 1), SLJIT_FR0);
        return;

    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeBoolean: {
        emitNative(instr, plan_.nativeShape(instr));
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::Yield: {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, warp));
        sljit_jump *warp = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        emitExitAt(pc + 1, static_cast<uint32_t>(BytecodeJit::Result::Yield), state_);
        sljit_set_label(warp, sljit_emit_label(c_));
        return;
    }

    default:
        return;
    }
}

bool Emitter::emit() {
    const size_t count = chunk_.instructions.size();

    promotedIndex_.assign(chunk_.variableSlots.size(), -1);
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        if (!plan_.supported[pc]) continue;
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        if (instr.op != BytecodeOp::PushVarNumber && instr.op != BytecodeOp::StoreVarNumber && instr.op != BytecodeOp::ChangeVarNumber && instr.op != BytecodeOp::StoreVarValue) continue;
        if (promotedIndex_[instr.a] < 0) {
            promotedIndex_[instr.a] = static_cast<int32_t>(code_.promoted.size());
            code_.promoted.push_back(instr.a);
        }
    }
    if (code_.promoted.size() > kMaxPromoted) return false;

    for (BytecodeIndex pc = 0; pc < count; pc++) {
        BytecodeAbstractState s;
        if (!analysis_.stateBefore(pc, s)) continue;
        maxNumbers_ = std::max(maxNumbers_, s.numbers + 2);
        maxBooleans_ = std::max(maxBooleans_, s.booleans + 2);
        maxValues_ = std::max(maxValues_, static_cast<uint32_t>(s.values.size()) + 2);
    }
    code_.slotBooleans = maxNumbers_;
    code_.slotValues = code_.slotBooleans + maxBooleans_;
    code_.slotVariables = code_.slotValues + maxValues_;
    if (code_.slotVariables + code_.promoted.size() > kMaxSlots) return false;

    c_ = sljit_create_compiler(nullptr);
    if (c_ == nullptr) return false;

    sljit_emit_enter(c_, 0, SLJIT_ARGS2(W, P, W), 3 | SLJIT_ENTER_FLOAT(3), 2, 0);

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S1, 0);
    callHelper(reinterpret_cast<void *>(&jitEnter), SLJIT_ARGS2V(P, W));
    code_.entryTable.assign(plan_.entryPcs.size(), nullptr);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(code_.entryTable.data()));
    sljit_emit_ijump(c_, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R1, SLJIT_S1), SLJIT_WORD_SHIFT);

    labels_.assign(count, nullptr);
    bool havePrevious = false;
    BytecodeIndex previous = 0;
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        const bool compiled = plan_.supported[pc] != 0;
        if (!compiled && !plan_.needsExit[pc]) continue;

        labels_[pc] = sljit_emit_label(c_);

        if (!(havePrevious && previous + 1 == pc && !analysis_.isBlockStart(pc))) resetStateFromAnalysis(pc);
        havePrevious = true;
        previous = pc;

        if (compiled) {
            emitInstruction(pc);
        } else {
            emitExitAt(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), state_);
            havePrevious = false;
        }
    }

    for (size_t i = 0; i < cold_.size(); i++) {
        auto emitCold = cold_[i];
        emitCold();
    }

    sljit_label *exitLabel = sljit_emit_label(c_);
    for (sljit_jump *j : exitJumps_)
        sljit_set_label(j, exitLabel);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&jitExit), SLJIT_ARGS2(W, P, P));
    sljit_emit_return(c_, SLJIT_MOV, SLJIT_R0, 0);

    for (auto &[jump, target] : pending_) {
        if (target >= count || labels_[target] == nullptr) {
            sljit_free_compiler(c_);
            c_ = nullptr;
            return false;
        }
        sljit_set_label(jump, labels_[target]);
    }

    code_.machine = sljit_generate_code(c_, 0, nullptr);
    if (code_.machine == nullptr) {
        sljit_free_compiler(c_);
        c_ = nullptr;
        return false;
    }
    code_.fn = reinterpret_cast<sljit_sw (*)(Context *, sljit_sw)>(code_.machine);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        code_.entryTable[i] = reinterpret_cast<void *>(sljit_get_label_addr(labels_[plan_.entryPcs[i]]));
    }
    sljit_free_compiler(c_);
    c_ = nullptr;
    return true;
}

} // namespace

namespace BytecodeJit {

bool enabled() {
    static const bool on = [] {
        const char *env = std::getenv("SE_JIT");
        return env != nullptr && std::string(env) != "0";
    }();
    return on;
}

void compile(const BytecodeChunk &chunk, const BytecodeLinkTable &link) {
    if (chunk.jitTried) return;
    chunk.jitTried = true;
    if (chunk.instructions.empty()) return;
    Timer compileTimer;

    BytecodeAnalysis analysis(chunk, link);
    if (!analysis.run()) return;

    std::vector<BytecodeIndex> roots;
    for (const BytecodeScript &script : chunk.scripts)
        roots.push_back(script.entryInstruction);
    for (const BytecodeProcedure &proc : chunk.procedures)
        roots.push_back(proc.entryInstruction);

    Planner planner(chunk, link, analysis);
    planner.plan(roots);
    if (planner.entryPcs.empty()) return;

    auto code = std::make_shared<BytecodeJitCode>();
    for (BytecodeIndex pc : planner.entryPcs) {
        BytecodeAbstractState s;
        analysis.stateBefore(pc, s);
        EntryDescriptor e;
        e.numbers = s.numbers;
        e.booleans = s.booleans;
        for (BytecodeValueType type : s.values)
            e.valueIsNumber.push_back(type == BytecodeValueType::Number ? 1 : 0);
        code->entries.push_back(std::move(e));
    }

    Emitter emitter(chunk, link, analysis, planner, *code);
    if (!emitter.emit()) return;

    for (size_t i = 0; i < code->entries.size(); i++) {
        for (BytecodeIndex variable : planner.entryReads[i]) {
            const auto it = std::find(code->promoted.begin(), code->promoted.end(), variable);
            if (it != code->promoted.end()) code->entries[i].loadVariables.push_back(static_cast<uint32_t>(it - code->promoted.begin()));
        }
    }

    chunk.jitEntryAt = planner.entryOf;
    chunk.jit = code;

    if (const char *log = std::getenv("SE_JIT_LOG"); log != nullptr && std::string(log) != "0") {
        size_t compiled = 0, reachable = 0;
        for (size_t pc = 0; pc < chunk.instructions.size(); pc++) {
            if (analysis.isReachable(static_cast<BytecodeIndex>(pc))) reachable++;
            if (planner.supported[pc]) compiled++;
        }
        std::string blockers;
        std::vector<std::pair<uint32_t, uint8_t>> ranked;
        for (const auto &[op, n] : planner.loopBlockers)
            ranked.push_back({n, op});
        std::sort(ranked.rbegin(), ranked.rend());
        for (size_t i = 0; i < ranked.size() && i < 12; i++)
            blockers += " " + std::to_string(ranked[i].second) + ":" + std::to_string(ranked[i].first);
        Log::log("[jit blockers]" + blockers);
        Log::log("[jit] compiled " + std::to_string(compiled) + " of " + std::to_string(reachable) + " reachable instructions, " +
                 std::to_string(code->entries.size()) + " entry points, " + std::to_string(code->promoted.size()) + " variables in slots, " +
                 std::to_string(code->exits.size()) + " exits, " + std::to_string(compileTimer.getTimeMsDouble()) + " ms");
    }
}

Result run(const BytecodeChunk &chunk, BytecodeThread &thread, uint32_t entryId, Timer &sliceTimer, bool &sliceTimerStarted) {
    Context ctx;
    ctx.thread = &thread;
    ctx.chunk = &chunk;
    ctx.code = chunk.jit.get();
    ctx.sprite = thread.sprite;
    ctx.timer = &sliceTimer;
    ctx.timerStarted = &sliceTimerStarted;
    const Code &code = *chunk.jit;
    return static_cast<Result>(code.fn(&ctx, entryId));
}

} // namespace BytecodeJit

#endif
