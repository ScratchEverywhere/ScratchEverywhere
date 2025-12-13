#include "../scratch/audio.hpp"
#include "../scratch/os.hpp"
#include "audio.hpp"
#include "interpret.hpp"
#include "miniz.h"
#include "sprite.hpp"
#include <SDL_endian.h>
#include <string>
#include <unordered_map>
#ifdef __3DS__
#include <3ds.h>
#endif
#ifdef USE_CMAKERC
#include <cmrc/cmrc.hpp>

CMRC_DECLARE(romfs);
#endif

std::unordered_map<std::string, std::unique_ptr<SDL_Audio>> SDL_Sounds;
std::string currentStreamedSound = "";
static bool isInit = false;

// IMA ADPCM step table
static const int ima_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

// IMA ADPCM index table
static const int ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8};

// Decode a single IMA ADPCM nibble
static int16_t decodeADPCMNibble(uint8_t nibble, int16_t &predictor, int &stepIndex) {
    int step = ima_step_table[stepIndex];
    int diff = step >> 3;

    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) diff = -diff;

    predictor += diff;
    if (predictor > 32767) predictor = 32767;
    if (predictor < -32768) predictor = -32768;

    stepIndex += ima_index_table[nibble];
    if (stepIndex < 0) stepIndex = 0;
    if (stepIndex > 88) stepIndex = 88;

    return predictor;
}

// Convert IMA ADPCM WAV to PCM WAV in memory
// Returns nullptr on failure, caller must free() the returned buffer
static void *decodeADPCMtoPCM(const void *adpcmData, size_t adpcmSize, size_t *outPcmSize) {
    const uint8_t *data = (const uint8_t *)adpcmData;

    // Verify RIFF/WAVE header
    if (adpcmSize < 44 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) {
        return nullptr;
    }

    // Parse fmt chunk
    size_t pos = 12;
    uint16_t channels = 0, blockAlign = 0;
    uint32_t sampleRate = 0;
    size_t dataOffset = 0, dataSize = 0;

    while (pos + 8 <= adpcmSize) {
        uint32_t chunkSize = data[pos + 4] | (data[pos + 5] << 8) | (data[pos + 6] << 16) | (data[pos + 7] << 24);

        if (memcmp(data + pos, "fmt ", 4) == 0) {
            if (pos + 8 + chunkSize > adpcmSize || chunkSize < 20) return nullptr;
            uint16_t format = data[pos + 8] | (data[pos + 9] << 8);
            if (format != 0x11) return nullptr; // Not IMA ADPCM

            channels = data[pos + 10] | (data[pos + 11] << 8);
            sampleRate = data[pos + 12] | (data[pos + 13] << 8) | (data[pos + 14] << 16) | (data[pos + 15] << 24);
            blockAlign = data[pos + 20] | (data[pos + 21] << 8);
        } else if (memcmp(data + pos, "data", 4) == 0) {
            dataOffset = pos + 8;
            dataSize = chunkSize;
            break;
        }
        pos += 8 + chunkSize;
        if (chunkSize & 1) pos++; // Padding
    }

    if (dataOffset == 0 || dataSize == 0 || channels == 0 || blockAlign == 0) {
        return nullptr;
    }

    // Only mono ADPCM supported
    if (channels != 1) {
        return nullptr;
    }

    // Calculate output size
    // IMA ADPCM: 4 bits per sample, but block has 4-byte header per channel
    size_t samplesPerBlock = (blockAlign - 4 * channels) * 2 / channels + 1;
    size_t numBlocks = (dataSize + blockAlign - 1) / blockAlign;
    size_t totalSamples = numBlocks * samplesPerBlock * channels;
    size_t pcmDataSize = totalSamples * 2; // 16-bit output

    // Allocate output buffer (WAV header + PCM data)
    size_t pcmFileSize = 44 + pcmDataSize;
    uint8_t *pcmBuffer = (uint8_t *)malloc(pcmFileSize);
    if (!pcmBuffer) return nullptr;

    // Write WAV header
    memcpy(pcmBuffer, "RIFF", 4);
    uint32_t riffSize = pcmFileSize - 8;
    pcmBuffer[4] = riffSize & 0xFF;
    pcmBuffer[5] = (riffSize >> 8) & 0xFF;
    pcmBuffer[6] = (riffSize >> 16) & 0xFF;
    pcmBuffer[7] = (riffSize >> 24) & 0xFF;
    memcpy(pcmBuffer + 8, "WAVE", 4);
    memcpy(pcmBuffer + 12, "fmt ", 4);
    pcmBuffer[16] = 16;
    pcmBuffer[17] = 0;
    pcmBuffer[18] = 0;
    pcmBuffer[19] = 0; // fmt chunk size
    pcmBuffer[20] = 1;
    pcmBuffer[21] = 0; // PCM format
    pcmBuffer[22] = channels & 0xFF;
    pcmBuffer[23] = (channels >> 8) & 0xFF;
    pcmBuffer[24] = sampleRate & 0xFF;
    pcmBuffer[25] = (sampleRate >> 8) & 0xFF;
    pcmBuffer[26] = (sampleRate >> 16) & 0xFF;
    pcmBuffer[27] = (sampleRate >> 24) & 0xFF;
    uint32_t byteRate = sampleRate * channels * 2;
    pcmBuffer[28] = byteRate & 0xFF;
    pcmBuffer[29] = (byteRate >> 8) & 0xFF;
    pcmBuffer[30] = (byteRate >> 16) & 0xFF;
    pcmBuffer[31] = (byteRate >> 24) & 0xFF;
    uint16_t pcmBlockAlign = channels * 2;
    pcmBuffer[32] = pcmBlockAlign & 0xFF;
    pcmBuffer[33] = (pcmBlockAlign >> 8) & 0xFF;
    pcmBuffer[34] = 16;
    pcmBuffer[35] = 0; // 16 bits per sample
    memcpy(pcmBuffer + 36, "data", 4);
    pcmBuffer[40] = pcmDataSize & 0xFF;
    pcmBuffer[41] = (pcmDataSize >> 8) & 0xFF;
    pcmBuffer[42] = (pcmDataSize >> 16) & 0xFF;
    pcmBuffer[43] = (pcmDataSize >> 24) & 0xFF;

    // Decode ADPCM blocks
    int16_t *pcmOut = (int16_t *)(pcmBuffer + 44);
    size_t outPos = 0;
    const uint8_t *blockPtr = data + dataOffset;
    size_t remaining = dataSize;

    while (remaining >= (size_t)blockAlign) {
        // Each block starts with predictor and step index for each channel
        int16_t predictor[2] = {0, 0};
        int stepIndex[2] = {0, 0};

        for (int ch = 0; ch < channels; ch++) {
            predictor[ch] = (int16_t)(blockPtr[ch * 4] | (blockPtr[ch * 4 + 1] << 8));
            stepIndex[ch] = blockPtr[ch * 4 + 2];
            if (stepIndex[ch] > 88) stepIndex[ch] = 88;

            // First sample is the predictor itself
            if (outPos < totalSamples) {
                pcmOut[outPos++] = SDL_SwapLE16(predictor[ch]);
            }
        }

        // Decode the rest of the block
        const uint8_t *adpcmBytes = blockPtr + 4 * channels;
        size_t bytesInBlock = blockAlign - 4 * channels;

        // Mono: sequential nibbles
        for (size_t i = 0; i < bytesInBlock && outPos < totalSamples; i++) {
            uint8_t byte = adpcmBytes[i];
            pcmOut[outPos++] = SDL_SwapLE16(decodeADPCMNibble(byte & 0x0F, predictor[0], stepIndex[0]));
            if (outPos < totalSamples) {
                pcmOut[outPos++] = SDL_SwapLE16(decodeADPCMNibble((byte >> 4) & 0x0F, predictor[0], stepIndex[0]));
            }
        }

        blockPtr += blockAlign;
        remaining -= blockAlign;
    }

    *outPcmSize = pcmFileSize;
    return pcmBuffer;
}

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
    if (file_data && file_data != nullptr && isStreaming) {
        if (useStdFree) {
            free(file_data);
        } else {
            mz_free(file_data);
        }
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
        .streamed = streamed || (sprite != nullptr && sprite->isStage)}; // stage sprites get streamed audio

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

            if (ext4 == ".mp3" || ext4 == ".mpga" || ext4 == ".wav" || ext4 == ".ogg" || ext4 == ".oga") {
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
            bool useStreaming = streamed; // Local copy we can modify

            // Check if this is IMA ADPCM format (format tag 0x11 at offset 20)
            unsigned char *bytes = (unsigned char *)file_data;
            bool isADPCM = (file_size > 22 && bytes[20] == 0x11 && bytes[21] == 0x00);

            // If ADPCM, decode to PCM in memory first
            void *audioData = file_data;
            size_t audioSize = file_size;
            bool needsFreeAudioData = false;

            if (isADPCM) {
                size_t pcmSize = 0;
                void *pcmData = decodeADPCMtoPCM(file_data, file_size, &pcmSize);
                mz_free(file_data);

                if (!pcmData) {
                    Log::logWarning("Failed to decode ADPCM: " + zipFileName);
                    return false;
                }
                audioData = pcmData;
                audioSize = pcmSize;
                needsFreeAudioData = true;
            }

            if (!useStreaming) {
                SDL_RWops *rw = SDL_RWFromMem(audioData, (int)audioSize);
                if (!rw) {
                    Log::logWarning("Failed to create RWops for: " + zipFileName);
                    if (needsFreeAudioData) free(audioData);
                    else mz_free(audioData);
                    return false;
                }
                chunk = Mix_LoadWAV_RW(rw, 1);
                if (needsFreeAudioData) free(audioData);
                else mz_free(audioData);

                if (!chunk) {
                    Log::logWarning("Failed to load audio from memory: " + zipFileName + " - SDL_mixer Error: " + Mix_GetError());
                    return false;
                }
            } else {
                SDL_RWops *rw = SDL_RWFromMem(audioData, (int)audioSize);
                if (!rw) {
                    Log::logWarning("Failed to create RWops for: " + zipFileName);
                    if (needsFreeAudioData) free(audioData);
                    else mz_free(audioData);
                    return false;
                }
                music = Mix_LoadMUS_RW(rw, 1);
                if (!music) {
                    Log::logWarning("Failed to load audio from memory: " + zipFileName + " - SDL_mixer Error: " + Mix_GetError());
                    if (needsFreeAudioData) free(audioData);
                    else mz_free(audioData);
                    return false;
                }
                // Note: for streaming, SDL_mixer keeps reference to RWops,
                // audioData must stay alive - we store it for cleanup in destructor
            }

            // Create SDL_Audio object
            auto it = SDL_Sounds.find(soundId);
            if (it == SDL_Sounds.end()) {
                std::unique_ptr<SDL_Audio> audio;
                audio = std::make_unique<SDL_Audio>();
                SDL_Sounds[soundId] = std::move(audio);
            }

            if (!useStreaming) {
                SDL_Sounds[soundId]->audioChunk = chunk;
            } else {
                SDL_Sounds[soundId]->music = music;
                SDL_Sounds[soundId]->isStreaming = true;
                // Store audioData for streaming - needed by SDL_mixer, freed in destructor
                SDL_Sounds[soundId]->file_data = audioData;
                SDL_Sounds[soundId]->file_size = audioSize;
                SDL_Sounds[soundId]->useStdFree = needsFreeAudioData; // true if decoded ADPCM (malloc)
            }
            SDL_Sounds[soundId]->audioId = soundId;

            Log::log("Successfully loaded audio!");
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

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed, const bool &fromCache) {
#ifdef ENABLE_AUDIO
    Log::log("Loading audio from file: " + fileName);

    // Check if file has supported extension
    std::string lowerFileName = fileName;
    std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
    if (!fromCache)
        fileName = OS::getRomFSLocation() + fileName;

    bool isSupported = false;
    if (lowerFileName.size() >= 4) {
        std::string ext = lowerFileName.substr(lowerFileName.size() - 4);
        if (ext == ".mp3" || ext == ".mpga" || ext == ".wav" || ext == ".ogg" || ext == ".oga") {
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
#ifdef USE_CMAKERC
        if (fromCache)
            chunk = Mix_LoadWAV(fileName.c_str());
        else {
            const auto &file = cmrc::romfs::get_filesystem().open(fileName);
            chunk = Mix_LoadWAV_RW(SDL_RWFromConstMem(file.begin(), file.size()), 1);
        }
#else
        chunk = Mix_LoadWAV(fileName.c_str());
#endif
        if (!chunk) {
            Log::logWarning("Failed to load audio file: " + fileName + " - SDL_mixer Error: " + Mix_GetError());
            return false;
        }
    } else {
#ifdef USE_CMAKERC
        if (fromCache)
            music = Mix_LoadMUS(fileName.c_str());
        else {
            const auto &file = cmrc::romfs::get_filesystem().open(fileName);
            music = Mix_LoadMUS_RW(SDL_RWFromConstMem(file.begin(), file.size()), 1);
        }
#else
        music = Mix_LoadMUS(fileName.c_str());
#endif
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

    // remove romfs and `project/` from filename for soundId
    // remove romfs from filename for soundId
    if (!fromCache) {
        // remove romfs and `project/` from filename for soundId
        fileName = fileName.substr(OS::getRomFSLocation().length());
        const std::string prefix = "project/";
        if (fileName.rfind(prefix, 0) == 0) {
            fileName = fileName.substr(prefix.length());
        }
    }

    audio->audioId = fileName;
    audio->memorySize = audioMemorySize;

    SDL_Sounds[fileName] = std::move(audio);
    SDL_Sounds[fileName]->isLoaded = true;
    SDL_Sounds[fileName]->channelId = SDL_Sounds.size();
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

double SoundPlayer::getMusicPosition(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        // ill get to it i think
    }

#endif
    return 0.0;
}

void SoundPlayer::setMusicPosition(double position, const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {
        // ...
    }
#endif
}

void SoundPlayer::stopSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = SDL_Sounds.find(soundId);
    if (soundFind != SDL_Sounds.end()) {

        if (!soundFind->second->isStreaming) {
            int channel = soundFind->second->channelId;
            if (channel >= 0 && channel < Mix_AllocateChannels(-1)) {
                // Only halt if this sound's chunk is actually playing on this channel
                // Prevents stopping other sounds that took over the channel
                Mix_Chunk *playing = Mix_GetChunk(channel);
                if (playing == soundFind->second->audioChunk) {
                    Mix_HaltChannel(channel);
                }
                soundFind->second->isPlaying = false;
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
