#include "procedure.hpp"
#include "blockExecutor.hpp"
#include "sprite.hpp"
#include "value.hpp"

#ifdef __3DS__
#include <3ds.h>
#endif

Value ProcedureBlocks::stringNumber(Block &block, Sprite *sprite) {
    if (block.fields.at("VALUE")[0].get<std::string>() == "Scratch Everywhere! platform") {
#if defined(__3DS__)
        return Value(std::string("3DS"));
#elif defined(__WIIU__)
        return Value(std::string("Wii U"));
#elif defined(__PC__)
        return Value(std::string("PC"));
#elif defined(GAMECUBE)
        return Value(std::string("GameCube"));
#elif defined(WII)
        return Value(std::string("Wii"));
#elif defined(__SWITCH__)
        return Value(std::string("Switch"));
#else
        return Value(std::string("Unknown"));
#endif
    }

    return BlockExecutor::getCustomBlockValue(block.fields.at("VALUE")[0], sprite, block);
}

Value ProcedureBlocks::booleanArgument(Block &block, Sprite *sprite) {
    const std::string name = block.fields.at("VALUE")[0].get<std::string>();
    if (name == "is Scratch Everywhere!?") return Value(true);
#ifdef __3DS__
    if (name == "is New 3DS?") {
        bool out = false;
        APT_CheckNew3DS(&out);
        return Value(out);
    }
#endif

    Value value = BlockExecutor::getCustomBlockValue(block.fields.at("VALUE")[0], sprite, block);
    return Value(value.asInt() == 1);
}

BlockResult ProcedureBlocks::call(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -8;
        block.customBlockExecuted = false;

        // Run the custom block for the first time
        BlockExecutor::runCustomBlock(sprite, block, &block, withoutScreenRefresh);
        block.customBlockExecuted = true;

        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    // Check if any repeat blocks are still running inside the custom block
    if (block.customBlockPtr != nullptr &&
        !BlockExecutor::hasActiveRepeats(sprite, block.customBlockPtr->blockChainID)) {

        // std::cout << "done with custom!" << std::endl;

        // Custom block execution is complete
        block.repeatTimes = -1; // Reset for next use
        block.customBlockExecuted = false;
        block.customBlockPtr = nullptr;

        BlockExecutor::removeFromRepeatQueue(sprite, &block);

        return BlockResult::CONTINUE;
    }
    if (block.customBlockPtr == nullptr) {
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

BlockResult ProcedureBlocks::definition(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}
