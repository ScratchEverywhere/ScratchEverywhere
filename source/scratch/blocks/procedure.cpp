#include "procedure.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"

#ifdef RENDERER_SDL1
#include <SDL/SDL.h>

extern SDL_Joystick *controller;
#elif defined(RENDERER_SDL2)
#include <SDL2/SDL.h>

extern SDL_GameController *controller;
#elif defined(RENDERER_SDL3)
#include <SDL3/SDL.h>

extern SDL_Gamepad *controller;
#endif

Value ProcedureBlocks::stringNumber(Block &block, Sprite *sprite) {
    const std::string name = Scratch::getFieldValue(block, "VALUE");
    if (name == "Scratch Everywhere! platform") {
        return Value(OS::getPlatform());
    }
    if (name == "\u200B\u200Breceived data\u200B\u200B") {
        return Scratch::dataNextProject;
    }
    if (name == "Scratch Everywhere! controller") {
#ifdef __3DS__
        return Value("3DS");
#elif defined(RENDERER_SDL1)
        if (controller != nullptr) return Value(std::string(SDL_JoystickName(SDL_JoystickIndex(controller))));
#elif defined(RENDERER_SDL2)
        if (controller != nullptr) return Value(std::string(SDL_GameControllerName(controller)));
#elif defined(RENDERER_SDL3)
        if (controller != nullptr) return Value(std::string(SDL_GetGamepadName(controller)));
#endif
    }
    return BlockExecutor::getCustomBlockValue(name, sprite, block);
}

Value ProcedureBlocks::booleanArgument(Block &block, Sprite *sprite) {
    const std::string name = Scratch::getFieldValue(block, "VALUE");
    if (name == "is Scratch Everywhere!?") return Value(true);
    if (name == "is New 3DS?") {
        return Value(OS::isNew3DS());
    }
    if (name == "is DSi?") {
        return Value(OS::isDSi());
    }

    Value value = BlockExecutor::getCustomBlockValue(name, sprite, block);
    return Value(value.asBoolean());
}

BlockResult ProcedureBlocks::call(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!fromRepeat) {
        block.customBlockExecuted = false;

        // Run the custom block for the first time
        if (BlockExecutor::runCustomBlock(sprite, block, &block, withoutScreenRefresh) == BlockResult::RETURN) return BlockResult::RETURN;
        block.customBlockExecuted = true;

        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    // Check if any repeat blocks are still running inside the custom block
    if (block.customBlockPtr != nullptr &&
        !BlockExecutor::hasActiveRepeats(sprite, block.customBlockPtr->blockChainID)) {

        // std::cout << "done with custom!" << std::endl;

        // Custom block execution is complete
        block.customBlockExecuted = false;
        block.customBlockPtr = nullptr;

        BlockExecutor::removeFromRepeatQueue(sprite, &block);

        return BlockResult::CONTINUE;
    }
    if (block.customBlockPtr == nullptr) {
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

BlockResult ProcedureBlocks::definition(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}
