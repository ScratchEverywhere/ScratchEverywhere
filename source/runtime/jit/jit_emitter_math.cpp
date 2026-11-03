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
#include <sljitLir.h>
#include <string>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitInlineRounding(Rounding kind, uint32_t slot, std::function<void()> general) {
    loadF(SLJIT_FR0, slot);
    sljit_emit_fop1(c_, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, SLJIT_FR0, 0);
    sljit_jump *unrepresentable = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(0x8000000000000000ull));
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, SLJIT_FR1, 0, SLJIT_R0, 0);
    if (kind == Rounding::Ceiling) {
        sljit_jump *exact = sljit_emit_fcmp(c_, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR1, 0, SLJIT_FR0, 0);
        sljit_emit_fset64(c_, SLJIT_FR2, 1.0);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR2, 0);
        sljit_set_label(exact, sljit_emit_label(c_));
    } else {
        sljit_jump *exact = sljit_emit_fcmp(c_, SLJIT_ORDERED_LESS_EQUAL, SLJIT_FR1, 0, SLJIT_FR0, 0);
        sljit_emit_fset64(c_, SLJIT_FR2, 1.0);
        sljit_emit_fop2(c_, SLJIT_SUB_F64, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR2, 0);
        sljit_set_label(exact, sljit_emit_label(c_));
        if (kind == Rounding::Round) {
            sljit_emit_fop2(c_, SLJIT_SUB_F64, SLJIT_FR2, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
            sljit_emit_fset64(c_, SLJIT_FR3, 0.5);
            sljit_jump *down = sljit_emit_fcmp(c_, SLJIT_ORDERED_LESS, SLJIT_FR2, 0, SLJIT_FR3, 0);
            sljit_emit_fset64(c_, SLJIT_FR2, 1.0);
            sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR2, 0);
            sljit_set_label(down, sljit_emit_label(c_));
        }
    }
    sljit_emit_fset64(c_, SLJIT_FR2, 0.0);
    sljit_jump *zero = sljit_emit_fcmp(c_, SLJIT_ORDERED_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0);
    storeF(slot, SLJIT_FR1);
    sljit_label *done = sljit_emit_label(c_);

    addCold([this, unrepresentable, zero, done, general]() {
        sljit_label *here = sljit_emit_label(c_);
        sljit_set_label(unrepresentable, here);
        sljit_set_label(zero, here);
        general();
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
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

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
    std::vector<sljit_jump *> slow;
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), offsetof(Sprite, isStage));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0));
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), offsetof(Sprite, penData.down));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0));

    const sljit_sw xOffset = offsetof(Sprite, xPosition), yOffset = offsetof(Sprite, yPosition);
    switch (kind) {
    case ChangeX:
        loadF(SLJIT_FR1, first);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), xOffset, SLJIT_FR1, 0);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), yOffset);
        break;
    case ChangeY:
        loadF(SLJIT_FR1, first);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), yOffset, SLJIT_FR1, 0);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), xOffset);
        break;
    case SetX:
        loadF(SLJIT_FR0, first);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R0), yOffset);
        break;
    case SetY:
        loadF(SLJIT_FR1, first);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R0), xOffset);
        break;
    case GoTo:
        loadF(SLJIT_FR0, first);
        loadF(SLJIT_FR1, second);
        break;
    }

    sljit_emit_fop1(c_, SLJIT_ABS_F64, SLJIT_FR2, 0, SLJIT_FR0, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(limitX)));
    sljit_emit_fop1(c_, SLJIT_ABS_F64, SLJIT_FR2, 0, SLJIT_FR1, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_GREATER_EQUAL, SLJIT_FR2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(limitY)));

    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), xOffset, SLJIT_FR0, 0);
    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), yOffset, SLJIT_FR1, 0);
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), offsetof(Sprite, visible));
    sljit_jump *hidden = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Scratch::forceRedraw), SLJIT_IMM, 1);
    sljit_set_label(hidden, sljit_emit_label(c_));
    sljit_label *done = sljit_emit_label(c_);

    addCold([this, slow, done, shape, first, second, &fn]() {
        const sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, const_cast<sljit_label *>(here));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        loadF(SLJIT_FR0, first);
        if (shape == NativeShape::VoidNumber) {
            callHelper(reinterpret_cast<void *>(fn.n_void), SLJIT_ARGS2V(P, F64));
        } else {
            loadF(SLJIT_FR1, second);
            callHelper(reinterpret_cast<void *>(fn.nn_void), SLJIT_ARGS3V(P, F64, F64));
        }
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
    return true;
}

bool Emitter::emitConstantMod(const BytecodeInstruction &instr, double divisor) {
    if (!(std::fabs(divisor) >= 2.0 && std::fabs(divisor) < 2147483648.0) || divisor != std::floor(divisor)) return false;
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    const uint64_t d = static_cast<uint64_t>(std::fabs(divisor));
    const uint64_t magic = UINT64_C(0xFFFFFFFFFFFFFFFF) / d + 1;
    const bool negativeDivisor = divisor < 0;

    sljit_jump *shadowDone = nullptr;
    std::vector<sljit_jump *> notShadow;
    if (shadowSupported && !negativeDivisor && active_ != nullptr && active_->shadowSlot >= 0 && currentPc_ >= 2 && !analysis_.isBlockStart(currentPc_) && !analysis_.isBlockStart(currentPc_ - 1) &&
        chunk_.instructions[currentPc_ - 2].op == BytecodeOp::PushVarNumber) {
        const int64_t source = plan_.promotedVariable(chunk_.instructions[currentPc_ - 2]);
        if (source >= 0 && static_cast<size_t>(source) < promotedIndex_.size() && promotedIndex_[source] >= 0 && variableSlot(static_cast<uint32_t>(promotedIndex_[source])) == static_cast<uint32_t>(active_->shadowSlot)) {
            sljit_emit_op2u(c_, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_S1, 0, SLJIT_IMM, 1);
            notShadow.push_back(sljit_emit_jump(c_, SLJIT_OVERFLOW));
            notShadow.push_back(sljit_emit_cmp(c_, SLJIT_SIG_LESS_EQUAL, SLJIT_S1, 0, SLJIT_IMM, 0));
            notShadow.push_back(sljit_emit_cmp(c_, SLJIT_SIG_GREATER, SLJIT_S1, 0, SLJIT_IMM, 0x7fffffff));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S1, 0);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(magic));
            sljit_emit_op0(c_, SLJIT_LMUL_UW);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, static_cast<sljit_sw>(d));
            sljit_emit_op0(c_, SLJIT_LMUL_UW);
            sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR0, 0, SLJIT_R1, 0);
            shadowDone = sljit_emit_jump(c_, SLJIT_JUMP);
            sljit_label *generic = sljit_emit_label(c_);
            for (sljit_jump *jump : notShadow)
                sljit_set_label(jump, generic);
        }
    }
    loadF(SLJIT_FR0, a);
    std::vector<sljit_jump *> slow;
    sljit_emit_fop1(c_, SLJIT_CONV_S32_FROM_F64, SLJIT_R0, 0, SLJIT_FR0, 0);
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR2, 0, SLJIT_R0, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, 0, SLJIT_FR2, 0));

    sljit_emit_op1(c_, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_R0, 0);
    sljit_jump *negative = negativeDivisor ? sljit_emit_jump(c_, SLJIT_JUMP) : sljit_emit_cmp(c_, SLJIT_SIG_LESS_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R0, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(magic));
    sljit_emit_op0(c_, SLJIT_LMUL_UW);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, static_cast<sljit_sw>(d));
    sljit_emit_op0(c_, SLJIT_LMUL_UW);
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR0, 0, SLJIT_R1, 0);
    sljit_label *joined = sljit_emit_label(c_);
    if (shadowDone != nullptr) sljit_set_label(shadowDone, joined);
    storeF(a, SLJIT_FR0);

    addCold([this, slow, negative, joined, a, b, &fn, magic, d, divisor, negativeDivisor]() {
        sljit_set_label(negative, sljit_emit_label(c_));
        // m = a >> 31 is 0 or -1 and |a| = (a ^ m) - m, all in the integer unit
        sljit_emit_op2(c_, SLJIT_ASHR32, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_IMM, 31);
        sljit_emit_op2(c_, SLJIT_XOR32, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_R2, 0);
        sljit_emit_op2(c_, SLJIT_SUB32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
        sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_R1, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(magic));
        sljit_emit_op0(c_, SLJIT_LMUL_UW);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, static_cast<sljit_sw>(d));
        sljit_emit_op0(c_, SLJIT_LMUL_UW);
        sljit_jump *zero = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
        // the result for a negative a is -r, or d - r when the signs of a and the divisor agree: ((r ^ m) - m) + (divisor & mask)
        sljit_emit_op2(c_, SLJIT_XOR, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
        sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
        if (negativeDivisor) sljit_emit_op2(c_, SLJIT_XOR, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, -1);
        else sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_R2, 0);
        sljit_emit_op2(c_, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, static_cast<sljit_sw>(divisor));
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
        sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR0, 0, SLJIT_R1, 0);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), joined);
        // a multiple of the divisor: zero with the sign of a
        sljit_set_label(zero, sljit_emit_label(c_));
        sljit_emit_fset64(c_, SLJIT_FR3, 0.0);
        sljit_emit_fop2(c_, SLJIT_MUL_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR3, 0);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), joined);

        sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, here);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), SLJIT_ARGS2(F64, F64, F64));
        storeF(a, SLJIT_FR0);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), joined);
    });
    return true;
}

void Emitter::emitInlineMod(const BytecodeInstruction &instr) {
    if (currentPc_ > 0 && !analysis_.isBlockStart(currentPc_) && chunk_.instructions[currentPc_ - 1].op == BytecodeOp::PushConstNumber) {
        if (emitConstantMod(instr, chunk_.numberConstants[chunk_.instructions[currentPc_ - 1].a])) return;
    }
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    loadF(SLJIT_FR0, a);
    loadF(SLJIT_FR1, b);

    std::vector<sljit_jump *> slow;
    sljit_emit_fop1(c_, SLJIT_CONV_S32_FROM_F64, SLJIT_R0, 0, SLJIT_FR0, 0);
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR2, 0, SLJIT_R0, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, 0, SLJIT_FR2, 0));
    sljit_emit_fop1(c_, SLJIT_CONV_S32_FROM_F64, SLJIT_R1, 0, SLJIT_FR1, 0);
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR2, 0, SLJIT_R1, 0);
    slow.push_back(sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR1, 0, SLJIT_FR2, 0));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 0));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, -1));

    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_R2, 0, SLJIT_R1, 0);
    sljit_emit_op0(c_, SLJIT_DIVMOD_S32);
    sljit_jump *zero = sljit_emit_cmp(c_, SLJIT_EQUAL | SLJIT_32, SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_op2(c_, SLJIT_XOR | SLJIT_32, SLJIT_R0, 0, SLJIT_R1, 0, SLJIT_R2, 0);
    sljit_jump *sameSign = sljit_emit_cmp(c_, SLJIT_SIG_GREATER_EQUAL | SLJIT_32, SLJIT_R0, 0, SLJIT_IMM, 0);
    sljit_emit_op2(c_, SLJIT_ADD | SLJIT_32, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R2, 0);
    sljit_set_label(sameSign, sljit_emit_label(c_));
    sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_S32, SLJIT_FR0, 0, SLJIT_R1, 0);
    sljit_jump *stored = sljit_emit_jump(c_, SLJIT_JUMP);

    sljit_set_label(zero, sljit_emit_label(c_));
    sljit_emit_fset64(c_, SLJIT_FR2, 0.0);
    sljit_emit_fop2(c_, SLJIT_MUL_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR2, 0);
    sljit_set_label(stored, sljit_emit_label(c_));
    sljit_label *done = sljit_emit_label(c_);
    storeF(a, SLJIT_FR0);

    addCold([this, slow, done, a, b, &fn]() {
        sljit_label *here = sljit_emit_label(c_);
        for (sljit_jump *jump : slow)
            sljit_set_label(jump, here);
        loadF(SLJIT_FR0, a);
        loadF(SLJIT_FR1, b);
        callHelper(reinterpret_cast<void *>(fn.nn_n), SLJIT_ARGS2(F64, F64, F64));
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
    });
}

} // namespace BytecodeJitInternal

#endif
