#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "jit_planner.hpp"
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
#include <sljitLir.h>
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal {

struct AbsValue {
    ValueEntry::Kind kind = ValueEntry::Opaque;
    uint32_t constant = 0;
};

struct AbsState {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<AbsValue> values;
    std::vector<StrEntry> strings;
};

class Emitter {
  public:
    Emitter(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, Planner &planner, Code &code)
        : chunk_(chunk), link_(link), analysis_(analysis), plan_(planner), code_(code) {}

    bool emit();

    bool giveUp(const std::string &why) const {
        if (const char *log = std::getenv("SE_JIT_LOG"); log != nullptr && std::string(log) != "0") Log::log("[jit] not compiled: " + why);
        return false;
    }

  private:
    const BytecodeChunk &chunk_;
    const BytecodeLinkTable &link_;
    BytecodeAnalysis &analysis_;
    Planner &plan_;
    Code &code_;

    sljit_compiler *c_ = nullptr;
    std::vector<sljit_label *> labels_;
    std::vector<std::pair<sljit_jump *, BytecodeIndex>> pending_;

    struct Region {
        BytecodeIndex head = 0, tail = 0;
        std::vector<uint32_t> slots;
        int64_t counterSlot = -1;
        int64_t shadowSlot = -1;
        int64_t hoistList = -1;
        bool span = false;
        int liveHead = -1;
        sljit_label *body = nullptr;
    };
    static constexpr int firstPin = 4;
    static constexpr size_t maxPins = std::min<size_t>(10, SLJIT_NUMBER_OF_FLOAT_REGISTERS - firstPin - 1);
    static constexpr size_t maxSpanPins = std::min<size_t>(6, maxPins);
    static_assert(SLJIT_NUMBER_OF_FLOAT_REGISTERS >= firstPin + 2, "the JIT needs four scratch floating point registers, one pin and one temporary");
    static_assert(SLJIT_NUMBER_OF_SAVED_REGISTERS >= 6, "the JIT keeps the context, poll counter, repeat counter, shadow and list base and limit in saved registers");
    static_assert(offsetof(Context, slots) % 8 == 0, "number slots must be naturally aligned");

    static constexpr bool nativeFrames = wide;
    static constexpr uint32_t minSpanInstructions = 4;
    std::deque<Region> regions_;
    std::vector<int32_t> regionOf_;
    const Region *active_ = nullptr;
    std::vector<std::pair<sljit_jump *, const Region *>> pendingBody_;
    bool pinViolation_ = false;
    const bool poisonPins_ = [] {
        const char *poison = std::getenv("SE_JIT_POISON");
        return poison != nullptr && std::string(poison) != "0";
    }();
    std::vector<sljit_jump *> exitJumps_;
    std::vector<sljit_jump *> leaveJumps_;
    std::vector<std::function<void()>> cold_;
    std::vector<int32_t> promotedIndex_;
    std::vector<bool> stringWritten_;
    std::vector<bool> fused_;
    BytecodeIndex fuseEnd_ = 0;

    AbsState state_;
    uint32_t maxNumbers_ = 0, maxBooleans_ = 0, maxValues_ = 0, maxStrings_ = 0;

    static constexpr sljit_sw slotsOffset() { return static_cast<sljit_sw>(offsetof(Context, slots)); }
    static sljit_sw slotOffset(uint32_t slot) { return slotsOffset() + static_cast<sljit_sw>(slot) * 8; }
    static sljit_sw dirtyOffset(uint32_t k) { return lowWord(static_cast<sljit_sw>(offsetof(Context, dirty)) + static_cast<sljit_sw>(k) * 8); }
    static constexpr sljit_sw warpOffset() { return lowWord(static_cast<sljit_sw>(offsetof(Context, warp))); }

    uint32_t numberSlot(uint32_t depth) const { return depth; }
    uint32_t booleanSlot(uint32_t depth) const { return code_.slotBooleans + depth; }
    uint32_t valueSlot(uint32_t depth) const { return code_.slotValues + depth; }
    uint32_t variableSlot(uint32_t k) const { return code_.slotVariables + k; }

    int pinOf(uint32_t slot) {
        if (active_ == nullptr) return -1;
        for (size_t i = 0; i < active_->slots.size(); i++) {
            if (active_->slots[i] == slot) return firstPin + static_cast<int>(i);
        }
        return -1;
    }
    void emitHoistLoad() {
        if (active_ == nullptr || active_->hoistList < 0) return;
        const Runtime::ListLayout &layout = Runtime::listLayout();
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, lists));
        sljit_jump *none = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), static_cast<sljit_sw>(active_->hoistList) * static_cast<sljit_sw>(sizeof(void *)));
        sljit_jump *missing = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S5, 0, SLJIT_MEM1(SLJIT_R0), layout.items + static_cast<sljit_sw>(sizeof(void *)));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S4, 0, SLJIT_MEM1(SLJIT_R0), layout.items);
        sljit_jump *loaded = sljit_emit_jump(c_, SLJIT_JUMP);
        sljit_label *zero = sljit_emit_label(c_);
        sljit_set_label(none, zero);
        sljit_set_label(missing, zero);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S5, 0, SLJIT_IMM, 0);
        sljit_set_label(loaded, sljit_emit_label(c_));
    }

    bool isCounterPin(uint32_t slot) const { return active_ != nullptr && active_->counterSlot == static_cast<int64_t>(slot); }

    bool wholeCounterAt(BytecodeIndex pc) const {
        if (!wide || pc < 2 || chunk_.instructions[pc - 1].op != BytecodeOp::ConvertNumberToValue || chunk_.instructions[pc - 2].op != BytecodeOp::PushConstNumber || analysis_.isBlockStart(pc - 1)) return false;
        const double start = chunk_.numberConstants[chunk_.instructions[pc - 2].a];
        if (!std::isfinite(start) || start != std::floor(start) || std::fabs(start) >= 4503599627370496.0) return false;
        for (BytecodeIndex q = 0; q < pc; q++) {
            if (plan_.supported[q] && isJump(chunk_.instructions[q].op) && chunk_.instructions[q].a == pc) return false;
        }
        return true;
    }

    int poolIndex(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof bits);
        static const bool disabled = std::getenv("SE_JIT_NOPOOL") != nullptr;
        if (bits == 0 || disabled) return -1;
        for (size_t i = 0; i < code_.pool.size(); i++) {
            uint64_t other;
            std::memcpy(&other, &code_.pool[i], sizeof other);
            if (other == bits) return static_cast<int>(i);
        }
        if (code_.pool.size() >= constantPoolSize) return -1;
        code_.pool.push_back(value);
        return static_cast<int>(code_.pool.size() - 1);
    }
    static sljit_sw poolOffset(int index) { return static_cast<sljit_sw>(offsetof(Context, pool)) + static_cast<sljit_sw>(index) * 8; }

    static bool exactReciprocal(double c) {
        int exponent;
        return std::isfinite(c) && c != 0.0 && std::fabs(std::frexp(c, &exponent)) == 0.5 && exponent > -1000 && exponent < 1000;
    }
    static double poolValue(NativeShape shape, double c) { return shape == NativeShape::Divide && exactReciprocal(c) ? 1.0 / c : c; }
    static sljit_s32 poolOp(NativeShape shape, double c) {
        switch (shape) {
        case NativeShape::Add:
            return SLJIT_ADD_F64;
        case NativeShape::Subtract:
            return SLJIT_SUB_F64;
        case NativeShape::Multiply:
            return SLJIT_MUL_F64;
        default:
            return exactReciprocal(c) ? SLJIT_MUL_F64 : SLJIT_DIV_F64;
        }
    }
    bool poolOperandFollows(BytecodeIndex pc) {
        if (pc + 1 >= chunk_.instructions.size() || analysis_.isBlockStart(pc + 1)) return false;
        const BytecodeInstruction &next = chunk_.instructions[pc + 1];
        if (next.op != BytecodeOp::CallNativeNumber) return false;
        const NativeShape shape = plan_.nativeShape(next);
        if (shape != NativeShape::Add && shape != NativeShape::Subtract && shape != NativeShape::Multiply && shape != NativeShape::Divide) return false;
        return poolIndex(poolValue(shape, chunk_.numberConstants[chunk_.instructions[pc].a])) >= 0;
    }

    bool shadowChangeFollows(BytecodeIndex pc) {
        if (!shadowSupported || active_ == nullptr || active_->shadowSlot < 0 || pc + 1 >= chunk_.instructions.size() || analysis_.isBlockStart(pc + 1)) return false;
        const BytecodeInstruction &next = chunk_.instructions[pc + 1];
        if (next.op != BytecodeOp::ChangeVarNumber) return false;
        const uint32_t slot = variableSlot(promoted(next.a));
        return active_->shadowSlot == static_cast<int64_t>(slot) && pinOf(slot) >= 0;
    }

    bool constantModFollows(BytecodeIndex pc) const {
        if (!wide || pc + 1 >= chunk_.instructions.size() || analysis_.isBlockStart(pc + 1)) return false;
        const BytecodeInstruction &next = chunk_.instructions[pc + 1];
        if (next.op != BytecodeOp::CallNativeNumber || plan_.nativeShape(next) != NativeShape::NumberNumber || plan_.opcodeName(next.a) != "operator_mod") return false;
        return constantModDivisor(chunk_.numberConstants[chunk_.instructions[pc].a]);
    }
    static bool constantModDivisor(double divisor) { return std::fabs(divisor) >= 2.0 && std::fabs(divisor) < 2147483648.0 && divisor == std::floor(divisor); }

    bool insideActive(BytecodeIndex target) const { return active_ != nullptr && target >= active_->head && target <= active_->tail; }

    int liveNumbersAt(BytecodeIndex pc) const {
        BytecodeAbstractState state;
        if (active_ == nullptr || !active_->span || pc >= chunk_.instructions.size() || !analysis_.stateBefore(pc, state)) return -1;
        return static_cast<int>(state.numbers);
    }

    void emitSpill(int live = -1) {
        if (active_ == nullptr) return;
        for (size_t i = 0; i < active_->slots.size() && (live < 0 || static_cast<int>(i) < live); i++)
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), slotOffset(active_->slots[i]), SLJIT_FR(firstPin + static_cast<int>(i)), 0);
        if (active_->counterSlot >= 0) {
            sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, SLJIT_FR(firstPin + maxPins), 0, SLJIT_S3, 0);
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), slotOffset(static_cast<uint32_t>(active_->counterSlot)), SLJIT_FR(firstPin + maxPins), 0);
        }
    }
    void emitReload(int live = -1) {
        if (active_ == nullptr) return;
        for (size_t i = 0; i < active_->slots.size() && (live < 0 || static_cast<int>(i) < live); i++)
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR(firstPin + static_cast<int>(i)), 0, SLJIT_MEM1(SLJIT_S0), slotOffset(active_->slots[i]));
        if (active_->counterSlot >= 0) {
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR(firstPin + maxPins), 0, SLJIT_MEM1(SLJIT_S0), slotOffset(static_cast<uint32_t>(active_->counterSlot)));
            sljit_emit_fop1(c_, SLJIT_CONV_SW_FROM_F64, SLJIT_S3, 0, SLJIT_FR(firstPin + maxPins), 0);
        }
        emitShadowSync();
    }

    static constexpr sljit_sw shadowNone = static_cast<sljit_sw>(std::numeric_limits<int64_t>::min());
    static constexpr bool shadowSupported = wide;

    void emitShadowSync() {
        if (!shadowSupported || active_ == nullptr || active_->shadowSlot < 0) return;
        const int pin = pinOf(static_cast<uint32_t>(active_->shadowSlot));
        if (pin < 0) return;
        const sljit_s32 variable = SLJIT_FR(pin), probe = SLJIT_FR(firstPin + maxPins);
        sljit_emit_fop1(c_, SLJIT_CONV_SW_FROM_F64, SLJIT_S1, 0, variable, 0);
        sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, probe, 0, SLJIT_S1, 0);
        sljit_jump *fractional = sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, probe, 0, variable, 0);
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_IMM, static_cast<sljit_sw>(1) << 52);
        sljit_jump *large = sljit_emit_cmp(c_, SLJIT_GREATER, SLJIT_S1, 0, SLJIT_IMM, static_cast<sljit_sw>(1) << 53);
        sljit_emit_op2(c_, SLJIT_SUB, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_IMM, static_cast<sljit_sw>(1) << 52);
        sljit_jump *done = sljit_emit_jump(c_, SLJIT_JUMP);
        sljit_label *none = sljit_emit_label(c_);
        sljit_set_label(fractional, none);
        sljit_set_label(large, none);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, shadowNone);
        sljit_set_label(done, sljit_emit_label(c_));
    }

    void addCold(std::function<void()> fn) {
        const Region *region = active_;
        cold_.push_back([this, region, fn = std::move(fn)]() {
            active_ = region;
            fn();
            active_ = nullptr;
        });
    }

    void pendJump(sljit_jump *jump, BytecodeIndex target) {
        if (active_ != nullptr && !insideActive(target)) {
            addCold([this, jump, target]() {
                sljit_set_label(jump, sljit_emit_label(c_));
                emitSpill(liveNumbersAt(target));
                pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), target});
            });
        } else if (active_ != nullptr && target == active_->head) {
            pendingBody_.push_back({jump, active_});
        } else {
            pending_.push_back({jump, target});
        }
    }
    void pendUnconditional(BytecodeIndex target) {
        if (active_ != nullptr && !insideActive(target)) emitSpill(liveNumbersAt(target));
        pendJump(sljit_emit_jump(c_, SLJIT_JUMP), target);
    }

    void loadF(sljit_s32 reg, uint32_t slot) {
        if (const int pin = pinOf(slot); pin >= 0) sljit_emit_fop1(c_, SLJIT_MOV_F64, reg, 0, SLJIT_FR(pin), 0);
        else if (isCounterPin(slot)) sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, reg, 0, SLJIT_S3, 0);
        else sljit_emit_fop1(c_, SLJIT_MOV_F64, reg, 0, SLJIT_MEM1(SLJIT_S0), slotOffset(slot));
    }
    void storeF(uint32_t slot, sljit_s32 reg) {
        if (isCounterPin(slot)) pinViolation_ = true;
        if (const int pin = pinOf(slot); pin >= 0) sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR(pin), 0, reg, 0);
        else sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_S0), slotOffset(slot), reg, 0);
    }
    void loadW(sljit_s32 reg, uint32_t slot) {
        if (pinOf(slot) >= 0 || isCounterPin(slot)) pinViolation_ = true;
        sljit_emit_op1(c_, SLJIT_MOV, reg, 0, SLJIT_MEM1(SLJIT_S0), lowWord(slotOffset(slot)));
    }
    void storeW(uint32_t slot, sljit_s32 reg) {
        if (pinOf(slot) >= 0 || isCounterPin(slot)) pinViolation_ = true;
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), lowWord(slotOffset(slot)), reg, 0);
        if (!wide) sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), highWord(slotOffset(slot)), SLJIT_IMM, 0);
    }
    void storeImmW(uint32_t slot, sljit_sw value) {
        if (pinOf(slot) >= 0 || isCounterPin(slot)) pinViolation_ = true;
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), lowWord(slotOffset(slot)), SLJIT_IMM, value);
        if (!wide) sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), highWord(slotOffset(slot)), SLJIT_IMM, 0);
    }
    void storeFlag(uint32_t slot, sljit_s32 flag) {
        sljit_emit_op_flags(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), lowWord(slotOffset(slot)), flag);
        if (!wide) sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), highWord(slotOffset(slot)), SLJIT_IMM, 0);
    }
    void storeImmF(uint32_t slot, double value) {
        if (const int pin = pinOf(slot); pin >= 0) {
            sljit_emit_fset64(c_, SLJIT_FR(pin), value);
            return;
        }
        sljit_emit_fset64(c_, SLJIT_FR0, value);
        storeF(slot, SLJIT_FR0);
    }

    sljit_s32 readF(uint32_t slot, sljit_s32 scratch) {
        if (const int pin = pinOf(slot); pin >= 0) return SLJIT_FR(pin);
        loadF(scratch, slot);
        return scratch;
    }
    sljit_s32 destF(uint32_t slot, sljit_s32 scratch) {
        const int pin = pinOf(slot);
        return pin >= 0 ? SLJIT_FR(pin) : scratch;
    }
    void commitF(uint32_t slot, sljit_s32 reg) {
        if (pinOf(slot) < 0) storeF(slot, reg);
    }
    sljit_s32 moveF(uint32_t to, uint32_t from) {
        const int pinTo = pinOf(to), pinFrom = pinOf(from);
        if (pinTo >= 0 && pinFrom >= 0) {
            sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR(pinTo), 0, SLJIT_FR(pinFrom), 0);
            return SLJIT_FR(pinTo);
        }
        if (pinTo >= 0) {
            loadF(SLJIT_FR(pinTo), from);
            return SLJIT_FR(pinTo);
        }
        if (pinFrom >= 0) {
            storeF(to, SLJIT_FR(pinFrom));
            return SLJIT_FR(pinFrom);
        }
        loadF(SLJIT_FR0, from);
        storeF(to, SLJIT_FR0);
        return SLJIT_FR0;
    }

    bool nonNanEnding(BytecodeIndex end) const {
        if (end >= chunk_.instructions.size() || !plan_.supported[end]) return false;
        const BytecodeInstruction &instr = chunk_.instructions[end];
        switch (instr.op) {
        case BytecodeOp::PushConstNumber:
            return !std::isnan(chunk_.numberConstants[instr.a]);
        case BytecodeOp::PushVarNumber:
            return true;
        case BytecodeOp::MathOp:
            if (instr.b != static_cast<uint32_t>(BytecodeMathOp::Abs) && instr.b != static_cast<uint32_t>(BytecodeMathOp::Floor) && instr.b != static_cast<uint32_t>(BytecodeMathOp::Ceiling)) return false;
            return end >= 1 && !analysis_.isBlockStart(end) && nonNanEnding(end - 1);
        case BytecodeOp::RandomInt:
            return end >= 2 && !analysis_.isBlockStart(end) && !analysis_.isBlockStart(end - 1) && nonNanEnding(end - 1) && nonNanEnding(end - 2);
        case BytecodeOp::CallNativeNumber: {
            const NativeShape shape = plan_.nativeShape(instr);
            if (shape != NativeShape::Add && shape != NativeShape::Subtract && shape != NativeShape::Multiply && shape != NativeShape::Divide) return false;
            if (end < 2 || analysis_.isBlockStart(end) || analysis_.isBlockStart(end - 1)) return false;
            const BytecodeInstruction &right = chunk_.instructions[end - 1];
            if (right.op != BytecodeOp::PushConstNumber) return false;
            const double constant = chunk_.numberConstants[right.a];
            if (!std::isfinite(constant) || ((shape == NativeShape::Multiply || shape == NativeShape::Divide) && constant == 0.0)) return false;
            return nonNanEnding(end - 2);
        }
        default:
            return false;
        }
    }
    bool topIsNonNan(BytecodeIndex pc) const { return pc >= 1 && !analysis_.isBlockStart(pc) && nonNanEnding(pc - 1); }

    void guardNanWrite(sljit_s32 reg, BytecodeIndex next, uint32_t k) {
        sljit_jump *nan = sljit_emit_fcmp(c_, SLJIT_UNORDERED, reg, 0, reg, 0);
        if (code_.strictNaN[k]) {
            const AbsState exitState = state_;
            addCold([this, nan, next, exitState]() {
                sljit_set_label(nan, sljit_emit_label(c_));
                emitExitAt(next, static_cast<uint32_t>(BytecodeJit::Result::Interpret), exitState);
            });
            return;
        }
        sljit_label *back = sljit_emit_label(c_);
        addCold([this, nan, back, k]() {
            sljit_set_label(nan, sljit_emit_label(c_));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, static_cast<sljit_sw>(k));
            callHelper(reinterpret_cast<void *>(&Runtime::jitSetNaN), SLJIT_ARGS2V(P, W));
            sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), back);
        });
    }

    void nanToZero(sljit_s32 reg) {
        sljit_jump *nan = sljit_emit_fcmp(c_, SLJIT_UNORDERED, reg, 0, reg, 0);
        sljit_label *back = sljit_emit_label(c_);
        addCold([this, nan, back, reg]() {
            sljit_set_label(nan, sljit_emit_label(c_));
            sljit_emit_fset64(c_, reg, 0.0);
            sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), back);
        });
    }

    void callHelper(void *fn, sljit_s32 argTypes) {
        emitSpill();
        sljit_emit_icall(c_, SLJIT_CALL, argTypes, SLJIT_IMM, reinterpret_cast<sljit_sw>(fn));
        if (poisonPins_ && active_ != nullptr) {
            for (size_t i = 0; i < active_->slots.size(); i++)
                sljit_emit_fset64(c_, SLJIT_FR(firstPin + static_cast<int>(i)), -123456.789);
            if (shadowSupported && active_->shadowSlot >= 0) sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S1, 0, SLJIT_IMM, 123456);
        }
        emitReload();
    }
    void findRegions();

    void resetStateFromAnalysis(BytecodeIndex pc);
    AbsState stateAfterPop(const AbsState &s) const { return s; }
    void emitExitAt(BytecodeIndex pc, uint32_t result, const AbsState &state);
    void emitJumpTo(sljit_jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge);
    void emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget);
    void emitInstruction(BytecodeIndex pc);
    void emitCompareValue(BytecodeCompareKind kind, uint32_t a, uint32_t b);

    static ValueEntry describe(const AbsValue &v) {
        ValueEntry e;
        e.kind = v.kind;
        e.constant = v.constant;
        return e;
    }

    StringOp &newStringOp(StringOp::Kind kind) {
        StringOp &op = code_.stringOps.emplace_back();
        op.kind = kind;
        return op;
    }

    void callStringOp(StringOp &op) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&op));
        void *helper = reinterpret_cast<void *>(&Runtime::jitStringOp);
        if (op.kind == StringOp::Compare) helper = reinterpret_cast<void *>(&Runtime::jitStringCompare);
        else if (op.kind == StringOp::LetterOfVariable) helper = reinterpret_cast<void *>(&Runtime::jitLetterOfVariable);
        else if (op.kind == StringOp::AppendVariable) helper = reinterpret_cast<void *>(&Runtime::jitStringAppend);
        callHelper(helper, SLJIT_ARGS2V(P, P));
    }

    void ensureVariableNumber(BytecodeIndex variable) {
        if (!stringWritten_[variable]) return;
        const uint32_t k = promoted(variable);
        sljit_jump *holdsString = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 4);
        sljit_label *back = sljit_emit_label(c_);
        addCold([this, holdsString, back, k]() {
            sljit_set_label(holdsString, sljit_emit_label(c_));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, k);
            callHelper(reinterpret_cast<void *>(&Runtime::jitEnsureStringNumber), SLJIT_ARGS2V(P, W));
            sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), back);
        });
    }

    void callListHelper(void *fn, ListOpDescriptor &op, sljit_s32 argTypes) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&op));
        callHelper(fn, argTypes);
    }
    void emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc);
    BytecodeIndex fr0ItemPc_ = invalidBytecodeIndex;
    void emitNativeCall(const CallDescriptor &call, BytecodeIndex target, std::vector<sljit_jump *> &slow);
    void emitNativeReturn(const ReturnDescriptor &ret, std::vector<sljit_jump *> &slow);
    enum class Rounding { Floor,
                          Ceiling,
                          Round };
    void emitInlineRounding(Rounding kind, uint32_t slot, std::function<void()> general);
    struct Operand {
        bool immediate = false;
        uint32_t slot = 0;
        double value = 0;
    };
    void loadOperand(sljit_s32 reg, const Operand &operand);
    void emitCompareBranch(BytecodeCompareKind kind, bool jumpIfTrue, BytecodeIndex target, bool backEdge, Operand a, Operand b, const AbsState &atTarget);
    void emitCompareBranchFloat(BytecodeCompareKind kind, bool jumpIfTrue, BytecodeIndex target, bool backEdge, Operand a, Operand b, const AbsState &atTarget);
    bool fedCompare(BytecodeIndex compare, Operand &a, Operand &b) const;
    bool convertFeedsStore(BytecodeIndex convert) const {
        const BytecodeIndex next = convert + 1;
        return next < chunk_.instructions.size() && chunk_.instructions[convert].op == BytecodeOp::ConvertNumberToValue && chunk_.instructions[next].op == BytecodeOp::StoreVarValue && plan_.supported[next] && !analysis_.isBlockStart(next) && plan_.entryOf[next] < 0;
    }
    bool feedsVarStore(BytecodeIndex push) const {
        const BytecodeIndex next = push + 1;
        if (next >= chunk_.instructions.size() || chunk_.instructions[next].op != BytecodeOp::StoreVarValue || !plan_.supported[next] || analysis_.isBlockStart(next) || plan_.entryOf[next] >= 0) return false;
        const int64_t source = plan_.promotedVariable(chunk_.instructions[push]);
        return source >= 0 && static_cast<size_t>(source) < promotedIndex_.size() && promotedIndex_[source] >= 0 && static_cast<uint32_t>(promotedIndex_[source]) < code_.promoted.size();
    }
    bool numberFedStore(BytecodeIndex store) const { return store > 0 && convertFeedsStore(store - 1); }
    bool feedsCompare(BytecodeIndex push) const {
        Operand a, b;
        return (push + 2 < chunk_.instructions.size() && fedCompare(push + 2, a, b)) || (push + 1 < chunk_.instructions.size() && fedCompare(push + 1, a, b));
    }
    bool fusableBranch(BytecodeIndex pc, bool &jumpIfTrue, BytecodeIndex &jumpPc) const;

    struct CondNode {
        enum Kind : uint8_t { Leaf,
                              Not,
                              And,
                              Or } kind = Leaf;
        BytecodeIndex rangeStart = 0, compare = 0;
        int a = -1, b = -1;
    };
    struct CondTree {
        std::vector<CondNode> nodes;
        int root = -1;
        BytecodeIndex jump = 0;
    };
    struct CondLabel {
        sljit_label *label = nullptr;
        std::vector<sljit_jump *> pending;
    };
    struct CondTarget {
        enum Kind : uint8_t { Fall,
                              Label,
                              Bytecode } kind = Fall;
        CondLabel *label = nullptr;
        BytecodeIndex pc = 0;
        bool backEdge = false;
        std::shared_ptr<AbsState> state;
    };
    bool suppressFusion_ = false;
    std::map<BytecodeIndex, CondTree> conditions_;
    std::vector<uint8_t> condLeaf_;
    std::deque<CondLabel> condLabels_;
    bool conditionOperandOk(BytecodeIndex pc) const;
    void buildConditions();
    bool parseCondition(BytecodeIndex jump, CondTree &tree, BytecodeIndex &start) const;
    void emitCondition(const CondTree &tree, BytecodeIndex start);
    void emitCond(const CondTree &tree, int id, const CondTarget &whenTrue, const CondTarget &whenFalse, bool fallIsTrue);
    void emitLeaf(const CondTree &tree, const CondNode &node, const CondTarget &target, bool jumpIfTrue);
    void emitLeafTest(BytecodeCompareKind kind, bool jumpIfTrue, Operand a, Operand b, const CondTarget &target, const std::vector<sljit_jump *> &notPlain = {}, const CompareDescriptor *general = nullptr);
    void emitLeafTestFloat(BytecodeCompareKind kind, bool jumpIfTrue, Operand a, Operand b, const CondTarget &target, const std::vector<sljit_jump *> &notPlain, const CompareDescriptor *general);
    static bool guardable(const AbsValue &entry, const Code &code, const BytecodeChunk &chunk);
    void routeJump(sljit_jump *jump, const CondTarget &target);
    void routeUnconditional(const CondTarget &target);
    void bindCondLabel(CondLabel &label);
    void emitNative(const BytecodeInstruction &instr, NativeShape shape);
    bool emitInlineMotion(const BytecodeInstruction &instr, NativeShape shape);
    void emitInlineMod(const BytecodeInstruction &instr);
    bool guardPlainNumber(const AbsValue &entry, uint32_t position, std::vector<sljit_jump *> &notPlain);
    void emitListItemAddress(uint32_t slot, const Operand &index, std::vector<sljit_jump *> &slow);
    bool indexFed(BytecodeIndex item, uint32_t numbersAtItem, Operand &source) const {
        if (item < 1 || !Runtime::listLayout().ok || chunk_.instructions[item].op != BytecodeOp::ItemOfList || !plan_.supported[item] || analysis_.isBlockStart(item) || plan_.entryOf[item] >= 0) return false;
        if (!plan_.topValuesAreNumeric(item, 1)) return false;
        const BytecodeInstruction &previous = chunk_.instructions[item - 1];
        if (previous.op == BytecodeOp::PushVarValue) {
            if (previous.a >= promotedIndex_.size() || promotedIndex_[previous.a] < 0 || stringWritten_[previous.a]) return false;
            source = {false, variableSlot(static_cast<uint32_t>(promotedIndex_[previous.a])), 0};
            return true;
        }
        if (previous.op == BytecodeOp::ConvertNumberToValue && plan_.supported[item - 1]) {
            source = {false, numberSlot(numbersAtItem), 0};
            return true;
        }
        return false;
    }
    bool feedsIndex(BytecodeIndex push) const {
        Operand ignored;
        return push + 1 < chunk_.instructions.size() && indexFed(push + 1, 0, ignored);
    }
    bool emitInlineListItem(const ListOpDescriptor &op, const std::vector<sljit_jump *> &notPlain = {}, void *helper = nullptr, const Operand *index = nullptr, bool carry = false);
    bool convertCarries(BytecodeIndex convert) const {
        return convert < chunk_.instructions.size() && chunk_.instructions[convert].op == BytecodeOp::ConvertValueToNumber && plan_.supported[convert] && !analysis_.isBlockStart(convert) && plan_.entryOf[convert] < 0;
    }
    bool emitInlineListReplace(const ListOpDescriptor &op, const AbsValue &item);
    bool emitInlineListAdd(const ListOpDescriptor &op, const AbsValue &item);
    bool emitConstantMod(const BytecodeInstruction &instr, double divisor);
    BytecodeIndex currentPc_ = 0;
    ExitDescriptor &makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state);
    uint32_t promoted(BytecodeIndex variable) const { return static_cast<uint32_t>(promotedIndex_[variable]); }
    double constantNumber(BytecodeIndex index) const { return chunk_.constants[index].get<double>(); }
};

} // namespace BytecodeJitInternal

#endif
