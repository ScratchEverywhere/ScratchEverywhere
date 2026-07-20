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
    bool stop;
    void *tts;
    std::string name;
    std::string text;
    std::vector<short> buffer;
};

static std::unordered_map<std::string, tts_value *> tts;
static std::unordered_map<void *, tts_value *> tts_lookup;

// undef this if you want to render all buffer first
#define ENABLE_DECTALK_STREAM
#endif

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

    if (v->stop) return 0;

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

SCRATCH_BLOCK(nishiowoDectalk, speakAndWait) {
#ifdef ENABLE_AUDIO
#ifdef ENABLE_DECTALK
#define STREAM SoundStream *strm = new SoundStream("dtc:" + name, dtc_callback, 1, 11025)

    BlockState *state = thread->getState(block);
    std::size_t h = std::hash<std::string>{}(state->name);
    std::string name = std::to_string(h);
    if (state->completedSteps == 0) {
        Value words;
        tts_value *v;

        if (!Scratch::getInputValue(block, "WORDS", thread, sprite, words)) return BlockResult::REPEAT;

        v = new tts_value();
        v->finished = false;
        v->tts = TextToSpeechAllocate();
        v->name = name;
        v->text = words.asString();

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
        return BlockResult::REPEAT;
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
#else
    Log::logWarning("[TextToSpeech] ENABLE_DECTALK is NOT defined");
    return BlockResult::CONTINUE;
#endif
#else
    Log::logWarning("[TextToSpeech] ENABLE_AUDIO is NOT defined");
#endif

    return BlockResult::CONTINUE;
}

SCRATCH_BLOCK(nishiowoDectalk, stopAll) {
#ifdef ENABLE_AUDIO
#ifdef ENABLE_DECTALK
    for (auto v : tts) {
        v.second->stop = true;
        Mixer::stopSound(v.second->name);
    }
#else
    Log::logWarning("[TextToSpeech] ENABLE_DECTALK is NOT defined");
    return BlockResult::CONTINUE;
#endif
#else
    Log::logWarning("[TextToSpeech] ENABLE_AUDIO is NOT defined");
#endif

    return BlockResult::CONTINUE;
}
