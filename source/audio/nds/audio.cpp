#include "audio.hpp"
#include <audio.hpp>
#include <miniz/miniz.h>
#include <os.hpp>
#include <runtime.hpp>
#include <sys/stat.h>

std::unordered_map<std::string, Sound> SoundPlayer::soundsPlaying;
std::unordered_map<std::string, NDS_Audio> NDS_Sounds;
extern std::unordered_map<std::string, NDS_Audio> NDS_Sounds;
char NDS_Audio::stream_buffer[BUFFER_LENGTH];
int NDS_Audio::stream_buffer_in;
int NDS_Audio::stream_buffer_out;

bool NDS_Audio::init() {
    return true;
}
#ifdef ENABLE_AUDIO
mm_word NDS_Audio::streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format) {
    size_t multiplier = 0;
    switch (format) {
    case MM_STREAM_8BIT_MONO:
        multiplier = 1;
        break;
    case MM_STREAM_8BIT_STEREO:
        multiplier = 2;
        break;
    case MM_STREAM_16BIT_MONO:
        multiplier = 2;
        break;
    case MM_STREAM_16BIT_STEREO:
        multiplier = 4;
        break;
    }

    size_t size = length * multiplier;

    size_t bytes_until_end = BUFFER_LENGTH - stream_buffer_out;

    if (bytes_until_end > size) {
        char *src_ = &stream_buffer[stream_buffer_out];

        memcpy(dest, src_, size);
        stream_buffer_out += size;
    } else {
        char *src_ = &stream_buffer[stream_buffer_out];
        char *dst_ = static_cast<char *>(dest);

        memcpy(dst_, src_, bytes_until_end);
        dst_ += bytes_until_end;
        size -= bytes_until_end;

        src_ = &stream_buffer[0];
        memcpy(dst_, src_, size);
        stream_buffer_out = size;
    }

    return length;
}

void NDS_Audio::clearStreamBuffer() {
    memset(stream_buffer, 0, BUFFER_LENGTH);
    stream_buffer_in = 0;
    stream_buffer_out = 0;
}

void NDS_Audio::readFile(char *buffer, size_t size, bool restartSound) {
    while (size > 0) {
        int res = fread(buffer, 1, size, audioFile);
        size -= res;
        buffer += res;

        if (feof(audioFile)) {
            memset(buffer, 0, size);
            clearStreamBuffer();
            SoundPlayer::stopSound(id);
            break;
        }
        if (restartSound) {
            fseek(audioFile, sizeof(WAVHeader_t), SEEK_SET);
            res = fread(buffer, 1, size, audioFile);
            size -= res;
            buffer += res;
            break;
        }
    }
}

void NDS_Audio::streamingFillBuffer(bool force_fill, bool restartSound) {
    if (!force_fill && stream_buffer_in == stream_buffer_out) {
        return;
    }

    if (stream_buffer_in < stream_buffer_out) {
        size_t size = stream_buffer_out - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size, restartSound);
        stream_buffer_in += size;
    } else {
        size_t size = BUFFER_LENGTH - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size, restartSound);
        stream_buffer_in = 0;

        size = stream_buffer_out - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size, restartSound);
        stream_buffer_in += size;
    }

    if (stream_buffer_in >= BUFFER_LENGTH)
        stream_buffer_in -= BUFFER_LENGTH;
}

int NDS_Audio::checkWAVHeader(const WAVHeader_t header) {
    if (header.chunkID != RIFF_ID) {
        printf("Wrong RIFF_ID %lx\n", header.chunkID);
        return 1;
    }

    if (header.format != WAVE_ID) {
        printf("Wrong WAVE_ID %lx\n", header.format);
        return 1;
    }

    if (header.subchunk1ID != FMT_ID) {
        printf("Wrong FMT_ID %lx\n", header.subchunk1ID);
        return 1;
    }

    if (header.subchunk2ID != DATA_ID) {
        printf("Wrong Subchunk2ID %lx\n", header.subchunk2ID);
        return 1;
    }

    return 0;
}

mm_stream_formats NDS_Audio::getMMStreamType(uint16_t numChannels, uint16_t bitsPerSample) {
    if (numChannels == 1) {
        if (bitsPerSample == 8)
            return MM_STREAM_8BIT_MONO;
        else
            return MM_STREAM_16BIT_MONO;
    } else if (numChannels == 2) {
        if (bitsPerSample == 8)
            return MM_STREAM_8BIT_STEREO;
        else
            return MM_STREAM_16BIT_STEREO;
    }
    return MM_STREAM_8BIT_MONO;
}
#endif

void SoundPlayer::startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed, const bool &fromProject, const bool &fromCache) {
#ifdef ENABLE_AUDIO

    if (Scratch::projectType != UNZIPPED && zip != nullptr && !fromCache)
        loadSoundFromSB3(sprite, zip, soundId, true);
    else
        loadSoundFromFile(sprite, (fromProject && !fromCache ? "project/" : "") + soundId, true, fromCache);
#endif
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {
#ifdef ENABLE_AUDIO
    if (!zip) {
        Log::logWarning("Error: Zip archive is null");
        return false;
    }

    if (soundId.size() < 4 || soundId.substr(soundId.size() - 4) != ".wav") {
        Log::logError("Audio type not supported!");
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

        if (zipFileName.find(soundId) == std::string::npos) {
            continue;
        }

        // Create temporary file
        std::string tempDir = OS::getScratchFolderLocation() + "cache/";
        std::string tempPath = tempDir + soundId;

        mkdir(tempDir.c_str(), 0777);

        FILE *tempFile = fopen(tempPath.c_str(), "wb");
        if (!tempFile) {
            Log::logWarning("Failed to create temp file");
            return false;
        }

        // Extract file in chunks to avoid large allocation
        mz_zip_reader_extract_iter_state *pState = mz_zip_reader_extract_iter_new(zip, i, 0);
        if (!pState) {
            Log::logWarning("Failed to create extraction iterator");
            fclose(tempFile);
            return false;
        }

        const size_t CHUNK_SIZE = 4096; // 4KB chunks
        char buffer[CHUNK_SIZE];
        size_t total_read = 0;

        while (true) {
            size_t read = mz_zip_reader_extract_iter_read(pState, buffer, CHUNK_SIZE);
            if (read == 0) break;

            fwrite(buffer, 1, read, tempFile);
            total_read += read;
        }

        mz_zip_reader_extract_iter_free(pState);
        fclose(tempFile);

        // Now load from the temp file
        bool success = loadSoundFromFile(sprite, tempPath, streamed);

        if (!streamed) {
            remove(tempPath.c_str());
        } else {
        }

        if (success) {
            playSound(soundId);
            setSoundVolume(soundId, sprite->volume);
        }

        return success;
    }
#endif
    Log::logWarning("Audio not found in archive: " + soundId);
    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed, const bool &fromCache) {
#ifdef ENABLE_AUDIO

    if (fileName.size() < 4 || fileName.substr(fileName.size() - 4) != ".wav") {
        Log::logError("Audio type not supported!");
        return false;
    }

    cleanupAudio();
    if (!fromCache)
        fileName = OS::getRomFSLocation() + fileName;

    if (!streamed) return false;

    NDS_Audio audio;

    audio.audioFile = fopen(fileName.c_str(), "rb");
    if (audio.audioFile == NULL) {
        Log::logError("Sound not found. " + fileName);
        return false;
    }

    WAVHeader_t wavHeader = {0};
    if (fread(&wavHeader, 1, sizeof(WAVHeader_t), audio.audioFile) != sizeof(WAVHeader_t)) {
        Log::logError("Failed to read WAV header.");
        return false;
    }
    if (audio.checkWAVHeader(wavHeader) != 0) {
        Log::logError("WAV file header is corrupt! Make sure it is in the correct PCM format!");
        return false;
    }

    // Fill the buffer before we start doing anything
    // audio.streamingFillBuffer(true);

    // We are not using a soundbank so we need to manually initialize
    // mm_ds_system.
    mm_ds_system mmSys =
        {
            .mod_count = 0,
            .samp_count = 0,
            .mem_bank = 0,
            .fifo_channel = FIFO_MAXMOD};
    mmInit(&mmSys);

    // Open the stream
    mm_stream stream =
        {
            .sampling_rate = wavHeader.sampleRate,
            .buffer_length = 2048,
            .callback = NDS_Audio::streamingCallback,
            .format = audio.getMMStreamType(wavHeader.numChannels, wavHeader.bitsPerSample),
            .timer = MM_TIMER2,
            .manual = false,
        };
    mmStreamOpen(&stream);
    audio.isPlaying = true;
    std::string baseName;
    if (!fromCache) {
        baseName = fileName.substr(fileName.find_last_of("/\\") + 1);
        baseName = baseName.substr(OS::getRomFSLocation().length());
    } else
        baseName = fileName;
    NDS_Sounds[baseName] = std::move(audio);
    NDS_Sounds[baseName].id = baseName;
    return true;
#endif

    return false;
}

int SoundPlayer::playSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO

    auto soundFind = NDS_Sounds.find(soundId);
    if (soundFind != NDS_Sounds.end()) {
        NDS_Audio::stopAllSounds();
        soundFind->second.isPlaying = true;
        soundFind->second.streamingFillBuffer(false, true);
        return 1;
    }
#endif

    return 0;
}

#ifdef ENABLE_AUDIO
void NDS_Audio::stopAllSounds() {
    for (auto &[id, audio] : NDS_Sounds) {
        SoundPlayer::stopSound(audio.id);
        audio.clearStreamBuffer();
    }
}
#endif

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {
}

float SoundPlayer::getSoundVolume(const std::string &soundId) {

    return 0.0f;
}

double SoundPlayer::getMusicPosition(const std::string &soundId) {
#ifdef ENABLE_AUDIO

#endif
    return 0.0;
}

void SoundPlayer::setMusicPosition(double position, const std::string &soundId) {
#ifdef ENABLE_AUDIO

#endif
}

void SoundPlayer::stopSound(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = NDS_Sounds.find(soundId);

    if (soundFind != NDS_Sounds.end()) {
        soundFind->second.isPlaying = false;
    }
#endif
}

void SoundPlayer::stopStreamedSound() {
}

void SoundPlayer::checkAudio() {
}

bool SoundPlayer::isSoundPlaying(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = NDS_Sounds.find(soundId);

    if (soundFind != NDS_Sounds.end()) {
        return soundFind->second.isPlaying;
    }
#endif

    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = NDS_Sounds.find(soundId);

    if (soundFind != NDS_Sounds.end()) {
        return true;
    }
#endif
    return false;
}

void SoundPlayer::freeAudio(const std::string &soundId) {
#ifdef ENABLE_AUDIO
    auto soundFind = NDS_Sounds.find(soundId);

    if (soundFind != NDS_Sounds.end()) {
        if (soundFind->second.audioFile) {
            fclose(soundFind->second.audioFile);
            soundFind->second.audioFile = NULL;
        }
        NDS_Sounds.erase(soundId);
    }
#endif
}

void SoundPlayer::flushAudio() {
#ifdef ENABLE_AUDIO
    if (NDS_Sounds.empty()) return;
    std::vector<std::string> toDelete;

    int consumed = (NDS_Audio::stream_buffer_out - NDS_Audio::stream_buffer_in + BUFFER_LENGTH) % BUFFER_LENGTH;

    if (consumed > 4096) {
        for (auto &[id, audio] : NDS_Sounds) {
            if (audio.isPlaying) {
                audio.streamingFillBuffer(false);
                audio.freeTimer = audio.maxFreeTimer;
            } else {
                audio.freeTimer--;
                if (audio.freeTimer <= 0) {
                    toDelete.push_back(audio.id);
                    continue;
                }
            }
        }
    }
    for (std::string &del : toDelete) {
        freeAudio(del);
    }
#endif
}

void SoundPlayer::cleanupAudio() {
#ifdef ENABLE_AUDIO
    std::vector<std::string> toDelete;
    for (auto &[id, audio] : NDS_Sounds) {
        toDelete.push_back(audio.id);
    }
    for (std::string &del : toDelete) {
        freeAudio(del);
    }
    mmStreamClose();
#endif
}

void SoundPlayer::deinit() {
    cleanupAudio();
}
