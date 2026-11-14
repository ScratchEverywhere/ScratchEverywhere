#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cstdint>
#include <deque>
#include <functional>

namespace BytecodeJitInternal {

bool Emitter::stateFromBlockParameters(BytecodeIndex pc, AbsState &out) const {
    const BytecodeJitIR::BlockId id = blockStartingAt(pc);
    if (id == BytecodeJitIR::noBlock) return false;
    out = AbsState{};
    for (const BytecodeJitIR::ValueId param : hir_.function.blocks[id].params) {
        const BytecodeJitIR::ValueInfo &info = hir_.function.values[param];
        switch (info.type) {
        case BytecodeJitIR::Type::F64:
            out.numbers++;
            break;
        case BytecodeJitIR::Type::Bool:
            out.booleans++;
            break;
        case BytecodeJitIR::Type::Str:
            out.strings.push_back(StrEntry{});
            break;
        case BytecodeJitIR::Type::Val: {
            AbsValue v;
            if (info.kind == BytecodeValueType::Number) v.kind = ValueEntry::Number;
            else if (info.kind == BytecodeValueType::NumericString || info.kind == BytecodeValueType::NumberLike) v.kind = ValueEntry::Like;
            else if (info.kind == BytecodeValueType::Boolean) v.kind = ValueEntry::Boolean;
            else v.kind = ValueEntry::Opaque;
            out.values.push_back(v);
            break;
        }
        default:
            break;
        }
    }
    return true;
}

void Emitter::resetState(BytecodeIndex pc) {
    if (!stateFromBlockParameters(pc, state_)) stateUnknown_ = true;
}

ExitDescriptor &Emitter::makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state) {
    ExitDescriptor &d = code_.exits.emplace_back();
    d.strings = state.strings;
    d.frames = state.frames;
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
    emitSpill();
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(&d));
    exitJumps_.push_back(lir::emitJump(c_, lir::JUMP));
}

bool Emitter::emitFrameExit(BytecodeIndex pc, uint32_t result, const std::vector<BytecodeJitIR::ValueId> &frame, const std::function<FrameLocation(BytecodeJitIR::ValueId)> &locate) {
    AbsState state;
    for (const BytecodeJitIR::ValueId id : frame) {
        const BytecodeJitIR::Type type = hir_.function.values[id].type;
        if (type == BytecodeJitIR::Type::Str || type == BytecodeJitIR::Type::Color) return false;
    }
    for (const BytecodeJitIR::ValueId id : frame) {
        const FrameLocation where = locate(id);
        switch (hir_.function.values[id].type) {
        case BytecodeJitIR::Type::F64: {
            const uint32_t slot = numberSlot(state.numbers++);
            if (where.where == FrameLocation::FloatReg) storeF(slot, where.reg);
            else if (where.where == FrameLocation::Immediate) storeImmF(slot, where.immediate);
            break;
        }
        case BytecodeJitIR::Type::Bool: {
            const uint32_t slot = booleanSlot(state.booleans++);
            if (where.where == FrameLocation::WordReg) storeW(slot, where.reg);
            else if (where.where == FrameLocation::Immediate) storeImmW(slot, where.immediate != 0.0 ? 1 : 0);
            break;
        }
        default: {
            const uint32_t slot = valueSlot(static_cast<uint32_t>(state.values.size()));
            if (where.where == FrameLocation::FloatReg) storeF(slot, where.reg);
            else if (where.where == FrameLocation::Immediate) storeImmF(slot, where.immediate);
            AbsValue value;
            value.kind = where.kind;
            value.constant = where.reference;
            state.values.push_back(value);
            break;
        }
        }
    }
    emitExitAt(pc, result, state);
    return true;
}

void Emitter::emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget) {
    lir::emitOp2(c_, lir::SUB32 | lir::SET_Z, lir::S2, 0, lir::S2, 0, lir::IMM, 1);
    pendJump(lir::emitJump(c_, lir::NOT_ZERO), target);
    lir::emitOp1(c_, lir::MOV32, lir::S2, 0, lir::IMM, pollInterval());
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, static_cast<lir::Word>(target));
    callHelper(reinterpret_cast<void *>(&Runtime::jitPreempt), LIR_ARGS2(W, P, W));
    lir::Jump *keepGoing = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0);
    lir::Jump *switchCode = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 2);
    emitExitAt(target, static_cast<uint32_t>(BytecodeJit::Result::Yield), stateAtTarget);
    lir::setLabel(switchCode, lir::emitLabel(c_));
    emitExitAt(target, static_cast<uint32_t>(BytecodeJit::Result::Interpret), stateAtTarget);
    lir::setLabel(keepGoing, lir::emitLabel(c_));
    pendUnconditional(target);
}

void Emitter::emitJumpTo(lir::Jump *jump, BytecodeIndex target, const AbsState &stateAtTarget, bool backEdge) {
    if (!backEdge) {
        pendJump(jump, target);
        return;
    }
    addCold([this, jump, target, stateAtTarget]() {
        lir::setLabel(jump, lir::emitLabel(c_));
        emitBackEdgeThenJump(target, stateAtTarget);
    });
}

} // namespace BytecodeJitInternal

#endif
