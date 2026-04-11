#include "blockUtils.hpp"
#include "runtime.hpp"
#include <audio.hpp>
#include <audiostack.hpp>
#include <math.hpp>
#include <sprite.hpp>
#include <unzip.hpp>
#include <value.hpp>

SCRATCH_BLOCK(sound, playuntildone) {
#ifdef ENABLE_AUDIO
    Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);
    if (!fromRepeat) {

        // Find sound by name first
        std::string soundFullName;
        bool soundFound = false;

        if (inputValue.isString()) {
            for (const Sound &sound : sprite->sounds) {
                if (sound.name == inputValue.asString()) {
                    soundFullName = sound.fullName;
                    soundFound = true;
                    break;
                }
            }
        }

        // If not found by name and input is a number, try index-based lookup
        if (!soundFound) {
            if (inputValue.isNaN() || !inputValue.isNumeric()) return BlockResult::CONTINUE;
            double index = std::trunc(inputValue.asDouble());
            double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
            soundFullName = sprite->sounds[soundIndex].fullName;
            soundFound = true;
        }

        if (soundFound) {
            if (Mixer::isSoundPlaying(soundFullName)) {
                // nothing
            } else {
                SoundStream *strm;
                if (Scratch::projectType == ProjectType::UNZIPPED)
                    strm = new SoundStream(soundFullName);
                else
                    strm = new SoundStream(&Unzip::zipArchive, soundFullName);
            }

        }

        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    // Check if sound is still playing (need to determine sound name again for check)
    std::string checkSoundName;
    for (const Sound &sound : sprite->sounds) {
        if (sound.name == inputValue.asString()) {
            checkSoundName = sound.fullName;
            break;
        }
    }
    if (checkSoundName.empty() || !inputValue.isString()) {
        double index = std::trunc(inputValue.asDouble());
        double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
        checkSoundName = sprite->sounds[soundIndex].fullName;
    }

    if (!checkSoundName.empty() && Mixer::isSoundPlaying(checkSoundName)) return BlockResult::RETURN;

    Mixer::setAutoClean(checkSoundName, true);

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, play) {
#ifdef ENABLE_AUDIO
    const Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);

    // Find sound by name first
    std::string soundFullName;
    bool soundFound = false;

    if (inputValue.isString()) {
        for (const Sound &sound : sprite->sounds) {
            if (sound.name == inputValue.asString()) {
                soundFullName = sound.fullName;
                soundFound = true;
                break;
            }
        }
    }

    // If not found by name and input is a number, try index-based lookup
    if (!soundFound) {
        if (inputValue.isNaN() || !inputValue.isNumeric()) return BlockResult::CONTINUE;
        double index = std::trunc(inputValue.asDouble());
        double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
        soundFullName = sprite->sounds[soundIndex].fullName;
        soundFound = true;
    }

    if (soundFound) {
        SoundStream *strm;
        if (Scratch::projectType == ProjectType::UNZIPPED)
            strm = new SoundStream(soundFullName, false);
        else
            strm = new SoundStream(&Unzip::zipArchive, soundFullName);

	Mixer::setAutoClean(soundFullName, true);
    }
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, stopallsounds) {
#ifdef ENABLE_AUDIO
    for (auto &currentSprite : Scratch::sprites) {
        for (Sound sound : currentSprite->sounds) {
            Mixer::setAutoClean(sound.fullName, true);
            Mixer::stopSound(sound.fullName);
        }
    }
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, changeeffectby) {
    if (!fromRepeat) {
        const std::string effect = Scratch::getFieldValue(block, "EFFECT");
        const Value amount = Scratch::getInputValue(block, "VALUE", sprite);

        if (effect == "PITCH") {
            sprite->pitch += amount.asDouble();
            sprite->pitch = std::clamp(sprite->pitch, -360.0f, 360.0f);
            for (Sound sound : sprite->sounds) {
                Mixer::setPitch(sound.fullName, sprite->pitch);
            }
        } else if (effect == "PAN") {
            sprite->pan += amount.asDouble();
            sprite->pan = std::clamp(sprite->pan, -100.0f, 100.0f);
            for (Sound sound : sprite->sounds) {
                Mixer::setPan(sound.fullName, sprite->pan);
            }
        }
        BlockExecutor::addToRepeatQueue(sprite, &block);
        return BlockResult::RETURN;
    }
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, seteffectto) {
    if (!fromRepeat) {
        const std::string effect = Scratch::getFieldValue(block, "EFFECT");
        const Value amount = Scratch::getInputValue(block, "VALUE", sprite);

        if (effect == "PITCH") {
            sprite->pitch = amount.asDouble();
            sprite->pitch = std::clamp(sprite->pitch, -360.0f, 360.0f);
            for (Sound sound : sprite->sounds) {
                Mixer::setPitch(sound.fullName, sprite->pitch);
            }
        } else if (effect == "PAN") {
            sprite->pan = amount.asDouble();
            sprite->pan = std::clamp(sprite->pan, -100.0f, 100.0f);
            for (Sound sound : sprite->sounds) {
                Mixer::setPan(sound.fullName, sprite->pan);
            }
        }
        BlockExecutor::addToRepeatQueue(sprite, &block);
        return BlockResult::RETURN;
    }
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, cleareffects) {
    sprite->pitch = 0.0f;
    sprite->pan = 0.0f;
    for (Sound sound : sprite->sounds) {
        Mixer::setPitch(sound.fullName, sprite->pitch);
        Mixer::setPan(sound.fullName, sprite->pan);
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, changevolumeby) {
    if (!fromRepeat) {
        double inputValue = Scratch::getInputValue(block, "VOLUME", sprite).asDouble();
        inputValue = std::clamp(inputValue, 0.0, 100.0);
        for (Sound sound : sprite->sounds) {
            Mixer::setSoundVolume(sound.fullName, sprite->volume + inputValue);
            sprite->volume = Mixer::getSoundVolume(sound.fullName);
        }
        BlockExecutor::addToRepeatQueue(sprite, &block);
        return BlockResult::RETURN;
    }
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, setvolumeto) {
    if (!fromRepeat) {
        const double inputValue = std::clamp(Scratch::getInputValue(block, "VOLUME", sprite).asDouble(), 0.0, 100.0);
        for (Sound sound : sprite->sounds) {
            Mixer::setSoundVolume(sound.fullName, inputValue);
        }
        sprite->volume = inputValue;
        BlockExecutor::addToRepeatQueue(sprite, &block);
        return BlockResult::RETURN;
    }
    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(sound, volume) {
    return Value(sprite->volume);
}
