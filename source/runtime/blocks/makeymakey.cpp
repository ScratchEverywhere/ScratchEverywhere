#include "blockUtils.hpp"
#include <input.hpp>
#include <sprite.hpp>

/*
    Value num1, num2;
    if (!Scratch::getInput(block, "NUM1", thread, sprite, num1) ||
        !Scratch::getInput(block, "NUM2", thread, sprite, num2)) return BlockResult::REPEAT;
    *outValue = num1 + num2;


*/
SCRATCH_BLOCK(makemakey, whenMakeyKeyPressed) {
    Value keyValue;
    if (!Scratch::getInput(block, "KEY", thread, sprite, keyValue)) return BlockResult::REPEAT;

    std::string key = Input::convertToKey(keyValue, true);
    if (Input::keyHeldDuration.find(key) != Input::keyHeldDuration.end() && Input::keyHeldDuration.find(key)->second > 0) {
        return BlockResult::CONTINUE_IMMEDIATELY;
    }
    return BlockResult::RETURN;
}

SCRATCH_BLOCK(makemakey, whenCodePressed) {
    if (Input::codePressedBlockOpcodes.find(block) != Input::codePressedBlockOpcodes.end()) return BlockResult::RETURN;
    Value sequence;
    if (!Scratch::getInput(block, "SEQUENCE", thread, sprite, sequence)) return BlockResult::REPEAT;

    std::string input = sequence.asString();
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

    if (keySequence.size() <= 1) return BlockResult::CONTINUE_IMMEDIATELY;

    if (Input::checkSequenceMatch(keySequence)) {
        Input::codePressedBlockOpcodes.insert(block);
        return BlockResult::CONTINUE_IMMEDIATELY;
    }
    return BlockResult::RETURN;
}

SCRATCH_SHADOW_BLOCK(makeymakey_menu_KEY, KEY)
SCRATCH_SHADOW_BLOCK(makeymakey_menu_SEQUENCE, SEQUENCE)