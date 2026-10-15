#include "audio.hpp"
#include "log.hpp"
#include "settings.hpp"
#include <memory>

#ifdef AUDIO_LIBPULSE
#include "audio_libpulse.hpp"
#endif
#ifdef AUDIO_LIBRETRO
#include "audio_libretro.hpp"
#endif
#ifdef AUDIO_WINMM
#include "audio_winmm.hpp"
#endif
#ifdef AUDIO_3DS
#include "audio_3ds.hpp"
#endif
#ifdef AUDIO_NDS
#include "audio_nds.hpp"
#endif
#ifdef AUDIO_SDL3
#include "audio_sdl3.hpp"
#endif
#ifdef AUDIO_SDL2
#include "audio_sdl2.hpp"
#endif
#ifdef AUDIO_SDL1
#include "audio_sdl1.hpp"
#endif
#ifdef AUDIO_HEADLESS
#include "audio_headless.hpp"
#endif

SoundDevice *globalSoundDevice = nullptr;

AudioBackends SoundPlayer::currentSoundBackend = AudioBackends::_default;

const std::vector<AudioBackendInfo> SoundPlayer::getAvailableAudioBackends() {
    std::vector<AudioBackendInfo> backends;

    auto add = [&](AudioBackends a) {
        uint8_t id = static_cast<uint8_t>(a);
        backends.push_back({id, std::string(AudioBackendNames[id])});
    };

#ifdef AUDIO_LIBPULSE
    add(AudioBackends::Libpulse);
#endif
#ifdef AUDIO_LIBRETRO
    add(AudioBackends::Libretro);
#endif
#ifdef AUDIO_WINMM
    add(AudioBackends::Winmm);
#endif
#ifdef AUDIO_3DS
    add(AudioBackends::N3DS);
#endif
#ifdef AUDIO_NDS
    add(AudioBackends::NDS);
#endif
#ifdef AUDIO_SDL3
    add(AudioBackends::SDL3);
#endif
#ifdef AUDIO_SDL2
    add(AudioBackends::SDL2);
#endif
#ifdef AUDIO_SDL1
    add(AudioBackends::SDL1);
#endif
#ifdef AUDIO_HEADLESS
    add(AudioBackends::Headless);
#endif
#ifndef ENABLE_AUDIO
    add(AudioBackends::Disabled);
#endif

    return backends;
}

bool SoundPlayer::init() {
    const auto j = SettingsManager::getConfigSettings();
    uint8_t startIndex = 0;
    bool found = false;

    if (j.contains("AudioEngine") && j["AudioEngine"].is_string()) {
        const std::string audioString = j["AudioEngine"].get<std::string>();
        for (auto &a : AudioBackendNames) {
            if (a == audioString) {
                Log::log("Initializing " + audioString + " audio backend from settings...");
                found = true;
                break;
            }
            startIndex++;
        }
    }

    if (!found) {
        startIndex = 0;
    }

    uint8_t totalBackends = static_cast<uint8_t>(AudioBackends::_default);
    for (uint8_t step = 0; step < totalBackends; ++step) {
        uint8_t i = (startIndex + step) % totalBackends;

        auto backendName = static_cast<AudioBackends>(i);

        switch (backendName) {
        case AudioBackends::Libpulse: {
#ifdef AUDIO_LIBPULSE
            currentSoundBackend = AudioBackends::Libpulse;
            globalSoundDevice = new SoundDevice_Libpulse;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::Libretro: {
#ifdef AUDIO_LIBRETRO
            currentSoundBackend = AudioBackends::Libretro;
            globalSoundDevice = new SoundDevice_Libretro;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::Winmm: {
#ifdef AUDIO_WINMM
            currentSoundBackend = AudioBackends::Winmm;
            globalSoundDevice = new SoundDevice_Winmm;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::N3DS: {
#ifdef AUDIO_3DS
            currentSoundBackend = AudioBackends::N3DS;
            globalSoundDevice = new SoundDevice_3DS;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::NDS: {
#ifdef AUDIO_NDS
            currentSoundBackend = AudioBackends::NDS;
            globalSoundDevice = new SoundDevice_NDS;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::SDL3: {
#ifdef AUDIO_SDL3
            currentSoundBackend = AudioBackends::SDL3;
            globalSoundDevice = new SoundDevice_SDL3;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::SDL2: {
#ifdef AUDIO_SDL2
            currentSoundBackend = AudioBackends::SDL2;
            globalSoundDevice = new SoundDevice_SDL2;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::SDL1: {
#ifdef AUDIO_SDL1
            currentSoundBackend = AudioBackends::SDL1;
            globalSoundDevice = new SoundDevice_SDL1;
            break;
#else
            continue;
#endif
        }
        case AudioBackends::Headless: {
#ifdef AUDIO_HEADLESS
            currentSoundBackend = AudioBackends::Headless;
            globalSoundDevice = new SoundDevice_Headless;
            break;
#else
            continue;
#endif
        }
        default: {
            continue;
        }
        }

        if (globalSoundDevice == nullptr) {
            Log::logError(std::string(AudioBackendNames[i]) + " failed to init.");
            delete globalSoundDevice;
            globalSoundDevice = nullptr;
            continue;
        } else if (globalSoundDevice->error.has_value()) {
            Log::logError(std::string(AudioBackendNames[i]) + " failed to init: " + globalSoundDevice->error.value());
            delete globalSoundDevice;
            globalSoundDevice = nullptr;
            continue;
        } else break;
    }

    if (!globalSoundDevice) {
        Log::logCritical("Failed to init audio: no available audio devices worked.", true);
        return false;
    }

    return true;
}

void SoundPlayer::deinit() {
    if (globalSoundDevice) {
        delete globalSoundDevice;
        globalSoundDevice = nullptr;
    }
}