#pragma once

namespace lc {

struct AudioSynthState {
    float phaseA;
    float phaseB;
    float pulsePhase;
};

void GenerateAudio(AudioSynthState& state, short* samples, int sampleCount, int sampleRate,
                   float baseEnergyRatio, int connectedRelays, bool blinkActive);

}  // namespace lc
