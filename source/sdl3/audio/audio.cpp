#include "../../scratch/audio.hpp"
#include "../../scratch/os.hpp"
#include "audio.hpp"
#include "interpret.hpp"
#include "miniz.h"
#include "sprite.hpp"
#include <string>
#include <unordered_map>
#ifdef __3DS__
#include <3ds.h>
#elif defined(__PC__) || defined(__PSP__)
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

std::unordered_map<std::string, std::unique_ptr<SDL_Audio>> SDL_Sounds;
std::string currentStreamedSound = "";
static bool isInit = false;
#ifdef ENABLE_AUDIO
static MIX_Mixer *mixer = nullptr;
#endif

bool SoundPlayer::init() {
    if (isInit) return true;
#ifdef ENABLE_AUDIO
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        Log::logError("Could not init SDL! " + std::string(SDL_GetError()));
        return false;
    }
    if (!MIX_Init()) {
        Log::logError("Could not init SDL Mixer! " + std::string(SDL_GetError()));
        return false;
    }
    SDL_AudioDeviceID device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!device) {
        Log::logWarning("Failed to open audio device: " + std::string(SDL_GetError()));
        return false;
    }

    SDL_AudioSpec spac;
    spac.channels = 2;
    spac.format = SDL_AUDIO_S16LE;
    spac.freq = 44100;

    // Create the mixer
    mixer = MIX_CreateMixerDevice(device, &spac);
    if (!mixer) {
        Log::logWarning("Failed to create mixer: " + std::string(SDL_GetError()));
        SDL_CloseAudioDevice(device);
        return false;
    }

    isInit = true;
    Log::log("Successfuly init SDL3!");
    return true;
#endif
}

#ifdef ENABLE_AUDIO
SDL_Audio::SDL_Audio() {}
#endif

SDL_Audio::~SDL_Audio() {
#ifdef ENABLE_AUDIO
    if (sound != nullptr) {
        MIX_DestroyAudio(sound);
        sound = nullptr;
    }
#endif
}

void SoundPlayer::startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed, const bool &fromProject, const bool &fromCache) { // fromCache is only necessary for dowmnloaded sounds like from T2S
#ifdef ENABLE_AUDIO
    if (!init()) return;

    if (SDL_Sounds.find(soundId) != SDL_Sounds.end()) {
        return;
    }

    SDL_Audio::SoundLoadParams params = {
        .sprite = sprite,
        .zip = zip,
        .soundId = soundId,
        .streamed = true};

#if defined(__OGC__)
    params.streamed = false;
#endif

    if (projectType != UNZIPPED && fromProject && !fromCache)
        loadSoundFromSB3(params.sprite, params.zip, params.soundId, params.streamed);
    else
        loadSoundFromFile(params.sprite, (fromProject && !fromCache ? "project/" : "") + params.soundId, params.streamed, fromCache);

#endif
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {
#ifdef ENABLE_AUDIO
    if (!zip) {
        Log::logWarning("Error: Zip archive is null");
        return false;
    }

    int file_count = (int)mz_zip_reader_get_num_files(zip);
    if (file_count <= 0) {
        Log::logWarning("Error: No files found in zip archive");
        return false;
    }

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip, i, &file_stat)) {
            continue;
        }

        std::string zipFileName = file_stat.m_filename;

        if (zipFileName != soundId) {
            continue;
        }

        size_t file_size;
        void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
        if (!file_data || file_size == 0) {
            Log::logWarning("Failed to extract: " + zipFileName);
            return false;
        }

        SDL_IOStream *rw = SDL_IOFromConstMem(file_data, (int)file_size);
        if (!rw) {
            Log::logWarning("Failed to create RWops for: " + zipFileName);
            mz_free(file_data);
            return false;
        }

        MIX_Audio *sound = MIX_LoadAudio_IO(mixer, rw, !streamed, true);
        mz_free(file_data);

        if (!sound) {
            Log::logWarning("Failed to load audio from memory: " + zipFileName + " - SDL Error: " + std::string(SDL_GetError()));
            return false;
        }

        auto audio = std::make_unique<SDL_Audio>();
        audio->sound = sound;
        audio->audioId = soundId;
        audio->isLoaded = true;

        SDL_Sounds[soundId] = std::move(audio);

        playSound(soundId);
        const int volume = sprite != nullptr ? sprite->volume : 100;
        setSoundVolume(soundId, volume);
        return true;
    }
#endif
    Log::logWarning("Audio not found in archive: " + soundId);
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed, const bool &fromCache) {
#ifdef ENABLE_AUDIO

    // Check if file has supported extension
    std::string lowerFileName = fileName;
    std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
    if (!fromCache)
        fileName = OS::getRomFSLocation() + fileName;

    bool isSupported = false;
    if (lowerFileName.size() >= 4) {
        std::string ext = lowerFileName.substr(lowerFileName.size() - 4);
        if (ext == ".mp3" || ext == ".wav" || ext == ".ogg") {
            isSupported = true;
        }
    }

    if (!isSupported) {
        Log::logWarning("Unsupported audio format: " + fileName);
        return false;
    }

#if defined(__PC__) || defined(__PSP__)
    const auto &file = cmrc::romfs::get_filesystem().open(fileName);
    MIX_Audio *sound = MIX_LoadAudio_IO(mixer, SDL_IOFromConstMem(file.begin(), file.size()), !streamed, true);
#else
    MIX_Audio *sound = MIX_LoadAudio(mixer, fileName.c_str(), !streamed);
#endif
    if (!sound) {
        Log::logWarning("Failed to load audio file: " + fileName + " - SDL_mixer Error: " + std::string(SDL_GetError()));
        return false;
    }

    if (!fromCache) {
        // remove romfs and `project` from filename for soundId
        fileName = fileName.substr(OS::getRomFSLocation().length());
        const std::string prefix = "project/";
        if (fileName.rfind(prefix, 0) == 0) {
            fileName = fileName.substr(prefix.length());
        }
    }

    // Create SDL_Audio object
    std::unique_ptr<SDL_Audio> audio = std::make_unique<SDL_Audio>();
    audio->sound = sound;
    audio->audioId = fileName;

    SDL_Sounds[fileName] = std::move(audio);

    SDL_Sounds[fileName]->isLoaded = true;
    playSound(fileName);
    const int volume = sprite != nullptr ? sprite->volume : 100;
    setSoundVolume(fileName, volume);
    return true;
#endif
    return false;
}

int SoundPlayer::playSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto it = SDL_Sounds.find(soundId);
    if (it != SDL_Sounds.end()) {

        it->second->isPlaying = true;

        if (it->second->track == nullptr) {
            it->second->track = MIX_CreateTrack(mixer);
            if (!it->second->track) {
                Log::logWarning("Failed to create track: " + std::string(SDL_GetError()));
                return -1;
            }
        }

        if (!MIX_SetTrackAudio(it->second->track, it->second->sound)) {
            Log::logWarning("Failed to set track audio: " + std::string(SDL_GetError()));
            return -1;
        }

        if (!MIX_PlayTrack(it->second->track, 0)) {
            Log::logWarning("Failed to play track: " + soundId + " " + std::string(SDL_GetError()));
            return -1;
        }
        return 0;
    }
#endif
    Log::logWarning("Sound not found: " + soundId);
    return -1;
}

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        float clampedVolume = std::clamp(volume, 0.0f, 100.0f);

        float sdlVolume = clampedVolume / 100.0f;

        if (soundFind->second->track == nullptr) {
            Log::logWarning("Track not initialized, cannot set volume!");
            return;
        }

        if (!MIX_SetTrackGain(soundFind->second->track, sdlVolume)) {
            Log::logWarning("Failed to set track volume: " + std::string(SDL_GetError()));
            return;
        }
    }
#endif
}

float SoundPlayer::getSoundVolume(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        if (soundFind->second->track == nullptr) {
            Log::logWarning("Track not initialized, cannot get volume!");
            return -1.0f;
        }

        float sdlVolume = MIX_GetTrackGain(soundFind->second->track);

        float appVolume = sdlVolume * 100.0f;
        return appVolume;
    }
#endif
    return -1.0f;
}

double SoundPlayer::getMusicPosition(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        return (double)MIX_TrackFramesToMS(soundFind->second->track, MIX_GetTrackPlaybackPosition(soundFind->second->track));
    }

#endif
    return 0.0;
}

void SoundPlayer::setMusicPosition(double position, const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        MIX_SetTrackPlaybackPosition(soundFind->second->track, MIX_TrackMSToFrames(soundFind->second->track, position));
    }
#endif
}

void SoundPlayer::stopSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        if (soundFind->second->track) {
            if (!MIX_StopTrack(soundFind->second->track, 0)) {
                Log::logWarning("Failed to stop track: " + std::string(SDL_GetError()));
            }
            soundFind->second->isPlaying = false;
        }
    } else {
        Log::logWarning("Sound not found, cannot stop: " + soundId);
    }
#endif
}

void SoundPlayer::stopStreamedSound() {
#ifdef ENABLE_AUDIO

#endif
}

void SoundPlayer::checkAudio() {
#ifdef ENABLE_AUDIO
    for (auto &[id, audio] : SDL_Sounds) {
        if (!isSoundPlaying(id)) {
            audio->isPlaying = false;
        }
    }
#endif
}

bool SoundPlayer::isSoundPlaying(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        if (!soundFind->second->isLoaded) return true;
        if (!soundFind->second->isPlaying) return false;
        return MIX_TrackPlaying(soundFind->second->track) != 0;
    }
#endif
    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        return soundFind->second->isLoaded;
    }
#endif
    return false;
}

void SoundPlayer::freeAudio(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto it = SDL_Sounds.find(soundId);
    if (it != SDL_Sounds.end()) {
        SDL_Sounds.erase(it);
    } else Log::logWarning("Could not find sound to free: " + soundId);
#endif
}

void SoundPlayer::flushAudio() {
#ifdef ENABLE_AUDIO
    if (SDL_Sounds.empty()) return;
    for (auto &[id, audio] : SDL_Sounds) {
        if (!isSoundPlaying(id)) {
            audio->freeTimer -= 1;
            if (audio->freeTimer <= 0) {
                freeAudio(id);
                return;
            }

        } else audio->freeTimer = 240;
    }
#endif
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    MIX_StopAllTracks(mixer, 0);
    SDL_Sounds.clear();

#endif
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    MIX_StopAllTracks(mixer, 0);
    cleanupAudio();
    MIX_DestroyMixer(mixer);
    MIX_Quit();
    SDL_Quit();
#endif
}
