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

void Emitter::resetStateFromAnalysis(BytecodeIndex pc) {
    BytecodeAbstractState s;
    analysis_.stateBefore(pc, s);
    state_.numbers = s.numbers;
    state_.booleans = s.booleans;
    state_.strings.assign(s.strings, StrEntry{});
    state_.values.clear();
    for (BytecodeValueType type : s.values) {
        AbsValue v;
        if (type == BytecodeValueType::Number) v.kind = ValueEntry::Number;
        else if (type == BytecodeValueType::NumericString || type == BytecodeValueType::NumberLike) v.kind = ValueEntry::Like;
        else if (type == BytecodeValueType::Boolean) v.kind = ValueEntry::Boolean;
        else v.kind = ValueEntry::Opaque;
        state_.values.push_back(v);
    }
}

ExitDescriptor &Emitter::makeExit(BytecodeIndex pc, uint32_t result, const AbsState &state) {
    ExitDescriptor &d = code_.exits.emplace_back();
    d.strings = state.strings;
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

void Emitter::emitBackEdgeThenJump(BytecodeIndex target, const AbsState &stateAtTarget) {
    lir::emitOp2(c_, lir::SUB32 | lir::SET_Z, lir::S2, 0, lir::S2, 0, lir::IMM, 1);
    pendJump(lir::emitJump(c_, lir::NOT_ZERO), target);
    lir::emitOp1(c_, lir::MOV32, lir::S2, 0, lir::IMM, pollInterval());
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitPreempt), LIR_ARGS1(W, P));
    lir::Jump *keepGoing = lir::emitCmp(c_, lir::EQUAL, lir::R0, 0, lir::IMM, 0);
    emitExitAt(target, static_cast<uint32_t>(BytecodeJit::Result::Yield), stateAtTarget);
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
