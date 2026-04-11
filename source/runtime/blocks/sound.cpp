#include "blockUtils.hpp"
#include <audio.hpp>
#include <math.hpp>
#include <sprite.hpp>
#include <unzip.hpp>
#include <value.hpp>

SCRATCH_BLOCK(sound, playuntildone) {
#ifdef ENABLE_AUDIO
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) {
        Value soundValue;
        if (!Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

        // Find sound by name first
        bool soundFound = false;

        if (soundValue.isString()) {
            for (const Sound &sound : sprite->sounds) {
                if (sound.name == soundValue.asString()) {
                    state->name = sound.fullName;
                    soundFound = true;
                    break;
                }
            }
        }

        // If not found by name and input is a number, try index-based lookup
        if (!soundFound) {
            if (soundValue.isNaN() || !soundValue.isNumeric()) return BlockResult::CONTINUE;
            double index = std::trunc(soundValue.asDouble());
            double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
            state->name = sprite->sounds[soundIndex].fullName;
            soundFound = true;
        }

        if (soundFound) {
            if (SoundPlayer::isSoundLoaded(state->name))
                SoundPlayer::playSound(state->name);
            else
                SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, state->name);
        }

        state->completedSteps = 1;
        return BlockResult::REPEAT;
    }
    std::string checkSoundName = state->name;

    if (!checkSoundName.empty() && SoundPlayer::isSoundPlaying(checkSoundName)) {
        return BlockResult::REPEAT;
    }
    thread->eraseState(block);
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, play) {
#ifdef ENABLE_AUDIO
    Value soundValue;
    if (!Scratch::getInput(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

    // Find sound by name first
    std::string soundFullName;
    bool soundFound = false;

    if (soundValue.isString()) {
        for (const Sound &sound : sprite->sounds) {
            if (sound.name == soundValue.asString()) {
                soundFullName = sound.fullName;
                soundFound = true;
                break;
            }
        }
    }

    // If not found by name and input is a number, try index-based lookup
    if (!soundFound) {
        if (soundValue.isNaN() || !soundValue.isNumeric()) return BlockResult::CONTINUE;
        double index = std::trunc(soundValue.asDouble());
        double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
        soundFullName = sprite->sounds[soundIndex].fullName;
        soundFound = true;
    }

    if (soundFound) {
        if (SoundPlayer::isSoundLoaded(soundFullName))
            SoundPlayer::playSound(soundFullName);
        else
            SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, soundFullName);
    }
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, stopallsounds) {
#ifdef ENABLE_AUDIO
    for (auto &currentSprite : Scratch::sprites) {
        for (Sound sound : currentSprite->sounds) {
            SoundPlayer::stopSound(sound.fullName);
        }
    }
#endif
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, changeeffectby) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) return BlockResult::CONTINUE;

    Value amount;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, amount)) return BlockResult::REPEAT;

    const std::string effect = Scratch::getFieldValue(*block, "EFFECT");

    if (effect == "PITCH") {
        sprite->pitch += amount.asDouble();
        sprite->pitch = std::clamp(sprite->pitch, -360.0f, 360.0f);
        for (Sound sound : sprite->sounds) {
            SoundPlayer::setPitch(sound.fullName, sprite->pitch - 100.0f);
        }
    } else if (effect == "PAN") {
        sprite->pan += amount.asDouble();
        sprite->pan = std::clamp(sprite->pan, -100.0f, 100.0f);
        for (Sound sound : sprite->sounds) {
            SoundPlayer::setPan(sound.fullName, sprite->pan - 100.0f);
        }
    }
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(sound, seteffectto) {
    Value amount;
    if (!Scratch::getInput(block, "VALUE", thread, sprite, amount)) return BlockResult::REPEAT;

    const std::string effect = Scratch::getFieldValue(*block, "EFFECT");

    if (effect == "PITCH") {
        sprite->pitch = amount.asDouble();
        sprite->pitch = std::clamp(sprite->pitch, -360.0f, 360.0f);
        for (Sound sound : sprite->sounds) {
            SoundPlayer::setPitch(sound.fullName, sprite->pitch - 100.0f);
        }
    } else if (effect == "PAN") {
        sprite->pan = amount.asDouble();
        sprite->pan = std::clamp(sprite->pan, -100.0f, 100.0f);
        for (Sound sound : sprite->sounds) {
            SoundPlayer::setPan(sound.fullName, sprite->pan - 100.0f);
        }
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, cleareffects) {
    sprite->pitch = 100.0f;
    sprite->pan = 100.0f;
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setPitch(sound.fullName, sprite->pitch - 100.0f);
        SoundPlayer::setPan(sound.fullName, sprite->pan - 100.0f);
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, changevolumeby) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) return BlockResult::CONTINUE;

    Value volume;
    if (!Scratch::getInput(block, "VOLUME", thread, sprite, volume)) return BlockResult::REPEAT;

    double inputValue = volume.asDouble();
    sprite->volume = std::clamp(sprite->volume + inputValue, 0.0, 100.0);
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, sprite->volume + inputValue);
    }

    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(sound, setvolumeto) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) return BlockResult::CONTINUE;

    Value volume;
    if (!Scratch::getInput(block, "VOLUME", thread, sprite, volume)) return BlockResult::REPEAT;

    const double inputValue = std::clamp(volume.asDouble(), 0.0, 100.0);
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, inputValue);
    }
    sprite->volume = inputValue;

    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(sound, volume) {
    *outValue = Value(sprite->volume);
    return BlockResult::CONTINUE;
}

SCRATCH_SHADOW_BLOCK(sound_sounds_menu, SOUND_MENU)
