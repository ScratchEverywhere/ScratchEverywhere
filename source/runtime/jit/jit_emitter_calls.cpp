#ifdef SE_JIT

#include "bytecode.hpp"
#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sljitLir.h>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitNativeCall(const CallDescriptor &call, BytecodeIndex target, std::vector<sljit_jump *> &slow) {
    const uint32_t n = call.numbers, b = call.booleans, v = call.values, argc = call.argCount;
    const uint32_t kept = v - argc;
    const uint32_t need = n + b + v;
    const auto field = [](size_t offset) { return static_cast<sljit_sw>(offset); };
    const auto load32 = [&](sljit_s32 reg, size_t offset) { sljit_emit_op1(c_, SLJIT_MOV_U32, reg, 0, SLJIT_MEM1(SLJIT_S0), field(offset)); };
    const auto store32 = [&](size_t offset, sljit_s32 reg) { sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), field(offset), reg, 0); };

    emitSpill();
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&call.fast));
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0);
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    if (!call.procWarp) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, warp)));
        slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    }
    load32(SLJIT_R0, offsetof(Context, lazyDepth));
    load32(SLJIT_R1, offsetof(Context, depthLimit));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER_EQUAL, SLJIT_R0, 0, SLJIT_R1, 0));
    load32(SLJIT_R0, offsetof(Context, stringExcess));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    load32(SLJIT_R0, offsetof(Context, saveTop));
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, need);
    load32(SLJIT_R1, offsetof(Context, saveCap));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER, SLJIT_R0, 0, SLJIT_R1, 0));

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R3, 0, SLJIT_IMM, 0);
    bool anyArgument = false;
    uint32_t invalidMask = 0, keptMask = 0;
    for (const CallCheck &check : call.checks) {
        if (check.kind == ValueEntry::Argument) {
            anyArgument = true;
            invalidMask |= 1u << check.constant;
            if (check.index < kept) keptMask |= 1u << check.constant;
        }
    }
    if (anyArgument) {
        load32(SLJIT_R0, offsetof(Context, lazyDepth));
        slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
        load32(SLJIT_R0, offsetof(Context, nativeDepth));
        sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
        sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 5);
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(Runtime::frames));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R0), field(offsetof(Runtime::NativeFrame, call)));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R0), field(offsetof(Runtime::NativeFrame, flags)));
        sljit_emit_op2(c_, SLJIT_LSHR, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 2);
        sljit_emit_op2(c_, SLJIT_AND, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, 0x3FFFFFFF);
        sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R1), field(offsetof(CallDescriptor, argFromFrame)));
        sljit_emit_op2(c_, SLJIT_AND, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R0, 0);
        sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R1), field(offsetof(CallDescriptor, argConstTyped)));
        sljit_emit_op2(c_, SLJIT_OR, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R0, 0);
        sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R1), field(offsetof(CallDescriptor, argInvalid)));
        sljit_emit_op2(c_, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, invalidMask);
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
        if (keptMask != 0) {
            sljit_emit_op2(c_, SLJIT_AND, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, keptMask);
            slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
        }
    }
    for (const CallCheck &check : call.checks) {
        const uint32_t i = check.index;
        if (check.kind == ValueEntry::Temp) {
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&Runtime::tempLazy[i]));
            sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0);
            if (i < kept) {
                slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 1));
            } else {
                sljit_jump *plain = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 1);
                slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 2));
                sljit_emit_op2(c_, SLJIT_OR, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 1 << (i - kept));
                sljit_set_label(plain, sljit_emit_label(c_));
            }
        } else if (check.kind == ValueEntry::Argument) {
            if (i < kept) continue;
            const uint32_t a = check.constant, bit = i - kept;
            if (a == bit) {
                sljit_emit_op2(c_, SLJIT_AND, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, 1u << a);
            } else {
                sljit_emit_op2(c_, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R2, 0, SLJIT_IMM, a);
                sljit_emit_op2(c_, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
                sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, bit);
            }
            sljit_emit_op2(c_, SLJIT_OR, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_R0, 0);
        } else if (check.kind == ValueEntry::VarValue) {
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), dirtyOffset(check.constant));
            sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
            slow.push_back(sljit_emit_cmp(c_, SLJIT_GREATER, SLJIT_R0, 0, SLJIT_IMM, 1));
        }
    }

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, savePtr)));
    load32(SLJIT_R1, offsetof(Context, saveTop));
    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R2, 0, SLJIT_R1, 0, SLJIT_IMM, 3);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_R2, 0);
    for (uint32_t i = 0; i < n; i++) {
        loadF(SLJIT_FR0, i);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), static_cast<sljit_sw>(i) * 8, SLJIT_FR0, 0);
    }
    for (uint32_t i = 0; i < b; i++) {
        loadF(SLJIT_FR0, booleanSlot(i));
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), static_cast<sljit_sw>(n + i) * 8, SLJIT_FR0, 0);
    }
    for (uint32_t i = 0; i < v; i++) {
        loadF(SLJIT_FR0, valueSlot(i));
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), static_cast<sljit_sw>(n + b + i) * 8, SLJIT_FR0, 0);
    }

    load32(SLJIT_R0, offsetof(Context, nativeDepth));
    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R2, 0, SLJIT_R0, 0, SLJIT_IMM, 5);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(Runtime::frames));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), field(offsetof(Runtime::NativeFrame, call)), SLJIT_IMM, reinterpret_cast<sljit_sw>(&call));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, numberBase)));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), field(offsetof(Runtime::NativeFrame, numberAndBooleanBase)), SLJIT_R4, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, stringBase)));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), field(offsetof(Runtime::NativeFrame, stringAndValueBase)), SLJIT_R4, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R4, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, warp)));
    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, 1);
    sljit_emit_op2(c_, SLJIT_OR, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, Runtime::lazyFlag);
    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 2);
    sljit_emit_op2(c_, SLJIT_OR, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_R3, 0);
    load32(SLJIT_R1, offsetof(Context, saveTop));
    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R3, 0, SLJIT_R1, 0, SLJIT_IMM, 32);
    sljit_emit_op2(c_, SLJIT_OR, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_R3, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_R2), field(offsetof(Runtime::NativeFrame, flags)), SLJIT_R4, 0);

    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, need);
    store32(offsetof(Context, saveTop), SLJIT_R1);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    store32(offsetof(Context, nativeDepth), SLJIT_R0);
    load32(SLJIT_R0, offsetof(Context, lazyDepth));
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    store32(offsetof(Context, lazyDepth), SLJIT_R0);
    const auto advance = [&](size_t offset, uint32_t by) {
        if (by == 0) return;
        load32(SLJIT_R0, offset);
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, by);
        store32(offset, SLJIT_R0);
    };
    advance(offsetof(Context, numberBase), n);
    advance(offsetof(Context, booleanBase), b);
    advance(offsetof(Context, valueBase), kept);

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, pendingRecursive)));
    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, -1);
    if (call.procWarp) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, threadWarp)));
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_IMM, 1);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, warp)), SLJIT_IMM, 1);
    }
    if (kept > 0) {
        for (uint32_t i = 0; i < argc; i++) {
            loadF(SLJIT_FR0, valueSlot(kept + i));
            storeF(valueSlot(i), SLJIT_FR0);
        }
    }
    pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), target});
}

void Emitter::emitNativeReturn(const ReturnDescriptor &ret, std::vector<sljit_jump *> &slow) {
    const auto field = [](size_t offset) { return static_cast<sljit_sw>(offset); };
    const auto load32 = [&](sljit_s32 reg, size_t offset) { sljit_emit_op1(c_, SLJIT_MOV_U32, reg, 0, SLJIT_MEM1(SLJIT_S0), field(offset)); };
    const auto store32 = [&](size_t offset, sljit_s32 reg) { sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_MEM1(SLJIT_S0), field(offset), reg, 0); };

    emitSpill();
    load32(SLJIT_R0, offsetof(Context, lazyDepth));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    load32(SLJIT_R4, offsetof(Context, nativeDepth));
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, 1);
    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, 5);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(Runtime::frames));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R2, 0, SLJIT_MEM1(SLJIT_R4), field(offsetof(Runtime::NativeFrame, call)));
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, resultNumber)));
    slow.push_back(sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    if (ret.resultMode == 2) {
        sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, wantsValue)));
        slow.push_back(sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0));
    }
    if (ret.resultMode == 1) loadF(SLJIT_FR1, valueSlot(ret.resultPosition));
    else sljit_emit_fset64(c_, SLJIT_FR1, 0.0);

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R3, 0, SLJIT_MEM1(SLJIT_R4), field(offsetof(Runtime::NativeFrame, flags)));
    sljit_emit_op2(c_, SLJIT_LSHR, SLJIT_R0, 0, SLJIT_R3, 0, SLJIT_IMM, 1);
    sljit_emit_op2(c_, SLJIT_AND, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    sljit_emit_op2(c_, SLJIT_LSHR, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 32);
    load32(SLJIT_R1, offsetof(Context, nativeDepth));
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
    store32(offsetof(Context, nativeDepth), SLJIT_R1);
    load32(SLJIT_R1, offsetof(Context, lazyDepth));
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
    store32(offsetof(Context, lazyDepth), SLJIT_R1);
    store32(offsetof(Context, saveTop), SLJIT_R3);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, threadWarp)));
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_MEM1(SLJIT_R1), 0, SLJIT_R0, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, warp)), SLJIT_R0, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R4), field(offsetof(Runtime::NativeFrame, numberAndBooleanBase)));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, numberBase)), SLJIT_R1, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R4), field(offsetof(Runtime::NativeFrame, stringAndValueBase)));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, stringBase)), SLJIT_R1, 0);

    sljit_emit_op2(c_, SLJIT_SHL, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 3);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_S0), field(offsetof(Context, savePtr)));
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_R1, 0);

    const auto copy = [&]() {
        sljit_label *top = sljit_emit_label(c_);
        sljit_jump *done = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R3), 0);
        sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0);
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 8);
        sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 8);
        sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_IMM, 1);
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), top);
        sljit_set_label(done, sljit_emit_label(c_));
    };
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, numbers)));
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, slotOffset(0));
    copy();
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, booleans)));
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, slotOffset(booleanSlot(0)));
    copy();
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, values)));
    sljit_emit_op1(c_, SLJIT_MOV_U32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, argCount)));
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R0, 0);
    sljit_emit_op2(c_, SLJIT_ADD, SLJIT_R0, 0, SLJIT_S0, 0, SLJIT_IMM, slotOffset(valueSlot(0)));
    copy();
    sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R1, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, wantsValue)));
    sljit_jump *noResult = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R1, 0, SLJIT_IMM, 0);
    sljit_emit_fop1(c_, SLJIT_MOV_F64, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR1, 0);
    sljit_set_label(noResult, sljit_emit_label(c_));
    sljit_emit_op1(c_, SLJIT_MOV_S32, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R2), field(offsetof(CallDescriptor, continuation)));
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(code_.entryTable.data()));
    sljit_emit_ijump(c_, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT);
}

} // namespace BytecodeJitInternal

#endif
