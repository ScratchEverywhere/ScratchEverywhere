#include "blockUtils.hpp"
#include "runtime.hpp"
#include <audio.hpp>
#include <audiostack.hpp>
#include <log.hpp>
#include <math.hpp>
#include <types.hpp>
#include <unordered_set>
#include <unzip.hpp>
#include <value.hpp>

static std::unordered_set<std::string> &failedSounds() {
    static std::unordered_set<std::string> s;
    return s;
}

bool Scratch::resolveSoundName(Sprite *sprite, const Value &soundValue, std::string &outFullName) {
    if (soundValue.isStringTyped()) {
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
    if (failedSounds().count(fullName)) return;
    SoundStream *strm;
    if (Scratch::projectType == ProjectType::UNZIPPED)
        strm = new SoundStream(fullName);
    else
#ifdef __XBOX__
        strm = new SoundStream(Unzip::zipArchive ? Unzip::zipArchive.get() : nullptr, fullName);
#else
        strm = new SoundStream(Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, fullName);
#endif
    if (strm->error.has_value()) {
        Log::logError("[Sound] " + strm->error.value());
        failedSounds().insert(fullName);
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

    if (soundValue.isStringTyped()) {
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
        if (failedSounds().count(soundFullName)) return BlockResult::CONTINUE;
        SoundStream *strm;
        if (Scratch::projectType == ProjectType::UNZIPPED)
            strm = new SoundStream(soundFullName, false);
        else
#ifdef __XBOX__
            strm = new SoundStream(Unzip::zipArchive ? Unzip::zipArchive.get() : nullptr, soundFullName);
#else
            strm = new SoundStream(Scratch::sb3InRam ? Unzip::zipArchive.get() : nullptr, soundFullName);
#endif
        if (strm->error.has_value()) {
            Log::logError("[Sound] " + strm->error.value());
            failedSounds().insert(soundFullName);
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

SCRATCH_BLOCK_POSITIONAL_VOID(sound, stopallsounds) {
#ifdef ENABLE_AUDIO
    for (auto &currentSprite : Scratch::sprites) {
        for (Sound sound : currentSprite->sounds) {
            Mixer::setAutoClean(sound.fullName, true);
            Mixer::stopSound(sound.fullName);
        }
    }
#endif
}
SCRATCH_BLOCK(sound, stopallsounds) {
    block_sound_stopallsounds_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sound, stopallsounds);

bool updateSoundEffect(Sprite *sprite, std::string effect, double amount, bool change) {
    std::transform(effect.begin(), effect.end(), effect.begin(), ::tolower);

    if (effect == "pitch") {
        sprite->pitch = std::clamp(static_cast<float>(change ? sprite->pitch + amount : amount), -360.0f, 360.0f);
        for (Sound sound : sprite->sounds) {
            Mixer::setPitch(sound.fullName, sprite->pitch);
        }
        return true;
    }
    if (effect == "pan") {
        sprite->pan = std::clamp(static_cast<float>(change ? sprite->pan + amount : amount), -100.0f, 100.0f);
        for (Sound sound : sprite->sounds) {
            Mixer::setPan(sound.fullName, sprite->pan);
        }
        return true;
    }
    return false;
}

SCRATCH_BLOCK(sound, changeeffectby) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    double amount;
    std::string effect;
    if (!Scratch::getInputValueAs(block, "VALUE", thread, sprite, amount) ||
        !Scratch::getInputValueAs(block, "EFFECT", thread, sprite, effect)) return BlockResult::REPEAT;

    if (!updateSoundEffect(sprite, effect, amount, true)) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}
SCRATCH_BLOCK_BLOCKING_SIGNATURE(sound, changeeffectby, {"VALUE", BytecodeStackKind::Number}, {"EFFECT", BytecodeStackKind::String});

SCRATCH_BLOCK(sound, seteffectto) {
    BlockState *state = thread->getState(block);
    if (state->completedSteps != 0) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }

    double amount;
    std::string effect;
    if (!Scratch::getInputValueAs(block, "VALUE", thread, sprite, amount) ||
        !Scratch::getInputValueAs(block, "EFFECT", thread, sprite, effect)) return BlockResult::REPEAT;

    if (!updateSoundEffect(sprite, effect, amount, false)) {
        thread->eraseState(block);
        return BlockResult::CONTINUE;
    }
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}
SCRATCH_BLOCK_BLOCKING_SIGNATURE(sound, seteffectto, {"VALUE", BytecodeStackKind::Number}, {"EFFECT", BytecodeStackKind::String});

SCRATCH_BLOCK_POSITIONAL_VOID(sound, cleareffects) {
    sprite->pitch = 0.0f;
    sprite->pan = 0.0f;
    for (Sound sound : sprite->sounds) {
        Mixer::setPitch(sound.fullName, sprite->pitch);
        Mixer::setPan(sound.fullName, sprite->pan);
    }
}
SCRATCH_BLOCK(sound, cleareffects) {
    block_sound_cleareffects_pos_(sprite);
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
        Mixer::setSoundVolume(sound.fullName, sprite->volume);
    }
    state->completedSteps = 1;
    return BlockResult::REPEAT;
}
SCRATCH_BLOCK_BLOCKING_SIGNATURE(sound, changevolumeby, {"VOLUME", BytecodeStackKind::Number});

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
SCRATCH_BLOCK_BLOCKING_SIGNATURE(sound, setvolumeto, {"VOLUME", BytecodeStackKind::Number});

SCRATCH_BLOCK_POSITIONAL_SPRITE_N(sound, volume) {
    return sprite->volume;
}
SCRATCH_BLOCK_DOUBLE(sound, volume) {
    *outValue = block_sound_volume_pos_(sprite);
    return BlockResult::CONTINUE;
}
SCRATCH_BLOCK_SIGNATURE(sound, volume);

SCRATCH_SHADOW_BLOCK(sound_sounds_menu, SOUND_MENU)
