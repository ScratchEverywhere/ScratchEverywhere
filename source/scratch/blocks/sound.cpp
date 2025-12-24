#include "sound.hpp"
#include "audio.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"

BlockResult SoundBlocks::playSoundUntilDone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
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

    if (!checkSoundName.empty() && SoundPlayer::isSoundPlaying(checkSoundName)) {
        return BlockResult::RETURN;
    }

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::playSound(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);

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

BlockResult SoundBlocks::stopAllSounds(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    for (auto &currentSprite : sprites) {
        for (Sound sound : currentSprite->sounds) {
            SoundPlayer::stopSound(sound.fullName);
        }
    }
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::changeEffectBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::setEffectTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::clearSoundEffects(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::changeVolumeBy(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "VOLUME", sprite);
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, sprite->volume + inputValue.asDouble());
        sprite->volume = SoundPlayer::getSoundVolume(sound.fullName);
    }
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::setVolumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "VOLUME", sprite);
    for (Sound sound : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, inputValue.asDouble());
    }
    sprite->volume = inputValue.asDouble();
    return BlockResult::CONTINUE;
}

Value SoundBlocks::volume(Block &block, Sprite *sprite) {
    return Value(sprite->volume);
}
