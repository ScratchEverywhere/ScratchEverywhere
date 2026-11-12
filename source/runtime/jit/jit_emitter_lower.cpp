#ifdef SE_JIT

#include "bytecode_disassembler.hpp"
#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <log.hpp>
#include <map>
#include <string>
#include <vector>

namespace BytecodeJitInternal {

namespace {

struct Num {
    enum Kind : uint8_t { None,
                          Shadow,
                          Constant,
                          Register };
    Kind kind = None;
    int64_t offset = 0;
    double value = 0.0;
    lir::Int reg = 0;
};

} // namespace

bool Emitter::lowerBlock(BytecodeJitIR::BlockId blockId) {
    const long limit = lowerLimit();
    if (limit <= 0 || lowered_ >= static_cast<uint32_t>(limit) || !shadowSupported) return false;
    namespace IR = BytecodeJitIR;
    const std::vector<IR::Block> &blocks = hir_.function.blocks;
    const IR::Block &head = blocks[blockId];
    if (head.term != IR::Terminator::CondBr || head.instrs.empty() || blockId + 2 >= blocks.size()) return false;
    const IR::BlockId armId = head.edges[0].target, joinId = head.edges[1].target;
    if (armId != blockId + 1 || joinId != blockId + 2) return false;
    const IR::Block &arm = blocks[armId];
    const IR::Block &join = blocks[joinId];
    if (!arm.reachable || arm.term != IR::Terminator::Br || arm.edges[0].target != joinId) return false;
    if (head.edges[0].args != head.params || head.edges[1].args != head.params || arm.edges[0].args != arm.params || arm.params.size() != head.params.size() || join.params.size() != head.params.size()) return false;
    uint32_t armPredecessors = 0;
    for (const IR::Block &other : blocks) {
        if (!other.reachable) continue;
        const int edges = other.term == IR::Terminator::Br ? 1 : other.term == IR::Terminator::CondBr ? 2
                                                                                                      : 0;
        for (int e = 0; e < edges; e++)
            if (other.edges[e].target == armId) armPredecessors++;
    }
    if (armPredecessors != 1) return false;
    if (head.startPc >= regionOf_.size() || regionOf_[head.startPc] < 0 || regionOf_[arm.startPc] != regionOf_[head.startPc]) return false;
    const Region &region = regions_[regionOf_[head.startPc]];
    const Runtime::ListLayout &layout = Runtime::listLayout();
    if (region.hoistList < 0 || region.shadowSlot < 0 || !layout.ok || layout.flags != layout.tag + 1) return false;
    for (const IR::ValueId param : head.params)
        if (hir_.function.values[param].type == IR::Type::Str || hir_.function.values[param].type == IR::Type::Color) return false;
    const Region *saved = active_;
    active_ = &region;
    if (pinOf(static_cast<uint32_t>(region.shadowSlot)) < 0) {
        active_ = saved;
        return false;
    }

    std::vector<int64_t> pointers;
    const uint16_t endianProbe = 1;
    const bool little = *reinterpret_cast<const uint8_t *>(&endianProbe) == 1;
    const lir::Word expectedTag = little ? layout.numberTag : layout.numberTag << 8;

    struct Pending {
        IR::ValueId left = IR::noValue, right = IR::noValue;
        BytecodeOp op = BytecodeOp::Nop;
        bool seen = false;
    };

    const auto evaluate = [&](bool emit, lir::Jump *&falseJump, std::vector<lir::Jump *> &slow) -> bool {
        std::vector<Num> value(hir_.function.values.size());
        std::map<uint32_t, Num> variables;
        std::map<int64_t, Num> items;
        int scratch = 0;
        Pending compare;
        bool ok = true;
        const auto allocate = [&]() -> lir::Int {
            if (scratch >= 4) {
                ok = false;
                return lir::FR0;
            }
            return lir::FR(scratch++);
        };
        const auto pointerFor = [&](int64_t offset) -> lir::Int {
            for (size_t i = 0; i < pointers.size(); i++)
                if (pointers[i] == offset) return lir::R3 + static_cast<lir::Int>(i);
            if (emit || pointers.size() >= 3) {
                ok = false;
                return lir::R3;
            }
            pointers.push_back(offset);
            return lir::R3 + static_cast<lir::Int>(pointers.size() - 1);
        };
        const auto integral = [](const Num &n) { return n.kind == Num::Constant && n.value == std::floor(n.value) && std::fabs(n.value) < 1073741824.0; };
        const auto step = [&](const IR::Instr &instr, bool last) -> bool {
            const IR::ValueId result = instr.results.empty() ? IR::noValue : instr.results[0];
            switch (instr.bc) {
            case BytecodeOp::PushConstNumber: {
                Num n;
                n.kind = Num::Constant;
                n.value = chunk_.numberConstants[instr.a];
                if (n.value != n.value) return false;
                value[result] = n;
                return true;
            }
            case BytecodeOp::PushConstValue: {
                const Value &constant = chunk_.constants[instr.a];
                if (!constant.isRealNumber()) return false;
                Num n;
                n.kind = Num::Constant;
                n.value = constant.get<double>();
                if (n.value != n.value) return false;
                value[result] = n;
                return true;
            }
            case BytecodeOp::PushVarNumber:
            case BytecodeOp::PushVarValue: {
                if (instr.a >= promotedIndex_.size() || promotedIndex_[instr.a] < 0) return false;
                const uint32_t slot = variableSlot(promoted(instr.a));
                Num n;
                if (slot == static_cast<uint32_t>(region.shadowSlot)) {
                    n.kind = Num::Shadow;
                } else if (const auto known = variables.find(slot); known != variables.end()) {
                    n = known->second;
                } else if (const int pin = pinOf(slot); pin >= 0) {
                    n.kind = Num::Register;
                    n.reg = lir::FR(pin);
                } else {
                    return false;
                }
                value[result] = n;
                return true;
            }
            case BytecodeOp::ConvertNumberToValue:
            case BytecodeOp::ConvertValueToNumber:
                if (instr.args.size() != 1 || value[instr.args[0]].kind == Num::None) return false;
                value[result] = value[instr.args[0]];
                return true;
            case BytecodeOp::CallNativeNumber: {
                if (instr.op != IR::Op::Arith || instr.args.size() != 2) return false;
                const IR::ArithKind kind = static_cast<IR::ArithKind>(instr.sub);
                const Num x = value[instr.args[0]], y = value[instr.args[1]];
                Num n;
                n.kind = Num::Shadow;
                if (kind == IR::ArithKind::Add && x.kind == Num::Shadow && integral(y)) n.offset = x.offset + static_cast<int64_t>(y.value);
                else if (kind == IR::ArithKind::Add && integral(x) && y.kind == Num::Shadow) n.offset = y.offset + static_cast<int64_t>(x.value);
                else if (kind == IR::ArithKind::Subtract && x.kind == Num::Shadow && integral(y)) n.offset = x.offset - static_cast<int64_t>(y.value);
                else return false;
                value[result] = n;
                return true;
            }
            case BytecodeOp::ItemOfList: {
                if (instr.a != static_cast<uint32_t>(region.hoistList) || instr.args.size() != 1 || value[instr.args[0]].kind != Num::Shadow) return false;
                const int64_t offset = value[instr.args[0]].offset;
                const lir::Int pointer = pointerFor(offset);
                if (const auto cached = items.find(offset); cached != items.end()) {
                    value[result] = cached->second;
                    return ok;
                }
                Num n;
                n.kind = Num::Register;
                n.reg = allocate();
                if (emit) lir::emitFOp1(c_, lir::MOV_F64, n.reg, 0, lir::MEM1(pointer), layout.number);
                items[offset] = n;
                value[result] = n;
                return ok;
            }
            case BytecodeOp::StoreVarValue: {
                if (instr.args.size() != 1 || value[instr.args[0]].kind != Num::Register || instr.a >= promotedIndex_.size() || promotedIndex_[instr.a] < 0) return false;
                const uint32_t k = promoted(instr.a);
                const uint32_t slot = variableSlot(k);
                if (slot == static_cast<uint32_t>(region.shadowSlot) || isCounterPin(slot)) return false;
                if (emit) {
                    storeF(slot, value[instr.args[0]].reg);
                    lir::emitOp1(c_, lir::MOV, lir::MEM1(lir::S0), dirtyOffset(k), lir::IMM, 2);
                }
                Num n = value[instr.args[0]];
                if (const int pin = pinOf(slot); pin >= 0) {
                    n.reg = lir::FR(pin);
                    if (emit) {
                    }
                }
                variables[slot] = n;
                return true;
            }
            case BytecodeOp::ReplaceInList: {
                if (instr.a != static_cast<uint32_t>(region.hoistList) || instr.args.size() != 2 || value[instr.args[0]].kind != Num::Register || value[instr.args[1]].kind != Num::Shadow) return false;
                const int64_t offset = value[instr.args[1]].offset;
                const lir::Int pointer = pointerFor(offset);
                const Num item = value[instr.args[0]];
                if (emit) {
                    lir::emitOp1(c_, lir::MOV_U16, lir::MEM1(pointer), layout.tag, lir::IMM, expectedTag);
                    lir::emitFOp1(c_, lir::MOV_F64, lir::MEM1(pointer), layout.number, item.reg, 0);
                }
                items[offset] = item;
                return ok;
            }
            case BytecodeOp::ValueGreater:
            case BytecodeOp::ValueLess:
            case BytecodeOp::ValueEquals:
                if (!last || instr.args.size() != 2 || value[instr.args[0]].kind != Num::Register || value[instr.args[1]].kind != Num::Register) return false;
                compare.left = instr.args[0];
                compare.right = instr.args[1];
                compare.op = instr.bc;
                compare.seen = true;
                return true;
            default:
                return false;
            }
        };
        for (size_t i = 0; i < head.instrs.size(); i++)
            if (!step(head.instrs[i], i + 1 == head.instrs.size()) || !ok) return false;
        if (!compare.seen || head.cond != head.instrs.back().results[0]) return false;
        if (emit) {
            const lir::Int x = value[compare.left].reg, y = value[compare.right].reg;
            slow.push_back(lir::emitFCmp(c_, lir::UNORDERED, x, 0, y, 0));
            const lir::Int whenFalse = compare.op == BytecodeOp::ValueGreater ? lir::ORDERED_LESS_EQUAL : compare.op == BytecodeOp::ValueLess ? lir::ORDERED_GREATER_EQUAL
                                                                                                                                              : lir::ORDERED_NOT_EQUAL;
            falseJump = lir::emitFCmp(c_, whenFalse, x, 0, y, 0);
        }
        for (const IR::Instr &instr : arm.instrs)
            if (!step(instr, false) || !ok) return false;
        return true;
    };

    lir::Jump *unusedJump = nullptr;
    std::vector<lir::Jump *> unusedSlow;
    if (!evaluate(false, unusedJump, unusedSlow)) {
        active_ = saved;
        return false;
    }

    blockLabels_[blockId] = lir::emitLabel(c_);
    std::vector<lir::Jump *> slow;
    slow.push_back(lir::emitCmp(c_, lir::EQUAL, lir::S1, 0, lir::IMM, shadowNone));
    for (size_t i = 0; i < pointers.size(); i++) {
        const lir::Int pointer = lir::R3 + static_cast<lir::Int>(i);
        lir::emitOp2(c_, lir::ADD, pointer, 0, lir::S1, 0, lir::IMM, static_cast<lir::Word>(pointers[i] - 1));
        slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, pointer, 0, lir::IMM, 0x7fffffff));
        lir::emitOp2(c_, lir::MUL, pointer, 0, pointer, 0, lir::IMM, layout.valueSize);
        lir::emitOp2(c_, lir::ADD, pointer, 0, pointer, 0, lir::S4, 0);
        slow.push_back(lir::emitCmp(c_, lir::GREATER_EQUAL, pointer, 0, lir::S5, 0));
        lir::emitOp1(c_, lir::MOV_U16, lir::R2, 0, lir::MEM1(pointer), layout.tag);
        slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R2, 0, lir::IMM, expectedTag));
    }
    lir::Jump *falseJump = nullptr;
    const bool ok = evaluate(true, falseJump, slow);
    if (!ok || falseJump == nullptr) {
        active_ = saved;
        stateUnknown_ = true;
        return true;
    }
    pendJump(falseJump, join.startPc);
    blockLabels_[armId] = lir::emitLabel(c_);
    AbsState entry;
    stateFromBlockParameters(head.startPc, entry);
    addCold([this, slow, entry, pc = head.startPc]() {
        lir::Label *here = lir::emitLabel(c_);
        for (lir::Jump *jump : slow)
            lir::setLabel(jump, here);
        emitExitAt(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), entry);
    });
    active_ = saved;
    if (lowerSkip_.size() < blocks.size()) lowerSkip_.assign(blocks.size(), 0);
    lowerSkip_[armId] = 1;
    if (std::getenv("SE_JIT_LOWER_LOG") != nullptr) Log::log("[lower] trace at pc " + std::to_string(head.startPc) + " (" + std::to_string(pointers.size()) + " guarded items)");
    return true;
}

void Emitter::emitEntryCounterNormalization(BytecodeIndex pc) {
    if (!entryOnlyRepeatAt(pc)) return;
    AbsState entry;
    if (!stateFromBlockParameters(pc, entry) || entry.values.empty()) return;
    const uint32_t counter = valueSlot(static_cast<uint32_t>(entry.values.size()) - 1);
    loadF(lir::FR0, counter);
    lir::emitFOp1(c_, lir::CONV_SW_FROM_F64, lir::R0, 0, lir::FR0, 0);
    lir::emitFOp1(c_, lir::CONV_F64_FROM_SW, lir::FR1, 0, lir::R0, 0);
    lir::Jump *whole = lir::emitFCmp(c_, lir::ORDERED_EQUAL, lir::FR0, 0, lir::FR1, 0);
    callHelper(reinterpret_cast<void *>(&Runtime::jitRepeatRound), LIR_ARGS1(F64, F64));
    storeF(counter, lir::FR0);
    lir::setLabel(whole, lir::emitLabel(c_));
}

bool Emitter::testFrameExit(BytecodeIndex pc, bool conditional) {
    static const long mode = [] {
        const char *value = std::getenv("SE_JIT_FRAMEEXIT");
        return value != nullptr ? std::atol(value) : 0;
    }();
    if (mode <= 0) return false;
    const BytecodeJitIR::Instr *instr = irInstruction(pc);
    const std::vector<BytecodeJitIR::ValueId> *frameValues = instr != nullptr ? &instr->frame : nullptr;
    if (frameValues == nullptr && pc < blockAt_.size() && blockAt_[pc] != BytecodeJitIR::noBlock) {
        const BytecodeJitIR::Block &block = hir_.function.blocks[blockAt_[pc]];
        if (block.endPc == pc + 1) frameValues = &block.termFrame;
    }
    if (frameValues == nullptr) {
        if (std::getenv("SE_JIT_FRAMEEXIT_LOG") != nullptr) Log::log("[frame exit] no frame at pc " + std::to_string(pc) + " op " + std::to_string(static_cast<int>(chunk_.instructions[pc].op)) + " block " + std::to_string(pc < blockAt_.size() ? blockAt_[pc] : 0xffffffffu));
        return false;
    }
    const std::vector<BytecodeJitIR::ValueId> &frame = *frameValues;
    uint32_t numbers = 0, booleans = 0, values = 0;
    for (const BytecodeJitIR::ValueId id : frame) {
        switch (hir_.function.values[id].type) {
        case BytecodeJitIR::Type::F64:
            numbers++;
            break;
        case BytecodeJitIR::Type::Bool:
            booleans++;
            break;
        case BytecodeJitIR::Type::Str:
        case BytecodeJitIR::Type::Color:
            return false;
        default:
            values++;
            break;
        }
    }
    if (!state_.strings.empty() || !state_.frames.empty()) return false;
    if (numbers != state_.numbers || booleans != state_.booleans || values != state_.values.size()) {
        Log::log("[frame exit] pc " + std::to_string(pc) + ": frame has " + std::to_string(numbers) + "/" + std::to_string(booleans) + "/" + std::to_string(values) + " numbers/booleans/values, emitter state " + std::to_string(state_.numbers) + "/" + std::to_string(state_.booleans) + "/" + std::to_string(state_.values.size()));
        return false;
    }
    std::map<BytecodeJitIR::ValueId, FrameLocation> where;
    std::vector<std::function<void()>> loads;
    uint32_t numberIndex = 0, booleanIndex = 0, valueIndex = 0;
    for (const BytecodeJitIR::ValueId id : frame) {
        const BytecodeJitIR::ValueInfo &info = hir_.function.values[id];
        FrameLocation location;
        if (info.type == BytecodeJitIR::Type::F64) {
            const uint32_t slot = numberSlot(numberIndex);
            const BytecodeJitIR::Instr *def = info.isParameter ? nullptr : &hir_.function.blocks[info.block].instrs[info.index];
            if (mode >= 2 && def != nullptr && def->bc == BytecodeOp::PushConstNumber) {
                location.where = FrameLocation::Immediate;
                location.immediate = chunk_.numberConstants[def->a];
            } else if (mode >= 2 && numberIndex < 3) {
                location.where = FrameLocation::FloatReg;
                location.reg = lir::FR(numberIndex);
                loads.push_back([this, reg = location.reg, slot]() { loadF(reg, slot); });
            }
            numberIndex++;
        } else if (info.type == BytecodeJitIR::Type::Bool) {
            if (mode >= 2 && booleanIndex < 2) {
                location.where = FrameLocation::WordReg;
                location.reg = booleanIndex == 0 ? lir::R0 : lir::R1;
                loads.push_back([this, reg = location.reg, slot = booleanSlot(booleanIndex)]() { loadW(reg, slot); });
            }
            booleanIndex++;
        } else {
            location.kind = state_.values[valueIndex].kind;
            location.reference = state_.values[valueIndex].constant;
            valueIndex++;
        }
        where[id] = location;
    }
    auto emitExit = [this, pc, frame, where, loads]() mutable {
        for (const auto &load : loads)
            load();
        return emitFrameExit(pc, static_cast<uint32_t>(BytecodeJit::Result::Interpret), frame, [&](BytecodeJitIR::ValueId id) { return where[id]; });
    };
    if (!conditional) return emitExit();
    static uint8_t arrivals[256];
    static uint32_t slot = 0;
    uint8_t *counter = &arrivals[slot++ % 256];
    lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(counter));
    lir::emitOp1(c_, lir::MOV_U8, lir::R1, 0, lir::MEM1(lir::R0), 0);
    lir::emitOp2(c_, lir::ADD, lir::R1, 0, lir::R1, 0, lir::IMM, 1);
    lir::emitOp1(c_, lir::MOV_U8, lir::MEM1(lir::R0), 0, lir::R1, 0);
    lir::Jump *take = lir::emitCmp(c_, lir::EQUAL, lir::R1, 0, lir::IMM, 2);
    addCold([this, take, emitExit]() mutable {
        lir::setLabel(take, lir::emitLabel(c_));
        emitExit();
    });
    return true;
}

void Emitter::surveyLowering() const {
    static const bool enabled = std::getenv("SE_JIT_LOWER_LOG") != nullptr;
    if (!enabled) return;
    for (const Region &region : regions_) {
        std::map<std::string, uint32_t> blockers;
        uint32_t total = 0;
        for (BytecodeIndex pc = region.head; pc <= region.tail; pc++) {
            total++;
            const BytecodeOp op = chunk_.instructions[pc].op;
            bool covered;
            switch (op) {
            case BytecodeOp::PushConstNumber:
            case BytecodeOp::PushConstValue:
            case BytecodeOp::PushVarNumber:
            case BytecodeOp::PushVarValue:
            case BytecodeOp::StoreVarNumber:
            case BytecodeOp::StoreVarValue:
            case BytecodeOp::ChangeVarNumber:
            case BytecodeOp::ConvertNumberToValue:
            case BytecodeOp::ConvertValueToNumber:
            case BytecodeOp::NumEquals:
            case BytecodeOp::NumLess:
            case BytecodeOp::NumGreater:
            case BytecodeOp::ValueEquals:
            case BytecodeOp::ValueLess:
            case BytecodeOp::ValueGreater:
            case BytecodeOp::ItemOfList:
            case BytecodeOp::ReplaceInList:
            case BytecodeOp::RepeatCheck:
            case BytecodeOp::Yield:
            case BytecodeOp::Jump:
            case BytecodeOp::JumpIfFalse:
            case BytecodeOp::JumpIfTrue:
                covered = true;
                break;
            case BytecodeOp::CallNativeNumber: {
                const NativeShape shape = plan_.nativeShape(chunk_.instructions[pc]);
                const std::string &name = plan_.opcodeName(chunk_.instructions[pc].a);
                covered = shape == NativeShape::Add || shape == NativeShape::Subtract || shape == NativeShape::Multiply || shape == NativeShape::Divide || name == "operator_mod";
                if (!covered) blockers[name]++;
                continue;
            }
            case BytecodeOp::CallNativeBoolean: {
                const NativeShape shape = plan_.nativeShape(chunk_.instructions[pc]);
                covered = shape == NativeShape::And || shape == NativeShape::Or || shape == NativeShape::Not;
                if (!covered) blockers[plan_.opcodeName(chunk_.instructions[pc].a)]++;
                continue;
            }
            default:
                covered = false;
                break;
            }
            if (!covered) blockers[BytecodeDisassembler::opMnemonic(op)]++;
        }
        std::string reasons;
        for (const auto &[name, count] : blockers)
            reasons += " " + name + "x" + std::to_string(count);
        Log::log("[lower survey] region " + std::to_string(region.head) + ".." + std::to_string(region.tail) + " (" + std::to_string(total) + " ops) " + (blockers.empty() ? "COVERED" : "blocked by" + reasons));
    }
}

} // namespace BytecodeJitInternal

#endif
