#include "text2speech.hpp"
#include "../audio.hpp"
#include "blockExecutor.hpp"
#include "control.hpp"
#include "downloader.hpp"
#include "interpret.hpp"
#include "math.hpp"
#include "os.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"
#include <filesystem>
#include <string>

BlockResult SpeechBlocks::speakAndWait(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
#if defined(ENABLE_AUDIO) && defined(ENABLE_CLOUDVARS) && (defined(__WIIU__) || defined(_WIN32) || defined(__linux__) || defined(__APPLE__) || defined(__3DS__))
    Value inputValue = Scratch::getInputValue(block, "WORDS", sprite);
    std::string inputString = inputValue.asString();

    std::string voice = sprite->textToSpeechData.gender;
    std::string language = sprite->textToSpeechData.language;
    std::string api = "https://synthesis-service.scratch.mit.edu/synth?locale=" + language + "&gender=" + voice + "&text=" + urlEncode(inputString);
    std::string tempDir = OS::getScratchFolderLocation() + "cache/";
    std::size_t h = std::hash<std::string>{}(api);
    std::string safeName = "t2s_temp_" + std::to_string(h) + ".mp3";
    std::string tempFile = tempDir + safeName;

    if (!fromRepeat && block.repeatTimes >= -1) {
        Log::log("T2S: resetting repeatTimes");
        block.repeatTimes = -2;
    }

    if (block.repeatTimes == -2) {
        if (SoundPlayer::isSoundPlaying(tempFile)) {
            Log::log("T2S: Currently speaking on other block, skipping");
            BlockExecutor::removeFromRepeatQueue(sprite, &block);
            return BlockResult::CONTINUE;
        }
        if (SoundPlayer::isSoundLoaded(tempFile)) {
            Log::log("T2S: sound loaded, playing");
            SoundPlayer::playSound(tempFile);
            block.repeatTimes = -4;
            BlockExecutor::addToRepeatQueue(sprite, &block);
            return BlockResult::RETURN;
        }
        block.repeatTimes = -3;
        BlockExecutor::addToRepeatQueue(sprite, &block);
        return BlockResult::RETURN;
    }
    if (block.repeatTimes == -3) {
        try {
            if (std::filesystem::exists(tempFile) && !DownloadManager::isDownloading(api)) {
                Log::log("T2S audio already downloaded: " + inputString);
                SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, tempFile, false, false, true);
                block.repeatTimes = -4;
                BlockExecutor::addToRepeatQueue(sprite, &block);
                return BlockResult::RETURN;
            }

            if (!DownloadManager::isDownloading(api)) {
                Log::log("T2S: starting download for: " + inputString + " -> " + tempFile);

                DownloadManager::addDownload(api, tempFile);
                BlockExecutor::addToRepeatQueue(sprite, &block);
                return BlockResult::RETURN;
            }
            BlockExecutor::addToRepeatQueue(sprite, &block);
            return BlockResult::RETURN;
        } catch (const std::exception &e) {
            Log::logWarning(std::string("Filesystem::exists threw: ") + e.what());
            BlockExecutor::removeFromRepeatQueue(sprite, &block);
            return BlockResult::CONTINUE;
        }
    }
    if (block.repeatTimes == -4) {

        if (SoundPlayer::isSoundPlaying(tempFile)) {
            BlockExecutor::addToRepeatQueue(sprite, &block);
            return BlockResult::RETURN;
        }
    }

    BlockExecutor::removeFromRepeatQueue(sprite, &block);
    block.repeatTimes = -1;
    Log::log("T2S: finished speaking: " + inputString);
#else
    Log::log("T2S: ENABLE_AUDIO is NOT defined");
#endif

    return BlockResult::CONTINUE;
}

BlockResult SpeechBlocks::setVoiceTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value voiceValue = Scratch::getInputValue(block, "VOICE", sprite);
    std::string voiceString = voiceValue.asString();
    if (voiceString == "tenor" || voiceString == "giant") {
        voiceString = "male";
    } else {
        voiceString = "female"; // alto squeak kitten and for any unknown values
    }
    // sprite->textToSpeechData.playbackRate = 1.0; //sound does currently not support playback rate changes, i think?
    sprite->textToSpeechData.gender = voiceString;
    return BlockResult::CONTINUE;
}

BlockResult SpeechBlocks::setLanguageTo(Block &block, Sprite *sprite, bool *withoutScreenRefresh, bool fromRepeat) {
    Value languageValue = Scratch::getInputValue(block, "LANGUAGE", sprite);
    std::string languageString = languageValue.asString();
    sprite->textToSpeechData.language = languageString;
    return BlockResult::CONTINUE;
}