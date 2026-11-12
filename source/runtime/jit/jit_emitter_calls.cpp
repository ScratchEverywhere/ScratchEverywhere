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
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitNativeCall(const CallDescriptor &call, BytecodeIndex target, std::vector<lir::Jump *> &slow) {
    const uint32_t n = call.numbers, b = call.booleans, v = call.values, argc = call.argCount;
    const uint32_t kept = v - argc;
    const uint32_t need = n + b + v;
    const auto field = [](size_t offset) { return static_cast<lir::Word>(offset); };
    const auto load32 = [&](lir::Int reg, size_t offset) { lir::emitOp1(c_, lir::MOV_U32, reg, 0, lir::MEM1(lir::S0), field(offset)); };
    const auto store32 = [&](size_t offset, lir::Int reg) { lir::emitOp1(c_, lir::MOV32, lir::MEM1(lir::S0), field(offset), reg, 0); };

    emitSpill();
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&call.fast));
    lir::emitOp1(c_, lir::MOV_U8, lir::R0, 0, lir::MEM1(lir::R0), 0);
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    if (!call.procWarp) {
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), field(offsetof(Context, warp)));
        slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    }
    load32(lir::R0, offsetof(Context, lazyDepth));
    load32(lir::R1, offsetof(Context, depthLimit));
    slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, lir::R0, 0, lir::R1, 0));
    load32(lir::R0, offsetof(Context, stringExcess));
    slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0));
    load32(lir::R0, offsetof(Context, saveTop));
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, need);
    load32(lir::R1, offsetof(Context, saveCap));
    slow.push_back(lir::emitCmp(c_, lir::GREATER, lir::R0, 0, lir::R1, 0));

    lir::emitOp1(c_, lir::MOV, lir::R3, 0, lir::IMM, 0);
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
        load32(lir::R0, offsetof(Context, lazyDepth));
        slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
        load32(lir::R0, offsetof(Context, nativeDepth));
        lir::emitOp2(c_, lir::SUB, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
        lir::emitOp2(c_, lir::SHL, lir::R0, 0, lir::R0, 0, lir::IMM, 5);
        lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(Runtime::frames));
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R0), field(offsetof(Runtime::NativeFrame, call)));
        lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::R0), field(offsetof(Runtime::NativeFrame, flags)));
        lir::emitOp2(c_, lir::LSHR, lir::R2, 0, lir::R2, 0, lir::IMM, 2);
        lir::emitOp2(c_, lir::AND, lir::R2, 0, lir::R2, 0, lir::IMM, 0x3FFFFFFF);
        lir::emitOp1(c_, lir::MOV_U32, lir::R0, 0, lir::MEM1(lir::R1), field(offsetof(CallDescriptor, argFromFrame)));
        lir::emitOp2(c_, lir::AND, lir::R2, 0, lir::R2, 0, lir::R0, 0);
        lir::emitOp1(c_, lir::MOV_U32, lir::R0, 0, lir::MEM1(lir::R1), field(offsetof(CallDescriptor, argConstTyped)));
        lir::emitOp2(c_, lir::OR, lir::R2, 0, lir::R2, 0, lir::R0, 0);
        lir::emitOp1(c_, lir::MOV_U32, lir::R0, 0, lir::MEM1(lir::R1), field(offsetof(CallDescriptor, argInvalid)));
        lir::emitOp2(c_, lir::AND, lir::R0, 0, lir::R0, 0, lir::IMM, invalidMask);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0));
        if (keptMask != 0) {
            lir::emitOp2(c_, lir::AND, lir::R0, 0, lir::R2, 0, lir::IMM, keptMask);
            slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0));
        }
    }
    for (const CallCheck &check : call.checks) {
        const uint32_t i = check.index;
        if (check.kind == ValueEntry::Temp) {
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::tempLazy[i]));
            lir::emitOp1(c_, lir::MOV_U8, lir::R0, 0, lir::MEM1(lir::R0), 0);
            if (i < kept) {
                slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 1));
            } else {
                lir::Jump *plain = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 1);
                slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 2));
                lir::emitOp2(c_, lir::OR, lir::R3, 0, lir::R3, 0, lir::IMM, 1 << (i - kept));
                lir::setLabel(plain, lir::emitLabel(c_));
            }
        } else if (check.kind == ValueEntry::Argument) {
            if (i < kept) continue;
            const uint32_t a = check.constant, bit = i - kept;
            if (a == bit) {
                lir::emitOp2(c_, lir::AND, lir::R0, 0, lir::R2, 0, lir::IMM, 1u << a);
            } else {
                lir::emitOp2(c_, lir::LSHR, lir::R0, 0, lir::R2, 0, lir::IMM, a);
                lir::emitOp2(c_, lir::AND, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
                lir::emitOp2(c_, lir::SHL, lir::R0, 0, lir::R0, 0, lir::IMM, bit);
            }
            lir::emitOp2(c_, lir::OR, lir::R3, 0, lir::R3, 0, lir::R0, 0);
        } else if (check.kind == ValueEntry::VarValue) {
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), dirtyOffset(check.constant));
            lir::emitOp2(c_, lir::SUB, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
            slow.push_back(lir::emitCmp(c_, lir::GREATER, lir::R0, 0, lir::IMM, 1));
        }
    }

    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), field(offsetof(Context, savePtr)));
    load32(lir::R1, offsetof(Context, saveTop));
    lir::emitOp2(c_, lir::SHL, lir::R2, 0, lir::R1, 0, lir::IMM, 3);
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::R2, 0);
    for (uint32_t i = 0; i < n; i++) {
        loadF(lir::FR0, i);
        lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), static_cast<lir::Word>(i) * 8, lir::FR0, 0);
    }
    for (uint32_t i = 0; i < b; i++) {
        loadF(lir::FR0, booleanSlot(i));
        lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), static_cast<lir::Word>(n + i) * 8, lir::FR0, 0);
    }
    for (uint32_t i = 0; i < v; i++) {
        loadF(lir::FR0, valueSlot(i));
        lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), static_cast<lir::Word>(n + b + i) * 8, lir::FR0, 0);
    }

    load32(lir::R0, offsetof(Context, nativeDepth));
    lir::emitOp2(c_, lir::SHL, lir::R2, 0, lir::R0, 0, lir::IMM, 5);
    lir::emitOp2(c_, lir::ADD, lir::R2, 0, lir::R2, 0, lir::IMM, reinterpret_cast<lir::Word>(Runtime::frames));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R2), field(offsetof(Runtime::NativeFrame, call)), lir::IMM, reinterpret_cast<lir::Word>(&call));
    lir::emitOp1(c_, lir::MOV, lir::R4, 0, lir::MEM1(lir::S0), field(offsetof(Context, numberBase)));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R2), field(offsetof(Runtime::NativeFrame, numberAndBooleanBase)), lir::R4, 0);
    lir::emitOp1(c_, lir::MOV, lir::R4, 0, lir::MEM1(lir::S0), field(offsetof(Context, stringBase)));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R2), field(offsetof(Runtime::NativeFrame, stringAndValueBase)), lir::R4, 0);
    lir::emitOp1(c_, lir::MOV, lir::R4, 0, lir::MEM1(lir::S0), field(offsetof(Context, warp)));
    lir::emitOp2(c_, lir::SHL, lir::R4, 0, lir::R4, 0, lir::IMM, 1);
    lir::emitOp2(c_, lir::OR, lir::R4, 0, lir::R4, 0, lir::IMM, Runtime::lazyFlag);
    lir::emitOp2(c_, lir::SHL, lir::R3, 0, lir::R3, 0, lir::IMM, 2);
    lir::emitOp2(c_, lir::OR, lir::R4, 0, lir::R4, 0, lir::R3, 0);
    load32(lir::R1, offsetof(Context, saveTop));
    lir::emitOp2(c_, lir::SHL, lir::R3, 0, lir::R1, 0, lir::IMM, 32);
    lir::emitOp2(c_, lir::OR, lir::R4, 0, lir::R4, 0, lir::R3, 0);
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::R2), field(offsetof(Runtime::NativeFrame, flags)), lir::R4, 0);

    lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R1, 0, lir::IMM, need);
    store32(offsetof(Context, saveTop), lir::R1);
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
    store32(offsetof(Context, nativeDepth), lir::R0);
    load32(lir::R0, offsetof(Context, lazyDepth));
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
    store32(offsetof(Context, lazyDepth), lir::R0);
    const auto advance = [&](size_t offset, uint32_t by) {
        if (by == 0) return;
        load32(lir::R0, offset);
        lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, by);
        store32(offset, lir::R0);
    };
    advance(offsetof(Context, numberBase), n);
    advance(offsetof(Context, booleanBase), b);
    advance(offsetof(Context, valueBase), kept);

    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), field(offsetof(Context, pendingRecursive)));
    lir::emitOp1(c_, lir::MOV32, lir::MEM1(lir::R0), 0, lir::IMM, -1);
    if (call.procWarp) {
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), field(offsetof(Context, threadWarp)));
        lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R0), 0, lir::IMM, 1);
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), field(offsetof(Context, warp)), lir::IMM, 1);
    }
    if (kept > 0) {
        for (uint32_t i = 0; i < argc; i++) {
            loadF(lir::FR0, valueSlot(kept + i));
            storeF(valueSlot(i), lir::FR0);
        }
    }
    pending_.push_back({lir::emitJump(c_, lir::JUMP), labelFor(target)});
}

void Emitter::emitNativeReturn(const ReturnDescriptor &ret, std::vector<lir::Jump *> &slow) {
    const auto field = [](size_t offset) { return static_cast<lir::Word>(offset); };
    const auto load32 = [&](lir::Int reg, size_t offset) { lir::emitOp1(c_, lir::MOV_U32, reg, 0, lir::MEM1(lir::S0), field(offset)); };
    const auto store32 = [&](size_t offset, lir::Int reg) { lir::emitOp1(c_, lir::MOV32, lir::MEM1(lir::S0), field(offset), reg, 0); };

    emitSpill();
    load32(lir::R0, offsetof(Context, lazyDepth));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    load32(lir::R4, offsetof(Context, nativeDepth));
    lir::emitOp2(c_, lir::SUB, lir::R4, 0, lir::R4, 0, lir::IMM, 1);
    lir::emitOp2(c_, lir::SHL, lir::R4, 0, lir::R4, 0, lir::IMM, 5);
    lir::emitOp2(c_, lir::ADD, lir::R4, 0, lir::R4, 0, lir::IMM, reinterpret_cast<lir::Word>(Runtime::frames));
    lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::R4), field(offsetof(Runtime::NativeFrame, call)));
    lir::emitOp1(c_, lir::MOV_U8, lir::R0, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, resultNumber)));
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0));
    if (ret.resultMode == 2) {
        lir::emitOp1(c_, lir::MOV_U8, lir::R0, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, wantsValue)));
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0));
    }
    if (ret.resultMode == 1) loadF(lir::FR1, valueSlot(ret.resultPosition));
    else lir::emitFSet64(c_, lir::FR1, 0.0);

    lir::emitOp1(c_, lir::MOV, lir::R3, 0, lir::MEM1(lir::R4), field(offsetof(Runtime::NativeFrame, flags)));
    lir::emitOp2(c_, lir::LSHR, lir::R0, 0, lir::R3, 0, lir::IMM, 1);
    lir::emitOp2(c_, lir::AND, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
    lir::emitOp2(c_, lir::LSHR, lir::R3, 0, lir::R3, 0, lir::IMM, 32);
    load32(lir::R1, offsetof(Context, nativeDepth));
    lir::emitOp2(c_, lir::SUB, lir::R1, 0, lir::R1, 0, lir::IMM, 1);
    store32(offsetof(Context, nativeDepth), lir::R1);
    load32(lir::R1, offsetof(Context, lazyDepth));
    lir::emitOp2(c_, lir::SUB, lir::R1, 0, lir::R1, 0, lir::IMM, 1);
    store32(offsetof(Context, lazyDepth), lir::R1);
    store32(offsetof(Context, saveTop), lir::R3);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::S0), field(offsetof(Context, threadWarp)));
    lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R1), 0, lir::R0, 0);
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), field(offsetof(Context, warp)), lir::R0, 0);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R4), field(offsetof(Runtime::NativeFrame, numberAndBooleanBase)));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), field(offsetof(Context, numberBase)), lir::R1, 0);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R4), field(offsetof(Runtime::NativeFrame, stringAndValueBase)));
    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), field(offsetof(Context, stringBase)), lir::R1, 0);

    lir::emitOp2(c_, lir::SHL, lir::R3, 0, lir::R3, 0, lir::IMM, 3);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::S0), field(offsetof(Context, savePtr)));
    lir::emitOp2(c_, lir::ADD, lir::R3, 0, lir::R3, 0, lir::R1, 0);

    const auto copy = [&]() {
        lir::Label *top = lir::emitLabel(c_);
        lir::Jump *done = lir::emitCmp(c_, lir::EQUAL, lir::R1, 0, lir::IMM, 0);
        lir::emitFOp1(c_, lir::MOV_F64, lir::FR0, 0, lir::MEM1(lir::R3), 0);
        lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), 0, lir::FR0, 0);
        lir::emitOp2(c_, lir::ADD, lir::R3, 0, lir::R3, 0, lir::IMM, 8);
        lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, 8);
        lir::emitOp2(c_, lir::SUB, lir::R1, 0, lir::R1, 0, lir::IMM, 1);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), top);
        lir::setLabel(done, lir::emitLabel(c_));
    };
    lir::emitOp1(c_, lir::MOV_U32, lir::R1, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, numbers)));
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::S0, 0, lir::IMM, slotOffset(0));
    copy();
    lir::emitOp1(c_, lir::MOV_U32, lir::R1, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, booleans)));
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::S0, 0, lir::IMM, slotOffset(booleanSlot(0)));
    copy();
    lir::emitOp1(c_, lir::MOV_U32, lir::R1, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, values)));
    lir::emitOp1(c_, lir::MOV_U32, lir::R0, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, argCount)));
    lir::emitOp2(c_, lir::SUB, lir::R1, 0, lir::R1, 0, lir::R0, 0);
    lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::S0, 0, lir::IMM, slotOffset(valueSlot(0)));
    copy();
    lir::emitOp1(c_, lir::MOV_U8, lir::R1, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, wantsValue)));
    lir::Jump *noResult = lir::emitCmp(c_, lir::EQUAL, lir::R1, 0, lir::IMM, 0);
    lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(lir::R0), 0, lir::FR1, 0);
    lir::setLabel(noResult, lir::emitLabel(c_));
    lir::emitOp1(c_, lir::MOV_S32, lir::R0, 0, lir::MEM1(lir::R2), field(offsetof(CallDescriptor, continuation)));
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(code_.entryTable.data()));
    lir::emitIJump(c_, lir::JUMP, lir::MEM2(lir::R1, lir::R0), lir::WORD_SHIFT);
}

} // namespace BytecodeJitInternal

#endif
