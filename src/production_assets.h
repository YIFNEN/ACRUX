#pragma once

#include <cstdint>

namespace lc {

enum class ProductionSpriteId : std::uint8_t {
    Ground1, Ground2, Ground3, Ground4,
    Rock1, Rock2, Rock3,
    Bush1, Bush2, Bush3,
    PathUncharged, PathCharged, PathBroken,
    Food, Water, PathMaterial,
    BaseObservatory, Relay1, Relay2, Relay3,
    Relay1Connected, Relay2Connected, Relay3Connected,
    RustleReady, RustleTriggered, SnareReady, SnareTriggered,
    MothIdleA, MothIdleB, MothAlert, MothCharge, MothStunned,
    BoarBaseA, BoarBaseB, BoarChargePrep1, BoarChargePrep2,
    BoarChargeDash, BoarStunnedDrop, BoarStunnedLoop,
    BaseInteraction1, BaseInteraction2,
    PathScrap,
    FxChargeManual, FxBoarPathImpact, FxBoarStunLoop,
    UiFoodCritical, UiWaterCritical,
    TitleLogo, Landmarks,
    HudPersonalLight, HudBaseEnergy, HudRelay, HudBlink,
    HudFood, HudWater, HudPathTiles, HudNightCycle,
    PathStabilized, PathBreakFxAmber,
    PuddleNormal, PuddleRippleVariants,
    SouthernCrossReflection, SouthernCrossReflectionRipple,
    WinterParticles, SpringParticles, SummerParticles, AutumnParticles,
    SeasonMicroOrnaments,
    BaseContextPanel, BaseContextCursor, BaseStorageWindow,
};

struct ProductionFrame {
    std::uint32_t pixelOffset;
    std::uint8_t width;
    std::uint8_t height;
    std::int8_t anchorX;
    std::int8_t anchorY;
    std::uint8_t palette;
};

const ProductionFrame& GetProductionFrame(ProductionSpriteId id);
std::uint8_t SampleProductionIndex(const ProductionFrame& frame, int x, int y, bool mirrorX);
std::uint32_t ProductionPaletteColor(const ProductionFrame& frame, std::uint8_t index);

}  // namespace lc
