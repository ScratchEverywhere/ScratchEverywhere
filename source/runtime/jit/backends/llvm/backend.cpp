#include "bytecode_jit.hpp"

#ifdef SE_JIT

#include "lir.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <llvm/Config/llvm-config.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicsX86.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/MDBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace BytecodeJitLir {

namespace {

llvm::Function *intrinsicDeclaration(llvm::Module *module, llvm::Intrinsic::ID id) {
#if LLVM_VERSION_MAJOR >= 20
    return llvm::Intrinsic::getOrInsertDeclaration(module, id);
#else
    return llvm::Intrinsic::getDeclaration(module, id);
#endif
}

struct Engine {
    std::unique_ptr<llvm::orc::LLJIT> jit;
    std::unique_ptr<llvm::TargetMachine> machine;
    std::atomic<unsigned> counter{0};
};

Engine *engine() {
    static Engine *instance = []() -> Engine * {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        auto builder = llvm::orc::JITTargetMachineBuilder::detectHost();
        if (!builder) {
            llvm::consumeError(builder.takeError());
            return nullptr;
        }
        builder->setCodeGenOptLevel(llvm::CodeGenOptLevel::Default);
        auto target = builder->createTargetMachine();
        if (!target) {
            llvm::consumeError(target.takeError());
            return nullptr;
        }
        auto jit = llvm::orc::LLJITBuilder().setJITTargetMachineBuilder(std::move(*builder)).create();
        if (!jit) {
            llvm::consumeError(jit.takeError());
            return nullptr;
        }
        auto *e = new Engine;
        e->machine = std::move(*target);
        e->jit = std::move(*jit);
        return e;
    }();
    return instance;
}

class Translator {
  public:
    Translator(Builder &b, llvm::LLVMContext &context, llvm::Module &module) : b_(b), module_(module), ir_(context) {
        i64_ = llvm::Type::getInt64Ty(context);
        i32_ = llvm::Type::getInt32Ty(context);
        i8_ = llvm::Type::getInt8Ty(context);
        i16_ = llvm::Type::getInt16Ty(context);
        f64_ = llvm::Type::getDoubleTy(context);
        ptr_ = llvm::PointerType::getUnqual(context);
    }

    llvm::Function *run(const std::string &name) {
        collectLabels();
        for (const Instr &i : b_.code) {
            if (failed_) return nullptr;
            emit(i, name);
        }
        if (failed_ || function_ == nullptr) return nullptr;
        if (!terminated(ir_.GetInsertBlock())) ir_.CreateUnreachable();
        unsigned unterminated = 0, unplaced = 0;
        for (llvm::BasicBlock &block : *function_) {
            if (terminated(&block)) continue;
            unterminated++;
            if (block.empty()) unplaced++;
            ir_.SetInsertPoint(&block);
            ir_.CreateUnreachable();
        }
        if (std::getenv("SE_JIT_LLVM_LOG") != nullptr) {
            unsigned total = 0, bodyInstructions = 0;
            for (llvm::BasicBlock &block : *function_) {
                if (block.getName() == "body") bodyInstructions = block.size();
                total++;
            }
            std::fprintf(stderr, "[jit llvm] %u blocks, body has %u instructions, %u LIR instructions\n", total, bodyInstructions, static_cast<unsigned>(b_.code.size()));
        }
        if (unterminated != 0 && std::getenv("SE_JIT_LLVM_LOG") != nullptr) std::fprintf(stderr, "[jit llvm] %u blocks without a terminator (%u never placed)\n", unterminated, unplaced);
        return function_;
    }

  private:
    Builder &b_;
    llvm::Module &module_;
    llvm::IRBuilder<> ir_;
    llvm::Type *i64_, *i32_, *i8_, *i16_, *f64_, *ptr_;
    llvm::Function *function_ = nullptr;
    llvm::Value *context_ = nullptr;
    llvm::AllocaInst *gpr_[16] = {};
    llvm::AllocaInst *fpr_[40] = {};
    std::unordered_map<const Label *, llvm::BasicBlock *> blocks_;
    std::unordered_map<const Jump *, const Label *> targets_;
    std::unordered_set<const Label *> cold_;
    llvm::Value *flagZero_ = nullptr, *flagOverflow_ = nullptr, *flagLess_ = nullptr;
    llvm::Value *floatLeft_ = nullptr, *floatRight_ = nullptr;
    unsigned deadBlocks_ = 0;
    bool failed_ = false;

    void fail(const char *why) {
        if (std::getenv("SE_JIT_LLVM_LOG") != nullptr) std::fprintf(stderr, "[jit llvm] cannot translate: %s\n", why);
        failed_ = true;
    }

    void collectLabels() {
        bool cold = false;
        for (const Instr &i : b_.code) {
            if (i.kind == Kind::SetLabel) targets_[i.jump] = i.label;
            if (i.kind == Kind::LabelOp) {
                if (i.label == b_.coldLabel) cold = true;
                if (cold) cold_.insert(i.label);
            }
        }
    }

    llvm::BasicBlock *blockFor(const Label *label) {
        auto found = blocks_.find(label);
        if (found != blocks_.end()) return found->second;
        llvm::BasicBlock *block = llvm::BasicBlock::Create(module_.getContext(), "l" + std::to_string(blocks_.size()), function_);
        blocks_[label] = block;
        return block;
    }

    static bool terminated(const llvm::BasicBlock *block) { return !block->empty() && block->back().isTerminator(); }

    void open() {
        if (!terminated(ir_.GetInsertBlock())) return;
        llvm::BasicBlock *dead = llvm::BasicBlock::Create(module_.getContext(), "dead" + std::to_string(deadBlocks_++), function_);
        ir_.SetInsertPoint(dead);
    }

    static int gprIndex(Opnd o) {
        if (o >= R0 && o <= R5) return o - R0;
        if (o >= S0 && o <= S7) return 6 + (o - S0);
        return -1;
    }
    static bool isMemory(Opnd o) { return (o & (MEM_FLAG | MEM2_FLAG)) != 0; }
    static bool isImmediate(Opnd o) { return (o & IMM) != 0; }
    static bool isFloatRegister(Opnd o) { return o >= FR0 && o < FR0 + 40; }

    llvm::Value *constant64(Word w) { return llvm::ConstantInt::get(i64_, static_cast<uint64_t>(static_cast<int64_t>(w))); }

    llvm::Value *readRegister(Opnd o) {
        if (o == S0) return ir_.CreatePtrToInt(context_, i64_);
        const int index = gprIndex(o);
        if (index < 0 || gpr_[index] == nullptr) {
            fail("bad register");
            return constant64(0);
        }
        return ir_.CreateLoad(i64_, gpr_[index]);
    }

    void writeRegister(Opnd o, llvm::Value *value) {
        const int index = gprIndex(o);
        if (index < 0 || o == S0 || gpr_[index] == nullptr) {
            fail("bad destination register");
            return;
        }
        ir_.CreateStore(value, gpr_[index]);
    }

    llvm::Value *address(Opnd o, Word w) {
        if ((o & MEM2_FLAG) != 0) {
            llvm::Value *base = readRegister(memBase(o));
            llvm::Value *index = readRegister(memIndex(o));
            llvm::Value *sum = ir_.CreateAdd(base, ir_.CreateShl(index, constant64(w)));
            return ir_.CreateIntToPtr(sum, ptr_);
        }
        const Opnd base = memBase(o);
        if (base == 0) return ir_.CreateIntToPtr(constant64(w), ptr_);
        if (base == S0) return ir_.CreateGEP(i8_, context_, {constant64(w)});
        return ir_.CreateIntToPtr(ir_.CreateAdd(readRegister(base), constant64(w)), ptr_);
    }

    llvm::Type *typeForMove(Op op) {
        switch (baseOp(op)) {
        case MOV_U8:
            return i8_;
        case MOV_U16:
            return i16_;
        case MOV_U32:
        case MOV_S32:
        case MOV32:
            return i32_;
        default:
            return i64_;
        }
    }

    llvm::Value *extendMove(Op op, llvm::Value *narrow) {
        if (narrow->getType() == i64_) return narrow;
        return baseOp(op) == MOV_S32 ? ir_.CreateSExt(narrow, i64_) : ir_.CreateZExt(narrow, i64_);
    }

    llvm::Value *readMove(Op op, Opnd o, Word w) {
        if (isImmediate(o)) return normalizeMove(op, constant64(w));
        if (isMemory(o)) return extendMove(op, ir_.CreateLoad(typeForMove(op), address(o, w)));
        return normalizeMove(op, readRegister(o));
    }

    llvm::Value *normalizeMove(Op op, llvm::Value *v64) {
        llvm::Type *type = typeForMove(op);
        if (type == i64_) return v64;
        return extendMove(op, ir_.CreateTrunc(v64, type));
    }

    void writeMove(Op op, Opnd o, Word w, llvm::Value *v64) {
        if (isMemory(o)) {
            llvm::Type *type = typeForMove(op);
            ir_.CreateStore(type == i64_ ? v64 : ir_.CreateTrunc(v64, type), address(o, w));
            return;
        }
        writeRegister(o, v64);
    }

    llvm::Value *readInt(Opnd o, Word w, bool narrow) {
        llvm::Value *value;
        if (isImmediate(o)) value = constant64(w);
        else if (isMemory(o)) value = ir_.CreateLoad(narrow ? i32_ : i64_, address(o, w));
        else value = readRegister(o);
        if (narrow && value->getType() == i64_) value = ir_.CreateTrunc(value, i32_);
        return value;
    }

    llvm::Value *readFloat(Opnd o, Word w) {
        if (isFloatRegister(o)) {
            llvm::AllocaInst *slot = fpr_[o - FR0];
            if (slot == nullptr) {
                fail("bad float register");
                return llvm::ConstantFP::get(f64_, 0.0);
            }
            return ir_.CreateLoad(f64_, slot);
        }
        if (isMemory(o)) return ir_.CreateLoad(f64_, address(o, w));
        fail("bad float operand");
        return llvm::ConstantFP::get(f64_, 0.0);
    }

    void writeFloat(Opnd o, Word w, llvm::Value *value) {
        if (isFloatRegister(o)) {
            if (fpr_[o - FR0] == nullptr) {
                fail("bad float destination");
                return;
            }
            ir_.CreateStore(value, fpr_[o - FR0]);
            return;
        }
        if (isMemory(o)) {
            ir_.CreateStore(value, address(o, w));
            return;
        }
        fail("bad float destination");
    }

    llvm::Value *vector2(llvm::Value *scalar) {
        llvm::Type *vec = llvm::FixedVectorType::get(f64_, 2);
        return ir_.CreateInsertElement(llvm::PoisonValue::get(vec), scalar, ir_.getInt32(0));
    }

    llvm::Value *integerCompare(Cond type, llvm::Value *a, llvm::Value *b) {
        switch (type & 0xff) {
        case EQUAL:
            return ir_.CreateICmpEQ(a, b);
        case NOT_EQUAL:
        case NOT_ZERO:
            return ir_.CreateICmpNE(a, b);
        case LESS_EQUAL:
            return ir_.CreateICmpULE(a, b);
        case GREATER:
            return ir_.CreateICmpUGT(a, b);
        case GREATER_EQUAL:
            return ir_.CreateICmpUGE(a, b);
        case SIG_LESS:
            return ir_.CreateICmpSLT(a, b);
        case SIG_LESS_EQUAL:
            return ir_.CreateICmpSLE(a, b);
        case SIG_GREATER:
            return ir_.CreateICmpSGT(a, b);
        case SIG_GREATER_EQUAL:
            return ir_.CreateICmpSGE(a, b);
        default:
            fail("unsupported integer condition");
            return ir_.getFalse();
        }
    }

    llvm::Value *floatCompare(Cond type, llvm::Value *a, llvm::Value *b) {
        switch (type & 0xff) {
        case UNORDERED:
            return ir_.CreateFCmpUNO(a, b);
        case ORDERED_EQUAL:
        case F_EQUAL:
            return ir_.CreateFCmpOEQ(a, b);
        case ORDERED_NOT_EQUAL:
            return ir_.CreateFCmpONE(a, b);
        case ORDERED_LESS:
        case F_LESS:
            return ir_.CreateFCmpOLT(a, b);
        case ORDERED_GREATER:
        case F_GREATER:
            return ir_.CreateFCmpOGT(a, b);
        case ORDERED_LESS_EQUAL:
            return ir_.CreateFCmpOLE(a, b);
        case ORDERED_GREATER_EQUAL:
            return ir_.CreateFCmpOGE(a, b);
        case UNORDERED_OR_NOT_EQUAL:
            return ir_.CreateFCmpUNE(a, b);
        case UNORDERED_OR_GREATER_EQUAL:
            return ir_.CreateFCmpUGE(a, b);
        default:
            fail("unsupported float condition");
            return ir_.getFalse();
        }
    }

    llvm::Value *flagCondition(Cond type) {
        switch (type & 0xff) {
        case EQUAL:
            if (flagZero_ == nullptr) break;
            return flagZero_;
        case NOT_EQUAL:
        case NOT_ZERO:
            if (flagZero_ == nullptr) break;
            return ir_.CreateNot(flagZero_);
        case OVERFLOW_SET:
            if (flagOverflow_ == nullptr) break;
            return flagOverflow_;
        case SIG_LESS:
            if (flagLess_ == nullptr) break;
            return flagLess_;
        case SIG_GREATER_EQUAL:
            if (flagLess_ == nullptr) break;
            return ir_.CreateNot(flagLess_);
        case F_EQUAL:
        case F_LESS:
        case F_GREATER:
            if (floatLeft_ == nullptr) break;
            return floatCompare(type, floatLeft_, floatRight_);
        default:
            break;
        }
        fail("condition without matching flags");
        return ir_.getFalse();
    }

    void conditionalJump(llvm::Value *condition, const Jump *jump) {
        const auto target = targets_.find(jump);
        if (target == targets_.end()) {
            fail("jump without a label");
            return;
        }
        llvm::BasicBlock *next = llvm::BasicBlock::Create(module_.getContext(), "c" + std::to_string(deadBlocks_++), function_);
        llvm::MDNode *weights = nullptr;
        if (cold_.count(target->second) != 0) weights = llvm::MDBuilder(module_.getContext()).createBranchWeights(1, 4000);
        ir_.CreateCondBr(condition, blockFor(target->second), next, weights);
        ir_.SetInsertPoint(next);
    }

    llvm::Value *arithmetic(Op op, llvm::Value *a, llvm::Value *b, bool narrow, bool wantOverflow, llvm::Value **overflow) {
        switch (baseOp(op)) {
        case ADD:
            if (wantOverflow) {
                llvm::Value *pair = ir_.CreateBinaryIntrinsic(llvm::Intrinsic::sadd_with_overflow, a, b);
                *overflow = ir_.CreateExtractValue(pair, 1);
                return ir_.CreateExtractValue(pair, 0);
            }
            return ir_.CreateAdd(a, b);
        case SUB:
            if (wantOverflow) {
                llvm::Value *pair = ir_.CreateBinaryIntrinsic(llvm::Intrinsic::ssub_with_overflow, a, b);
                *overflow = ir_.CreateExtractValue(pair, 1);
                return ir_.CreateExtractValue(pair, 0);
            }
            return ir_.CreateSub(a, b);
        case MUL:
            return ir_.CreateMul(a, b);
        case AND:
            return ir_.CreateAnd(a, b);
        case OR:
            return ir_.CreateOr(a, b);
        case XOR:
            return ir_.CreateXor(a, b);
        case SHL:
        case LSHR:
        case ASHR: {
            const unsigned bits = narrow ? 32 : 64;
            llvm::Value *amount = ir_.CreateAnd(b, llvm::ConstantInt::get(b->getType(), bits - 1));
            if (baseOp(op) == SHL) return ir_.CreateShl(a, amount);
            if (baseOp(op) == LSHR) return ir_.CreateLShr(a, amount);
            return ir_.CreateAShr(a, amount);
        }
        default:
            fail("unsupported integer operation");
            return a;
        }
    }

    void emitOp2(const Instr &i, bool store) {
        const bool narrow = (i.op & OP32) != 0;
        llvm::Value *a = readInt(i.src1, i.src1w, narrow);
        llvm::Value *b = readInt(i.src2, i.src2w, narrow);
        llvm::Value *overflow = nullptr;
        const bool wantOverflow = (i.op & SET_OVERFLOW) != 0;
        llvm::Value *result = arithmetic(i.op, a, b, narrow, wantOverflow, &overflow);
        if ((i.op & SET_Z) != 0) flagZero_ = ir_.CreateICmpEQ(result, llvm::ConstantInt::get(result->getType(), 0));
        if (wantOverflow) flagOverflow_ = overflow;
        if ((i.op & SET_SIG_LESS) != 0) flagLess_ = ir_.CreateICmpSLT(a, b);
        if (!store) return;
        if (isMemory(i.dst)) {
            ir_.CreateStore(result, address(i.dst, i.dstw));
            return;
        }
        writeRegister(i.dst, narrow ? ir_.CreateZExt(result, i64_) : result);
    }

    llvm::FunctionType *functionType(Sig sig) {
        std::vector<llvm::Type *> arguments;
        for (int index = 0; index < 4 && sigArgument(sig, index) != ArgType::Void; index++)
            arguments.push_back(typeFor(sigArgument(sig, index), true));
        return llvm::FunctionType::get(typeFor(sigReturn(sig), false), arguments, false);
    }

    llvm::Type *typeFor(ArgType type, bool argument) {
        switch (type) {
        case ArgType::W:
            return i64_;
        case ArgType::P:
            return argument ? ptr_ : i64_;
        case ArgType::F64:
            return f64_;
        case ArgType::I32:
            return i32_;
        default:
            return llvm::Type::getVoidTy(module_.getContext());
        }
    }

    void emitCall(const Instr &i) {
        llvm::Value *callee;
        if (isImmediate(i.src1)) callee = ir_.CreateIntToPtr(constant64(i.src1w), ptr_);
        else callee = ir_.CreateIntToPtr(readInt(i.src1, i.src1w, false), ptr_);
        std::vector<llvm::Value *> arguments;
        int integers = 0, floats = 0;
        for (int index = 0; index < 4 && sigArgument(i.sig, index) != ArgType::Void; index++) {
            const ArgType type = sigArgument(i.sig, index);
            if (type == ArgType::F64) {
                arguments.push_back(readFloat(FR(floats++), 0));
            } else {
                llvm::Value *value = readRegister(R0 + integers++);
                if (type == ArgType::P) value = ir_.CreateIntToPtr(value, ptr_);
                else if (type == ArgType::I32) value = ir_.CreateTrunc(value, i32_);
                arguments.push_back(value);
            }
        }
        llvm::Value *result = ir_.CreateCall(functionType(i.sig), callee, arguments);
        switch (sigReturn(i.sig)) {
        case ArgType::W:
        case ArgType::P:
            writeRegister(R0, result);
            break;
        case ArgType::I32:
            writeRegister(R0, ir_.CreateZExt(result, i64_));
            break;
        case ArgType::F64:
            writeFloat(FR0, 0, result);
            break;
        default:
            break;
        }
    }

    void emitEnter(const Instr &i, const std::string &name) {
        llvm::FunctionType *type = llvm::FunctionType::get(i64_, {ptr_, i64_}, false);
        function_ = llvm::Function::Create(type, llvm::Function::ExternalLinkage, name, module_);
        llvm::BasicBlock *entry = llvm::BasicBlock::Create(module_.getContext(), "entry", function_);
        ir_.SetInsertPoint(entry);
        context_ = function_->getArg(0);
        if (std::getenv("SE_JIT_LLVM_NOALIAS") != nullptr) {
            function_->addParamAttr(0, llvm::Attribute::NoAlias);
            function_->addParamAttr(0, llvm::Attribute::NonNull);
        }
        for (int index = 0; index < 13; index++) {
            if (index == 6) continue;
            gpr_[index] = ir_.CreateAlloca(i64_);
            ir_.CreateStore(constant64(0), gpr_[index]);
        }
        const int floatCount = i.counts[2] < 40 ? i.counts[2] : 40;
        for (int index = 0; index < floatCount; index++) {
            fpr_[index] = ir_.CreateAlloca(f64_);
            ir_.CreateStore(llvm::ConstantFP::get(f64_, 0.0), fpr_[index]);
        }
        ir_.CreateStore(function_->getArg(1), gpr_[gprIndex(S1)]);
        llvm::BasicBlock *body = llvm::BasicBlock::Create(module_.getContext(), "body", function_);
        ir_.CreateBr(body);
        ir_.SetInsertPoint(body);
    }

    void emit(const Instr &i, const std::string &name) {
        if (i.kind == Kind::Enter) {
            emitEnter(i, name);
            return;
        }
        if (function_ == nullptr) {
            fail("code before enter");
            return;
        }
        if (i.kind == Kind::SetLabel) return;
        if (i.kind == Kind::LabelOp) {
            llvm::BasicBlock *block = blockFor(i.label);
            if (!terminated(ir_.GetInsertBlock())) ir_.CreateBr(block);
            ir_.SetInsertPoint(block);
            return;
        }
        open();
        switch (i.kind) {
        case Kind::Return:
            ir_.CreateRet(readMove(i.op, i.src1, i.src1w));
            break;
        case Kind::Op0:
            if (baseOp(i.op) == DIVMOD_S32) {
                llvm::Value *a = ir_.CreateTrunc(readRegister(R0), i32_);
                llvm::Value *b = ir_.CreateTrunc(readRegister(R1), i32_);
                writeRegister(R0, ir_.CreateZExt(ir_.CreateSDiv(a, b), i64_));
                writeRegister(R1, ir_.CreateZExt(ir_.CreateSRem(a, b), i64_));
            } else if (baseOp(i.op) == LMUL_UW) {
                llvm::Type *wide = llvm::Type::getInt128Ty(module_.getContext());
                llvm::Value *product = ir_.CreateMul(ir_.CreateZExt(readRegister(R0), wide), ir_.CreateZExt(readRegister(R1), wide));
                writeRegister(R0, ir_.CreateTrunc(product, i64_));
                writeRegister(R1, ir_.CreateTrunc(ir_.CreateLShr(product, llvm::ConstantInt::get(wide, 64)), i64_));
            } else {
                fail("unsupported op0");
            }
            break;
        case Kind::Op1:
            writeMove(i.op, i.dst, i.dstw, readMove(i.op, i.src1, i.src1w));
            break;
        case Kind::Op2:
            emitOp2(i, true);
            break;
        case Kind::Op2U:
            emitOp2(i, false);
            break;
        case Kind::OpFlags: {
            llvm::Value *condition = ir_.CreateZExt(flagCondition(i.counts[0]), i64_);
            writeMove(i.op, i.dst, i.dstw, condition);
            break;
        }
        case Kind::FOp1:
            emitFloat1(i);
            break;
        case Kind::FOp2: {
            llvm::Value *a = readFloat(i.src1, i.src1w);
            llvm::Value *b = readFloat(i.src2, i.src2w);
            llvm::Value *result = nullptr;
            switch (baseOp(i.op)) {
            case ADD_F64:
                result = ir_.CreateFAdd(a, b);
                break;
            case SUB_F64:
                result = ir_.CreateFSub(a, b);
                break;
            case MUL_F64:
                result = ir_.CreateFMul(a, b);
                break;
            case DIV_F64:
                result = ir_.CreateFDiv(a, b);
                break;
            default:
                fail("unsupported float operation");
                return;
            }
            writeFloat(i.dst, i.dstw, result);
            break;
        }
        case Kind::FSet64:
            writeFloat(i.dst, 0, llvm::ConstantFP::get(f64_, i.value));
            break;
        case Kind::IJump:
            emitIndirectJump(i);
            break;
        case Kind::ICall:
            emitCall(i);
            break;
        case Kind::JumpOp: {
            const Cond type = i.op & 0xff;
            if (type == JUMP) {
                const auto target = targets_.find(i.jump);
                if (target == targets_.end()) {
                    fail("jump without a label");
                    return;
                }
                ir_.CreateBr(blockFor(target->second));
            } else {
                conditionalJump(flagCondition(i.op), i.jump);
            }
            break;
        }
        case Kind::Cmp: {
            const bool narrow = (i.op & OP32) != 0;
            conditionalJump(integerCompare(i.op, readInt(i.src1, i.src1w, narrow), readInt(i.src2, i.src2w, narrow)), i.jump);
            break;
        }
        case Kind::FCmp:
            conditionalJump(floatCompare(i.op, readFloat(i.src1, i.src1w), readFloat(i.src2, i.src2w)), i.jump);
            break;
        default:
            fail("unknown instruction");
            break;
        }
    }

    void emitFloat1(const Instr &i) {
        switch (baseOp(i.op)) {
        case MOV_F64:
            writeFloat(i.dst, i.dstw, readFloat(i.src1, i.src1w));
            break;
        case ABS_F64:
            writeFloat(i.dst, i.dstw, ir_.CreateUnaryIntrinsic(llvm::Intrinsic::fabs, readFloat(i.src1, i.src1w)));
            break;
        case CMP_F64:
            floatLeft_ = readFloat(i.dst, i.dstw);
            floatRight_ = readFloat(i.src1, i.src1w);
            break;
        case CONV_F64_FROM_SW:
            writeFloat(i.dst, i.dstw, ir_.CreateSIToFP(readInt(i.src1, i.src1w, false), f64_));
            break;
        case CONV_F64_FROM_S32:
            writeFloat(i.dst, i.dstw, ir_.CreateSIToFP(readInt(i.src1, i.src1w, true), f64_));
            break;
        case CONV_SW_FROM_F64: {
            llvm::Function *convert = intrinsicDeclaration(&module_, llvm::Intrinsic::x86_sse2_cvttsd2si64);
            llvm::Value *value = ir_.CreateCall(convert, {vector2(readFloat(i.src1, i.src1w))});
            storeIntResult(i, value);
            break;
        }
        case CONV_S32_FROM_F64: {
            llvm::Function *convert = intrinsicDeclaration(&module_, llvm::Intrinsic::x86_sse2_cvttsd2si);
            llvm::Value *value = ir_.CreateZExt(ir_.CreateCall(convert, {vector2(readFloat(i.src1, i.src1w))}), i64_);
            storeIntResult(i, value);
            break;
        }
        default:
            fail("unsupported float conversion");
            break;
        }
    }

    void storeIntResult(const Instr &i, llvm::Value *value) {
        if (isMemory(i.dst)) {
            ir_.CreateStore(value, address(i.dst, i.dstw));
            return;
        }
        writeRegister(i.dst, value);
    }

    void emitIndirectJump(const Instr &i) {
        if ((i.src1 & MEM2_FLAG) == 0 || b_.entryLabels.empty()) {
            fail("indirect jump that is not an entry dispatch");
            return;
        }
        llvm::Value *index = readRegister(memIndex(i.src1));
        llvm::BasicBlock *invalid = llvm::BasicBlock::Create(module_.getContext(), "badentry" + std::to_string(deadBlocks_++), function_);
        llvm::SwitchInst *dispatch = ir_.CreateSwitch(index, invalid, static_cast<unsigned>(b_.entryLabels.size()));
        for (size_t entry = 0; entry < b_.entryLabels.size(); entry++) {
            if (b_.entryLabels[entry] == nullptr) continue;
            dispatch->addCase(llvm::ConstantInt::get(llvm::cast<llvm::IntegerType>(i64_), entry), blockFor(b_.entryLabels[entry]));
        }
        ir_.SetInsertPoint(invalid);
        ir_.CreateUnreachable();
    }
};

} // namespace

void *llvmGenerate(Builder &b) {
    Engine *e = engine();
    if (e == nullptr) return nullptr;
    static const size_t maxInstructions = [] {
        const char *value = std::getenv("SE_JIT_LLVM_MAX");
        return value != nullptr ? static_cast<size_t>(std::atol(value)) : static_cast<size_t>(15000);
    }();
    if (b.code.size() > maxInstructions) {
        if (std::getenv("SE_JIT_LLVM_LOG") != nullptr) std::fprintf(stderr, "[jit llvm] %zu LIR instructions exceed the limit of %zu\n", b.code.size(), maxInstructions);
        return nullptr;
    }
    auto context = std::make_unique<llvm::LLVMContext>();
    auto module = std::make_unique<llvm::Module>("se_jit", *context);
    module->setDataLayout(e->machine->createDataLayout());
#if LLVM_VERSION_MAJOR >= 21
    module->setTargetTriple(e->machine->getTargetTriple());
#else
    module->setTargetTriple(e->machine->getTargetTriple().getTriple());
#endif
    const std::string name = "se_jit_" + std::to_string(e->counter++);
    {
        Translator translator(b, *context, *module);
        if (translator.run(name) == nullptr) return nullptr;
    }
    if (std::getenv("SE_JIT_LLVM_RAW") != nullptr) module->print(llvm::errs(), nullptr);
    if (std::getenv("SE_JIT_LLVM_LOG") != nullptr) {
        std::string errors;
        llvm::raw_string_ostream stream(errors);
        if (llvm::verifyModule(*module, &stream)) {
            std::fprintf(stderr, "[jit llvm] verifier: %s\n", errors.c_str());
            return nullptr;
        }
    }
    if (const char *dump = std::getenv("SE_JIT_LLVM_IR"); dump != nullptr) module->print(llvm::errs(), nullptr);

    int level = 2;
    if (const char *value = std::getenv("SE_JIT_LLVM_OPT"); value != nullptr) level = std::atoi(value);
    if (level > 0) {
        llvm::PassBuilder passes(e->machine.get());
        llvm::LoopAnalysisManager loops;
        llvm::FunctionAnalysisManager functions;
        llvm::CGSCCAnalysisManager components;
        llvm::ModuleAnalysisManager modules;
        passes.registerModuleAnalyses(modules);
        passes.registerCGSCCAnalyses(components);
        passes.registerFunctionAnalyses(functions);
        passes.registerLoopAnalyses(loops);
        passes.crossRegisterProxies(loops, functions, components, modules);
        llvm::OptimizationLevel optimization = level == 1 ? llvm::OptimizationLevel::O1 : level == 3 ? llvm::OptimizationLevel::O3
                                                                                                     : llvm::OptimizationLevel::O2;
        llvm::ModulePassManager pipeline = passes.buildPerModuleDefaultPipeline(optimization);
        const auto started = std::chrono::steady_clock::now();
        pipeline.run(*module, modules);
        if (std::getenv("SE_JIT_LLVM_LOG") != nullptr) std::fprintf(stderr, "[jit llvm] optimized in %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
        if (std::getenv("SE_JIT_LLVM_IR") != nullptr) module->print(llvm::errs(), nullptr);
    }

    if (auto error = e->jit->addIRModule(llvm::orc::ThreadSafeModule(std::move(module), std::move(context)))) {
        llvm::consumeError(std::move(error));
        return nullptr;
    }
    const auto lookupStarted = std::chrono::steady_clock::now();
    auto symbol = e->jit->lookup(name);
    if (std::getenv("SE_JIT_LLVM_LOG") != nullptr) std::fprintf(stderr, "[jit llvm] code generation %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lookupStarted).count());
    if (!symbol) {
        llvm::consumeError(symbol.takeError());
        return nullptr;
    }
    return symbol->toPtr<void *>();
}

void llvmRelease(Builder &) {}

void llvmFreeMachine(void *) {}

bool llvmUsable() { return engine() != nullptr; }

const BackendOps llvmOps = {"llvm", &llvmUsable, &llvmGenerate, &llvmRelease, &llvmFreeMachine, nullptr, false};

} // namespace BytecodeJitLir

#endif
