#include "blockUtils.hpp"
#include "runtime.hpp"
#include <audio.hpp>
#include <audiostack.hpp>
#include <log.hpp>
#include <math.hpp>
#include <types.hpp>
#include <unzip.hpp>
#include <value.hpp>

bool Scratch::resolveSoundName(Sprite *sprite, const Value &soundValue, std::string &outFullName) {
    if (soundValue.isString()) {
        for (const Sound &sound : sprite->sounds) {
            if (sound.name == soundValue.get<std::string>()) {
                outFullName = sound.fullName;
                return true;
            }
        }
    }

    if (soundValue.isNaN() || !soundValue.isNumeric() || sprite->sounds.empty()) return false;
    double index = std::trunc(soundValue.get<double>());
    double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
    outFullName = sprite->sounds[soundIndex].fullName;
    return true;
}

void Scratch::startSoundUntilDone(Sprite *sprite, const std::string &fullName) {
#ifdef ENABLE_AUDIO
    SoundStream *strm;
    if (Scratch::projectType == ProjectType::UNZIPPED)
        strm = new SoundStream(fullName);
    else
        strm = new SoundStream(Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, fullName);
    if (strm->error.has_value()) {
        Log::logError("[Sound] " + strm->error.value());
        delete strm;
    }

    Mixer::setSoundVolume(fullName, sprite->volume);
#endif
}

SCRATCH_BLOCK(sound, playuntildone) {
#ifdef ENABLE_AUDIO
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) {
        Value soundValue;
        if (!Scratch::getInputValue(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

        if (!Scratch::resolveSoundName(sprite, soundValue, state->name)) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }

        Scratch::startSoundUntilDone(sprite, state->name);

        state->completedSteps = 1;
        return BlockResult::REPEAT;
    }

    if (!state->name.empty() && Mixer::isSoundPlaying(state->name)) return BlockResult::REPEAT;

    Mixer::setAutoClean(state->name, true);

    thread->eraseState(block);
#endif
    return BlockResult::CONTINUE;
}
SCRATCH_BYTECODE_BLOCK(sound, playuntildone) {
    outFirstInstr = compiler.here();
    compiler.compileInput(block, "SOUND_MENU", BytecodeStackKind::Value);
    compiler.emit(BytecodeOp::PlaySoundUntilDone);
    return true;
}

SCRATCH_BLOCK(sound, play) {
#ifdef ENABLE_AUDIO
    Value soundValue;
    if (!Scratch::getInputValue(block, "SOUND_MENU", thread, sprite, soundValue)) return BlockResult::REPEAT;

    // Find sound by name first
    std::string soundFullName;
    bool soundFound = false;

    if (soundValue.isString()) {
        for (const Sound &sound : sprite->sounds) {
            if (sound.name == soundValue.get<std::string>()) {
                soundFullName = sound.fullName;
                soundFound = true;
                break;
            }
        }
    }

    // If not found by name and input is a number, try index-based lookup
    if (!soundFound) {
        if (soundValue.isNaN() || !soundValue.isNumeric() || sprite->sounds.empty()) return BlockResult::CONTINUE;
        double index = std::trunc(soundValue.get<double>());
        double soundIndex = index - (std::floor((index - 1) / sprite->sounds.size()) * sprite->sounds.size()) - 1;
        soundFullName = sprite->sounds[soundIndex].fullName;
        soundFound = true;
    }

    if (soundFound) {
        SoundStream *strm;
        if (Scratch::projectType == ProjectType::UNZIPPED)
            strm = new SoundStream(soundFullName, false);
        else
            strm = new SoundStream(Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, soundFullName);
        if (strm->error.has_value()) {
            Log::logError("[Sound] " + strm->error.value());
            delete strm;
            return BlockResult::CONTINUE;
        }

        Mixer::setSoundVolume(soundFullName, sprite->volume);
        Mixer::setAutoClean(soundFullName, true);
    }
#endif
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sound, play, {"SOUND_MENU", BytecodeStackKind::Value});

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
SCRATCH_BLOCK_SIGNATURE(sound, stopallsounds);

SCRATCH_BLOCK(sound, changeeffectby) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) return BlockResult::CONTINUE;

    double amount;
    if (!Scratch::getInputValueAs(block, "VALUE", thread, sprite, amount)) return BlockResult::REPEAT;

    const std::string effect = Scratch::getFieldValue(*block, "EFFECT");

    if (effect == "PITCH") {
        sprite->pitch += amount;
        sprite->pitch = std::clamp(sprite->pitch, -360.0f, 360.0f);
        for (Sound sound : sprite->sounds) {
            Mixer::setPitch(sound.fullName, sprite->pitch);
        }
    } else if (effect == "PAN") {
        sprite->pan += amount;
        sprite->pan = std::clamp(sprite->pan, -100.0f, 100.0f);
        for (Sound sound : sprite->sounds) {
            Mixer::setPan(sound.fullName, sprite->pan);
        }
    }
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(sound, seteffectto) {
    double amount;
    if (!Scratch::getInputValueAs(block, "VALUE", thread, sprite, amount)) return BlockResult::REPEAT;

    const std::string effect = Scratch::getFieldValue(*block, "EFFECT");

    if (effect == "PITCH") {
        sprite->pitch = amount;
        sprite->pitch = std::clamp(sprite->pitch, -360.0f, 360.0f);
        for (Sound sound : sprite->sounds) {
            Mixer::setPitch(sound.fullName, sprite->pitch);
        }
    } else if (effect == "PAN") {
        sprite->pan = amount;
        sprite->pan = std::clamp(sprite->pan, -100.0f, 100.0f);
        for (Sound sound : sprite->sounds) {
            Mixer::setPan(sound.fullName, sprite->pan);
        }
    }
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
SCRATCH_BLOCK_SIGNATURE(sound, cleareffects);

SCRATCH_BLOCK(sound, changevolumeby) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) {
        state->completedSteps = 0;
        return BlockResult::CONTINUE;
    }

    double volume;
    if (!Scratch::getInputValueAs(block, "VOLUME", thread, sprite, volume)) return BlockResult::REPEAT;

    double inputValue = volume;
    sprite->volume = std::clamp(sprite->volume + inputValue, 0.0, 100.0);
    for (Sound sound : sprite->sounds) {
        Mixer::setSoundVolume(sound.fullName, sprite->volume + inputValue);
    }
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK(sound, setvolumeto) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) {
        state->completedSteps = 0;
        return BlockResult::CONTINUE;
    }

    double volume;
    if (!Scratch::getInputValueAs(block, "VOLUME", thread, sprite, volume)) return BlockResult::REPEAT;

    const double inputValue = std::clamp(volume, 0.0, 100.0);
    for (Sound sound : sprite->sounds) {
        Mixer::setSoundVolume(sound.fullName, inputValue);
    }
    sprite->volume = inputValue;
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}

SCRATCH_BLOCK_DOUBLE(sound, volume) {
    *outValue = sprite->volume;
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sound, volume);

SCRATCH_SHADOW_BLOCK(sound_sounds_menu, SOUND_MENU)
