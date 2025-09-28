#include "sound.hpp"
#include "audio.hpp"
#include "blockExecutor.hpp"
#include "interpret.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"

BlockResult SoundBlocks::playSoundUntilDone(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);
    std::string inputString = inputValue.asString();

    // if no blocks are inside the input
    auto inputFind = block.parsedInputs->find("SOUND_MENU");
    if (inputFind != block.parsedInputs->end() && inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputValue.asString());
        if (inputBlock != nullptr) {
            inputString = Scratch::getFieldValue(*inputBlock, "SOUND_MENU");
        }
    }

    if (block.repeatTimes != -1 && !fromRepeat) {
        block.repeatTimes = -1;
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        // Find sound by name first
        std::string soundFullName;
        bool soundFound = false;

        auto soundFind = sprite->sounds.find(inputString);
        if (soundFind != sprite->sounds.end()) {
            soundFullName = soundFind->second.fullName;
            soundFound = true;
        }

        // If not found by name and input is a number, try index-based lookup
        if (!soundFound && Math::isNumber(inputString) && inputFind != block.parsedInputs->end() &&
            (inputFind->second.inputType == ParsedInput::BLOCK || inputFind->second.inputType == ParsedInput::VARIABLE)) {
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
    auto soundFind = sprite->sounds.find(inputString);
    if (soundFind != sprite->sounds.end()) {
        checkSoundName = soundFind->second.fullName;
    } else if (Math::isNumber(inputString) && inputFind != block.parsedInputs->end() &&
               (inputFind->second.inputType == ParsedInput::BLOCK || inputFind->second.inputType == ParsedInput::VARIABLE)) {
        int soundIndex = inputValue.asInt() - 1;
        if (soundIndex >= 0 && static_cast<size_t>(soundIndex) < sprite->sounds.size()) {
            auto it = sprite->sounds.begin();
            std::advance(it, soundIndex);
            checkSoundName = it->second.fullName;
        }
    }

    if (!checkSoundName.empty() && SoundPlayer::isSoundPlaying(checkSoundName)) {
        return BlockResult::RETURN;
    }

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::playSound(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);
    std::string inputString = inputValue.asString();

    // if no blocks are inside the input
    auto inputFind = block.parsedInputs->find("SOUND_MENU");
    if (inputFind != block.parsedInputs->end() && inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputValue.asString());
        if (inputBlock != nullptr) {
            inputString = Scratch::getFieldValue(*inputBlock, "SOUND_MENU");
        }
    }

    // Find sound by name first
    std::string soundFullName;
    bool soundFound = false;

    auto soundFind = sprite->sounds.find(inputString);
    if (soundFind != sprite->sounds.end()) {
        soundFullName = soundFind->second.fullName;
        soundFound = true;
    }

    // If not found by name and input is a number, try index-based lookup
    if (!soundFound && Math::isNumber(inputString) && inputFind != block.parsedInputs->end() &&
        (inputFind->second.inputType == ParsedInput::BLOCK || inputFind->second.inputType == ParsedInput::VARIABLE)) {
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

BlockResult SoundBlocks::stopAllSounds(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    for (auto &[id, sound] : sprite->sounds) {
        SoundPlayer::stopSound(sound.fullName);
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
    for (auto &[id, sound] : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, sprite->volume + inputValue.asDouble());
        sprite->volume = SoundPlayer::getSoundVolume(sound.fullName);
    }
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::setVolumeTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value inputValue = Scratch::getInputValue(block, "VOLUME", sprite);
    for (auto &[id, sound] : sprite->sounds) {
        SoundPlayer::setSoundVolume(sound.fullName, inputValue.asDouble());
    }
    sprite->volume = inputValue.asDouble();
    return BlockResult::CONTINUE;
}

Value SoundBlocks::volume(Block &block, Sprite *sprite) {
    return Value(sprite->volume);
}
