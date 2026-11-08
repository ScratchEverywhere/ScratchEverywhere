#include "hir.hpp"

#ifdef SE_JIT

#include <array>
#include <string>

namespace BytecodeJitIR {

namespace {

using K = BytecodeStackKind;

constexpr std::array<K, 5> parameterOrder = {K::Number, K::Boolean, K::String, K::Color, K::Value};

size_t stackIndex(K kind) {
    switch (kind) {
    case K::Number:
        return 0;
    case K::String:
        return 1;
    case K::Boolean:
        return 2;
    case K::Color:
        return 3;
    default:
        return 4;
    }
}

struct Stacks {
    std::array<std::vector<ValueId>, 5> stack;
    std::vector<ValueId> &of(K kind) { return stack[stackIndex(kind)]; }
};

struct Effect {
    std::vector<K> pops;
    std::vector<K> pushes;
};

bool effectOf(const BytecodeChunk &chunk, const BytecodeInstruction &instr, Effect &e, std::string &error) {
    const auto none = [&] { return true; };
    const auto push = [&](K kind) {
        e.pushes.push_back(kind);
        return true;
    };
    const auto pop = [&](K kind, int n = 1) {
        for (int i = 0; i < n; i++)
            e.pops.push_back(kind);
        return true;
    };
    switch (instr.op) {
    case BytecodeOp::Nop:
    case BytecodeOp::ShowListMonitor:
    case BytecodeOp::HideListMonitor:
    case BytecodeOp::ShowVariableMonitor:
    case BytecodeOp::HideVariableMonitor:
    case BytecodeOp::GoToFrontBack:
    case BytecodeOp::SetRotationStyle:
    case BytecodeOp::SetDragMode:
    case BytecodeOp::DeleteAllOfList:
    case BytecodeOp::CallLegacyStatement:
    case BytecodeOp::Yield:
        return none();

    case BytecodeOp::PushConstNumber:
    case BytecodeOp::PushVarNumber:
    case BytecodeOp::LengthOfList:
    case BytecodeOp::LengthOfVar:
    case BytecodeOp::CurrentDateTime:
        return push(K::Number);
    case BytecodeOp::PushConstString:
    case BytecodeOp::PushVarString:
        return push(K::String);
    case BytecodeOp::PushConstBoolean:
    case BytecodeOp::PushVarBoolean:
        return push(K::Boolean);
    case BytecodeOp::PushConstColor:
    case BytecodeOp::PushVarColor:
        return push(K::Color);
    case BytecodeOp::PushConstValue:
    case BytecodeOp::PushVarValue:
    case BytecodeOp::PushList:
    case BytecodeOp::PushArg:
    case BytecodeOp::CallLegacyReporter:
    case BytecodeOp::CostumeNumberName:
    case BytecodeOp::BackdropNumberName:
        return push(K::Value);

    case BytecodeOp::StoreVarNumber:
    case BytecodeOp::ChangeVarNumber:
    case BytecodeOp::PopNumber:
    case BytecodeOp::Wait:
    case BytecodeOp::ChangeEffectBy:
    case BytecodeOp::GoForwardBackwardLayers:
        return pop(K::Number);
    case BytecodeOp::StoreVarString:
    case BytecodeOp::AppendVarString:
    case BytecodeOp::PopString:
    case BytecodeOp::BroadcastAndWait:
        return pop(K::String);
    case BytecodeOp::StoreVarBoolean:
    case BytecodeOp::PopBoolean:
        return pop(K::Boolean);
    case BytecodeOp::StoreVarColor:
    case BytecodeOp::PopColor:
        return pop(K::Color);
    case BytecodeOp::StoreVarValue:
    case BytecodeOp::PopValue:
    case BytecodeOp::AddToList:
    case BytecodeOp::DeleteFromList:
    case BytecodeOp::PlaySoundUntilDone:
    case BytecodeOp::SetEffectTo:
        return pop(K::Value);
    case BytecodeOp::InsertInList:
    case BytecodeOp::ReplaceInList:
        return pop(K::Value, 2);

    case BytecodeOp::ItemOfList:
        return pop(K::Value) && push(K::Value);
    case BytecodeOp::ItemNumOfList:
    case BytecodeOp::LengthOf:
        return pop(K::Value) && push(K::Number);
    case BytecodeOp::ListContainsItem:
        return pop(K::Value) && push(K::Boolean);

    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean:
    case BytecodeOp::CallNativeColor:
    case BytecodeOp::CallNativeValue:
    case BytecodeOp::CallNativeBlocking:
    case BytecodeOp::CallNativeBlockingValue: {
        const BytecodeIndex slot = instr.a;
        if (slot >= chunk.paramKindsStart.size() || slot >= chunk.paramKindsCount.size()) {
            error = "bad opcode slot";
            return false;
        }
        const BytecodeIndex start = chunk.paramKindsStart[slot], count = chunk.paramKindsCount[slot];
        if (start + count > chunk.paramKinds.size()) {
            error = "bad parameter kinds";
            return false;
        }
        for (BytecodeIndex i = 0; i < count; i++)
            e.pops.push_back(chunk.paramKinds[start + i]);
        switch (instr.op) {
        case BytecodeOp::CallNativeNumber:
            push(K::Number);
            break;
        case BytecodeOp::CallNativeString:
            push(K::String);
            break;
        case BytecodeOp::CallNativeBoolean:
            push(K::Boolean);
            break;
        case BytecodeOp::CallNativeColor:
            push(K::Color);
            break;
        case BytecodeOp::CallNativeValue:
        case BytecodeOp::CallNativeBlockingValue:
            push(K::Value);
            break;
        default:
            break;
        }
        return true;
    }

    case BytecodeOp::CallProcedure:
    case BytecodeOp::CallProcedureValue:
        pop(K::Value, static_cast<int>(instr.b));
        if (instr.op == BytecodeOp::CallProcedureValue) push(K::Value);
        return true;

    case BytecodeOp::ConvertNumberToValue:
        return pop(K::Number) && push(K::Value);
    case BytecodeOp::ConvertStringToValue:
        return pop(K::String) && push(K::Value);
    case BytecodeOp::ConvertBooleanToValue:
        return pop(K::Boolean) && push(K::Value);
    case BytecodeOp::ConvertColorToValue:
        return pop(K::Color) && push(K::Value);
    case BytecodeOp::ConvertValueToNumber:
        return pop(K::Value) && push(K::Number);
    case BytecodeOp::ConvertValueToString:
        return pop(K::Value) && push(K::String);
    case BytecodeOp::ConvertValueToBoolean:
        return pop(K::Value) && push(K::Boolean);
    case BytecodeOp::ConvertValueToColor:
        return pop(K::Value) && push(K::Color);

    case BytecodeOp::RandomInt:
        return pop(K::Number, 2) && push(K::Number);
    case BytecodeOp::Random:
        return pop(K::Value, 2) && push(K::Number);
    case BytecodeOp::LetterOf:
        e.pops.push_back(K::Number);
        e.pops.push_back(K::Value);
        return push(K::String);
    case BytecodeOp::LetterOfVar:
        return pop(K::Number) && push(K::String);
    case BytecodeOp::MathOp:
        return pop(K::Number) && push(K::Number);

    case BytecodeOp::ValueEquals:
    case BytecodeOp::ValueLess:
    case BytecodeOp::ValueGreater:
        return pop(K::Value, 2) && push(K::Boolean);
    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater:
        return pop(K::Number, 2) && push(K::Boolean);
    case BytecodeOp::StrEquals:
    case BytecodeOp::StrLess:
    case BytecodeOp::StrGreater:
        return pop(K::String, 2) && push(K::Boolean);

    default:
        error = "no stack effect for this operation";
        return false;
    }
}

BytecodeValueType resultKind(const BytecodeChunk &chunk, const BytecodeInstruction &instr) {
    switch (instr.op) {
    case BytecodeOp::ConvertNumberToValue:
        return BytecodeValueType::Number;
    case BytecodeOp::ConvertStringToValue:
        return BytecodeValueType::String;
    case BytecodeOp::ConvertBooleanToValue:
        return BytecodeValueType::Boolean;
    case BytecodeOp::ConvertColorToValue:
        return BytecodeValueType::Color;
    case BytecodeOp::PushConstValue:
        if (instr.a < chunk.constants.size()) {
            const Value &constant = chunk.constants[instr.a];
            if (constant.isRealNumber()) return BytecodeValueType::Number;
            if (constant.isBoolean()) return BytecodeValueType::Boolean;
            if (constant.isColor()) return BytecodeValueType::Color;
        }
        return BytecodeValueType::Unknown;
    default:
        return BytecodeValueType::Unknown;
    }
}

const std::string &opcodeName(const BytecodeChunk &chunk, BytecodeIndex slot) {
    static const std::string empty;
    if (slot >= chunk.opcodeSlots.size() || chunk.opcodeSlots[slot] >= chunk.strings.size()) return empty;
    return chunk.strings[chunk.opcodeSlots[slot]];
}

Op classify(const BytecodeChunk &chunk, const BytecodeInstruction &instr, uint8_t &sub, uint8_t &effects) {
    sub = 0;
    effects = Opaque;
    switch (instr.op) {
    case BytecodeOp::PushVarNumber:
    case BytecodeOp::PushVarString:
    case BytecodeOp::PushVarBoolean:
    case BytecodeOp::PushVarColor:
    case BytecodeOp::PushVarValue:
    case BytecodeOp::LetterOfVar:
    case BytecodeOp::LengthOfVar:
        effects = ReadsVariable;
        return Op::Var;
    case BytecodeOp::StoreVarNumber:
    case BytecodeOp::StoreVarString:
    case BytecodeOp::StoreVarBoolean:
    case BytecodeOp::StoreVarColor:
    case BytecodeOp::StoreVarValue:
    case BytecodeOp::AppendVarString:
    case BytecodeOp::ChangeVarNumber:
        effects = Writes;
        return Op::Var;
    case BytecodeOp::PushArg:
        effects = NoEffect;
        return Op::Arg;
    case BytecodeOp::ConvertNumberToValue:
    case BytecodeOp::ConvertStringToValue:
    case BytecodeOp::ConvertBooleanToValue:
    case BytecodeOp::ConvertColorToValue:
    case BytecodeOp::ConvertValueToNumber:
    case BytecodeOp::ConvertValueToString:
    case BytecodeOp::ConvertValueToBoolean:
    case BytecodeOp::ConvertValueToColor:
        effects = NoEffect;
        return Op::Convert;
    case BytecodeOp::MathOp:
        effects = NoEffect;
        return Op::MathFn;
    case BytecodeOp::NumEquals:
    case BytecodeOp::NumLess:
    case BytecodeOp::NumGreater:
    case BytecodeOp::StrEquals:
    case BytecodeOp::StrLess:
    case BytecodeOp::StrGreater:
    case BytecodeOp::ValueEquals:
    case BytecodeOp::ValueLess:
    case BytecodeOp::ValueGreater:
        effects = NoEffect;
        return Op::Compare;
    case BytecodeOp::PushList:
    case BytecodeOp::ItemOfList:
    case BytecodeOp::ItemNumOfList:
    case BytecodeOp::LengthOfList:
    case BytecodeOp::ListContainsItem:
        effects = ReadsList;
        return Op::List;
    case BytecodeOp::AddToList:
    case BytecodeOp::DeleteFromList:
    case BytecodeOp::DeleteAllOfList:
    case BytecodeOp::InsertInList:
    case BytecodeOp::ReplaceInList:
        effects = Writes;
        return Op::List;
    case BytecodeOp::CallProcedure:
    case BytecodeOp::CallProcedureValue:
        return Op::Call;
    case BytecodeOp::CallLegacyStatement:
    case BytecodeOp::CallLegacyReporter:
        return Op::Legacy;
    case BytecodeOp::Yield:
        effects = Yields;
        return Op::Yield;
    case BytecodeOp::PopNumber:
    case BytecodeOp::PopString:
    case BytecodeOp::PopBoolean:
    case BytecodeOp::PopColor:
    case BytecodeOp::PopValue:
        effects = NoEffect;
        return Op::Discard;
    case BytecodeOp::LetterOf:
    case BytecodeOp::LengthOf:
        effects = NoEffect;
        return Op::Native;
    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean:
    case BytecodeOp::CallNativeColor:
    case BytecodeOp::CallNativeValue: {
        const std::string &name = opcodeName(chunk, instr.a);
        const uint32_t args = instr.a < chunk.paramKindsCount.size() ? chunk.paramKindsCount[instr.a] : 0;
        if (instr.op == BytecodeOp::CallNativeNumber && args == 2) {
            if (name == "operator_add") sub = static_cast<uint8_t>(ArithKind::Add);
            else if (name == "operator_subtract") sub = static_cast<uint8_t>(ArithKind::Subtract);
            else if (name == "operator_multiply") sub = static_cast<uint8_t>(ArithKind::Multiply);
            else if (name == "operator_divide") sub = static_cast<uint8_t>(ArithKind::Divide);
            else if (name == "operator_mod") sub = static_cast<uint8_t>(ArithKind::Mod);
        } else if (instr.op == BytecodeOp::CallNativeBoolean) {
            if (name == "operator_and" && args == 2) sub = static_cast<uint8_t>(ArithKind::And);
            else if (name == "operator_or" && args == 2) sub = static_cast<uint8_t>(ArithKind::Or);
            else if (name == "operator_not" && args == 1) sub = static_cast<uint8_t>(ArithKind::Not);
        }
        if (sub != 0) {
            effects = NoEffect;
            return Op::Arith;
        }
        if (name == "operator_join" || name == "operator_contains") effects = NoEffect;
        return Op::Native;
    }
    case BytecodeOp::CallNativeBlocking:
    case BytecodeOp::CallNativeBlockingValue:
    case BytecodeOp::Wait:
    case BytecodeOp::BroadcastAndWait:
    case BytecodeOp::PlaySoundUntilDone:
    case BytecodeOp::RandomInt:
    case BytecodeOp::Random:
    case BytecodeOp::CurrentDateTime:
    case BytecodeOp::SetEffectTo:
    case BytecodeOp::ChangeEffectBy:
    case BytecodeOp::GoToFrontBack:
    case BytecodeOp::GoForwardBackwardLayers:
    case BytecodeOp::SetRotationStyle:
    case BytecodeOp::SetDragMode:
    case BytecodeOp::ShowListMonitor:
    case BytecodeOp::HideListMonitor:
    case BytecodeOp::ShowVariableMonitor:
    case BytecodeOp::HideVariableMonitor:
    case BytecodeOp::CostumeNumberName:
    case BytecodeOp::BackdropNumberName:
        return Op::Effect;
    default:
        return Op::Bytecode;
    }
}

class Builder {
  public:
    Builder(const BytecodeChunk &chunk, const BytecodeAnalysis &analysis, BuildResult &out, const std::vector<BytecodeIndex> &extraStarts, bool recordFrames) : chunk_(chunk), analysis_(analysis), out_(out), f_(out.function), extraStart_(chunk.instructions.size() + 1, 0), recordFrames_(recordFrames) {
        out_.stats.byBytecode.assign(static_cast<size_t>(BytecodeOp::Halt) + 1, 0);
        for (const BytecodeIndex pc : extraStarts)
            if (pc < extraStart_.size()) extraStart_[pc] = 1;
    }

    void run() {
        const size_t count = chunk_.instructions.size();
        blockOf_.assign(count + 1, noBlock);
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (!analysis_.isBlockStart(pc) && !extraStart_[pc]) continue;
            BytecodeAbstractState state;
            Block block;
            block.startPc = pc;
            block.endPc = pc + 1;
            const BlockId id = static_cast<BlockId>(f_.blocks.size());
            block.reachable = analysis_.stateBefore(pc, state);
            if (block.reachable) {
                const std::array<uint32_t, 5> counts = {state.numbers, state.booleans, state.strings, state.colors, static_cast<uint32_t>(state.values.size())};
                for (size_t k = 0; k < parameterOrder.size(); k++) {
                    for (uint32_t i = 0; i < counts[k]; i++) {
                        const ValueId value = f_.newValue(typeOf(parameterOrder[k]), parameterOrder[k] == K::Value ? state.values[i] : BytecodeValueType::Unknown);
                        f_.values[value].block = id;
                        f_.values[value].index = static_cast<uint32_t>(block.params.size());
                        f_.values[value].isParameter = true;
                        block.params.push_back(value);
                    }
                }
            }
            f_.blocks.push_back(std::move(block));
            blockOf_[pc] = id;
        }
        for (size_t i = 0; i < f_.blocks.size(); i++)
            f_.blocks[i].endPc = i + 1 < f_.blocks.size() ? f_.blocks[i + 1].startPc : static_cast<BytecodeIndex>(count);
        for (BytecodeIndex pc = 0; pc < count; pc++) {
            if (blockOf_[pc] != noBlock) current_ = blockOf_[pc];
            blockOf_[pc] = current_;
        }

        for (size_t i = 0; i < chunk_.scripts.size(); i++)
            addEntry(chunk_.scripts[i].entryInstruction, false, static_cast<uint32_t>(i));
        for (size_t i = 0; i < chunk_.procedures.size(); i++)
            addEntry(chunk_.procedures[i].entryInstruction, true, static_cast<uint32_t>(i));

        for (BlockId id = 0; id < f_.blocks.size(); id++) {
            if (!f_.blocks[id].reachable) continue;
            out_.stats.blocks++;
            fill(id);
        }
    }

  private:
    const BytecodeChunk &chunk_;
    const BytecodeAnalysis &analysis_;
    BuildResult &out_;
    Function &f_;
    std::vector<BlockId> blockOf_;
    std::vector<uint8_t> extraStart_;
    bool recordFrames_ = false;
    BlockId current_ = 0;

    void error(const std::string &message) { out_.errors.push_back(message); }

    void addEntry(BytecodeIndex pc, bool isProcedure, uint32_t index) {
        if (pc >= chunk_.instructions.size() || !analysis_.isReachable(pc) || f_.blocks[blockOf_[pc]].startPc != pc) return;
        EntryPoint entry;
        entry.block = blockOf_[pc];
        entry.pc = pc;
        entry.isProcedure = isProcedure;
        entry.index = index;
        f_.entries.push_back(entry);
    }

    ValueId define(Instr &instr, K kind, BytecodeValueType valueKind = BytecodeValueType::Unknown) {
        const ValueId value = f_.newValue(typeOf(kind), valueKind);
        instr.results.push_back(value);
        return value;
    }

    Edge edgeTo(BytecodeIndex pc, Stacks &stacks) {
        Edge edge;
        if (pc >= blockOf_.size() || blockOf_[pc] == noBlock) {
            error("jump to instruction " + std::to_string(pc) + " outside the chunk");
            return edge;
        }
        edge.target = blockOf_[pc];
        if (f_.blocks[edge.target].startPc != pc) error("jump into the middle of a block at instruction " + std::to_string(pc));
        for (const K kind : parameterOrder)
            for (const ValueId value : stacks.of(kind))
                edge.args.push_back(value);
        return edge;
    }

    static std::vector<ValueId> snapshot(Stacks &stacks) {
        std::vector<ValueId> frame;
        for (const K kind : parameterOrder)
            for (const ValueId value : stacks.of(kind))
                frame.push_back(value);
        return frame;
    }

    bool pop(Stacks &stacks, K kind, ValueId &out) {
        std::vector<ValueId> &stack = stacks.of(kind);
        if (stack.empty()) return false;
        out = stack.back();
        stack.pop_back();
        return true;
    }

    void fill(BlockId id) {
        Block &block = f_.blocks[id];
        Stacks stacks;
        {
            size_t at = 0;
            const std::array<K, 5> order = parameterOrder;
            for (const ValueId param : block.params) {
                const Type type = f_.values[param].type;
                K kind = type == Type::F64 ? K::Number : type == Type::Bool ? K::Boolean
                                                     : type == Type::Str    ? K::String
                                                     : type == Type::Color  ? K::Color
                                                                            : K::Value;
                stacks.of(kind).push_back(param);
                at++;
            }
            (void)order;
            (void)at;
        }

        for (BytecodeIndex pc = block.startPc; pc < block.endPc; pc++) {
            const BytecodeInstruction &instr = chunk_.instructions[pc];
            out_.stats.instructions++;
            const bool last = pc + 1 == block.endPc;
            std::vector<ValueId> frame;
            if (recordFrames_) frame = snapshot(stacks);
            if (lowerStructural(block, stacks, instr, pc)) {
                if (!block.instrs.empty() && block.instrs.back().pc == pc && block.instrs.back().frame.empty()) block.instrs.back().frame = frame;
                block.termFrame = std::move(frame);
                out_.stats.structured++;
                return;
            }
            Instr ir;
            ir.frame = std::move(frame);
            ir.bc = instr.op;
            ir.a = instr.a == invalidBytecodeIndex ? 0 : instr.a;
            ir.b = instr.b == invalidBytecodeIndex ? 0 : instr.b;
            ir.pc = pc;

            if (instr.op == BytecodeOp::PushConstNumber || instr.op == BytecodeOp::PushConstString || instr.op == BytecodeOp::PushConstBoolean ||
                instr.op == BytecodeOp::PushConstColor || instr.op == BytecodeOp::PushConstValue) {
                ir.op = Op::Const;
                ir.effects = NoEffect;
                const K kind = instr.op == BytecodeOp::PushConstNumber ? K::Number : instr.op == BytecodeOp::PushConstString ? K::String
                                                                                 : instr.op == BytecodeOp::PushConstBoolean  ? K::Boolean
                                                                                 : instr.op == BytecodeOp::PushConstColor    ? K::Color
                                                                                                                             : K::Value;
                const ValueId value = define(ir, kind, resultKind(chunk_, instr));
                stacks.of(kind).push_back(value);
                out_.stats.structured++;
                block.instrs.push_back(std::move(ir));
                if (last) endWithFallthrough(block, stacks);
                continue;
            }

            if (instr.op == BytecodeOp::SetForEachBound) {
                ValueId bound;
                std::vector<ValueId> &values = stacks.of(K::Value);
                if (!pop(stacks, K::Number, bound) || values.size() < 2) {
                    error("stack underflow at instruction " + std::to_string(pc));
                    return;
                }
                ir.args.push_back(bound);
                const ValueId next = define(ir, K::Value, BytecodeValueType::Number);
                values[values.size() - 2] = next;
            } else {
                Effect effect;
                std::string why;
                if (!effectOf(chunk_, instr, effect, why)) {
                    error(why + " at instruction " + std::to_string(pc));
                    return;
                }
                std::vector<ValueId> args(effect.pops.size());
                for (size_t i = effect.pops.size(); i-- > 0;) {
                    if (!pop(stacks, effect.pops[i], args[i])) {
                        error("stack underflow at instruction " + std::to_string(pc));
                        return;
                    }
                }
                ir.args = std::move(args);
                for (const K kind : effect.pushes) {
                    const ValueId value = define(ir, kind, resultKind(chunk_, instr));
                    stacks.of(kind).push_back(value);
                }
            }
            ir.op = classify(chunk_, instr, ir.sub, ir.effects);
            if (ir.op == Op::Bytecode) {
                out_.stats.generic++;
                out_.stats.byBytecode[static_cast<size_t>(instr.op)]++;
            } else {
                out_.stats.structured++;
            }
            block.instrs.push_back(std::move(ir));
            if (last) endWithFallthrough(block, stacks);
        }
    }

    void endWithFallthrough(Block &block, Stacks &stacks) {
        if (block.endPc >= chunk_.instructions.size()) {
            error("execution runs off the end of the chunk at instruction " + std::to_string(block.endPc));
            return;
        }
        block.term = Terminator::Br;
        block.edges[0] = edgeTo(block.endPc, stacks);
    }

    bool lowerStructural(Block &block, Stacks &stacks, const BytecodeInstruction &instr, BytecodeIndex pc) {
        const auto instrFor = [&](Op op) {
            Instr ir;
            ir.op = op;
            ir.effects = op == Op::ForEachStep ? Writes : NoEffect;
            ir.bc = instr.op;
            ir.a = instr.a == invalidBytecodeIndex ? 0 : instr.a;
            ir.b = instr.b == invalidBytecodeIndex ? 0 : instr.b;
            ir.pc = pc;
            return ir;
        };
        switch (instr.op) {
        case BytecodeOp::Jump:
            block.term = Terminator::Br;
            block.edges[0] = edgeTo(instr.a, stacks);
            return true;
        case BytecodeOp::JumpIfFalse:
        case BytecodeOp::JumpIfTrue: {
            ValueId cond;
            if (!pop(stacks, K::Boolean, cond)) {
                error("stack underflow at instruction " + std::to_string(pc));
                return true;
            }
            block.term = Terminator::CondBr;
            block.cond = cond;
            Edge target = edgeTo(instr.a, stacks);
            Edge fall = edgeTo(pc + 1, stacks);
            block.edges[0] = instr.op == BytecodeOp::JumpIfTrue ? std::move(target) : std::move(fall);
            block.edges[1] = instr.op == BytecodeOp::JumpIfTrue ? std::move(fall) : std::move(target);
            return true;
        }
        case BytecodeOp::JumpIfNumCompareFalse:
        case BytecodeOp::JumpIfNumCompareTrue: {
            ValueId y, x;
            if (!pop(stacks, K::Number, y) || !pop(stacks, K::Number, x)) {
                error("stack underflow at instruction " + std::to_string(pc));
                return true;
            }
            Instr ir = instrFor(Op::NumCompare);
            ir.args = {x, y};
            const ValueId cond = define(ir, K::Boolean);
            block.instrs.push_back(std::move(ir));
            block.term = Terminator::CondBr;
            block.cond = cond;
            Edge target = edgeTo(instr.a, stacks);
            Edge fall = edgeTo(pc + 1, stacks);
            block.edges[0] = instr.op == BytecodeOp::JumpIfNumCompareTrue ? std::move(target) : std::move(fall);
            block.edges[1] = instr.op == BytecodeOp::JumpIfNumCompareTrue ? std::move(fall) : std::move(target);
            return true;
        }
        case BytecodeOp::RepeatCheck: {
            ValueId counter;
            if (!pop(stacks, K::Value, counter)) {
                error("stack underflow at instruction " + std::to_string(pc));
                return true;
            }
            Instr ir = instrFor(Op::RepeatStep);
            ir.args = {counter};
            const ValueId next = define(ir, K::Value, BytecodeValueType::Number);
            const ValueId done = define(ir, K::Boolean);
            block.instrs.push_back(std::move(ir));
            block.term = Terminator::CondBr;
            block.cond = done;
            block.edges[0] = edgeTo(instr.a, stacks);
            stacks.of(K::Value).push_back(next);
            block.edges[1] = edgeTo(pc + 1, stacks);
            return true;
        }
        case BytecodeOp::ForEachCheck: {
            std::vector<ValueId> &values = stacks.of(K::Value);
            if (values.size() < 2) {
                error("stack underflow at instruction " + std::to_string(pc));
                return true;
            }
            Instr ir = instrFor(Op::ForEachStep);
            ir.args = {values[values.size() - 2], values.back()};
            const ValueId next = define(ir, K::Value, BytecodeValueType::Number);
            const ValueId done = define(ir, K::Boolean);
            block.instrs.push_back(std::move(ir));
            block.term = Terminator::CondBr;
            block.cond = done;
            const ValueId bound = values[values.size() - 2];
            values.pop_back();
            values.pop_back();
            block.edges[0] = edgeTo(instr.a, stacks);
            values.push_back(bound);
            values.push_back(next);
            block.edges[1] = edgeTo(pc + 1, stacks);
            return true;
        }
        case BytecodeOp::Return: {
            ValueId value;
            if (!pop(stacks, K::Value, value)) {
                error("stack underflow at instruction " + std::to_string(pc));
                return true;
            }
            block.term = Terminator::Ret;
            block.cond = value;
            return true;
        }
        case BytecodeOp::Halt:
            block.term = Terminator::Halt;
            return true;
        case BytecodeOp::EscapeToLegacyThread:
            block.term = Terminator::Escape;
            return true;
        default:
            return false;
        }
    }
};

} // namespace

BuildResult build(const BytecodeChunk &chunk, const BytecodeLinkTable &, const BytecodeAnalysis &analysis, const std::vector<BytecodeIndex> &extraBlockStarts, bool recordFrames) {
    BuildResult result;
    Builder builder(chunk, analysis, result, extraBlockStarts, recordFrames);
    builder.run();
    return result;
}

} // namespace BytecodeJitIR

#endif
