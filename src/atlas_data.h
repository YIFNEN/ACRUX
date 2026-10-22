#pragma once

#include <cstddef>
#include <cstdint>

namespace lc {

enum class SpriteId : std::uint8_t {
    PlayerFrontIdle,
    PlayerFrontWalk1,
    PlayerFrontWalk2,
    PlayerFrontWalk3,
    PlayerBackIdle,
    PlayerBackWalk1,
    PlayerBackWalk2,
    PlayerBackWalk3,
    PlayerSideIdle,
    PlayerSideWalk1,
    PlayerSideWalk2,
    PlayerSideWalk3,
    PlayerBlink1,
    PlayerBlink2,
    PlayerTake1,
    PlayerTake2
};

struct SpriteFrame {
    std::uint32_t pixelOffset;
    std::uint16_t sheetWidth;
    std::uint16_t x;
    std::uint16_t y;
    std::uint8_t width;
    std::uint8_t height;
    std::int8_t anchorX;
    std::int8_t anchorY;
};

const SpriteFrame& GetSpriteFrame(SpriteId id);
std::uint8_t SampleSpriteIndex(const SpriteFrame& frame, int x, int y, bool mirrorX);
std::uint32_t AtlasPaletteColor(std::uint8_t index);

}  // namespace lc
