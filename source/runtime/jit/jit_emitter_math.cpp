#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit_emitter.hpp"
#include "jit_planner.hpp"
#include "jit_types.hpp"
#include "runtime.hpp"
#include "runtime/data/blueprint.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitInlineRounding(Rounding kind, uint32_t slot, std::function<void()> general) {
    loadF(lir::FR0, slot);
    lir::Jump *unrepresentable;
    if constexpr (lir::floatToWordOverflowsToMinimum) {
        lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R0, 0, lir::FR0, 0);
        unrepresentable = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, static_cast<lir::Word>(static_cast<lir::UWord>(1) << (sizeof(lir::Word) * 8 - 1)));
    } else {
        lir::emitFOp1(c_, lir::ABS_F64, lir::FR1, 0, lir::FR0, 0);
        lir::emitFSet64(c_, lir::FR2, wide ? 4503599627370496.0 : 1073741824.0);
        unrepresentable = lir::emitFCmp(c_, lir::UNORDERED_OR_GREATER_EQUAL, lir::FR1, 0, lir::FR2, 0);
        lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R0, 0, lir::FR0, 0);
    }
    lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, lir::FR1, 0, lir::R0, 0);
    if (kind == Rounding::Ceiling) {
        lir::Jump *exact = lir::emitFCmp(c_, lir::ORDERED_GREATER_EQUAL, lir::FR1, 0, lir::FR0, 0);
        lir::emitFSet64(c_, lir::FR2, 1.0);
        lir::emitFOp2(c_, lir::ADD_F64, lir::FR1, 0, lir::FR1, 0, lir::FR2, 0);
        lir::setLabel(exact, lir::emitLabel(c_));
    } else {
        lir::Jump *exact = lir::emitFCmp(c_, lir::ORDERED_LESS_EQUAL, lir::FR1, 0, lir::FR0, 0);
        lir::emitFSet64(c_, lir::FR2, 1.0);
        lir::emitFOp2(c_, lir::SUB_F64, lir::FR1, 0, lir::FR1, 0, lir::FR2, 0);
        lir::setLabel(exact, lir::emitLabel(c_));
        if (kind == Rounding::Round) {
            lir::emitFOp2(c_, lir::SUB_F64, lir::FR2, 0, lir::FR0, 0, lir::FR1, 0);
            lir::emitFSet64(c_, lir::FR3, 0.5);
            lir::Jump *down = lir::emitFCmp(c_, lir::ORDERED_LESS, lir::FR2, 0, lir::FR3, 0);
            lir::emitFSet64(c_, lir::FR2, 1.0);
            lir::emitFOp2(c_, lir::ADD_F64, lir::FR1, 0, lir::FR1, 0, lir::FR2, 0);
            lir::setLabel(down, lir::emitLabel(c_));
        }
    }
    lir::emitFSet64(c_, lir::FR2, 0.0);
    lir::Jump *zero = lir::emitFCmp(c_, lir::ORDERED_EQUAL, lir::FR1, 0, lir::FR2, 0);
    storeF(slot, lir::FR1);
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, unrepresentable, zero, done, general]() {
        lir::Label *here = lir::emitLabel(c_);
        lir::setLabel(unrepresentable, here);
        lir::setLabel(zero, here);
        general();
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
}

bool Emitter::emitInlineMotion(const BytecodeInstruction &instr, NativeShape shape) {
    enum Kind { ChangeX,
                ChangeY,
                SetX,
                SetY,
                GoTo } kind;
    const std::string &name = plan_.opcodeName(instr.a);
    if (shape == NativeShape::VoidNumber) {
        if (name == "motion_changexby") kind = ChangeX;
        else if (name == "motion_changeyby") kind = ChangeY;
        else if (name == "motion_setx") kind = SetX;
        else if (name == "motion_sety") kind = SetY;
        else return false;
    } else if (name == "motion_gotoxy") {
        kind = GoTo;
    } else {
        return false;
    }

    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t first = shape == NativeShape::VoidNumber ? state_.numbers - 1 : state_.numbers - 2;
    const uint32_t second = state_.numbers - 1;

    code_.doubles.push_back(Scratch::projectWidth * 0.3);
    const double *limitX = &code_.doubles.back();
    code_.doubles.push_back(Scratch::projectHeight * 0.3);
    const double *limitY = &code_.doubles.back();

    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), offsetof(Context, sprite));
    std::vector<lir::Jump *> slow;
    lir::emitOp1(c_, lir::MOV_U8, lir::R1, 0, lir::MEM1(lir::R0), offsetof(Sprite, isStage));
    slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R1, 0, lir::IMM, 0));
    lir::emitOp1(c_, lir::MOV_U8, lir::R1, 0, lir::MEM1(lir::R0), offsetof(Sprite, penData.down));
    slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R1, 0, lir::IMM, 0));

    const lir::Word xOffset = offsetof(Sprite, xPosition), yOffset = offsetof(Sprite, yPosition);
    switch (kind) {
    case ChangeX:
        loadF(lir::FR1, first);
        lir::emitFOp2(c_, lir::ADD_F64, lir::FR0, 0, lir::MEM1(lir::R0), xOffset, lir::FR1, 0);
        lir::emitFOp1(c_, lir::MOV_F64, lir::FR1, 0, lir::MEM1(lir::R0), yOffset);
        break;
    case ChangeY:
        loadF(lir::FR1, first);
        lir::emitFOp2(c_, lir::ADD_F64, lir::FR1, 0, lir::MEM1(lir::R0), yOffset, lir::FR1, 0);
        lir::emitFOp1(c_, lir::MOV_F64, lir::FR0, 0, lir::MEM1(lir::R0), xOffset);
        break;
    case SetX:
        loadF(lir::FR0, first);
        lir::emitFOp1(c_, lir::MOV_F64, lir::FR1, 0, lir::MEM1(lir::R0), yOffset);
        break;
    case SetY:
        loadF(lir::FR1, first);
        lir::emitFOp1(c_, lir::MOV_F64, lir::FR0, 0, lir::MEM1(lir::R0), xOffset);
        break;
    case GoTo:
        loadF(lir::FR0, first);
        loadF(lir::FR1, second);
        break;
    }

    lir::emitFOp1(c_, lir::ABS_F64, lir::FR2, 0, lir::FR0, 0);
    slow.push_back(lir::emitFCmp(c_, lir::UNORDERED_OR_GREATER_EQUAL, lir::FR2, 0, lir::MEM0(), reinterpret_cast<lir::Word>(limitX)));
    lir::emitFOp1(c_, lir::ABS_F64, lir::FR2, 0, lir::FR1, 0);
    slow.push_back(lir::emitFCmp(c_, lir::UNORDERED_OR_GREATER_EQUAL, lir::FR2, 0, lir::MEM0(), reinterpret_cast<lir::Word>(limitY)));

    lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), xOffset, lir::FR0, 0);
    lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), yOffset, lir::FR1, 0);
    lir::emitOp1(c_, lir::MOV_U8, lir::R1, 0, lir::MEM1(lir::R0), offsetof(Sprite, visible));
    lir::Jump *hidden = lir::emitCmp(c_, lir::EQUAL, lir::R1, 0, lir::IMM, 0);
    lir::emitOp1(c_, lir::MOV_U8, lir::MEM0(), reinterpret_cast<lir::Word>(&Scratch::forceRedraw), lir::IMM, 1);
    lir::setLabel(hidden, lir::emitLabel(c_));
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, slow, done, shape, first, second, &fn]() {
        const lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, const_cast<lir::Label *>(here));
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), offsetof(Context, sprite));
        loadF(lir::FR0, first);
        if (shape == NativeShape::VoidNumber) {
            callHelper(reinterpret_cast<void *>(fn.n_void), LIR_ARGS2V(P, F64));
        } else {
            loadF(lir::FR1, second);
            callHelper(reinterpret_cast<void *>(fn.nn_void), LIR_ARGS3V(P, F64, F64));
        }
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
    return true;
}

bool Emitter::emitConstantMod(const BytecodeInstruction &instr, double divisor) {
    if (!wide || !constantModDivisor(divisor)) return false;
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    const uint64_t d = static_cast<uint64_t>(std::fabs(divisor));
    const uint64_t magic = UINT64_C(0xFFFFFFFFFFFFFFFF) / d + 1;
    const bool negativeDivisor = divisor < 0;
    const lir::Int source = readF(a, lir::FR0);
    const lir::Int out = destF(a, lir::FR0);

    lir::Jump *shadowDone = nullptr;
    std::vector<lir::Jump *> notShadow;
    if (shadowSupported && !negativeDivisor && active_ != nullptr && active_->shadowSlot >= 0 && currentPc_ >= 2 && !analysis_.isBlockStart(currentPc_) && !analysis_.isBlockStart(currentPc_ - 1) &&
        chunk_.instructions[currentPc_ - 2].op == BytecodeOp::PushVarNumber) {
        const int64_t source = plan_.promotedVariable(chunk_.instructions[currentPc_ - 2]);
        if (source >= 0 && static_cast<size_t>(source) < promotedIndex_.size() && promotedIndex_[source] >= 0 && variableSlot(static_cast<uint32_t>(promotedIndex_[source])) == static_cast<uint32_t>(active_->shadowSlot)) {
            lir::emitOp2U(c_, lir::SUB | lir::SET_OVERFLOW, lir::S1, 0, lir::IMM, 1);
            notShadow.push_back(lir::emitJump(c_, lir::OVERFLOW_SET));
            notShadow.push_back(lir::emitCmp(c_, lir::SIG_LESS_EQUAL, lir::S1, 0, lir::IMM, 0));
            notShadow.push_back(lir::emitCmp(c_, lir::SIG_GREATER, lir::S1, 0, lir::IMM, 0x7fffffff));
            lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::S1, 0);
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, static_cast<lir::Word>(magic));
            lir::emitOp0(c_, lir::LMUL_UW);
            lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(d));
            lir::emitOp0(c_, lir::LMUL_UW);
            lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, out, 0, lir::R1, 0);
            shadowDone = lir::emitJump(c_, lir::JUMP);
            lir::Label *generic = lir::emitLabel(c_);
            for (lir::Jump *jump : notShadow)
                lir::setLabel(jump, generic);
        }
    }
    std::vector<lir::Jump *> slow;
    lir::emitFOp1(c_, lir::CONV_S32_FROM_F64, lir::R0, 0, source, 0);
    lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, lir::FR2, 0, lir::R0, 0);
    slow.push_back(lir::emitFCmp(c_, lir::UNORDERED_OR_NOT_EQUAL, source, 0, lir::FR2, 0));

    lir::emitOp1(c_, lir::MOV_S32, lir::R0, 0, lir::R0, 0);
    lir::Jump *negative = negativeDivisor ? lir::emitJump(c_, lir::JUMP) : lir::emitCmp(c_, lir::SIG_LESS_EQUAL, lir::R0, 0, lir::IMM, 0);
    lir::emitOp1(c_, lir::MOV_U32, lir::R1, 0, lir::R0, 0);
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, static_cast<lir::Word>(magic));
    lir::emitOp0(c_, lir::LMUL_UW);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(d));
    lir::emitOp0(c_, lir::LMUL_UW);
    lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, out, 0, lir::R1, 0);
    lir::Label *joined = lir::emitLabel(c_);
    if (shadowDone != nullptr) lir::setLabel(shadowDone, joined);
    commitF(a, out);

    addCold([this, slow, negative, joined, a, b, &fn, magic, d, divisor, negativeDivisor, source, out]() {
        lir::setLabel(negative, lir::emitLabel(c_));
        // m = a >> 31 is 0 or -1 and |a| = (a ^ m) - m, all in the integer unit
        lir::emitOp2(c_, lir::ASHR32, lir::R2, 0, lir::R0, 0, lir::IMM, 31);
        lir::emitOp2(c_, lir::XOR32, lir::R1, 0, lir::R0, 0, lir::R2, 0);
        lir::emitOp2(c_, lir::SUB32, lir::R1, 0, lir::R1, 0, lir::R2, 0);
        lir::emitOp1(c_, lir::MOV_U32, lir::R1, 0, lir::R1, 0);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, static_cast<lir::Word>(magic));
        lir::emitOp0(c_, lir::LMUL_UW);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(d));
        lir::emitOp0(c_, lir::LMUL_UW);
        lir::Jump *zero = lir::emitCmp(c_, lir::EQUAL, lir::R1, 0, lir::IMM, 0);
        // the result for a negative a is -r, or d - r when the signs of a and the divisor agree: ((r ^ m) - m) + (divisor & mask)
        lir::emitOp2(c_, lir::XOR, lir::R1, 0, lir::R1, 0, lir::R2, 0);
        lir::emitOp2(c_, lir::SUB, lir::R1, 0, lir::R1, 0, lir::R2, 0);
        if (negativeDivisor) lir::emitOp2(c_, lir::XOR, lir::R0, 0, lir::R2, 0, lir::IMM, -1);
        else lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::R2, 0);
        lir::emitOp2(c_, lir::AND, lir::R0, 0, lir::R0, 0, lir::IMM, static_cast<lir::Word>(divisor));
        lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R1, 0, lir::R0, 0);
        lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, out, 0, lir::R1, 0);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), joined);
        // a multiple of the divisor: zero with the sign of a
        lir::setLabel(zero, lir::emitLabel(c_));
        lir::emitFSet64(c_, lir::FR3, 0.0);
        lir::emitFOp2(c_, lir::MUL_F64, out, 0, source, 0, lir::FR3, 0);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), joined);

        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        storeImmF(b, divisor);
        loadF(lir::FR0, a);
        loadF(lir::FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), LIR_ARGS2(F64, F64, F64));
        storeF(a, lir::FR0);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), joined);
    });
    return true;
}

void Emitter::emitInlineMod(const BytecodeInstruction &instr) {
    if (currentPc_ > 0 && !analysis_.isBlockStart(currentPc_) && chunk_.instructions[currentPc_ - 1].op == BytecodeOp::PushConstNumber) {
        if (emitConstantMod(instr, chunk_.numberConstants[chunk_.instructions[currentPc_ - 1].a])) return;
    }
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    loadF(lir::FR0, a);
    loadF(lir::FR1, b);

    std::vector<lir::Jump *> slow;
    lir::emitFOp1(c_, lir::CONV_S32_FROM_F64, lir::R0, 0, lir::FR0, 0);
    lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, lir::FR2, 0, lir::R0, 0);
    slow.push_back(lir::emitFCmp(c_, lir::UNORDERED_OR_NOT_EQUAL, lir::FR0, 0, lir::FR2, 0));
    lir::emitFOp1(c_, lir::CONV_S32_FROM_F64, lir::R1, 0, lir::FR1, 0);
    lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, lir::FR2, 0, lir::R1, 0);
    slow.push_back(lir::emitFCmp(c_, lir::UNORDERED_OR_NOT_EQUAL, lir::FR1, 0, lir::FR2, 0));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL | lir::OP32, lir::R1, 0, lir::IMM, 0));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL | lir::OP32, lir::R1, 0, lir::IMM, -1));

    lir::emitOp1(c_, lir::MOV32, lir::R2, 0, lir::R1, 0);
    lir::emitOp0(c_, lir::DIVMOD_S32);
    lir::Jump *zero = lir::emitCmp(c_, lir::EQUAL | lir::OP32, lir::R1, 0, lir::IMM, 0);
    lir::emitOp2(c_, lir::XOR | lir::OP32, lir::R0, 0, lir::R1, 0, lir::R2, 0);
    lir::Jump *sameSign = lir::emitCmp(c_, lir::SIG_GREATER_EQUAL | lir::OP32, lir::R0, 0, lir::IMM, 0);
    lir::emitOp2(c_, lir::ADD | lir::OP32, lir::R1, 0, lir::R1, 0, lir::R2, 0);
    lir::setLabel(sameSign, lir::emitLabel(c_));
    lir::emitFOp1(c_, lir::CONV_F64_FROM_S32, lir::FR0, 0, lir::R1, 0);
    lir::Jump *stored = lir::emitJump(c_, lir::JUMP);

    lir::setLabel(zero, lir::emitLabel(c_));
    lir::emitFSet64(c_, lir::FR2, 0.0);
    lir::emitFOp2(c_, lir::MUL_F64, lir::FR0, 0, lir::FR0, 0, lir::FR2, 0);
    lir::setLabel(stored, lir::emitLabel(c_));
    lir::Label *done = lir::emitLabel(c_);
    storeF(a, lir::FR0);

    addCold([this, slow, done, a, b, &fn]() {
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        loadF(lir::FR0, a);
        loadF(lir::FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), LIR_ARGS2(F64, F64, F64));
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
}

} // namespace BytecodeJitInternal

#endif
