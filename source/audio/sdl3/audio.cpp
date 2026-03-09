#include "audio.hpp"
#include <audio.hpp>
#include <miniz.h>
#include <os.hpp>
#include <runtime.hpp>
#include <sprite.hpp>
#include <string>
#include <unordered_map>
#include <unzip.hpp>
#ifdef USE_CMAKERC
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

    SDL_AudioSpec spac;
    spac.channels = 2;
    spac.format = SDL_AUDIO_S16LE;
    spac.freq = 44100;

    SDL_AudioDeviceID device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spac);
    if (!device) {
        Log::logWarning("Failed to open audio device: " + std::string(SDL_GetError()));
        return false;
    }

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
    return false;
}

#ifdef ENABLE_AUDIO
SDL_Audio::SDL_Audio() {}
#endif

SDL_Audio::~SDL_Audio() {
#ifdef ENABLE_AUDIO
    if (track != nullptr) {
        MIX_DestroyTrack(track);
    }
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
        .zip = Scratch::sb3InRam ? zip : nullptr,
        .soundId = soundId,
        .streamed = true};

#if defined(__OGC__)
    params.streamed = false;
#endif

    if (Scratch::projectType != ProjectType::UNZIPPED && fromProject && !fromCache)
        loadSoundFromSB3(params.sprite, params.zip, params.soundId, params.streamed);
    else {
        std::string filePrefix = "";
        if (fromProject && !fromCache) {
            if (Unzip::UnpackedInSD) filePrefix = Unzip::filePath;
            else filePrefix = "project/";
        }
        loadSoundFromFile(params.sprite, filePrefix + params.soundId, params.streamed, fromCache);
    }

#endif
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {
#ifdef ENABLE_AUDIO
    size_t file_size = 0;
    void *file_data = nullptr;

    if (zip != nullptr) {
        int file_index = mz_zip_reader_locate_file(zip, soundId.c_str(), nullptr, 0);
        if (file_index < 0) {
            Log::logWarning("Audio not found in zip: " + soundId);
            return false;
        }
        file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    } else {
        file_data = Unzip::getFileInSB3(soundId, &file_size);
    }

    if (!file_data || file_size == 0) {
        Log::logWarning("Failed to extract: " + soundId);
        return false;
    }

    SDL_IOStream *rw = SDL_IOFromConstMem(file_data, file_size);
    if (!rw) {
        Log::logWarning("Failed to create IOStream for: " + soundId);
        mz_free(file_data);
        return false;
    }

    MIX_Audio *sound = MIX_LoadAudio_IO(mixer, rw, !streamed, true);
    mz_free(file_data);

    if (!sound) {
        Log::logWarning("Failed to load audio from memory: " + soundId + " - " + std::string(SDL_GetError()));
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

    MIX_Audio *sound;
#ifdef USE_CMAKERC
    if (fromCache || Unzip::UnpackedInSD)
        sound = MIX_LoadAudio(mixer, fileName.c_str(), !streamed);
    else {
        const auto &file = cmrc::romfs::get_filesystem().open(fileName);
        sound = MIX_LoadAudio_IO(mixer, SDL_IOFromConstMem(file.begin(), file.size()), !streamed, true);
    }
#else
    sound = MIX_LoadAudio(mixer, fileName.c_str(), !streamed);
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
        if (Unzip::UnpackedInSD) {
            fileName = fileName.substr(Unzip::filePath.length());
        }
    }

    // Create SDL_Audio object
    std::unique_ptr<SDL_Audio>
        audio = std::make_unique<SDL_Audio>();
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
    return -1;
}

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        float clampedVolume = std::clamp(volume, 0.0f, 100.0f);

        float sdlVolume = clampedVolume / 100.0f;

        if (soundFind->second->track == nullptr) {
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
            return -1.0f;
        }

        float sdlVolume = MIX_GetTrackGain(soundFind->second->track);

        float appVolume = sdlVolume * 100.0f;
        return appVolume;
    }
#endif
    return -1.0f;
}

void SoundPlayer::setPitch(const std::string &soundId, float pitch) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        if (soundFind->second->track == nullptr) {
            return;
        }

        if (!MIX_SetTrackFrequencyRatio(soundFind->second->track, (pitch * 0.01f) + 1.0f)) {
            Log::logWarning("Failed to set pitch effect: " + std::string(SDL_GetError()));
        }
    }

#endif
}

void SoundPlayer::setPan(const std::string &soundId, float pan) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        if (soundFind->second->track == nullptr) {
            return;
        }

        // this method kinda just forces the sound to be on one side without it being gradual..
        MIX_Point3D panPos;
        panPos.x = std::clamp(pan * 0.0001f, -0.1f, 0.1f);
        panPos.y = 0;
        panPos.z = 0;

        if (!MIX_SetTrack3DPosition(soundFind->second->track, &panPos)) {
            Log::logWarning("Failed to set pan effect: " + std::string(SDL_GetError()));
        }
    }
#endif
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
    }
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
