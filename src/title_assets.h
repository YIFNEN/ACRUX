#pragma once

#include <cstdint>

namespace lc {

constexpr int kTitleBackgroundWidth = 320;
constexpr int kTitleBackgroundHeight = 180;

const std::uint8_t* TitleBackgroundPixels();
std::uint32_t TitleBackgroundPaletteColor(std::uint8_t index);

}  // namespace lc
