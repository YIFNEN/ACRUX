#include "atlas_data.h"

namespace lc {
namespace {

#include "generated/atlas_data.inc"

constexpr std::size_t kSpriteFrameCount = sizeof(kSpriteFrames) / sizeof(kSpriteFrames[0]);

}  // namespace

const SpriteFrame& GetSpriteFrame(SpriteId id) {
    const std::size_t index = static_cast<std::size_t>(id);
    return kSpriteFrames[index < kSpriteFrameCount ? index : 0];
}

std::uint8_t SampleSpriteIndex(const SpriteFrame& frame, int x, int y, bool mirrorX) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) return 0;
    const int sourceX = mirrorX ? static_cast<int>(frame.width) - 1 - x : x;
    const std::size_t pixelIndex =
        static_cast<std::size_t>(frame.pixelOffset) +
        static_cast<std::size_t>(frame.y + y) * frame.sheetWidth +
        static_cast<std::size_t>(frame.x + sourceX);
    const std::size_t bitIndex = pixelIndex * 5u;
    const std::size_t byteIndex = bitIndex / 8u;
    const unsigned bitOffset = static_cast<unsigned>(bitIndex & 7u);
    const std::uint16_t window = static_cast<std::uint16_t>(kAtlasBytes[byteIndex]) << 8u |
                                 static_cast<std::uint16_t>(
                                     byteIndex + 1u < sizeof(kAtlasBytes) ? kAtlasBytes[byteIndex + 1u] : 0u);
    return static_cast<std::uint8_t>((window >> (11u - bitOffset)) & 0x1Fu);
}

std::uint32_t AtlasPaletteColor(std::uint8_t index) {
    return index < 32u ? kPlayerPalette[index] : 0u;
}

}  // namespace lc
