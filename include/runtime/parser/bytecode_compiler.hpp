#pragma once
#include "bytecode.hpp"
#include <se_export.hpp>
#include <string>
#include <unordered_map>
#include <vector>

struct ParsedInput;

class SE_EXPORT BytecodeCompiler {
  public:
    using CustomCompileFn = bool (*)(BytecodeCompiler &compiler, Block *block, BytecodeIndex &outFirstInstr);
    static std::unordered_map<std::string, CustomCompileFn> &getCustomCompilers();

    explicit BytecodeCompiler(Sprite *sprite) : sprite_(sprite) {}

    Sprite *sprite() const { return sprite_; }
    const std::vector<std::string> *currentArgumentNames() const { return currentArgumentNames_; }

    BytecodeIndex here() const { return static_cast<BytecodeIndex>(chunk_.instructions.size()); }
    BytecodeIndex emit(BytecodeOp op, BytecodeIndex a = invalidBytecodeIndex, BytecodeIndex b = invalidBytecodeIndex);
    void patchJumpTarget(BytecodeIndex jumpInstr, BytecodeIndex target);

    void compileOperand(const ParsedInput *input, BytecodeStackKind wanted);
    void compileInput(Block *block, const std::string &inputName, BytecodeStackKind wanted);

    static Block *substackBlock(Block *block, const std::string &inputName);

    bool compileBlockChain(Block *start, Block *stopBlock, BytecodeIndex &outFirstInstr);
    bool compileSubstack(Block *block, const std::string &inputName, Block *stopBlock, BytecodeIndex &outFirstInstr);

    BytecodeIndex variableSlot(const std::string &variableId);
    BytecodeIndex listSlot(const std::string &listId);
    BytecodeIndex escapeSlot(Block *block);
    BytecodeIndex opcodeSlot(const std::string &opcode, const std::vector<BytecodeInputSignature> &signature);

    BytecodeChunk &chunk() { return chunk_; }
    BytecodeLinkTable &link() { return link_; }
    void setCurrentArgumentNames(const std::vector<std::string> *names) { currentArgumentNames_ = names; }

  private:
    Sprite *sprite_ = nullptr;
    BytecodeChunk chunk_;
    BytecodeLinkTable link_;

    std::unordered_map<std::string, BytecodeIndex> opcodeSlotMap_;
    std::unordered_map<std::string, BytecodeIndex> variableSlotMap_;
    std::unordered_map<std::string, BytecodeIndex> listSlotMap_;
    std::unordered_map<Block *, BytecodeIndex> escapeBlockMap_;

    const std::vector<std::string> *currentArgumentNames_ = nullptr;
};
