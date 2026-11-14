#pragma once
#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "blockExecutor.hpp"
#include "bytecode_analysis.hpp"
#include "ir/hir.hpp"
#include "ir/lir.hpp"
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
#include <string>
#include <unordered_map>

namespace BytecodeJitInternal {

namespace lir = BytecodeJitLir;

struct AbsValue {
    ValueEntry::Kind kind = ValueEntry::Opaque;
    uint32_t constant = 0;
};

struct FrameLocation {
    enum Where : uint8_t { Slot,
                           FloatReg,
                           WordReg,
                           Immediate };
    Where where = Slot;
    lir::Int reg = 0;
    double immediate = 0.0;
    ValueEntry::Kind kind = ValueEntry::Number;
    uint32_t reference = 0;
};

struct AbsState {
    uint32_t numbers = 0;
    uint32_t booleans = 0;
    std::vector<AbsValue> values;
    std::vector<StrEntry> strings;
    std::vector<InlineFrame> frames;
};

struct InlineInfo {
    bool ok = false;
    std::vector<BytecodeJitIR::BlockId> blocks;
    uint32_t expanded = 0;
    uint32_t reserveNumbers = 0, reserveBooleans = 0, reserveValues = 0;
    bool returnsNumber = false;
    bool hasHalt = false;
};

struct InlineInstance {
    std::unordered_map<BytecodeIndex, BytecodeJitIR::BlockId> labels;
    uint32_t argBase = 0, argCount = 0;
    std::vector<AbsValue> arguments;
    AbsState prefix;
    std::vector<lir::Jump *> returns;
    bool wantsValue = false;
};

class Emitter {
  public:
    Emitter(const BytecodeChunk &chunk, const BytecodeLinkTable &link, BytecodeAnalysis &analysis, Planner &planner, Code &code, const lir::BackendOps &ops)
        : chunk_(chunk), link_(link), analysis_(analysis), plan_(planner), code_(code), ops_(ops) {}

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
    const lir::BackendOps &ops_;

    lir::Builder *c_ = nullptr;
    std::vector<lir::Label *> blockLabels_;
    std::vector<std::pair<lir::Jump *, BytecodeJitIR::BlockId>> pending_;

    struct Region {
        BytecodeIndex head = 0, tail = 0;
        std::vector<uint32_t> slots;
        int64_t counterSlot = -1;
        int64_t shadowSlot = -1;
        int64_t hoistList = -1;
        bool span = false;
        int liveHead = -1;
        lir::Label *body = nullptr;
    };
    static constexpr int firstPin = 4;
    static constexpr size_t maxPins = std::min<size_t>(10, lir::floatRegisters - firstPin - 1);
    static constexpr size_t maxSpanPins = std::min<size_t>(6, maxPins);
    static_assert(lir::floatRegisters >= firstPin + 2, "the JIT needs four scratch floating point registers, one pin and one temporary");
    static_assert(lir::savedRegisters >= 6, "the JIT keeps the context, poll counter, repeat counter, shadow and list base and limit in saved registers");
    static_assert(offsetof(Context, slots) % 8 == 0, "number slots must be naturally aligned");

    static constexpr bool nativeFrames = wide;
    static constexpr uint32_t minSpanInstructions = 4;
    std::deque<Region> regions_;
    std::vector<int32_t> regionOf_;
    const Region *active_ = nullptr;
    std::vector<std::pair<lir::Jump *, const Region *>> pendingBody_;
    bool pinViolation_ = false;
    const bool poisonPins_ = [] {
        const char *poison = std::getenv("SE_JIT_POISON");
        return poison != nullptr && std::string(poison) != "0";
    }();
    std::vector<lir::Jump *> exitJumps_;
    std::vector<lir::Jump *> leaveJumps_;
    std::vector<std::function<void()>> cold_;
    std::vector<int32_t> promotedIndex_;
    std::vector<bool> stringWritten_;
    std::vector<bool> fused_;
    BytecodeIndex fuseEnd_ = 0;

    AbsState state_;
    uint32_t maxNumbers_ = 0, maxBooleans_ = 0, maxValues_ = 0, maxStrings_ = 0;

    static constexpr lir::Word slotsOffset() { return static_cast<lir::Word>(offsetof(Context, slots)); }
    static lir::Word slotOffset(uint32_t slot) { return slotsOffset() + static_cast<lir::Word>(slot) * 8; }
    static lir::Word dirtyOffset(uint32_t k) { return lowWord(static_cast<lir::Word>(offsetof(Context, dirty)) + static_cast<lir::Word>(k) * 8); }
    static constexpr lir::Word warpOffset() { return lowWord(static_cast<lir::Word>(offsetof(Context, warp))); }

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
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), offsetof(Context, lists));
        lir::Jump *none = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::R0), static_cast<lir::Word>(active_->hoistList) * static_cast<lir::Word>(sizeof(void *)));
        lir::Jump *missing = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0);
        lir::emitOp1(c_, lir::MOV, lir::S5, 0, lir::MEM1(lir::R0), layout.items + static_cast<lir::Word>(sizeof(void *)));
        lir::emitOp1(c_, lir::MOV, lir::S4, 0, lir::MEM1(lir::R0), layout.items);
        lir::Jump *loaded = lir::emitJump(c_, lir::JUMP);
        lir::Label *zero = lir::emitLabel(c_);
        lir::setLabel(none, zero);
        lir::setLabel(missing, zero);
        lir::emitOp1(c_, lir::MOV, lir::S4, 0, lir::IMM, 0);
        lir::emitOp1(c_, lir::MOV, lir::S5, 0, lir::IMM, 0);
        lir::setLabel(loaded, lir::emitLabel(c_));
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

    bool emitFrameExit(BytecodeIndex pc, uint32_t result, const std::vector<BytecodeJitIR::ValueId> &frame, const std::function<FrameLocation(BytecodeJitIR::ValueId)> &locate);
    static long lowerLimit() {
        static const long limit = [] {
            const char *value = std::getenv("SE_JIT_LOWER");
            return value != nullptr ? std::atol(value) : 1000000L;
        }();
        return limit;
    }
    bool lowerBlock(BytecodeJitIR::BlockId blockId);
    void surveyLowering() const;
    bool testFrameExit(BytecodeIndex pc, bool conditional = false);
    uint32_t lowered_ = 0;
    std::vector<uint8_t> lowerSkip_;

    void emitEntryCounterNormalization(BytecodeIndex pc);

    bool entryOnlyRepeatAt(BytecodeIndex pc) const {
        static const bool disabled = std::getenv("SE_JIT_NOREPEATNORM") != nullptr;
        if (disabled || !wide || pc < 1 || pc >= chunk_.instructions.size() || chunk_.instructions[pc].op != BytecodeOp::RepeatCheck || chunk_.instructions[pc - 1].op != BytecodeOp::ConvertNumberToValue || !plan_.supported[pc - 1] || analysis_.isBlockStart(pc - 1)) return false;
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
    static lir::Word poolOffset(int index) { return static_cast<lir::Word>(offsetof(Context, pool)) + static_cast<lir::Word>(index) * 8; }

    static bool exactReciprocal(double c) {
        int exponent;
        return std::isfinite(c) && c != 0.0 && std::fabs(std::frexp(c, &exponent)) == 0.5 && exponent > -1000 && exponent < 1000;
    }
    static double poolValue(NativeShape shape, double c) { return shape == NativeShape::Divide && exactReciprocal(c) ? 1.0 / c : c; }
    static lir::Int poolOp(NativeShape shape, double c) {
        switch (shape) {
        case NativeShape::Add:
            return lir::ADD_F64;
        case NativeShape::Subtract:
            return lir::SUB_F64;
        case NativeShape::Multiply:
            return lir::MUL_F64;
        default:
            return exactReciprocal(c) ? lir::MUL_F64 : lir::DIV_F64;
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
            lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::S0), slotOffset(active_->slots[i]), lir::FR(firstPin + static_cast<int>(i)), 0);
        if (active_->counterSlot >= 0) {
            lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, lir::FR(firstPin + maxPins), 0, lir::S3, 0);
            lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::S0), slotOffset(static_cast<uint32_t>(active_->counterSlot)), lir::FR(firstPin + maxPins), 0);
        }
    }
    void emitReload(int live = -1) {
        if (active_ == nullptr) return;
        for (size_t i = 0; i < active_->slots.size() && (live < 0 || static_cast<int>(i) < live); i++)
            lir::emitFOp1(c_, lir::MOV_F64, lir::FR(firstPin + static_cast<int>(i)), 0, lir::MEM1(lir::S0), slotOffset(active_->slots[i]));
        if (active_->counterSlot >= 0) {
            lir::emitFOp1(c_, lir::MOV_F64, lir::FR(firstPin + maxPins), 0, lir::MEM1(lir::S0), slotOffset(static_cast<uint32_t>(active_->counterSlot)));
            lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::S3, 0, lir::FR(firstPin + maxPins), 0);
        }
        emitShadowSync();
    }

    static constexpr lir::Word shadowNone = static_cast<lir::Word>(std::numeric_limits<int64_t>::min());
    static constexpr bool shadowSupported = wide;

    void emitShadowSync() {
        if (!shadowSupported || active_ == nullptr || active_->shadowSlot < 0) return;
        const int pin = pinOf(static_cast<uint32_t>(active_->shadowSlot));
        if (pin < 0) return;
        const lir::Int variable = lir::FR(pin), probe = lir::FR(firstPin + maxPins);
        lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::S1, 0, variable, 0);
        lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, probe, 0, lir::S1, 0);
        lir::Jump *fractional = lir::emitFCmp(c_, lir::UNORDERED_OR_NOT_EQUAL, probe, 0, variable, 0);
        lir::emitOp2(c_, lir::ADD, lir::S1, 0, lir::S1, 0, lir::IMM, static_cast<lir::Word>(1) << 52);
        lir::Jump *large = lir::emitCmp(c_, lir::GREATER, lir::S1, 0, lir::IMM, static_cast<lir::Word>(1) << 53);
        lir::emitOp2(c_, lir::SUB, lir::S1, 0, lir::S1, 0, lir::IMM, static_cast<lir::Word>(1) << 52);
        lir::Jump *done = lir::emitJump(c_, lir::JUMP);
        lir::Label *none = lir::emitLabel(c_);
        lir::setLabel(fractional, none);
        lir::setLabel(large, none);
        lir::emitOp1(c_, lir::MOV, lir::S1, 0, lir::IMM, shadowNone);
        lir::setLabel(done, lir::emitLabel(c_));
    }

    void addCold(std::function<void()> fn) {
        const Region *region = active_;
        cold_.push_back([this, region, fn = std::move(fn)]() {
            active_ = region;
            fn();
            active_ = nullptr;
        });
    }

    void pendJump(lir::Jump *jump, BytecodeIndex target) {
        if (active_ != nullptr && !insideActive(target)) {
            addCold([this, jump, target]() {
                lir::setLabel(jump, lir::emitLabel(c_));
                emitSpill(liveNumbersAt(target));
                pending_.push_back({lir::emitJump(c_, lir::JUMP), labelFor(target)});
            });
        } else if (active_ != nullptr && target == active_->head) {
            pendingBody_.push_back({jump, active_});
        } else {
            pending_.push_back({jump, labelFor(target)});
        }
    }
    void pendUnconditional(BytecodeIndex target) {
        if (active_ != nullptr && !insideActive(target)) emitSpill(liveNumbersAt(target));
        pendJump(lir::emitJump(c_, lir::JUMP), target);
    }

    void loadF(lir::Int reg, uint32_t slot) {
        if (const int pin = pinOf(slot); pin >= 0) lir::emitFOp1(c_, lir::MOV_F64, reg, 0, lir::FR(pin), 0);
        else if (isCounterPin(slot)) lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, reg, 0, lir::S3, 0);
        else lir::emitFOp1(c_, lir::MOV_F64, reg, 0, lir::MEM1(lir::S0), slotOffset(slot));
    }
    void storeF(uint32_t slot, lir::Int reg) {
        if (isCounterPin(slot)) pinViolation_ = true;
        if (const int pin = pinOf(slot); pin >= 0) lir::emitFOp1(c_, lir::MOV_F64, lir::FR(pin), 0, reg, 0);
        else lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::S0), slotOffset(slot), reg, 0);
    }
    void loadW(lir::Int reg, uint32_t slot) {
        if (pinOf(slot) >= 0 || isCounterPin(slot)) pinViolation_ = true;
        lir::emitOp1(c_, lir::MOV, reg, 0, lir::MEM1(lir::S0), lowWord(slotOffset(slot)));
    }
    void storeW(uint32_t slot, lir::Int reg) {
        if (pinOf(slot) >= 0 || isCounterPin(slot)) pinViolation_ = true;
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), lowWord(slotOffset(slot)), reg, 0);
        if (!wide) lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), highWord(slotOffset(slot)), lir::IMM, 0);
    }
    void storeImmW(uint32_t slot, lir::Word value) {
        if (pinOf(slot) >= 0 || isCounterPin(slot)) pinViolation_ = true;
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), lowWord(slotOffset(slot)), lir::IMM, value);
        if (!wide) lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), highWord(slotOffset(slot)), lir::IMM, 0);
    }
    void storeFlag(uint32_t slot, lir::Int flag) {
        lir::emitOpFlags(c_, lir::MOV, lir::MEM1(lir::S0), lowWord(slotOffset(slot)), flag);
        if (!wide) lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), highWord(slotOffset(slot)), lir::IMM, 0);
    }
    void storeImmF(uint32_t slot, double value) {
        if (const int pin = pinOf(slot); pin >= 0) {
            lir::emitFSet64(c_, lir::FR(pin), value);
            return;
        }
        lir::emitFSet64(c_, lir::FR0, value);
        storeF(slot, lir::FR0);
    }

    lir::Int readF(uint32_t slot, lir::Int scratch) {
        if (const int pin = pinOf(slot); pin >= 0) return lir::FR(pin);
        loadF(scratch, slot);
        return scratch;
    }
    lir::Int destF(uint32_t slot, lir::Int scratch) {
        const int pin = pinOf(slot);
        return pin >= 0 ? lir::FR(pin) : scratch;
    }
    void commitF(uint32_t slot, lir::Int reg) {
        if (pinOf(slot) < 0) storeF(slot, reg);
    }
    lir::Int moveF(uint32_t to, uint32_t from) {
        const int pinTo = pinOf(to), pinFrom = pinOf(from);
        if (pinTo >= 0 && pinFrom >= 0) {
            lir::emitFOp1(c_, lir::MOV_F64, lir::FR(pinTo), 0, lir::FR(pinFrom), 0);
            return lir::FR(pinTo);
        }
        if (pinTo >= 0) {
            loadF(lir::FR(pinTo), from);
            return lir::FR(pinTo);
        }
        if (pinFrom >= 0) {
            storeF(to, lir::FR(pinFrom));
            return lir::FR(pinFrom);
        }
        loadF(lir::FR0, from);
        storeF(to, lir::FR0);
        return lir::FR0;
    }

    BytecodeJitIR::BuildResult hir_;
    std::vector<std::pair<uint32_t, uint32_t>> instructionAt_;
    void prepareHir();
    std::vector<InlineInfo> inlineInfo_;
    InlineInstance *inline_ = nullptr;
    bool inlineAbort_ = false;
    uint32_t inlineReserveNumbers_ = 0, inlineReserveBooleans_ = 0, inlineReserveValues_ = 0;
    void computeInlinable();
    bool inlineSite(BytecodeIndex pc, const BytecodeInstruction &instr, const CallDescriptor &call) const;
    bool emitInlineCall(BytecodeIndex pc, const BytecodeInstruction &instr, const CallDescriptor &call, std::vector<lir::Jump *> &slow);
    bool inlineEntryState(BytecodeIndex pc, AbsState &out) const;
    void emitInlineReturn(BytecodeIndex pc, bool isReturn);
    BytecodeJitIR::BlockId labelFor(BytecodeIndex pc) const;
    std::vector<BytecodeJitIR::BlockId> blockAt_;
    BytecodeJitIR::BlockId blockStartingAt(BytecodeIndex pc) const {
        if (pc >= blockAt_.size() || blockAt_[pc] == BytecodeJitIR::noBlock || hir_.function.blocks[blockAt_[pc]].startPc != pc) return BytecodeJitIR::noBlock;
        return blockAt_[pc];
    }
    const BytecodeJitIR::Instr *irInstruction(BytecodeIndex pc) const;
    bool valueNonNan(BytecodeJitIR::ValueId v) const { return v < hir_.function.values.size() && hir_.function.values[v].nonNaN; }
    bool topNumberNonNan(BytecodeIndex pc) const;
    bool boxedNumberNonNan(BytecodeIndex pc) const;

    void guardNanWrite(lir::Int reg, BytecodeIndex next, uint32_t k) {
        lir::Jump *nan = lir::emitFCmp(c_, lir::UNORDERED, reg, 0, reg, 0);
        if (code_.strictNaN[k]) {
            const AbsState exitState = state_;
            addCold([this, nan, next, exitState]() {
                lir::setLabel(nan, lir::emitLabel(c_));
                emitExitAt(next, static_cast<uint32_t>(BytecodeJit::Result::Interpret), exitState);
            });
            return;
        }
        lir::Label *back = lir::emitLabel(c_);
        addCold([this, nan, back, k]() {
            lir::setLabel(nan, lir::emitLabel(c_));
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
            lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(k));
            callHelper(reinterpret_cast<void *>(&Runtime::jitSetNaN), LIR_ARGS2V(P, W));
            lir::setLabel(lir::emitJump(c_, lir::JUMP), back);
        });
    }

    void nanToZero(lir::Int reg) {
        lir::Jump *nan = lir::emitFCmp(c_, lir::UNORDERED, reg, 0, reg, 0);
        lir::Label *back = lir::emitLabel(c_);
        addCold([this, nan, back, reg]() {
            lir::setLabel(nan, lir::emitLabel(c_));
            lir::emitFSet64(c_, reg, 0.0);
            lir::setLabel(lir::emitJump(c_, lir::JUMP), back);
        });
    }

    void callHelper(void *fn, lir::Int argTypes) {
        emitSpill();
        lir::emitICall(c_, lir::CALL, argTypes, lir::IMM, reinterpret_cast<lir::Word>(fn));
        if (poisonPins_ && active_ != nullptr) {
            for (size_t i = 0; i < active_->slots.size(); i++)
                lir::emitFSet64(c_, lir::FR(firstPin + static_cast<int>(i)), -123456.789);
            if (shadowSupported && active_->shadowSlot >= 0) lir::emitOp1(c_, lir::MOV, lir::S1, 0, lir::IMM, 123456);
        }
        emitReload();
    }
    void findRegions();

    void resetState(BytecodeIndex pc);
    bool stateUnknown_ = false;
    bool stateFromBlockParameters(BytecodeIndex pc, AbsState &out) const;
    AbsState stateAfterPop(const AbsState &s) const { return s; }
    void emitExitAt(BytecodeIndex pc, uint32_t result, const AbsState &state);
    void emitJumpTo(lir::Jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge);
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

    bool emitInlineAppend(StringOp &op);
    bool emitInlineLetterEquals(StringOp &letter, BytecodeIndex pc);

    void callStringOp(StringOp &op) {
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&op));
        void *helper = reinterpret_cast<void *>(&Runtime::jitStringOp);
        if (op.kind == StringOp::Compare) helper = reinterpret_cast<void *>(&Runtime::jitStringCompare);
        else if (op.kind == StringOp::LetterOfVariable) helper = reinterpret_cast<void *>(&Runtime::jitLetterOfVariable);
        else if (op.kind == StringOp::AppendVariable) helper = reinterpret_cast<void *>(&Runtime::jitStringAppend);
        callHelper(helper, LIR_ARGS2V(P, P));
    }

    void ensureVariableNumber(BytecodeIndex variable) {
        if (!stringWritten_[variable]) return;
        const uint32_t k = promoted(variable);
        lir::Jump *holdsString = lir::emitCmp(c_, lir::EQUAL, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 4);
        lir::Label *back = lir::emitLabel(c_);
        addCold([this, holdsString, back, k]() {
            lir::setLabel(holdsString, lir::emitLabel(c_));
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
            lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, k);
            callHelper(reinterpret_cast<void *>(&Runtime::jitEnsureStringNumber), LIR_ARGS2V(P, W));
            lir::setLabel(lir::emitJump(c_, lir::JUMP), back);
        });
    }

    void callListHelper(void *fn, ListOpDescriptor &op, lir::Int argTypes) {
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&op));
        callHelper(fn, argTypes);
    }
    void emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc);
    BytecodeIndex fr0ItemPc_ = invalidBytecodeIndex;
    void emitNativeCall(const CallDescriptor &call, BytecodeIndex target, std::vector<lir::Jump *> &slow);
    void emitNativeReturn(const ReturnDescriptor &ret, std::vector<lir::Jump *> &slow);
    enum class Rounding { Floor,
                          Ceiling,
                          Round };
    void emitInlineRounding(Rounding kind, uint32_t slot, std::function<void()> general);
    struct Operand {
        bool immediate = false;
        uint32_t slot = 0;
        double value = 0;
    };
    void loadOperand(lir::Int reg, const Operand &operand);
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
        lir::Label *label = nullptr;
        std::vector<lir::Jump *> pending;
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
    void emitLeafTest(BytecodeCompareKind kind, bool jumpIfTrue, Operand a, Operand b, const CondTarget &target, const std::vector<lir::Jump *> &notPlain = {}, const CompareDescriptor *general = nullptr);
    void emitLeafTestFloat(BytecodeCompareKind kind, bool jumpIfTrue, Operand a, Operand b, const CondTarget &target, const std::vector<lir::Jump *> &notPlain, const CompareDescriptor *general);
    static bool guardable(const AbsValue &entry, const Code &code, const BytecodeChunk &chunk);
    void routeJump(lir::Jump *jump, const CondTarget &target);
    void routeUnconditional(const CondTarget &target);
    void bindCondLabel(CondLabel &label);
    void emitNative(const BytecodeInstruction &instr, NativeShape shape);
    bool emitInlineMotion(const BytecodeInstruction &instr, NativeShape shape);
    void emitInlineMod(const BytecodeInstruction &instr);
    bool guardPlainNumber(const AbsValue &entry, uint32_t position, std::vector<lir::Jump *> &notPlain);
    void emitListItemAddress(uint32_t slot, const Operand &index, std::vector<lir::Jump *> &slow);
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
    bool emitInlineListItem(const ListOpDescriptor &op, const std::vector<lir::Jump *> &notPlain = {}, void *helper = nullptr, const Operand *index = nullptr, bool carry = false);
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
