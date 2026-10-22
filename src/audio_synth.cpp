#include "audio_synth.h"

#include <cmath>

namespace lc {

namespace {

constexpr float kPi = 3.1415926535f;

float WrapPhase(float phase) {
    const float turn = 2.0f * kPi;
    return phase >= turn ? phase - turn : phase;
}

float Clamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

}  // namespace

void GenerateAudio(AudioSynthState& state, short* samples, int sampleCount, int sampleRate,
                   float baseEnergyRatio, int connectedRelays, bool blinkActive) {
    if (samples == nullptr || sampleCount <= 0 || sampleRate <= 0) return;
    const float energy = Clamp(baseEnergyRatio, 0.0f, 1.0f);
    const int relays = connectedRelays < 0 ? 0 : (connectedRelays > 3 ? 3 : connectedRelays);
    const float frequencyA = 48.0f + 7.0f * static_cast<float>(relays);
    const float frequencyB = 72.0f + 13.0f * static_cast<float>(relays);
    const float stepA = 2.0f * kPi * frequencyA / static_cast<float>(sampleRate);
    const float stepB = 2.0f * kPi * frequencyB / static_cast<float>(sampleRate);
    const float pulseStep = 2.0f * kPi * (1.2f + 0.25f * relays) / static_cast<float>(sampleRate);
    const float volume = 2400.0f + 2600.0f * energy;

    for (int index = 0; index < sampleCount; ++index) {
        const float drone = std::sin(state.phaseA) * 0.62f + std::sin(state.phaseB) * 0.28f;
        const float pulse = std::sin(state.pulsePhase) > 0.92f ? 0.24f : 0.0f;
        const float blink = blinkActive ? std::sin(state.phaseB * 5.0f) * 0.32f : 0.0f;
        samples[index] = static_cast<short>((drone + pulse + blink) * volume);
        state.phaseA = WrapPhase(state.phaseA + stepA);
        state.phaseB = WrapPhase(state.phaseB + stepB);
        state.pulsePhase = WrapPhase(state.pulsePhase + pulseStep);
    }
}

}  // namespace lc
