#include "../scratch/audio.hpp"
#include "../scratch/os.hpp"
#include "audio.hpp"
#include "interpret.hpp"
#include "miniz/miniz.h"

std::unordered_map<std::string, Sound> SoundPlayer::soundsPlaying;
std::unordered_map<std::string, NDS_Audio> NDS_Sounds;
FILE *NDS_Audio::streamedFile = NULL;
char NDS_Audio::stream_buffer[BUFFER_LENGTH];
int NDS_Audio::stream_buffer_in;
int NDS_Audio::stream_buffer_out;
bool NDS_Audio::isStreaming = false;
NDS_Audio streamedSound;

bool NDS_Audio::init() {
    return true;
}

mm_word NDS_Audio::streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format) {
    size_t multiplier = 0;

    if (format == MM_STREAM_8BIT_MONO)
        multiplier = 1;
    else if (format == MM_STREAM_8BIT_STEREO)
        multiplier = 2;
    else if (format == MM_STREAM_16BIT_MONO)
        multiplier = 2;
    else if (format == MM_STREAM_16BIT_STEREO)
        multiplier = 4;

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

void NDS_Audio::readFile(char *buffer, size_t size) {
    while (size > 0) {
        int res = fread(buffer, 1, size, streamedFile);
        size -= res;
        buffer += res;

        if (feof(streamedFile)) {
            // Loop back when song ends
            // fseek(streamedFile, sizeof(WAVHeader_t), SEEK_SET);
            // res = fread(buffer, 1, size, streamedFile);
            // size -= res;
            // buffer += res;

            // printf("Restarting...\n");
            break;
        }
    }
}

void NDS_Audio::streamingFillBuffer(bool force_fill) {
    if (!force_fill) {
        if (stream_buffer_in == stream_buffer_out)
            return;
    }

    if (stream_buffer_in < stream_buffer_out) {
        size_t size = stream_buffer_out - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size);
        stream_buffer_in += size;
    } else {
        size_t size = BUFFER_LENGTH - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size);
        stream_buffer_in = 0;

        size = stream_buffer_out - stream_buffer_in;
        readFile(&stream_buffer[stream_buffer_in], size);
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

void SoundPlayer::startSoundLoaderThread(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId) {

    if (projectType != UNZIPPED)
        loadSoundFromSB3(sprite, zip, soundId, true);
    else
        loadSoundFromFile(sprite, "project/" + soundId, true);
}

bool SoundPlayer::loadSoundFromSB3(Sprite *sprite, mz_zip_archive *zip, const std::string &soundId, const bool &streamed) {

    return false;
}

bool SoundPlayer::loadSoundFromFile(Sprite *sprite, std::string fileName, const bool &streamed) {

    fileName = OS::getRomFSLocation() + fileName;

    if (streamed) {
        NDS_Audio::streamedFile = fopen(fileName.c_str(), "rb");
        if (NDS_Audio::streamedFile == NULL) {
            Log::logError("Sound not found. " + fileName);
            return false;
        }

        WAVHeader_t wavHeader = {0};
        if (fread(&wavHeader, 1, sizeof(WAVHeader_t), NDS_Audio::streamedFile) != sizeof(WAVHeader_t)) {
            Log::logError("Failed to read WAV header.");
            return false;
        }
        if (NDS_Audio::checkWAVHeader(wavHeader) != 0) {
            Log::logError("WAV file header is corrupt! Make sure it is in the correct PCM format!");
            return false;
        }

        // Fill the buffer before we start doing anything
        NDS_Audio::streamingFillBuffer(true);

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
                .format = NDS_Audio::getMMStreamType(wavHeader.numChannels, wavHeader.bitsPerSample),
                .timer = MM_TIMER2,
                .manual = false,
            };
        mmStreamOpen(&stream);
        NDS_Audio::isStreaming = true;
        return true;
    }

    return false;
}

int SoundPlayer::playSound(const std::string &soundId) {

    return -1;
}

void SoundPlayer::setSoundVolume(const std::string &soundId, float volume) {
}

float SoundPlayer::getSoundVolume(const std::string &soundId) {

    return 0.0f;
}

void SoundPlayer::stopSound(const std::string &soundId) {
}

void SoundPlayer::stopStreamedSound() {
}

void SoundPlayer::checkAudio() {
}

bool SoundPlayer::isSoundPlaying(const std::string &soundId) {

    return false;
}

bool SoundPlayer::isSoundLoaded(const std::string &soundId) {

    return false;
}

void SoundPlayer::freeAudio(const std::string &soundId) {
}

void SoundPlayer::flushAudio() {

    // if (NDS_Audio::isStreaming) {
    //     NDS_Audio::streamingFillBuffer(false);
    // }

    if (!NDS_Audio::isStreaming) return;

    // Only refill if callback has consumed at least 4KB
    int consumed = (NDS_Audio::stream_buffer_out - NDS_Audio::stream_buffer_in + BUFFER_LENGTH) % BUFFER_LENGTH;

    if (consumed > 4096 * 2) { // Only refill when 4KB+ consumed
        streamedSound.streamingFillBuffer(false);
    }
}

void SoundPlayer::cleanupAudio() {
}

void SoundPlayer::deinit() {
}