#pragma once
#include <stdint.h>
#include "acrux_epilogue.h"

// ACRUX procedural audio core. No audio files, no external runtime library.
// Music: approved v8 structure, 62 BPM, ~61.94 s loop, 4 relay layer states.
// Epilogue: full Relay-3 base + procedural seasonal overlays, no shipped audio media.
// Output: stereo signed 16-bit PCM at ACRUX_AUDIO_SAMPLE_RATE.

#define ACRUX_AUDIO_SAMPLE_RATE 11025

enum AcruxSfx {
    ACRUX_SFX_PICKUP = 0,
    ACRUX_SFX_BLINK,
    ACRUX_SFX_MOTH_ALERT,
    ACRUX_SFX_MOTH_CONTACT,
    ACRUX_SFX_BOAR_ALERT,
    ACRUX_SFX_BOAR_CONTACT,
    ACRUX_SFX_PATH_BREAK,
    ACRUX_SFX_REPAIR,
    ACRUX_SFX_RELAY,
    ACRUX_SFX_STAR,
    ACRUX_SFX_FAILURE,
    ACRUX_SFX_RUSTLE,
    ACRUX_SFX_SNARE,
    ACRUX_SFX_PATH_PLACE,
    ACRUX_SFX_PATH_REMOVE,
    ACRUX_SFX_SLEEP,
    ACRUX_SFX_DAY_TRANSITION,
    ACRUX_SFX_COUNT
};

bool AcruxSynth_Init();
void AcruxSynth_Shutdown();
void AcruxSynth_SetRelayCount(int relayCount);          // 0..3, preserves song position
void AcruxSynth_EnterEpilogue(AcruxSeason firstSeason);          // normal ending handoff: 16-beat blend-in
void AcruxSynth_EnterEpilogueImmediate(AcruxSeason season);      // load/resume already inside epilogue
void AcruxSynth_SetEpilogueSeason(AcruxSeason season);
void AcruxSynth_SetEpilogueSeasonImmediate(AcruxSeason season);  // load/debug only; no morph
void AcruxSynth_ExitEpilogue();
bool AcruxSynth_IsEpilogue();
AcruxSeason AcruxSynth_CurrentSeason();
void AcruxSynth_ResetPlayback();                         // new run/retry: beat 0, relay 0, clear SFX/hum
void AcruxSynth_SetMusicGain(float gain);               // 0..1
void AcruxSynth_SetPathHum(float amount);               // 0..1, proximity/intensity
void AcruxSynth_PlaySfx(AcruxSfx id, float pan = 0.0f); // pan -1..+1
void AcruxSynth_RenderStereo(int16_t* outInterleavedLR, int frames);

int AcruxSynth_LoopFrames();
