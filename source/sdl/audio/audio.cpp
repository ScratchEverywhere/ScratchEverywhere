#include "../scratch/audio.hpp"
#include "../scratch/os.hpp"
#include "audio.hpp"
#include "interpret.hpp"
#include "miniz/miniz.h"
#include "sprite.hpp"
#include <string>
#include <unordered_map>
#ifdef __3DS__
#include <3ds.h>
#endif

std::unordered_map<std::string, std::unique_ptr<SDL_Audio>> SDL_Sounds;
std::string currentStreamedSound = "";
static bool isInit = false;

#ifdef ENABLE_AUDIO
SDL_Audio::SDL_Audio() : audioChunk(nullptr) {}
#endif

SDL_Audio::~SDL_Audio() {
#ifdef ENABLE_AUDIO
    if (audioChunk != nullptr) {
        Mix_FreeChunk(audioChunk);
        audioChunk = nullptr;
    }
    if (music != nullptr) {
        Mix_FreeMusic(music);
        music = nullptr;
    }
#endif
}

bool SoundPlayer::init() {
    if (isInit) return true;
#ifdef ENABLE_AUDIO
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        Log::logWarning(std::string("SDL_Mixer could not initialize! ") + Mix_GetError());
        return false;
    }
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    if (Mix_Init(flags) != flags) {
        Log::logWarning(std::string("SDL_Mixer could not initialize MP3/OGG Support! ") + Mix_GetError());
    }
    isInit = true;
    return true;
#endif
    return false;
}

void SoundPlayer::startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId) {
#ifdef ENABLE_AUDIO
    if (!init()) return;

    if (SDL_Sounds.find(soundId) != SDL_Sounds.end()) {
        return;
    }

    std::unique_ptr<SDL_Audio> audio = std::make_unique<SDL_Audio>();
    SDL_Sounds[soundId] = std::move(audio);

    SDL_Audio::SoundLoadParams params = {
        .sprite = sprite,
        .zip = zip,
        .soundId = soundId,
        .streamed = sprite->isStage}; // stage sprites get streamed audio

#if defined(__OGC__)
    params.streamed = false; // streamed sounds crash on wii.
#endif

    if (projectType != UNZIPPED)
        loadSoundFromSB3(params.sprite, params.zip, params.soundId, params.streamed);
    else
        loadSoundFromFile(params.sprite, "project/" + params.soundId, params.streamed);

#endif
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {
#ifdef ENABLE_AUDIO
    if (!zip) {
        Log::logWarning("Error: Zip archive is null");
        return false;
    }

    // Log::log("Loading sound: '" + soundId + "'");

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
            // Log::log("Extracting sound from sb3...");
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
                // Log::log("Converting sound into SDL sound...");
                chunk = Mix_LoadWAV_RW(rw, 1);
                mz_free(file_data);

                if (!chunk) {
                    Log::logWarning("Failed to load audio from memory: " + zipFileName + " - SDL_mixer Error: " + Mix_GetError());
                    return false;
                }
            } else {
                // need to write to a temp file because this is a zip file
                std::string tempDir = OS::getScratchFolderLocation() + "/cache";
                std::string tempFile = tempDir + "/temp_" + soundId;

                // make cache directory
                try {
                    std::filesystem::create_directories(tempDir);
                } catch (const std::exception &e) {
                    Log::logWarning(std::string("Failed to create temp directory: ") + e.what());
                    mz_free(file_data);
                    return false;
                }

                FILE *fp = fopen(tempFile.c_str(), "wb");
                if (!fp) {
                    Log::logWarning("Failed to create temp file for streaming");
                    mz_free(file_data);
                    return false;
                }

                fwrite(file_data, 1, file_size, fp);
                fclose(fp);
                mz_free(file_data);

                // Log::log("Converting sound into SDL streamed music...");
                music = Mix_LoadMUS(tempFile.c_str());

                // Clean up temp file
                remove(tempFile.c_str());

                if (!music) {
                    Log::logWarning("Failed to load music from memory: " + zipFileName + " - SDL_mixer Error: " + Mix_GetError());
                    return false;
                }
            }

            // Log::log("Creating SDL sound object...");

            // Create SDL_Audio object
            auto it = SDL_Sounds.find(soundId);
            if (it == SDL_Sounds.end()) {
                std::unique_ptr<SDL_Audio> audio;
                audio = std::make_unique<SDL_Audio>();
                SDL_Sounds[soundId] = std::move(audio);
            }

            if (!streamed) {
                SDL_Sounds[soundId]->audioChunk = chunk;
            } else {
                SDL_Sounds[soundId]->music = music;
                SDL_Sounds[soundId]->isStreaming = true;
            }
            SDL_Sounds[soundId]->audioId = soundId;

            Log::log("Successfully loaded audio!");
            // Log::log("memory usage: " + std::to_string(MemoryTracker::getCurrentUsage() / 1024) + " KB");
            SDL_Sounds[soundId]->isLoaded = true;
            SDL_Sounds[soundId]->channelId = SDL_Sounds.size();
            playSound(soundId);
            setSoundVolume(soundId, sprite->volume);
            return true;
        }
    }
#endif
    Log::logWarning("Audio not found: " + soundId);
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed) {
#ifdef ENABLE_AUDIO
    // Log::log("Loading audio from file: " + fileName);

    // Check if file has supported extension
    std::string lowerFileName = fileName;
    std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);

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

    Mix_Chunk *chunk = nullptr;
    Mix_Music *music = nullptr;
    size_t audioMemorySize = 0;

    if (!streamed) {
        chunk = Mix_LoadWAV(fileName.c_str());
        if (!chunk) {
            Log::logWarning("Failed to load audio file: " + fileName + " - SDL_mixer Error: " + Mix_GetError());
            return false;
        }
    } else {
        music = Mix_LoadMUS(fileName.c_str());
        if (!music) {
            Log::logWarning("Failed to load streamed audio file: " + fileName + " - SDL_mixer Error: " + Mix_GetError());
            return false;
        }
    }

    // Create SDL_Audio object
    std::unique_ptr<SDL_Audio> audio = std::make_unique<SDL_Audio>();
    if (!streamed)
        audio->audioChunk = chunk;
    else {
        audio->music = music;
        audio->isStreaming = true;
    }
    audio->audioId = fileName;
    audio->memorySize = audioMemorySize;

    SDL_Sounds[fileName] = std::move(audio);

    Log::log("Successfully loaded audio!");
    SDL_Sounds[fileName]->isLoaded = true;
    SDL_Sounds[fileName]->channelId = SDL_Sounds.size();
    playSound(fileName);
    setSoundVolume(fileName, sprite->volume);
    return true;
#endif
    return false;
}

int SoundPlayer::playSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO
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
#endif
    Log::logWarning("Sound not found: " + soundId);
    return -1;
}

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {

        float clampedVolume = std::clamp(volume, 0.0f, 100.0f);
        int sdlVolume = (int)((clampedVolume / 100.0f) * 128.0f);

        int channel = soundFind->second->channelId;
        if (soundFind->second->isStreaming) {
            Mix_VolumeMusic(sdlVolume);
        } else {
            if (channel < 0 || channel >= Mix_AllocateChannels(-1)) {
                Log::logWarning("Invalid channel to set volume to!");
                return;
            }
            Mix_Volume(channel, sdlVolume);
        }
    }
#endif
}

float SoundPlayer::getSoundVolume(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        int sdlVolume = 0;

        if (soundFind->second->isStreaming) {
            sdlVolume = Mix_VolumeMusic(-1);
        } else {
            int channel = soundFind->second->channelId;
            if (channel >= 0 && channel < Mix_AllocateChannels(-1)) {
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
#endif
    return -1.0f;
}

void SoundPlayer::stopSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {

        if (!soundFind->second->isStreaming) {
            int channel = soundFind->second->channelId;
            if (channel >= 0 && channel < Mix_AllocateChannels(-1)) {
                Mix_HaltChannel(channel);
                soundFind->second->isPlaying = false;
            } else {
                Log::logWarning("Invalid channel for sound: " + soundId);
            }
        } else {
            stopStreamedSound();
            soundFind->second->isPlaying = false;
        }
    }
#endif
}

void SoundPlayer::stopStreamedSound() {
#ifdef ENABLE_AUDIO
    Mix_HaltMusic();
    if (!currentStreamedSound.empty()) {
        auto it = SDL_Sounds.find(currentStreamedSound);
        if (it != SDL_Sounds.end()) {
            it->second->isPlaying = false;
        }
        currentStreamedSound = "";
    }
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
        int channel = soundFind->second->channelId;
        if (!soundFind->second->isStreaming)
            return Mix_Playing(channel) != 0;
        else
            return Mix_PlayingMusic() != 0;
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
        Log::log("A sound has been freed!");
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
    Mix_HaltMusic();
    Mix_HaltChannel(-1);
    SDL_Sounds.clear();

#endif
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    if (!isInit) return;
    Mix_HaltMusic();
    Mix_HaltChannel(-1);
    cleanupAudio();
    Mix_CloseAudio();
    Mix_Quit();
#endif
}
