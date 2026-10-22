#include "title_assets.h"

namespace lc {
namespace {

#include "generated/title_assets.inc"

}  // namespace

const std::uint8_t* TitleBackgroundPixels() {
    return kTitleBackgroundPixels;
}

std::uint32_t TitleBackgroundPaletteColor(std::uint8_t index) {
    if (index >= 24u) return 0u;
    const int offset = static_cast<int>(index) * 3;
    return (static_cast<std::uint32_t>(kTitleBackgroundPaletteRgb[offset]) << 16u) |
           (static_cast<std::uint32_t>(kTitleBackgroundPaletteRgb[offset + 1]) << 8u) |
           static_cast<std::uint32_t>(kTitleBackgroundPaletteRgb[offset + 2]);
}

}  // namespace lc
