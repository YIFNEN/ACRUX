#include "acrux_waveout.h"
#include "acrux_synth.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

namespace {
constexpr int BLOCK_FRAMES = 1024;
constexpr int BLOCKS = 4;
static HWAVEOUT g_wave = nullptr;
static WAVEHDR g_hdr[BLOCKS] = {};
static int16_t g_pcm[BLOCKS][BLOCK_FRAMES*2] = {};
static bool g_ready = false;

static bool submit(int i) {
    AcruxSynth_RenderStereo(g_pcm[i], BLOCK_FRAMES);
    g_hdr[i].lpData = (LPSTR)g_pcm[i];
    g_hdr[i].dwBufferLength = sizeof(g_pcm[i]);
    g_hdr[i].dwFlags &= ~WHDR_DONE;
    return waveOutWrite(g_wave, &g_hdr[i], sizeof(WAVEHDR)) == MMSYSERR_NOERROR;
}
}

bool AcruxAudio_Init() {
    if (g_ready) return true;
    if (!AcruxSynth_Init()) return false;

    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = ACRUX_AUDIO_SAMPLE_RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = fmt.nChannels * (fmt.wBitsPerSample / 8);
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

    if (waveOutOpen(&g_wave, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        AcruxSynth_Shutdown();
        return false;
    }

    for (int i=0;i<BLOCKS;i++) {
        g_hdr[i] = {};
        g_hdr[i].lpData = (LPSTR)g_pcm[i];
        g_hdr[i].dwBufferLength = sizeof(g_pcm[i]);
        if (waveOutPrepareHeader(g_wave, &g_hdr[i], sizeof(WAVEHDR)) != MMSYSERR_NOERROR || !submit(i)) {
            AcruxAudio_Shutdown();
            return false;
        }
    }
    g_ready = true;
    return true;
}

void AcruxAudio_Update() {
    if (!g_ready) return;
    for (int i=0;i<BLOCKS;i++) {
        if (g_hdr[i].dwFlags & WHDR_DONE) submit(i);
    }
}

void AcruxAudio_SetPaused(bool paused) {
    if (!g_ready || !g_wave) return;
    if (paused) waveOutPause(g_wave); else waveOutRestart(g_wave);
}

void AcruxAudio_Shutdown() {
    if (g_wave) {
        waveOutReset(g_wave);
        for (int i=0;i<BLOCKS;i++) {
            if (g_hdr[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(g_wave, &g_hdr[i], sizeof(WAVEHDR));
        }
        waveOutClose(g_wave);
        g_wave = nullptr;
    }
    AcruxSynth_Shutdown();
    g_ready = false;
}
#else
bool AcruxAudio_Init() { return AcruxSynth_Init(); }
void AcruxAudio_Update() {}
void AcruxAudio_SetPaused(bool) {}
void AcruxAudio_Shutdown() { AcruxSynth_Shutdown(); }
#endif

