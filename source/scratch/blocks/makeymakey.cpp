#include "makeymakey.hpp"
#include "blockExecutor.hpp"
#include "input.hpp"
#include "interpret.hpp"
#include "sprite.hpp"

BlockResult MakeyMakeyBlocks::whenMakeyKeyPressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult MakeyMakeyBlocks::whenCodePressed(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (Input::codePressedBlockOpcodes.find(block.id) != Input::codePressedBlockOpcodes.end()) return BlockResult::RETURN;
    std::string input = Scratch::getInputValue(block, "SEQUENCE", sprite).asString();
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

    if (keySequence.size() <= 1) return BlockResult::RETURN;

    if (Input::checkSequenceMatch(keySequence)) {
        Input::codePressedBlockOpcodes.insert(block.id);
        return BlockResult::CONTINUE;
    } else {
        return BlockResult::RETURN;
    }
}