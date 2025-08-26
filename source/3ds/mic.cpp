#include "mic.hpp"
#include <stdio.h>
#include <math.h>

u8* micbuf = nullptr;
u32 micbuf_datasize = 0;
bool micInitialized = false;

#define SAMPLE_CHUNK 128
#define MIC_SENSITIVITY 30.0 

static s16 sampleBuffer[SAMPLE_CHUNK];

bool initMic() {
    if(micInitialized) return true;

    u32 micbuf_size = 0x30000;
    micbuf = (u8*)memalign(0x1000, micbuf_size);
    if(!micbuf) return false;

    if(R_FAILED(csndInit())) return false;
    if(R_FAILED(micInit(micbuf, micbuf_size))) return false;

    micbuf_datasize = micGetSampleDataSize();
    if(R_SUCCEEDED(MICU_StartSampling(MICU_ENCODING_PCM16_SIGNED, MICU_SAMPLE_RATE_16360, 0, micbuf_datasize, true)))
        micInitialized = true;

    return micInitialized;
}

int getMicLevel() {
    if(!micInitialized) return 0;

    u32 lastOffset = micGetLastSampleOffset();
    u32 readPos = (lastOffset + micbuf_datasize - SAMPLE_CHUNK) % micbuf_datasize;
    size_t count = 0;

    while(count < SAMPLE_CHUNK) {
        sampleBuffer[count] = ((s16*)micbuf)[readPos];
        readPos = (readPos + 1) % micbuf_datasize;
        count++;
    }

    double sum = 0.0;
    for(size_t i = 0; i < count; i++)
        sum += sampleBuffer[i] * sampleBuffer[i];

    double rms = sqrt(sum / count);

    int level = (int)((rms / 32767.0) * 100.0 * MIC_SENSITIVITY);
    if(level > 100) level = 100;
    return level;
}

void exitMic() {
    if(!micInitialized) return;
    MICU_StopSampling();
    micExit();
    csndExit();
    free(micbuf);
    micInitialized = false;
}