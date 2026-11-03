#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit_emitter.hpp"
#include "jit_planner.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <algorithm>
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

void Emitter::emitInstruction(BytecodeIndex pc) {
    currentPc_ = pc;
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
        ensureVariableNumber(instr.a);
        const uint32_t to = numberSlot(state_.numbers);
        const sljit_s32 reg = destF(to, SLJIT_FR0);
        if (const int pin = pinOf(variableSlot(promoted(instr.a))); pin >= 0) sljit_emit_fop1(c_, SLJIT_MOV_F64, reg, 0, SLJIT_FR(pin), 0);
        else loadF(reg, variableSlot(promoted(instr.a)));
        nanToZero(reg);
        commitF(to, reg);
        state_.numbers++;
        return;
    }
    case BytecodeOp::StoreVarNumber: {
        const uint32_t k = promoted(instr.a);
        moveF(variableSlot(k), numberSlot(state_.numbers - 1));
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.numbers--;
        return;
    }
    case BytecodeOp::ChangeVarNumber: {
        ensureVariableNumber(instr.a);
        const uint32_t k = promoted(instr.a);
        if (shadowSupported && active_ != nullptr && active_->shadowSlot == static_cast<int64_t>(variableSlot(k)) && pinOf(variableSlot(k)) >= 0) {
            const double amount = chunk_.numberConstants[chunk_.instructions[pc - 1].a];
            const sljit_s32 pin = SLJIT_FR(pinOf(variableSlot(k)));
            sljit_emit_op2u(c_, SLJIT_SUB | SLJIT_SET_OVERFLOW, SLJIT_S1, 0, SLJIT_IMM, 1);
            sljit_jump *notInteger = sljit_emit_jump(c_, SLJIT_OVERFLOW);
            sljit_emit_op2(c_, SLJIT_ADD, SLJIT_S1, 0, SLJIT_S1, 0, SLJIT_IMM, static_cast<sljit_sw>(amount));
            sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, pin, 0, SLJIT_S1, 0);
            sljit_label *stored = sljit_emit_label(c_);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
            state_.numbers--;
            addCold([this, notInteger, stored, pin, amount]() {
                sljit_set_label(notInteger, sljit_emit_label(c_));
                sljit_jump *nan = sljit_emit_fcmp(c_, SLJIT_UNORDERED, pin, 0, pin, 0);
                sljit_label *add = sljit_emit_label(c_);
                sljit_emit_fset64(c_, SLJIT_FR1, amount);
                sljit_emit_fop2(c_, SLJIT_ADD_F64, pin, 0, pin, 0, SLJIT_FR1, 0);
                sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), stored);
                sljit_set_label(nan, sljit_emit_label(c_));
                sljit_emit_fset64(c_, pin, 0.0);
                sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), add);
            });
            return;
        }
        const sljit_s32 variable = readF(variableSlot(k), SLJIT_FR0);
        nanToZero(variable);
        const sljit_s32 amount = readF(numberSlot(state_.numbers - 1), SLJIT_FR1);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, variable, 0, variable, 0, amount, 0);
        commitF(variableSlot(k), variable);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.numbers--;
        return;
    }

    case BytecodeOp::PushConstValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        if (!feedsCompare(pc)) storeImmF(valueSlot(depth), chunk_.constants[instr.a].numberRaw());
        AbsValue v;
        v.kind = chunk_.constants[instr.a].isRealNumber() ? ValueEntry::Number : ValueEntry::Constant;
        v.constant = instr.a;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }
    case BytecodeOp::ConvertNumberToValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        if (!convertFeedsStore(pc) && !feedsIndex(pc) && !feedsCompare(pc)) {
            loadF(SLJIT_FR0, numberSlot(state_.numbers - 1));
            storeF(valueSlot(depth), SLJIT_FR0);
        }
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
            const bool carried = top.kind == ValueEntry::Temp && fr0ItemPc_ + 1 == pc && convertCarries(pc);
            if (!carried) loadF(SLJIT_FR0, valueSlot(static_cast<uint32_t>(state_.values.size())));
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
        const uint32_t position = static_cast<uint32_t>(state_.values.size());
        if (top.kind == ValueEntry::Constant) {
            storeImmF(variableSlot(k), constantNumber(top.constant));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 5 + static_cast<sljit_sw>(top.constant));
        } else if (top.kind == ValueEntry::Number) {
            if (numberFedStore(pc)) {
                moveF(variableSlot(k), numberSlot(state_.numbers));
            } else {
                loadF(SLJIT_FR0, valueSlot(position));
                storeF(variableSlot(k), SLJIT_FR0);
            }
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 2);
        } else {
            ShadowDescriptor &shadow = code_.shadows.emplace_back();
            shadow.variable = k;
            shadow.entry = describe(top);
            shadow.position = position;

            const auto emitBoxed = [this, k, position, &shadow]() {
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&shadow));
                callHelper(reinterpret_cast<void *>(&Runtime::jitStoreShadow), SLJIT_ARGS2V(P, P));
                loadF(SLJIT_FR0, valueSlot(position));
                storeF(variableSlot(k), SLJIT_FR0);
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 3);
            };

            if (top.kind == ValueEntry::VarValue && top.constant < code_.promoted.size()) {
                const uint32_t source = top.constant;
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), dirtyOffset(source));
                sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
                sljit_jump *other = sljit_emit_cmp(c_, SLJIT_GREATER, SLJIT_R0, 0, SLJIT_IMM, 1);
                loadF(SLJIT_FR0, variableSlot(source));
                storeF(variableSlot(k), SLJIT_FR0);
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 2);
                sljit_label *done = sljit_emit_label(c_);

                addCold([this, other, done, source, k, emitBoxed]() {
                    sljit_set_label(other, sljit_emit_label(c_));
                    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_S0), dirtyOffset(source));
                    sljit_jump *boxed0 = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
                    sljit_jump *boxed3 = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 3);
                    sljit_jump *boxed4 = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 4);
                    loadF(SLJIT_FR0, variableSlot(source));
                    storeF(variableSlot(k), SLJIT_FR0);
                    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_R0, 0);
                    sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
                    sljit_label *general = sljit_emit_label(c_);
                    sljit_set_label(boxed0, general);
                    sljit_set_label(boxed3, general);
                    sljit_set_label(boxed4, general);
                    emitBoxed();
                    sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
                });
            } else if (top.kind == ValueEntry::Temp) {
                sljit_emit_op1(c_, SLJIT_MOV_U8, SLJIT_R2, 0, SLJIT_MEM0(), reinterpret_cast<sljit_sw>(&Runtime::tempLazy[position]));
                sljit_jump *boxedTemp = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_R2, 0, SLJIT_IMM, 1);
                loadF(SLJIT_FR0, valueSlot(position));
                storeF(variableSlot(k), SLJIT_FR0);
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 2);
                sljit_label *done = sljit_emit_label(c_);
                addCold([this, boxedTemp, done, emitBoxed]() {
                    sljit_set_label(boxedTemp, sljit_emit_label(c_));
                    emitBoxed();
                    sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
                });
            } else {
                emitBoxed();
            }
        }
        return;
    }

    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater: {
        const BytecodeCompareKind kind = instr.op == BytecodeOp::NumEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::NumLess ? BytecodeCompareKind::Less
                                                                                                                                           : BytecodeCompareKind::Greater;
        if (bool jumpIfTrue; fusableBranch(pc, jumpIfTrue, fuseEnd_)) {
            const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
            state_.numbers -= 2;
            const BytecodeIndex target = chunk_.instructions[fuseEnd_].a;
            for (BytecodeIndex q = pc + 1; q <= fuseEnd_; q++)
                fused_[q] = true;
            emitCompareBranch(kind, jumpIfTrue, target, target <= fuseEnd_, {false, a, 0}, {false, b, 0}, state_);
            return;
        }
        emitCompareValue(instr.op == BytecodeOp::NumEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::NumLess ? BytecodeCompareKind::Less
                                                                                                                           : BytecodeCompareKind::Greater,
                         numberSlot(state_.numbers - 2), numberSlot(state_.numbers - 1));
        state_.numbers -= 2;
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::ValueEquals:
    case BytecodeOp::ValueLess:
    case BytecodeOp::ValueGreater: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const BytecodeCompareKind kind = instr.op == BytecodeOp::ValueEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::ValueLess ? BytecodeCompareKind::Less
                                                                                                                                               : BytecodeCompareKind::Greater;
        BytecodeAbstractState s;
        analysis_.stateBefore(pc, s);
        const bool bothNumeric = s.values.size() >= 2 && Planner::numericLike(s.values[s.values.size() - 1]) && Planner::numericLike(s.values[s.values.size() - 2]);
        if (bothNumeric) {
            if (bool jumpIfTrue; fusableBranch(pc, jumpIfTrue, fuseEnd_)) {
                state_.values.pop_back();
                state_.values.pop_back();
                const BytecodeIndex target = chunk_.instructions[fuseEnd_].a;
                for (BytecodeIndex q = pc + 1; q <= fuseEnd_; q++)
                    fused_[q] = true;
                Operand a{false, valueSlot(depth - 2), 0}, b{false, valueSlot(depth - 1), 0};
                if (!fedCompare(pc, a, b)) {
                    a = {false, valueSlot(depth - 2), 0};
                    b = {false, valueSlot(depth - 1), 0};
                }
                emitCompareBranch(kind, jumpIfTrue, target, target <= fuseEnd_, a, b, state_);
                return;
            }
            emitCompareValue(kind, valueSlot(depth - 2), valueSlot(depth - 1));
        } else {
            CompareDescriptor &compare = code_.compares.emplace_back();
            compare.kind = kind;
            compare.a = describe(state_.values[depth - 2]);
            compare.aPosition = depth - 2;
            compare.b = describe(state_.values[depth - 1]);
            compare.bPosition = depth - 1;
            const uint32_t result = booleanSlot(state_.booleans);
            const auto emitHelper = [this, &compare, result]() {
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
                sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&compare));
                callHelper(reinterpret_cast<void *>(&Runtime::jitCompareValues), SLJIT_ARGS2(W, P, P));
                storeW(result, SLJIT_R0);
            };

            std::vector<sljit_jump *> notPlain;
            const bool inlinable = guardPlainNumber(state_.values[depth - 2], depth - 2, notPlain) && guardPlainNumber(state_.values[depth - 1], depth - 1, notPlain);
            if (inlinable) {
                emitCompareValue(kind, valueSlot(depth - 2), valueSlot(depth - 1));
                sljit_label *compared = sljit_emit_label(c_);
                addCold([this, notPlain, compared, emitHelper]() {
                    sljit_label *here = sljit_emit_label(c_);
                    for (sljit_jump *jump : notPlain)
                        sljit_set_label(jump, here);
                    emitHelper();
                    sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), compared);
                });
            } else {
                for (sljit_jump *jump : notPlain)
                    sljit_set_label(jump, sljit_emit_label(c_));
                emitHelper();
            }
        }
        state_.values.pop_back();
        state_.values.pop_back();
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::PushArg: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        loadF(SLJIT_FR0, valueSlot(instr.a));
        storeF(valueSlot(depth), SLJIT_FR0);
        AbsValue v;
        v.kind = ValueEntry::Argument;
        v.constant = instr.a;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

    case BytecodeOp::CallProcedure:
    case BytecodeOp::CallProcedureValue: {
        const BytecodeProcedure &proc = chunk_.procedures[instr.a];
        CallDescriptor &call = code_.calls.emplace_back();
        call.state = &makeExit(pc, 0, state_);
        call.callPc = pc;
        call.procedure = instr.a;
        call.argCount = instr.b;
        call.wantsValue = instr.op == BytecodeOp::CallProcedureValue;
        call.calleeEntry = static_cast<uint32_t>(plan_.entryOf[proc.entryInstruction]);
        call.leanKinds = state_.strings.empty();
        for (const AbsValue &value : state_.values) {
            if (value.kind != ValueEntry::Number && value.kind != ValueEntry::Constant && value.kind != ValueEntry::Temp && value.kind != ValueEntry::Argument && value.kind != ValueEntry::VarValue && value.kind != ValueEntry::Opaque && value.kind != ValueEntry::Like) call.leanKinds = false;
        }
        call.proc = &proc;
        call.procWarp = proc.runWithoutScreenRefresh;
        call.numbers = state_.numbers;
        call.booleans = state_.booleans;
        call.values = static_cast<uint32_t>(state_.values.size());
        if (call.numbers > 0xFFFF || call.booleans > 0xFFFF || call.values > 0xFFFF || call.argCount > Runtime::maxTypedArguments) call.leanKinds = false;
        for (uint32_t i = 0; i < call.values; i++) {
            const AbsValue &value = state_.values[i];
            if (value.kind == ValueEntry::Opaque || value.kind == ValueEntry::Like || value.kind == ValueEntry::Temp || value.kind == ValueEntry::Argument || value.kind == ValueEntry::VarValue) call.checks.push_back({i, value.constant, static_cast<uint8_t>(value.kind)});
        }
        {
            static const bool disabled = std::getenv("SE_JIT_NOFASTCALL") != nullptr;
            const std::vector<AbsValue> &values = state_.values;
            call.nativeChecks = !disabled && call.leanKinds && call.values <= 16 && call.numbers <= 16 && call.booleans <= 16;
            for (const CallCheck &check : call.checks) {
                if (check.kind == ValueEntry::Argument && check.constant >= Runtime::maxTypedArguments) call.nativeChecks = false;
                if (check.kind == ValueEntry::VarValue && check.constant >= code_.promoted.size()) call.nativeChecks = false;
            }
            call.argInvalid = call.argCount >= 32 ? 0 : ~0u << call.argCount;
            if (values.size() < call.argCount) call.argInvalid = ~0u;
            else {
                for (uint32_t j = 0; j < call.argCount; j++) {
                    const AbsValue &arg = values[values.size() - call.argCount + j];
                    if (arg.kind == ValueEntry::Number) continue;
                    if (arg.kind == ValueEntry::Temp || arg.kind == ValueEntry::Argument || arg.kind == ValueEntry::VarValue) call.argFromFrame |= 1u << j;
                    else if (arg.kind == ValueEntry::Constant) {
                        const int how = Runtime::numberKindOf(chunk_.constants[arg.constant]);
                        if (how == 1) call.argConstTyped |= 1u << j;
                        else if (how < 0) call.argInvalid |= 1u << j;
                    } else call.argInvalid |= 1u << j;
                }
            }
        }
        std::vector<sljit_jump *> slowCall;
        if (call.nativeChecks) {
            emitNativeCall(call, proc.entryInstruction, slowCall);
            sljit_label *slowHere = sljit_emit_label(c_);
            for (sljit_jump *jump : slowCall)
                sljit_set_label(jump, slowHere);
        }
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&call));
        callHelper(reinterpret_cast<void *>(&Runtime::jitCall), SLJIT_ARGS2(W, P, P));
        sljit_jump *native = sljit_emit_cmp(c_, SLJIT_EQUAL, SLJIT_R0, 0, SLJIT_IMM, 0);
        leaveJumps_.push_back(sljit_emit_jump(c_, SLJIT_JUMP));
        sljit_set_label(native, sljit_emit_label(c_));
        pending_.push_back({sljit_emit_jump(c_, SLJIT_JUMP), proc.entryInstruction});
        return;
    }

    case BytecodeOp::Return:
    case BytecodeOp::Halt: {
        ReturnDescriptor &ret = code_.returns.emplace_back();
        ret.state = &makeExit(pc, 0, state_);
        ret.pc = pc;
        ret.isReturn = instr.op == BytecodeOp::Return;
        {
            static const bool disabled = std::getenv("SE_JIT_NOFASTCALL") != nullptr;
            if (ret.isReturn && !state_.values.empty()) {
                if (state_.values.back().kind == ValueEntry::Number) {
                    ret.resultMode = 1;
                    ret.resultPosition = static_cast<uint32_t>(state_.values.size()) - 1;
                } else {
                    ret.resultMode = 2;
                }
            }
            if (!disabled) {
                std::vector<sljit_jump *> slowReturn;
                emitNativeReturn(ret, slowReturn);
                sljit_label *slowHere = sljit_emit_label(c_);
                for (sljit_jump *jump : slowReturn)
                    sljit_set_label(jump, slowHere);
            }
        }
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&ret));
        callHelper(reinterpret_cast<void *>(&Runtime::jitReturn), SLJIT_ARGS2(W, P, P));
        sljit_jump *leave = sljit_emit_cmp(c_, SLJIT_SIG_LESS, SLJIT_R0, 0, SLJIT_IMM, 0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(code_.entryTable.data()));
        sljit_emit_ijump(c_, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R1, SLJIT_R0), SLJIT_WORD_SHIFT);
        addCold([this, leave]() {
            sljit_set_label(leave, sljit_emit_label(c_));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, 1 + static_cast<sljit_sw>(BytecodeJit::Result::Interpret));
            leaveJumps_.push_back(sljit_emit_jump(c_, SLJIT_JUMP));
        });
        return;
    }

    case BytecodeOp::ItemOfList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.indexPosition = depth - 1;
        if (plan_.topValuesAreNumeric(pc, 1)) {
            Operand source;
            const bool fed = indexFed(pc, state_.numbers, source);
            if (emitInlineListItem(op, {}, nullptr, fed ? &source : nullptr, convertCarries(pc + 1))) fr0ItemPc_ = pc;
            else callListHelper(reinterpret_cast<void *>(&Runtime::jitListItem), op, SLJIT_ARGS2V(P, P));
        } else {
            op.item = describe(state_.values[depth - 1]);
            op.itemPosition = depth - 1;
            std::vector<sljit_jump *> notPlain;
            const AbsValue &index = state_.values[depth - 1];
            const bool guardable = index.kind == ValueEntry::Temp || (index.kind == ValueEntry::VarValue && index.constant < code_.promoted.size());
            bool inlined = false;
            if (guardable) {
                guardPlainNumber(index, depth - 1, notPlain);
                inlined = emitInlineListItem(op, notPlain, reinterpret_cast<void *>(&Runtime::jitListItemAny), nullptr, convertCarries(pc + 1));
                if (inlined) fr0ItemPc_ = pc;
            }
            if (!inlined) callListHelper(reinterpret_cast<void *>(&Runtime::jitListItemAny), op, SLJIT_ARGS2V(P, P));
        }
        state_.values.back().kind = ValueEntry::Temp;
        return;
    }
    case BytecodeOp::ReplaceInList:
    case BytecodeOp::InsertInList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.indexPosition = depth - 1;
        op.itemPosition = depth - 2;
        op.item = describe(state_.values[depth - 2]);
        const bool replace = instr.op == BytecodeOp::ReplaceInList;
        void *helper;
        if (plan_.topValuesAreNumeric(pc, 1)) {
            if (replace && emitInlineListReplace(op, state_.values[depth - 2])) {
                state_.values.pop_back();
                state_.values.pop_back();
                return;
            }
            helper = reinterpret_cast<void *>(replace ? &Runtime::jitListReplace : &Runtime::jitListInsert);
        } else {
            op.index = describe(state_.values[depth - 1]);
            helper = reinterpret_cast<void *>(replace ? &Runtime::jitListReplaceAny : &Runtime::jitListInsertAny);
        }
        state_.values.pop_back();
        state_.values.pop_back();
        callListHelper(helper, op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::DeleteFromList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.indexPosition = depth - 1;
        const bool numeric = plan_.topValuesAreNumeric(pc, 1);
        if (!numeric) op.index = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(numeric ? &Runtime::jitListDelete : &Runtime::jitListDeleteAny), op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::AddToList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListAdd), op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::DeleteAllOfList: {
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListClear), op, SLJIT_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::LengthOfList: {
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListLength), op, SLJIT_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }
    case BytecodeOp::ItemNumOfList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListIndexOf), op, SLJIT_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), SLJIT_FR0);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }
    case BytecodeOp::ListContainsItem: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListContains), op, SLJIT_ARGS2(W, P, P));
        storeW(booleanSlot(state_.booleans), SLJIT_R0);
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::PushVarValue: {
        ensureVariableNumber(instr.a);
        const uint32_t k = promoted(instr.a);
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        if (!feedsCompare(pc) && !feedsIndex(pc)) {
            loadF(SLJIT_FR0, variableSlot(k));
            storeF(valueSlot(depth), SLJIT_FR0);
        }
        AbsValue v;
        v.kind = ValueEntry::VarValue;
        v.constant = k;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

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
        else pendUnconditional(instr.a);
        return;

    case BytecodeOp::RepeatCheck: {
        const uint32_t counter = valueSlot(static_cast<uint32_t>(state_.values.size()) - 1);
        if (isCounterPin(counter)) {
            sljit_emit_op2(c_, SLJIT_SUB | SLJIT_SET_SIG_LESS, SLJIT_S3, 0, SLJIT_S3, 0, SLJIT_IMM, 1);
            sljit_jump *finished = sljit_emit_jump(c_, SLJIT_SIG_LESS);
            state_.values.back().kind = ValueEntry::Number;
            AbsState exited = state_;
            exited.values.pop_back();
            emitJumpTo(finished, instr.a, exited, instr.a <= pc);
            return;
        }
        loadF(SLJIT_FR0, counter);

        const bool wholeCounter = wholeCounterAt(pc);
        if (wholeCounter) {
            sljit_emit_fset64(c_, SLJIT_FR1, 1.0);
            sljit_emit_fop2(c_, SLJIT_SUB_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        } else {
            sljit_emit_fop1(c_, SLJIT_CONV_SW_FROM_F64, SLJIT_R0, 0, SLJIT_FR0, 0);
            sljit_emit_fop1(c_, SLJIT_CONV_F64_FROM_SW, SLJIT_FR1, 0, SLJIT_R0, 0);
            sljit_jump *fractional = sljit_emit_fcmp(c_, SLJIT_UNORDERED_OR_NOT_EQUAL, SLJIT_FR0, 0, SLJIT_FR1, 0);
            sljit_emit_fset64(c_, SLJIT_FR1, 1.0);
            sljit_emit_fop2(c_, SLJIT_SUB_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
            sljit_label *counted = sljit_emit_label(c_);
            addCold([this, fractional, counted]() {
                sljit_set_label(fractional, sljit_emit_label(c_));
                callHelper(reinterpret_cast<void *>(&Runtime::jitRepeatRemaining), SLJIT_ARGS1(F64, F64));
                sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), counted);
            });
        }
        sljit_emit_fset64(c_, SLJIT_FR1, 0.0);
        state_.values.back().kind = ValueEntry::Number;
        AbsState exited = state_;
        exited.values.pop_back();
        sljit_jump *done = sljit_emit_fcmp(c_, SLJIT_ORDERED_LESS, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(counter, SLJIT_FR0);
        emitJumpTo(done, instr.a, exited, instr.a <= pc);
        return;
    }

    case BytecodeOp::PushConstString: {
        StrEntry entry;
        entry.kind = StrEntry::Const;
        entry.constant = instr.a;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::PushVarString: {
        if (plan_.variableBorrowable(pc)) {
            StrEntry entry;
            entry.kind = StrEntry::Variable;
            entry.constant = promoted(instr.a);
            state_.strings.push_back(entry);
            return;
        }
        StringOp &op = newStringOp(StringOp::PushVariable);
        op.variable = promoted(instr.a);
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::StoreVarString:
    case BytecodeOp::AppendVarString: {
        StringOp &op = newStringOp(instr.op == BytecodeOp::StoreVarString ? StringOp::StoreVariable : StringOp::AppendVariable);
        op.variable = promoted(instr.a);
        op.aPosition = static_cast<uint32_t>(state_.strings.size()) - 1;
        op.a = state_.strings.back();
        state_.strings.pop_back();
        callStringOp(op);
        return;
    }
    case BytecodeOp::PopString:
        state_.strings.pop_back();
        return;
    case BytecodeOp::StrEquals:
    case BytecodeOp::StrLess:
    case BytecodeOp::StrGreater: {
        const uint32_t n = static_cast<uint32_t>(state_.strings.size());
        StringOp &op = newStringOp(StringOp::Compare);
        op.compare = instr.op == BytecodeOp::StrEquals ? BytecodeCompareKind::Equals : instr.op == BytecodeOp::StrLess ? BytecodeCompareKind::Less
                                                                                                                       : BytecodeCompareKind::Greater;
        op.a = state_.strings[n - 2];
        op.aPosition = n - 2;
        op.b = state_.strings[n - 1];
        op.bPosition = n - 1;
        op.numberOut = booleanSlot(state_.booleans);
        state_.strings.pop_back();
        state_.strings.pop_back();
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        const auto literalChar = [this](const StrEntry &entry) -> int {
            if (entry.kind != StrEntry::Const) return -1;
            const std::string &text = chunk_.stringConstants[entry.constant];
            if (text.size() != 1) return -1;
            const unsigned char c = static_cast<unsigned char>(text[0]);
            if (c <= 0x20 || c >= 0x7F || (c >= '0' && c <= '9') || c == '.') return -1;
            return c >= 'A' && c <= 'Z' ? c + 32 : c;
        };
        const bool varFirst = op.a.kind == StrEntry::Variable;
        const StrEntry &var = varFirst ? op.a : op.b;
        const int literal = literalChar(varFirst ? op.b : op.a);
        if (op.compare == BytecodeCompareKind::Equals && var.kind == StrEntry::Variable && literal >= 0) {
            const uint32_t k = var.constant;
            sljit_jump *slow = sljit_emit_cmp(c_, SLJIT_NOT_EQUAL, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 4);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(&Runtime::stringChar[k]));
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0);
            sljit_emit_op2u(c_, SLJIT_SUB | SLJIT_SET_Z, SLJIT_R0, 0, SLJIT_IMM, literal);
            sljit_emit_op_flags(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), slotOffset(op.numberOut), SLJIT_EQUAL);
            sljit_label *done = sljit_emit_label(c_);
            addCold([this, slow, done, &op]() {
                sljit_set_label(slow, sljit_emit_label(c_));
                callStringOp(op);
                sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), done);
            });
            return;
        }
        callStringOp(op);
        return;
    }
    case BytecodeOp::LengthOfVar: {
        StringOp &op = newStringOp(StringOp::LengthOfVariable);
        op.variable = promoted(instr.a);
        op.numberOut = numberSlot(state_.numbers);
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        callStringOp(op);
        return;
    }
    case BytecodeOp::LetterOfVar: {
        StringOp &op = newStringOp(StringOp::LetterOfVariable);
        op.variable = promoted(instr.a);
        op.numberIn = numberSlot(state_.numbers - 1);
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        state_.numbers--;
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::LengthOf: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::LengthOf);
        op.value = describe(state_.values[depth - 1]);
        op.valuePosition = depth - 1;
        op.numberOut = numberSlot(state_.numbers);
        state_.values.pop_back();
        state_.numbers++;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        callStringOp(op);
        return;
    }
    case BytecodeOp::LetterOf: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::LetterOf);
        op.value = describe(state_.values[depth - 1]);
        op.valuePosition = depth - 1;
        op.numberIn = numberSlot(state_.numbers - 1);
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        state_.values.pop_back();
        state_.numbers--;
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::ConvertValueToString: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::ValueToString);
        op.value = describe(state_.values[depth - 1]);
        op.valuePosition = depth - 1;
        op.outPosition = static_cast<uint32_t>(state_.strings.size());
        state_.values.pop_back();
        callStringOp(op);
        StrEntry entry;
        entry.kind = StrEntry::Temp;
        state_.strings.push_back(entry);
        return;
    }
    case BytecodeOp::ConvertStringToValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        StringOp &op = newStringOp(StringOp::StringToValue);
        op.a = state_.strings.back();
        op.aPosition = static_cast<uint32_t>(state_.strings.size()) - 1;
        op.outPosition = depth;
        state_.strings.pop_back();
        callStringOp(op);
        AbsValue v;
        v.kind = ValueEntry::Temp;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

    case BytecodeOp::ForEachCheck: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const uint32_t counter = valueSlot(depth - 1), bound = valueSlot(depth - 2);
        const uint32_t k = promoted(instr.b);
        loadF(SLJIT_FR0, counter);
        nanToZero(SLJIT_FR0);
        loadF(SLJIT_FR1, bound);
        nanToZero(SLJIT_FR1);
        AbsState exited = state_;
        exited.values.pop_back();
        exited.values.pop_back();
        sljit_jump *done = sljit_emit_fcmp(c_, SLJIT_ORDERED_GREATER_EQUAL, SLJIT_FR0, 0, SLJIT_FR1, 0);
        sljit_emit_fset64(c_, SLJIT_FR1, 1.0);
        sljit_emit_fop2(c_, SLJIT_ADD_F64, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
        storeF(counter, SLJIT_FR0);
        storeF(variableSlot(k), SLJIT_FR0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_MEM1(SLJIT_S0), dirtyOffset(k), SLJIT_IMM, 1);
        state_.values.back().kind = ValueEntry::Number;
        emitJumpTo(done, instr.a, exited, instr.a <= pc);
        return;
    }

    case BytecodeOp::MathOp: {
        const uint32_t slot = numberSlot(state_.numbers - 1);
        const auto general = [this, slot, op = static_cast<sljit_sw>(instr.b)]() {
            loadF(SLJIT_FR0, slot);
            sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, op);
            callHelper(reinterpret_cast<void *>(&Runtime::jitMathOp), SLJIT_ARGS2(F64, F64, W));
            storeF(slot, SLJIT_FR0);
        };
        switch (static_cast<BytecodeMathOp>(instr.b)) {
        case BytecodeMathOp::Abs: {
            const sljit_s32 reg = readF(slot, SLJIT_FR0);
            sljit_emit_fop1(c_, SLJIT_ABS_F64, destF(slot, SLJIT_FR0), 0, reg, 0);
            commitF(slot, destF(slot, SLJIT_FR0));
            return;
        }
        case BytecodeMathOp::Floor:
            emitInlineRounding(Rounding::Floor, slot, general);
            return;
        case BytecodeMathOp::Ceiling:
            emitInlineRounding(Rounding::Ceiling, slot, general);
            return;
        default:
            general();
            return;
        }
    }

    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean: {
        emitNative(instr, plan_.nativeShape(instr));
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::Yield: {
        if (const int procedure = analysis_.procedureAt(pc); procedure >= 0 && static_cast<size_t>(procedure) < chunk_.procedures.size() && chunk_.procedures[procedure].runWithoutScreenRefresh) return;
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

} // namespace BytecodeJitInternal

#endif
