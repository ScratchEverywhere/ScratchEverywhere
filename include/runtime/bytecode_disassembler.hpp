#pragma once
#include "bytecode.hpp"
#include <se_export.hpp>
#include <string>

namespace BytecodeDisassembler {

// used for the inspector
SE_EXPORT std::string disassemble(const BytecodeChunk &chunk, const BytecodeLinkTable *link = nullptr);

} // namespace BytecodeDisassembler
