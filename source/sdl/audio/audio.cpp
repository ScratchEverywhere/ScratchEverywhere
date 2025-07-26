#include "../scratch/audio.hpp"
#include "audio.hpp"
#include <SDL2/SDL_mixer.h>
#include <algorithm>
#include <iostream>

std::unordered_map<std::string, SDL_Audio *> SDL_Sounds;

SDL_Audio::SDL_Audio() : audioChunk(nullptr) {}

SDL_Audio::~SDL_Audio() {
    if (audioChunk) {
        Mix_FreeChunk(audioChunk);
        audioChunk = nullptr;
    }
}

// code down here kinda messy,,, TODO fix that

int SB3SoundLoaderThread(void *data) {
    SDL_Audio::SoundLoadParams *params = static_cast<SDL_Audio::SoundLoadParams *>(data);
    bool success = params->player->loadSoundFromSB3(params->sprite, params->zip, params->soundId);

    delete params;

    return success ? 0 : 1;
}

void SoundPlayer::startSB3SoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId) {
    SDL_Sounds[soundId] = new SDL_Audio();
    SDL_Audio::SoundLoadParams *params = new SDL_Audio::SoundLoadParams{
        .sprite = sprite,
        .zip = zip,
        .soundId = soundId};

    SDL_Thread *thread = SDL_CreateThread(SB3SoundLoaderThread, "SoundLoader", params);

    if (!thread) {
        std::cerr << "Failed to create SDL thread: " << SDL_GetError() << std::endl;
    } else {
        SDL_DetachThread(thread);
    }
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId) {
    if (!zip) {
        std::cout << "Error: Zip archive is null" << std::endl;
        return false;
    }

    std::cout << "Loading sound: '" << soundId << "'" << std::endl;

    int file_count = (int)mz_zip_reader_get_num_files(zip);
    if (file_count <= 0) {
        std::cout << "Error: No files found in zip archive" << std::endl;
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
            std::cout << "Extracting sound from sb3..." << std::endl;
            void *file_data = mz_zip_reader_extract_to_heap(zip, i, &file_size, 0);
            if (!file_data || file_size == 0) {
                std::cout << "Failed to extract: " << zipFileName << std::endl;
                return false;
            }

            SDL_RWops *rw = SDL_RWFromMem(file_data, (int)file_size);
            if (!rw) {
                std::cout << "Failed to create RWops for: " << zipFileName << std::endl;
                mz_free(file_data);
                return false;
            }
            std::cout << "Converting sound into SDL sound..." << std::endl;
            Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 1);
            mz_free(file_data);

            if (!chunk) {
                std::cout << "Failed to load audio from memory: " << zipFileName << " - SDL_mixer Error: " << Mix_GetError() << std::endl;
                return false;
            }

            std::cout << "Creating SDL sound object..." << std::endl;
            // Create SDL_Audio object
            SDL_Audio *audio = new SDL_Audio();
            audio->audioChunk = chunk;
            audio->audioId = soundId;

            SDL_Sounds[soundId] = audio;

            std::cout << "Successfully loaded audio: " << soundId << std::endl;
            SDL_Sounds[soundId]->isLoaded = true;
            playSound(soundId);
            return true;
        }
    }

    std::cout << "Audio not found: " << soundId << std::endl;
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, const std::string &fileName) {
    std::cout << "Loading audio from file: " << fileName << std::endl;

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
        std::cout << "Unsupported audio format: " << fileName << std::endl;
        return false;
    }

    Mix_Chunk *chunk = Mix_LoadWAV(fileName.c_str());
    if (!chunk) {
        std::cout << "Failed to load audio file: " << fileName << " - SDL_mixer Error: " << Mix_GetError() << std::endl;
        return false;
    }

    // Create SDL_Audio object
    SDL_Audio *audio = new SDL_Audio();
    audio->audioChunk = chunk;
    audio->audioId = fileName;

    SDL_Sounds[fileName] = audio;

    std::cout << "Successfully loaded audio: " << fileName << std::endl;
    SDL_Sounds[fileName]->isLoaded = true;
    playSound(fileName);
    return true;
}

int SoundPlayer::playSound(const std::string &soundId) {
    auto it = SDL_Sounds.find(soundId);
    if (it != SDL_Sounds.end()) {
        it->second->isPlaying = true;
        int channel = Mix_PlayChannel(-1, it->second->audioChunk, 0);
        if (channel != -1) {
            SDL_Sounds[soundId]->channelId = channel;
        }
        return channel;
    }
    std::cout << "Sound not found: " << soundId << std::endl;
    return -1;
}

void SoundPlayer::stopSound(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        int channel = soundFind->second->channelId;
        Mix_HaltChannel(channel);
    } else {
        std::cout << "No active channel found for sound: " << soundId << std::endl;
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
        return Mix_Playing(channel) != 0;
    }
    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {
    auto soundFind = SDL_Sounds.find(soundId);
    return soundFind != SDL_Sounds.end();
}

void SoundPlayer::cleanupAudio() {
    for (auto &pair : SDL_Sounds) {
        delete pair.second;
    }
    SDL_Sounds.clear();
}