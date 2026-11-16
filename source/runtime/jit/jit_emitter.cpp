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
#include <string>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::prepareHir() {
    std::vector<BytecodeIndex> blockStarts = plan_.entryPcs;
    for (BytecodeIndex pc = 0; pc + 1 < chunk_.instructions.size(); pc++) {
        const BytecodeOp op = chunk_.instructions[pc].op;
        if (op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue) blockStarts.push_back(pc + 1);
    }
    hir_ = BytecodeJitIR::build(chunk_, link_, analysis_, blockStarts, std::getenv("SE_JIT_FRAMEEXIT") != nullptr);
    BytecodeJitIR::computeFacts(hir_.function, chunk_);
    if (const char *check = std::getenv("SE_JIT_IR"); check != nullptr && std::string(check) != "0") {
        const std::vector<std::string> problems = BytecodeJitIR::verify(hir_.function);
        std::set<BytecodeIndex> starts;
        for (const BytecodeJitIR::Block &block : hir_.function.blocks)
            if (block.reachable) starts.insert(block.startPc);
        uint32_t entriesNotAtBlockStart = 0;
        for (const BytecodeIndex pc : plan_.entryPcs)
            if (pc < chunk_.instructions.size() && analysis_.isReachable(pc) && starts.count(pc) == 0) entriesNotAtBlockStart++;
        Log::log("[ir emit] " + std::to_string(hir_.function.blocks.size()) + " blocks with entry splits; builder errors " + std::to_string(hir_.errors.size()) + ", verifier errors " + std::to_string(problems.size()) + ", entry points not at a block start " + std::to_string(entriesNotAtBlockStart));
        for (size_t i = 0; i < problems.size() && i < 3; i++)
            Log::log("[ir emit]   " + problems[i]);
    }
    blockAt_.assign(chunk_.instructions.size() + 1, BytecodeJitIR::noBlock);
    for (BytecodeJitIR::BlockId id = 0; id < hir_.function.blocks.size(); id++) {
        const BytecodeJitIR::Block &block = hir_.function.blocks[id];
        if (!block.reachable) continue;
        for (BytecodeIndex at = block.startPc; at < block.endPc && at < blockAt_.size(); at++)
            blockAt_[at] = id;
    }
    instructionAt_.assign(chunk_.instructions.size(), {BytecodeJitIR::noBlock, 0});
    for (BytecodeJitIR::BlockId id = 0; id < hir_.function.blocks.size(); id++) {
        const BytecodeJitIR::Block &block = hir_.function.blocks[id];
        for (size_t i = 0; i < block.instrs.size(); i++)
            if (block.instrs[i].pc < instructionAt_.size()) instructionAt_[block.instrs[i].pc] = {id, static_cast<uint32_t>(i)};
    }
}

BytecodeJitIR::BlockId Emitter::labelFor(BytecodeIndex pc) const {
    if (inline_ != nullptr) {
        const auto found = inline_->labels.find(pc);
        return found == inline_->labels.end() ? BytecodeJitIR::noBlock : found->second;
    }
    return blockStartingAt(pc);
}

const BytecodeJitIR::Instr *Emitter::irInstruction(BytecodeIndex pc) const {
    if (pc >= instructionAt_.size() || instructionAt_[pc].first == BytecodeJitIR::noBlock) return nullptr;
    return &hir_.function.blocks[instructionAt_[pc].first].instrs[instructionAt_[pc].second];
}

bool Emitter::topNumberNonNan(BytecodeIndex pc) const {
    const BytecodeJitIR::Instr *instr = irInstruction(pc);
    if (instr == nullptr) return false;
    for (size_t i = instr->args.size(); i-- > 0;) {
        if (hir_.function.values[instr->args[i]].type != BytecodeJitIR::Type::F64) continue;
        return valueNonNan(instr->args[i]);
    }
    return false;
}

bool Emitter::boxedNumberNonNan(BytecodeIndex pc) const {
    const BytecodeJitIR::Instr *instr = irInstruction(pc);
    if (instr == nullptr || instr->args.empty()) return false;
    const BytecodeJitIR::ValueInfo &boxed = hir_.function.values[instr->args.back()];
    if (boxed.isParameter || boxed.block >= hir_.function.blocks.size()) return false;
    const BytecodeJitIR::Instr &def = hir_.function.blocks[boxed.block].instrs[boxed.index];
    return def.op == BytecodeJitIR::Op::Convert && def.bc == BytecodeOp::ConvertNumberToValue && !def.args.empty() && valueNonNan(def.args[0]);
}

bool Emitter::emit() {
    const size_t count = chunk_.instructions.size();
    prepareHir();
    computeInlinable();

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

    code_.strictNaN.assign(code_.promoted.size(), 0);
    for (BytecodeIndex pc = 2; pc < count; pc++) {
        const BytecodeOp op = chunk_.instructions[pc].op;
        if (op != BytecodeOp::ValueEquals && op != BytecodeOp::ValueLess && op != BytecodeOp::ValueGreater) continue;
        for (BytecodeIndex operand = pc - 2; operand < pc; operand++) {
            const BytecodeInstruction &push = chunk_.instructions[operand];
            if (push.op == BytecodeOp::PushVarValue && push.a < promotedIndex_.size() && promotedIndex_[push.a] >= 0) code_.strictNaN[promotedIndex_[push.a]] = 1;
        }
    }

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
    if (inlineReserveValues_ != 0 || inlineReserveNumbers_ != 0 || inlineReserveBooleans_ != 0) {
        const uint32_t numbers = maxNumbers_ + inlineReserveNumbers_, booleans = maxBooleans_ + inlineReserveBooleans_, values = maxValues_ + inlineReserveValues_;
        if (values + 2 > Runtime::maxTemps || numbers + booleans + values + code_.promoted.size() > maxSlots) {
            inlineInfo_.assign(chunk_.procedures.size(), InlineInfo{});
        } else {
            maxNumbers_ = numbers;
            maxBooleans_ = booleans;
            maxValues_ = values;
        }
    }
    if (maxStrings_ > Runtime::maxStrs) return giveUp("string stack too deep for the temporaries: " + std::to_string(maxStrings_));
    if (maxValues_ + 2 > Runtime::maxTemps) return giveUp("value stack too deep for the temporaries: " + std::to_string(maxValues_));
    code_.slotBooleans = maxNumbers_;
    code_.slotValues = code_.slotBooleans + maxBooleans_;
    code_.slotVariables = code_.slotValues + maxValues_;
    if (code_.slotVariables + code_.promoted.size() > maxSlots) return giveUp("frame too large: " + std::to_string(code_.slotVariables + code_.promoted.size()) + " slots");

    c_ = lir::createCompiler(ops_);
    if (c_ == nullptr) return false;

    lir::emitEnter(c_, LIR_ARGS2(W, P, W), 5, 6, firstPin + maxPins + 1, 0);

    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::S1, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitEnter), LIR_ARGS2(W, P, W));
    lir::Jump *declined = lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0);
    lir::Label *entered = lir::emitLabel(c_);
    addCold([this, declined, entered]() {
        lir::setLabel(declined, lir::emitLabel(c_));
        lir::emitReturn(c_, lir::MOV, lir::IMM, static_cast<lir::Word>(BytecodeJit::Result::Interpret));
    });
    (void)entered;
    if (poisonPins_) {
        for (size_t i = 0; i < maxPins; i++)
            lir::emitFSet64(c_, lir::FR(firstPin + static_cast<int>(i)), -123456.789);
    }
    if (poisonPins_) {
        lir::emitOp1(c_, lir::MOV, lir::S3, 0, lir::IMM, 0x7ffffff0);
        lir::emitOp1(c_, lir::MOV, lir::S4, 0, lir::IMM, 0x10);
        lir::emitOp1(c_, lir::MOV, lir::S5, 0, lir::IMM, 0x7ffffff0);
    }
    lir::emitOp1(c_, lir::MOV32, lir::S2, 0, lir::IMM, pollInterval());
    code_.entryTable.assign(plan_.entryPcs.size(), nullptr);
    lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(code_.entryTable.data()));
    lir::emitIJump(c_, lir::JUMP, lir::MEM2(lir::R1, lir::S1), lir::WORD_SHIFT);

    findRegions();
    surveyLowering();

    blockLabels_.assign(hir_.function.blocks.size(), nullptr);
    fused_.assign(count, false);
    condLeaf_.assign(count, 0);
    buildConditions();
    static const bool forceFrameExits = [] {
        const char *value = std::getenv("SE_JIT_FRAMEEXIT");
        return value != nullptr && std::atol(value) >= 3;
    }();
    static const long onlyFrameExitPc = [] {
        const char *value = std::getenv("SE_JIT_FRAMEEXIT_ONLY");
        return value != nullptr ? std::atol(value) : -1L;
    }();
    bool havePrevious = false;
    BytecodeIndex previous = 0;
    for (BytecodeJitIR::BlockId blockId = 0; blockId < hir_.function.blocks.size(); blockId++) {
        const BytecodeJitIR::Block &block = hir_.function.blocks[blockId];
        if (!block.reachable) continue;
        if (blockId < lowerSkip_.size() && lowerSkip_[blockId]) continue;
        if (lowerBlock(blockId)) {
            lowered_++;
            havePrevious = false;
            continue;
        }
        for (BytecodeIndex pc = block.startPc; pc < block.endPc && pc < count; pc++) {
            const bool compiled = plan_.supported[pc] != 0;
            if (!compiled && !plan_.needsExit[pc]) continue;

            if (pc == block.startPc) blockLabels_[blockId] = lir::emitLabel(c_);

            active_ = regionOf_[pc] >= 0 ? &regions_[regionOf_[pc]] : nullptr;
            if (active_ != nullptr && pc == active_->head) {
                emitReload(active_->span ? active_->liveHead : -1);
                emitHoistLoad();
                regions_[regionOf_[pc]].body = lir::emitLabel(c_);
            }

            if (!(havePrevious && previous + 1 == pc && !analysis_.isBlockStart(pc))) resetState(pc);
            havePrevious = true;
            previous = pc;

            if (compiled) {
                if (forceFrameExits && !fused_[pc] && pc == block.startPc && conditions_.find(pc) == conditions_.end() && (onlyFrameExitPc < 0 || onlyFrameExitPc == static_cast<long>(pc))) testFrameExit(pc, true);
                if (!fused_[pc]) {
                    if (const auto found = conditions_.find(pc); found != conditions_.end()) emitCondition(found->second, pc);
                    else emitInstruction(pc);
                }
                const BytecodeOp op = chunk_.instructions[pc].op;
                if (op == BytecodeOp::CallProcedure || op == BytecodeOp::CallProcedureValue || op == BytecodeOp::Return || op == BytecodeOp::Halt) havePrevious = false;
                if (active_ != nullptr && pc == active_->tail && op != BytecodeOp::Jump) emitSpill(liveNumbersAt(pc + 1));
            } else {
                if (!testFrameExit(pc)) emitExitAt(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), state_);
                havePrevious = false;
            }
        }
    }

    if (stateUnknown_) {
        lir::freeCompiler(c_);
        c_ = nullptr;
        return giveUp("the stack at an instruction does not match a block start");
    }

    active_ = nullptr;
    c_->coldLabel = lir::emitLabel(c_);
    for (size_t i = 0; i < cold_.size(); i++) {
        auto emitCold = cold_[i];
        emitCold();
    }
    active_ = nullptr;

    lir::Label *exitLabel = lir::emitLabel(c_);
    for (lir::Jump *j : exitJumps_)
        lir::setLabel(j, exitLabel);
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitExit), LIR_ARGS2(W, P, P));
    lir::emitReturn(c_, lir::MOV, lir::R0, 0);

    lir::Label *leaveLabel = lir::emitLabel(c_);
    for (lir::Jump *j : leaveJumps_)
        lir::setLabel(j, leaveLabel);
    lir::emitOp2(c_, lir::SUB, lir::R1, 0, lir::R0, 0, lir::IMM, 1);
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitLeave), LIR_ARGS2(W, P, W));
    lir::emitReturn(c_, lir::MOV, lir::R0, 0);

    for (auto &[jump, target] : pending_) {
        if (target == BytecodeJitIR::noBlock || target >= blockLabels_.size() || blockLabels_[target] == nullptr) {
            lir::freeCompiler(c_);
            c_ = nullptr;
            return false;
        }
        lir::setLabel(jump, blockLabels_[target]);
    }

    for (auto &[jump, region] : pendingBody_)
        lir::setLabel(jump, region->body);

    for (const BytecodeIndex pc : plan_.entryPcs) {
        const BytecodeJitIR::BlockId block = blockStartingAt(pc);
        if (block == BytecodeJitIR::noBlock || blockLabels_[block] == nullptr) {
            lir::freeCompiler(c_);
            c_ = nullptr;
            return false;
        }
    }

    std::vector<lir::Label *> entryLabels(plan_.entryPcs.size(), nullptr);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        const BytecodeIndex pc = plan_.entryPcs[i];
        if (pc >= count || regionOf_[pc] < 0 || pc == regions_[regionOf_[pc]].head) continue;
        active_ = &regions_[regionOf_[pc]];
        entryLabels[i] = lir::emitLabel(c_);
        active_ = nullptr;
        emitEntryCounterNormalization(pc);
        active_ = &regions_[regionOf_[pc]];
        emitReload();
        emitHoistLoad();
        lir::setLabel(lir::emitJump(c_, lir::JUMP), blockLabels_[blockStartingAt(pc)]);
    }
    active_ = nullptr;
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        const BytecodeIndex pc = plan_.entryPcs[i];
        if (entryLabels[i] != nullptr || pc >= count || !entryOnlyRepeatAt(pc)) continue;
        entryLabels[i] = lir::emitLabel(c_);
        emitEntryCounterNormalization(pc);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), blockLabels_[blockStartingAt(pc)]);
    }
    if (pinViolation_) {
        lir::freeCompiler(c_);
        c_ = nullptr;
        return giveUp("a slot kept in a register was accessed behind its back");
    }

    c_->entryLabels.assign(plan_.entryPcs.size(), nullptr);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++)
        c_->entryLabels[i] = entryLabels[i] != nullptr ? entryLabels[i] : blockLabels_[blockStartingAt(plan_.entryPcs[i])];
    code_.machine = lir::generateCode(c_);
    if (code_.machine == nullptr) {
        lir::freeCompiler(c_);
        c_ = nullptr;
        return false;
    }
    code_.ops = &ops_;
    code_.fn = reinterpret_cast<lir::Word (*)(Context *, lir::Word)>(code_.machine);
    for (size_t i = 0; i < plan_.entryPcs.size(); i++) {
        code_.entryTable[i] = ops_.labelAddress != nullptr ? reinterpret_cast<void *>(ops_.labelAddress(c_->entryLabels[i])) : nullptr;
    }
    lir::freeCompiler(c_);
    c_ = nullptr;
    return true;
}

} // namespace BytecodeJitInternal

#endif
