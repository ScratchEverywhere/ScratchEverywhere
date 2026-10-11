#include "bytecode_disassembler.hpp"
#include "types.hpp"
#include <sstream>

namespace BytecodeDisassembler {

namespace {

std::string indexOrDash(BytecodeIndex idx) {
    if (idx == invalidBytecodeIndex) return "-";
    return std::to_string(idx);
}

std::string stringAt(const BytecodeChunk &chunk, BytecodeIndex idx) {
    if (idx == invalidBytecodeIndex || idx >= chunk.strings.size()) return "<?>";
    return chunk.strings[idx];
}

std::string opcodeNameForSlot(const BytecodeChunk &chunk, BytecodeIndex slot) {
    if (slot == invalidBytecodeIndex || slot >= chunk.opcodeSlots.size()) return "<?>";
    return stringAt(chunk, chunk.opcodeSlots[slot]);
}

std::string variableNameForSlot(const BytecodeChunk &chunk, BytecodeIndex slot) {
    if (slot == invalidBytecodeIndex || slot >= chunk.variableSlots.size()) return "<?>";
    return stringAt(chunk, chunk.variableSlots[slot]);
}

std::string listNameForSlot(const BytecodeChunk &chunk, BytecodeIndex slot) {
    if (slot == invalidBytecodeIndex || slot >= chunk.listSlots.size()) return "<?>";
    return stringAt(chunk, chunk.listSlots[slot]);
}

std::string escapeTargetForSlot(const BytecodeLinkTable *link, BytecodeIndex slot) {
    if (link == nullptr || slot == invalidBytecodeIndex || slot >= link->escapeBlocks.size()) {
        return "escapeBlocks[" + std::to_string(slot) + "]";
    }
    Block *block = link->escapeBlocks[slot];
    if (block == nullptr) return "escapeBlocks[" + std::to_string(slot) + "] (null)";
    return block->opcode + " (escapeBlocks[" + std::to_string(slot) + "])";
}

const char *mnemonic(BytecodeOp op) {
    switch (op) {
    case BytecodeOp::Nop:
        return "Nop";
    case BytecodeOp::PushConstNumber:
        return "PushConstNumber";
    case BytecodeOp::PushConstString:
        return "PushConstString";
    case BytecodeOp::PushConstBoolean:
        return "PushConstBoolean";
    case BytecodeOp::PushConstColor:
        return "PushConstColor";
    case BytecodeOp::PushConstValue:
        return "PushConstValue";
    case BytecodeOp::PushVarNumber:
        return "PushVarNumber";
    case BytecodeOp::PushVarString:
        return "PushVarString";
    case BytecodeOp::PushVarBoolean:
        return "PushVarBoolean";
    case BytecodeOp::PushVarColor:
        return "PushVarColor";
    case BytecodeOp::PushVarValue:
        return "PushVarValue";
    case BytecodeOp::PushList:
        return "PushList";
    case BytecodeOp::PushArg:
        return "PushArg";
    case BytecodeOp::StoreVarNumber:
        return "StoreVarNumber";
    case BytecodeOp::StoreVarString:
        return "StoreVarString";
    case BytecodeOp::StoreVarBoolean:
        return "StoreVarBoolean";
    case BytecodeOp::StoreVarColor:
        return "StoreVarColor";
    case BytecodeOp::StoreVarValue:
        return "StoreVarValue";
    case BytecodeOp::ChangeVarNumber:
        return "ChangeVarNumber";
    case BytecodeOp::CallNative:
        return "CallNative";
    case BytecodeOp::CallNativeNumber:
        return "CallNativeNumber";
    case BytecodeOp::CallNativeString:
        return "CallNativeString";
    case BytecodeOp::CallNativeBoolean:
        return "CallNativeBoolean";
    case BytecodeOp::CallNativeColor:
        return "CallNativeColor";
    case BytecodeOp::CallNativeValue:
        return "CallNativeValue";
    case BytecodeOp::CallProcedure:
        return "CallProcedure";
    case BytecodeOp::Return:
        return "Return";
    case BytecodeOp::CallLegacyReporter:
        return "CallLegacyReporter";
    case BytecodeOp::EscapeToLegacyThread:
        return "EscapeToLegacyThread";
    case BytecodeOp::ConvertNumberToValue:
        return "ConvertNumberToValue";
    case BytecodeOp::ConvertStringToValue:
        return "ConvertStringToValue";
    case BytecodeOp::ConvertBooleanToValue:
        return "ConvertBooleanToValue";
    case BytecodeOp::ConvertColorToValue:
        return "ConvertColorToValue";
    case BytecodeOp::ConvertValueToNumber:
        return "ConvertValueToNumber";
    case BytecodeOp::ConvertValueToString:
        return "ConvertValueToString";
    case BytecodeOp::ConvertValueToBoolean:
        return "ConvertValueToBoolean";
    case BytecodeOp::ConvertValueToColor:
        return "ConvertValueToColor";
    case BytecodeOp::Jump:
        return "Jump";
    case BytecodeOp::JumpIfFalse:
        return "JumpIfFalse";
    case BytecodeOp::JumpIfTrue:
        return "JumpIfTrue";
    case BytecodeOp::RepeatCheck:
        return "RepeatCheck";
    case BytecodeOp::Wait:
        return "Wait";
    case BytecodeOp::PopNumber:
        return "PopNumber";
    case BytecodeOp::PopString:
        return "PopString";
    case BytecodeOp::PopBoolean:
        return "PopBoolean";
    case BytecodeOp::PopColor:
        return "PopColor";
    case BytecodeOp::PopValue:
        return "PopValue";
    case BytecodeOp::Yield:
        return "Yield";
    case BytecodeOp::Halt:
        return "Halt";
    }
    return "<unknown op>";
}

bool isCallNative(BytecodeOp op) {
    switch (op) {
    case BytecodeOp::CallNative:
    case BytecodeOp::CallNativeNumber:
    case BytecodeOp::CallNativeString:
    case BytecodeOp::CallNativeBoolean:
    case BytecodeOp::CallNativeColor:
    case BytecodeOp::CallNativeValue:
        return true;
    default:
        return false;
    }
}

void appendOperandDetail(std::ostringstream &out, const BytecodeChunk &chunk, const BytecodeLinkTable *link, const BytecodeInstruction &instr) {
    switch (instr.op) {
    case BytecodeOp::PushConstNumber:
        if (instr.a < chunk.numberConstants.size()) out << "  ; " << chunk.numberConstants[instr.a];
        return;
    case BytecodeOp::PushConstString:
        if (instr.a < chunk.stringConstants.size()) out << "  ; \"" << chunk.stringConstants[instr.a] << "\"";
        return;
    case BytecodeOp::PushConstBoolean:
        if (instr.a < chunk.boolConstants.size()) out << "  ; " << (chunk.boolConstants[instr.a] ? "true" : "false");
        return;
    case BytecodeOp::PushConstColor:
        if (instr.a < chunk.colorConstants.size()) {
            const Color &c = chunk.colorConstants[instr.a];
            out << "  ; hue=" << c.hue << " sat=" << c.saturation << " bri=" << c.brightness << " trans=" << c.transparency;
        }
        return;
    case BytecodeOp::PushConstValue:
        if (instr.a < chunk.constants.size()) out << "  ; " << chunk.constants[instr.a].asString();
        return;

    case BytecodeOp::PushVarNumber:
    case BytecodeOp::PushVarString:
    case BytecodeOp::PushVarBoolean:
    case BytecodeOp::PushVarColor:
    case BytecodeOp::PushVarValue:
    case BytecodeOp::StoreVarNumber:
    case BytecodeOp::StoreVarString:
    case BytecodeOp::StoreVarBoolean:
    case BytecodeOp::StoreVarColor:
    case BytecodeOp::StoreVarValue:
    case BytecodeOp::ChangeVarNumber:
        out << "  ; var \"" << variableNameForSlot(chunk, instr.a) << "\"";
        return;

    case BytecodeOp::PushList:
        out << "  ; list \"" << listNameForSlot(chunk, instr.a) << "\"";
        return;

    case BytecodeOp::PushArg:
        out << "  ; arg slot " << instr.a;
        return;

    case BytecodeOp::CallProcedure:
        if (instr.a < chunk.procedures.size()) {
            out << "  ; \"" << stringAt(chunk, chunk.procedures[instr.a].procCodeIndex) << "\", argCount=" << instr.b;
        }
        return;

    case BytecodeOp::CallLegacyReporter:
    case BytecodeOp::EscapeToLegacyThread:
        out << "  ; " << escapeTargetForSlot(link, instr.a);
        return;

    case BytecodeOp::Jump:
    case BytecodeOp::JumpIfFalse:
    case BytecodeOp::JumpIfTrue:
    case BytecodeOp::RepeatCheck:
        out << "  ; -> #" << instr.a;
        return;

    default:
        if (isCallNative(instr.op)) {
            out << "  ; " << opcodeNameForSlot(chunk, instr.a) << ", argCount=" << instr.b;
            if (link != nullptr && instr.a < link->positional.size() && link->positional[instr.a].nn_n != nullptr) {
                out << " [positional fast path]";
            }
        }
        return;
    }
}

void writeInstructions(std::ostringstream &out, const BytecodeChunk &chunk, const BytecodeLinkTable *link) {
    out << "Instructions (" << chunk.instructions.size() << "):\n";
    for (size_t i = 0; i < chunk.instructions.size(); i++) {
        const BytecodeInstruction &instr = chunk.instructions[i];
        out << "  #" << i << "  " << mnemonic(instr.op);

        if (instr.a != invalidBytecodeIndex) out << " a=" << instr.a;
        if (instr.b != invalidBytecodeIndex) out << " b=" << instr.b;

        appendOperandDetail(out, chunk, link, instr);
        out << "\n";
    }
}

} // namespace

std::string disassemble(const BytecodeChunk &chunk, const BytecodeLinkTable *link) {
    std::ostringstream out;

    size_t escapeCount = 0;
    for (const auto &instr : chunk.instructions) {
        if (instr.op == BytecodeOp::CallLegacyReporter || instr.op == BytecodeOp::EscapeToLegacyThread) escapeCount++;
    }

    out << "BytecodeChunk: " << chunk.instructions.size() << " instructions, "
        << chunk.scripts.size() << " scripts, " << chunk.procedures.size() << " procedures, "
        << escapeCount << " escape-hatch instruction(s)"
        << (link == nullptr ? " (unlinked -- escape targets shown as raw slots)" : "") << "\n\n";

    out << "Scripts (" << chunk.scripts.size() << "):\n";
    for (size_t i = 0; i < chunk.scripts.size(); i++) {
        const BytecodeScript &script = chunk.scripts[i];
        out << "  [" << i << "] " << stringAt(chunk, script.opcodeIndex)
            << " -> entry #" << indexOrDash(script.entryInstruction) << "\n";
    }
    out << "\n";

    out << "Procedures (" << chunk.procedures.size() << "):\n";
    for (size_t i = 0; i < chunk.procedures.size(); i++) {
        const BytecodeProcedure &proc = chunk.procedures[i];
        out << "  [" << i << "] \"" << stringAt(chunk, proc.procCodeIndex) << "\""
            << " argCount=" << proc.argCount
            << " warp=" << (proc.runWithoutScreenRefresh ? "true" : "false")
            << " -> entry #" << indexOrDash(proc.entryInstruction);
        if (proc.argumentNamesCount > 0) {
            out << " args=(";
            for (BytecodeIndex a = 0; a < proc.argumentNamesCount; a++) {
                if (a > 0) out << ", ";
                out << stringAt(chunk, proc.argumentNamesStart + a);
            }
            out << ")";
        }
        out << "\n";
    }
    out << "\n";

    writeInstructions(out, chunk, link);

    return out.str();
}

} // namespace BytecodeDisassembler
