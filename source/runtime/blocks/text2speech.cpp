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

#if defined(ENABLE_DECTALK) && defined(ENABLE_AUDIO)
#define DT_EXTERN
#include <epsonapi.h>
#include <thread.hpp>

struct tts_value {
    SE_Thread thread;
    SE_Mutex mutex;
    bool finished;
    bool threaded;
    void *tts;
    std::string text;
    std::vector<short> buffer;
};

static std::unordered_map<std::string, tts_value *> tts;
static std::unordered_map<void *, tts_value *> tts_lookup;

// undef this if you want to render all buffer first
#define ENABLE_DECTALK_STREAM
#endif

SCRATCH_SHADOW_BLOCK(text2speech_menu_voices, voices)
SCRATCH_SHADOW_BLOCK(text2speech_menu_languages, languages)

#if defined(ENABLE_DECTALK) && defined(ENABLE_AUDIO)
static short *dtc_callback(void *ptr, short *iwave, long length, int phoneme) {
    tts_lookup[ptr]->mutex.lock();
    for (int i = 0; i < length; i++) {
        tts_lookup[ptr]->buffer.push_back(iwave[i]);
    }
    tts_lookup[ptr]->mutex.unlock();

    return iwave;
}

static void dtc_generate(void *arg) {
    tts_value *v = (tts_value *)arg;

    TextToSpeechInitEx(v->tts, nullptr, dtc_callback, nullptr);
    TextToSpeechStartEx(v->tts, (char *)v->text.c_str(), NULL, WAVE_FORMAT_1M16);
    TextToSpeechSyncEx(v->tts);

    v->mutex.lock();
    v->finished = true;
    v->mutex.unlock();
}

static int dtc_callback(SoundStream *strm, float *iwave, int length) {
    tts_value *v = tts[strm->name.substr(4)];
    int len;
    bool wait_buffer = false;

    do {
        v->mutex.lock();
        wait_buffer = !v->finished && (v->buffer.size() < length);
        v->mutex.unlock();
    } while (wait_buffer);

    v->mutex.lock();
    len = std::min((int)v->buffer.size(), length);
    for (int i = 0; i < len; i++) {
        iwave[i] = v->buffer[i] / 32767.0;
    }
    v->buffer.erase(v->buffer.begin(), v->buffer.begin() + len);
    v->mutex.unlock();

    return len;
}
#endif

#include <iostream>

SCRATCH_BLOCK(text2speech, speakAndWait) {
#ifdef ENABLE_AUDIO
#ifdef ENABLE_DOWNLOAD
    bool forceDectalk = false;
#elif defined(ENABLE_DECTALK)
    bool forceDectalk = true;
#else
    /* no download and also no dectalk */

    Log::logWarning("T2S: Neither of ENABLE_DECTALK nor ENABLE_DOWNLOAD were defined");
    return BlockResult::CONTINUE;
#endif

#ifdef ENABLE_DECTALK
    if (forceDectalk || SettingsManager::getConfigSettings().value("UseDectalk",
#ifdef DECTALK_DEFAULT
                                                                   true
#else
                                                                   false
#endif
                                                                   )) {
#define STREAM SoundStream *strm = new SoundStream("dtc:" + name, dtc_callback, 1, 11025)

        BlockState *state = thread->getState(block);
        std::size_t h = std::hash<std::string>{}(state->name);
        std::string name = std::to_string(h);
        if (state->completedSteps == 0) {
            Value words;
            tts_value *v;
            std::string g;
            if (!Scratch::getInput(block, "WORDS", thread, sprite, words)) return BlockResult::REPEAT;

            if (sprite->textToSpeechData.gender == "male") {
                g = "[:np]";
            } else {
                g = "[:nb]";
            }

            v = new tts_value();
            v->finished = false;
            v->tts = TextToSpeechAllocate();
            v->text = g + words.asString();

            tts[name] = v;

            tts_lookup[v->tts] = v;

            if ((v->threaded = v->thread.create(dtc_generate, v, 8 * 1024, 999, -2, "dectalkThread" + name))) {
                state->completedSteps = 1;

#ifdef ENABLE_DECTALK_STREAM
                STREAM;
#endif
            } else {
                dtc_generate(v);

                state->completedSteps = 2;
            }
        } else if (state->completedSteps == 1) {
            bool v;

            tts[name]->mutex.lock();
            v = tts[name]->finished;
            tts[name]->mutex.unlock();

            if (v) {
                state->completedSteps = 2;
                return BlockResult::REPEAT;
            }
        } else if (state->completedSteps == 2) {
            if (tts[name]->threaded) {
                tts[name]->thread.join();
#ifndef ENABLE_DECTALK_STREAM
                STREAM;
#endif
            } else {
                STREAM;
            }

            tts_lookup.erase(tts_lookup.find(tts[name]->tts));

            TextToSpeechFree(tts[name]->tts);

            state->completedSteps = 3;
        } else if (state->completedSteps == 3) {
            if (Mixer::isSoundPlaying("dtc:" + name)) return BlockResult::REPEAT;

            delete tts[name];

            tts.erase(tts.find(name));
        }

        if (state->completedSteps == 3) {
            Mixer::setAutoClean("dtc:" + name, true);
            thread->eraseState(block);
        } else {
            return BlockResult::REPEAT;
        }
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
                Log::log("T2S audio already downloaded: " + inputString);
                SoundStream *strm = new SoundStream(tempFile, false, true);
                if (strm->error.has_value()) {
                    Log::logError(strm->error.value());
                    delete strm;
                }
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

            if (FileSystem::fileExists(tempFile) && !DownloadManager::isDownloading(state->name)) {
                Log::log("T2S audio already downloaded");
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
