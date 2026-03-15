#include "../audio.hpp"
#include "blockUtils.hpp"
#include "downloader.hpp"
#include "math.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"
#include <string>


SCRATCH_SHADOW_BLOCK(text2speech_menu_voices, voices)
SCRATCH_SHADOW_BLOCK(text2speech_menu_languages, languages)

SCRATCH_BLOCK(text2speech, speakAndWait) {
#if defined(ENABLE_DOWNLOAD) && defined(ENABLE_AUDIO)
    BlockState *state = thread->getState(block);
    if (state->completedSteps == 0) {

        Value words;
        if (!Scratch::getInput(block, "WORDS", thread, sprite, words)) return BlockResult::REPEAT;
        
        std::string inputString = words.asString();

        std::string voice = sprite->textToSpeechData.gender;
        std::string language = sprite->textToSpeechData.language;
        state->name = "https://synthesis-service.scratch.mit.edu/synth?locale=" + language + "&gender=" + voice + "&text=" + urlEncode(inputString);
        std::string tempDir = OS::getScratchFolderLocation() + "cache/";
        std::size_t h = std::hash<std::string>{}(state->name);
        std::string safeName = "t2s_temp_" + std::to_string(h) + ".mp3";
        std::string tempFile = tempDir + safeName;
        if (SoundPlayer::isSoundPlaying(tempFile)) {
            thread->eraseState(block);
            return BlockResult::CONTINUE;
        }
        state->completedSteps = 2;
        if (SoundPlayer::isSoundLoaded(tempFile)) {
            Log::log("T2S: sound loaded, playing");
            SoundPlayer::playSound(tempFile);
            return BlockResult::REPEAT;
        }
        if (!DownloadManager::init()) return BlockResult::CONTINUE;
        if (OS::fileExists(tempFile) && !DownloadManager::isDownloading(state->name)) {
            Log::log("T2S audio already downloaded: " + inputString);
            SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, tempFile, false, false, true);
            return BlockResult::REPEAT;
        }
        if (!DownloadManager::isDownloading(state->name)) {
            Log::log("T2S: starting download for: " + inputString + " -> " + tempFile);
            DownloadManager::addDownload(state->name, tempFile);
            state->completedSteps = 1;
            return BlockResult::REPEAT;
        }
    }
    std::string tempDir = OS::getScratchFolderLocation() + "cache/";
    std::size_t h = std::hash<std::string>{}(state->name);
    std::string safeName = "t2s_temp_" + std::to_string(h) + ".mp3";
    std::string tempFile = tempDir + safeName;

    if (state->completedSteps == 1) {
        if (DownloadManager::isDownloading(state->name)) return BlockResult::REPEAT;

        if (OS::fileExists(tempFile) && !DownloadManager::isDownloading(state->name)) {
            Log::log("T2S audio already downloaded");
            SoundPlayer::startSoundLoaderThread(sprite, &Unzip::zipArchive, tempFile, false, false, true);
            state->completedSteps = 2;
            return BlockResult::RETURN;
        }
    }
    if (state->completedSteps == 2) {
        if (SoundPlayer::isSoundPlaying(tempFile)) return BlockResult::REPEAT;
    }
    thread->eraseState(block);
#else
    Log::logWarning("T2S: ENABLE_AUDIO is NOT defined");
#endif

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(text2speech, setVoice) {
    Value voice;
    if (!Scratch::getInput(block, "VOICE", thread, sprite, voice)) return BlockResult::REPEAT;
    
    std::string voiceString = voice.asString();
    if (voiceString == "tenor" || voiceString == "giant") {
        voiceString = "male";
    } else {
        voiceString = "female"; // alto squeak kitten and for any unknown values
    }
    // sprite->textToSpeechData.playbackRate = 1.0; /ToDo playbackRate is implemeted for Audio, i think? So it could be added?
    sprite->textToSpeechData.gender = voiceString;
    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(text2speech, setLanguage) {
    Value language;
    if (!Scratch::getInput(block, "LANGUAGE", thread, sprite, language)) return BlockResult::REPEAT;
    
    std::string languageString = language.asString();
    sprite->textToSpeechData.language = languageString;
    return BlockResult::CONTINUE;
}