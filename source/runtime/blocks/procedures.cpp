#include "blockUtils.hpp"
#include <sprite.hpp>
#include <value.hpp>

#if defined(RENDERER_SDL1) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL/SDL.h>

extern SDL_Joystick *controller;
#elif defined(RENDERER_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL2/SDL.h>

extern SDL_GameController *controller;
#elif defined(RENDERER_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
#include <SDL3/SDL.h>

extern SDL_Gamepad *controller;
#endif

SCRATCH_REPORTER_BLOCK_OPCODE(argument_reporter_string_number) {
    const std::string name = Scratch::getFieldValue(block, "VALUE");
    if (name == "Scratch Everywhere! platform") return Value(OS::getPlatform());
    if (name == "\u200B\u200Breceived data\u200B\u200B") return Scratch::dataNextProject;
    if (name == "Scratch Everywhere! controller") {
#ifdef RENDERER_CITRO2D
        return Value("3DS");
#elif defined(RENDERER_GL2D)
        return Value("NDS");
#elif defined(RENDERER_SDL1) && defined(PLATFORM_HAS_CONTROLLER)
        if (controller != nullptr) return Value(std::string(SDL_JoystickName(SDL_JoystickIndex(controller))));
#elif defined(RENDERER_SDL2) && defined(PLATFORM_HAS_CONTROLLER)
        if (controller != nullptr) return Value(std::string(SDL_GameControllerName(controller)));
#elif defined(RENDERER_SDL3) && defined(PLATFORM_HAS_CONTROLLER)
        if (controller != nullptr) return Value(std::string(SDL_GetGamepadName(controller)));
#endif
    }
    return BlockExecutor::getCustomBlockValue(name, sprite, block);
}

SCRATCH_REPORTER_BLOCK_OPCODE(argument_reporter_boolean) {
    const std::string name = Scratch::getFieldValue(block, "VALUE");
    if (name == "is Scratch Everywhere!?") return Value(true);
    if (name == "is New 3DS?") return Value(OS::isNew3DS());
    if (name == "is DSi?") return Value(OS::isDSi());

    return Value(BlockExecutor::getCustomBlockValue(name, sprite, block));
}

SCRATCH_BLOCK(procedures, call) {
    if (!fromRepeat) {
        // Run the custom block for the first time
        if (BlockExecutor::runCustomBlock(sprite, block, &block, withoutScreenRefresh) == BlockResult::RETURN) return BlockResult::RETURN;
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    // Check if any repeat blocks are still running inside the custom block
    if (block.customBlockPtr != nullptr && !BlockExecutor::hasActiveRepeats(sprite, block.customBlockPtr->blockChainID)) {

        // std::cout << "done with custom!" << std::endl;

        // Custom block execution is complete
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

SCRATCH_BLOCK_NOP(procedures, definition)
