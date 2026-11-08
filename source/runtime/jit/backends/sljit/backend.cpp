#include "lir.hpp"

#ifdef SE_JIT

#include <sljitLir.h>

namespace BytecodeJitInternal {
bool sljitProbe();
}

namespace BytecodeJitLir {

namespace {

sljit_s32 reg(Opnd o) {
    if (o >= FR0) return SLJIT_FR(o - FR0);
    if (o >= S0) return SLJIT_S(o - S0);
    return SLJIT_R(o - R0);
}

sljit_s32 operand(Opnd o) {
    if (o == NONE) return 0;
    if ((o & IMM) != 0) return SLJIT_IMM;
    if ((o & MEM2_FLAG) != 0) return SLJIT_MEM2(reg(memBase(o)), reg(memIndex(o)));
    if ((o & MEM_FLAG) != 0) return memBase(o) == 0 ? SLJIT_MEM0() : SLJIT_MEM1(reg(memBase(o)));
    return reg(o);
}

sljit_s32 baseOperation(Op op) {
    switch (baseOp(op)) {
    case MOV:
        return SLJIT_MOV;
    case MOV_U8:
        return SLJIT_MOV_U8;
    case MOV_U16:
        return SLJIT_MOV_U16;
    case MOV_U32:
        return SLJIT_MOV_U32;
    case MOV_S32:
        return SLJIT_MOV_S32;
    case MOV32:
        return SLJIT_MOV32;
    case ADD:
        return SLJIT_ADD;
    case SUB:
        return SLJIT_SUB;
    case MUL:
        return SLJIT_MUL;
    case AND:
        return SLJIT_AND;
    case OR:
        return SLJIT_OR;
    case XOR:
        return SLJIT_XOR;
    case SHL:
        return SLJIT_SHL;
    case LSHR:
        return SLJIT_LSHR;
    case ASHR:
        return SLJIT_ASHR;
    case LMUL_UW:
        return SLJIT_LMUL_UW;
    case DIVMOD_S32:
        return SLJIT_DIVMOD_S32;
    case MOV_F64:
        return SLJIT_MOV_F64;
    case ADD_F64:
        return SLJIT_ADD_F64;
    case SUB_F64:
        return SLJIT_SUB_F64;
    case MUL_F64:
        return SLJIT_MUL_F64;
    case DIV_F64:
        return SLJIT_DIV_F64;
    case ABS_F64:
        return SLJIT_ABS_F64;
    case CMP_F64:
        return SLJIT_CMP_F64;
    case CONV_F64_FROM_SW:
        return SLJIT_CONV_F64_FROM_SW;
    case CONV_F64_FROM_S32:
        return SLJIT_CONV_F64_FROM_S32;
    case CONV_SW_FROM_F64:
        return SLJIT_CONV_SW_FROM_F64;
    default:
        return SLJIT_CONV_S32_FROM_F64;
    }
}

sljit_s32 operation(Op op) {
    sljit_s32 out = baseOperation(op);
    if ((op & OP32) != 0) out |= SLJIT_32;
    if ((op & SET_Z) != 0) out |= SLJIT_SET_Z;
    if ((op & SET_OVERFLOW) != 0) out |= SLJIT_SET_OVERFLOW;
    if ((op & SET_SIG_LESS) != 0) out |= SLJIT_SET_SIG_LESS;
    if ((op & SET_F_LESS) != 0) out |= SLJIT_SET_F_LESS;
    if ((op & SET_F_GREATER) != 0) out |= SLJIT_SET_F_GREATER;
    if ((op & SET_F_EQUAL) != 0) out |= SLJIT_SET_F_EQUAL;
    return out;
}

sljit_s32 condition(Cond c) {
    if ((c & OP32) != 0) return condition(c & ~OP32) | SLJIT_32;
    switch (c) {
    case JUMP:
        return SLJIT_JUMP;
    case CALL:
        return SLJIT_CALL;
    case EQUAL:
        return SLJIT_EQUAL;
    case NOT_EQUAL:
        return SLJIT_NOT_EQUAL;
    case NOT_ZERO:
        return SLJIT_NOT_ZERO;
    case LESS_EQUAL:
        return SLJIT_LESS_EQUAL;
    case GREATER:
        return SLJIT_GREATER;
    case GREATER_EQUAL:
        return SLJIT_GREATER_EQUAL;
    case SIG_LESS:
        return SLJIT_SIG_LESS;
    case SIG_LESS_EQUAL:
        return SLJIT_SIG_LESS_EQUAL;
    case SIG_GREATER:
        return SLJIT_SIG_GREATER;
    case SIG_GREATER_EQUAL:
        return SLJIT_SIG_GREATER_EQUAL;
    case OVERFLOW_SET:
        return SLJIT_OVERFLOW;
    case UNORDERED:
        return SLJIT_UNORDERED;
    case ORDERED_EQUAL:
        return SLJIT_ORDERED_EQUAL;
    case ORDERED_NOT_EQUAL:
        return SLJIT_ORDERED_NOT_EQUAL;
    case ORDERED_LESS:
        return SLJIT_ORDERED_LESS;
    case ORDERED_GREATER:
        return SLJIT_ORDERED_GREATER;
    case ORDERED_LESS_EQUAL:
        return SLJIT_ORDERED_LESS_EQUAL;
    case ORDERED_GREATER_EQUAL:
        return SLJIT_ORDERED_GREATER_EQUAL;
    case UNORDERED_OR_NOT_EQUAL:
        return SLJIT_UNORDERED_OR_NOT_EQUAL;
    case UNORDERED_OR_GREATER_EQUAL:
        return SLJIT_UNORDERED_OR_GREATER_EQUAL;
    case F_EQUAL:
        return SLJIT_F_EQUAL;
    case F_LESS:
        return SLJIT_F_LESS;
    default:
        return SLJIT_F_GREATER;
    }
}

sljit_s32 argumentType(ArgType t) {
    switch (t) {
    case ArgType::W:
        return SLJIT_ARG_TYPE_W;
    case ArgType::P:
        return SLJIT_ARG_TYPE_P;
    case ArgType::F64:
        return SLJIT_ARG_TYPE_F64;
    default:
        return SLJIT_ARG_TYPE_32;
    }
}

sljit_s32 signature(Sig s) {
    sljit_s32 out = sigReturn(s) == ArgType::Void ? SLJIT_ARG_RETURN(SLJIT_ARG_TYPE_RET_VOID) : SLJIT_ARG_RETURN(argumentType(sigReturn(s)));
    for (int i = 0; i < 4 && sigArgument(s, i) != ArgType::Void; i++)
        out |= SLJIT_ARG_VALUE(argumentType(sigArgument(s, i)), i + 1);
    return out;
}

} // namespace

void *sljitGenerate(Builder &b) {
    sljit_compiler *c = sljit_create_compiler(nullptr);
    if (c == nullptr) return nullptr;
    b.backend = c;
    for (const Instr &i : b.code) {
        switch (i.kind) {
        case Kind::Enter:
            sljit_emit_enter(c, 0, signature(i.sig), i.counts[0] | SLJIT_ENTER_FLOAT(i.counts[2]), i.counts[1], i.counts[3]);
            break;
        case Kind::Return:
            sljit_emit_return(c, operation(i.op), operand(i.src1), i.src1w);
            break;
        case Kind::Op0:
            sljit_emit_op0(c, operation(i.op));
            break;
        case Kind::Op1:
            sljit_emit_op1(c, operation(i.op), operand(i.dst), i.dstw, operand(i.src1), i.src1w);
            break;
        case Kind::Op2:
            sljit_emit_op2(c, operation(i.op), operand(i.dst), i.dstw, operand(i.src1), i.src1w, operand(i.src2), i.src2w);
            break;
        case Kind::Op2U:
            sljit_emit_op2u(c, operation(i.op), operand(i.src1), i.src1w, operand(i.src2), i.src2w);
            break;
        case Kind::OpFlags:
            sljit_emit_op_flags(c, operation(i.op), operand(i.dst), i.dstw, condition(i.counts[0]));
            break;
        case Kind::FOp1:
            sljit_emit_fop1(c, operation(i.op), operand(i.dst), i.dstw, operand(i.src1), i.src1w);
            break;
        case Kind::FOp2:
            sljit_emit_fop2(c, operation(i.op), operand(i.dst), i.dstw, operand(i.src1), i.src1w, operand(i.src2), i.src2w);
            break;
        case Kind::FSet64:
            sljit_emit_fset64(c, operand(i.dst), i.value);
            break;
        case Kind::IJump:
            sljit_emit_ijump(c, condition(i.op), operand(i.src1), i.src1w);
            break;
        case Kind::ICall:
            sljit_emit_icall(c, condition(i.op), signature(i.sig), operand(i.src1), i.src1w);
            break;
        case Kind::JumpOp:
            i.jump->real = sljit_emit_jump(c, condition(i.op));
            break;
        case Kind::Cmp:
            i.jump->real = sljit_emit_cmp(c, condition(i.op), operand(i.src1), i.src1w, operand(i.src2), i.src2w);
            break;
        case Kind::FCmp:
            i.jump->real = sljit_emit_fcmp(c, condition(i.op), operand(i.src1), i.src1w, operand(i.src2), i.src2w);
            break;
        case Kind::LabelOp:
            i.label->real = sljit_emit_label(c);
            break;
        case Kind::SetLabel:
            sljit_set_label(static_cast<sljit_jump *>(i.jump->real), static_cast<sljit_label *>(i.label->real));
            break;
        }
    }
    void *machine = sljit_generate_code(c, 0, nullptr);
    if (machine != nullptr) b.machineSize = sljit_get_generated_code_size(c);
    return machine;
}

void sljitFreeMachine(void *machine) { sljit_free_code(machine, nullptr); }

void sljitRelease(Builder &b) {
    if (b.backend != nullptr) sljit_free_compiler(static_cast<sljit_compiler *>(b.backend));
    b.backend = nullptr;
}

UWord sljitLabelAddress(Label *label) { return sljit_get_label_addr(static_cast<sljit_label *>(label->real)); }

bool sljitUsable() { return BytecodeJitInternal::sljitProbe(); }

const BackendOps sljitOps = {"sljit", &sljitUsable, &sljitGenerate, &sljitRelease, &sljitFreeMachine, &sljitLabelAddress, true};

} // namespace BytecodeJitLir

#endif
