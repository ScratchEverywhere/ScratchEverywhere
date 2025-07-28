
#include "../scratch/audio.hpp"
#include "../scratch/os.hpp"
#include "audio.hpp"
#ifdef __3DS__
#include <3ds.h>
#endif
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <iostream>

std::unordered_map<std::string, SDL_Audio *> SDL_Sounds;
std::string currentStreamedSound = "";

SDL_Audio::SDL_Audio() : audioChunk(nullptr) {}

SDL_Audio::~SDL_Audio() {
    if (memorySize > 0) {
        MemoryTracker::deallocate(nullptr, memorySize);
    }
    if (audioChunk) {
        Mix_FreeChunk(audioChunk);
        audioChunk = nullptr;
    }
    if (music != nullptr) {
        // Mix_FreeMusic(music);
        // music = nullptr;
    }
}

// code down here kinda messy,,, TODO fix that

int soundLoaderThread(void *data) {
    SDL_Audio::SoundLoadParams *params = static_cast<SDL_Audio::SoundLoadParams *>(data);
    bool success = false;
    if (projectType != UNZIPPED)
        success = params->player->loadSoundFromSB3(params->sprite, params->zip, params->soundId, params->streamed);
    else
        success = params->player->loadSoundFromFile(params->sprite, "project/" + params->soundId, params->streamed);

    delete params;

    return success ? 0 : 1;
}

void NDS_soundLoaderThread(void *data) {
    SDL_Audio::SoundLoadParams *params = static_cast<SDL_Audio::SoundLoadParams *>(data);
    if (projectType != UNZIPPED)
        params->player->loadSoundFromSB3(params->sprite, params->zip, params->soundId, params->streamed);
    else
        params->player->loadSoundFromFile(params->sprite, "project/" + params->soundId, params->streamed);

    delete params;

    return;
}

void SoundPlayer::startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId) {

    if (SDL_Sounds.find(soundId) != SDL_Sounds.end()) {
        return;
    }

    SDL_Audio *audio = MemoryTracker::allocate<SDL_Audio>();
    new (audio) SDL_Audio();
    SDL_Sounds[soundId] = audio;

    SDL_Audio::SoundLoadParams *params = new SDL_Audio::SoundLoadParams{
        .sprite = sprite,
        .zip = zip,
        .soundId = soundId,
        .streamed = sprite->isStage}; // stage sprites get streamed audio

// do 3DS threads so it can actually run in the background
#ifdef __3DS__
    s32 mainPrio = 0;
    svcGetThreadPriority(&mainPrio, CUR_THREAD_HANDLE);

    threadCreate(
        NDS_soundLoaderThread,
        params,
        0x10000,
        mainPrio + 1,
        1,
        true);
#else

    SDL_Thread *thread = SDL_CreateThread(soundLoaderThread, "SoundLoader", params);

    if (!thread) {
        Log::logWarning("Failed to create SDL thread: " + std::string(SDL_GetError()));
    } else {
        SDL_DetachThread(thread);
    }
#endif
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {
    if (!zip) {
        Log::logWarning("Error: Zip archive is null");
        return false;
    }

    Log::log("Loading sound: '" + soundId + "'");

    int file_count = (int)mz_zip_reader_get_num_files(zip);
    if (file_count <= 0) {
        Log::logWarning("Error: No files found in zip archive");
        return false;
    }

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip, i, &file_stat)) continue;

        std::string zipFileName = file_stat.m_filename;

        bool isAudio = false;
        std::string extension = "";

        if (zipFileName.size() >= 4) {
            std::string ext4 = zipFileName.substr(zipFileName.size() - 4);
            std::transform(ext4.begin(), ext4.end(), ext4.begin(), ::tolower);

            if (ext4 == ".mp3" || ext4 == ".wav" || ext4 == ".ogg") {
                isAudio = true;
                extension = ext4;
            }
        }

        if (isAudio) {
            if (zipFileName != soundId) {
                continue;
            }

            size_t file_size;
            Log::log("Extracting sound from sb3...");
            void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
            if (!file_data || file_size == 0) {
                Log::logWarning("Failed to extract: " + zipFileName);
                return false;
            }

            Mix_Music *music = nullptr;
            Mix_Chunk *chunk = nullptr;

            if (!streamed) {
                SDL_RWops *rw = SDL_RWFromMem(file_data, (int)file_size);
                if (!rw) {
                    Log::logWarning("Failed to create RWops for: " + zipFileName);
                    mz_free(file_data);
                    return false;
                }
                Log::log("Converting sound into SDL sound...");
                chunk = Mix_LoadWAV_RW(rw, 1);
                mz_free(file_data);

                if (!chunk) {
                    Log::logWarning("Failed to load audio from memory: " + zipFileName + " - SDL_mixer Error: " + Mix_GetError());
                    return false;
                }
            } else {
                // need to write to a temp file beacause this is zip file
                std::string tempFile = "temp_" + soundId;
                FILE *fp = fopen(tempFile.c_str(), "wb");
                if (!fp) {
                    Log::logWarning("Failed to create temp file for streaming");
                    mz_free(file_data);
                    return false;
                }

                fwrite(file_data, 1, file_size, fp);
                fclose(fp);
                mz_free(file_data);

                Log::log("Converting sound into SDL streamed music...");
                music = Mix_LoadMUS(tempFile.c_str());

                // Clean up temp file
                remove(tempFile.c_str());

                if (!music) {
                    Log::logWarning("Failed to load music from memory: " + zipFileName + " - SDL_mixer Error: " + Mix_GetError());
                    return false;
                }
            }

            Log::log("Creating SDL sound object...");

            // Create SDL_Audio object
            SDL_Audio *audio;
            auto it = SDL_Sounds.find(soundId);
            if (it != SDL_Sounds.end()) {
                audio = it->second;
            } else {
                audio = MemoryTracker::allocate<SDL_Audio>();
                new (audio) SDL_Audio();
                SDL_Sounds[soundId] = audio;
            }

            if (!streamed) {
                audio->audioChunk = chunk;
                audio->memorySize = file_size * 2; // Rough estimate..
                MemoryTracker::allocate(audio->memorySize);
            } else {
                audio->music = music;
                audio->isStreaming = true;
                audio->memorySize = 64 * 1024; // streaming buffer is ~64kb
                MemoryTracker::allocate(audio->memorySize);
            }
            audio->audioId = soundId;

            SDL_Sounds[soundId] = audio;

            Log::log("Successfully loaded audio: " + soundId);
            Log::log("memory usage: " + std::to_string(MemoryTracker::getCurrentUsage() / 1024) + " KB");
            SDL_Sounds[soundId]->isLoaded = true;
            playSound(soundId);
            setSoundVolume(soundId, sprite->volume);
            return true;
        }
    }

    Log::logWarning("Audio not found: " + soundId);
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, const std::string &fileName, const bool &streamed) {
    Log::log("Loading audio from file: " + fileName);

    // Check if file has supported extension
    std::string lowerFileName = fileName;
    std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);

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

    Mix_Chunk *chunk = nullptr;
    Mix_Music *music = nullptr;
    size_t audioMemorySize = 0;

    if (!streamed) {
        chunk = Mix_LoadWAV(fileName.c_str());
        if (!chunk) {
            Log::logWarning("Failed to load audio file: " + fileName + " - SDL_mixer Error: " + Mix_GetError());
            return false;
        }
        // estimate memory based on file size
        FILE *file = fopen(fileName.c_str(), "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            fclose(file);
            audioMemorySize = file_size * 2; // rough estimate
        } else {
            audioMemorySize = 1024 * 1024; // 1MB defualt
        }
        MemoryTracker::allocate(audioMemorySize);
    } else {
        music = Mix_LoadMUS(fileName.c_str());
        if (!music) {
            Log::logWarning("Failed to load streamed audio file: " + fileName + " - SDL_mixer Error: " + Mix_GetError());
            return false;
        }
        audioMemorySize = 64 * 1024; // estimate
        MemoryTracker::allocate(audioMemorySize);
    }

    // Create SDL_Audio object
    SDL_Audio *audio = MemoryTracker::allocate<SDL_Audio>();
    new (audio) SDL_Audio();
    if (!streamed)
        audio->audioChunk = chunk;
    else {
        audio->music = music;
        audio->isStreaming = true;
    }
    audio->audioId = fileName;
    audio->memorySize = audioMemorySize;

    SDL_Sounds[fileName] = audio;

    Log::log("Successfully loaded audio: " + fileName);
    SDL_Sounds[fileName]->isLoaded = true;
    playSound(fileName);
    setSoundVolume(fileName, sprite->volume);
    return true;
}

int SoundPlayer::playSound(const std::string &soundId) {
    auto it = SDL_Sounds.find(soundId);
    if (it != SDL_Sounds.end()) {

        if (!currentStreamedSound.empty() && it->second->isStreaming) {
            stopStreamedSound();
        }

        it->second->isPlaying = true;

        if (!it->second->isStreaming) {
            int channel = Mix_PlayChannel(-1, it->second->audioChunk, 0);
            if (channel != -1) {
                SDL_Sounds[soundId]->channelId = channel;
            }
            return channel;
        } else {
            currentStreamedSound = soundId;
            int result = Mix_PlayMusic(it->second->music, 0);
            if (result == -1) {
                Log::logWarning("Failed to play streamed sound: " + std::string(Mix_GetError()));
                it->second->isPlaying = false;
                currentStreamedSound = "";
            }
            return result;
        }
    }
    Log::logWarning("Sound not found: " + soundId);
    return -1;
}

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {

        float clampedVolume = std::clamp(volume, 0.0f, 100.0f);
        int sdlVolume = (int)((clampedVolume / 100.0f) * 128.0f);

        int channel = soundFind->second->channelId;
        if (soundFind->second->isStreaming) {
            Mix_VolumeMusic(sdlVolume);
        } else {
            Mix_Volume(channel, sdlVolume);
        }
    }
}

float SoundPlayer::getSoundVolume(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        int sdlVolume = 0;

        if (soundFind->second->isStreaming) {
            sdlVolume = Mix_VolumeMusic(-1);
        } else {
            int channel = soundFind->second->channelId;
            if (channel != -1) {
                sdlVolume = Mix_Volume(channel, -1);
            } else {
                // no channel assigned
                if (soundFind->second->audioChunk) {
                    sdlVolume = Mix_VolumeChunk(soundFind->second->audioChunk, -1);
                }
            }
        }
        // convert from SDL's 0-128 range back to 0-100 range
        return (sdlVolume / 128.0f) * 100.0f;
    }

    // return -1 to indicate sound not found
    return -1.0f;
}

void SoundPlayer::stopSound(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        int channel = soundFind->second->channelId;
        Mix_HaltChannel(channel);
    } else {
        Log::logWarning("No active channel found for sound: " + soundId);
    }
}

void SoundPlayer::stopStreamedSound() {
    Mix_HaltMusic();
    if (!currentStreamedSound.empty()) {
        auto it = SDL_Sounds.find(currentStreamedSound);
        if (it != SDL_Sounds.end()) {
            it->second->isPlaying = false;
        }
        currentStreamedSound = "";
    }
}

void SoundPlayer::checkAudio() {
    for (auto &[id, audio] : SDL_Sounds) {
        if (!isSoundPlaying(id)) {
            audio->isPlaying = false;
        }
    }
}

bool SoundPlayer::isSoundPlaying(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        if (!soundFind->second->isLoaded) return true;
        if (!soundFind->second->isPlaying) return false;
        int channel = soundFind->second->channelId;
        if (!soundFind->second->isStreaming)
            return Mix_Playing(channel) != 0;
        else
            return Mix_PlayingMusic() != 0;
    }
    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        return soundFind->second->isLoaded;
    }
    return false;
}

void SoundPlayer::freeAudio(const std::string &soundId) {
    auto it = SDL_Sounds.find(soundId);
    if (it != SDL_Sounds.end()) {
        SDL_Audio *audio = it->second;
        audio->~SDL_Audio();
        MemoryTracker::deallocate<SDL_Audio>(audio);

        SDL_Sounds.erase(it);
    }
}

void SoundPlayer::cleanupAudio() {
    Mix_HaltMusic();
    Mix_HaltChannel(-1);

    // Track memory cleanup
    for (auto &pair : SDL_Sounds) {
        pair.second->~SDL_Audio();
        MemoryTracker::deallocate<SDL_Audio>(pair.second);
    }
    SDL_Sounds.clear();
    Mix_CloseAudio();
    Mix_Quit();
}