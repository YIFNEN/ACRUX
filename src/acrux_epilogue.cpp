#include "acrux_epilogue.h"
#include "acrux_synth.h"

#include <math.h>
#include <stdlib.h>

namespace {
constexpr int SR = ACRUX_AUDIO_SAMPLE_RATE;
constexpr float BPM = 62.0f;
constexpr float BEAT = 60.0f / BPM;
constexpr float TWO_PI = 6.2831853071795864769f;

static int g_loopFrames = 0;
static int16_t* g_overlay[ACRUX_SEASON_COUNT] = {};
static float g_sine[256];

static int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
static float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static int frameAtBeat(float beat) { return static_cast<int>(beat * BEAT * SR + 0.5f); }
static float midiHz(int midi) {
    return 440.0f * powf(2.0f, (static_cast<float>(clampi(midi, 0, 127)) - 69.0f) / 12.0f);
}

static const uint8_t kProg[32][4] = {
    {45,48,52,59},{41,45,48,52},{48,52,55,59},{43,47,50,57},
    {45,48,52,55},{41,45,52,55},{48,52,57,59},{43,47,52,57},
    {45,48,52,59},{41,48,52,57},{48,52,55,62},{43,47,52,59},
    {45,52,55,59},{41,45,48,55},{48,52,59,64},{43,50,52,57},
    {40,47,52,59},{41,48,52,57},{38,45,50,57},{43,47,52,59},
    {45,48,52,59},{41,45,52,55},{48,52,55,59},{43,47,50,57},
    {45,48,52,59},{41,45,48,52},{48,52,55,59},{43,47,50,57},
    {45,48,52,55},{41,45,52,55},{48,52,57,59},{43,47,52,57}
};

static int chordMidi(float beat, int rank, int octaveAdd) {
    const int bar = clampi(static_cast<int>(beat * 0.5f), 0, 31);
    return static_cast<int>(kProg[bar][rank & 3]) + octaveAdd;
}

static float envAt(int i, int n, int a, int d, float s, int r) {
    if (i < a) return a > 0 ? static_cast<float>(i) / static_cast<float>(a) : 1.0f;
    if (i < a + d) {
        const float q = d > 0 ? static_cast<float>(i - a) / static_cast<float>(d) : 1.0f;
        return 1.0f + (s - 1.0f) * q;
    }
    if (i < n - r) return s;
    if (r <= 0) return 0.0f;
    const float q = static_cast<float>(i - (n - r)) / static_cast<float>(r);
    return s * (1.0f - q);
}

enum WaveKind { W_SINE = 0, W_TRI = 1 };

static void addTone(float* dst, int start, float durSec, float f0, float f1, WaveKind wave,
                    float amp, float aSec, float dSec, float sustain, float rSec,
                    float vib = 0.0f, float vibHz = 4.6f) {
    int frames = static_cast<int>(durSec * SR + 0.5f);
    if (start < 0 || start >= g_loopFrames || frames <= 0) return;
    if (start + frames > g_loopFrames) frames = g_loopFrames - start;
    const int a = static_cast<int>(aSec * SR);
    const int d = static_cast<int>(dSec * SR);
    const int r = static_cast<int>(rSec * SR);
    uint32_t phase = 0;
    for (int i = 0; i < frames; ++i) {
        const float u = frames > 1 ? static_cast<float>(i) / static_cast<float>(frames - 1) : 0.0f;
        float f = f0 + (f1 - f0) * u;
        if (vib != 0.0f) f *= 1.0f + vib * sinf(TWO_PI * vibHz * (static_cast<float>(i) / SR));
        phase += static_cast<uint32_t>(f * (4294967296.0 / static_cast<double>(SR)));
        float v = 0.0f;
        if (wave == W_SINE) {
            v = g_sine[phase >> 24];
        } else {
            const float x = static_cast<float>(phase >> 16) * (1.0f / 65536.0f);
            v = 4.0f * (x < 0.5f ? (0.5f - x) : (x - 0.5f)) - 1.0f;
        }
        dst[start + i] += v * amp * envAt(i, frames, a, d, sustain, r);
    }
}

static void addMidi(float* dst, float beat, float dur, int midi, WaveKind wave, float amp,
                    float a, float d, float s, float r, float vib = 0.0f, float vibHz = 4.6f) {
    const float f = midiHz(midi);
    addTone(dst, frameAtBeat(beat), dur, f, f, wave, amp, a, d, s, r, vib, vibHz);
}

static void addMidiSweep(float* dst, float beat, float dur, int midi0, int midi1,
                         WaveKind wave, float amp, float a, float d, float s, float r) {
    addTone(dst, frameAtBeat(beat), dur, midiHz(midi0), midiHz(midi1), wave, amp, a, d, s, r);
}

static void renderWinter(float* b) {
    for (int base = 2; base < 64; base += 4) {
        const int m0 = chordMidi(static_cast<float>(base), 3, 24);
        const int m1 = chordMidi(static_cast<float>(base) + 0.5f, 2, 24);
        addMidiSweep(b, static_cast<float>(base), 0.34f, m0, m0 - 2, W_SINE,
                     0.033f, 0.005f, 0.04f, 0.035f, 0.23f);
        addMidi(b, static_cast<float>(base), 0.34f, m0 + 7, W_SINE,
                0.0045f, 0.004f, 0.03f, 0.008f, 0.18f);
        addMidiSweep(b, static_cast<float>(base) + 0.5f, 0.34f, m1, m1 - 2, W_SINE,
                     0.027f, 0.005f, 0.04f, 0.032f, 0.22f);
    }
    for (int base = 0; base < 64; base += 8) {
        addMidi(b, static_cast<float>(base), 0.50f, chordMidi(static_cast<float>(base), 0, -12),
                W_SINE, 0.026f, 0.04f, 0.08f, 0.12f, 0.26f);
    }
}

static void addMallet(float* b, float beat, int midi, float gain) {
    addMidi(b, beat, 0.24f, midi, W_TRI, 0.051f * gain, 0.001f, 0.04f, 0.040f, 0.11f);
    addMidi(b, beat, 0.24f, midi + 12, W_SINE, 0.006f * gain, 0.001f, 0.02f, 0.008f, 0.065f);
    addMidi(b, beat + 0.5f, 0.16f, midi, W_TRI, 0.0018f * gain, 0.001f, 0.025f, 0.012f, 0.07f);
}

static void renderSpring(float* b) {
    for (int base = 1; base < 64; base += 4) {
        const float off[4] = {0.0f, 0.33f, 0.66f, 0.99f};
        const int ranks[4] = {1, 2, 3, 2};
        for (int k = 0; k < 4; ++k) {
            const float beat = static_cast<float>(base) + off[k];
            addMallet(b, beat, chordMidi(beat, ranks[k], 0), 1.0f);
        }
    }
    for (int base = 3; base < 64; base += 8) {
        const float beat = static_cast<float>(base);
        addMidi(b, beat, 0.42f, chordMidi(beat, 3, 0), W_SINE,
                0.022f, 0.03f, 0.06f, 0.08f, 0.24f, 0.0008f, 4.2f);
    }
}

static void addFirefly(float* b, float beat, int midi, float gain) {
    addMidi(b, beat, 0.31f, midi, W_SINE, 0.044f * gain, 0.014f, 0.04f, 0.06f, 0.17f, 0.0012f, 3.6f);
    addMidi(b, beat, 0.31f, midi + 12, W_TRI, 0.0025f * gain, 0.010f, 0.025f, 0.007f, 0.11f);
}

static void addWarmPluck(float* b, float beat, int midi) {
    addMidi(b, beat, 0.22f, midi, W_TRI, 0.034f, 0.001f, 0.035f, 0.030f, 0.10f);
    addMidi(b, beat, 0.22f, midi + 12, W_SINE, 0.004f, 0.001f, 0.02f, 0.006f, 0.06f);
}

static void renderSummer(float* b) {
    for (int base = 0; base < 64; base += 4) {
        const float off[3] = {0.0f, 0.75f, 1.5f};
        const int ranks[3] = {2, 3, 1};
        const float gain[3] = {1.0f, 0.84f, 0.75f};
        for (int k = 0; k < 3; ++k) {
            const float beat = static_cast<float>(base) + off[k];
            addFirefly(b, beat, chordMidi(beat, ranks[k], 24), gain[k]);
        }
    }
    for (float beat = 2.5f; beat < 64.0f; beat += 4.0f) {
        addWarmPluck(b, beat, chordMidi(beat, 1, 0));
    }
    const float blink[] = {7.25f, 15.25f, 23.25f, 31.25f, 55.25f, 63.25f};
    for (float beat : blink) addFirefly(b, beat, chordMidi(beat, 3, 24), 0.88f);
}

static void addFlute(float* b, float beat, int midi, float gain) {
    addMidi(b, beat, 0.72f, midi, W_SINE, 0.050f * gain, 0.055f, 0.09f, 0.24f, 0.30f, 0.0028f, 4.8f);
    addMidi(b, beat, 0.72f, midi + 12, W_SINE, 0.0033f * gain, 0.060f, 0.08f, 0.045f, 0.24f, 0.0015f, 4.8f);
    addMidi(b, beat, 0.72f, midi + 19, W_SINE, 0.0014f * gain, 0.060f, 0.07f, 0.022f, 0.20f);
}

static void renderAutumn(float* b) {
    for (int base = 0; base < 64; base += 8) {
        const float start = static_cast<float>(base) + 0.25f;
        const float off[4] = {0.0f, 0.38f, 0.76f, 1.14f};
        const int ranks[4] = {3, 2, 1, 0};
        for (int k = 0; k < 4; ++k) {
            const float beat = start + off[k];
            addFlute(b, beat, chordMidi(beat, ranks[k], 24), 1.0f);
        }
    }
    for (float beat = 4.5f; beat < 64.0f; beat += 8.0f) {
        addMidi(b, beat, 0.58f, chordMidi(beat, 1, 0), W_SINE,
                0.024f, 0.04f, 0.07f, 0.13f, 0.27f, 0.0018f, 4.6f);
    }
}

static bool buildOverlay(int season) {
    float* b = static_cast<float*>(calloc(static_cast<size_t>(g_loopFrames), sizeof(float)));
    if (!b) return false;
    if (season == ACRUX_SEASON_WINTER) renderWinter(b);
    else if (season == ACRUX_SEASON_SPRING) renderSpring(b);
    else if (season == ACRUX_SEASON_SUMMER) renderSummer(b);
    else renderAutumn(b);

    const float seasonalScale[4] = {1.00f, 1.00f, 0.92f, 0.98f};
    g_overlay[season] = static_cast<int16_t*>(malloc(static_cast<size_t>(g_loopFrames) * sizeof(int16_t)));
    if (!g_overlay[season]) {
        free(b);
        return false;
    }
    for (int i = 0; i < g_loopFrames; ++i) {
        const float v = clampf(b[i] * seasonalScale[season], -0.30f, 0.30f);
        g_overlay[season][i] = static_cast<int16_t>(v * 32767.0f);
    }
    free(b);
    return true;
}
}

bool AcruxEpilogue_Init(int loopFrames) {
    g_loopFrames = loopFrames;
    for (int i = 0; i < 256; ++i) g_sine[i] = sinf(TWO_PI * static_cast<float>(i) / 256.0f);
    for (int i = 0; i < ACRUX_SEASON_COUNT; ++i) {
        if (!buildOverlay(i)) {
            AcruxEpilogue_Shutdown();
            return false;
        }
    }
    return true;
}

void AcruxEpilogue_Shutdown() {
    for (int i = 0; i < ACRUX_SEASON_COUNT; ++i) {
        free(g_overlay[i]);
        g_overlay[i] = nullptr;
    }
    g_loopFrames = 0;
}

int16_t AcruxEpilogue_Sample(AcruxSeason season, int frame) {
    if (season < 0 || season >= ACRUX_SEASON_COUNT ||
        frame < 0 || frame >= g_loopFrames || !g_overlay[season]) {
        return 0;
    }
    return g_overlay[season][frame];
}
