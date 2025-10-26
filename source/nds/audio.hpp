#ifdef ENABLE_AUDIO
#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#endif

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
    std::string id;
    FILE *audioFile = NULL;
    bool isPlaying = false;
    static char stream_buffer[BUFFER_LENGTH];
    static int stream_buffer_in;
    static int stream_buffer_out;
    size_t maxFreeTimer = 5;
    size_t freeTimer = maxFreeTimer;

    static bool init();
#ifdef ENABLE_AUDIO
    static mm_word streamingCallback(mm_word length, mm_addr dest, mm_stream_formats format);
    static void stopAllSounds();
    void readFile(char *buffer, size_t size, bool restartSound = false);
    void streamingFillBuffer(bool force_fill, bool restartSound = false);
    void clearStreamBuffer();
    int checkWAVHeader(const WAVHeader_t header);
    mm_stream_formats getMMStreamType(uint16_t numChannels, uint16_t bitsPerSample);
#endif
};

extern std::unordered_map<std::string, NDS_Audio> NDS_Sounds;