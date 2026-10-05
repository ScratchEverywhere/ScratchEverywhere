#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "audio.hpp"
#include "audiostack.hpp"
#include "os.hpp"
#include <log.hpp>
#include <mmsystem.h>
#include <vector>

#define NUM_BUFFERS 4

/* 8192 / 2 / 2 = 2048. same as SDL2 */
#define BUFFER_SIZE 8192

static HWAVEOUT hWaveOut;
static WAVEHDR headers[NUM_BUFFERS];
static BYTE buffers[NUM_BUFFERS][BUFFER_SIZE];
static int stop;

extern "C" void CALLBACK callback(HWAVEOUT hWaveOut, UINT msg, DWORD_PTR instance, DWORD_PTR param1, DWORD_PTR param2) {
    WAVEHDR *header;

    if (msg != WOM_DONE) return;
    if (stop) return;

    header = (WAVEHDR *)param1;

    Mixer::requestSound((short *)header->lpData, BUFFER_SIZE / 2 / 2);

    waveOutWrite(hWaveOut, header, sizeof(*header));
}

bool SoundPlayer::init() {
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

    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &fmt, (DWORD_PTR)callback, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        Log::logCritical("Failed to initialize WinMM: Code " + std::to_string(GetLastError()), true);
        return false;
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

    return true;
#endif
    return false;
}

void SoundPlayer::deinit() {
#ifdef ENABLE_AUDIO
    Mixer::cleanupAudio();
    stop = 1;
    waveOutReset(hWaveOut);
    waveOutClose(hWaveOut);
#endif
}
