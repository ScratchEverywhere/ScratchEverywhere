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
        if (!shadowChangeFollows(pc) && !constantModFollows(pc) && !poolOperandFollows(pc)) storeImmF(numberSlot(state_.numbers), chunk_.numberConstants[instr.a]);
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
        const lir::Int reg = destF(to, lir::FR0);
        if (const int pin = pinOf(variableSlot(promoted(instr.a))); pin >= 0) lir::emitFOp1(c_, lir::MOV_F64, reg, 0, lir::FR(pin), 0);
        else loadF(reg, variableSlot(promoted(instr.a)));
        commitF(to, reg);
        state_.numbers++;
        return;
    }
    case BytecodeOp::StoreVarNumber: {
        const uint32_t k = promoted(instr.a);
        const lir::Int written = moveF(variableSlot(k), numberSlot(state_.numbers - 1));
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 1);
        state_.numbers--;
        if (!topNumberNonNan(pc)) guardNanWrite(written, pc + 1, k);
        return;
    }
    case BytecodeOp::ChangeVarNumber: {
        ensureVariableNumber(instr.a);
        const uint32_t k = promoted(instr.a);
        if (shadowSupported && active_ != nullptr && active_->shadowSlot == static_cast<int64_t>(variableSlot(k)) && pinOf(variableSlot(k)) >= 0) {
            const double amount = chunk_.numberConstants[chunk_.instructions[pc - 1].a];
            const lir::Int pin = lir::FR(pinOf(variableSlot(k)));
            lir::emitOp2U(c_, lir::SUB | lir::SET_OVERFLOW, lir::S1, 0, lir::IMM, 1);
            lir::Jump *notInteger = lir::emitJump(c_, lir::OVERFLOW_SET);
            lir::emitOp2(c_, lir::ADD, lir::S1, 0, lir::S1, 0, lir::IMM, static_cast<lir::Word>(amount));
            lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, pin, 0, lir::S1, 0);
            lir::Label *stored = lir::emitLabel(c_);
            lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 1);
            state_.numbers--;
            addCold([this, notInteger, stored, pin, amount]() {
                lir::setLabel(notInteger, lir::emitLabel(c_));
                lir::emitFSet64(c_, lir::FR1, amount);
                lir::emitFOp2(c_, lir::ADD_F64, pin, 0, pin, 0, lir::FR1, 0);
                lir::setLabel(lir::emitJump(c_, lir::JUMP), stored);
            });
            return;
        }
        const lir::Int variable = readF(variableSlot(k), lir::FR0);
        const lir::Int amount = readF(numberSlot(state_.numbers - 1), lir::FR1);
        lir::emitFOp2(c_, lir::ADD_F64, variable, 0, variable, 0, amount, 0);
        commitF(variableSlot(k), variable);
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 1);
        state_.numbers--;
        const bool finiteConstant = pc >= 1 && !analysis_.isBlockStart(pc) && chunk_.instructions[pc - 1].op == BytecodeOp::PushConstNumber && std::isfinite(chunk_.numberConstants[chunk_.instructions[pc - 1].a]);
        if (!finiteConstant) guardNanWrite(variable, pc + 1, k);
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
        if (entryOnlyRepeatAt(pc + 1)) {
            loadF(lir::FR0, numberSlot(state_.numbers - 1));
            lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R0, 0, lir::FR0, 0);
            lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, lir::FR1, 0, lir::R0, 0);
            lir::Jump *fractional = lir::emitFCmp(c_, lir::UNORDERED_OR_NOT_EQUAL, lir::FR0, 0, lir::FR1, 0);
            lir::Label *whole = lir::emitLabel(c_);
            addCold([this, fractional, whole]() {
                lir::setLabel(fractional, lir::emitLabel(c_));
                callHelper(reinterpret_cast<void *>(&Runtime::jitRepeatRound), LIR_ARGS1(F64, F64));
                lir::setLabel(lir::emitJump(c_, lir::JUMP), whole);
            });
            storeF(valueSlot(depth), lir::FR0);
        } else if (!convertFeedsStore(pc) && !feedsIndex(pc) && !feedsCompare(pc)) {
            loadF(lir::FR0, numberSlot(state_.numbers - 1));
            storeF(valueSlot(depth), lir::FR0);
        }
        AbsValue v;
        v.kind = ValueEntry::Number;
        state_.values.push_back(v);
        state_.numbers--;
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }
    case BytecodeOp::StoreVarBoolean: {
        const uint32_t k = promoted(instr.a);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(k));
        loadW(lir::R2, booleanSlot(state_.booleans - 1));
        callHelper(reinterpret_cast<void *>(&Runtime::jitStoreBoolean), LIR_ARGS3V(P, W, W));
        state_.booleans--;
        return;
    }
    case BytecodeOp::ConvertBooleanToValue: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const uint32_t boolSlot = booleanSlot(state_.booleans - 1);
        loadW(lir::R0, boolSlot);
        lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, lir::FR0, 0, lir::R0, 0);
        storeF(valueSlot(depth), lir::FR0);
        AbsValue v;
        v.kind = ValueEntry::Boolean;
        state_.values.push_back(v);
        state_.booleans--;
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
            if (!carried) loadF(lir::FR0, valueSlot(static_cast<uint32_t>(state_.values.size())));
            nanToZero(lir::FR0);
            storeF(numberSlot(state_.numbers), lir::FR0);
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
        const AbsState exitState = state_;
        const BytecodeIndex nextPc = pc + 1;
        if (top.kind == ValueEntry::Constant) {
            const double number = constantNumber(top.constant);
            storeImmF(variableSlot(k), number == number ? number : 0.0);
            lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 5 + static_cast<lir::Word>(top.constant));
        } else if (top.kind == ValueEntry::Number) {
            lir::Int written;
            bool clean = false;
            if (numberFedStore(pc)) {
                written = moveF(variableSlot(k), numberSlot(state_.numbers));
                clean = boxedNumberNonNan(pc);
            } else {
                loadF(lir::FR0, valueSlot(position));
                storeF(variableSlot(k), lir::FR0);
                written = lir::FR0;
            }
            lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 2);
            if (!clean) guardNanWrite(written, nextPc, k);
        } else {
            ShadowDescriptor &shadow = code_.shadows.emplace_back();
            shadow.variable = k;
            shadow.entry = describe(top);
            shadow.position = position;

            const bool strict = code_.strictNaN[k] != 0;
            const auto emitBoxed = [this, k, position, &shadow, exitState, nextPc, strict]() {
                lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
                lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&shadow));
                callHelper(reinterpret_cast<void *>(&Runtime::jitStoreShadow), LIR_ARGS2V(P, P));
                loadF(lir::FR0, valueSlot(position));
                if (strict) {
                    storeF(variableSlot(k), lir::FR0);
                    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 3);
                    lir::Jump *nan = lir::emitFCmp(c_, lir::UNORDERED, lir::FR0, 0, lir::FR0, 0);
                    addCold([this, nan, nextPc, exitState]() {
                        lir::setLabel(nan, lir::emitLabel(c_));
                        emitExitAt(nextPc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), exitState);
                    });
                } else {
                    lir::Jump *ordered = lir::emitFCmp(c_, lir::ORDERED_EQUAL, lir::FR0, 0, lir::FR0, 0);
                    lir::emitFSet64(c_, lir::FR0, 0.0);
                    lir::setLabel(ordered, lir::emitLabel(c_));
                    storeF(variableSlot(k), lir::FR0);
                    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 3);
                }
            };

            if (top.kind == ValueEntry::VarValue && top.constant < code_.promoted.size()) {
                const uint32_t source = top.constant;
                lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), dirtyOffset(source));
                lir::emitOp2(c_, lir::SUB, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
                lir::Jump *other = lir::emitCmp(c_, lir::GREATER, lir::R0, 0, lir::IMM, 1);
                moveF(variableSlot(k), variableSlot(source));
                lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 2);
                lir::Label *done = lir::emitLabel(c_);

                addCold([this, other, done, source, k, position, emitBoxed]() {
                    lir::setLabel(other, lir::emitLabel(c_));
                    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), dirtyOffset(source));
                    lir::Jump *boxed0 = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0);
                    lir::Jump *boxed3 = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 3);
                    lir::Jump *boxed4 = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 4);
                    loadF(lir::FR0, variableSlot(source));
                    storeF(variableSlot(k), lir::FR0);
                    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::R0, 0);
                    lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
                    lir::Label *general = lir::emitLabel(c_);
                    lir::setLabel(boxed0, general);
                    lir::setLabel(boxed3, general);
                    lir::setLabel(boxed4, general);
                    loadF(lir::FR0, variableSlot(source));
                    storeF(valueSlot(position), lir::FR0);
                    emitBoxed();
                    lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
                });
            } else if (top.kind == ValueEntry::Temp) {
                lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[position]));
                lir::Jump *boxedTemp = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, 1);
                loadF(lir::FR0, valueSlot(position));
                storeF(variableSlot(k), lir::FR0);
                lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 2);
                guardNanWrite(lir::FR0, nextPc, k);
                lir::Label *done = lir::emitLabel(c_);
                addCold([this, boxedTemp, done, emitBoxed]() {
                    lir::setLabel(boxedTemp, lir::emitLabel(c_));
                    emitBoxed();
                    lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
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
                lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
                lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&compare));
                callHelper(reinterpret_cast<void *>(&Runtime::jitCompareValues), LIR_ARGS2(W, P, P));
                storeW(result, lir::R0);
            };

            std::vector<lir::Jump *> notPlain;
            const bool inlinable = guardPlainNumber(state_.values[depth - 2], depth - 2, notPlain) && guardPlainNumber(state_.values[depth - 1], depth - 1, notPlain);
            if (inlinable) {
                emitCompareValue(kind, valueSlot(depth - 2), valueSlot(depth - 1));
                lir::Label *compared = lir::emitLabel(c_);
                addCold([this, notPlain, compared, emitHelper]() {
                    lir::Label *here = lir::emitLabel(c_);
                    for (lir::Jump *jump : notPlain)
                        lir::setLabel(jump, here);
                    emitHelper();
                    lir::setLabel(lir::emitJump(c_, lir::JUMP), compared);
                });
            } else {
                for (lir::Jump *jump : notPlain)
                    lir::setLabel(jump, lir::emitLabel(c_));
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
        if (inline_ != nullptr) {
            const AbsValue &argument = inline_->arguments[instr.a];
            AbsValue pushed;
            if (argument.kind == ValueEntry::Constant) {
                storeImmF(valueSlot(depth), chunk_.constants[argument.constant].numberRaw());
                pushed = argument;
            } else {
                loadF(lir::FR0, valueSlot(inline_->argBase + instr.a));
                storeF(valueSlot(depth), lir::FR0);
                pushed.kind = argument.kind == ValueEntry::Boolean ? ValueEntry::Boolean : ValueEntry::Number;
            }
            state_.values.push_back(pushed);
            maxValues_ = std::max(maxValues_, depth + 1);
            return;
        }
        loadF(lir::FR0, valueSlot(instr.a));
        storeF(valueSlot(depth), lir::FR0);
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
            static const bool disabled = !nativeFrames || std::getenv("SE_JIT_NOFASTCALL") != nullptr;
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
                    if (arg.kind == ValueEntry::Number || arg.kind == ValueEntry::Temp || arg.kind == ValueEntry::Argument || arg.kind == ValueEntry::VarValue) call.argNumbers |= 1u << j;
                    else if (double constantNumber; arg.kind == ValueEntry::Constant && arg.constant < chunk_.constants.size() && chunk_.constants[arg.constant].plainNumber(constantNumber)) call.argNumbers |= 1u << j;
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
        const AbsState beforeCall = state_;
        std::vector<lir::Jump *> inlineSlow;
        bool inlined = false;
        if (emitInlineCall(pc, instr, call, inlineSlow)) {
            if (inline_ != nullptr) {
                addCold([this, inlineSlow, pc, beforeCall]() {
                    lir::Label *here = lir::emitLabel(c_);
                    for (lir::Jump *jump : inlineSlow)
                        lir::setLabel(jump, here);
                    emitExitAt(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), beforeCall);
                });
                return;
            }
            pending_.push_back({lir::emitJump(c_, lir::JUMP), labelFor(pc + 1)});
            lir::Label *slowHere = lir::emitLabel(c_);
            for (lir::Jump *jump : inlineSlow)
                lir::setLabel(jump, slowHere);
            inlined = true;
        } else if (inline_ != nullptr) {
            inlineAbort_ = true;
            return;
        }
        std::vector<lir::Jump *> slowCall;
        if (call.nativeChecks) {
            emitNativeCall(call, proc.entryInstruction, slowCall);
            lir::Label *slowHere = lir::emitLabel(c_);
            for (lir::Jump *jump : slowCall)
                lir::setLabel(jump, slowHere);
        }
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&call));
        callHelper(reinterpret_cast<void *>(&Runtime::jitCall), LIR_ARGS2(W, P, P));
        lir::Jump *native = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0);
        leaveJumps_.push_back(lir::emitJump(c_, lir::JUMP));
        lir::setLabel(native, lir::emitLabel(c_));
        pending_.push_back({lir::emitJump(c_, lir::JUMP), labelFor(proc.entryInstruction)});
        return;
    }

    case BytecodeOp::Return:
    case BytecodeOp::Halt: {
        if (inline_ != nullptr) {
            emitInlineReturn(pc, instr.op == BytecodeOp::Return);
            return;
        }
        ReturnDescriptor &ret = code_.returns.emplace_back();
        ret.state = &makeExit(pc, 0, state_);
        ret.pc = pc;
        ret.isReturn = instr.op == BytecodeOp::Return;
        {
            static const bool disabled = !nativeFrames || std::getenv("SE_JIT_NOFASTCALL") != nullptr;
            if (ret.isReturn && !state_.values.empty()) {
                if (state_.values.back().kind == ValueEntry::Number) {
                    ret.resultMode = 1;
                    ret.resultPosition = static_cast<uint32_t>(state_.values.size()) - 1;
                } else {
                    ret.resultMode = 2;
                }
            }
            if (!disabled) {
                std::vector<lir::Jump *> slowReturn;
                emitNativeReturn(ret, slowReturn);
                lir::Label *slowHere = lir::emitLabel(c_);
                for (lir::Jump *jump : slowReturn)
                    lir::setLabel(jump, slowHere);
            }
        }
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&ret));
        callHelper(reinterpret_cast<void *>(&Runtime::jitReturn), LIR_ARGS2(W, P, P));
        lir::Jump *leave = lir::emitCmp(c_, lir::SIG_LESS, lir::R0, 0, lir::IMM, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(code_.entryTable.data()));
        lir::emitIJump(c_, lir::JUMP, lir::MEM2(lir::R1, lir::R0), lir::WORD_SHIFT);
        addCold([this, leave]() {
            lir::setLabel(leave, lir::emitLabel(c_));
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, 1 + static_cast<lir::Word>(BytecodeJit::Result::Interpret));
            leaveJumps_.push_back(lir::emitJump(c_, lir::JUMP));
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
            else callListHelper(reinterpret_cast<void *>(&Runtime::jitListItem), op, LIR_ARGS2V(P, P));
        } else {
            op.item = describe(state_.values[depth - 1]);
            op.itemPosition = depth - 1;
            std::vector<lir::Jump *> notPlain;
            const AbsValue &index = state_.values[depth - 1];
            const bool guardable = index.kind == ValueEntry::Temp || (index.kind == ValueEntry::VarValue && index.constant < code_.promoted.size());
            bool inlined = false;
            if (guardable) {
                guardPlainNumber(index, depth - 1, notPlain);
                inlined = emitInlineListItem(op, notPlain, reinterpret_cast<void *>(&Runtime::jitListItemAny), nullptr, convertCarries(pc + 1));
                if (inlined) fr0ItemPc_ = pc;
            }
            if (!inlined) callListHelper(reinterpret_cast<void *>(&Runtime::jitListItemAny), op, LIR_ARGS2V(P, P));
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
        callListHelper(helper, op, LIR_ARGS2V(P, P));
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
        callListHelper(reinterpret_cast<void *>(numeric ? &Runtime::jitListDelete : &Runtime::jitListDeleteAny), op, LIR_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::AddToList: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        op.itemPosition = depth - 1;
        op.item = describe(state_.values[depth - 1]);
        const bool inlined = emitInlineListAdd(op, state_.values[depth - 1]);
        state_.values.pop_back();
        if (!inlined) callListHelper(reinterpret_cast<void *>(&Runtime::jitListAdd), op, LIR_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::DeleteAllOfList: {
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListClear), op, LIR_ARGS2V(P, P));
        return;
    }
    case BytecodeOp::LengthOfList: {
        ListOpDescriptor &op = code_.listOps.emplace_back();
        op.list = instr.a;
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListLength), op, LIR_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), lir::FR0);
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
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListIndexOf), op, LIR_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), lir::FR0);
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
        callListHelper(reinterpret_cast<void *>(&Runtime::jitListContains), op, LIR_ARGS2(W, P, P));
        storeW(booleanSlot(state_.booleans), lir::R0);
        state_.booleans++;
        maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        return;
    }

    case BytecodeOp::PushVarValue: {
        ensureVariableNumber(instr.a);
        const uint32_t k = promoted(instr.a);
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        if (!feedsCompare(pc) && !feedsIndex(pc) && !feedsVarStore(pc)) {
            loadF(lir::FR0, variableSlot(k));
            storeF(valueSlot(depth), lir::FR0);
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
        loadW(lir::R0, booleanSlot(state_.booleans - 1));
        state_.booleans--;
        const bool backEdge = instr.a <= pc;
        lir::Jump *j = lir::emitCmp(c_, instr.op == BytecodeOp::JumpIfTrue ? lir::NOT_EQUAL : lir::EQUAL, lir::R0, 0, lir::IMM, 0);
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
            lir::emitOp2(c_, lir::SUB | lir::SET_SIG_LESS, lir::S3, 0, lir::S3, 0, lir::IMM, 1);
            lir::Jump *finished = lir::emitJump(c_, lir::SIG_LESS);
            state_.values.back().kind = ValueEntry::Number;
            AbsState exited = state_;
            exited.values.pop_back();
            emitJumpTo(finished, instr.a, exited, instr.a <= pc);
            return;
        }
        loadF(lir::FR0, counter);

        const bool wholeCounter = wholeCounterAt(pc) || entryOnlyRepeatAt(pc);
        if (wholeCounter) {
            lir::emitFSet64(c_, lir::FR1, 1.0);
            lir::emitFOp2(c_, lir::SUB_F64, lir::FR0, 0, lir::FR0, 0, lir::FR1, 0);
        } else {
            lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R0, 0, lir::FR0, 0);
            lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, lir::FR1, 0, lir::R0, 0);
            lir::Jump *fractional = lir::emitFCmp(c_, lir::UNORDERED_OR_NOT_EQUAL, lir::FR0, 0, lir::FR1, 0);
            lir::emitFSet64(c_, lir::FR1, 1.0);
            lir::emitFOp2(c_, lir::SUB_F64, lir::FR0, 0, lir::FR0, 0, lir::FR1, 0);
            lir::Label *counted = lir::emitLabel(c_);
            addCold([this, fractional, counted]() {
                lir::setLabel(fractional, lir::emitLabel(c_));
                callHelper(reinterpret_cast<void *>(&Runtime::jitRepeatRemaining), LIR_ARGS1(F64, F64));
                lir::setLabel(lir::emitJump(c_, lir::JUMP), counted);
            });
        }
        lir::emitFSet64(c_, lir::FR1, 0.0);
        state_.values.back().kind = ValueEntry::Number;
        AbsState exited = state_;
        exited.values.pop_back();
        lir::Jump *done = lir::emitFCmp(c_, lir::ORDERED_LESS, lir::FR0, 0, lir::FR1, 0);
        storeF(counter, lir::FR0);
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
        if (op.kind == StringOp::AppendVariable && emitInlineAppend(op)) return;
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
            lir::Jump *slow = lir::emitCmp(c_, lir::NOT_EQUAL, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 4);
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::stringChar[k]));
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::R0), 0);
            lir::emitOp2U(c_, lir::SUB | lir::SET_Z, lir::R0, 0, lir::IMM, literal);
            storeFlag(op.numberOut, lir::EQUAL);
            lir::Label *done = lir::emitLabel(c_);
            addCold([this, slow, done, &op]() {
                lir::setLabel(slow, lir::emitLabel(c_));
                callStringOp(op);
                lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
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
        if (emitInlineLetterEquals(op, pc)) return;
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

    case BytecodeOp::SetForEachBound: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const uint32_t boundSlot = valueSlot(depth - 2);
        loadF(lir::FR0, numberSlot(state_.numbers - 1));
        storeF(boundSlot, lir::FR0);
        state_.numbers--;
        state_.values[depth - 2].kind = ValueEntry::Number;
        return;
    }

    case BytecodeOp::ForEachCheck: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        const uint32_t counter = valueSlot(depth - 1), bound = valueSlot(depth - 2);
        const uint32_t k = promoted(instr.b);
        const lir::Int counterReg = readF(counter, lir::FR0);
        const lir::Int boundReg = readF(bound, lir::FR1);
        nanToZero(boundReg);
        AbsState exited = state_;
        exited.values.pop_back();
        exited.values.pop_back();
        lir::Jump *done = lir::emitFCmp(c_, lir::ORDERED_GREATER_EQUAL, counterReg, 0, boundReg, 0);
        lir::emitFOp2(c_, lir::ADD_F64, counterReg, 0, counterReg, 0, lir::MEM1(lir::S0), static_cast<lir::Word>(offsetof(Context, one)));
        commitF(counter, counterReg);
        storeF(variableSlot(k), counterReg);
        lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 1);
        state_.values.back().kind = ValueEntry::Number;
        emitJumpTo(done, instr.a, exited, instr.a <= pc);
        return;
    }

    case BytecodeOp::MathOp: {
        const uint32_t slot = numberSlot(state_.numbers - 1);
        const auto general = [this, slot, op = static_cast<lir::Word>(instr.b)]() {
            loadF(lir::FR0, slot);
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, op);
            callHelper(reinterpret_cast<void *>(&Runtime::jitMathOp), LIR_ARGS2(F64, F64, W));
            storeF(slot, lir::FR0);
        };
        switch (static_cast<BytecodeMathOp>(instr.b)) {
        case BytecodeMathOp::Abs: {
            const lir::Int reg = readF(slot, lir::FR0);
            lir::emitFOp1(c_, lir::ABS_F64, destF(slot, lir::FR0), 0, reg, 0);
            commitF(slot, destF(slot, lir::FR0));
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

    case BytecodeOp::RandomInt: {
        const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
        loadF(lir::FR0, a);
        loadF(lir::FR1, b);
        callHelper(reinterpret_cast<void *>(&Runtime::jitRandomInt), LIR_ARGS2(F64, F64, F64));
        storeF(a, lir::FR0);
        state_.numbers -= 1;
        return;
    }

    case BytecodeOp::ConvertValueToColor:
    case BytecodeOp::ConvertValueToBoolean: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ValueCallDescriptor &call = code_.valueCalls.emplace_back();
        call.itemPosition = depth - 1;
        call.item = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&call));
        if (instr.op == BytecodeOp::ConvertValueToColor) {
            callHelper(reinterpret_cast<void *>(&Runtime::jitConvertValueToColor), LIR_ARGS2V(P, P));
        } else {
            callHelper(reinterpret_cast<void *>(&Runtime::jitConvertValueToBoolean), LIR_ARGS2(I32, P, P));
            lir::emitOp1(c_, lir::MOV_U8, lir::R0, 0, lir::R0, 0);
            storeW(booleanSlot(state_.booleans), lir::R0);
            state_.booleans += 1;
            maxBooleans_ = std::max(maxBooleans_, state_.booleans);
        }
        return;
    }

    case BytecodeOp::Random: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        ValueCallDescriptor &call = code_.valueCalls.emplace_back();
        call.itemPosition = depth - 2;
        call.item = describe(state_.values[depth - 2]);
        call.secondPosition = depth - 1;
        call.second = describe(state_.values[depth - 1]);
        state_.values.pop_back();
        state_.values.pop_back();
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&call));
        callHelper(reinterpret_cast<void *>(&Runtime::jitRandomValues), LIR_ARGS2(F64, P, P));
        storeF(numberSlot(state_.numbers), lir::FR0);
        state_.numbers += 1;
        maxNumbers_ = std::max(maxNumbers_, state_.numbers);
        return;
    }

    case BytecodeOp::CostumeNumberName: {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(depth));
        lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::IMM, instr.b == 0 ? 0 : 1);
        callHelper(reinterpret_cast<void *>(&Runtime::jitCostumeNumberName), LIR_ARGS3V(P, W, W));
        AbsValue v;
        v.kind = ValueEntry::Temp;
        state_.values.push_back(v);
        maxValues_ = std::max(maxValues_, depth + 1);
        return;
    }

    case BytecodeOp::PushConstColor:
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(instr.a));
        callHelper(reinterpret_cast<void *>(&Runtime::jitPushColorConst), LIR_ARGS2V(P, W));
        return;

    case BytecodeOp::PushVarColor: {
        const int32_t k = instr.a < promotedIndex_.size() ? promotedIndex_[instr.a] : -1;
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(k >= 0 ? k : instr.a));
        callHelper(reinterpret_cast<void *>(k >= 0 ? &Runtime::jitPushColorPromoted : &Runtime::jitPushColorVariable), LIR_ARGS2V(P, W));
        return;
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
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), warpOffset());
        lir::Jump *warp = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0);
        emitExitAt(pc + 1, static_cast<uint32_t>(BytecodeJit::Result::Yield), state_);
        lir::setLabel(warp, lir::emitLabel(c_));
        return;
    }

    default:
        return;
    }
}

} // namespace BytecodeJitInternal

#endif
