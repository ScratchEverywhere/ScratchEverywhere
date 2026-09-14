#include "blockExecutor.hpp"
#include "blockUtils.hpp"
#include "math.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cstddef>
#include <image.hpp>
#include <log.hpp>
#include <render.hpp>
#include <set>
#include <speech_manager.hpp>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(looks, say) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;

    std::string message;
    if (!Scratch::getInputValueAs(block, "MESSAGE", thread, sprite, message)) return BlockResult::REPEAT;

    SpeechManager *speechManager = Render::getSpeechManager();

    speechManager->showSpeech(sprite, message, -1, "say");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, sayforsecs) {
    BlockState *state = thread->getState(block);
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    SpeechManager *speechManager = Render::getSpeechManager();
    if (state->completedSteps == 0) {
        double seconds;
        std::string message;
        if (!Scratch::getInputValueAs(block, "SECS", thread, sprite, seconds) ||
            !Scratch::getInputValueAs(block, "MESSAGE", thread, sprite, message)) return BlockResult::REPEAT;

        state->waitDuration = seconds * 1000; // convert to milliseconds
        state->waitTimer.start();
        speechManager->showSpeech(sprite, message, seconds, "say");
        state->completedSteps = 1;
        return BlockResult::REPEAT;
    }

    if (state->waitTimer.getTimeMs() >= state->waitDuration) {
        thread->eraseState(block);
        speechManager->showSpeech(sprite, "", 0.01, "say");
        return BlockResult::CONTINUE;
    }
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(looks, think) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    SpeechManager *speechManager = Render::getSpeechManager();

    std::string message;
    if (!Scratch::getInputValueAs(block, "MESSAGE", thread, sprite, message)) return BlockResult::REPEAT;

    speechManager->showSpeech(sprite, message, -1, "think");

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK(looks, thinkforsecs) {
    BlockState *state = thread->getState(block);
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    SpeechManager *speechManager = Render::getSpeechManager();
    if (state->completedSteps == 0) {
        double seconds;
        std::string message;
        if (!Scratch::getInputValueAs(block, "SECS", thread, sprite, seconds) ||
            !Scratch::getInputValueAs(block, "MESSAGE", thread, sprite, message)) return BlockResult::REPEAT;

        state->waitDuration = seconds * 1000; // convert to milliseconds
        state->waitTimer.start();

        speechManager->showSpeech(sprite, message, state->waitDuration, "think");
        state->completedSteps = 1;
        return BlockResult::REPEAT;
    }

    if (state->waitTimer.hasElapsed(state->waitDuration)) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(looks, show) {
    if (!sprite->visible) Scratch::loadCurrentCostumeImage(sprite);
    sprite->visible = true;
    Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, hide) {
    if (!sprite->isStage) sprite->visible = false;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchcostumeto) {
    Value costume;
    if (!Scratch::getInputValue(block, "COSTUME", thread, sprite, costume)) return BlockResult::REPEAT;

    if (costume.isDouble()) {
        Scratch::switchCostume(sprite, costume.isNaN() ? 0 : costume.get<double>() - 1);
        return BlockResult::CONTINUE;
    }

    const std::string &costumeString = costume.get<std::string>();
    for (size_t i = 0; i < sprite->costumes.size(); i++) {
        if (sprite->costumes[i].name == costumeString) {
            Scratch::switchCostume(sprite, i);
            return BlockResult::CONTINUE;
        }
    }

    if (costumeString == "next costume") {
        Scratch::switchCostume(sprite, ++sprite->currentCostume);
        return BlockResult::CONTINUE;
    } else if (costumeString == "previous costume") {
        Scratch::switchCostume(sprite, --sprite->currentCostume);
        return BlockResult::CONTINUE;
    }

    if (costume.isNumeric()) {
        Scratch::switchCostume(sprite, costume.get<double>() - 1);
        return BlockResult::CONTINUE;
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, nextcostume) {
    Scratch::switchCostume(sprite, ++sprite->currentCostume);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchbackdropto) {
    Value backdrop;
    if (!Scratch::getInputValue(block, "BACKDROP", thread, sprite, backdrop)) return BlockResult::REPEAT;

    const std::string &backdropString = backdrop.asString();

    if (backdrop.isDouble()) {
        Scratch::switchCostume(Scratch::stageSprite, backdrop.isNaN() ? 0 : backdrop.get<double>() - 1);
        goto end;
    }

    for (size_t i = 0; i < Scratch::stageSprite->costumes.size(); i++) {
        if (Scratch::stageSprite->costumes[i].name == backdropString) {
            Scratch::switchCostume(Scratch::stageSprite, i);
            goto end;
        }
    }

    if (backdropString == "next backdrop") {
        Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
        goto end;
    } else if (backdropString == "previous backdrop") {
        Scratch::switchCostume(Scratch::stageSprite, --Scratch::stageSprite->currentCostume);
        goto end;
    } else if (backdropString == "random backdrop") {
        if (Scratch::stageSprite->costumes.size() == 1) goto end;
        int randomIndex = std::rand() % (Scratch::stageSprite->costumes.size() - 1);
        if (randomIndex >= Scratch::stageSprite->currentCostume) randomIndex++;
        Scratch::switchCostume(Scratch::stageSprite, randomIndex);
        goto end;
    }

    if (backdrop.isNumeric()) {
        Scratch::switchCostume(Scratch::stageSprite, backdrop.get<double>() - 1);
        goto end;
    }

end:
    std::string currentBackdrop = Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name;
    for (auto &spr : Scratch::sprites) {
        if (spr->hats["event_whenbackdropswitchesto"].empty()) continue;
        for (Block *hat : spr->hats["event_whenbackdropswitchesto"]) {

            if (Scratch::getFieldValue(*hat, "BACKDROP") == currentBackdrop) {
                BlockExecutor::startThread(spr, hat);
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, switchbackdroptoandwait) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps < 1) {
        Value backdrop;
        if (!Scratch::getInputValue(block, "BACKDROP", thread, sprite, backdrop)) return BlockResult::REPEAT;

        if (backdrop.isDouble()) {
            const double bk = backdrop.isNaN() ? 0 : backdrop.get<double>() - 1;
            if (bk < 0 || bk >= sprite->costumes.size()) return BlockResult::CONTINUE;
            Scratch::switchCostume(Scratch::stageSprite, bk);
        } else {
            const std::string &backdropString = backdrop.asString();

            bool found = false;
            for (size_t i = 0; i < Scratch::stageSprite->costumes.size(); i++) {
                if (Scratch::stageSprite->costumes[i].name == backdropString) {
                    Scratch::switchCostume(Scratch::stageSprite, i);
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (backdropString == "next backdrop") {
                    Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
                    found = true;
                } else if (backdropString == "previous backdrop") {
                    Scratch::switchCostume(Scratch::stageSprite, --Scratch::stageSprite->currentCostume);
                    found = true;
                } else if (backdropString == "random backdrop") {
                    if (Scratch::stageSprite->costumes.size() > 1) {
                        int randomIndex = std::rand() % (Scratch::stageSprite->costumes.size() - 1);
                        if (randomIndex >= Scratch::stageSprite->currentCostume) randomIndex++;
                        Scratch::switchCostume(Scratch::stageSprite, randomIndex);
                        found = true;
                    }
                } else if (backdrop.isNumeric()) {
                    Scratch::switchCostume(Scratch::stageSprite, backdrop.get<double>() - 1);
                    found = true;
                }
            }
            if (!found) return BlockResult::CONTINUE;
        }
        std::vector<ScriptThread *> newthreads;

        std::string currentBackdrop = Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name;
        for (auto &spr : Scratch::sprites) {
            if (spr->hats["event_whenbackdropswitchesto"].empty()) continue;
            for (Block *hat : spr->hats["event_whenbackdropswitchesto"]) {
                if (Scratch::getFieldValue(*hat, "BACKDROP") == currentBackdrop) {
                    BlockExecutor::startThread(spr, hat);
                }
            }
        }

        for (ScriptThread *t : newthreads) {
            state->threads.push_back(t->id);
        }
        state->completedSteps = 1;
        return BlockResult::REPEAT;
    }
    for (auto &stateID : state->threads) {
        for (auto &spriteThread : BlockExecutor::threads) {
            if (spriteThread->id != stateID) continue;
            if (!spriteThread->finished) return BlockResult::REPEAT;
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, nextbackdrop) {
    Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
    std::string currentBackdrop = Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name;
    for (auto &spr : Scratch::sprites) {
        if (spr->hats["event_whenbackdropswitchesto"].empty()) continue;
        for (Block *hat : spr->hats["event_whenbackdropswitchesto"]) {

            if (Scratch::getFieldValue(*hat, "BACKDROP") == currentBackdrop) {
                BlockExecutor::startThread(spr, hat);
            }
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, goforwardbackwardlayers) {
    if (sprite->isStage) return BlockResult::CONTINUE;
    double num;
    if (!Scratch::getInputValueAs(block, "NUM", thread, sprite, num)) return BlockResult::REPEAT;

    const std::string forwardBackward = Scratch::getFieldValue(*block, "FORWARD_BACKWARD");

    int shift = floor(num);
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

    BlockExecutor::sortSprites = true;

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, gotofrontback) {
    if (sprite->isStage) return BlockResult::CONTINUE;

    const std::string value = Scratch::getFieldValue(*block, "FRONT_BACK");

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
    BlockExecutor::sortSprites = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, setsizeto) {
    Value size;
    if (!Scratch::getInputValue(block, "SIZE", thread, sprite, size)) return BlockResult::REPEAT;

    if (!Scratch::fencing) {
        sprite->size = size.get<double>();

        Render::resizeSVGs(sprite);
        return BlockResult::CONTINUE;
    }

    if (size.isNumeric()) {
        const double inputSizePercent = size.get<double>();

        double minScale;
        double maxScale;
        if (sprite->spriteWidth < 1 || sprite->spriteHeight < 1) {
            minScale = 1.0;
            maxScale = 1800.0;
        } else {
            const Costume &costume = sprite->costumes[sprite->currentCostume];
            const int sprWidth = sprite->spriteWidth / costume.bitmapResolution;
            const int sprHeight = sprite->spriteHeight / costume.bitmapResolution;
            minScale = std::min(1.0, std::max(5.0 / sprWidth, 5.0 / sprHeight)) * 100.0;
            maxScale = std::min((1.5 * Scratch::projectWidth) / sprWidth, (1.5 * Scratch::projectHeight) / sprHeight) * 100.0;
        }

        sprite->size = std::clamp(inputSizePercent, minScale, maxScale);
        Render::resizeSVGs(sprite);
    }
    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changesizeby) {
    Value size;
    if (!Scratch::getInputValue(block, "CHANGE", thread, sprite, size)) return BlockResult::REPEAT;

    if (!Scratch::fencing) {
        sprite->size += size.get<double>();

        Render::resizeSVGs(sprite);
        return BlockResult::CONTINUE;
    }

    if (size.isNumeric()) {
        sprite->size += size.get<double>();

        double minScale;
        double maxScale;

        if (sprite->spriteWidth < 1 || sprite->spriteHeight < 1) {
            minScale = 1.0;
            maxScale = 1800.0;
        } else {
            const Costume &costume = sprite->costumes[sprite->currentCostume];
            const int sprWidth = sprite->spriteWidth / costume.bitmapResolution;
            const int sprHeight = sprite->spriteHeight / costume.bitmapResolution;
            minScale = std::min(1.0, std::max(5.0 / sprWidth, 5.0 / sprHeight)) * 100.0;
            maxScale = std::min((1.5 * Scratch::projectWidth) / sprWidth, (1.5 * Scratch::projectHeight) / sprHeight) * 100.0;
        }

        sprite->size = std::clamp(static_cast<double>(sprite->size), minScale, maxScale);

        Render::resizeSVGs(sprite);
    }
    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, seteffectto) {
    Value amount;
    if (!Scratch::getInputValue(block, "VALUE", thread, sprite, amount)) return BlockResult::REPEAT;

    std::string effect = Scratch::getFieldValue(*block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        sprite->colorEffect = amount.get<double>();
    } else if (effect == "FISHEYE") {
        sprite->fisheyeEffect = amount.get<double>();
    } else if (effect == "WHIRL") {
        sprite->whirlEffect = amount.get<double>();
    } else if (effect == "PIXELATE") {
        sprite->pixelateEffect = amount.get<double>();
    } else if (effect == "MOSAIC") {
        sprite->mosaicEffect = amount.get<double>();
    } else if (effect == "BRIGHTNESS") {
        sprite->brightnessEffect = std::clamp(amount.get<double>(), -100.0, 100.0);
    } else if (effect == "GHOST") {
        sprite->ghostEffect = std::clamp(amount.get<double>(), 0.0, 100.0);
    }

    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changeeffectby) {
    double amount;
    if (!Scratch::getInputValueAs(block, "CHANGE", thread, sprite, amount)) return BlockResult::REPEAT;

    std::string effect = Scratch::getFieldValue(*block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);

    if (effect == "COLOR") {
        sprite->colorEffect += amount;
    } else if (effect == "FISHEYE") {
        sprite->fisheyeEffect += amount;
    } else if (effect == "WHIRL") {
        sprite->whirlEffect += amount;
    } else if (effect == "PIXELATE") {
        sprite->pixelateEffect += amount;
    } else if (effect == "MOSAIC") {
        sprite->mosaicEffect += amount;
    } else if (effect == "BRIGHTNESS") {
        sprite->brightnessEffect += amount;
        sprite->brightnessEffect = std::clamp(sprite->brightnessEffect, -100.0f, 100.0f);
    } else if (effect == "GHOST") {
        sprite->ghostEffect += amount;
        sprite->ghostEffect = std::clamp(sprite->ghostEffect, 0.0f, 100.0f);
    }

    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, cleargraphiceffects) {
    sprite->ghostEffect = 0.0f;
    sprite->colorEffect = 0.0f;
    sprite->brightnessEffect = 0.0f;

    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, size) {
    *outValue = Value(std::round(sprite->size));
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, costumenumbername) {
    const std::string value = Scratch::getFieldValue(*block, "NUMBER_NAME");

    if (value == "name") *outValue = Value(sprite->costumes[sprite->currentCostume].name);
    else if (value == "number") *outValue = Value(sprite->currentCostume + 1);

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, backdropnumbername) {
    const std::string value = Scratch::getFieldValue(*block, "NUMBER_NAME");

    if (value == "name") *outValue = Value(Scratch::stageSprite->costumes[Scratch::stageSprite->currentCostume].name);
    if (value == "number") *outValue = Value(Scratch::stageSprite->currentCostume + 1);

    return BlockResult::CONTINUE;
}

SCRATCH_SHADOW_BLOCK(looks_costume, COSTUME)
SCRATCH_SHADOW_BLOCK(looks_backdrops, BACKDROP)
