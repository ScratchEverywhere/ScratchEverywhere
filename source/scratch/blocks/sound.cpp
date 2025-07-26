#include "sound.hpp"
#include "../audio.hpp"
#include "../unzip.hpp"

BlockResult SoundBlocks::playSoundUntilDone(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {

    Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);
    std::string inputString = inputValue.asString();

    // if no blocks are inside the input
    auto inputFind = block.parsedInputs.find("SOUND_MENU");
    if (inputFind != block.parsedInputs.end() && inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputValue.asString());
        if (inputBlock != nullptr) {
            inputString = inputBlock->fields["SOUND_MENU"][0].get<std::string>();
        }
    }

    if (block.repeatTimes == -1) {
        block.repeatTimes = -2;

        // stop playing the same sound if it's already playing
        if (SoundPlayer::isSoundPlaying(sprite->sounds[inputString].fullName)) {
            SoundPlayer::stopSound(sprite->sounds[inputString].fullName);
        }

        auto soundFind = sprite->sounds.find(inputString);
        if (soundFind != sprite->sounds.end()) {
            const Sound *sound = &soundFind->second;
            if (!SoundPlayer::isSoundLoaded(sprite->sounds[inputString].fullName))
                SoundPlayer::startSB3SoundLoaderThread(sprite, &Unzip::zipArchive, sound->fullName);
            else
                SoundPlayer::playSound(sprite->sounds[inputString].fullName);
        }

        BlockExecutor::addToRepeatQueue(sprite, &block);
    }

    if (SoundPlayer::isSoundPlaying(sprite->sounds[inputString].fullName)) {
        return BlockResult::RETURN;
    }
    BlockExecutor::removeFromRepeatQueue(sprite, &block);

    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::playSound(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {

    Value inputValue = Scratch::getInputValue(block, "SOUND_MENU", sprite);
    std::string inputString = inputValue.asString();

    // if no blocks are inside the input
    auto inputFind = block.parsedInputs.find("SOUND_MENU");
    if (inputFind != block.parsedInputs.end() && inputFind->second.inputType == ParsedInput::LITERAL) {
        Block *inputBlock = findBlock(inputValue.asString());
        if (inputBlock != nullptr) {
            inputString = inputBlock->fields["SOUND_MENU"][0].get<std::string>();
        }
    }

    // stop playing the same sound if it's already playing
    if (SoundPlayer::isSoundPlaying(sprite->sounds[inputString].fullName)) {
        SoundPlayer::stopSound(sprite->sounds[inputString].fullName);
    }

    auto soundFind = sprite->sounds.find(inputString);
    if (soundFind != sprite->sounds.end()) {
        const Sound *sound = &soundFind->second;
        if (!SoundPlayer::isSoundLoaded(sprite->sounds[inputString].fullName))
            SoundPlayer::startSB3SoundLoaderThread(sprite, &Unzip::zipArchive, sound->fullName);
        else
            SoundPlayer::playSound(sprite->sounds[inputString].fullName);
    }

    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::stopAllSounds(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    for (auto &[id, sound] : sprite->sounds) {
        SoundPlayer::stopSound(sound.fullName);
    }
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::changeEffectBy(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::setEffectTo(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::clearSoundEffects(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::changeVolumeBy(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}

BlockResult SoundBlocks::setVolumeTo(Block &block, Sprite *sprite, Block **waitingBlock, bool *withoutScreenRefresh) {
    return BlockResult::CONTINUE;
}

Value SoundBlocks::volume(Block &block, Sprite *sprite) {
    return Value(sprite->volume);
}