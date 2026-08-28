#pragma once
#include <cstdint>
#include <vector>

#include "../core/value.hpp"
#include "../opcodes/opcodes.hpp"
#include "../vm/engine_state.hpp"

class BytecodeChunk {
  public:
    std::vector<uint16_t> code;

    void emitOpcode(uint16_t opcode) {
        code.push_back(opcode);
    }

    void emit16(uint16_t val) {
        code.push_back(val);
    }

    void emit32(uint32_t val) {
        code.push_back(static_cast<uint16_t>(val & 0xFFFF));
        code.push_back(static_cast<uint16_t>(val >> 16));
    }

    void emitPushConstant(const Value &val) {
        if (val.isBoolean()) {
            code.push_back(static_cast<uint16_t>(
                val.asBoolean() ? Opcode::PUSH_TRUE : Opcode::PUSH_FALSE));
            return;
        }

        // -65535 <= x <= +65535, I think that should be enough for most projects
        double d = 0.0;
        if (val.tryGetDouble(d) && !std::isnan(d) && std::trunc(d) == d) {
            if (d >= 0.0 && d <= UINT16_MAX) {
                Log::log("Pushing positive int: " + std::to_string(d));
                code.push_back(static_cast<uint16_t>(Opcode::PUSH_POS_INT));
                code.push_back(static_cast<uint16_t>(d));
                return;
            }
            if (d < 0.0 && d >= -UINT16_MAX) {
                Log::log("Pushing negative int: " + std::to_string(d));
                code.push_back(static_cast<uint16_t>(Opcode::PUSH_NEG_INT));
                code.push_back(static_cast<uint16_t>(-d));
                return;
            }
        }
        Log::log("CONST VALUE: " + val.asString() + " as index: " + std::to_string(ConstantPool::getOrInsert(val)));

        uint16_t constIndex = ConstantPool::getOrInsert(val);
        code.push_back(static_cast<uint16_t>(Opcode::PUSH_CONST));
        code.push_back(constIndex);
    }

    struct ProcedureCallPatch {
        std::string proccode;
        size_t codeOffset;
    };

    std::vector<ProcedureCallPatch> unresolvedProcedureCalls;

    void append(BytecodeChunk &&other) {
        size_t baseOffset = code.size();
        for (auto &patch : other.unresolvedProcedureCalls) {
            unresolvedProcedureCalls.push_back({std::move(patch.proccode), patch.codeOffset + baseOffset});
        }
        other.unresolvedProcedureCalls.clear();

        code.insert(
            code.end(),
            std::make_move_iterator(other.code.begin()),
            std::make_move_iterator(other.code.end()));
        other.code.clear();
    }

    bool empty() const { return code.empty(); }

    size_t emitForwardJump(uint16_t jumpOpcode) {
        code.push_back(jumpOpcode);
        size_t placeholderIndex = code.size();
        code.push_back(0xFFFF);
        return placeholderIndex;
    }

    void patchForwardJump(size_t placeholderIndex) {
        size_t offset = code.size() - (placeholderIndex + 1);
        if (offset > UINT16_MAX) {
            // TODO: Handle 16-bit offset overflow (> 65535).
            // Keeping 16-bit jumps by default saves memory/shifts on low-end CPUs (like 3DS).
            // For long blocks, we could chain jumps (trampoline) or pre-calculate
            // the substack length upfront to emit long jumps conditionally.
            throw std::runtime_error("Jump offset too large (> 65535)!");
        }

        code[placeholderIndex] = static_cast<uint16_t>(offset);
    }

    void emitBackwardJump(uint16_t offset) {
        code.push_back(static_cast<uint16_t>(Opcode::JUMP_BACK));
        code.push_back(static_cast<uint16_t>(offset));
    }
};