#pragma once

#include <cstdint>

namespace lc {

enum class CutsceneId : std::uint8_t {
    Sleep,
    SolarCharge,
    Dusk,
    Ending,
};

constexpr int kCutsceneWidth = 480;
constexpr int kCutsceneHeight = 270;

const std::uint8_t* CutscenePixels(CutsceneId id);
std::uint32_t CutscenePaletteColor(CutsceneId id, std::uint8_t index);

}  // namespace lc
