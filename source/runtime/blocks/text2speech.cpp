#include "../../audio.hpp"
#include "../../audiostack.hpp"
#include "blockUtils.hpp"
#include "downloader.hpp"
#include "math.hpp"
#include "os.hpp"
#include "runtime.hpp"
#include "settings.hpp"
#include "sprite.hpp"
#include "unzip.hpp"
#include "value.hpp"
#include <filesystem.hpp>
#include <log.hpp>
#include <string>

SCRATCH_SHADOW_BLOCK(text2speech_menu_voices, voices)
SCRATCH_SHADOW_BLOCK(text2speech_menu_languages, languages)

#include <iostream>

SCRATCH_BLOCK(text2speech, speakAndWait) {
#ifdef ENABLE_AUDIO
#ifdef ENABLE_DOWNLOAD
    bool forceDectalk = false;
#elif defined(ENABLE_DECTALK)
    bool forceDectalk = true;
#else
    /* no download and also no dectalk */

    Log::logWarning("[TextToSpeech] Neither of ENABLE_DECTALK nor ENABLE_DOWNLOAD were defined");
    return BlockResult::CONTINUE;
#endif

#ifdef ENABLE_DECTALK
    if (forceDectalk || SettingsManager::getConfigSettings().value("useDectalk",
#ifdef DECTALK_DEFAULT
                                                                   true
#else
                                                                   false
#endif
                                                                   )) {
        return BlockExecutor::getHandlers()["nishiowoDectalk_speakAndWait"](block, thread, sprite, outValue);
    } else
#endif
    {
#ifdef ENABLE_DOWNLOAD
        BlockState *state = thread->getState(block);
        if (state->completedSteps == 0) {

            Value words;
            if (!Scratch::getInput(block, "WORDS", thread, sprite, words)) return BlockResult::REPEAT;

            std::string inputString = words.asString();

            std::string voice = sprite->textToSpeechData.gender;
            std::string language = sprite->textToSpeechData.language;
            state->name = "http://synthesis-service.scratch.mit.edu/synth?locale=" + language + "&gender=" + voice + "&text=" + urlEncode(inputString);
            std::string tempDir = OS::getScratchFolderLocation() + "cache/";
            std::size_t h = std::hash<std::string>{}(state->name);
            std::string safeName = "t2s_temp_" + std::to_string(h) + ".mp3";
            std::string tempFile = tempDir + safeName;
            if (Mixer::isSoundPlaying(tempFile)) {
                thread->eraseState(block);
                return BlockResult::CONTINUE;
            }
            state->completedSteps = 2;
            if (!DownloadManager::init()) return BlockResult::CONTINUE;
            if (FileSystem::fileExists(tempFile) && !DownloadManager::isDownloading(state->name)) {
                Log::log("[TextToSpeech] audio already downloaded: " + inputString);
                SoundStream *strm = new SoundStream(tempFile, false, true);
                if (strm->error.has_value()) {
                    Log::logError("[TextToSpeech] " + strm->error.value());
                    delete strm;
                }
                return BlockResult::REPEAT;
            }
            if (!DownloadManager::isDownloading(state->name)) {
                Log::log("[TextToSpeech] starting download for: " + inputString + " -> " + tempFile);
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

            if (FileSystem::fileExists(tempFile) && !DownloadManager::isDownloading(state->name)) {
                Log::log("[TextToSpeech] audio already downloaded");
                SoundStream *strm = new SoundStream(tempFile, false, true);
                if (strm->error.has_value()) {
                    Log::logError(strm->error.value());
                    delete strm;
                }
                state->completedSteps = 2;
                return BlockResult::REPEAT;
            }
        }
        if (state->completedSteps == 2) {
            if (Mixer::isSoundPlaying(tempFile)) return BlockResult::REPEAT;
        }

        Mixer::setAutoClean(tempFile, true);
        thread->eraseState(block);
#endif
    }
#else
    Log::logWarning("[TextToSpeech] ENABLE_AUDIO is NOT defined");
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
