#include "blockUtils.hpp"
#include <audio.hpp>
#include <math.hpp>
#include <sprite.hpp>
#include <unzip.hpp>
#include <value.hpp>

SCRATCH_BLOCK(sound, playuntildone) {
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
            if (!SoundPlayer::isSoundLoaded(soundFullName))
                SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, soundFullName);
            else
                SoundPlayer::playSound(soundFullName);
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

    if (!checkSoundName.empty() && SoundPlayer::isSoundPlaying(checkSoundName)) return BlockResult::RETURN;

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, play) {
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
        if (!SoundPlayer::isSoundLoaded(soundFullName))
            SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, soundFullName);
        else
            SoundPlayer::playSound(soundFullName);
    }

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, stopallsounds) {
    for (auto &currentSprite : Scratch::sprites) {
        for (Sound sound : currentSprite->sounds) {
            SoundPlayer::stopSound(sound.fullName);
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK_NOP(sound, changeeffectby)

SCRATCH_BLOCK_NOP(sound, seteffectto)

SCRATCH_BLOCK_NOP(sound, cleareffects)

SCRATCH_BLOCK(sound, changevolumeby) {
    const Value inputValue = Scratch::getInputValue(block, "VOLUME", sprite);
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, sprite->volume + inputValue.asDouble());
        sprite->volume = SoundPlayer::getSoundVolume(sound.fullName);
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, setvolumeto) {
    const Value inputValue = Scratch::getInputValue(block, "VOLUME", sprite);
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, inputValue.asDouble());
    }
    sprite->volume = inputValue.asDouble();
    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(sound, volume) {
    return Value(sprite->volume);
}
