#include "looks.hpp"
#include "blockExecutor.hpp"
#include "image.hpp"
#include "interpret.hpp"
#include "math.hpp"

#ifdef __3DS__
#include "speech_manager_3ds.hpp"
#elif defined(SDL_BUILD)
#include "speech_manager_sdl.hpp"
#elif defined(__NDS__)
#include "speech_manager_nds.hpp"
#else
#include "speech_manager.hpp"
#endif

#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"
#include <algorithm>
#include <cstddef>

BlockResult LooksBlocks::show(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->visible = true;
    Image::loadImageFromProject(sprite);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::hide(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    sprite->visible = false;
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::switchCostumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "COSTUME", sprite);

    if (inputValue.isDouble()) {
        Scratch::switchCostume(sprite, inputValue.isNaN() ? 0 : inputValue.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (size_t i = 0; i < sprite->costumes.size(); i++) {
        if (sprite->costumes[i].name == inputValue.asString()) {
            Scratch::switchCostume(sprite, i);
            return BlockResult::CONTINUE;
        }
    }

    if (inputValue.asString() == "next costume") {
        Scratch::switchCostume(sprite, ++sprite->currentCostume);
        return BlockResult::CONTINUE;
    } else if (inputValue.asString() == "previous costume") {
        Scratch::switchCostume(sprite, --sprite->currentCostume);
        return BlockResult::CONTINUE;
    }

    if (inputValue.isNumeric()) {
        Scratch::switchCostume(sprite, inputValue.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::nextCostume(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Scratch::switchCostume(sprite, ++sprite->currentCostume);
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::switchBackdropTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "BACKDROP", sprite);

    if (inputValue.isDouble()) {
        Scratch::switchCostume(stageSprite, inputValue.isNaN() ? 0 : inputValue.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (size_t i = 0; i < stageSprite->costumes.size(); i++) {
        if (stageSprite->costumes[i].name == inputValue.asString()) {
            Scratch::switchCostume(stageSprite, i);
            return BlockResult::CONTINUE;
        }
    }

    if (inputValue.asString() == "next backdrop") {
        Scratch::switchCostume(stageSprite, ++stageSprite->currentCostume);
        return BlockResult::CONTINUE;
    } else if (inputValue.asString() == "previous backdrop") {
        Scratch::switchCostume(stageSprite, --stageSprite->currentCostume);
        return BlockResult::CONTINUE;
    } else if (inputValue.asString() == "random backdrop") {
        if (stageSprite->costumes.size() == 1) return BlockResult::CONTINUE;
        int randomIndex = std::rand() % (stageSprite->costumes.size() - 1);
        if (randomIndex >= stageSprite->currentCostume) randomIndex++;
        Scratch::switchCostume(stageSprite, randomIndex);
        return BlockResult::CONTINUE;
    }

    if (inputValue.isNumeric()) {
        Scratch::switchCostume(stageSprite, inputValue.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (auto &currentSprite : sprites) {
        for (auto &[id, spriteBlock] : currentSprite->blocks) {
            if (spriteBlock.opcode == "event_whenbackdropswitchesto") {
                if (Scratch::getFieldValue(spriteBlock, "BACKDROP") == stageSprite->costumes[stageSprite->currentCostume].name) {
                    executor.runBlock(spriteBlock, currentSprite, withoutScreenRefresh, false);
                }
            }
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::nextBackdrop(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Scratch::switchCostume(stageSprite, stageSprite->currentCostume++);

    for (auto &currentSprite : sprites) {
        for (auto &[id, spriteBlock] : currentSprite->blocks) {
            if (spriteBlock.opcode == "event_whenbackdropswitchesto") {
                if (Scratch::getFieldValue(spriteBlock, "BACKDROP") == stageSprite->costumes[stageSprite->currentCostume].name) {
                    executor.runBlock(spriteBlock, currentSprite, withoutScreenRefresh, false);
                }
            }
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::goForwardBackwardLayers(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "NUM", sprite);
    std::string forwardBackward = Scratch::getFieldValue(block, "FORWARD_BACKWARD");
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    int shift = value.asInt();
    if (shift == 0) return BlockResult::CONTINUE;

    if (forwardBackward == "forward") {
        int targetLayer = sprite->layer + shift;

        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage || currentSprite == sprite) continue;
            if (currentSprite->layer >= targetLayer) {
                currentSprite->layer++;
            }
        }

        sprite->layer = targetLayer;

    } else if (forwardBackward == "backward") {
        int targetLayer = sprite->layer - shift;

        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage || currentSprite == sprite) continue;
            if (currentSprite->layer <= targetLayer) {
                currentSprite->layer--;
                if (currentSprite->layer < 0) currentSprite->layer = 0;
            }
        }

        sprite->layer = targetLayer;
    }

    Scratch::forceRedraw = true;
    Scratch::sortSprites();
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::goToFrontBack(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string value = Scratch::getFieldValue(block, "FRONT_BACK");
    if (value == "front") {

        double maxLayer = 0.0;
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->layer > maxLayer) {
                maxLayer = currentSprite->layer;
            }
        }

        sprite->layer = maxLayer + 1;

    } else if (value == "back") {
        double minLayer = std::numeric_limits<double>::max();
        for (Sprite *currentSprite : sprites) {
            if (currentSprite->isStage) continue;
            if (currentSprite->layer < minLayer) {
                minLayer = currentSprite->layer;
            }
        }

        sprite->layer = minLayer - 1;
    }
    Scratch::forceRedraw = true;
    Scratch::sortSprites();
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::setSizeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "SIZE", sprite);

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size = value.asDouble();
        return BlockResult::CONTINUE;
    }

    if (value.isNumeric()) {
        const double inputSizePercent = value.asDouble();

        const double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight));

        const double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight);

        const double clampedScale = std::clamp(inputSizePercent / 100.0, minScale, maxScale);
        sprite->size = clampedScale * 100.0;
    }
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::changeSizeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value value = Scratch::getInputValue(block, "CHANGE", sprite);

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size += value.asDouble();
        return BlockResult::CONTINUE;
    }

    if (value.isNumeric()) {
        sprite->size += value.asDouble();

        double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight)) * 100.0;

        double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight) * 100.0;

        sprite->size = std::clamp(static_cast<double>(sprite->size), minScale, maxScale);
    }
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    std::string effect = Scratch::getFieldValue(block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);
    Value amount = Scratch::getInputValue(block, "VALUE", sprite);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        // doable....
    } else if (effect == "FISHEYE") {
        // blehhh
    } else if (effect == "WHIRL") {
        // blehhh
    } else if (effect == "PIXELATE") {
        // blehhh
    } else if (effect == "MOSAIC") {
        // blehhh
    } else if (effect == "BRIGHTNESS") {
        sprite->brightnessEffect = std::clamp(amount.asDouble(), -100.0, 100.0);
    } else if (effect == "GHOST") {
        sprite->ghostEffect = std::clamp(amount.asDouble(), 0.0, 100.0);
    } else {
    }

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    std::string effect = Scratch::getFieldValue(block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);
    Value amount = Scratch::getInputValue(block, "CHANGE", sprite);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        // doable....
    } else if (effect == "FISHEYE") {
        // blehhh
    } else if (effect == "WHIRL") {
        // blehhh
    } else if (effect == "PIXELATE") {
        // blehhh
    } else if (effect == "MOSAIC") {
        // blehhh
    } else if (effect == "BRIGHTNESS") {
        sprite->brightnessEffect += amount.asDouble();
        sprite->brightnessEffect = std::clamp(sprite->brightnessEffect, -100.0f, 100.0f);
    } else if (effect == "GHOST") {
        sprite->ghostEffect += amount.asDouble();
        sprite->ghostEffect = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    } else {
    }
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::clearGraphicEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {

    sprite->ghostEffect = 0.0f;
    sprite->colorEffect = 0.0f;
    sprite->brightnessEffect = 0.0f;

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

BlockResult LooksBlocks::say(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!speechManager) return BlockResult::CONTINUE;

    Value messageValue = Scratch::getInputValue(block, "MESSAGE", sprite);
    std::string message = messageValue.asString();

    speechManager->showSpeech(sprite, message, -1, "say");

    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::sayForSeconds(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!speechManager) return BlockResult::CONTINUE;

    // copied ControlBlocks::wait functionality to hold off subsequent block execution until speech timer expires
    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        Value messageValue = Scratch::getInputValue(block, "MESSAGE", sprite);
        Value secondsValue = Scratch::getInputValue(block, "SECS", sprite);
        std::string message = messageValue.asString();
        double seconds = secondsValue.asDouble();

        block.waitDuration = seconds * 1000; // convert to milliseconds
        block.waitTimer.start();

        speechManager->showSpeech(sprite, message, seconds, "say");
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    block.repeatTimes -= 1;

    if (block.waitTimer.hasElapsed(block.waitDuration) && block.repeatTimes <= -4) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}
BlockResult LooksBlocks::think(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!speechManager) return BlockResult::CONTINUE;

    Value messageValue = Scratch::getInputValue(block, "MESSAGE", sprite);
    std::string message = messageValue.asString();

    speechManager->showSpeech(sprite, message, -1, "think");

    return BlockResult::CONTINUE;
}
BlockResult LooksBlocks::thinkForSeconds(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    if (!speechManager) return BlockResult::CONTINUE;

    // copied ControlBlocks::wait functionality to hold off subsequent block execution until speech timer expires
    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        Value messageValue = Scratch::getInputValue(block, "MESSAGE", sprite);
        Value secondsValue = Scratch::getInputValue(block, "SECS", sprite);
        std::string message = messageValue.asString();
        double seconds = secondsValue.asDouble();

        block.waitDuration = seconds * 1000; // convert to milliseconds
        block.waitTimer.start();

        speechManager->showSpeech(sprite, message, seconds, "think");
        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    block.repeatTimes -= 1;

    if (block.waitTimer.hasElapsed(block.waitDuration) && block.repeatTimes <= -4) {
        block.repeatTimes = -1;
        BlockExecutor::removeFromRepeatQueue(sprite, &block);
        return BlockResult::CONTINUE;
    }

    return BlockResult::RETURN;
}

Value LooksBlocks::size(Block &block, Sprite *sprite) {
    return Value(sprite->size);
}

Value LooksBlocks::costumeNumberName(Block &block, Sprite *sprite) {
    std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");
    if (value == "name") {
        return Value(sprite->costumes[sprite->currentCostume].name);
    } else if (value == "number") {
        return Value(sprite->currentCostume + 1);
    }
    return Value();
}

Value LooksBlocks::backdropNumberName(Block &block, Sprite *sprite) {
    const std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");

    if (value == "name") return Value(stageSprite->costumes[stageSprite->currentCostume].name);
    if (value == "number") return Value(stageSprite->currentCostume + 1);

    return Value();
}
