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
#include <map>
#include <sljitLir.h>
#include <string>
#include <vector>

namespace BytecodeJitInternal {

bool Emitter::emit() {
    const size_t count = chunk_.instructions.size();

    promotedIndex_.assign(chunk_.variableSlots.size(), -1);
    stringWritten_.assign(chunk_.variableSlots.size(), false);
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        if (!plan_.supported[pc]) continue;
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        const int64_t variable = plan_.promotedVariable(instr);
        if (variable < 0 || static_cast<size_t>(variable) >= promotedIndex_.size()) continue;
        if (instr.op == BytecodeOp::StoreVarString || instr.op == BytecodeOp::AppendVarString) stringWritten_[variable] = true;
        if (promotedIndex_[variable] < 0) {
            promotedIndex_[variable] = static_cast<int32_t>(code_.promoted.size());
            code_.promoted.push_back(static_cast<uint32_t>(variable));
        }
    }
    if (code_.promoted.size() > maxPromoted) return giveUp("too many variables kept in slots: " + std::to_string(code_.promoted.size()));

    code_.constantIsNumber.assign(chunk_.constants.size() + 1, 0);
    for (size_t i = 0; i < chunk_.constants.size(); i++) {
        const Value &constant = chunk_.constants[i];
        code_.constantIsNumber[i] = !constant.isString() && (constant.isRealNumber() || constant.isStringTyped());
    }

    for (BytecodeIndex pc = 0; pc < count; pc++) {
        BytecodeAbstractState s;
        if (!analysis_.stateBefore(pc, s)) continue;
        maxNumbers_ = std::max(maxNumbers_, s.numbers + 2);
        maxBooleans_ = std::max(maxBooleans_, s.booleans + 2);
        maxValues_ = std::max(maxValues_, static_cast<uint32_t>(s.values.size()) + 2);
        maxStrings_ = std::max(maxStrings_, s.strings + 2);
    }
    if (maxStrings_ > Runtime::maxStrs) return giveUp("string stack too deep for the temporaries: " + std::to_string(maxStrings_));
    if (maxValues_ + 2 > Runtime::maxTemps) return giveUp("value stack too deep for the temporaries: " + std::to_string(maxValues_));
    code_.slotBooleans = maxNumbers_;
    code_.slotValues = code_.slotBooleans + maxBooleans_;
    code_.slotVariables = code_.slotValues + maxValues_;
    if (code_.slotVariables + code_.promoted.size() > maxSlots) return giveUp("frame too large: " + std::to_string(code_.slotVariables + code_.promoted.size()) + " slots");

    c_ = sljit_create_compiler(nullptr);
    if (c_ == nullptr) return false;

    sljit_emit_enter(c_, 0, SLJIT_ARGS2(W, P, W), 5 | SLJIT_ENTER_FLOAT(firstPin + maxPins + 1), 6, 0);

    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_S1, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitEnter), SLJIT_ARGS2V(P, W));
    if (poisonPins_) {
        for (size_t i = 0; i < maxPins; i++)
            sljit_emit_fset64(c_, SLJIT_FR(firstPin + static_cast<int>(i)), -123456.789);
    }
    if (poisonPins_) {
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S3, 0, SLJIT_IMM, 0x7ffffff0);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S4, 0, SLJIT_IMM, 0x10);
        sljit_emit_op1(c_, SLJIT_MOV, SLJIT_S5, 0, SLJIT_IMM, 0x7ffffff0);
    }
    sljit_emit_op1(c_, SLJIT_MOV32, SLJIT_S2, 0, SLJIT_IMM, pollInterval());
    code_.entryTable.assign(plan_.entryPcs.size(), nullptr);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, reinterpret_cast<sljit_sw>(code_.entryTable.data()));
    sljit_emit_ijump(c_, SLJIT_JUMP, SLJIT_MEM2(SLJIT_R1, SLJIT_S1), SLJIT_WORD_SHIFT);

    findRegions();

    labels_.assign(count, nullptr);
    fused_.assign(count, false);
    condLeaf_.assign(count, 0);
    buildConditions();
    bool havePrevious = false;
    BytecodeIndex previous = 0;
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        const bool compiled = plan_.supported[pc] != 0;
        if (!compiled && !plan_.needsExit[pc]) continue;

        labels_[pc] = sljit_emit_label(c_);

        active_ = regionOf_[pc] >= 0 ? &regions_[regionOf_[pc]] : nullptr;
        if (active_ != nullptr && pc == active_->head) {
            emitReload(active_->span ? active_->liveHead : -1);
            emitHoistLoad();
            regions_[regionOf_[pc]].body = sljit_emit_label(c_);
        }

        if (!(havePrevious && previous + 1 == pc && !analysis_.isBlockStart(pc))) resetStateFromAnalysis(pc);
        havePrevious = true;
        previous = pc;

        if (compiled) {
            if (!fused_[pc]) {
                if (const auto found = conditions_.find(pc); found != conditions_.end()) emitCondition(found->second, pc);
                else emitInstruction(pc);
            }
            const BytecodeOp op = chunk_.instructions[pc].op;
            if (op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue || op == BytecodeOp::Return || op == BytecodeOp::Halt) havePrevious = false;
            if (active_ != nullptr && pc == active_->tail && op != BytecodeOp::Jump) emitSpill(liveNumbersAt(pc + 1));
        } else {
            emitExitAt(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), state_);
            havePrevious = false;
        }
    }

    active_ = nullptr;
    for (size_t i = 0; i < cold_.size(); i++) {
        auto emitCold = cold_[i];
        emitCold();
    }
    active_ = nullptr;

    sljit_label *exitLabel = sljit_emit_label(c_);
    for (sljit_jump *j : exitJumps_)
        sljit_set_label(j, exitLabel);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitExit), SLJIT_ARGS2(W, P, P));
    sljit_emit_return(c_, SLJIT_MOV, SLJIT_R0, 0);

    sljit_label *leaveLabel = sljit_emit_label(c_);
    for (sljit_jump *j : leaveJumps_)
        sljit_set_label(j, leaveLabel);
    sljit_emit_op2(c_, SLJIT_SUB, SLJIT_R1, 0, SLJIT_R0, 0, SLJIT_IMM, 1);
    sljit_emit_op1(c_, SLJIT_MOV, SLJIT_R0, 0, SLJIT_S0, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitLeave), SLJIT_ARGS2(W, P, W));
    sljit_emit_return(c_, SLJIT_MOV, SLJIT_R0, 0);

    for (auto &[jump, target] : pending_) {
        if (target >= count || labels_[target] == nullptr) {
            sljit_free_compiler(c_);
            c_ = nullptr;
            return false;
        }
        sljit_set_label(jump, labels_[target]);
    }

    for (auto &[jump, region] : pendingBody_)
        sljit_set_label(jump, region->body);

    std::vector<sljit_label *> entryLabels(plan_.entryPcs.size(), nullptr);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        const BytecodeIndex pc = plan_.entryPcs[i];
        if (pc >= count || regionOf_[pc] < 0 || pc == regions_[regionOf_[pc]].head) continue;
        active_ = &regions_[regionOf_[pc]];
        entryLabels[i] = sljit_emit_label(c_);
        emitReload();
        emitHoistLoad();
        sljit_set_label(sljit_emit_jump(c_, SLJIT_JUMP), labels_[pc]);
    }
    active_ = nullptr;
    if (pinViolation_) {
        sljit_free_compiler(c_);
        c_ = nullptr;
        return giveUp("a slot kept in a register was accessed behind its back");
    }

    code_.machine = sljit_generate_code(c_, 0, nullptr);
    if (code_.machine == nullptr) {
        sljit_free_compiler(c_);
        c_ = nullptr;
        return false;
    }
    code_.fn = reinterpret_cast<sljit_sw (*)(Context *, sljit_sw)>(code_.machine);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        code_.entryTable[i] = reinterpret_cast<void *>(sljit_get_label_addr(entryLabels[i] != nullptr ? entryLabels[i] : labels_[plan_.entryPcs[i]]));
    }
    sljit_free_compiler(c_);
    c_ = nullptr;
    return true;
}

} // namespace BytecodeJitInternal

#endif
