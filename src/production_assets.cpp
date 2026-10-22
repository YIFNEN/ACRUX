#include "production_assets.h"

#include <cstddef>

namespace lc {
namespace {

#include "generated/production_assets.inc"

constexpr std::size_t kFrameCount = sizeof(kProductionFrames) / sizeof(kProductionFrames[0]);

}  // namespace

const ProductionFrame& GetProductionFrame(ProductionSpriteId id) {
    const std::size_t index = static_cast<std::size_t>(id);
    return kProductionFrames[index < kFrameCount ? index : 0];
}

std::uint8_t SampleProductionIndex(const ProductionFrame& frame, int x, int y, bool mirrorX) {
    if (x < 0 || y < 0 || x >= frame.width || y >= frame.height) return 0;
    const int sourceX = mirrorX ? static_cast<int>(frame.width) - 1 - x : x;
    const std::size_t pixelIndex = frame.pixelOffset +
        static_cast<std::size_t>(y) * frame.width + static_cast<std::size_t>(sourceX);
    const std::size_t bitIndex = pixelIndex * 5u;
    const std::size_t byteIndex = bitIndex / 8u;
    const unsigned bitOffset = static_cast<unsigned>(bitIndex & 7u);
    const std::uint16_t window = static_cast<std::uint16_t>(kProductionBytes[byteIndex]) << 8u |
        static_cast<std::uint16_t>(byteIndex + 1u < sizeof(kProductionBytes)
            ? kProductionBytes[byteIndex + 1u] : 0u);
    return static_cast<std::uint8_t>((window >> (11u - bitOffset)) & 0x1Fu);
}

std::uint32_t ProductionPaletteColor(const ProductionFrame& frame, std::uint8_t index) {
    constexpr std::size_t kPaletteCount = sizeof(kProductionPalettes) / sizeof(kProductionPalettes[0]);
    return index < 32u && frame.palette < kPaletteCount ? kProductionPalettes[frame.palette][index] : 0u;
}

}  // namespace lc
