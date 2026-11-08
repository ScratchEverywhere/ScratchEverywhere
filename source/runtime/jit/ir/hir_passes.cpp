#include "hir.hpp"

#ifdef SE_JIT

#include <cmath>
#include <string>
#include <vector>

namespace BytecodeJitIR {

namespace {

uint32_t eliminateDeadCode(Function &f) {
    std::vector<uint32_t> uses(f.values.size(), 0);
    const auto use = [&](ValueId value) {
        if (value < uses.size()) uses[value]++;
    };
    for (const Block &block : f.blocks) {
        if (!block.reachable) continue;
        for (const Instr &instr : block.instrs)
            for (const ValueId arg : instr.args)
                use(arg);
        for (const Instr &instr : block.instrs)
            for (const ValueId value : instr.frame)
                use(value);
        for (const ValueId value : block.termFrame)
            use(value);
        if (block.term == Terminator::CondBr || block.term == Terminator::Ret) use(block.cond);
        const int edges = block.term == Terminator::Br ? 1 : block.term == Terminator::CondBr ? 2
                                                                                              : 0;
        for (int e = 0; e < edges; e++)
            for (const ValueId arg : block.edges[e].args)
                use(arg);
    }

    uint32_t removed = 0;
    for (BlockId id = 0; id < f.blocks.size(); id++) {
        Block &block = f.blocks[id];
        if (!block.reachable) continue;
        for (size_t i = block.instrs.size(); i-- > 0;) {
            const Instr &instr = block.instrs[i];
            if (!removable(instr)) continue;
            bool dead = true;
            for (const ValueId result : instr.results)
                if (uses[result] != 0) dead = false;
            if (!dead) continue;
            for (const ValueId arg : instr.args)
                uses[arg]--;
            for (const ValueId value : instr.frame)
                uses[value]--;
            block.instrs.erase(block.instrs.begin() + static_cast<std::ptrdiff_t>(i));
            removed++;
        }
    }
    return removed;
}

} // namespace

namespace {

class FactComputer {
  public:
    FactComputer(Function &f, const BytecodeChunk &chunk) : f_(f), chunk_(chunk), state_(f.values.size(), 0) {}

    bool nonNaN(ValueId v) {
        if (v >= f_.values.size()) return false;
        if (state_[v] != 0) return state_[v] == 2;
        state_[v] = 1;
        const bool result = compute(v);
        state_[v] = result ? 2 : 1;
        f_.values[v].nonNaN = result;
        return result;
    }

  private:
    Function &f_;
    const BytecodeChunk &chunk_;
    std::vector<uint8_t> state_; // 0 unknown, 1 false, 2 true

    const Instr *definition(ValueId v) const {
        const ValueInfo &info = f_.values[v];
        if (info.isParameter || info.block >= f_.blocks.size() || info.index >= f_.blocks[info.block].instrs.size()) return nullptr;
        return &f_.blocks[info.block].instrs[info.index];
    }

    bool constantOf(ValueId v, double &out) const {
        const Instr *def = definition(v);
        if (def == nullptr || def->op != Op::Const || def->bc != BytecodeOp::PushConstNumber || def->a >= chunk_.numberConstants.size()) return false;
        out = chunk_.numberConstants[def->a];
        return true;
    }

    bool compute(ValueId v) {
        if (f_.values[v].type != Type::F64) return false;
        const Instr *def = definition(v);
        if (def == nullptr) return false;
        double constant;
        switch (def->op) {
        case Op::Const:
            return constantOf(v, constant) && !std::isnan(constant);
        case Op::Var:
            return def->bc == BytecodeOp::PushVarNumber;
        case Op::MathFn: {
            const uint32_t fn = def->b;
            if (fn != static_cast<uint32_t>(BytecodeMathOp::Abs) && fn != static_cast<uint32_t>(BytecodeMathOp::Floor) && fn != static_cast<uint32_t>(BytecodeMathOp::Ceiling)) return false;
            return def->args.size() == 1 && nonNaN(def->args[0]);
        }
        case Op::Effect:
            return def->bc == BytecodeOp::RandomInt && def->args.size() == 2 && nonNaN(def->args[0]) && nonNaN(def->args[1]);
        case Op::Arith: {
            const ArithKind kind = static_cast<ArithKind>(def->sub);
            if (kind != ArithKind::Add && kind != ArithKind::Subtract && kind != ArithKind::Multiply && kind != ArithKind::Divide) return false;
            if (def->args.size() != 2 || !constantOf(def->args[1], constant)) return false;
            if (!std::isfinite(constant) || ((kind == ArithKind::Multiply || kind == ArithKind::Divide) && constant == 0.0)) return false;
            return nonNaN(def->args[0]);
        }
        default:
            return false;
        }
    }
};

} // namespace

void computeFacts(Function &f, const BytecodeChunk &chunk) {
    for (BlockId id = 0; id < f.blocks.size(); id++) {
        const Block &block = f.blocks[id];
        for (size_t i = 0; i < block.instrs.size(); i++) {
            for (const ValueId result : block.instrs[i].results) {
                f.values[result].block = id;
                f.values[result].index = static_cast<uint32_t>(i);
                f.values[result].isParameter = false;
            }
        }
    }
    FactComputer facts(f, chunk);
    for (ValueId v = 0; v < f.values.size(); v++)
        facts.nonNaN(v);
}

std::string inlineCandidates(const Function &f, const BytecodeChunk &chunk) {
    struct Info {
        uint32_t size = 0;
        bool leaf = true;
        bool seen = false;
    };
    std::vector<Info> info(chunk.procedures.size());
    for (const EntryPoint &entry : f.entries) {
        if (!entry.isProcedure || entry.index >= info.size()) continue;
        Info &i = info[entry.index];
        std::vector<BlockId> stack = {entry.block};
        std::vector<uint8_t> visited(f.blocks.size(), 0);
        while (!stack.empty()) {
            const BlockId id = stack.back();
            stack.pop_back();
            if (visited[id]) continue;
            visited[id] = 1;
            const Block &block = f.blocks[id];
            i.size += static_cast<uint32_t>(block.instrs.size());
            for (const Instr &instr : block.instrs)
                if (instr.op == Op::Call || instr.op == Op::Yield || instr.op == Op::Legacy) i.leaf = false;
            const int edges = block.term == Terminator::Br ? 1 : block.term == Terminator::CondBr ? 2
                                                                                                  : 0;
            for (int e = 0; e < edges; e++)
                stack.push_back(block.edges[e].target);
        }
        i.seen = true;
    }
    std::vector<uint8_t> straight(info.size(), 0);
    for (const EntryPoint &entry : f.entries) {
        if (!entry.isProcedure || entry.index >= info.size() || !chunk.procedures[entry.index].runWithoutScreenRefresh) continue;
        const Block &block = f.blocks[entry.block];
        bool ok = (block.term == Terminator::Ret || block.term == Terminator::Halt) && block.instrs.size() <= 24 && info[entry.index].leaf;
        for (const Instr &instr : block.instrs)
            if (instr.op == Op::Effect || (instr.op == Op::Native && !(instr.effects == NoEffect))) ok = false;
        straight[entry.index] = ok ? 1 : 0;
    }
    std::vector<uint8_t> argsOnly(info.size(), 0);
    for (const EntryPoint &entry : f.entries) {
        if (!entry.isProcedure || entry.index >= info.size() || !chunk.procedures[entry.index].runWithoutScreenRefresh || !info[entry.index].leaf || info[entry.index].size > 40) continue;
        const uint32_t argc = chunk.procedures[entry.index].argCount;
        bool ok = true, cycle = false;
        std::vector<uint8_t> state(f.blocks.size(), 0);
        std::vector<std::pair<BlockId, int>> stack = {{entry.block, 0}};
        state[entry.block] = 1;
        while (!stack.empty() && ok && !cycle) {
            auto &[id, next] = stack.back();
            const Block &block = f.blocks[id];
            if (block.params.size() != argc) ok = false;
            for (const ValueId param : block.params)
                if (f.values[param].type != Type::Val) ok = false;
            for (const Instr &instr : block.instrs)
                if (instr.op == Op::Effect || (instr.op == Op::Native && instr.effects != NoEffect)) ok = false;
            const int edges = block.term == Terminator::Br ? 1 : block.term == Terminator::CondBr ? 2
                                                                                                  : 0;
            if (next < edges) {
                const BlockId target = block.edges[next++].target;
                if (state[target] == 1) cycle = true;
                else if (state[target] == 0) {
                    state[target] = 1;
                    stack.push_back({target, 0});
                }
            } else {
                state[id] = 2;
                stack.pop_back();
            }
        }
        argsOnly[entry.index] = ok && !cycle ? 1 : 0;
    }
    std::vector<std::vector<uint32_t>> callees(info.size());
    std::vector<uint8_t> shapeOk(info.size(), 0);
    std::vector<uint32_t> ownSize(info.size(), 0);
    for (const EntryPoint &entry : f.entries) {
        if (!entry.isProcedure || entry.index >= info.size() || !chunk.procedures[entry.index].runWithoutScreenRefresh) continue;
        const uint32_t argc = chunk.procedures[entry.index].argCount;
        bool ok = true, cycle = false;
        std::vector<uint8_t> state(f.blocks.size(), 0);
        std::vector<std::pair<BlockId, int>> stack = {{entry.block, 0}};
        state[entry.block] = 1;
        while (!stack.empty() && ok && !cycle) {
            auto &[id, next] = stack.back();
            const Block &block = f.blocks[id];
            if (next == 0) {
                ownSize[entry.index] += static_cast<uint32_t>(block.instrs.size());
                if (block.params.size() != argc) ok = false;
                for (const ValueId param : block.params)
                    if (f.values[param].type != Type::Val) ok = false;
                for (const Instr &instr : block.instrs) {
                    if (instr.op == Op::Effect || instr.op == Op::Legacy || instr.op == Op::Yield || (instr.op == Op::Native && instr.effects != NoEffect)) ok = false;
                    if (instr.op == Op::Call && instr.a < info.size()) callees[entry.index].push_back(instr.a);
                }
            }
            const int edges = block.term == Terminator::Br ? 1 : block.term == Terminator::CondBr ? 2
                                                                                                  : 0;
            if (next < edges) {
                const BlockId target = block.edges[next++].target;
                if (state[target] == 1) cycle = true;
                else if (state[target] == 0) {
                    state[target] = 1;
                    stack.push_back({target, 0});
                }
            } else {
                state[id] = 2;
                stack.pop_back();
            }
        }
        shapeOk[entry.index] = ok && !cycle ? 1 : 0;
    }
    std::vector<uint32_t> expanded(info.size(), 0);
    std::vector<uint8_t> inlinable(info.size(), 0);
    for (int round = 0; round < 12; round++) {
        bool changed = false;
        for (size_t proc = 0; proc < info.size(); proc++) {
            if (inlinable[proc] || !shapeOk[proc]) continue;
            uint32_t size = ownSize[proc];
            bool ok = true;
            for (const uint32_t callee : callees[proc]) {
                if (!inlinable[callee]) {
                    ok = false;
                    break;
                }
                size += expanded[callee];
            }
            if (ok && size <= 120) {
                inlinable[proc] = 1;
                expanded[proc] = size;
                changed = true;
            }
        }
        if (!changed) break;
    }
    uint32_t closureProcs = 0, callsToClosure = 0;
    for (const uint8_t flag : inlinable)
        closureProcs += flag;
    for (const Block &block : f.blocks)
        for (const Instr &instr : block.instrs)
            if (instr.op == Op::Call && instr.a < inlinable.size() && inlinable[instr.a]) callsToClosure++;
    uint32_t argsOnlyProcs = 0, callsToArgsOnly = 0;
    for (const uint8_t flag : argsOnly)
        argsOnlyProcs += flag;
    for (const Block &block : f.blocks)
        for (const Instr &instr : block.instrs)
            if (instr.op == Op::Call && instr.a < argsOnly.size() && argsOnly[instr.a]) callsToArgsOnly++;
    uint32_t straightProcs = 0, callsToStraight = 0;
    for (const uint8_t flag : straight)
        straightProcs += flag;
    for (const Block &block : f.blocks)
        for (const Instr &instr : block.instrs)
            if (instr.op == Op::Call && instr.a < straight.size() && straight[instr.a]) callsToStraight++;
    uint32_t calls = 0, toSmallLeaf = 0, toSmall = 0, procs = 0, smallLeafProcs = 0;
    for (const Info &i : info) {
        if (!i.seen) continue;
        procs++;
        if (i.leaf && i.size <= 40) smallLeafProcs++;
    }
    for (const Block &block : f.blocks)
        for (const Instr &instr : block.instrs) {
            if (instr.op != Op::Call || instr.a >= info.size() || !info[instr.a].seen) continue;
            calls++;
            if (info[instr.a].size <= 40) toSmall++;
            if (info[instr.a].leaf && info[instr.a].size <= 40) toSmallLeaf++;
        }
    return "procedures " + std::to_string(procs) + " (small leaf " + std::to_string(smallLeafProcs) + "); call sites " + std::to_string(calls) + ", to small bodies " + std::to_string(toSmall) + ", to small leaf bodies " + std::to_string(toSmallLeaf) + "; straight-line warp leaf procedures " + std::to_string(straightProcs) + ", call sites to them " + std::to_string(callsToStraight) + "; acyclic args-only leaf procedures " + std::to_string(argsOnlyProcs) + ", call sites to them " + std::to_string(callsToArgsOnly) + "; bottom-up inlinable (budget 120) " + std::to_string(closureProcs) + ", call sites to them " + std::to_string(callsToClosure);
}

PassStats runPasses(Function &function, std::vector<std::string> &errors) {
    PassStats stats;
    for (int round = 0; round < 8; round++) {
        const uint32_t removed = eliminateDeadCode(function);
        stats.rounds++;
        stats.removedInstructions += removed;
        if (removed == 0) break;
    }
    (void)errors;
    return stats;
}

} // namespace BytecodeJitIR

#endif
