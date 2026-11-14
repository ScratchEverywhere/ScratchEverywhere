#include "lir.hpp"

#ifdef SE_JIT

#include <cstdlib>
#include <string>
#include <unordered_map>

namespace BytecodeJitLir {

namespace {

constexpr Opnd contextSlot = MEM_FLAG | S0;

bool isSlot(Opnd o) { return o == contextSlot; }
bool isFloatRegister(Opnd o) { return o >= FR0 && o < FR0 + 40; }

class SlotForwarder {
  public:
    void run(Builder &b) {
        std::vector<Instr> out;
        out.reserve(b.code.size());
        for (Instr &i : b.code) {
            if (visit(i)) out.push_back(i);
        }
        b.code.swap(out);
    }

  private:
    std::unordered_map<Word, Opnd> holder_;

    void forgetRegister(Opnd reg) {
        for (auto it = holder_.begin(); it != holder_.end();) {
            if (it->second == reg) it = holder_.erase(it);
            else ++it;
        }
    }

    void forgetOverlap(Word offset) {
        for (auto it = holder_.begin(); it != holder_.end();) {
            const Word distance = it->first > offset ? it->first - offset : offset - it->first;
            if (distance < 8) it = holder_.erase(it);
            else ++it;
        }
    }

    void forward(Opnd &operand, Word &offset) {
        if (!isSlot(operand)) return;
        const auto found = holder_.find(offset);
        if (found == holder_.end()) return;
        operand = found->second;
        offset = 0;
    }

    bool visit(Instr &i) {
        switch (i.kind) {
        case Kind::Enter:
        case Kind::LabelOp:
        case Kind::ICall:
        case Kind::IJump:
        case Kind::Return:
            holder_.clear();
            return true;
        case Kind::JumpOp:
            if ((i.op & 0xff) == JUMP) holder_.clear();
            return true;
        case Kind::FOp1:
            return visitFloat1(i);
        case Kind::FOp2:
            forward(i.src1, i.src1w);
            forward(i.src2, i.src2w);
            if (isFloatRegister(i.dst)) forgetRegister(i.dst);
            else if (isSlot(i.dst)) forgetOverlap(i.dstw);
            return true;
        case Kind::FSet64:
            if (isFloatRegister(i.dst)) forgetRegister(i.dst);
            return true;
        case Kind::FCmp:
            forward(i.src1, i.src1w);
            forward(i.src2, i.src2w);
            return true;
        case Kind::Op1:
        case Kind::Op2:
        case Kind::OpFlags:
            if (isSlot(i.dst)) forgetOverlap(i.dstw);
            return true;
        default:
            return true;
        }
    }

    bool visitFloat1(Instr &i) {
        switch (baseOp(i.op)) {
        case MOV_F64:
            if (isFloatRegister(i.dst) && isSlot(i.src1)) {
                const auto found = holder_.find(i.src1w);
                if (found != holder_.end()) {
                    if (found->second == i.dst) return false;
                    i.src1 = found->second;
                    i.src1w = 0;
                    forgetRegister(i.dst);
                    return true;
                }
                forgetRegister(i.dst);
                holder_[i.src1w] = i.dst;
                return true;
            }
            if (isSlot(i.dst) && isFloatRegister(i.src1)) {
                forgetOverlap(i.dstw);
                holder_[i.dstw] = i.src1;
                return true;
            }
            if (isFloatRegister(i.dst)) forgetRegister(i.dst);
            return true;
        case CMP_F64:
            forward(i.dst, i.dstw);
            forward(i.src1, i.src1w);
            return true;
        case ABS_F64:
            forward(i.src1, i.src1w);
            if (isFloatRegister(i.dst)) forgetRegister(i.dst);
            return true;
        case CONV_F64_FROM_SW:
        case CONV_F64_FROM_S32:
            if (isFloatRegister(i.dst)) forgetRegister(i.dst);
            return true;
        case CONV_SW_FROM_F64:
        case CONV_S32_FROM_F64:
            forward(i.src1, i.src1w);
            return true;
        default:
            return true;
        }
    }
};

} // namespace

void optimize(Builder &b) {
    static const bool enabled = [] {
        const char *value = std::getenv("SE_JIT_LIROPT");
        return value == nullptr || std::string(value) != "0";
    }();
    if (!enabled) return;
    SlotForwarder forwarder;
    forwarder.run(b);
}

} // namespace BytecodeJitLir

#endif
