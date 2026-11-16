#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "bytecode.hpp"
#include "bytecode_analysis.hpp"
#include "jit_emitter.hpp"
#include "jit_planner.hpp"
#include "jit_runtime.hpp"
#include "jit_types.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace BytecodeJitInternal {

void Emitter::emitCompareValue(BytecodeCompareKind kind, uint32_t a, uint32_t b) {
    const uint32_t out = booleanSlot(state_.booleans);
    loadF(lir::FR0, a);
    loadF(lir::FR1, b);
    lir::Jump *unordered = lir::emitFCmp(c_, lir::UNORDERED, lir::FR0, 0, lir::FR1, 0);

    const lir::Int setFlag = kind == BytecodeCompareKind::Equals ? lir::SET_F_EQUAL : kind == BytecodeCompareKind::Less ? lir::SET_F_LESS
                                                                                                                        : lir::SET_F_GREATER;
    const lir::Int flag = kind == BytecodeCompareKind::Equals ? lir::F_EQUAL : kind == BytecodeCompareKind::Less ? lir::F_LESS
                                                                                                                 : lir::F_GREATER;
    lir::emitFOp1(c_, lir::CMP_F64 | setFlag, lir::FR0, 0, lir::FR1, 0);
    storeFlag(out, flag);
    lir::Label *done = lir::emitLabel(c_);

    addCold([this, unordered, done, kind, out]() {
        lir::setLabel(unordered, lir::emitLabel(c_));
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, static_cast<lir::Word>(kind));
        callHelper(reinterpret_cast<void *>(&Runtime::jitCompareUnordered), LIR_ARGS3(W, F64, F64, W));
        storeW(out, lir::R0);
        lir::setLabel(lir::emitJump(c_, lir::JUMP), done);
    });
}

void Emitter::emitCompareJump(const BytecodeInstruction &instr, BytecodeIndex pc) {
    const BytecodeCompareKind kind = static_cast<BytecodeCompareKind>(instr.b);
    const uint32_t a = numberSlot(state_.numbers - 2), b = numberSlot(state_.numbers - 1);
    state_.numbers -= 2;
    emitCompareBranch(kind, instr.op == BytecodeOp::JumpIfNumCompareTrue, instr.a, instr.a <= pc, {false, a, 0}, {false, b, 0}, state_);
}

void Emitter::loadOperand(lir::Int reg, const Operand &operand) {
    if (operand.immediate) lir::emitFSet64(c_, reg, operand.value);
    else loadF(reg, operand.slot);
}

bool Emitter::fedCompare(BytecodeIndex compare, Operand &a, Operand &b) const {
    if (compare < 2 || regionOf_.empty()) return false;
    const BytecodeCompareKind unused = BytecodeCompareKind::Equals;
    (void)unused;
    const BytecodeOp op = chunk_.instructions[compare].op;
    if (op != BytecodeOp::ValueEquals && op != BytecodeOp::ValueLess && op != BytecodeOp::ValueGreater) return false;
    if (analysis_.isBlockStart(compare) || analysis_.isBlockStart(compare - 1)) return false;
    if (!plan_.supported[compare - 2] || !plan_.supported[compare - 1] || !plan_.supported[compare]) return false;
    if (plan_.entryOf[compare - 1] >= 0 || plan_.entryOf[compare] >= 0) return false;
    BytecodeAbstractState s;
    if (!analysis_.stateBefore(compare, s) || s.values.size() < 2) return false;
    if (!Planner::numericLike(s.values[s.values.size() - 1]) || !Planner::numericLike(s.values[s.values.size() - 2])) return false;
    bool jumpIfTrue;
    BytecodeIndex jumpPc;
    if (!(compare < condLeaf_.size() && condLeaf_[compare]) && !fusableBranch(compare, jumpIfTrue, jumpPc)) return false;
    Operand *out[2] = {&a, &b};
    bool converted = false;
    for (int i = 0; i < 2; i++) {
        const BytecodeInstruction &push = chunk_.instructions[compare - 2 + i];
        if (push.op == BytecodeOp::ConvertNumberToValue) {
            if (converted) return false;
            converted = true;
            *out[i] = {false, numberSlot(state_.numbers), 0};
        } else if (push.op == BytecodeOp::PushVarValue) {
            if (push.a >= promotedIndex_.size() || promotedIndex_[push.a] < 0 || stringWritten_[push.a]) return false;
            *out[i] = {false, variableSlot(static_cast<uint32_t>(promotedIndex_[push.a])), 0};
        } else if (push.op == BytecodeOp::PushConstValue) {
            const Value &constant = chunk_.constants[push.a];
            if (!constant.isRealNumber() && !constant.isStringTyped()) return false;
            *out[i] = {true, 0, constant.numberRaw()};
        } else {
            return false;
        }
    }
    return true;
}

void Emitter::emitCompareBranch(BytecodeCompareKind kind, bool jumpIfTrue, BytecodeIndex target, bool backEdge, Operand a, Operand b, const AbsState &atTarget) {
    if (shadowSupported && active_ != nullptr && active_->shadowSlot >= 0 && a.immediate != b.immediate) {
        const bool variableFirst = !a.immediate;
        const Operand &variable = variableFirst ? a : b;
        const Operand &constant = variableFirst ? b : a;
        if (variable.slot == static_cast<uint32_t>(active_->shadowSlot) && std::isfinite(constant.value) && constant.value == std::floor(constant.value) && std::fabs(constant.value) < 2147483648.0) {
            lir::emitOp2U(c_, lir::SUB | lir::SET_OVERFLOW, lir::S1, 0, lir::IMM, 1);
            lir::Jump *notInteger = lir::emitJump(c_, lir::OVERFLOW_SET);
            const bool less = kind == BytecodeCompareKind::Less ? variableFirst : (kind == BytecodeCompareKind::Greater ? !variableFirst : false);
            lir::Int type;
            if (kind == BytecodeCompareKind::Equals) type = jumpIfTrue ? lir::EQUAL : lir::NOT_EQUAL;
            else if (less) type = jumpIfTrue ? lir::SIG_LESS : lir::SIG_GREATER_EQUAL;
            else type = jumpIfTrue ? lir::SIG_GREATER : lir::SIG_LESS_EQUAL;
            emitJumpTo(lir::emitCmp(c_, type, lir::S1, 0, lir::IMM, static_cast<lir::Word>(constant.value)), target, atTarget, backEdge);
            lir::Label *fall = lir::emitLabel(c_);
            addCold([this, notInteger, fall, kind, jumpIfTrue, target, backEdge, a, b, atTarget]() {
                lir::setLabel(notInteger, lir::emitLabel(c_));
                emitCompareBranchFloat(kind, jumpIfTrue, target, backEdge, a, b, atTarget);
                lir::setLabel(lir::emitJump(c_, lir::JUMP), fall);
            });
            return;
        }
    }
    emitCompareBranchFloat(kind, jumpIfTrue, target, backEdge, a, b, atTarget);
}

void Emitter::emitCompareBranchFloat(BytecodeCompareKind kind, bool jumpIfTrue, BytecodeIndex target, bool backEdge, Operand a, Operand b, const AbsState &atTarget) {
    lir::Int left = lir::FR0, right = lir::FR1;
    lir::Word rightOffset = 0;
    if (a.immediate) loadOperand(lir::FR0, a);
    else left = readF(a.slot, lir::FR0);
    const int poolSlot = b.immediate ? poolIndex(b.value) : -1;
    if (poolSlot >= 0) {
        right = lir::MEM1(lir::S0);
        rightOffset = poolOffset(poolSlot);
    } else if (b.immediate) {
        loadOperand(lir::FR1, b);
    } else {
        right = readF(b.slot, lir::FR1);
    }
    lir::Jump *unordered = lir::emitFCmp(c_, lir::UNORDERED, left, 0, right, rightOffset);

    lir::Int type;
    switch (kind) {
    case BytecodeCompareKind::Equals:
        type = jumpIfTrue ? lir::ORDERED_EQUAL : lir::ORDERED_NOT_EQUAL;
        break;
    case BytecodeCompareKind::Less:
        type = jumpIfTrue ? lir::ORDERED_LESS : lir::ORDERED_GREATER_EQUAL;
        break;
    default:
        type = jumpIfTrue ? lir::ORDERED_GREATER : lir::ORDERED_LESS_EQUAL;
        break;
    }
    emitJumpTo(lir::emitFCmp(c_, type, left, 0, right, rightOffset), target, atTarget, backEdge);
    lir::Label *fall = lir::emitLabel(c_);

    addCold([this, unordered, fall, kind, jumpIfTrue, atTarget, backEdge, target, a, b]() {
        lir::setLabel(unordered, lir::emitLabel(c_));
        loadOperand(lir::FR0, a);
        loadOperand(lir::FR1, b);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, static_cast<lir::Word>(kind));
        callHelper(reinterpret_cast<void *>(&Runtime::jitCompareUnordered), LIR_ARGS3(W, F64, F64, W));
        lir::Jump *no = lir::emitCmp(c_, jumpIfTrue ? lir::EQUAL : lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0);
        if (backEdge) {
            emitBackEdgeThenJump(target, atTarget);
        } else {
            pendUnconditional(target);
        }
        lir::setLabel(no, fall);
    });
}

bool Emitter::fusableBranch(BytecodeIndex pc, bool &jumpIfTrue, BytecodeIndex &jumpPc) const {
    static const bool disabled = std::getenv("SE_JIT_NOFUSE") != nullptr;
    if (disabled || suppressFusion_) return false;
    const size_t count = chunk_.instructions.size();
    bool negated = false;
    for (BytecodeIndex q = pc + 1; q < count; q++) {
        if (!plan_.supported[q] || analysis_.isBlockStart(q) || plan_.entryOf[q] >= 0) return false;
        const BytecodeInstruction &next = chunk_.instructions[q];
        if (next.op == BytecodeOp::CallNativeBoolean && plan_.nativeShape(next) == NativeShape::Not) {
            negated = !negated;
            continue;
        }
        if (next.op != BytecodeOp::JumpIfTrue && next.op != BytecodeOp::JumpIfFalse) return false;
        if (regionOf_[pc] >= 0 && !regions_[regionOf_[pc]].span && q >= regions_[regionOf_[pc]].tail) return false;
        jumpIfTrue = (next.op == BytecodeOp::JumpIfTrue) != negated;
        jumpPc = q;
        return true;
    }
    return false;
}

bool Emitter::conditionOperandOk(BytecodeIndex pc) const {
    const BytecodeInstruction &instr = chunk_.instructions[pc];
    switch (instr.op) {
    case BytecodeOp::PushConstNumber:
    case BytecodeOp::PushVarNumber:
    case BytecodeOp::PushConstValue:
    case BytecodeOp::PushVarValue:
    case BytecodeOp::ConvertNumberToValue:
    case BytecodeOp::ConvertValueToNumber:
    case BytecodeOp::MathOp:
    case BytecodeOp::RandomInt:
    case BytecodeOp::PushArg:
    case BytecodeOp::ItemOfList:
    case BytecodeOp::LengthOfList:
        return true;
    case BytecodeOp::CallNativeNumber: {
        const NativeShape shape = plan_.nativeShape(instr);
        const std::string &name = plan_.opcodeName(instr.a);
        return shape == NativeShape::Add || shape == NativeShape::Subtract || shape == NativeShape::Multiply || shape == NativeShape::Divide || name == "operator_mod" || name == "operator_round";
    }
    default:
        return false;
    }
}

bool Emitter::parseCondition(BytecodeIndex jump, CondTree &tree, BytecodeIndex &start) const {
    BytecodeAbstractState end;
    if (!analysis_.stateBefore(jump, end) || end.booleans == 0) return false;
    const uint32_t n0 = end.numbers, b0 = end.booleans - 1, v0 = static_cast<uint32_t>(end.values.size());

    BytecodeIndex q = jump;
    bool found = false;
    while (q > 0 && jump - q < 96) {
        q--;
        BytecodeAbstractState s;
        if (!analysis_.stateBefore(q, s) || !plan_.supported[q]) return false;
        if (s.numbers == n0 && s.booleans == b0 && s.values.size() == v0 && s.strings == end.strings) {
            found = true;
            break;
        }
    }
    if (!found) return false;
    for (BytecodeIndex p = q + 1; p <= jump; p++) {
        if (!plan_.supported[p] || analysis_.isBlockStart(p) || plan_.entryOf[p] >= 0) return false;
    }

    std::vector<int> stack;
    BytecodeIndex pending = q;
    for (BytecodeIndex pc = q; pc < jump; pc++) {
        const BytecodeInstruction &instr = chunk_.instructions[pc];
        switch (instr.op) {
        case BytecodeOp::NumEquals:
        case BytecodeOp::NumLess:
        case BytecodeOp::NumGreater:
        case BytecodeOp::ValueEquals:
        case BytecodeOp::ValueLess:
        case BytecodeOp::ValueGreater: {
            if (pending >= pc) return false;
            BytecodeAbstractState before, atCompare;
            if (!analysis_.stateBefore(pending, before) || !analysis_.stateBefore(pc, atCompare)) return false;
            const bool numeric = instr.op == BytecodeOp::NumEquals || instr.op == BytecodeOp::NumLess || instr.op == BytecodeOp::NumGreater;
            if (atCompare.booleans != before.booleans) return false;
            if (numeric) {
                if (atCompare.numbers != before.numbers + 2 || atCompare.values.size() != before.values.size()) return false;
            } else if (atCompare.values.size() != before.values.size() + 2 || atCompare.numbers != before.numbers) {
                return false;
            }
            CondNode node;
            node.kind = CondNode::Leaf;
            node.rangeStart = pending;
            node.compare = pc;
            tree.nodes.push_back(node);
            stack.push_back(static_cast<int>(tree.nodes.size()) - 1);
            pending = pc + 1;
            break;
        }
        case BytecodeOp::CallNativeBoolean: {
            const NativeShape shape = plan_.nativeShape(instr);
            CondNode node;
            if (shape == NativeShape::Not) {
                if (stack.empty()) return false;
                node.kind = CondNode::Not;
                node.a = stack.back();
                stack.pop_back();
            } else if (shape == NativeShape::And || shape == NativeShape::Or) {
                if (stack.size() < 2) return false;
                node.kind = shape == NativeShape::And ? CondNode::And : CondNode::Or;
                node.b = stack.back();
                stack.pop_back();
                node.a = stack.back();
                stack.pop_back();
            } else {
                return false;
            }
            tree.nodes.push_back(node);
            stack.push_back(static_cast<int>(tree.nodes.size()) - 1);
            pending = pc + 1;
            break;
        }
        default:
            if (!conditionOperandOk(pc)) return false;
            break;
        }
    }
    if (stack.size() != 1 || pending != jump) return false;
    tree.root = stack.back();
    tree.jump = jump;
    start = q;
    return true;
}

void Emitter::buildConditions() {
    static const bool disabled = std::getenv("SE_JIT_NOCOND") != nullptr;
    if (disabled) return;
    const size_t count = chunk_.instructions.size();
    for (BytecodeIndex pc = 0; pc < count; pc++) {
        const BytecodeOp op = chunk_.instructions[pc].op;
        if ((op != BytecodeOp::JumpIfFalse && op != BytecodeOp::JumpIfTrue) || !plan_.supported[pc] || !analysis_.isReachable(pc)) continue;
        CondTree tree;
        BytecodeIndex start = 0;
        if (!parseCondition(pc, tree, start) || conditions_.count(start)) continue;
        for (const CondNode &node : tree.nodes) {
            if (node.kind == CondNode::Leaf) condLeaf_[node.compare] = 1;
        }
        conditions_[start] = std::move(tree);
    }
}

void Emitter::bindCondLabel(CondLabel &label) {
    label.label = lir::emitLabel(c_);
    for (lir::Jump *jump : label.pending)
        lir::setLabel(jump, label.label);
    label.pending.clear();
}

void Emitter::routeJump(lir::Jump *jump, const CondTarget &target) {
    if (target.kind == CondTarget::Bytecode) {
        emitJumpTo(jump, target.pc, *target.state, target.backEdge);
    } else if (target.label->label != nullptr) {
        lir::setLabel(jump, target.label->label);
    } else {
        target.label->pending.push_back(jump);
    }
}

void Emitter::routeUnconditional(const CondTarget &target) {
    if (target.kind == CondTarget::Bytecode) {
        if (target.backEdge) emitBackEdgeThenJump(target.pc, *target.state);
        else pendUnconditional(target.pc);
    } else {
        routeJump(lir::emitJump(c_, lir::JUMP), target);
    }
}

void Emitter::emitLeafTest(BytecodeCompareKind kind, bool jumpIfTrue, Operand a, Operand b, const CondTarget &target, const std::vector<lir::Jump *> &notPlain, const CompareDescriptor *general) {
    if (shadowSupported && active_ != nullptr && active_->shadowSlot >= 0 && a.immediate != b.immediate && notPlain.empty() && general == nullptr) {
        const bool variableFirst = !a.immediate;
        const Operand &variable = variableFirst ? a : b;
        const Operand &constant = variableFirst ? b : a;
        if (variable.slot == static_cast<uint32_t>(active_->shadowSlot) && std::isfinite(constant.value) && constant.value == std::floor(constant.value) && std::fabs(constant.value) < 2147483648.0) {
            lir::emitOp2U(c_, lir::SUB | lir::SET_OVERFLOW, lir::S1, 0, lir::IMM, 1);
            lir::Jump *notInteger = lir::emitJump(c_, lir::OVERFLOW_SET);
            const bool less = kind == BytecodeCompareKind::Less ? variableFirst : (kind == BytecodeCompareKind::Greater ? !variableFirst : false);
            lir::Int type;
            if (kind == BytecodeCompareKind::Equals) type = jumpIfTrue ? lir::EQUAL : lir::NOT_EQUAL;
            else if (less) type = jumpIfTrue ? lir::SIG_LESS : lir::SIG_GREATER_EQUAL;
            else type = jumpIfTrue ? lir::SIG_GREATER : lir::SIG_LESS_EQUAL;
            routeJump(lir::emitCmp(c_, type, lir::S1, 0, lir::IMM, static_cast<lir::Word>(constant.value)), target);
            lir::Label *fall = lir::emitLabel(c_);
            addCold([this, notInteger, fall, kind, jumpIfTrue, a, b, target]() {
                lir::setLabel(notInteger, lir::emitLabel(c_));
                emitLeafTestFloat(kind, jumpIfTrue, a, b, target, {}, nullptr);
                lir::setLabel(lir::emitJump(c_, lir::JUMP), fall);
            });
            return;
        }
    }
    emitLeafTestFloat(kind, jumpIfTrue, a, b, target, notPlain, general);
}

void Emitter::emitLeafTestFloat(BytecodeCompareKind kind, bool jumpIfTrue, Operand a, Operand b, const CondTarget &target, const std::vector<lir::Jump *> &notPlain, const CompareDescriptor *general) {
    loadOperand(lir::FR0, a);
    loadOperand(lir::FR1, b);
    lir::Jump *unordered = lir::emitFCmp(c_, lir::UNORDERED, lir::FR0, 0, lir::FR1, 0);

    lir::Int type;
    switch (kind) {
    case BytecodeCompareKind::Equals:
        type = jumpIfTrue ? lir::ORDERED_EQUAL : lir::ORDERED_NOT_EQUAL;
        break;
    case BytecodeCompareKind::Less:
        type = jumpIfTrue ? lir::ORDERED_LESS : lir::ORDERED_GREATER_EQUAL;
        break;
    default:
        type = jumpIfTrue ? lir::ORDERED_GREATER : lir::ORDERED_LESS_EQUAL;
        break;
    }
    routeJump(lir::emitFCmp(c_, type, lir::FR0, 0, lir::FR1, 0), target);
    lir::Label *fall = lir::emitLabel(c_);

    addCold([this, unordered, fall, kind, jumpIfTrue, target, notPlain, general]() {
        lir::setLabel(unordered, lir::emitLabel(c_));
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, static_cast<lir::Word>(kind));
        callHelper(reinterpret_cast<void *>(&Runtime::jitCompareUnordered), LIR_ARGS3(W, F64, F64, W));
        lir::Jump *no = lir::emitCmp(c_, jumpIfTrue ? lir::EQUAL : lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0);
        routeUnconditional(target);
        lir::setLabel(no, fall);

        if (general != nullptr) {
            lir::Label *here = lir::emitLabel(c_);
            for (lir::Jump *jump : notPlain)
                lir::setLabel(jump, here);
            lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::S0, 0);
            lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::IMM, reinterpret_cast<lir::Word>(general));
            callHelper(reinterpret_cast<void *>(&Runtime::jitCompareValues), LIR_ARGS2(W, P, P));
            lir::Jump *skip = lir::emitCmp(c_, jumpIfTrue ? lir::EQUAL : lir::NOT_EQUAL, lir::R0, 0, lir::IMM, 0);
            routeUnconditional(target);
            lir::setLabel(skip, fall);
        }
    });
}

bool Emitter::guardable(const AbsValue &entry, const Code &code, const BytecodeChunk &chunk) {
    switch (entry.kind) {
    case ValueEntry::Number:
    case ValueEntry::Temp:
        return true;
    case ValueEntry::VarValue:
        return entry.constant < code.promoted.size();
    case ValueEntry::Argument:
        return entry.constant < Runtime::maxTypedArguments;
    case ValueEntry::Constant: {
        if (entry.constant >= chunk.constants.size()) return false;
        const Value &constant = chunk.constants[entry.constant];
        return !constant.isString() && (constant.isRealNumber() || constant.isStringTyped());
    }
    default:
        return false;
    }
}

void Emitter::emitLeaf(const CondTree &, const CondNode &node, const CondTarget &target, bool jumpIfTrue) {
    for (BytecodeIndex pc = node.rangeStart; pc < node.compare; pc++)
        emitInstruction(pc);
    const BytecodeOp op = chunk_.instructions[node.compare].op;
    const bool numeric = op == BytecodeOp::NumEquals || op == BytecodeOp::NumLess || op == BytecodeOp::NumGreater;
    const BytecodeCompareKind kind = (op == BytecodeOp::NumEquals || op == BytecodeOp::ValueEquals) ? BytecodeCompareKind::Equals : (op == BytecodeOp::NumLess || op == BytecodeOp::ValueLess) ? BytecodeCompareKind::Less
                                                                                                                                                                                               : BytecodeCompareKind::Greater;
    Operand a, b;
    if (numeric) {
        a = {false, numberSlot(state_.numbers - 2), 0};
        b = {false, numberSlot(state_.numbers - 1), 0};
        state_.numbers -= 2;
    } else {
        const uint32_t depth = static_cast<uint32_t>(state_.values.size());
        a = {false, valueSlot(depth - 2), 0};
        b = {false, valueSlot(depth - 1), 0};
        BytecodeAbstractState at;
        const bool bothNumeric = analysis_.stateBefore(node.compare, at) && at.values.size() >= 2 && Planner::numericLike(at.values[at.values.size() - 1]) && Planner::numericLike(at.values[at.values.size() - 2]);
        if (bothNumeric) {
            if (!fedCompare(node.compare, a, b)) {
                a = {false, valueSlot(depth - 2), 0};
                b = {false, valueSlot(depth - 1), 0};
            }
        } else if (guardable(state_.values[depth - 2], code_, chunk_) && guardable(state_.values[depth - 1], code_, chunk_)) {
            CompareDescriptor &compare = code_.compares.emplace_back();
            compare.kind = kind;
            compare.a = describe(state_.values[depth - 2]);
            compare.aPosition = depth - 2;
            compare.b = describe(state_.values[depth - 1]);
            compare.bPosition = depth - 1;
            std::vector<lir::Jump *> notPlain;
            guardPlainNumber(state_.values[depth - 2], depth - 2, notPlain);
            guardPlainNumber(state_.values[depth - 1], depth - 1, notPlain);
            state_.values.pop_back();
            state_.values.pop_back();
            emitLeafTest(kind, jumpIfTrue, a, b, target, notPlain, &compare);
            return;
        } else {
            suppressFusion_ = true;
            emitInstruction(node.compare);
            suppressFusion_ = false;
            loadW(lir::R0, booleanSlot(state_.booleans - 1));
            state_.booleans--;
            routeJump(lir::emitCmp(c_, jumpIfTrue ? lir::NOT_EQUAL : lir::EQUAL, lir::R0, 0, lir::IMM, 0), target);
            return;
        }
        state_.values.pop_back();
        state_.values.pop_back();
    }
    emitLeafTest(kind, jumpIfTrue, a, b, target);
}

void Emitter::emitCond(const CondTree &tree, int id, const CondTarget &whenTrue, const CondTarget &whenFalse, bool fallIsTrue) {
    const CondNode &node = tree.nodes[id];
    switch (node.kind) {
    case CondNode::Leaf:
        if (fallIsTrue) emitLeaf(tree, node, whenFalse, false);
        else emitLeaf(tree, node, whenTrue, true);
        return;
    case CondNode::Not:
        emitCond(tree, node.a, whenFalse, whenTrue, !fallIsTrue);
        return;
    case CondNode::And: {
        CondLabel *end = nullptr;
        CondTarget leftFalse = whenFalse;
        if (whenFalse.kind == CondTarget::Fall) {
            end = &condLabels_.emplace_back();
            leftFalse = CondTarget{};
            leftFalse.kind = CondTarget::Label;
            leftFalse.label = end;
        }
        emitCond(tree, node.a, CondTarget{}, leftFalse, true);
        emitCond(tree, node.b, whenTrue, whenFalse, fallIsTrue);
        if (end != nullptr) bindCondLabel(*end);
        return;
    }
    case CondNode::Or: {
        CondLabel *end = nullptr;
        CondTarget leftTrue = whenTrue;
        if (whenTrue.kind == CondTarget::Fall) {
            end = &condLabels_.emplace_back();
            leftTrue = CondTarget{};
            leftTrue.kind = CondTarget::Label;
            leftTrue.label = end;
        }
        emitCond(tree, node.a, leftTrue, CondTarget{}, false);
        emitCond(tree, node.b, whenTrue, whenFalse, fallIsTrue);
        if (end != nullptr) bindCondLabel(*end);
        return;
    }
    }
}

void Emitter::emitCondition(const CondTree &tree, BytecodeIndex start) {
    const BytecodeInstruction &jump = chunk_.instructions[tree.jump];
    const bool jumpIfTrue = jump.op == BytecodeOp::JumpIfTrue;
    for (BytecodeIndex pc = start + 1; pc <= tree.jump; pc++)
        fused_[pc] = true;
    CondTarget target;
    target.kind = CondTarget::Bytecode;
    target.pc = jump.a;
    target.backEdge = jump.a <= tree.jump;
    target.state = std::make_shared<AbsState>(state_);
    if (jumpIfTrue) emitCond(tree, tree.root, target, CondTarget{}, false);
    else emitCond(tree, tree.root, CondTarget{}, target, true);
}

bool Emitter::guardPlainNumber(const AbsValue &entry, uint32_t position, std::vector<lir::Jump *> &notPlain) {
    switch (entry.kind) {
    case ValueEntry::Number:
        return true;
    case ValueEntry::Constant: {
        if (entry.constant >= chunk_.constants.size()) return false;
        const Value &constant = chunk_.constants[entry.constant];
        return !constant.isString() && (constant.isRealNumber() || constant.isStringTyped());
    }
    case ValueEntry::Temp:
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM0(), reinterpret_cast<lir::Word>(&Runtime::tempLazy[position]));
        notPlain.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 0));
        return true;
    case ValueEntry::VarValue: {
        if (entry.constant >= code_.promoted.size()) return false;
        lir::emitOp1(c_, lir::MOV, lir::R2, 0, lir::MEM1(lir::S0), dirtyOffset(entry.constant));
        lir::emitOp2(c_, lir::SUB, lir::R2, 0, lir::R2, 0, lir::IMM, 1);
        lir::Jump *plain = lir::emitCmp(c_, lir::LESS_EQUAL, lir::R2, 0, lir::IMM, 1);
        lir::Jump *constantKind = lir::emitCmp(c_, lir::SIG_GREATER_EQUAL, lir::R2, 0, lir::IMM, 4);
        notPlain.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 3));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM1(lir::S0), static_cast<lir::Word>(offsetof(Context, numericTagged)) + static_cast<lir::Word>(entry.constant));
        notPlain.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 0));
        lir::Jump *tagged = lir::emitJump(c_, lir::JUMP);
        lir::setLabel(constantKind, lir::emitLabel(c_));
        lir::emitOp2(c_, lir::SUB, lir::R2, 0, lir::R2, 0, lir::IMM, 4);
        lir::emitOp1(c_, lir::MOV, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(code_.constantIsNumber.data()));
        lir::emitOp1(c_, lir::MOV_U8, lir::R2, 0, lir::MEM2(lir::R0, lir::R2), 0);
        notPlain.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 0));
        lir::Label *usable = lir::emitLabel(c_);
        lir::setLabel(plain, usable);
        lir::setLabel(tagged, usable);
        return true;
    }
    case ValueEntry::Argument: {
        if (entry.constant >= Runtime::maxTypedArguments) return false;
        const auto field = [](size_t offset) { return static_cast<lir::Word>(offset); };
        const auto load32 = [&](lir::Int reg, size_t offset) { lir::emitOp1(c_, lir::MOV_U32, reg, 0, lir::MEM1(lir::S0), field(offset)); };
        load32(lir::R2, offsetof(Context, lazyDepth));
        notPlain.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 0));
        load32(lir::R0, offsetof(Context, nativeDepth));
        lir::emitOp2(c_, lir::SUB, lir::R0, 0, lir::R0, 0, lir::IMM, 1);
        lir::emitOp2(c_, lir::SHL, lir::R0, 0, lir::R0, 0, lir::IMM, 5);
        lir::emitOp2(c_, lir::ADD, lir::R0, 0, lir::R0, 0, lir::IMM, reinterpret_cast<lir::Word>(Runtime::frames));
        lir::emitOp1(c_, lir::MOV, lir::R1, 0, lir::MEM1(lir::R0), field(offsetof(Runtime::NativeFrame, call)));
        lir::emitOp1(c_, lir::MOV_U32, lir::R2, 0, lir::MEM1(lir::R1), field(offsetof(CallDescriptor, argNumbers)));
        lir::emitOp2(c_, lir::LSHR, lir::R2, 0, lir::R2, 0, lir::IMM, entry.constant);
        lir::emitOp2(c_, lir::AND, lir::R2, 0, lir::R2, 0, lir::IMM, 1);
        notPlain.push_back(lir::emitCmp(c_, lir::EQUAL, lir::R2, 0, lir::IMM, 0));
        return true;
    }
    default:
        return false;
    }
}

} // namespace BytecodeJitInternal

#endif
