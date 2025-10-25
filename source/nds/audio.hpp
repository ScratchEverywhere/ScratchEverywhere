#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>

#define DATA_ID 0x61746164
#define FMT_ID 0x20746d66
#define RIFF_ID 0x46464952
#define WAVE_ID 0x45564157
#define BUFFER_LENGTH 16384

typedef struct WAVHeader {
    // "RIFF" chunk descriptor
    uint32_t chunkID;
    uint32_t chunkSize;
    uint32_t format;
    // "fmt" subchunk
    uint32_t subchunk1ID;
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    // "data" subchunk
    uint32_t subchunk2ID;
    uint32_t subchunk2Size;
} WAVHeader_t;

class NDS_Audio {
  public:
    static FILE *streamedFile;
    static bool isStreaming;
    static char stream_buffer[BUFFER_LENGTH];
    static int stream_buffer_in;
    static int stream_buffer_out;

    bool init();
    static mm_word streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format);
    static void readFile(char *buffer, size_t size);
    static void streamingFillBuffer(bool force_fill);
    static int checkWAVHeader(const WAVHeader_t header);
    static mm_stream_formats getMMStreamType(uint16_t numChannels, uint16_t bitsPerSample);
};

extern NDS_Audio streamedSound;