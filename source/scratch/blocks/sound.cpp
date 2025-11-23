#include "audio.hpp"
#include "blockUtils.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"

SCRATCH_BLOCK(sound, playuntildone) {
    const Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);

    if (block.repeatTimes != -1 && !fromRepeat) block.repeatTimes = -1;

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        // Find sound by name first
        std::string soundFullName;
        bool soundFound = false;

        auto soundFind = sprite->sounds.find(inputValue.asString());
        if (soundFind != sprite->sounds.end()) {
            soundFullName = soundFind->second.fullName;
            soundFound = true;
        }

        // If not found by name and input is a number, try index-based lookup
        if (!soundFound && Math::isNumber(inputValue.asString())) {
            int soundIndex = inputValue.asInt() - 1;
            if (soundIndex >= 0 && static_cast<size_t>(soundIndex) < sprite->sounds.size()) {
                auto it = sprite->sounds.begin();
                std::advance(it, soundIndex);
                soundFullName = it->second.fullName;
                soundFound = true;
            }
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
    auto soundFind = sprite->sounds.find(inputValue.asString());
    if (soundFind != sprite->sounds.end()) {
        checkSoundName = soundFind->second.fullName;
    } else if (Math::isNumber(inputValue.asString())) {
        int soundIndex = inputValue.asInt() - 1;
        if (soundIndex >= 0 && static_cast<size_t>(soundIndex) < sprite->sounds.size()) {
            auto it = sprite->sounds.begin();
            std::advance(it, soundIndex);
            checkSoundName = it->second.fullName;
        }
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

    auto soundFind = sprite->sounds.find(inputValue.asString());
    if (soundFind != sprite->sounds.end()) {
        soundFullName = soundFind->second.fullName;
        soundFound = true;
    }

    // If not found by name and input is a number, try index-based lookup
    if (!soundFound && Math::isNumber(inputValue.asString())) {
        int soundIndex = inputValue.asInt() - 1;
        if (soundIndex >= 0 && static_cast<size_t>(soundIndex) < sprite->sounds.size()) {
            auto it = sprite->sounds.begin();
            std::advance(it, soundIndex);
            soundFullName = it->second.fullName;
            soundFound = true;
        }
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
    for (auto &currentSprite : sprites) {
        for (auto &[id, sound] : currentSprite->sounds) {
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
    for (auto &[id, sound] : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, sprite->volume + inputValue.asDouble());
        sprite->volume = SoundPlayer::getSoundVolume(sound.fullName);
    }
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(sound, setvolumeto) {
    const Value inputValue = Scratch::getInputValue(block, "VOLUME", sprite);
    for (auto &[id, sound] : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, inputValue.asDouble());
    }
    sprite->volume = inputValue.asDouble();
    return BlockResult::CONTINUE;
}

SCRATCH_REPORTER_BLOCK(sound, volume) {
    return Value(sprite->volume);
}
