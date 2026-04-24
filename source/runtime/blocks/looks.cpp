#include "blockExecutor.hpp"
#include "blockUtils.hpp"
#include "runtime.hpp"
#include <algorithm>
#include <cstddef>
#include <image.hpp>
#include <render.hpp>
#include <set>
#include <speech_manager.hpp>
#include <sprite.hpp>
#include <value.hpp>

SCRATCH_BLOCK(looks, say) {
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;

    Value messageValue;
    if (!Scratch::getInput(block, "MESSAGE", thread, sprite, messageValue)) return BlockResult::REPEAT;

    std::string message = messageValue.asString();

    SpeechManager *speechManager = Render::getSpeechManager();

    speechManager->showSpeech(sprite, message, -1, "say");

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, sayforsecs) {
    BlockState *state = thread->getState(block);
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    SpeechManager *speechManager = Render::getSpeechManager();
    if (state->completedSteps == 0) {
        Value seconds, message;
        if (!Scratch::getInput(block, "SECS", thread, sprite, seconds) ||
            !Scratch::getInput(block, "MESSAGE", thread, sprite, message)) return BlockResult::REPEAT;

        state->waitDuration = seconds.asDouble() * 1000; // convert to milliseconds
        state->waitTimer.start();
        speechManager->showSpeech(sprite, message.asString(), seconds.asDouble(), "say");
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

    Value messageValue;
    if (!Scratch::getInput(block, "MESSAGE", thread, sprite, messageValue)) return BlockResult::REPEAT;

    std::string message = messageValue.asString();

    speechManager->showSpeech(sprite, message, -1, "think");

    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK(looks, thinkforsecs) {
    BlockState *state = thread->getState(block);
    if (!Render::createSpeechManager()) return BlockResult::CONTINUE;
    SpeechManager *speechManager = Render::getSpeechManager();
    if (state->completedSteps == 0) {
        Value seconds, message;
        if (!Scratch::getInput(block, "SECS", thread, sprite, seconds) ||
            !Scratch::getInput(block, "MESSAGE", thread, sprite, message)) return BlockResult::REPEAT;

        state->waitDuration = seconds.asDouble() * 1000; // convert to milliseconds
        state->waitTimer.start();

        speechManager->showSpeech(sprite, message.asString(), state->waitDuration, "think");
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
    if (!Scratch::getInput(block, "COSTUME", thread, sprite, costume)) return BlockResult::REPEAT;

    if (costume.isDouble()) {
        Scratch::switchCostume(sprite, costume.isNaN() ? 0 : costume.asDouble() - 1);
        return BlockResult::CONTINUE;
    }

    for (size_t i = 0; i < sprite->costumes.size(); i++) {
        if (sprite->costumes[i].name == costume.asString()) {
            Scratch::switchCostume(sprite, i);
            return BlockResult::CONTINUE;
        }
    }

    if (costume.asString() == "next costume") {
        Scratch::switchCostume(sprite, ++sprite->currentCostume);
        return BlockResult::CONTINUE;
    } else if (costume.asString() == "previous costume") {
        Scratch::switchCostume(sprite, --sprite->currentCostume);
        return BlockResult::CONTINUE;
    }

    if (costume.isNumeric()) {
        Scratch::switchCostume(sprite, costume.asDouble() - 1);
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
    if (!Scratch::getInput(block, "BACKDROP", thread, sprite, backdrop)) return BlockResult::REPEAT;

    if (backdrop.isDouble()) {
        Scratch::switchCostume(Scratch::stageSprite, backdrop.isNaN() ? 0 : backdrop.asDouble() - 1);
        goto end;
    }

    for (size_t i = 0; i < Scratch::stageSprite->costumes.size(); i++) {
        if (Scratch::stageSprite->costumes[i].name == backdrop.asString()) {
            Scratch::switchCostume(Scratch::stageSprite, i);
            goto end;
        }
    }

    if (backdrop.asString() == "next backdrop") {
        Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
        goto end;
    } else if (backdrop.asString() == "previous backdrop") {
        Scratch::switchCostume(Scratch::stageSprite, --Scratch::stageSprite->currentCostume);
        goto end;
    } else if (backdrop.asString() == "random backdrop") {
        if (Scratch::stageSprite->costumes.size() == 1) goto end;
        int randomIndex = std::rand() % (Scratch::stageSprite->costumes.size() - 1);
        if (randomIndex >= Scratch::stageSprite->currentCostume) randomIndex++;
        Scratch::switchCostume(Scratch::stageSprite, randomIndex);
        goto end;
    }

    if (backdrop.isNumeric()) {
        Scratch::switchCostume(Scratch::stageSprite, backdrop.asDouble() - 1);
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
        if (!Scratch::getInput(block, "BACKDROP", thread, sprite, backdrop)) return BlockResult::REPEAT;

        if (backdrop.isDouble()) {
            const double bk = backdrop.isNaN() ? 0 : backdrop.asDouble() - 1;
            if (bk < 0 || bk >= sprite->costumes.size()) return BlockResult::CONTINUE;
            Scratch::switchCostume(Scratch::stageSprite, bk);
        } else {
            bool found = false;
            for (size_t i = 0; i < Scratch::stageSprite->costumes.size(); i++) {
                if (Scratch::stageSprite->costumes[i].name == backdrop.asString()) {
                    Scratch::switchCostume(Scratch::stageSprite, i);
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (backdrop.asString() == "next backdrop") {
                    Scratch::switchCostume(Scratch::stageSprite, ++Scratch::stageSprite->currentCostume);
                    found = true;
                } else if (backdrop.asString() == "previous backdrop") {
                    Scratch::switchCostume(Scratch::stageSprite, --Scratch::stageSprite->currentCostume);
                    found = true;
                } else if (backdrop.asString() == "random backdrop") {
                    if (Scratch::stageSprite->costumes.size() > 1) {
                        int randomIndex = std::rand() % (Scratch::stageSprite->costumes.size() - 1);
                        if (randomIndex >= Scratch::stageSprite->currentCostume) randomIndex++;
                        Scratch::switchCostume(Scratch::stageSprite, randomIndex);
                        found = true;
                    }
                } else if (backdrop.isNumeric()) {
                    Scratch::switchCostume(Scratch::stageSprite, backdrop.asDouble() - 1);
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
    Value num;
    if (!Scratch::getInput(block, "NUM", thread, sprite, num)) return BlockResult::REPEAT;

    const std::string forwardBackward = Scratch::getFieldValue(*block, "FORWARD_BACKWARD");
    if (!num.isNumeric()) return BlockResult::CONTINUE;

    int shift = floor(num.asDouble());
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
    if (!Scratch::getInput(block, "SIZE", thread, sprite, size)) return BlockResult::REPEAT;

    const auto &costumeName = sprite->costumes[sprite->currentCostume].fullName;
    auto imgFind = Scratch::costumeImages.find(costumeName);
    if (imgFind == Scratch::costumeImages.end()) {
        static std::set<std::string> failedCostumes;
        if (failedCostumes.count(costumeName) == 0) {
            Log::logWarning("Invalid Image in current costume.");
            failedCostumes.insert(costumeName);
        }
        return BlockResult::CONTINUE;
    }

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size = size.asDouble();

        Render::resizeSVGs(sprite);
        return BlockResult::CONTINUE;
    }

    if (size.isNumeric()) {
        const double inputSizePercent = size.asDouble();

        const Costume &costume = sprite->costumes[sprite->currentCostume];
        const int sprWidth = sprite->spriteWidth / costume.bitmapResolution;
        const int sprHeight = sprite->spriteHeight / costume.bitmapResolution;

        const double minScale = std::min(1.0, std::max(5.0 / sprWidth, 5.0 / sprHeight));

        const double maxScale = std::min((1.5 * Scratch::projectWidth) / sprWidth, (1.5 * Scratch::projectHeight) / sprHeight);

        const double clampedScale = std::clamp(inputSizePercent / 100.0, minScale, maxScale);
        sprite->size = clampedScale * 100.0;

        Render::resizeSVGs(sprite);
    }
    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, changesizeby) {
    Value size;
    if (!Scratch::getInput(block, "CHANGE", thread, sprite, size)) return BlockResult::REPEAT;

    const auto &costumeName = sprite->costumes[sprite->currentCostume].fullName;
    auto imgFind = Scratch::costumeImages.find(costumeName);
    if (imgFind == Scratch::costumeImages.end()) {
        static std::set<std::string> failedCostumes;
        if (failedCostumes.count(costumeName) == 0) {
            Log::logWarning("Invalid Image in current costume.");
            failedCostumes.insert(costumeName);
        }
        return BlockResult::CONTINUE;
    }

    // hasn't been rendered yet, or fencing is disabled
    if ((sprite->spriteWidth < 1 || sprite->spriteHeight < 1) || !Scratch::fencing) {
        sprite->size += size.asDouble();

        Render::resizeSVGs(sprite);
        return BlockResult::CONTINUE;
    }

    if (size.isNumeric()) {
        sprite->size += size.asDouble();

        const Costume &costume = sprite->costumes[sprite->currentCostume];
        const int sprWidth = sprite->spriteWidth / costume.bitmapResolution;
        const int sprHeight = sprite->spriteHeight / costume.bitmapResolution;

        const double minScale = std::min(1.0, std::max(5.0 / sprWidth, 5.0 / sprHeight)) * 100.0;

        const double maxScale = std::min((1.5 * Scratch::projectWidth) / sprWidth, (1.5 * Scratch::projectHeight) / sprHeight) * 100.0;

        sprite->size = std::clamp(static_cast<double>(sprite->size), minScale, maxScale);

        Render::resizeSVGs(sprite);
    }
    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(looks, seteffectto) {
    Value amount;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, amount)) return BlockResult::REPEAT;

    std::string effect = Scratch::getFieldValue(*block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        static bool logged = false;
        if (!logged) {
            Log::logWarning("Color effect is not supported yet.");
            logged = true;
        }
    } else if (effect == "FISHEYE") {
        static bool logged = false;
        if (!logged) {
            Log::logWarning("Fisheye effect is not supported yet.");
            logged = true;
        }
    } else if (effect == "WHIRL") {
        static bool logged = false;
        if (!logged) {
            Log::logWarning("Whirl effect is not supported yet.");
            logged = true;
        }
    } else if (effect == "PIXELATE") {
        static bool logged = false;
        if (!logged) {
            Log::logWarning("Pixelate effect is not supported yet.");
            logged = true;
        }
    } else if (effect == "MOSAIC") {
        static bool logged = false;
        if (!logged) {
            Log::logWarning("Mosaic effect is not supported yet.");
            logged = true;
        }
    } else if (effect == "BRIGHTNESS") {
        sprite->brightnessEffect = std::clamp(amount.asDouble(), -100.0, 100.0);
    } else if (effect == "GHOST") {
        sprite->ghostEffect = std::clamp(amount.asDouble(), 0.0, 100.0);
    }

    if (sprite->visible) Scratch::forceRedraw = true;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK(looks, changeeffectby) {
    Value amount;
    if (!Scratch::getInput(block, "CHANGE", thread, sprite, amount)) return BlockResult::REPEAT;

    std::string effect = Scratch::getFieldValue(*block, "EFFECT");
    std::transform(effect.begin(), effect.end(), effect.begin(), ::toupper);

    if (!amount.isNumeric()) return BlockResult::CONTINUE;

    if (effect == "COLOR") {
        Log::logWarning("Color effect is not supported yet.");
    } else if (effect == "FISHEYE") {
        Log::logWarning("Fisheye effect is not supported yet.");
    } else if (effect == "WHIRL") {
        Log::logWarning("Whirl effect is not supported yet.");
    } else if (effect == "PIXELATE") {
        Log::logWarning("Pixelate effect is not supported yet.");
    } else if (effect == "MOSAIC") {
        Log::logWarning("Mosaic effect is not supported yet.");
    } else if (effect == "BRIGHTNESS") {
        sprite->brightnessEffect += amount.asDouble();
        sprite->brightnessEffect = std::clamp(sprite->brightnessEffect, -100.0f, 100.0f);
    } else if (effect == "GHOST") {
        sprite->ghostEffect += amount.asDouble();
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