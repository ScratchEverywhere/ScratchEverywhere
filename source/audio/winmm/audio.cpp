#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "audio_winmm.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <log.hpp>
#include <loudness.hpp>
#include <mmsystem.h>
#include <vector>

#define NUM_BUFFERS 4

/* 8192 / 2 / 2 = 2048. same as SDL2 */
#define BUFFER_SIZE 8192

static HWAVEOUT hWaveOut;
static WAVEHDR headers[NUM_BUFFERS];
static BYTE buffers[NUM_BUFFERS][BUFFER_SIZE];
static int stop;

#ifdef ENABLE_AUDIO
static constexpr int MIC_RATE = 44100;
static constexpr int MIC_NUM_BUFFERS = 4;
static constexpr int MIC_BUFFER_SAMPLES = 2048;

static LoudnessMeter micMeter(MIC_RATE);
static HWAVEIN hWaveIn = nullptr;
static WAVEHDR micHeaders[MIC_NUM_BUFFERS];
static int16_t micBuffers[MIC_NUM_BUFFERS][MIC_BUFFER_SAMPLES];
static bool micTried = false;

static void closeMicrophone() {
    if (!hWaveIn) return;
    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    for (int i = 0; i < MIC_NUM_BUFFERS; i++)
        waveInUnprepareHeader(hWaveIn, &micHeaders[i], sizeof(micHeaders[i]));
    waveInClose(hWaveIn);
    hWaveIn = nullptr;
}
#endif

extern "C" void CALLBACK winmmCallback(HWAVEOUT hWaveOut, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2) {
    WAVEHDR *header;

    if (msg != WOM_DONE) return;
    if (stop) return;

    header = (WAVEHDR *)param1;

    Mixer::requestSound((short *)header->lpData, BUFFER_SIZE / 2 / 2);

    waveOutWrite(hWaveOut, header, sizeof(*header));
}

SoundDevice_Winmm::SoundDevice_Winmm() {
#ifdef ENABLE_AUDIO
    WAVEFORMATEX fmt;
    int i;

    stop = 0;

    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = Mixer::rate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &fmt, (DWORD_PTR)winmmCallback, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        error = "Failed to initialize WinMM: Code " + std::to_string(GetLastError());
        return;
    }

    for (i = 0; i < NUM_BUFFERS; i++) {
        memset(&headers[i], 0, sizeof(headers[i]));
        memset(&buffers[i], 0, sizeof(buffers[i]));

        headers[i].lpData = (LPSTR)buffers[i];
        headers[i].dwBufferLength = BUFFER_SIZE;

        waveOutPrepareHeader(hWaveOut, &headers[i], sizeof(headers[i]));
    }

    for (i = 0; i < NUM_BUFFERS; i++)
        waveOutWrite(hWaveOut, &headers[i], sizeof(headers[i]));

    return;
#endif
    error = "Audio is not enabled.";
}

int SoundDevice_Winmm::getLoudness() {
#ifdef ENABLE_AUDIO
    if (!micTried) {
        micTried = true;

        if (waveInGetNumDevs() > 0) {
            WAVEFORMATEX fmt;
            memset(&fmt, 0, sizeof(fmt));
            fmt.wFormatTag = WAVE_FORMAT_PCM;
            fmt.nChannels = 1;
            fmt.nSamplesPerSec = MIC_RATE;
            fmt.wBitsPerSample = 16;
            fmt.nBlockAlign = fmt.nChannels * fmt.wBitsPerSample / 8;
            fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

            if (waveInOpen(&hWaveIn, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
                for (int i = 0; i < MIC_NUM_BUFFERS; i++) {
                    memset(&micHeaders[i], 0, sizeof(micHeaders[i]));
                    micHeaders[i].lpData = (LPSTR)micBuffers[i];
                    micHeaders[i].dwBufferLength = sizeof(micBuffers[i]);
                    waveInPrepareHeader(hWaveIn, &micHeaders[i], sizeof(micHeaders[i]));
                    waveInAddBuffer(hWaveIn, &micHeaders[i], sizeof(micHeaders[i]));
                }
                waveInStart(hWaveIn);
            } else {
                hWaveIn = nullptr;
                Log::logWarning("Failed to open WinMM microphone.");
            }
        }
    }

    if (!hWaveIn) return -1;

    for (int i = 0; i < MIC_NUM_BUFFERS; i++) {
        if (!(micHeaders[i].dwFlags & WHDR_DONE)) continue;
        micMeter.feed(micBuffers[i], micHeaders[i].dwBytesRecorded / sizeof(int16_t));
        micHeaders[i].dwBytesRecorded = 0;
        waveInAddBuffer(hWaveIn, &micHeaders[i], sizeof(micHeaders[i]));
    }
    return micMeter.get();
#else
    return -1;
#endif
}

SoundDevice_Winmm::~SoundDevice_Winmm() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    closeMicrophone();
    micTried = false;
    stop = 1;
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);
#endif
}
