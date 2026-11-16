#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "jit_emitter.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace BytecodeJitInternal {

namespace {

constexpr uint32_t maxInlineBody = 60;
constexpr uint32_t maxInlineExpanded = 120;
constexpr size_t maxInlineDepth = 3;

} // namespace

void Emitter::computeInlinable() {
    const size_t procedures = chunk_.procedures.size();
    inlineInfo_.assign(procedures, InlineInfo{});
    static const bool enabled = [] {
        const char *env = std::getenv("SE_JIT_INLINE");
        return env != nullptr && std::string(env) != "0";
    }();
    if (!enabled) return;

    const BytecodeJitIR::Function &f = hir_.function;
    std::vector<BytecodeJitIR::BlockId> entryBlock(procedures, BytecodeJitIR::noBlock);
    for (const BytecodeJitIR::EntryPoint &entry : f.entries)
        if (entry.isProcedure && entry.index < procedures) entryBlock[entry.index] = entry.block;

    struct Shape {
        bool ok = false;
        std::vector<BytecodeJitIR::BlockId> blocks;
        std::vector<uint32_t> callees;
        uint32_t size = 0;
        uint32_t numbers = 0, booleans = 0, values = 0;
        bool hasReturn = false, allReturnsNumber = true, hasHalt = false;
    };
    std::vector<Shape> shapes(procedures);

    for (size_t proc = 0; proc < procedures; proc++) {
        if (entryBlock[proc] == BytecodeJitIR::noBlock || !chunk_.procedures[proc].runWithoutScreenRefresh) continue;
        Shape &shape = shapes[proc];
        const uint32_t argc = chunk_.procedures[proc].argCount;
        bool ok = true, cycle = false;
        std::vector<uint8_t> state(f.blocks.size(), 0);
        std::vector<std::pair<BytecodeJitIR::BlockId, int>> stack = {{entryBlock[proc], 0}};
        state[entryBlock[proc]] = 1;
        while (!stack.empty() && ok && !cycle) {
            const BytecodeJitIR::BlockId id = stack.back().first;
            const int next = stack.back().second;
            const BytecodeJitIR::Block &block = f.blocks[id];
            if (next == 0) {
                shape.blocks.push_back(id);
                shape.size += static_cast<uint32_t>(block.instrs.size());
                if (!block.reachable) {
                    ok = false;
                }
                uint32_t vals = 0;
                for (size_t i = 0; ok && i < block.params.size(); i++) {
                    const BytecodeJitIR::ValueInfo &param = f.values[block.params[i]];
                    if (param.type == BytecodeJitIR::Type::Val) {
                        if (vals >= argc && param.kind != BytecodeValueType::Number) ok = false;
                        vals++;
                    } else if (param.type != BytecodeJitIR::Type::F64 && param.type != BytecodeJitIR::Type::Bool) {
                        ok = false;
                    }
                }
                if (ok && vals < argc) ok = false;
                if (id == entryBlock[proc] && block.params.size() != argc) {
                    ok = false;
                }
                for (const BytecodeJitIR::Instr &instr : block.instrs) {
                    if (instr.op == BytecodeJitIR::Op::Effect || instr.op == BytecodeJitIR::Op::Legacy || instr.op == BytecodeJitIR::Op::Yield) {
                        ok = false;
                    }
                    if (instr.op == BytecodeJitIR::Op::Arg && instr.a >= argc) ok = false;
                    if (instr.op == BytecodeJitIR::Op::Call && instr.a < procedures) shape.callees.push_back(instr.a);
                }
                for (BytecodeIndex pc = block.startPc; ok && pc < block.endPc; pc++) {
                    if (!plan_.supported[pc]) {
                        ok = false;
                    }
                    BytecodeAbstractState at;
                    if (analysis_.stateBefore(pc, at)) {
                        shape.numbers = std::max(shape.numbers, at.numbers + 2);
                        shape.booleans = std::max(shape.booleans, at.booleans + 2);
                        shape.values = std::max(shape.values, static_cast<uint32_t>(at.values.size()) + 2);
                    }
                }
                if (block.term == BytecodeJitIR::Terminator::Ret) {
                    shape.hasReturn = true;
                    if (block.cond >= f.values.size() || f.values[block.cond].kind != BytecodeValueType::Number) shape.allReturnsNumber = false;
                } else if (block.term == BytecodeJitIR::Terminator::Halt) {
                    shape.hasHalt = true;
                } else if (block.term == BytecodeJitIR::Terminator::Escape) {
                    ok = false;
                }
            }
            const int edges = block.term == BytecodeJitIR::Terminator::Br ? 1 : block.term == BytecodeJitIR::Terminator::CondBr ? 2
                                                                                                                                : 0;
            if (next < edges) {
                stack.back().second++;
                const BytecodeJitIR::BlockId target = block.edges[next].target;
                if (target >= f.blocks.size()) ok = false;
                else if (state[target] == 1) cycle = true;
                else if (state[target] == 0) {
                    state[target] = 1;
                    stack.push_back({target, 0});
                }
            } else {
                state[id] = 2;
                stack.pop_back();
            }
        }
        if (ok && !cycle && shape.size <= maxInlineBody) {
            std::sort(shape.blocks.begin(), shape.blocks.end(), [&](BytecodeJitIR::BlockId a, BytecodeJitIR::BlockId b) { return f.blocks[a].startPc < f.blocks[b].startPc; });
            bool contiguous = f.blocks[shape.blocks.front()].startPc == chunk_.procedures[proc].entryInstruction;
            for (size_t i = 0; contiguous && i + 1 < shape.blocks.size(); i++)
                contiguous = f.blocks[shape.blocks[i]].endPc == f.blocks[shape.blocks[i + 1]].startPc;
            shape.ok = contiguous;
        }
    }

    std::vector<uint32_t> needNumbers(procedures, 0), needBooleans(procedures, 0), needValues(procedures, 0);
    for (int round = 0; round < 12; round++) {
        bool changed = false;
        for (size_t proc = 0; proc < procedures; proc++) {
            if (inlineInfo_[proc].ok || !shapes[proc].ok) continue;
            const Shape &shape = shapes[proc];
            uint32_t expanded = shape.size, n = 0, b = 0, v = 0;
            bool ok = true;
            for (const uint32_t callee : shape.callees) {
                if (callee == proc || !inlineInfo_[callee].ok) {
                    ok = false;
                    break;
                }
                expanded += inlineInfo_[callee].expanded;
                n = std::max(n, needNumbers[callee]);
                b = std::max(b, needBooleans[callee]);
                v = std::max(v, needValues[callee]);
            }
            if (!ok || expanded > maxInlineExpanded) continue;
            InlineInfo &info = inlineInfo_[proc];
            info.ok = true;
            info.blocks = shape.blocks;
            info.expanded = expanded;
            info.returnsNumber = shape.hasReturn && shape.allReturnsNumber;
            info.hasHalt = shape.hasHalt;
            needNumbers[proc] = shape.numbers + n;
            needBooleans[proc] = shape.booleans + b;
            needValues[proc] = shape.values + v;
            changed = true;
        }
        if (!changed) break;
    }
    for (size_t proc = 0; proc < procedures; proc++) {
        if (!inlineInfo_[proc].ok) continue;
        inlineReserveNumbers_ = std::max(inlineReserveNumbers_, needNumbers[proc]);
        inlineReserveBooleans_ = std::max(inlineReserveBooleans_, needBooleans[proc]);
        inlineReserveValues_ = std::max(inlineReserveValues_, needValues[proc]);
    }
}

bool Emitter::inlineSite(BytecodeIndex pc, const BytecodeInstruction &instr, const CallDescriptor &call) const {
    if (active_ != nullptr) return false;
    if (instr.a >= inlineInfo_.size() || !inlineInfo_[instr.a].ok) return false;
    if (state_.frames.size() >= maxInlineDepth) return false;
    const InlineInfo &info = inlineInfo_[instr.a];
    const bool wantsValue = instr.op == BytecodeOp::CallProcedureValue;
    const uint32_t argc = call.argCount;
    if (argc != chunk_.procedures[instr.a].argCount || state_.values.size() < argc) return false;
    if (wantsValue) {
        if (!info.returnsNumber || info.hasHalt) return false;
        const BytecodeJitIR::BlockId next = blockStartingAt(pc + 1);
        if (next == BytecodeJitIR::noBlock) return false;
        const BytecodeJitIR::Block &block = hir_.function.blocks[next];
        if (block.params.empty()) return false;
        const BytecodeJitIR::ValueInfo &result = hir_.function.values[block.params.back()];
        if (result.type != BytecodeJitIR::Type::Val || result.kind != BytecodeValueType::Number) return false;
    }
    const uint32_t argBase = static_cast<uint32_t>(state_.values.size()) - argc;
    for (uint32_t j = 0; j < argc; j++) {
        const AbsValue &arg = state_.values[argBase + j];
        switch (arg.kind) {
        case ValueEntry::Number:
        case ValueEntry::Temp:
        case ValueEntry::Boolean:
            break;
        case ValueEntry::Constant:
            if (arg.constant >= chunk_.constants.size() || chunk_.constants[arg.constant].isString() || !(chunk_.constants[arg.constant].isRealNumber() || chunk_.constants[arg.constant].isStringTyped())) return false;
            break;
        case ValueEntry::VarValue:
            if (arg.constant >= code_.promoted.size()) return false;
            break;
        default:
            return false;
        }
    }
    return true;
}

bool Emitter::inlineEntryState(BytecodeIndex pc, AbsState &out) const {
    AbsState local;
    if (inline_ == nullptr || !stateFromBlockParameters(pc, local) || !local.strings.empty()) return false;
    const std::vector<InlineFrame> frames = state_.frames;
    out = inline_->prefix;
    out.numbers += local.numbers;
    out.booleans += local.booleans;
    for (size_t i = 0; i < local.values.size(); i++)
        out.values.push_back(i < inline_->argCount ? inline_->arguments[i] : local.values[i]);
    out.frames = frames;
    return true;
}

bool Emitter::emitInlineCall(BytecodeIndex pc, const BytecodeInstruction &instr, const CallDescriptor &call, std::vector<lir::Jump *> &slow) {
    if (!inlineSite(pc, instr, call)) return false;
    const InlineInfo &info = inlineInfo_[instr.a];
    const bool wantsValue = instr.op == BytecodeOp::CallProcedureValue;
    const uint32_t argc = call.argCount;
    const uint32_t argBase = static_cast<uint32_t>(state_.values.size()) - argc;

    const bool outermost = inline_ == nullptr;
    const lir::Mark mark = lir::mark(c_);
    const size_t pendingMark = pending_.size(), exitMark = exitJumps_.size(), leaveMark = leaveJumps_.size(), coldMark = cold_.size(), bodyMark = pendingBody_.size(), labelMark = blockLabels_.size(), slowMark = slow.size();
    const AbsState caller = state_;

    for (uint32_t j = 0; j < argc; j++) {
        const AbsValue &arg = state_.values[argBase + j];
        if (arg.kind == ValueEntry::Temp) {
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(&Runtime::tempLazy[argBase + j]));
            lir::emitOp1(c_, lir::MOV_U8, lir::R0, 0, lir::MEM1(lir::R0), 0);
            slow.push_back(lir::emitCmp(c_, lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 1));
        } else if (arg.kind == ValueEntry::VarValue) {
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::MEM1(lir::S0), dirtyOffset(arg.constant));
            lir::emitOp2(c_, lir::SUB, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
            slow.push_back(lir::emitCmp(c_, lir::GREATER, lir::R0, 0, lir::IMM, 1));
        }
    }

    InlineInstance instance;
    instance.argBase = argBase;
    instance.argCount = argc;
    instance.wantsValue = wantsValue;
    instance.arguments.assign(state_.values.begin() + argBase, state_.values.end());
    instance.prefix = state_;
    instance.prefix.values.resize(argBase);
    for (const BytecodeJitIR::BlockId id : info.blocks) {
        instance.labels[hir_.function.blocks[id].startPc] = static_cast<BytecodeJitIR::BlockId>(blockLabels_.size());
        blockLabels_.push_back(nullptr);
    }

    InlineInstance *outer = inline_;
    inline_ = &instance;
    InlineFrame frame;
    frame.procedure = instr.a;
    frame.callPc = pc;
    frame.argCount = argc;
    frame.valueStart = static_cast<uint32_t>(state_.values.size());
    frame.wantsValue = wantsValue;
    state_.frames.push_back(frame);

    for (const BytecodeJitIR::BlockId id : info.blocks) {
        const BytecodeJitIR::Block &block = hir_.function.blocks[id];
        blockLabels_[instance.labels[block.startPc]] = lir::emitLabel(c_);
        for (BytecodeIndex at = block.startPc; at < block.endPc && !inlineAbort_; at++) {
            if (at == block.startPc && !inlineEntryState(at, state_)) {
                inlineAbort_ = true;
                break;
            }
            if (fused_[at]) continue;
            if (const auto found = conditions_.find(at); found != conditions_.end()) emitCondition(found->second, at);
            else emitInstruction(at);
        }
        if (inlineAbort_) break;
    }
    inline_ = outer;

    if (inlineAbort_ && outermost) {
        lir::rewind(c_, mark);
        pending_.resize(pendingMark);
        exitJumps_.resize(exitMark);
        leaveJumps_.resize(leaveMark);
        cold_.resize(coldMark);
        pendingBody_.resize(bodyMark);
        blockLabels_.resize(labelMark);
        slow.resize(slowMark);
        state_ = caller;
        inlineAbort_ = false;
        return false;
    }

    lir::Label *continuation = lir::emitLabel(c_);
    for (lir::Jump *jump : instance.returns)
        lir::setLabel(jump, continuation);

    state_ = caller;
    state_.values.resize(argBase);
    if (wantsValue) {
        AbsValue result;
        result.kind = ValueEntry::Number;
        state_.values.push_back(result);
    }
    return true;
}

void Emitter::emitInlineReturn(BytecodeIndex pc, bool isReturn) {
    if (isReturn) {
        if (state_.values.empty() || state_.values.back().kind != ValueEntry::Number) {
            inlineAbort_ = true;
            return;
        }
        const uint32_t top = static_cast<uint32_t>(state_.values.size()) - 1;
        state_.values.pop_back();
        if (inline_->wantsValue) moveF(valueSlot(inline_->argBase), valueSlot(top));
    } else if (inline_->wantsValue) {
        inlineAbort_ = true;
        return;
    }
    inline_->returns.push_back(lir::emitJump(c_, lir::JUMP));
}

} // namespace BytecodeJitInternal

#endif
