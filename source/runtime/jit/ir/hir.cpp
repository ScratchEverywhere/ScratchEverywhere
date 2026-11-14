#include "hir.hpp"

#ifdef SE_JIT

#include "bytecode_disassembler.hpp"
#include <algorithm>
#include <sstream>

namespace BytecodeJitIR {

const char *typeName(Type type) {
    switch (type) {
    case Type::F64:
        return "f64";
    case Type::Bool:
        return "bool";
    case Type::Str:
        return "str";
    case Type::Color:
        return "color";
    case Type::Val:
        return "val";
    default:
        return "none";
    }
}

namespace {

const char *opName(Op op) {
    switch (op) {
    case Op::Bytecode:
        return "bc";
    case Op::Const:
        return "const";
    case Op::NumCompare:
        return "numcmp";
    case Op::RepeatStep:
        return "repeat_step";
    case Op::ForEachStep:
        return "foreach_step";
    case Op::Var:
        return "var";
    case Op::Arg:
        return "arg";
    case Op::Convert:
        return "convert";
    case Op::Arith:
        return "arith";
    case Op::MathFn:
        return "math";
    case Op::Compare:
        return "compare";
    case Op::List:
        return "list";
    case Op::Call:
        return "call";
    case Op::Native:
        return "native";
    case Op::Legacy:
        return "legacy";
    case Op::Yield:
        return "yield";
    case Op::Discard:
        return "discard";
    default:
        return "effect";
    }
}

const char *arithName(uint8_t sub) {
    switch (static_cast<ArithKind>(sub)) {
    case ArithKind::Add:
        return "add";
    case ArithKind::Subtract:
        return "sub";
    case ArithKind::Multiply:
        return "mul";
    case ArithKind::Divide:
        return "div";
    case ArithKind::Mod:
        return "mod";
    case ArithKind::And:
        return "and";
    case ArithKind::Or:
        return "or";
    case ArithKind::Not:
        return "not";
    default:
        return "";
    }
}

std::string valueName(const Function &f, ValueId id) {
    if (id == noValue) return "-";
    std::ostringstream out;
    out << "%" << id << ":" << (id < f.values.size() ? typeName(f.values[id].type) : "?");
    return out.str();
}

void printEdge(std::ostringstream &out, const Function &f, const Edge &edge) {
    out << "b" << edge.target << "(";
    for (size_t i = 0; i < edge.args.size(); i++) {
        if (i != 0) out << ", ";
        out << "%" << edge.args[i];
    }
    out << ")";
    (void)f;
}

} // namespace

std::string print(const Function &f, const BytecodeChunk *chunk) {
    (void)chunk;
    std::ostringstream out;
    out << "function: " << f.blocks.size() << " blocks, " << f.values.size() << " values, " << f.entries.size() << " entries\n";
    for (const EntryPoint &entry : f.entries) {
        out << "entry " << (entry.isProcedure ? "procedure#" : "script#") << entry.index << " pc " << entry.pc << " -> b" << entry.block << "\n";
    }
    for (BlockId id = 0; id < f.blocks.size(); id++) {
        const Block &block = f.blocks[id];
        if (!block.reachable) continue;
        out << "b" << id << "(";
        for (size_t i = 0; i < block.params.size(); i++) {
            if (i != 0) out << ", ";
            out << valueName(f, block.params[i]);
        }
        out << ")  ; pc " << block.startPc << ".." << block.endPc << "\n";
        for (const Instr &instr : block.instrs) {
            out << "  ";
            for (size_t i = 0; i < instr.results.size(); i++) {
                if (i != 0) out << ", ";
                out << valueName(f, instr.results[i]);
            }
            if (!instr.results.empty()) out << " = ";
            out << opName(instr.op);
            if (instr.op == Op::Arith) out << "." << arithName(instr.sub);
            out << " " << BytecodeDisassembler::opMnemonic(instr.bc);
            if (instr.a != 0 || instr.b != 0) out << " a=" << instr.a << " b=" << instr.b;
            for (size_t i = 0; i < instr.args.size(); i++)
                out << (i == 0 ? " (" : ", ") << "%" << instr.args[i];
            if (!instr.args.empty()) out << ")";
            out << "  ; #" << instr.pc << "\n";
        }
        switch (block.term) {
        case Terminator::Br:
            out << "  br ";
            printEdge(out, f, block.edges[0]);
            out << "\n";
            break;
        case Terminator::CondBr:
            out << "  condbr %" << block.cond << " ? ";
            printEdge(out, f, block.edges[0]);
            out << " : ";
            printEdge(out, f, block.edges[1]);
            out << "\n";
            break;
        case Terminator::Ret:
            out << "  ret %" << block.cond << "\n";
            break;
        case Terminator::Halt:
            out << "  halt\n";
            break;
        case Terminator::Escape:
            out << "  escape\n";
            break;
        default:
            out << "  <no terminator>\n";
            break;
        }
    }
    return out.str();
}

std::vector<std::string> verify(const Function &f) {
    std::vector<std::string> errors;
    const auto fail = [&](const std::string &message) {
        if (errors.size() < 40) errors.push_back(message);
    };
    const size_t blockCount = f.blocks.size();

    std::vector<std::vector<BlockId>> successors(blockCount), predecessors(blockCount);
    const auto checkEdge = [&](BlockId from, const Edge &edge) {
        if (edge.target >= blockCount) {
            fail("b" + std::to_string(from) + ": edge to a missing block");
            return;
        }
        const Block &target = f.blocks[edge.target];
        if (!target.reachable) fail("b" + std::to_string(from) + ": edge to unreachable block b" + std::to_string(edge.target));
        if (edge.args.size() != target.params.size()) {
            fail("b" + std::to_string(from) + " -> b" + std::to_string(edge.target) + ": " + std::to_string(edge.args.size()) + " arguments for " + std::to_string(target.params.size()) + " parameters");
            return;
        }
        for (size_t i = 0; i < edge.args.size(); i++) {
            if (edge.args[i] >= f.values.size() || target.params[i] >= f.values.size()) {
                fail("b" + std::to_string(from) + " -> b" + std::to_string(edge.target) + ": bad value id");
                continue;
            }
            if (f.values[edge.args[i]].type != f.values[target.params[i]].type)
                fail("b" + std::to_string(from) + " -> b" + std::to_string(edge.target) + ": argument " + std::to_string(i) + " is " + typeName(f.values[edge.args[i]].type) + ", parameter is " + typeName(f.values[target.params[i]].type));
        }
        successors[from].push_back(edge.target);
        predecessors[edge.target].push_back(from);
    };
    for (BlockId id = 0; id < blockCount; id++) {
        const Block &block = f.blocks[id];
        if (!block.reachable) continue;
        switch (block.term) {
        case Terminator::Br:
            checkEdge(id, block.edges[0]);
            break;
        case Terminator::CondBr:
            if (block.cond >= f.values.size() || f.values[block.cond].type != Type::Bool) fail("b" + std::to_string(id) + ": condition is not a bool");
            checkEdge(id, block.edges[0]);
            checkEdge(id, block.edges[1]);
            break;
        case Terminator::Ret:
            if (block.cond >= f.values.size() || f.values[block.cond].type != Type::Val) fail("b" + std::to_string(id) + ": returned value is not a val");
            break;
        case Terminator::Halt:
        case Terminator::Escape:
            break;
        default:
            fail("b" + std::to_string(id) + ": no terminator");
            break;
        }
        for (const Instr &instr : block.instrs) {
            for (ValueId arg : instr.args)
                if (arg >= f.values.size()) fail("b" + std::to_string(id) + " #" + std::to_string(instr.pc) + ": bad argument id");
            for (ValueId result : instr.results)
                if (result >= f.values.size()) fail("b" + std::to_string(id) + " #" + std::to_string(instr.pc) + ": bad result id");
        }
    }
    if (!errors.empty()) return errors;

    const BlockId root = static_cast<BlockId>(blockCount);
    std::vector<BlockId> order;
    std::vector<int> postIndex(blockCount + 1, -1);
    {
        std::vector<uint8_t> seen(blockCount + 1, 0);
        std::vector<std::pair<BlockId, size_t>> stack;
        std::vector<BlockId> post;
        std::vector<BlockId> rootSuccessors;
        for (const EntryPoint &entry : f.entries)
            if (entry.block < blockCount) rootSuccessors.push_back(entry.block);
        const auto successorsOf = [&](BlockId id) -> const std::vector<BlockId> & { return id == root ? rootSuccessors : successors[id]; };
        stack.push_back({root, 0});
        seen[root] = 1;
        while (!stack.empty()) {
            auto &[node, next] = stack.back();
            const std::vector<BlockId> &succ = successorsOf(node);
            if (next < succ.size()) {
                const BlockId to = succ[next++];
                if (!seen[to]) {
                    seen[to] = 1;
                    stack.push_back({to, 0});
                }
            } else {
                postIndex[node] = static_cast<int>(post.size());
                post.push_back(node);
                stack.pop_back();
            }
        }
        order.assign(post.rbegin(), post.rend());
    }
    std::vector<BlockId> idom(blockCount + 1, noBlock);
    idom[root] = root;
    std::vector<std::vector<BlockId>> preds = predecessors;
    preds.resize(blockCount + 1);
    for (const EntryPoint &entry : f.entries)
        if (entry.block < blockCount) preds[entry.block].push_back(root);
    const auto intersect = [&](BlockId a, BlockId b) {
        while (a != b) {
            while (postIndex[a] < postIndex[b])
                a = idom[a];
            while (postIndex[b] < postIndex[a])
                b = idom[b];
        }
        return a;
    };
    for (bool changed = true; changed;) {
        changed = false;
        for (const BlockId node : order) {
            if (node == root) continue;
            BlockId best = noBlock;
            for (const BlockId pred : preds[node]) {
                if (idom[pred] == noBlock) continue;
                best = best == noBlock ? pred : intersect(pred, best);
            }
            if (best != noBlock && idom[node] != best) {
                idom[node] = best;
                changed = true;
            }
        }
    }
    const auto dominates = [&](BlockId a, BlockId b) {
        for (BlockId node = b; node != noBlock;) {
            if (node == a) return true;
            if (node == root) break;
            node = idom[node];
        }
        return false;
    };

    struct Def {
        BlockId block = noBlock;
        int position = -1;
    };
    std::vector<Def> defs(f.values.size());
    std::vector<uint8_t> defined(f.values.size(), 0);
    const auto define = [&](ValueId value, BlockId block, int position) {
        if (defined[value]) fail("%" + std::to_string(value) + " is defined twice");
        defined[value] = 1;
        defs[value] = {block, position};
    };
    for (BlockId id = 0; id < blockCount; id++) {
        const Block &block = f.blocks[id];
        if (!block.reachable) continue;
        for (ValueId param : block.params)
            define(param, id, -1);
        for (size_t i = 0; i < block.instrs.size(); i++)
            for (ValueId result : block.instrs[i].results)
                define(result, id, static_cast<int>(i));
    }
    const auto use = [&](ValueId value, BlockId block, int position, const char *what) {
        if (!defined[value]) {
            fail("b" + std::to_string(block) + ": " + what + " uses undefined %" + std::to_string(value));
            return;
        }
        const Def &def = defs[value];
        const bool ok = def.block == block ? def.position < position : dominates(def.block, block);
        if (!ok) fail("b" + std::to_string(block) + ": " + what + " uses %" + std::to_string(value) + " before it is defined");
    };
    for (BlockId id = 0; id < blockCount; id++) {
        const Block &block = f.blocks[id];
        if (!block.reachable) continue;
        for (size_t i = 0; i < block.instrs.size(); i++) {
            const Instr &instr = block.instrs[i];
            for (ValueId arg : instr.args)
                use(arg, id, static_cast<int>(i), "instruction");
            for (ValueId value : instr.frame)
                use(value, id, static_cast<int>(i), "frame state");
        }
        const int end = static_cast<int>(block.instrs.size());
        for (ValueId value : block.termFrame)
            use(value, id, end, "terminator frame state");
        if (block.term == Terminator::CondBr || block.term == Terminator::Ret) use(block.cond, id, end, "terminator");
        const int edges = block.term == Terminator::Br ? 1 : block.term == Terminator::CondBr ? 2
                                                                                              : 0;
        for (int e = 0; e < edges; e++)
            for (ValueId arg : block.edges[e].args)
                use(arg, id, end, "edge");
    }
    return errors;
}

std::string summary(const BuildResult &result, const std::vector<std::string> &verifyErrors) {
    std::ostringstream out;
    const BuildStats &s = result.stats;
    out << "[ir] " << s.instructions << " instructions, " << s.blocks << " blocks, " << result.function.values.size() << " values; "
        << s.structured << " structured, " << s.generic << " generic ("
        << (s.instructions != 0 ? 100.0 * s.structured / s.instructions : 0.0) << "% structured)";
    out << "; builder errors " << result.errors.size() << ", verifier errors " << verifyErrors.size();
    {
        std::vector<uint32_t> byOp(32, 0);
        for (const Block &block : result.function.blocks)
            for (const Instr &instr : block.instrs)
                byOp[static_cast<size_t>(instr.op)]++;
        out << "\n[ir]   categories:";
        for (size_t op = 0; op < byOp.size(); op++)
            if (byOp[op] != 0) out << " " << opName(static_cast<Op>(op)) << "=" << byOp[op];
    }
    std::vector<std::pair<uint32_t, size_t>> top;
    for (size_t op = 0; op < s.byBytecode.size(); op++)
        if (s.byBytecode[op] != 0) top.push_back({s.byBytecode[op], op});
    std::sort(top.rbegin(), top.rend());
    out << "\n[ir]   generic by op:";
    for (size_t i = 0; i < top.size() && i < 12; i++)
        out << " " << BytecodeDisassembler::opMnemonic(static_cast<BytecodeOp>(top[i].second)) << "=" << top[i].first;
    for (size_t i = 0; i < result.errors.size() && i < 5; i++)
        out << "\n[ir]   builder: " << result.errors[i];
    for (size_t i = 0; i < verifyErrors.size() && i < 5; i++)
        out << "\n[ir]   verify: " << verifyErrors[i];
    return out.str();
}

} // namespace BytecodeJitIR

#endif
