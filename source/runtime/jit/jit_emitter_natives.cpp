#ifdef SE_JIT

#include "bytecode.hpp"
#include "jit_emitter.hpp"
#include "jit_planner.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <sljitLir.h>

namespace BytecodeJitInternal {

void Emitter::emitNative(const BytecodeInstruction &instr, NativeShape shape) {
    const BytecodePositionalFn &fn = link_.positional[instr.a];
    switch (shape) {
    case NativeShape::Add:
    case NativeShape::Subtract:
    case NativeShape::Multiply:
    case NativeShape::Divide: {
        const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
        const sljit_s32 left = readF(a, SLJIT_FR0);
        const sljit_s32 op = shape == NativeShape::Add ? SLJIT_ADD_F64 : shape == NativeShape::Subtract ? SLJIT_SUB_F64
                                                                     : shape == NativeShape::Multiply   ? SLJIT_MUL_F64
                                                                                                        : SLJIT_DIV_F64;
        if (currentPc_ >= 1 && chunk_.instructions[currentPc_ - 1].op == BytecodeOp::PushConstNumber && poolOperandFollows(currentPc_ - 1)) {
            sljit_emit_fop2(c_, op, left, 0, left, 0, SLJIT_MEM1(SLJIT_S0), poolOffset(poolIndex(chunk_.numberConstants[chunk_.instructions[currentPc_ - 1].a])));
        } else {
            const sljit_s32 right = readF(b, SLJIT_FR1);
            sljit_emit_fop2(c_, op, left, 0, left, 0, right, 0);
        }
        commitF(a, left);
        state_.numbers -= 1;
        break;
    }
    case NativeShape::NumberNumber: {
        if (plan_.opcodeName(instr.a) == "operator_mod") {
            emitInlineMod(instr);
            state_.numbers -= 1;
            break;
        }
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
        const auto general = [this, a, target = fn.n_n]() {
            loadF(SLJIT_FR0, a);
            callHelper(reinterpret_cast<void *>(target), SLJIT_ARGS1(F64, F64));
            storeF(a, SLJIT_FR0);
        };
        if (plan_.opcodeName(instr.a) == "operator_round") emitInlineRounding(Rounding::Round, a, general);
        else general();
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
    case NativeShape::VoidNone:
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.voidFn), SLJIT_ARGS1V(P));
        break;
    case NativeShape::VoidNumber:
        if (emitInlineMotion(instr, shape)) {
            state_.numbers -= 1;
            break;
        }
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.n_void), SLJIT_ARGS2V(P, F64));
        state_.numbers -= 1;
        break;
    case NativeShape::VoidNumberNumber:
        if (emitInlineMotion(instr, shape)) {
            state_.numbers -= 2;
            break;
        }
        loadF(SLJIT_FR0, numberSlot(state_.numbers - 2));
        loadF(SLJIT_FR1, numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), offsetof(Context, sprite));
        callHelper(reinterpret_cast<void *>(fn.nn_void), SLJIT_ARGS3V(P, F64, F64));
        state_.numbers -= 2;
        break;
    case NativeShape::VoidValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ValueCallDescriptor &call = code_.valueCalls.emplace_back();
        call.fn = fn.v_void;
        call.itemPosition = depth - 1;
        call.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&call));
        callHelper(reinterpret_cast<void *>(&Runtime::jitCallValueVoid), SLJIT_ARGS2V(P, P));
        break;
    }
    case NativeShape::StringStringString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallStringString);
        op.fn = reinterpret_cast<void *>(fn.ss_s);
        op.a = state_.strings[n - 2];
        op.aPosition = n - 2;
        op.b = state_.strings[n - 1];
        op.bPosition = n - 1;
        op.outPosition = n - 2;
        state_.strings.pop_back();
        state_.strings.back().kind = StrEntry::Temp;
        callStringOp(op);
        break;
    }
    case NativeShape::StringStringBool: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallStringStringBool);
        op.fn = reinterpret_cast<void *>(fn.ss_b);
        op.a = state_.strings[n - 2];
        op.aPosition = n - 2;
        op.b = state_.strings[n - 1];
        op.bPosition = n - 1;
        op.numberOut = booleanSlot(state_.booleans);
        state_.strings.pop_back();
        state_.strings.pop_back();
        state_.booleans++;
        callStringOp(op);
        break;
    }
    case NativeShape::StringNumber: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallStringNumber);
        op.fn = reinterpret_cast<void *>(fn.s_n);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberOut = numberSlot(state_.numbers);
        state_.strings.pop_back();
        state_.numbers++;
        callStringOp(op);
        break;
    }
    case NativeShape::NumberStringString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallNumberStringString);
        op.fn = reinterpret_cast<void *>(fn.ns_s);
        op.numberIn = numberSlot(state_.numbers - 1);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.outPosition = n - 1;
        state_.numbers--;
        state_.strings.back().kind = StrEntry::Temp;
        callStringOp(op);
        break;
    }
    case NativeShape::SpriteStringNumber: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallSpriteStringNumber);
        op.fn = reinterpret_cast<void *>(fn.spriteS_n);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberOut = numberSlot(state_.numbers);
        state_.strings.pop_back();
        state_.numbers++;
        callStringOp(op);
        break;
    }
    case NativeShape::SpriteStringBool: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallSpriteStringBool);
        op.fn = reinterpret_cast<void *>(fn.spriteS_b);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberOut = booleanSlot(state_.booleans);
        state_.strings.pop_back();
        state_.booleans++;
        callStringOp(op);
        break;
    }
    case NativeShape::SpriteString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::CallSpriteString);
        op.fn = reinterpret_cast<void *>(fn.sprite_s);
        op.outPosition = n;
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        callStringOp(op);
        break;
    }
    case NativeShape::VoidString: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::VoidString);
        op.fn = reinterpret_cast<void *>(fn.s_void);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        state_.strings.pop_back();
        callStringOp(op);
        break;
    }
    case NativeShape::VoidStringNumber: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::VoidStringNumber);
        op.fn = reinterpret_cast<void *>(fn.sn_void);
        op.a = state_.strings[n - 1];
        op.aPosition = n - 1;
        op.numberIn = numberSlot(state_.numbers - 1);
        state_.strings.pop_back();
        state_.numbers--;
        callStringOp(op);
        break;
    }
    case NativeShape::None:
        break;
    }
}

} // namespace BytecodeJitInternal

#endif
