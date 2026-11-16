#include "blockUtils.hpp"
#include <input.hpp>
#include <types.hpp>

SCRATCH_BLOCK(makeymakey, whenMakeyKeyPressed) {
    Value keyValue;
    if (!Scratch::getInputValue(block, "KEY", thread, sprite, keyValue)) return BlockResult::REPEAT;

    std::string key = Input::convertToKey(keyValue, true);
    if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && Input::keyHeldDuration.find(key)->second > 0) {
        return BlockResult::CONTINUE;
    }
    return BlockResult::RETURN;
}
SCRATCH_BLOCK_SIGNATURE(makeymakey, whenMakeyKeyPressed, {"KEY", BytecodeStackKind::Value});

SCRATCH_BLOCK(makeymakey, whenCodePressed) {
    Block *const hat = thread->blockHat != nullptr ? thread->blockHat : block;
    if (Input::codePressedBlockOpcodes.find(hat) != Input::codePressedBlockOpcodes.end()) return BlockResult::RETURN;
    std::string input;
    if (!Scratch::getInputValueAs(block, "SEQUENCE", thread, sprite, input)) return BlockResult::REPEAT;

    std::vector<std::string> keySequence;
    size_t start = 0;
    size_t end = input.find(' ');
    std::string key;
    while (end != std::string::npos) {
        key = input.substr(start, end - start);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        keySequence.push_back(key);
        start = end + 1;
        end = input.find(' ', start);
    }
    key = input.substr(start);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    keySequence.push_back(key);

    if (keySequence.size() <= 1) return BlockResult::CONTINUE;

    if (Input::checkSequenceMatch(keySequence)) {
        Input::codePressedBlockOpcodes.insert(hat);
        return BlockResult::CONTINUE;
    }
    return BlockResult::RETURN;
}
SCRATCH_BLOCK_SIGNATURE(makeymakey, whenCodePressed, {"SEQUENCE", BytecodeStackKind::String});

SCRATCH_SHADOW_BLOCK(makeymakey_menu_KEY, KEY)
SCRATCH_SHADOW_BLOCK(makeymakey_menu_SEQUENCE, SEQUENCE)
