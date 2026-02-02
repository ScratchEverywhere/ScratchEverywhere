#include "blockExecutor.hpp"
#include "blockUtils.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cstddef>
#include <image.hpp>
#include <render.hpp>
#include <speech_manager.hpp>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(looks, say) {
    SpeechManager *speechManager = Render::getSpeechManager();
    if (!speechManager) return BlockResult::CONTINUE;

    Value messageValue = Scratch::getInputValue(block, "MESSAGE", sprite);
    std::string message = messageValue.asString();

    speechManager->showSpeech(sprite, message, -1, "say");

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK(looks, sayforsecs) {
    SpeechManager *speechManager = Render::getSpeechManager();
    if (!speechManager) return BlockResult::CONTINUE;

    // copied wait block functionality to hold off subsequent block execution until speech timer expires
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
SCRATCH_BLOCK(looks, think) {
    SpeechManager *speechManager = Render::getSpeechManager();
    if (!speechManager) return BlockResult::CONTINUE;

    Value messageValue = Scratch::getInputValue(block, "MESSAGE", sprite);
    std::string message = messageValue.asString();

    speechManager->showSpeech(sprite, message, -1, "think");

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK(looks, thinkforsecs) {
    SpeechManager *speechManager = Render::getSpeechManager();
    if (!speechManager) return BlockResult::CONTINUE;

    // copied wait block functionality to hold off subsequent block execution until speech timer expires
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

SCRATCH_BLOCK(looks, show) {
    sprite->visible = true;
    Image::loadImageFromProject(sprite);
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, hide) {
    sprite->visible = false;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchcostumeto) {
    const Value inputValue = Scratch::getInputValue(block, "COSTUME", sprite);

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

SCRATCH_BLOCK(looks, nextcostume) {
    Scratch::switchCostume(sprite, ++sprite->currentCostume);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchbackdropto) {
    const Value inputValue = Scratch::getInputValue(block, "BACKDROP", sprite);

    if (inputValue.isDouble()) {
        Scratch::switchCostume(Scratch::stageSprite, inputValue.isNaN() ? 0 : inputValue.asDouble() - 1);
        goto end;
    }

    for (size_t i = 0; i < Scratch::stageSprite->costumes.size(); i++) {
        if (Scratch::stageSprite->costumes[i].name == inputValue.asString()) {
            Scratch::switchCostume(Scratch::stageSprite, i);
            goto end;
        }
    }

    if (inputValue.asString() == "next backdrop") {
        Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
        goto end;
    } else if (inputValue.asString() == "previous backdrop") {
        Scratch::switchCostume(Scratch::stageSprite, --Scratch::stageSprite->currentCostume);
        goto end;
    } else if (inputValue.asString() == "random backdrop") {
        if (Scratch::stageSprite->costumes.size() == 1) goto end;
        int randomIndex = std::rand() % (Scratch::stageSprite->costumes.size() - 1);
        if (randomIndex >= Scratch::stageSprite->currentCostume) randomIndex++;
        Scratch::switchCostume(Scratch::stageSprite, randomIndex);
        goto end;
    }

    if (inputValue.isNumeric()) {
        Scratch::switchCostume(Scratch::stageSprite, inputValue.asDouble() - 1);
        goto end;
    }

end:
    Scratch::backdropQueue.push_back(Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchbackdroptoandwait) {
    const Value inputValue = Scratch::getInputValue(block, "BACKDROP", sprite);

    if (!fromRepeat) {
        if (inputValue.isDouble()) {
            Scratch::switchCostume(Scratch::stageSprite, inputValue.isNaN() ? 0 : inputValue.asDouble() - 1);
            goto end;
        }

        for (size_t i = 0; i < Scratch::stageSprite->costumes.size(); i++) {
            if (Scratch::stageSprite->costumes[i].name == inputValue.asString()) {
                Scratch::switchCostume(Scratch::stageSprite, i);
                goto end;
            }
        }

        if (inputValue.asString() == "next backdrop") {
            Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
            goto end;
        } else if (inputValue.asString() == "previous backdrop") {
            Scratch::switchCostume(Scratch::stageSprite, --Scratch::stageSprite->currentCostume);
            goto end;
        } else if (inputValue.asString() == "random backdrop") {
            if (Scratch::stageSprite->costumes.size() == 1) goto end;
            int randomIndex = std::rand() % (Scratch::stageSprite->costumes.size() - 1);
            if (randomIndex >= Scratch::stageSprite->currentCostume) randomIndex++;
            Scratch::switchCostume(Scratch::stageSprite, randomIndex);
            goto end;
        }

        if (inputValue.isNumeric()) {
            Scratch::switchCostume(Scratch::stageSprite, inputValue.asDouble() - 1);
            goto end;
        }

    end:
        const std::string finalBackdrop = Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name;
        for (Sprite *spr : Scratch::sprites) {
            for (auto &[id, hat_block] : spr->blocks) {
                if (hat_block.opcode == "event_whenbackdropswitchesto" && Scratch::getFieldValue(hat_block, "BACKDROP") == finalBackdrop) {
                    Scratch::backdropQueue.push_back(finalBackdrop);
                    BlockExecutor::addToRepeatQueue(sprite, &block);
                    return BlockResult::RETURN;
                }
            }
        }
        return BlockResult::CONTINUE;
    }

    if (block.backdropsRun.empty()) {
        for (Sprite *spr : Scratch::sprites) {
            for (auto &[id, chain] : spr->blockChains) {
                if (chain.blocksToRepeat.empty()) continue;

                for (auto &chainBlock : chain.blockChain) {
                    if (chainBlock->opcode == "event_whenbackdropswitchesto" && Scratch::getFieldValue(*chainBlock, "BACKDROP") == inputValue.asString()) {
                        block.backdropsRun.push_back({chainBlock, spr});
                        break;
                    }
                }
            }
        }
    }

    bool shouldEnd = true;
    for (auto &[blockPtr, spritePtr] : block.backdropsRun) {
        if (spritePtr->toDelete) continue;
        if (!spritePtr->blockChains[blockPtr->blockChainID].blocksToRepeat.empty()) {
            shouldEnd = false;
            break;
        }
    }

    if (!shouldEnd) return BlockResult::RETURN;

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    block.backdropsRun.clear();
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, nextbackdrop) {
    Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
    Scratch::backdropQueue.push_back(Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, goforwardbackwardlayers) {
    Scratch::forceRedraw = true;
    if (sprite->isStage) return BlockResult::CONTINUE;

    const Value value = Scratch::getInputValue(block, "NUM", sprite);
    const std::string forwardBackward = Scratch::getFieldValue(block, "FORWARD_BACKWARD");
    if (!value.isNumeric()) return BlockResult::CONTINUE;

    int shift = floor(value.asDouble());
    if (forwardBackward == "backward") shift = -shift;

    const int currentIndex = (Scratch::sprites.size() - 1) - sprite->layer;
    const int targetIndex = std::clamp<int>(currentIndex - shift, 0, Scratch::sprites.size() - 2);

    if (targetIndex == currentIndex) return BlockResult::CONTINUE;

    if (targetIndex < currentIndex) {
        std::rotate(Scratch::sprites.begin() + targetIndex, Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1);
    } else {
        std::rotate(Scratch::sprites.begin() + currentIndex, Scratch::sprites.begin() + currentIndex + 1, Scratch::sprites.begin() + targetIndex + 1);
    }

    for (int i = std::min(currentIndex, targetIndex); i <= std::max(currentIndex, targetIndex); i++) {
        Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, gotofrontback) {
    Scratch::forceRedraw = true;
    if (sprite->isStage) return BlockResult::CONTINUE;

    const std::string value = Scratch::getFieldValue(block, "FRONT_BACK");

    const int currentIndex = (Scratch::sprites.size() - 1) - sprite->layer;
    const int targetIndex = value == "front" ? 0 : (Scratch::sprites.size() - 2);

    if (currentIndex == targetIndex) return BlockResult::CONTINUE;

    if (targetIndex < currentIndex) {
        std::rotate(Scratch::sprites.begin() + targetIndex,
                    Scratch::sprites.begin() + currentIndex,
                    Scratch::sprites.begin() + currentIndex + 1);
    } else {
        std::rotate(Scratch::sprites.begin() + currentIndex,
                    Scratch::sprites.begin() + currentIndex + 1,
                    Scratch::sprites.begin() + targetIndex + 1);
    }

    for (int i = std::min(currentIndex, targetIndex); i <= std::max(currentIndex, targetIndex); i++) {
        Scratch::sprites[i]->layer = (Scratch::sprites.size() - 1) - i;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setsizeto) {
    const Value value = Scratch::getInputValue(block, "SIZE", sprite);

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

SCRATCH_BLOCK(looks, changesizeby) {
    const Value value = Scratch::getInputValue(block, "CHANGE", sprite);

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size += value.asDouble();
        return BlockResult::CONTINUE;
    }

    if (value.isNumeric()) {
        sprite->size += value.asDouble();

        const double minScale = std::min(1.0, std::max(5.0 / sprite->spriteWidth, 5.0 / sprite->spriteHeight)) * 100.0;

        const double maxScale = std::min((1.5 * Scratch::projectWidth) / sprite->spriteWidth, (1.5 * Scratch::projectHeight) / sprite->spriteHeight) * 100.0;

        sprite->size = std::clamp(static_cast<double>(sprite->size), minScale, maxScale);
    }
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, seteffectto) {
    std::string effect = Scratch::getFieldValue(block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);
    const Value amount = Scratch::getInputValue(block, "VALUE", sprite);

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
    }

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK(looks, changeeffectby) {
    std::string effect = Scratch::getFieldValue(block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);
    const Value amount = Scratch::getInputValue(block, "CHANGE", sprite);

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
    }

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, cleargraphiceffects) {
    sprite->ghostEffect = 0.0f;
    sprite->colorEffect = 0.0f;
    sprite->brightnessEffect = 0.0f;

    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(looks, size) {
    return Value(sprite->size);
}

SCRATCH_REPORTER_BLOCK(looks, costumenumbername) {
    const std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");

    if (value == "name") return Value(sprite->costumes[sprite->currentCostume].name);
    if (value == "number") return Value(sprite->currentCostume + 1);

    return Value();
}

SCRATCH_REPORTER_BLOCK(looks, backdropnumbername) {
    const std::string value = Scratch::getFieldValue(block, "NUMBER_NAME");

    if (value == "name") return Value(Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name);
    if (value == "number") return Value(Scratch::stageSprite->currentCostume + 1);

    return Value();
}
