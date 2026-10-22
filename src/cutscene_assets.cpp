#include "cutscene_assets.h"

#include "generated/cutscene_assets.inc"

namespace lc {

const std::uint8_t* CutscenePixels(CutsceneId id) {
    return kCutsceneBytes + kCutsceneOffsets[static_cast<int>(id)];
}

std::uint32_t CutscenePaletteColor(CutsceneId id, std::uint8_t index) {
    const int offset = (static_cast<int>(id) * 256 + static_cast<int>(index)) * 3;
    return (static_cast<std::uint32_t>(kCutscenePaletteRgb[offset]) << 16u) |
           (static_cast<std::uint32_t>(kCutscenePaletteRgb[offset + 1]) << 8u) |
           static_cast<std::uint32_t>(kCutscenePaletteRgb[offset + 2]);
}

}  // namespace lc
