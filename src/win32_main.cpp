#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "acrux_synth.h"
#include "acrux_waveout.h"
#include "atlas_data.h"
#include "cutscene_assets.h"
#include "game_core.h"
#include "production_assets.h"
#include "title_assets.h"

namespace {

constexpr int kScreenWidth = 960;
constexpr int kScreenHeight = 540;
constexpr int kTilePixels = 24;
constexpr int kTileCenter = kTilePixels / 2;
constexpr int kUiScale = 3;
constexpr float kPlayerSpeed = 108.0f;
constexpr float kPi = 3.1415926535f;
constexpr float kDayTransitionSeconds = 6.0f;
constexpr float kEndingTransitionSeconds = 18.0f;
constexpr float kBoarImpactFxSeconds = 0.45f;
constexpr float kResourceSignalTiles = 14.0f;
constexpr float kCrisisResourceSignalTiles = 10.0f;

constexpr int WorldPixels(int baselinePixels) {
    return baselinePixels * kTilePixels / 16;
}

std::uint32_t gPixels[kScreenWidth * kScreenHeight];
std::uint8_t gLight[kScreenWidth * kScreenHeight];
bool gRunning = true;
bool gFullscreen = false;
WINDOWPLACEMENT gWindowPlacement = {sizeof(WINDOWPLACEMENT)};

#ifdef _DEBUG
bool gDebugOverlay = false;
bool gDebugFullVisibility = false;
float gSmoothedFrameSeconds = 1.0f / 60.0f;
#endif

struct InputState {
    bool down[256];
    bool pressed[256];
};

InputState gInput = {};

enum class UiScreen : std::uint8_t {
    Title,
    Intro,
    HowToPlay,
    Gameplay,
    BaseMenu,
    Storage,
    Pause,
    ConfirmNewGame,
    ConfirmTitle,
    ConfirmQuit,
};

enum class PlayerActionAnimation : std::uint8_t {
    None,
    Blink,
    Take,
};

struct Runtime {
    lc::GameState game;
    float playerX;
    float playerY;
    int facingX;
    int facingY;
    lc::PlacementState placement;
    bool started;
    lc::Language language;
    UiScreen uiScreen;
    UiScreen returnScreen;
    int menuSelection;
    int storageSelection;
    bool storageTargetBase;
    bool hasValidSave;
    float dayTimer;
    float totalTime;
    float blinkOriginX;
    float blinkOriginY;
    float messageTimer;
    const wchar_t* message;
    int dayLoreIndex;
    wchar_t dayLoreText[128];
    float pathHum;
    int audioRelayCount;
    bool audioPaused;
    int firstNightHint;
    float firstNightHintDelay;
    float playerWalkTime;
    float playerActionTime;
    bool playerMoving;
    PlayerActionAnimation playerAction;
    float boarImpactTimer;
    float boarImpactX;
    float boarImpactY;
    bool boarImpactMirror;
#ifdef _DEBUG
    bool wasCharging;
    int manualChargeCount;
    float manualChargeTime;
    int storageOpenCount;
    int pathDeposited;
    int pathWithdrawn;
    int repairCount;
    int brokenPathRecoveredCount;
    int saveCheckpointCount;
    int continueLoadCount;
#endif
};

Runtime gRuntime = {};

std::uint32_t Color(unsigned red, unsigned green, unsigned blue) {
    return (red << 16u) | (green << 8u) | blue;
}

float Clamp(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

std::uint32_t ScaleColor(std::uint32_t color, float scale) {
    const unsigned red = static_cast<unsigned>(((color >> 16u) & 255u) * scale);
    const unsigned green = static_cast<unsigned>(((color >> 8u) & 255u) * scale);
    const unsigned blue = static_cast<unsigned>((color & 255u) * scale);
    return Color(red > 255u ? 255u : red, green > 255u ? 255u : green,
                 blue > 255u ? 255u : blue);
}

COLORREF ColorRef(std::uint32_t color) {
    return RGB((color >> 16u) & 255u, (color >> 8u) & 255u, color & 255u);
}

COLORREF FadeColor(std::uint32_t color, float alpha) {
    return ColorRef(ScaleColor(color, Clamp(alpha, 0.0f, 1.0f)));
}

void PutPixel(int x, int y, std::uint32_t color) {
    if (x >= 0 && y >= 0 && x < kScreenWidth && y < kScreenHeight) {
        gPixels[y * kScreenWidth + x] = color;
    }
}

void DrawSprite(lc::SpriteId id, int anchorScreenX, int anchorScreenY, bool mirrorX) {
    const lc::SpriteFrame& frame = lc::GetSpriteFrame(id);
    const int left = anchorScreenX - frame.anchorX;
    const int top = anchorScreenY - frame.anchorY;
    for (int sourceY = 0; sourceY < frame.height; ++sourceY) {
        for (int sourceX = 0; sourceX < frame.width; ++sourceX) {
            const std::uint8_t paletteIndex = lc::SampleSpriteIndex(frame, sourceX, sourceY, mirrorX);
            if (paletteIndex != 0u) {
                PutPixel(left + sourceX, top + sourceY, lc::AtlasPaletteColor(paletteIndex));
            }
        }
    }
}

void DrawProductionSprite(lc::ProductionSpriteId id, int anchorScreenX, int anchorScreenY,
                          bool mirrorX = false) {
    const lc::ProductionFrame& frame = lc::GetProductionFrame(id);
    const int left = anchorScreenX - frame.anchorX;
    const int top = anchorScreenY - frame.anchorY;
    for (int sourceY = 0; sourceY < frame.height; ++sourceY) {
        for (int sourceX = 0; sourceX < frame.width; ++sourceX) {
            const std::uint8_t paletteIndex = lc::SampleProductionIndex(frame, sourceX, sourceY, mirrorX);
            if (paletteIndex != 0u) {
                PutPixel(left + sourceX, top + sourceY,
                         lc::ProductionPaletteColor(frame, paletteIndex));
            }
        }
    }
}

void BlendPixel(int x, int y, std::uint32_t color, int alpha);

void DrawProductionSpriteRegion(lc::ProductionSpriteId id, int frameLeft, int frameTop,
                                int frameWidth, int frameHeight,
                                int anchorX, int anchorY,
                                int anchorScreenX, int anchorScreenY,
                                bool mirrorX = false) {
    const lc::ProductionFrame& frame = lc::GetProductionFrame(id);
    const int left = anchorScreenX - anchorX;
    const int top = anchorScreenY - anchorY;
    for (int sourceY = 0; sourceY < frameHeight; ++sourceY) {
        for (int sourceX = 0; sourceX < frameWidth; ++sourceX) {
            const int localX = mirrorX ? frameWidth - 1 - sourceX : sourceX;
            const int sampleX = frameLeft + localX;
            const int sampleY = frameTop + sourceY;
            const std::uint8_t paletteIndex = lc::SampleProductionIndex(
                frame, sampleX, sampleY, false);
            if (paletteIndex != 0u) {
                PutPixel(left + sourceX, top + sourceY,
                         lc::ProductionPaletteColor(frame, paletteIndex));
            }
        }
    }
}

void BlendProductionSpriteRegion(lc::ProductionSpriteId id, int frameLeft, int frameTop,
                                 int frameWidth, int frameHeight,
                                 int anchorX, int anchorY,
                                 int anchorScreenX, int anchorScreenY,
                                 int alpha, float brightness = 1.0f) {
    const lc::ProductionFrame& frame = lc::GetProductionFrame(id);
    const int left = anchorScreenX - anchorX;
    const int top = anchorScreenY - anchorY;
    for (int sourceY = 0; sourceY < frameHeight; ++sourceY) {
        for (int sourceX = 0; sourceX < frameWidth; ++sourceX) {
            const std::uint8_t paletteIndex = lc::SampleProductionIndex(
                frame, frameLeft + sourceX, frameTop + sourceY, false);
            if (paletteIndex != 0u) {
                const std::uint32_t color = ScaleColor(
                    lc::ProductionPaletteColor(frame, paletteIndex), brightness);
                BlendPixel(left + sourceX, top + sourceY, color, alpha);
            }
        }
    }
}

void BlendProductionSpriteRegionScaled(lc::ProductionSpriteId id, int frameLeft, int frameTop,
                                       int frameWidth, int frameHeight,
                                       int anchorX, int anchorY,
                                       int anchorScreenX, int anchorScreenY,
                                       int scale, int alpha, float brightness = 1.0f,
                                       bool mirrorX = false) {
    const lc::ProductionFrame& frame = lc::GetProductionFrame(id);
    const int left = anchorScreenX - anchorX * scale;
    const int top = anchorScreenY - anchorY * scale;
    for (int sourceY = 0; sourceY < frameHeight; ++sourceY) {
        for (int sourceX = 0; sourceX < frameWidth; ++sourceX) {
            const int localX = mirrorX ? frameWidth - 1 - sourceX : sourceX;
            const std::uint8_t paletteIndex = lc::SampleProductionIndex(
                frame, frameLeft + localX, frameTop + sourceY, false);
            if (paletteIndex == 0u) continue;
            const std::uint32_t color = ScaleColor(
                lc::ProductionPaletteColor(frame, paletteIndex), brightness);
            const int dstX = left + sourceX * scale;
            const int dstY = top + sourceY * scale;
            for (int y = 0; y < scale; ++y) {
                for (int x = 0; x < scale; ++x) {
                    BlendPixel(dstX + x, dstY + y, color, alpha);
                }
            }
        }
    }
}

void DrawProductionUiSprite(lc::ProductionSpriteId id, int left, int top) {
    const lc::ProductionFrame& frame = lc::GetProductionFrame(id);
    for (int sourceY = 0; sourceY < frame.height; ++sourceY) {
        for (int sourceX = 0; sourceX < frame.width; ++sourceX) {
            const std::uint8_t paletteIndex = lc::SampleProductionIndex(
                frame, sourceX, sourceY, false);
            if (paletteIndex != 0u) {
                PutPixel(left + sourceX, top + sourceY,
                         lc::ProductionPaletteColor(frame, paletteIndex));
            }
        }
    }
}

void DrawTitleBackground() {
    const std::uint8_t* pixels = lc::TitleBackgroundPixels();
    for (int sourceY = 0; sourceY < lc::kTitleBackgroundHeight; ++sourceY) {
        for (int sourceX = 0; sourceX < lc::kTitleBackgroundWidth; ++sourceX) {
            const std::uint32_t color = lc::TitleBackgroundPaletteColor(
                pixels[sourceY * lc::kTitleBackgroundWidth + sourceX]);
            const int x = sourceX * 3;
            const int y = sourceY * 3;
            for (int dy = 0; dy < 3; ++dy) {
                for (int dx = 0; dx < 3; ++dx) {
                    gPixels[(y + dy) * kScreenWidth + x + dx] = color;
                }
            }
        }
    }
    DrawProductionSprite(lc::ProductionSpriteId::TitleLogo, (kScreenWidth - 192) / 2, 18 * 3);
}

void BlendPixel(int x, int y, std::uint32_t color, int alpha) {
    if (x < 0 || y < 0 || x >= kScreenWidth || y >= kScreenHeight || alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    const std::uint32_t old = gPixels[y * kScreenWidth + x];
    const unsigned oldR = (old >> 16u) & 255u;
    const unsigned oldG = (old >> 8u) & 255u;
    const unsigned oldB = old & 255u;
    const unsigned newR = (color >> 16u) & 255u;
    const unsigned newG = (color >> 8u) & 255u;
    const unsigned newB = color & 255u;
    const unsigned inverse = static_cast<unsigned>(255 - alpha);
    const unsigned red = (oldR * inverse + newR * static_cast<unsigned>(alpha)) / 255u;
    const unsigned green = (oldG * inverse + newG * static_cast<unsigned>(alpha)) / 255u;
    const unsigned blue = (oldB * inverse + newB * static_cast<unsigned>(alpha)) / 255u;
    gPixels[y * kScreenWidth + x] = Color(red, green, blue);
}

void FillRect(int x, int y, int width, int height, std::uint32_t color) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) PutPixel(px, py, color);
    }
}

void BlendCircle(int centerX, int centerY, int radius, std::uint32_t color, int alpha) {
    const int radiusSquared = radius * radius;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const int distanceSquared = x * x + y * y;
            if (distanceSquared <= radiusSquared) {
                const int localAlpha = alpha * (radiusSquared - distanceSquared) / (radiusSquared + 1);
                BlendPixel(centerX + x, centerY + y, color, localAlpha);
            }
        }
    }
}

void AddLight(int centerX, int centerY, int radius, int strength) {
    if (radius <= 0 || strength <= 0) return;
    const int radiusSquared = radius * radius;
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            const int px = centerX + x;
            const int py = centerY + y;
            const int distanceSquared = x * x + y * y;
            if (px < 0 || py < 0 || px >= kScreenWidth || py >= kScreenHeight ||
                distanceSquared > radiusSquared) continue;
            const float distance = std::sqrt(static_cast<float>(distanceSquared));
            const int amount = static_cast<int>(strength * (1.0f - distance / radius));
            std::uint8_t& light = gLight[py * kScreenWidth + px];
            if (amount > light) light = static_cast<std::uint8_t>(amount > 255 ? 255 : amount);
        }
    }
}

void RaiseLightRect(int x, int y, int width, int height, std::uint8_t minimum) {
    const int left = x < 0 ? 0 : x;
    const int top = y < 0 ? 0 : y;
    const int right = x + width > kScreenWidth ? kScreenWidth : x + width;
    const int bottom = y + height > kScreenHeight ? kScreenHeight : y + height;
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            std::uint8_t& light = gLight[py * kScreenWidth + px];
            if (light < minimum) light = minimum;
        }
    }
}

void ApplyLightMask() {
    for (int index = 0; index < kScreenWidth * kScreenHeight; ++index) {
        const unsigned light = gLight[index];
        const std::uint32_t old = gPixels[index];
        const unsigned red = ((old >> 16u) & 255u) * light / 255u;
        const unsigned green = ((old >> 8u) & 255u) * light / 255u;
        const unsigned blue = (old & 255u) * light / 255u;
        gPixels[index] = Color(red, green, blue);
    }
}

void DrawCutsceneImage(lc::CutsceneId id, float brightness) {
    const std::uint8_t* pixels = lc::CutscenePixels(id);
    for (int sourceY = 0; sourceY < lc::kCutsceneHeight; ++sourceY) {
        for (int sourceX = 0; sourceX < lc::kCutsceneWidth; ++sourceX) {
            const std::uint32_t color = ScaleColor(
                lc::CutscenePaletteColor(id, pixels[sourceY * lc::kCutsceneWidth + sourceX]),
                brightness);
            const int x = sourceX * 2;
            const int y = sourceY * 2;
            gPixels[y * kScreenWidth + x] = color;
            gPixels[y * kScreenWidth + x + 1] = color;
            gPixels[(y + 1) * kScreenWidth + x] = color;
            gPixels[(y + 1) * kScreenWidth + x + 1] = color;
        }
    }
}

void DrawLine(int x0, int y0, int x1, int y1, std::uint32_t color) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        PutPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void DrawStar(int x, int y, int radius, std::uint32_t color) {
    DrawLine(x - radius, y, x + radius, y, color);
    DrawLine(x, y - radius, x, y + radius, color);
    if (radius >= 3) {
        DrawLine(x - radius / 2, y - radius / 2, x + radius / 2, y + radius / 2, color);
        DrawLine(x + radius / 2, y - radius / 2, x - radius / 2, y + radius / 2, color);
    }
}

void DrawCircleOutline(int centerX, int centerY, int radius, std::uint32_t color) {
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        PutPixel(centerX + x, centerY + y, color);
        PutPixel(centerX + y, centerY + x, color);
        PutPixel(centerX - y, centerY + x, color);
        PutPixel(centerX - x, centerY + y, color);
        PutPixel(centerX - x, centerY - y, color);
        PutPixel(centerX - y, centerY - x, color);
        PutPixel(centerX + y, centerY - x, color);
        PutPixel(centerX + x, centerY - y, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

int TileIndex(const lc::GameState& game, int x, int y) {
    return y * game.width + x;
}

bool IsInsideWorld(const lc::GameState& game, float x, float y) {
    constexpr float margin = static_cast<float>(WorldPixels(4));
    return x >= margin && y >= margin && x < game.width * kTilePixels - margin &&
           y < game.height * kTilePixels - margin;
}

bool IsBlockedAt(const lc::GameState& game, float x, float y) {
    if (!IsInsideWorld(game, x, y)) return true;
    const int tileX = static_cast<int>(x) / kTilePixels;
    const int tileY = static_cast<int>(y) / kTilePixels;
    if (lc::IsBaseFootprintTile(game, tileX, tileY)) return true;
    if (lc::IsBasePuddleTile(game, tileX, tileY)) return true;
    if (lc::IsRelayFootprintTile(game, tileX, tileY)) return true;
    const lc::Terrain terrain = game.terrain[TileIndex(game, tileX, tileY)];
    return terrain == lc::Terrain::Rock || terrain == lc::Terrain::Bush;
}

bool NearTile(float x, float y, int tileX, int tileY, float radius) {
    const float targetX = (static_cast<float>(tileX) + 0.5f) * kTilePixels;
    const float targetY = (static_cast<float>(tileY) + 0.5f) * kTilePixels;
    const float dx = x - targetX;
    const float dy = y - targetY;
    return dx * dx + dy * dy <= radius * radius;
}

bool PlayerOnPoweredPath(const Runtime& runtime) {
    const int x = static_cast<int>(runtime.playerX) / kTilePixels;
    const int y = static_cast<int>(runtime.playerY) / kTilePixels;
    return lc::IsChargeNode(runtime.game, x, y);
}

float WorldPan(const Runtime& runtime, float targetTileX) {
    return Clamp((targetTileX - runtime.playerX / kTilePixels) / 8.0f, -1.0f, 1.0f);
}

float NearestAnimalPan(const Runtime& runtime, lc::AnimalKind kind) {
    float bestDistanceSquared = 1000000.0f;
    float pan = 0.0f;
    const float playerTileX = runtime.playerX / kTilePixels;
    const float playerTileY = runtime.playerY / kTilePixels;
    for (int index = 0; index < runtime.game.animalCount; ++index) {
        const lc::Animal& animal = runtime.game.animals[index];
        if (!animal.active || animal.kind != kind) continue;
        const float dx = animal.x - playerTileX;
        const float dy = animal.y - playerTileY;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            pan = WorldPan(runtime, animal.x);
        }
    }
    return pan;
}

AcruxSeason ToAudioSeason(lc::Season season) {
    switch (season) {
        case lc::Season::Spring: return ACRUX_SEASON_SPRING;
        case lc::Season::Summer: return ACRUX_SEASON_SUMMER;
        case lc::Season::Autumn: return ACRUX_SEASON_AUTUMN;
        case lc::Season::Winter:
        default:
            return ACRUX_SEASON_WINTER;
    }
}

void DrawBasePuddle(const Runtime& runtime, float cameraX, float cameraY) {
    const lc::TileRect rect = lc::GetBasePuddleRect(runtime.game);
    if (rect.width <= 0 || rect.height <= 0) return;
    const int centerX = rect.x * kTilePixels + rect.width * kTilePixels / 2 -
        static_cast<int>(cameraX);
    const int centerY = rect.y * kTilePixels + rect.height * kTilePixels / 2 -
        static_cast<int>(cameraY);
    DrawProductionSprite(lc::ProductionSpriteId::PuddleNormal, centerX, centerY);
    const int rippleFrame = static_cast<int>(runtime.totalTime / 0.28f) % 3;
    BlendProductionSpriteRegion(lc::ProductionSpriteId::PuddleRippleVariants,
                                rippleFrame * 40, 0, 40, 40, 20, 20,
                                centerX, centerY, 92, 1.0f);
    if (!runtime.game.completed) return;
    BlendCircle(centerX, centerY, 25, Color(80, 197, 255), 28);
    DrawProductionSprite(lc::ProductionSpriteId::SouthernCrossReflection, centerX, centerY);
    BlendProductionSpriteRegion(lc::ProductionSpriteId::SouthernCrossReflectionRipple,
                                rippleFrame * 40, 0, 40, 40, 20, 20,
                                centerX, centerY, 126, 1.0f);
    int ornamentFrame = 0;
    switch (lc::CurrentSeason(runtime.game)) {
        case lc::Season::Spring: ornamentFrame = 1; break;
        case lc::Season::Summer: ornamentFrame = 2; break;
        case lc::Season::Autumn: ornamentFrame = 3; break;
        case lc::Season::Winter:
        default:
            ornamentFrame = 0;
            break;
    }
    BlendProductionSpriteRegion(lc::ProductionSpriteId::SeasonMicroOrnaments,
                                ornamentFrame * 8, 0, 8, 8, 4, 4,
                                centerX + 15, centerY - 13, 88, 1.0f);
}

void DrawEpilogueSeasonParticles(const Runtime& runtime) {
    if (!runtime.game.completed) return;
    lc::ProductionSpriteId sheet = lc::ProductionSpriteId::WinterParticles;
    int alpha = 72;
    float speed = 11.0f;
    float brightness = 0.82f;
    switch (lc::CurrentSeason(runtime.game)) {
        case lc::Season::Winter:
            sheet = lc::ProductionSpriteId::WinterParticles;
            alpha = 122;
            speed = 9.0f;
            brightness = 1.12f;
            break;
        case lc::Season::Spring:
            sheet = lc::ProductionSpriteId::SpringParticles;
            alpha = 128;
            speed = 13.0f;
            brightness = 1.16f;
            break;
        case lc::Season::Summer:
            sheet = lc::ProductionSpriteId::SummerParticles;
            alpha = 136;
            speed = 5.0f;
            brightness = 1.08f;
            break;
        case lc::Season::Autumn:
            sheet = lc::ProductionSpriteId::AutumnParticles;
            alpha = 126;
            speed = 15.0f;
            brightness = 1.12f;
            break;
        default:
            return;
    }
    for (int index = 0; index < 48; ++index) {
        const unsigned hash = static_cast<unsigned>(index * 747796405u) ^
            static_cast<unsigned>(runtime.game.day * 2891336453u);
        if (((hash >> 9) & 3u) == 0u) continue;
        const int seedX = static_cast<int>((hash ^ (hash >> 11)) % kScreenWidth);
        const int seedY = static_cast<int>(((hash >> 5) ^ (hash >> 19)) % kScreenHeight);
        const float localSpeed = speed * (0.62f + static_cast<float>((hash >> 16) & 31u) / 52.0f);
        const float phase = static_cast<float>((hash >> 2) & 1023u) * 0.0061359f;
        const int frame = (static_cast<int>(runtime.totalTime * (2.3f + (hash & 3u) * 0.55f) +
                                            static_cast<float>(index)) +
                           static_cast<int>((hash >> 20) & 7u)) & 7;
        int x = seedX + static_cast<int>(std::sin(runtime.totalTime * 0.45f + phase) *
                                         (5.0f + static_cast<float>((hash >> 24) & 15u)));
        int y = seedY + static_cast<int>(runtime.totalTime * localSpeed +
                                         static_cast<float>((hash >> 12) & 63u));
        x %= kScreenWidth;
        y %= kScreenHeight;
        if (x < 0) x += kScreenWidth;
        if (y < 0) y += kScreenHeight;
        int particleAlpha = alpha;
        float particleBrightness = brightness;
        if (sheet == lc::ProductionSpriteId::SummerParticles) {
            const float pulse = 0.5f + 0.5f * std::sin(runtime.totalTime * 3.4f + index * 1.7f);
            particleAlpha = 82 + static_cast<int>(pulse * 126.0f);
            particleBrightness = 0.82f + pulse * 0.98f;
        }
        const int centerDx = x - kScreenWidth / 2;
        const int centerDy = y - kScreenHeight / 2;
        if (centerDx * centerDx + centerDy * centerDy < 28 * 28) continue;
        const int cropX = static_cast<int>((hash >> 27) & 3u);
        const int cropY = static_cast<int>((hash >> 29) & 3u);
        const int cropW = 9 + static_cast<int>((hash >> 7) & 3u);
        const int cropH = 9 + static_cast<int>((hash >> 14) & 3u);
        BlendProductionSpriteRegion(sheet, frame * 12 + cropX, cropY, cropW, cropH,
                                    cropW / 2, cropH / 2,
                                    x, y, particleAlpha, particleBrightness);
    }
}

bool AudioShouldPause(const Runtime& runtime) {
    if (runtime.uiScreen == UiScreen::Pause) return true;
    if (runtime.uiScreen == UiScreen::BaseMenu || runtime.uiScreen == UiScreen::Storage) return true;
    if (runtime.uiScreen == UiScreen::HowToPlay && runtime.returnScreen == UiScreen::Pause) return true;
    return (runtime.uiScreen == UiScreen::ConfirmNewGame ||
            runtime.uiScreen == UiScreen::ConfirmTitle || runtime.uiScreen == UiScreen::ConfirmQuit) &&
           runtime.returnScreen == UiScreen::Pause;
}

void UpdateAudioFrame(Runtime& runtime, float seconds) {
    const bool shouldPause = AudioShouldPause(runtime);
    if (shouldPause != runtime.audioPaused) {
        AcruxAudio_SetPaused(shouldPause);
        runtime.audioPaused = shouldPause;
    }
    if (shouldPause) return;

    float targetHum = 0.0f;
    if (runtime.started && runtime.game.phase == lc::GamePhase::Night) {
        if (PlayerOnPoweredPath(runtime)) {
            targetHum = 1.0f;
        } else {
            const float playerTileX = runtime.playerX / kTilePixels;
            const float playerTileY = runtime.playerY / kTilePixels;
            float nearestSquared = 1000000.0f;
            for (int y = 0; y < runtime.game.height; ++y) {
                for (int x = 0; x < runtime.game.width; ++x) {
                    if (!runtime.game.powered[TileIndex(runtime.game, x, y)]) continue;
                    const float dx = static_cast<float>(x) + 0.5f - playerTileX;
                    const float dy = static_cast<float>(y) + 0.5f - playerTileY;
                    const float distanceSquared = dx * dx + dy * dy;
                    if (distanceSquared < nearestSquared) nearestSquared = distanceSquared;
                }
            }
            if (nearestSquared < 1.0f) targetHum = 0.65f;
            else if (nearestSquared < 4.0f) targetHum = 0.28f;
        }
    }
    runtime.pathHum += (targetHum - runtime.pathHum) * Clamp(seconds * 8.0f, 0.0f, 1.0f);
    AcruxSynth_SetPathHum(runtime.pathHum);

    const int relayCount = runtime.started ? runtime.game.relaysPowered : 0;
    if (runtime.started && runtime.game.completed) {
        AcruxSynth_SetEpilogueSeason(ToAudioSeason(lc::CurrentSeason(runtime.game)));
    } else {
        if (AcruxSynth_IsEpilogue()) AcruxSynth_ExitEpilogue();
        if (relayCount > runtime.audioRelayCount) {
            AcruxSynth_PlaySfx(ACRUX_SFX_RELAY);
        }
        AcruxSynth_SetRelayCount(relayCount);
    }
    runtime.audioRelayCount = relayCount;
}

void ShowMessage(Runtime& runtime, const wchar_t* message, float duration = 1.4f) {
    runtime.message = message;
    runtime.messageTimer = duration;
}

bool IsKorean(const Runtime& runtime) {
    return runtime.language == lc::Language::Korean;
}

const wchar_t* LocalText(const Runtime& runtime, const wchar_t* english,
                         const wchar_t* korean) {
    return IsKorean(runtime) ? korean : english;
}

void ClearDayLore(Runtime& runtime) {
    runtime.dayLoreIndex = -1;
    runtime.dayLoreText[0] = 0;
}

void SetDayLore(Runtime& runtime, int loreIndex) {
    runtime.dayLoreIndex = loreIndex;
    const char* text = lc::LoreText(loreIndex, runtime.language);
    const int converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                                               runtime.dayLoreText,
                                               static_cast<int>(sizeof(runtime.dayLoreText) /
                                                                sizeof(runtime.dayLoreText[0])));
    if (converted <= 0) runtime.dayLoreText[0] = 0;
}

constexpr wchar_t kSavePath[] = L"save.dat";
constexpr wchar_t kSaveTempPath[] = L"save.tmp";

bool ReadSaveFile(lc::SaveData& save) {
    HANDLE file = CreateFileW(kSavePath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(file, &save, sizeof(save), &bytesRead, nullptr);
    DWORD extra = 0;
    unsigned char ignored = 0;
    const BOOL hasExtra = ReadFile(file, &ignored, 1, &extra, nullptr);
    CloseHandle(file);
    return ok && bytesRead == sizeof(save) && hasExtra && extra == 0;
}

bool HasValidSaveFile() {
    lc::SaveData save = {};
    lc::GameState restored = {};
    return ReadSaveFile(save) && lc::RestoreSaveData(restored, save);
}

bool WriteSaveFile(const lc::GameState& game) {
    lc::SaveData save = {};
    if (!lc::CreateSaveData(game, save)) return false;
    HANDLE file = CreateFileW(kSaveTempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD bytesWritten = 0;
    const BOOL ok = WriteFile(file, &save, sizeof(save), &bytesWritten, nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok || bytesWritten != sizeof(save)) {
        DeleteFileW(kSaveTempPath);
        return false;
    }
    return MoveFileExW(kSaveTempPath, kSavePath,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

void ToggleFullscreen(HWND window) {
    const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
    if (!gFullscreen) {
        MONITORINFO monitor = {sizeof(MONITORINFO)};
        if (!GetWindowPlacement(window, &gWindowPlacement) ||
            !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
            return;
        }
        SetWindowLongPtrW(window, GWL_STYLE, style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW));
        SetWindowPos(window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        gFullscreen = true;
    } else {
        SetWindowLongPtrW(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &gWindowPlacement);
        SetWindowPos(window, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                         SWP_NOOWNERZORDER | SWP_NOZORDER);
        gFullscreen = false;
    }
}

void PlacePlayerAtBaseExit(Runtime& runtime);

void ResetGame(Runtime& runtime, bool started) {
    const lc::Language language = runtime.language;
    AcruxAudio_SetPaused(false);
    AcruxSynth_ResetPlayback();
    runtime = {};
    runtime.language = language;
    runtime.hasValidSave = HasValidSaveFile();
    lc::InitializeGame(runtime.game);
    runtime.started = started;
    runtime.uiScreen = started ? UiScreen::Gameplay : UiScreen::Title;
    runtime.returnScreen = UiScreen::Title;
    runtime.message = L"FOLLOW THE GROUND-STARS";
    runtime.messageTimer = 4.0f;
    ClearDayLore(runtime);
    runtime.firstNightHint = 0;
    runtime.firstNightHintDelay = 4.25f;
    PlacePlayerAtBaseExit(runtime);
    if (started) lc::BeginNight(runtime.game);
}

void PlacePlayerAtBaseExit(Runtime& runtime) {
    const lc::TileRect rect = lc::GetBaseFootprintRect(runtime.game);
    runtime.playerX = (static_cast<float>(rect.x) + static_cast<float>(rect.width) * 0.5f) *
        kTilePixels;
    runtime.playerY = (static_cast<float>(rect.y + rect.height) + 0.2f) * kTilePixels;
    runtime.facingX = 0;
    runtime.facingY = -1;
}

#if defined(_DEBUG) && defined(ACRUX_ENDING_PREVIEW_BUILD)
void DebugPrepareEndingPreview(Runtime& runtime);
#endif

void BeginNewGame(Runtime& runtime) {
#if defined(_DEBUG) && defined(ACRUX_ENDING_PREVIEW_BUILD)
    DebugPrepareEndingPreview(runtime);
#else
    ResetGame(runtime, true);
#endif
}

bool ContinueFromSave(Runtime& runtime) {
    lc::SaveData save = {};
    lc::GameState restored = {};
    if (!ReadSaveFile(save) || !lc::RestoreSaveData(restored, save)) {
        runtime.hasValidSave = false;
        return false;
    }
    AcruxAudio_SetPaused(false);
    AcruxSynth_ResetPlayback();
    runtime = {};
    runtime.hasValidSave = true;
    runtime.game = restored;
    PlacePlayerAtBaseExit(runtime);
    runtime.started = true;
    runtime.uiScreen = UiScreen::Gameplay;
    runtime.returnScreen = UiScreen::Title;
    runtime.message = L"CHECKPOINT RESTORED";
    runtime.messageTimer = 2.4f;
    ClearDayLore(runtime);
    runtime.firstNightHint = 3;
    if (runtime.game.completed) {
        AcruxSynth_EnterEpilogueImmediate(ToAudioSeason(lc::CurrentSeason(runtime.game)));
    }
    lc::BeginNight(runtime.game);
#ifdef _DEBUG
    ++runtime.continueLoadCount;
#endif
    return true;
}

void StepFirstNightHints(Runtime& runtime, float seconds) {
    if (!runtime.started || runtime.game.day != 1 ||
        runtime.game.phase != lc::GamePhase::Night || runtime.firstNightHint >= 3) {
        return;
    }
    runtime.firstNightHintDelay -= seconds;
    if (runtime.firstNightHintDelay > 0.0f || runtime.messageTimer > 0.0f) return;

    static const wchar_t* const hintsEn[] = {
        L"BASE, RELAYS, AND POWERED PATHS CAN CHARGE",
        L"X BLINK REVEALS THE DARK, BUT SPENDS 25% LIGHT",
        L"RETURN HOME AND Z SLEEP BEFORE YOUR LIGHT IS LOST",
    };
    static const wchar_t* const hintsKo[] = {
        L"기지, 릴레이, 켜진 PATH에서 충전할 수 있다",
        L"X 점멸은 어둠을 드러내지만 빛을 25% 쓴다",
        L"빛을 잃기 전에 집으로 돌아와 Z로 잠들어라",
    };
    const wchar_t* const* hints = IsKorean(runtime) ? hintsKo : hintsEn;
    ShowMessage(runtime, hints[runtime.firstNightHint], 3.2f);
    ++runtime.firstNightHint;
    runtime.firstNightHintDelay = 0.8f;
}

void MoveMenuSelection(Runtime& runtime, int itemCount) {
    if (gInput.pressed[VK_UP]) {
        runtime.menuSelection = (runtime.menuSelection + itemCount - 1) % itemCount;
    }
    if (gInput.pressed[VK_DOWN]) {
        runtime.menuSelection = (runtime.menuSelection + 1) % itemCount;
    }
}

void OpenHowToPlay(Runtime& runtime, UiScreen returnScreen) {
    runtime.returnScreen = returnScreen;
    runtime.uiScreen = UiScreen::HowToPlay;
    runtime.menuSelection = 0;
}

void OpenConfirmation(Runtime& runtime, UiScreen confirmation, UiScreen returnScreen) {
    runtime.returnScreen = returnScreen;
    runtime.uiScreen = confirmation;
    runtime.menuSelection = 0;
}

bool HandleFrontendInput(Runtime& runtime) {
    if (runtime.uiScreen == UiScreen::Gameplay &&
        runtime.game.phase != lc::GamePhase::GameOver &&
        runtime.game.phase != lc::GamePhase::Ending) {
        if (gInput.pressed[VK_ESCAPE]) {
            lc::CancelPathPlacement(runtime.placement);
            runtime.uiScreen = UiScreen::Pause;
            runtime.menuSelection = 0;
            return true;
        }
        return false;
    }

    if (runtime.uiScreen == UiScreen::Gameplay &&
        runtime.game.phase == lc::GamePhase::GameOver) {
        MoveMenuSelection(runtime, 3);
        if (gInput.pressed['Z']) {
            if (runtime.menuSelection == 0) {
                ResetGame(runtime, true);
            } else if (runtime.menuSelection == 1) {
                ResetGame(runtime, false);
            } else {
                OpenConfirmation(runtime, UiScreen::ConfirmQuit, UiScreen::Gameplay);
            }
        }
        return true;
    }

    if (runtime.uiScreen == UiScreen::Gameplay &&
        runtime.game.phase == lc::GamePhase::Ending) {
        return false;
    }

    if (runtime.uiScreen == UiScreen::Title) {
        const int titleItems = runtime.hasValidSave ? 5 : 4;
        const int continueIndex = runtime.hasValidSave ? 1 : -1;
        const int howToIndex = runtime.hasValidSave ? 2 : 1;
        const int languageIndex = runtime.hasValidSave ? 3 : 2;
        const int quitIndex = runtime.hasValidSave ? 4 : 3;
        MoveMenuSelection(runtime, titleItems);
        if (gInput.pressed['Z']) {
            if (runtime.menuSelection == 0) {
                if (runtime.hasValidSave) {
                    OpenConfirmation(runtime, UiScreen::ConfirmNewGame, UiScreen::Title);
                } else {
                    runtime.uiScreen = UiScreen::Intro;
                    runtime.menuSelection = 0;
                }
            } else if (runtime.menuSelection == continueIndex) {
                if (!ContinueFromSave(runtime)) {
                    ShowMessage(runtime, LocalText(runtime, L"SAVE UNAVAILABLE",
                                                  L"저장 데이터를 열 수 없습니다"));
                }
            } else if (runtime.menuSelection == howToIndex) {
                OpenHowToPlay(runtime, UiScreen::Title);
            } else if (runtime.menuSelection == languageIndex) {
                runtime.language = lc::NextLanguage(runtime.language);
                ShowMessage(runtime, LocalText(runtime, L"LANGUAGE: ENGLISH",
                                              L"언어: 한국어"), 1.6f);
            } else if (runtime.menuSelection == quitIndex) {
                OpenConfirmation(runtime, UiScreen::ConfirmQuit, UiScreen::Title);
            }
        } else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) {
            OpenConfirmation(runtime, UiScreen::ConfirmQuit, UiScreen::Title);
        }
        return true;
    }

    if (runtime.uiScreen == UiScreen::BaseMenu) {
        if (gInput.pressed[VK_UP] || gInput.pressed[VK_DOWN] ||
            gInput.pressed[VK_LEFT] || gInput.pressed[VK_RIGHT]) {
            runtime.menuSelection = 1 - runtime.menuSelection;
        }
        if (gInput.pressed['Z']) {
            if (runtime.menuSelection == 0) {
                AcruxSynth_PlaySfx(ACRUX_SFX_SLEEP);
                lc::SleepAtBase(runtime.game);
                if (runtime.game.phase == lc::GamePhase::Day) {
                    const int loreIndex = lc::ChooseNextLore(runtime.game);
                    lc::MarkLoreSeen(runtime.game, loreIndex);
                    SetDayLore(runtime, loreIndex);
                    runtime.hasValidSave = WriteSaveFile(runtime.game);
#ifdef _DEBUG
                    if (runtime.hasValidSave) ++runtime.saveCheckpointCount;
#endif
                    AcruxSynth_PlaySfx(ACRUX_SFX_DAY_TRANSITION);
                } else if (runtime.game.phase == lc::GamePhase::Ending) {
                    ClearDayLore(runtime);
                    AcruxSynth_PlaySfx(ACRUX_SFX_DAY_TRANSITION);
                } else if (runtime.game.phase == lc::GamePhase::GameOver) {
                    ClearDayLore(runtime);
                    AcruxSynth_PlaySfx(ACRUX_SFX_FAILURE);
                }
                runtime.uiScreen = UiScreen::Gameplay;
                runtime.dayTimer = runtime.game.phase == lc::GamePhase::Ending
                    ? kEndingTransitionSeconds : kDayTransitionSeconds;
            } else {
                runtime.uiScreen = UiScreen::Storage;
                runtime.storageSelection = 0;
                runtime.storageTargetBase = true;
#ifdef _DEBUG
                ++runtime.storageOpenCount;
#endif
            }
        } else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) {
            runtime.uiScreen = UiScreen::Gameplay;
            runtime.menuSelection = 0;
        }
        return true;
    }

    if (runtime.uiScreen == UiScreen::Storage) {
        if (gInput.pressed[VK_UP]) {
            runtime.storageSelection = (runtime.storageSelection + 2) % 3;
        }
        if (gInput.pressed[VK_DOWN]) {
            runtime.storageSelection = (runtime.storageSelection + 1) % 3;
        }
        if (gInput.pressed[VK_LEFT]) runtime.storageTargetBase = false;
        if (gInput.pressed[VK_RIGHT]) runtime.storageTargetBase = true;
        if (gInput.pressed['Z']) {
            const lc::ResourceKind kind = static_cast<lc::ResourceKind>(runtime.storageSelection);
            const bool moved = runtime.storageTargetBase
                ? lc::MoveOneResourceToBase(runtime.game, kind)
                : lc::MoveOneResourceToCarry(runtime.game, kind);
#ifdef _DEBUG
            if (moved && kind == lc::ResourceKind::Material && runtime.storageTargetBase) {
                ++runtime.pathDeposited;
            } else if (moved && kind == lc::ResourceKind::Material) {
                ++runtime.pathWithdrawn;
            }
#endif
            ShowMessage(runtime,
                        moved ? LocalText(runtime, L"STORAGE UPDATED", L"창고가 갱신되었습니다")
                              : LocalText(runtime, L"NOTHING TO MOVE", L"옮길 자원이 없습니다"));
        } else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) {
            runtime.uiScreen = UiScreen::Gameplay;
            runtime.menuSelection = 0;
        }
        return true;
    }

    if (runtime.uiScreen == UiScreen::Intro) {
        if (gInput.pressed['Z']) BeginNewGame(runtime);
        else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) ResetGame(runtime, false);
        return true;
    }

    if (runtime.uiScreen == UiScreen::HowToPlay) {
        if (gInput.pressed['Z'] || gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) {
            runtime.uiScreen = runtime.returnScreen;
            runtime.menuSelection = 0;
        }
        return true;
    }

    if (runtime.uiScreen == UiScreen::Pause) {
        if (gInput.pressed[VK_ESCAPE]) {
            runtime.uiScreen = UiScreen::Gameplay;
            runtime.menuSelection = 0;
            return true;
        }
        MoveMenuSelection(runtime, 4);
        if (gInput.pressed['Z']) {
            if (runtime.menuSelection == 0) {
                runtime.uiScreen = UiScreen::Gameplay;
            } else if (runtime.menuSelection == 1) {
                OpenHowToPlay(runtime, UiScreen::Pause);
            } else if (runtime.menuSelection == 2) {
                OpenConfirmation(runtime, UiScreen::ConfirmTitle, UiScreen::Pause);
            } else {
                OpenConfirmation(runtime, UiScreen::ConfirmQuit, UiScreen::Pause);
            }
            runtime.menuSelection = 0;
        } else if (gInput.pressed['X']) {
            runtime.uiScreen = UiScreen::Gameplay;
            runtime.menuSelection = 0;
        }
        return true;
    }

    if (runtime.uiScreen == UiScreen::ConfirmTitle) {
        if (gInput.pressed['Z']) ResetGame(runtime, false);
        else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) runtime.uiScreen = runtime.returnScreen;
        return true;
    }

    if (runtime.uiScreen == UiScreen::ConfirmNewGame) {
        if (gInput.pressed['Z']) BeginNewGame(runtime);
        else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) runtime.uiScreen = runtime.returnScreen;
        return true;
    }

    if (runtime.uiScreen == UiScreen::ConfirmQuit) {
        if (gInput.pressed['Z']) gRunning = false;
        else if (gInput.pressed['X'] || gInput.pressed[VK_ESCAPE]) runtime.uiScreen = runtime.returnScreen;
        return true;
    }

    return true;
}

void HandleActions(Runtime& runtime) {
    lc::GameState& game = runtime.game;
    if (game.phase != lc::GamePhase::Night) return;

    if (gInput.pressed['C']) {
        if (!runtime.placement.active) {
            const int playerTileX = static_cast<int>(runtime.playerX) / kTilePixels;
            const int playerTileY = static_cast<int>(runtime.playerY) / kTilePixels;
            if (lc::BeginPathPlacement(game, runtime.placement, playerTileX, playerTileY,
                                       runtime.facingX, runtime.facingY)) {
                ShowMessage(runtime, LocalText(runtime, L"PATH READY  C PLACE  X CANCEL",
                                               L"PATH 준비  C 설치  X 취소"));
            } else {
                ShowMessage(runtime, LocalText(runtime, L"NO PATH TILES", L"PATH가 없습니다"));
            }
        } else {
            const int x = runtime.placement.x;
            const int y = runtime.placement.y;
            bool changed = false;
            bool keepSelection = false;
            if (x >= 0 && y >= 0 && x < game.width && y < game.height) {
                const lc::PathState path = game.paths[TileIndex(game, x, y)];
                if (path == lc::PathState::Empty) {
                    changed = lc::PlacePath(game, x, y);
                    if (changed) {
                        AcruxSynth_PlaySfx(ACRUX_SFX_PATH_PLACE);
                        ShowMessage(runtime, LocalText(runtime, L"PATH SET FOR NEXT DAWN",
                                                       L"다음 새벽에 PATH가 켜집니다"));
                    } else {
                        ShowMessage(runtime, LocalText(runtime, L"PATH BLOCKED", L"PATH 설치 불가"));
                    }
                } else if (path != lc::PathState::Broken) {
                    changed = lc::RemovePath(game, x, y);
                    if (changed) {
                        AcruxSynth_PlaySfx(ACRUX_SFX_PATH_REMOVE);
                        ShowMessage(runtime, LocalText(runtime, L"PATH RECOVERED", L"PATH 회수"));
                    }
                } else {
                    changed = lc::RecoverBrokenPath(game, x, y);
                    if (changed) {
                        AcruxSynth_PlaySfx(ACRUX_SFX_PATH_REMOVE);
                        ShowMessage(runtime, LocalText(runtime, L"PATH MATERIAL RECOVERED",
                                                       L"PATH 재료 회수"));
#ifdef _DEBUG
                        ++runtime.brokenPathRecoveredCount;
#endif
                    } else {
                        keepSelection = true;
                        ShowMessage(runtime, LocalText(runtime, L"Z  REPAIR WITH ONE PATH",
                                                       L"Z  PATH 하나로 수리"));
                    }
                }
            }
            if (changed) lc::RefreshRelayWiring(game);
            runtime.placement.active = keepSelection;
        }
    }

    if (gInput.pressed['X']) {
        if (runtime.placement.active) {
            lc::CancelPathPlacement(runtime.placement);
            ShowMessage(runtime, LocalText(runtime, L"PATH CANCELLED", L"PATH 취소"));
        } else if (lc::UseBlink(game)) {
            AcruxSynth_PlaySfx(ACRUX_SFX_BLINK);
            runtime.blinkOriginX = runtime.playerX;
            runtime.blinkOriginY = runtime.playerY;
            runtime.playerAction = PlayerActionAnimation::Blink;
            runtime.playerActionTime = 0.36f;
            lc::ApplyBlinkToAnimals(game, runtime.playerX / kTilePixels,
                                    runtime.playerY / kTilePixels,
                                    runtime.blinkOriginX / kTilePixels,
                                    runtime.blinkOriginY / kTilePixels);
            const float signalRange = game.waterCrisis
                ? kCrisisResourceSignalTiles : kResourceSignalTiles;
            const int nearest = lc::FindNearestResource(game, runtime.playerX / kTilePixels,
                                                        runtime.playerY / kTilePixels, signalRange);
            if (nearest >= 0) {
                AcruxSynth_PlaySfx(ACRUX_SFX_STAR,
                    WorldPan(runtime, static_cast<float>(game.resources[nearest].x) + 0.5f));
            }
            ShowMessage(runtime, LocalText(runtime, L"BLINK", L"점멸"));
        } else {
            ShowMessage(runtime, LocalText(runtime, L"BLINK RECHARGING", L"점멸 재충전 중"));
        }
    }

    if (gInput.pressed['Z']) {
        if (runtime.placement.active) {
            lc::CancelPathPlacement(runtime.placement);
        }
        const float centerX = runtime.playerX / kTilePixels;
        const float centerY = runtime.playerY / kTilePixels;
        const lc::Interaction action = lc::ResolveInteraction(game, centerX, centerY);
        if (action.kind == lc::InteractionKind::Repair &&
            lc::RepairPath(game, action.pathX, action.pathY)) {
            runtime.playerAction = PlayerActionAnimation::Take;
            runtime.playerActionTime = 0.32f;
            AcruxSynth_PlaySfx(ACRUX_SFX_REPAIR,
                               WorldPan(runtime, static_cast<float>(action.pathX) + 0.5f));
            lc::RefreshRelayWiring(game);
            ShowMessage(runtime, LocalText(runtime, L"CIRCUIT RESTORED", L"회로 복구"));
#ifdef _DEBUG
            ++runtime.repairCount;
#endif
        } else if (action.kind == lc::InteractionKind::Take) {
            const lc::ResourceKind kind = game.resources[action.resourceIndex].kind;
            if (lc::CollectResource(game, action.resourceIndex)) {
                runtime.playerAction = PlayerActionAnimation::Take;
                runtime.playerActionTime = 0.32f;
                AcruxSynth_PlaySfx(ACRUX_SFX_PICKUP);
                const wchar_t* labels[] = {
                    LocalText(runtime, L"FOOD RECOVERED", L"식량 획득"),
                    LocalText(runtime, L"WATER RECOVERED", L"물 획득"),
                    LocalText(runtime, L"PATH MATERIAL RECOVERED", L"PATH 재료 획득")};
                ShowMessage(runtime, labels[static_cast<int>(kind)]);
            }
        } else if (action.kind == lc::InteractionKind::Sleep) {
            runtime.uiScreen = UiScreen::BaseMenu;
            runtime.menuSelection = 0;
        }
    }
}

void MovePlayer(Runtime& runtime, float seconds) {
    runtime.playerMoving = false;
    if (runtime.game.phase != lc::GamePhase::Night) return;
    float dx = 0.0f;
    float dy = 0.0f;
    if (gInput.down[VK_LEFT]) dx -= 1.0f;
    if (gInput.down[VK_RIGHT]) dx += 1.0f;
    if (gInput.down[VK_UP]) dy -= 1.0f;
    if (gInput.down[VK_DOWN]) dy += 1.0f;
    if (dx == 0.0f && dy == 0.0f) return;
    if (dx != 0.0f && dy != 0.0f) {
        dx *= 0.7071067f;
        dy *= 0.7071067f;
    }
    if (std::fabs(dx) > std::fabs(dy)) {
        runtime.facingX = dx < 0.0f ? -1 : 1;
        runtime.facingY = 0;
    } else {
        runtime.facingX = 0;
        runtime.facingY = dy < 0.0f ? -1 : 1;
    }

    float speedScale = runtime.game.foodCrisis ? 0.8f : 1.0f;
    if (runtime.game.snareTimer > 0.0f) speedScale *= 0.5f;
    const float nextX = runtime.playerX + dx * kPlayerSpeed * speedScale * seconds;
    const float nextY = runtime.playerY + dy * kPlayerSpeed * speedScale * seconds;
    if (!IsBlockedAt(runtime.game, nextX, runtime.playerY)) runtime.playerX = nextX;
    if (!IsBlockedAt(runtime.game, runtime.playerX, nextY)) runtime.playerY = nextY;
    runtime.playerMoving = true;
    runtime.playerWalkTime += seconds;
    lc::UpdatePathPlacement(runtime.placement,
                            static_cast<int>(runtime.playerX) / kTilePixels,
                            static_cast<int>(runtime.playerY) / kTilePixels,
                            runtime.facingX, runtime.facingY);

}

void StepAnimalRuntime(Runtime& runtime, float seconds,
                       const lc::AnimalMode* previousModes) {
    lc::AnimalInput input = {};
    input.playerX = runtime.playerX / kTilePixels;
    input.playerY = runtime.playerY / kTilePixels;
    input.afterimageX = runtime.blinkOriginX / kTilePixels;
    input.afterimageY = runtime.blinkOriginY / kTilePixels;
    input.playerSafe = PlayerOnPoweredPath(runtime);
    input.afterimageActive = runtime.game.afterimageTime > 0.0f;
    lc::AnimalEvents events = {};
    lc::StepAnimals(runtime.game, input, seconds, events);
    bool mothAlertPlayed = false;
    bool boarAlertPlayed = false;
    for (int index = 0; index < runtime.game.animalCount; ++index) {
        const lc::Animal& animal = runtime.game.animals[index];
        if (previousModes[index] == lc::AnimalMode::Idle &&
            animal.mode == lc::AnimalMode::Alert) {
            const bool alreadyPlayed = animal.kind == lc::AnimalKind::Moth
                ? mothAlertPlayed : boarAlertPlayed;
            if (!alreadyPlayed) {
                AcruxSynth_PlaySfx(animal.kind == lc::AnimalKind::Moth
                                      ? ACRUX_SFX_MOTH_ALERT : ACRUX_SFX_BOAR_ALERT,
                                  WorldPan(runtime, animal.x));
                if (animal.kind == lc::AnimalKind::Moth) mothAlertPlayed = true;
                else boarAlertPlayed = true;
            }
        }
        if (animal.kind == lc::AnimalKind::Boar &&
            previousModes[index] == lc::AnimalMode::Charge &&
            animal.mode == lc::AnimalMode::Stunned && animal.timer > 6.5f) {
            runtime.boarImpactTimer = kBoarImpactFxSeconds;
            runtime.boarImpactX = animal.x;
            runtime.boarImpactY = animal.y;
            runtime.boarImpactMirror = input.playerX < animal.x;
        }
    }
    if (events.pathBroken) {
        AcruxSynth_PlaySfx(ACRUX_SFX_PATH_BREAK,
                           WorldPan(runtime, static_cast<float>(events.pathBreakX) + 0.5f));
    }
    if (events.personalLightLost > 0.0f &&
        events.damageSourceKind == static_cast<int>(lc::AnimalKind::Moth)) {
        AcruxSynth_PlaySfx(ACRUX_SFX_MOTH_CONTACT,
                           NearestAnimalPan(runtime, lc::AnimalKind::Moth));
        ShowMessage(runtime, LocalText(runtime, L"THE MOTH DRINKS LIGHT",
                                       L"나방이 빛을 마신다"));
    } else if (events.personalLightLost > 0.0f) {
        AcruxSynth_PlaySfx(ACRUX_SFX_BOAR_CONTACT,
                           NearestAnimalPan(runtime, lc::AnimalKind::Boar));
        ShowMessage(runtime, LocalText(runtime, L"THE BOAR SCATTERS YOUR SUPPLIES",
                                       L"멧돼지가 자원을 흩뜨렸다"));
    }
}

void CompleteEndingTransition(Runtime& runtime) {
    runtime.game.completed = true;
    runtime.game.completionDay = runtime.game.day;
    runtime.game.phase = lc::GamePhase::Day;
    PlacePlayerAtBaseExit(runtime);
    ClearDayLore(runtime);
    lc::BeginNight(runtime.game);
    AcruxSynth_EnterEpilogue(ToAudioSeason(lc::CurrentSeason(runtime.game)));
    runtime.hasValidSave = WriteSaveFile(runtime.game);
#ifdef _DEBUG
    if (runtime.hasValidSave) ++runtime.saveCheckpointCount;
#endif
    lc::CancelPathPlacement(runtime.placement);
    runtime.menuSelection = 0;
}

#ifdef _DEBUG
bool DebugCanRouteThrough(const lc::GameState& game, int x, int y) {
    if (x < 0 || y < 0 || x >= game.width || y >= game.height) return false;
    if (lc::IsBasePuddleTile(game, x, y)) return false;
    if (lc::IsRelayFootprintTile(game, x, y)) return false;
    const int tile = TileIndex(game, x, y);
    if (game.paths[tile] == lc::PathState::Broken) return false;
    return game.terrain[tile] == lc::Terrain::Ground || game.terrain[tile] == lc::Terrain::Base;
}

bool DebugConnectRelay(Runtime& runtime, int relayIndex) {
    lc::GameState& game = runtime.game;
    bool target[lc::kMaxTiles] = {};
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};
    const lc::TileRect rect = lc::GetRelayFootprintRect(game, relayIndex);
    for (int side = 0; side < 4; ++side) {
        const bool horizontal = side < 2;
        const int fixed = side == 0 ? rect.y - 1 :
            side == 1 ? rect.y + rect.height : side == 2 ? rect.x - 1 : rect.x + rect.width;
        const int start = horizontal ? rect.x : rect.y;
        const int end = start + (horizontal ? rect.width : rect.height);
        for (int offset = start; offset < end; ++offset) {
            const int x = horizontal ? offset : fixed;
            const int y = horizontal ? fixed : offset;
            if (DebugCanRouteThrough(game, x, y)) target[TileIndex(game, x, y)] = true;
        }
    }

    int previous[lc::kMaxTiles] = {};
    int queue[lc::kMaxTiles] = {};
    for (int index = 0; index < lc::kMaxTiles; ++index) previous[index] = -1;
    const int start = TileIndex(game, game.baseX, game.baseY);
    int read = 0;
    int write = 0;
    queue[write++] = start;
    previous[start] = start;
    int found = -1;
    while (read < write && found < 0) {
        const int tile = queue[read++];
        const int x = tile % game.width;
        const int y = tile / game.width;
        if (target[tile]) {
            found = tile;
            break;
        }
        for (int direction = 0; direction < 4; ++direction) {
            const int nx = x + kDx[direction];
            const int ny = y + kDy[direction];
            if (!DebugCanRouteThrough(game, nx, ny)) continue;
            const int next = TileIndex(game, nx, ny);
            if (previous[next] >= 0) continue;
            previous[next] = tile;
            queue[write++] = next;
        }
    }
    if (found < 0) return false;

    int route[lc::kMaxTiles] = {};
    int routeCount = 0;
    for (int tile = found; tile != start && routeCount < lc::kMaxTiles; tile = previous[tile]) {
        route[routeCount++] = tile;
    }
    for (int step = routeCount - 1; step >= 0; --step) {
        const int tile = route[step];
        const int x = tile % game.width;
        const int y = tile / game.width;
        if (game.paths[tile] == lc::PathState::Empty) {
            lc::PlacePath(game, x, y);
        }
    }
    return true;
}

void DebugAutoConnectEnding(Runtime& runtime) {
    if (!runtime.started) ResetGame(runtime, true);
    if (runtime.game.completed) {
        ShowMessage(runtime, L"DEBUG ALREADY IN EPILOGUE", 2.0f);
        return;
    }
    if (runtime.game.phase != lc::GamePhase::Night) {
        lc::BeginNight(runtime.game);
    }
    runtime.game.pathInventory = 1000;
    bool connected = true;
    for (int relay = 0; relay < runtime.game.relayCount; ++relay) {
        connected = DebugConnectRelay(runtime, relay) && connected;
    }
    lc::ChargeConnectedPaths(runtime.game);
    lc::RefreshRelayWiring(runtime.game);
    if (connected && runtime.game.relaysConnected >= 3) {
        lc::SleepAtBase(runtime.game);
        runtime.uiScreen = UiScreen::Gameplay;
        runtime.dayTimer = runtime.game.phase == lc::GamePhase::Ending
            ? kEndingTransitionSeconds : kDayTransitionSeconds;
        ClearDayLore(runtime);
        ShowMessage(runtime, L"DEBUG ENDING ROUTE CONNECTED", 2.0f);
    } else {
        ShowMessage(runtime, L"DEBUG ROUTE FAILED", 2.0f);
    }
}

void DebugPrepareEndingPreview(Runtime& runtime) {
    ResetGame(runtime, true);
    runtime.game.pathInventory = 1000;
    bool connected = true;
    for (int relay = 0; relay < runtime.game.relayCount; ++relay) {
        connected = DebugConnectRelay(runtime, relay) && connected;
    }
    lc::ChargeConnectedPaths(runtime.game);
    lc::RefreshRelayWiring(runtime.game);
    PlacePlayerAtBaseExit(runtime);
    runtime.message = connected && runtime.game.relaysConnected >= 3
        ? L"ENDING PREVIEW READY: Z THEN SLEEP"
        : L"ENDING PREVIEW ROUTE FAILED";
    runtime.messageTimer = 5.0f;
}
#endif

void Step(Runtime& runtime, float seconds) {
#ifdef _DEBUG
    if (gInput.pressed[VK_F2]) gDebugOverlay = !gDebugOverlay;
    if (gInput.pressed[VK_F3]) {
        if (!runtime.started) ResetGame(runtime, true);
        runtime.game.pathInventory = 400;
        ShowMessage(runtime, L"DEBUG PATH MATERIAL READY", 2.0f);
    }
    if (gInput.pressed[VK_F4]) {
        DebugAutoConnectEnding(runtime);
    }
    if (gInput.pressed[VK_F5]) {
        gDebugFullVisibility = !gDebugFullVisibility;
        ShowMessage(runtime, gDebugFullVisibility
                                 ? L"DEBUG FULL VISIBILITY ON"
                                 : L"DEBUG FULL VISIBILITY OFF",
                    2.0f);
    }
#endif
    runtime.totalTime += seconds;
    runtime.messageTimer = Clamp(runtime.messageTimer - seconds, 0.0f, 100.0f);
    runtime.playerActionTime = Clamp(runtime.playerActionTime - seconds, 0.0f, 10.0f);
    runtime.boarImpactTimer = Clamp(runtime.boarImpactTimer - seconds, 0.0f, 10.0f);
    if (runtime.playerActionTime <= 0.0f) runtime.playerAction = PlayerActionAnimation::None;
    if (HandleFrontendInput(runtime)) return;

    StepFirstNightHints(runtime, seconds);
    HandleActions(runtime);

    if (runtime.game.phase == lc::GamePhase::Day) {
        if (runtime.dayLoreIndex >= 0 && gInput.pressed['Z']) {
            ClearDayLore(runtime);
        }
        runtime.dayTimer -= seconds;
        if (runtime.dayTimer <= 0.0f) {
            PlacePlayerAtBaseExit(runtime);
            ClearDayLore(runtime);
            lc::BeginNight(runtime.game);
            ShowMessage(runtime, LocalText(runtime, L"NIGHT RETURNS", L"밤이 돌아온다"), 2.0f);
        }
        return;
    }
    if (runtime.game.phase == lc::GamePhase::Ending) {
        runtime.dayTimer -= seconds;
        if (runtime.dayTimer <= 0.0f) {
            CompleteEndingTransition(runtime);
        }
        return;
    }
    if (runtime.game.phase != lc::GamePhase::Night) return;

    MovePlayer(runtime, seconds);
    lc::AnimalMode previousModes[lc::kMaxAnimals] = {};
    for (int index = 0; index < runtime.game.animalCount; ++index) {
        previousModes[index] = runtime.game.animals[index].mode;
    }
    lc::TrapEvents trapEvents = {};
    lc::StepTraps(runtime.game, runtime.playerX / kTilePixels,
                  runtime.playerY / kTilePixels, seconds, trapEvents);
    if (trapEvents.rustleTriggered) AcruxSynth_PlaySfx(ACRUX_SFX_RUSTLE);
    if (trapEvents.snareTriggered) AcruxSynth_PlaySfx(ACRUX_SFX_SNARE);
    if (trapEvents.rustleTriggered) {
        ShowMessage(runtime, LocalText(runtime, L"THE BRUSH CRIES OUT", L"수풀이 울부짖는다"));
    } else if (trapEvents.snareTriggered) {
        ShowMessage(runtime, LocalText(runtime, L"SNARED", L"덫에 걸렸다"));
    }
    StepAnimalRuntime(runtime, seconds, previousModes);
    const bool canCharge = PlayerOnPoweredPath(runtime) && gInput.down[VK_SPACE] && !runtime.playerMoving;
#ifdef _DEBUG
    if (canCharge && !runtime.wasCharging) ++runtime.manualChargeCount;
    if (canCharge) runtime.manualChargeTime += seconds;
    runtime.wasCharging = canCharge;
#endif
    lc::StepNight(runtime.game, seconds, PlayerOnPoweredPath(runtime), canCharge);
    if (runtime.game.phase == lc::GamePhase::Day) {
        AcruxSynth_PlaySfx(ACRUX_SFX_FAILURE);
        runtime.dayTimer = kDayTransitionSeconds;
        lc::CancelPathPlacement(runtime.placement);
        ShowMessage(runtime,
                    runtime.game.lastFailure == lc::FailureReason::BaseEnergy
                        ? L"BASE POWER LOST" : L"PERSONAL LIGHT LOST",
                    1.5f);
    }
}

void RenderEndingOverview(const Runtime& runtime) {
    (void)runtime;
    DrawCutsceneImage(lc::CutsceneId::Ending, 1.0f);
}

void RenderDaylight(const Runtime& runtime) {
    const float progress = Clamp(runtime.dayTimer / kDayTransitionSeconds, 0.0f, 1.0f);
    const lc::CutsceneId scene = runtime.dayTimer > 4.0f
        ? lc::CutsceneId::Sleep
        : (runtime.dayTimer > 2.0f ? lc::CutsceneId::SolarCharge : lc::CutsceneId::Dusk);
    const float segment = runtime.dayTimer > 4.0f
        ? (kDayTransitionSeconds - runtime.dayTimer) / 2.0f
        : (runtime.dayTimer > 2.0f ? (4.0f - runtime.dayTimer) / 2.0f
                                   : (2.0f - runtime.dayTimer) / 2.0f);
    const float edge = segment < 0.18f ? segment / 0.18f
        : (segment > 0.82f ? (1.0f - segment) / 0.18f : 1.0f);
    const float brightness = 0.62f + 0.38f * Clamp(edge, 0.0f, 1.0f);
    DrawCutsceneImage(scene, brightness);
    if (runtime.dayTimer > 2.0f) {
        const int pulse = static_cast<int>(
            (0.5f + 0.5f * std::sin((1.0f - progress) * kPi * 7.0f)) * 54.0f);
        BlendCircle(480, 150, 40 + pulse / 6, Color(255, 176, 72), 28 + pulse / 2);
        BlendCircle(480, 270, 115 + pulse / 2, Color(0, 202, 255), 18 + pulse / 4);
    }
}

void RenderWorld(const Runtime& runtime) {
    if (!runtime.started) {
        if (runtime.uiScreen == UiScreen::Title) {
            DrawTitleBackground();
        } else {
            for (int y = 0; y < kScreenHeight; ++y) {
                for (int x = 0; x < kScreenWidth; ++x) {
                    const unsigned hash = static_cast<unsigned>((x + 31) * 73856093u) ^
                                          static_cast<unsigned>((y + 47) * 19349663u);
                    const unsigned star = (hash % 733u == 0u) ? 20u : 0u;
                    gPixels[y * kScreenWidth + x] = Color(2u + star / 3u, 5u + star / 2u,
                                                           14u + star);
                }
            }
        }
        return;
    }
    if (runtime.game.phase == lc::GamePhase::Ending) {
        RenderEndingOverview(runtime);
        return;
    }
    if (runtime.game.phase == lc::GamePhase::Day) {
        RenderDaylight(runtime);
        return;
    }
    for (int y = 0; y < kScreenHeight; ++y) {
        for (int x = 0; x < kScreenWidth; ++x) {
            const unsigned hash = static_cast<unsigned>((x + 17) * 73856093u) ^
                                  static_cast<unsigned>((y + 31) * 19349663u);
            const unsigned star = (hash % 997u == 0u) ? 8u : 0u;
            gPixels[y * kScreenWidth + x] = Color(2u + star, 4u + star, 11u + star * 2u);
        }
    }
    std::memset(gLight, 14, sizeof(gLight));

    const float cameraX = Clamp(runtime.playerX - kScreenWidth * 0.5f, 0.0f,
                                runtime.game.width * kTilePixels - static_cast<float>(kScreenWidth));
    const float cameraY = Clamp(runtime.playerY - kScreenHeight * 0.5f, 0.0f,
                                runtime.game.height * kTilePixels - static_cast<float>(kScreenHeight));
    const int firstTileX = static_cast<int>(cameraX) / kTilePixels;
    const int firstTileY = static_cast<int>(cameraY) / kTilePixels;
    const int lastTileX = (static_cast<int>(cameraX) + kScreenWidth) / kTilePixels + 1;
    const int lastTileY = (static_cast<int>(cameraY) + kScreenHeight) / kTilePixels + 1;

    for (int tileY = firstTileY; tileY <= lastTileY && tileY < runtime.game.height; ++tileY) {
        for (int tileX = firstTileX; tileX <= lastTileX && tileX < runtime.game.width; ++tileX) {
            if (tileX < 0 || tileY < 0) continue;
            const int index = TileIndex(runtime.game, tileX, tileY);
            const int screenX = tileX * kTilePixels - static_cast<int>(cameraX);
            const int screenY = tileY * kTilePixels - static_cast<int>(cameraY);
            const bool powered = runtime.game.powered[index];
            const int tileAnchorX = screenX + kTileCenter;
            const int tileAnchorY = screenY + 18;
            const unsigned groundVariant = (static_cast<unsigned>(tileX) * 73856093u ^
                                            static_cast<unsigned>(tileY) * 19349663u) & 3u;
            DrawProductionSprite(static_cast<lc::ProductionSpriteId>(
                                     static_cast<int>(lc::ProductionSpriteId::Ground1) + groundVariant),
                                 tileAnchorX, tileAnchorY);
            bool loopBoundary = false;
            if (powered) {
                constexpr int kDx[4] = {1, -1, 0, 0};
                constexpr int kDy[4] = {0, 0, 1, -1};
                for (int direction = 0; direction < 4; ++direction) {
                    const int neighborX = tileX + kDx[direction];
                    const int neighborY = tileY + kDy[direction];
                    if (neighborX >= 0 && neighborY >= 0 &&
                        neighborX < runtime.game.width && neighborY < runtime.game.height &&
                        runtime.game.loopLit[TileIndex(runtime.game, neighborX, neighborY)]) {
                        loopBoundary = true;
                    }
                }
            }

            if (runtime.game.loopLit[index]) {
                RaiseLightRect(screenX, screenY, kTilePixels, kTilePixels, 180);
                AddLight(screenX + kTileCenter, screenY + kTileCenter, WorldPixels(36), 178);
            }

            if (powered) {
                BlendCircle(screenX + kTileCenter, screenY + kTileCenter,
                            WorldPixels(loopBoundary ? 48 : 36),
                            Color(0, 190, 255), loopBoundary ? 78 : 55);
                DrawProductionSprite(lc::IsStabilizedPath(runtime.game, tileX, tileY)
                                         ? lc::ProductionSpriteId::PathStabilized
                                         : lc::ProductionSpriteId::PathCharged,
                                     tileAnchorX, tileAnchorY);
                AddLight(screenX + kTileCenter, screenY + kTileCenter,
                         WorldPixels(loopBoundary ? 48 : 36),
                          loopBoundary ? 205 : 178);
            } else if (runtime.game.paths[index] != lc::PathState::Empty) {
                DrawProductionSprite(runtime.game.paths[index] == lc::PathState::Broken
                                         ? lc::ProductionSpriteId::PathBroken
                                         : lc::ProductionSpriteId::PathUncharged,
                                     tileAnchorX, tileAnchorY);
                if (runtime.game.paths[index] == lc::PathState::Broken) {
                    BlendCircle(screenX + kTileCenter, screenY + kTileCenter,
                                WorldPixels(24), Color(255, 134, 37), 58);
                    DrawProductionSprite(lc::ProductionSpriteId::PathScrap,
                                         tileAnchorX, tileAnchorY);
                    const int breakFrame = static_cast<int>(runtime.totalTime / 0.14f) % 3;
                    BlendProductionSpriteRegionScaled(lc::ProductionSpriteId::PathBreakFxAmber,
                                                      breakFrame * 24, 0, 24, 24, 12, 18,
                                                      tileAnchorX, tileAnchorY, 2, 165, 1.16f);
                }
            }

            const lc::Terrain terrain = runtime.game.terrain[index];
            if (terrain == lc::Terrain::Rock) {
                const int variant = (tileX + tileY * 3) % 3;
                DrawProductionSprite(static_cast<lc::ProductionSpriteId>(
                                         static_cast<int>(lc::ProductionSpriteId::Rock1) + variant),
                                     tileAnchorX, tileAnchorY);
            } else if (terrain == lc::Terrain::Bush) {
                const int variant = (tileX * 2 + tileY) % 3;
                DrawProductionSprite(static_cast<lc::ProductionSpriteId>(
                                         static_cast<int>(lc::ProductionSpriteId::Bush1) + variant),
                                     tileAnchorX, tileAnchorY);
            } else if (terrain == lc::Terrain::Base) {
                const int objectAnchorX = screenX;
                const int objectAnchorY = screenY + 18;
                const float baseDx = runtime.playerX / kTilePixels - static_cast<float>(tileX);
                const float baseDy = runtime.playerY / kTilePixels - static_cast<float>(tileY);
                const bool nearBase = baseDx * baseDx + baseDy * baseDy <= 2.5f * 2.5f;
                BlendCircle(screenX, screenY, WorldPixels(28), Color(47, 219, 255), 70);
                BlendCircle(screenX, screenY, WorldPixels(18), Color(255, 188, 62), 45);
                AddLight(screenX, screenY, WorldPixels(30), 150);
                DrawProductionSprite(lc::ProductionSpriteId::BaseObservatory,
                                     objectAnchorX, objectAnchorY);
                if (nearBase) {
                    DrawProductionSprite((static_cast<int>(runtime.totalTime / 0.22f) & 1)
                                             ? lc::ProductionSpriteId::BaseInteraction2
                                             : lc::ProductionSpriteId::BaseInteraction1,
                                         objectAnchorX, objectAnchorY);
                }
            } else if (terrain == lc::Terrain::Relay) {
                const int objectAnchorX = screenX + kTilePixels;
                const int objectAnchorY = screenY + kTilePixels + 18;
                bool lit = false;
                int relayIndex = 0;
                for (int relay = 0; relay < runtime.game.relayCount; ++relay) {
                    if (runtime.game.relayX[relay] == tileX && runtime.game.relayY[relay] == tileY) {
                        lit = runtime.game.relayPowered[relay];
                        relayIndex = relay;
                    }
                }
                if (lit) {
                    BlendCircle(screenX + kTilePixels, screenY + kTilePixels,
                                WorldPixels(44), Color(255, 204, 83), 120);
                    BlendCircle(screenX + kTilePixels, screenY + kTilePixels,
                                WorldPixels(58), Color(53, 230, 255), 74);
                    AddLight(screenX + kTilePixels, screenY + kTilePixels,
                             WorldPixels(54), 225);
                }
                const lc::ProductionSpriteId relaySprite = lit
                    ? static_cast<lc::ProductionSpriteId>(
                          static_cast<int>(lc::ProductionSpriteId::Relay1Connected) + relayIndex)
                    : static_cast<lc::ProductionSpriteId>(
                          static_cast<int>(lc::ProductionSpriteId::Relay1) + relayIndex);
                DrawProductionSprite(relaySprite, objectAnchorX, objectAnchorY);
            }
        }
    }

    DrawBasePuddle(runtime, cameraX, cameraY);

    const float playerTileX = runtime.playerX / kTilePixels;
    const float playerTileY = runtime.playerY / kTilePixels;
#ifdef _DEBUG
    const bool revealAllForDebug = gDebugFullVisibility;
#else
    constexpr bool revealAllForDebug = false;
#endif
    const float coreVisionTiles = runtime.game.waterCrisis ? 2.25f : 3.0f;
    const float peripheralVisionTiles = runtime.game.waterCrisis ? 3.75f : 5.0f;
    constexpr float kBlinkRevealTiles = 6.0f;
    const float signalRange = runtime.game.waterCrisis
        ? kCrisisResourceSignalTiles : kResourceSignalTiles;
    const int nearest = runtime.game.afterimageTime > 0.0f
        ? lc::FindNearestResource(runtime.game, playerTileX, playerTileY, signalRange) : -1;
    if (nearest >= 0) {
        const lc::ResourceNode& resource = runtime.game.resources[nearest];
        const float dx = (resource.x + 0.5f) * kTilePixels - runtime.playerX;
        const float dy = (resource.y + 0.5f) * kTilePixels - runtime.playerY;
        const float length = std::sqrt(dx * dx + dy * dy + 0.01f);
        const int starX = static_cast<int>(runtime.playerX - cameraX + dx / length * 27.0f);
        const int starY = static_cast<int>(runtime.playerY - cameraY + dy / length * 27.0f);
        const int twinkle = 2 + (static_cast<int>(runtime.totalTime * 8.0f) & 1);
        DrawStar(starX, starY, twinkle, Color(255, 224, 142));
        BlendCircle(starX, starY, 8, Color(255, 187, 65), 35);

    }

    for (int index = 0; index < runtime.game.resourceCount; ++index) {
        const lc::ResourceNode& resource = runtime.game.resources[index];
        if (!resource.active) continue;
        const float resourceDx = static_cast<float>(resource.x) + 0.5f - playerTileX;
        const float resourceDy = static_cast<float>(resource.y) + 0.5f - playerTileY;
        const float resourceDistance = resourceDx * resourceDx + resourceDy * resourceDy;
        const bool blinkReveals = runtime.game.afterimageTime > 0.0f &&
            resourceDistance <= kBlinkRevealTiles * kBlinkRevealTiles;
        if (!revealAllForDebug &&
            resourceDistance > coreVisionTiles * coreVisionTiles && !blinkReveals) continue;
        const int resourceX = static_cast<int>((resource.x + 0.5f) * kTilePixels - cameraX);
        const int resourceY = static_cast<int>((resource.y + 0.5f) * kTilePixels - cameraY);
        const lc::ProductionSpriteId resourceSprites[] = {
            lc::ProductionSpriteId::Food, lc::ProductionSpriteId::Water,
            lc::ProductionSpriteId::PathMaterial};
        DrawProductionSprite(resourceSprites[static_cast<int>(resource.kind)],
                             resourceX, resourceY + 6);
    }

    for (int index = 0; index < runtime.game.animalCount; ++index) {
        const lc::Animal& animal = runtime.game.animals[index];
        if (!animal.active) continue;
        const float animalDx = animal.x - playerTileX;
        const float animalDy = animal.y - playerTileY;
        const float animalDistance = animalDx * animalDx + animalDy * animalDy;
        const bool blinkReveals = runtime.game.afterimageTime > 0.0f &&
            animalDistance <= kBlinkRevealTiles * kBlinkRevealTiles;
        if (!revealAllForDebug &&
            animalDistance > peripheralVisionTiles * peripheralVisionTiles && !blinkReveals) continue;
        const int x = static_cast<int>(animal.x * kTilePixels - cameraX);
        const int y = static_cast<int>(animal.y * kTilePixels - cameraY);
        const bool stunned = animal.mode == lc::AnimalMode::Stunned;
        const bool attacking = animal.mode == lc::AnimalMode::Charge;
        const bool warning = animal.mode == lc::AnimalMode::Alert;
        if (animal.kind == lc::AnimalKind::Moth) {
            lc::ProductionSpriteId sprite = (static_cast<int>(runtime.totalTime / 0.18f) & 1)
                                                ? lc::ProductionSpriteId::MothIdleB
                                                : lc::ProductionSpriteId::MothIdleA;
            if (stunned) sprite = lc::ProductionSpriteId::MothStunned;
            else if (attacking) sprite = lc::ProductionSpriteId::MothCharge;
            else if (warning) sprite = lc::ProductionSpriteId::MothAlert;
            BlendCircle(x, y, attacking ? 14 : 10, Color(255, 92, 38), attacking ? 55 : 28);
            DrawProductionSprite(sprite, x, y);
            if (warning) DrawCircleOutline(x, y, 8, Color(255, 178, 244));
        } else {
            lc::ProductionSpriteId sprite = (static_cast<int>(runtime.totalTime / 0.22f) & 1)
                                                ? lc::ProductionSpriteId::BoarBaseB
                                                : lc::ProductionSpriteId::BoarBaseA;
            if (stunned) sprite = (animal.timer > 0.26f)
                                      ? lc::ProductionSpriteId::BoarStunnedDrop
                                      : lc::ProductionSpriteId::BoarStunnedLoop;
            else if (attacking) sprite = lc::ProductionSpriteId::BoarChargeDash;
            else if (warning) sprite = (static_cast<int>(runtime.totalTime / 0.11f) & 1)
                                           ? lc::ProductionSpriteId::BoarChargePrep2
                                           : lc::ProductionSpriteId::BoarChargePrep1;
            const bool mirror = animal.vx < -0.01f ||
                                (std::fabs(animal.vx) <= 0.01f && animal.targetX < animal.x);
            DrawProductionSprite(sprite, x, y, mirror);
            if (stunned) {
                const int stunFrame = static_cast<int>(runtime.totalTime / 0.7f) & 1;
                BlendProductionSpriteRegionScaled(lc::ProductionSpriteId::FxBoarStunLoop,
                                                  stunFrame * 40, 0, 40, 24, 20, 18,
                                                  x, y, 2, 175, 1.12f, mirror);
            }
            if (warning) {
                const float dx = runtime.playerX - animal.x * kTilePixels;
                const float dy = runtime.playerY - animal.y * kTilePixels;
                const float length = std::sqrt(dx * dx + dy * dy + 0.01f);
                DrawLine(x, y, x + static_cast<int>(dx / length * 20.0f),
                         y + static_cast<int>(dy / length * 20.0f), Color(255, 96, 96));
                DrawCircleOutline(x, y, 9, Color(255, 128, 96));
            } else if (stunned) {
                DrawCircleOutline(x, y, 9, Color(108, 235, 255));
            }
        }
    }

    if (runtime.boarImpactTimer > 0.0f) {
        const float progress = 1.0f - Clamp(runtime.boarImpactTimer / kBoarImpactFxSeconds,
                                           0.0f, 1.0f);
        const int impactFrame = static_cast<int>(progress * 3.0f);
        const int clampedFrame = impactFrame < 0 ? 0 : (impactFrame > 2 ? 2 : impactFrame);
        const int impactX = static_cast<int>(runtime.boarImpactX * kTilePixels - cameraX);
        const int impactY = static_cast<int>(runtime.boarImpactY * kTilePixels - cameraY);
        const int impactAlpha = 95 + static_cast<int>((1.0f - progress) * 55.0f);
        BlendCircle(impactX, impactY, WorldPixels(34), Color(255, 170, 48), impactAlpha);
        BlendCircle(impactX, impactY, WorldPixels(22), Color(92, 235, 255), 70);
        BlendProductionSpriteRegionScaled(lc::ProductionSpriteId::FxBoarPathImpact,
                                          clampedFrame * 24, 0, 24, 24, 12, 12,
                                          impactX, impactY, 2, 205, 1.2f,
                                          runtime.boarImpactMirror);
    }

    for (int index = 0; index < runtime.game.trapCount; ++index) {
        const lc::Trap& trap = runtime.game.traps[index];
        const float trapDx = static_cast<float>(trap.x) + 0.5f - playerTileX;
        const float trapDy = static_cast<float>(trap.y) + 0.5f - playerTileY;
        const float trapDistance = trapDx * trapDx + trapDy * trapDy;
        const bool blinkReveals = runtime.game.afterimageTime > 0.0f &&
            trapDistance <= kBlinkRevealTiles * kBlinkRevealTiles;
        if (!revealAllForDebug &&
            trapDistance > coreVisionTiles * coreVisionTiles && !blinkReveals) continue;
        const int x = trap.x * kTilePixels - static_cast<int>(cameraX) + kTileCenter;
        const int y = trap.y * kTilePixels - static_cast<int>(cameraY) + kTileCenter;
        if (trap.kind == lc::TrapKind::Rustle) {
            DrawProductionSprite(trap.triggered ? lc::ProductionSpriteId::RustleTriggered
                                                : lc::ProductionSpriteId::RustleReady,
                                 x, y + 6);
        } else {
            DrawProductionSprite(trap.triggered ? lc::ProductionSpriteId::SnareTriggered
                                                : lc::ProductionSpriteId::SnareReady,
                                 x, y + 6);
        }
    }

    if (runtime.placement.active) {
        const int x = runtime.placement.x * kTilePixels - static_cast<int>(cameraX);
        const int y = runtime.placement.y * kTilePixels - static_cast<int>(cameraY);
        const std::uint32_t color = lc::CanPlacePath(runtime.game, runtime.placement.x, runtime.placement.y)
                                        ? Color(100, 245, 255)
                                        : Color(255, 64, 96);
        DrawLine(x + 1, y + 1, x + kTilePixels - 2, y + 1, color);
        DrawLine(x + kTilePixels - 2, y + 1, x + kTilePixels - 2, y + kTilePixels - 2, color);
        DrawLine(x + kTilePixels - 2, y + kTilePixels - 2, x + 1, y + kTilePixels - 2, color);
        DrawLine(x + 1, y + kTilePixels - 2, x + 1, y + 1, color);
    }

    const int playerX = static_cast<int>(runtime.playerX - cameraX);
    const int playerY = static_cast<int>(runtime.playerY - cameraY);
    BlendCircle(playerX, playerY, 13, Color(240, 229, 177), 35);
    BlendCircle(playerX, playerY - 5, 4, Color(220, 226, 225), 180);
    lc::SpriteId playerSprite = lc::SpriteId::PlayerFrontIdle;
    bool mirrorPlayer = false;
    const bool charging = PlayerOnPoweredPath(runtime) && gInput.down[VK_SPACE] && !runtime.playerMoving;
    if (runtime.playerAction == PlayerActionAnimation::Blink) {
        playerSprite = runtime.playerActionTime > 0.18f
                           ? lc::SpriteId::PlayerBlink1
                           : lc::SpriteId::PlayerBlink2;
    } else if (runtime.playerAction == PlayerActionAnimation::Take || charging) {
        playerSprite = (runtime.playerActionTime > 0.16f ||
                        (charging && (static_cast<int>(runtime.totalTime / 0.18f) & 1) == 0))
                           ? lc::SpriteId::PlayerTake1
                           : lc::SpriteId::PlayerTake2;
        mirrorPlayer = runtime.facingX > 0;
    } else {
        const int walkFrame = 1 + static_cast<int>(runtime.playerWalkTime / 0.12f) % 3;
        if (runtime.facingY < 0) {
            playerSprite = runtime.playerMoving
                               ? static_cast<lc::SpriteId>(static_cast<int>(lc::SpriteId::PlayerBackIdle) + walkFrame)
                               : lc::SpriteId::PlayerBackIdle;
        } else if (runtime.facingX != 0) {
            playerSprite = runtime.playerMoving
                               ? static_cast<lc::SpriteId>(static_cast<int>(lc::SpriteId::PlayerSideIdle) + walkFrame)
                               : lc::SpriteId::PlayerSideIdle;
            mirrorPlayer = runtime.facingX > 0;
        } else {
            playerSprite = runtime.playerMoving
                               ? static_cast<lc::SpriteId>(static_cast<int>(lc::SpriteId::PlayerFrontIdle) + walkFrame)
                               : lc::SpriteId::PlayerFrontIdle;
        }
    }
    DrawSprite(playerSprite, playerX, playerY, mirrorPlayer);
    if (charging) {
        const int chargeFrame = static_cast<int>(runtime.totalTime / 0.11f) % 3;
        const int anchorX = mirrorPlayer ? 21 : 2;
        BlendCircle(playerX, playerY - 2, 30, Color(79, 225, 255), 70);
        BlendCircle(playerX, playerY - 6, 17, Color(255, 205, 94), 54);
        BlendProductionSpriteRegionScaled(lc::ProductionSpriteId::FxChargeManual,
                                          chargeFrame * 24, 0, 24, 24, anchorX, 22,
                                          playerX, playerY, 2, 185, 1.12f,
                                          mirrorPlayer);
    }

    const float personalRatio = runtime.game.personalLightMax > 0.0f
                                    ? runtime.game.personalLight / runtime.game.personalLightMax
                                    : 0.0f;
    const int peripheralRadius = runtime.game.waterCrisis ? 90 : 120;
    const int coreRadius = runtime.game.waterCrisis ? 54 : 72;
    AddLight(playerX, playerY, peripheralRadius, 112 + static_cast<int>(18.0f * personalRatio));
    AddLight(playerX, playerY, coreRadius, 218 + static_cast<int>(28.0f * personalRatio));
    const int baseScreenX = runtime.game.baseX * kTilePixels - static_cast<int>(cameraX) + kTileCenter;
    const int baseScreenY = runtime.game.baseY * kTilePixels - static_cast<int>(cameraY) + kTileCenter;
    AddLight(baseScreenX, baseScreenY, WorldPixels(64), 220);

    int blinkRadius = 0;
    if (runtime.game.afterimageTime > 0.0f) {
        const float elapsed = 1.0f - runtime.game.afterimageTime;
        const float expansion = Clamp(elapsed / 0.18f, 0.0f, 1.0f);
        blinkRadius = static_cast<int>(static_cast<float>(kTilePixels * 6) * expansion);
        const float fade = runtime.game.afterimageTime;
        AddLight(playerX, playerY, blinkRadius, 245 + static_cast<int>(10.0f * fade));
    }
#ifdef _DEBUG
    if (!gDebugFullVisibility) ApplyLightMask();
#else
    ApplyLightMask();
#endif
    DrawEpilogueSeasonParticles(runtime);
    if (blinkRadius > 2) {
        DrawCircleOutline(playerX, playerY, blinkRadius, Color(99, 225, 255));
        if (blinkRadius > 4) {
            DrawCircleOutline(playerX, playerY, blinkRadius - 2, Color(30, 111, 143));
        }
    }

    constexpr int mapSide = 48;
    constexpr int mapPixels = mapSide * kUiScale;
    constexpr int mapX = kScreenWidth - mapPixels - 9;
    constexpr int mapY = 38;
    const int playerMapX = static_cast<int>(runtime.playerX) / kTilePixels;
    const int playerMapY = static_cast<int>(runtime.playerY) / kTilePixels;
    const int worldLeft = playerMapX - mapSide / 2;
    const int worldTop = playerMapY - mapSide / 2;
    FillRect(mapX - 3, mapY - 3, mapPixels + 6, mapPixels + 6, Color(2, 8, 15));
    for (int localY = 0; localY < mapSide; ++localY) {
        for (int localX = 0; localX < mapSide; ++localX) {
            const int worldX = worldLeft + localX;
            const int worldY = worldTop + localY;
            if (worldX < 0 || worldY < 0 || worldX >= runtime.game.width || worldY >= runtime.game.height) continue;
            const int index = TileIndex(runtime.game, worldX, worldY);
            if (runtime.game.loopLit[index]) {
                FillRect(mapX + localX * kUiScale, mapY + localY * kUiScale,
                         kUiScale, kUiScale, Color(18, 58, 69));
            }
            if (runtime.game.powered[index]) {
                FillRect(mapX + localX * kUiScale, mapY + localY * kUiScale,
                         kUiScale, kUiScale, Color(35, 188, 218));
            }
        }
    }
    if (runtime.game.baseX >= worldLeft && runtime.game.baseX < worldLeft + mapSide &&
        runtime.game.baseY >= worldTop && runtime.game.baseY < worldTop + mapSide) {
        FillRect(mapX + (runtime.game.baseX - worldLeft) * kUiScale,
                 mapY + (runtime.game.baseY - worldTop) * kUiScale,
                 kUiScale, kUiScale, Color(255, 211, 91));
    }
    for (int relay = 0; relay < runtime.game.relayCount; ++relay) {
        const int relayX = runtime.game.relayX[relay];
        const int relayY = runtime.game.relayY[relay];
        if (runtime.game.relayPowered[relay] && relayX >= worldLeft && relayX < worldLeft + mapSide &&
            relayY >= worldTop && relayY < worldTop + mapSide) {
            FillRect(mapX + (relayX - worldLeft) * kUiScale,
                     mapY + (relayY - worldTop) * kUiScale,
                     kUiScale, kUiScale, Color(255, 244, 184));
        }
    }
    FillRect(mapX + (mapSide / 2) * kUiScale, mapY + (mapSide / 2) * kUiScale,
             kUiScale, kUiScale, Color(255, 255, 255));

}

const wchar_t* InteractionPrompt(const Runtime& runtime) {
    if (!runtime.started || runtime.uiScreen != UiScreen::Gameplay ||
        runtime.game.phase != lc::GamePhase::Night) return nullptr;
    const lc::Interaction action = lc::ResolveInteraction(
        runtime.game, runtime.playerX / kTilePixels, runtime.playerY / kTilePixels);
    if (action.kind == lc::InteractionKind::Repair) {
        return LocalText(runtime, L"Z  REPAIR", L"Z  수리");
    }
    if (action.kind == lc::InteractionKind::Sleep) {
        return LocalText(runtime, L"Z  BASE", L"Z  기지");
    }
    if (action.kind == lc::InteractionKind::Take) {
        return LocalText(runtime, L"Z  TAKE", L"Z  줍기");
    }
    return nullptr;
}

struct UiFonts {
    int scale;
    HFONT title;
    HFONT heading;
    HFONT menu;
    HFONT body;
};

UiFonts& GetUiFonts(int scale, lc::Language language) {
    static UiFonts fontSets[2] = {};
    UiFonts& fonts = fontSets[language == lc::Language::Korean ? 1 : 0];
    if (fonts.scale == scale && fonts.title != nullptr) return fonts;
    if (fonts.title != nullptr) DeleteObject(fonts.title);
    if (fonts.heading != nullptr) DeleteObject(fonts.heading);
    if (fonts.menu != nullptr) DeleteObject(fonts.menu);
    if (fonts.body != nullptr) DeleteObject(fonts.body);
    fonts.scale = scale;
    const wchar_t* face = language == lc::Language::Korean ? L"Malgun Gothic" : L"Consolas";
    const BYTE pitch = language == lc::Language::Korean ? VARIABLE_PITCH : FIXED_PITCH;
    fonts.title = CreateFontW(-56 * scale, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              NONANTIALIASED_QUALITY, pitch, face);
    fonts.heading = CreateFontW(-28 * scale, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                NONANTIALIASED_QUALITY, pitch, face);
    fonts.menu = CreateFontW(-22 * scale, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             NONANTIALIASED_QUALITY, pitch, face);
    fonts.body = CreateFontW(-17 * scale, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             NONANTIALIASED_QUALITY, pitch, face);
    return fonts;
}

void DrawCenteredText(HDC surface, int centerX, int top, const wchar_t* text,
                      HFONT font, COLORREF color) {
    const HGDIOBJ oldFont = SelectObject(surface, font);
    SetTextColor(surface, color);
    SIZE size = {};
    const int length = lstrlenW(text);
    GetTextExtentPoint32W(surface, text, length, &size);
    TextOutW(surface, centerX - size.cx / 2, top, text, length);
    SelectObject(surface, oldFont);
}

void DrawMenu(HDC surface, int centerX, int top, int lineHeight, const wchar_t* const* items,
              int itemCount, int selection, HFONT font) {
    for (int index = 0; index < itemCount; ++index) {
        wchar_t line[80] = {};
        wsprintfW(line, index == selection ? L">  %s" : L"   %s", items[index]);
        DrawCenteredText(surface, centerX, top + lineHeight * index, line, font,
                         index == selection ? RGB(83, 230, 234) : RGB(162, 167, 191));
    }
}

void DrawUiPanel(HDC surface, int left, int top, int width, int height,
                 float intensity = 1.0f) {
    intensity = Clamp(intensity, 0.0f, 1.0f);
    RECT panel = {left, top, left + width, top + height};
    HBRUSH background = CreateSolidBrush(FadeColor(Color(7, 11, 24), intensity));
    ::FillRect(surface, &panel, background);
    DeleteObject(background);
    HPEN border = CreatePen(PS_SOLID, 2, FadeColor(Color(35, 139, 154), intensity));
    const HGDIOBJ oldPen = SelectObject(surface, border);
    const HGDIOBJ oldBrush = SelectObject(surface, GetStockObject(NULL_BRUSH));
    Rectangle(surface, panel.left, panel.top, panel.right, panel.bottom);
    SelectObject(surface, oldBrush);
    SelectObject(surface, oldPen);
    DeleteObject(border);
}

void DrawHudBar(HDC surface, int left, int top, int width, int height,
                float ratio, COLORREF fillColor) {
    RECT background = {left, top, left + width, top + height};
    HBRUSH dark = CreateSolidBrush(RGB(12, 20, 35));
    ::FillRect(surface, &background, dark);
    DeleteObject(dark);
    const int inset = 2;
    const int fillWidth = static_cast<int>((width - inset * 2) * Clamp(ratio, 0.0f, 1.0f));
    RECT fill = {left + inset, top + inset, left + inset + fillWidth, top + height - inset};
    HBRUSH light = CreateSolidBrush(fillColor);
    ::FillRect(surface, &fill, light);
    DeleteObject(light);
    HPEN border = CreatePen(PS_SOLID, 1, RGB(44, 104, 122));
    const HGDIOBJ oldPen = SelectObject(surface, border);
    const HGDIOBJ oldBrush = SelectObject(surface, GetStockObject(NULL_BRUSH));
    Rectangle(surface, background.left, background.top, background.right, background.bottom);
    SelectObject(surface, oldBrush);
    SelectObject(surface, oldPen);
    DeleteObject(border);
}

#ifdef _DEBUG
const wchar_t* PhaseLabel(lc::GamePhase phase) {
    if (phase == lc::GamePhase::Day) return L"DAY";
    if (phase == lc::GamePhase::Night) return L"NIGHT";
    if (phase == lc::GamePhase::Ending) return L"ENDING";
    return L"GAME OVER";
}

void DrawDebugOverlay(HDC surface, int x, int y, const UiFonts& fonts) {
    DrawUiPanel(surface, x, y, 520, 164);
    const HGDIOBJ oldFont = SelectObject(surface, fonts.body);
    SetTextColor(surface, RGB(162, 167, 191));
    wchar_t line[180] = {};
    const int tileX = static_cast<int>(gRuntime.playerX) / kTilePixels;
    const int tileY = static_cast<int>(gRuntime.playerY) / kTilePixels;
    wsprintfW(line, L"F2 F3 F4 F5:%s  %dFPS  %s D%d  %d,%d",
              gDebugFullVisibility ? L"FULL" : L"MASK",
              static_cast<int>(1.0f / (gSmoothedFrameSeconds > 0.0001f
                                          ? gSmoothedFrameSeconds : 0.0001f) + 0.5f),
              PhaseLabel(gRuntime.game.phase), gRuntime.game.day, tileX, tileY);
    TextOutW(surface, x + 12, y + 10, line, lstrlenW(line));
    wsprintfW(line, L"LIGHT %d/%d  BASE %d/%d  RELAY %d/%d  CONNECTED %d",
              static_cast<int>(gRuntime.game.personalLight),
              static_cast<int>(gRuntime.game.personalLightMax),
              static_cast<int>(gRuntime.game.baseEnergy),
              static_cast<int>(gRuntime.game.baseEnergyMax),
              gRuntime.game.relaysPowered, gRuntime.game.relayCount,
              gRuntime.game.relaysConnected);
    TextOutW(surface, x + 12, y + 38, line, lstrlenW(line));

    int moths = 0;
    int boars = 0;
    int aware = 0;
    int charging = 0;
    for (int index = 0; index < gRuntime.game.animalCount; ++index) {
        const lc::Animal& animal = gRuntime.game.animals[index];
        if (!animal.active) continue;
        if (animal.kind == lc::AnimalKind::Moth) ++moths;
        else ++boars;
        if (animal.mode != lc::AnimalMode::Idle && animal.mode != lc::AnimalMode::Retreat &&
            animal.mode != lc::AnimalMode::Stunned) ++aware;
        if (animal.mode == lc::AnimalMode::Charge) ++charging;
    }
    wsprintfW(line, L"MOTH %d  BOAR %d  AWARE %d  CHARGE %d  AFTERIMAGE %dMS",
              moths, boars, aware, charging,
              static_cast<int>(gRuntime.game.afterimageTime * 1000.0f));
    TextOutW(surface, x + 12, y + 66, line, lstrlenW(line));
    wsprintfW(line, L"FOOD %d%s  WATER %d%s  PATH MAT %u  PLACE %s",
              gRuntime.game.food, gRuntime.game.foodCrisis ? L"!" : L"",
              gRuntime.game.water, gRuntime.game.waterCrisis ? L"!" : L"",
              static_cast<unsigned>(gRuntime.game.pathInventory),
              gRuntime.placement.active ? L"READY" : L"OFF");
    TextOutW(surface, x + 12, y + 94, line, lstrlenW(line));
    wsprintfW(line, L"CHARGE %d/%dMS  STORAGE %d  PATH D/W %d/%d  SAVE/CONT %d/%d",
              gRuntime.manualChargeCount,
              static_cast<int>(gRuntime.manualChargeTime * 1000.0f),
              gRuntime.storageOpenCount, gRuntime.pathDeposited, gRuntime.pathWithdrawn,
              gRuntime.saveCheckpointCount, gRuntime.continueLoadCount);
    TextOutW(surface, x + 12, y + 122, line, lstrlenW(line));
    SelectObject(surface, oldFont);
}
#endif

void Present(HWND window, HDC dc) {
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = kScreenWidth;
    info.bmiHeader.biHeight = -kScreenHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    RECT client = {};
    GetClientRect(window, &client);
    const int clientWidth = client.right - client.left;
    const int clientHeight = client.bottom - client.top;
    if (clientWidth <= 0 || clientHeight <= 0) return;

    static HDC backDc = nullptr;
    static HBITMAP backBitmap = nullptr;
    static HGDIOBJ backOldBitmap = nullptr;
    static int backWidth = 0;
    static int backHeight = 0;
    if (backDc == nullptr || backWidth != clientWidth || backHeight != clientHeight) {
        if (backDc != nullptr) {
            SelectObject(backDc, backOldBitmap);
            DeleteObject(backBitmap);
            DeleteDC(backDc);
        }
        backDc = CreateCompatibleDC(dc);
        backBitmap = CreateCompatibleBitmap(dc, clientWidth, clientHeight);
        backOldBitmap = SelectObject(backDc, backBitmap);
        backWidth = clientWidth;
        backHeight = clientHeight;
    }
    HDC surface = backDc;
    int scale = clientWidth / kScreenWidth;
    const int verticalScale = clientHeight / kScreenHeight;
    if (verticalScale < scale) scale = verticalScale;
    if (scale < 1) scale = 1;
    const int width = kScreenWidth * scale;
    const int height = kScreenHeight * scale;
    const int x = (clientWidth - width) / 2;
    const int y = (clientHeight - height) / 2;
    const bool gameplayHudVisible = gRuntime.started &&
        gRuntime.uiScreen == UiScreen::Gameplay &&
        gRuntime.game.phase == lc::GamePhase::Night;
    if (gameplayHudVisible) {
        DrawProductionUiSprite(lc::ProductionSpriteId::HudPersonalLight, 15, 13);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudBaseEnergy, 15, 43);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudNightCycle, 370, 19);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudRelay, 483, 19);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudFood, 10, 498);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudWater, 64, 498);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudPathTiles, 118, 498);
        DrawProductionUiSprite(lc::ProductionSpriteId::HudBlink, 788, 493);

        int warningLeft = 12;
        if (gRuntime.game.foodCrisis || gRuntime.game.food <= 1) {
            DrawProductionUiSprite(gRuntime.game.foodCrisis
                                       ? lc::ProductionSpriteId::UiFoodCritical
                                       : lc::ProductionSpriteId::HudFood,
                                   warningLeft, 92);
            warningLeft += 28;
        }
        if (gRuntime.game.waterCrisis || gRuntime.game.water <= 1) {
            DrawProductionUiSprite(gRuntime.game.waterCrisis
                                       ? lc::ProductionSpriteId::UiWaterCritical
                                       : lc::ProductionSpriteId::HudWater,
                                   warningLeft, 92);
        }
    }
    if (gRuntime.uiScreen == UiScreen::BaseMenu) {
        const float cameraX = Clamp(gRuntime.playerX - kScreenWidth * 0.5f, 0.0f,
                                    gRuntime.game.width * kTilePixels - static_cast<float>(kScreenWidth));
        const float cameraY = Clamp(gRuntime.playerY - kScreenHeight * 0.5f, 0.0f,
                                    gRuntime.game.height * kTilePixels - static_cast<float>(kScreenHeight));
        const int baseCenterX = static_cast<int>(gRuntime.game.baseX * kTilePixels - cameraX);
        const int baseTopY = static_cast<int>(gRuntime.game.baseY * kTilePixels - 44 - cameraY);
        int panelLeft = baseCenterX - 40;
        int panelTop = baseTopY - 70;
        if (panelLeft < 8) panelLeft = 8;
        if (panelLeft > kScreenWidth - 88) panelLeft = kScreenWidth - 88;
        if (panelTop < 8) panelTop = 8;
        DrawProductionUiSprite(lc::ProductionSpriteId::BaseContextPanel, panelLeft, panelTop);
        DrawProductionSpriteRegion(lc::ProductionSpriteId::BaseContextCursor,
                                   gRuntime.menuSelection * 8, 0, 8, 8, 0, 0,
                                   panelLeft + 10, panelTop + (gRuntime.menuSelection == 0 ? 18 : 39));
    } else if (gRuntime.uiScreen == UiScreen::Storage) {
        DrawProductionUiSprite(lc::ProductionSpriteId::BaseStorageWindow,
                               (kScreenWidth - 176) / 2, 118);
    }
    PatBlt(surface, 0, 0, clientWidth, clientHeight, BLACKNESS);
    SetStretchBltMode(surface, COLORONCOLOR);
    StretchDIBits(surface, x, y, width, height, 0, 0, kScreenWidth, kScreenHeight, gPixels, &info,
                  DIB_RGB_COLORS, SRCCOPY);

    SetBkMode(surface, TRANSPARENT);
    UiFonts& fonts = GetUiFonts(scale, gRuntime.language);
    const int centerX = x + width / 2;
    const auto screenY = [y, scale](int logicalY) { return y + logicalY * scale; };

    if (gameplayHudVisible) {
        const int energyLeft = x + 10 * scale;
        const int energyTop = y + 10 * scale;
        const int objectiveLeft = centerX - 115 * scale;
        const int objectiveTop = y + 10 * scale;
        const int carryLeft = x + 10 * scale;
        const int carryTop = y + height - 42 * scale;
        const int blinkLeft = x + width - 180 * scale;
        const int blinkTop = y + height - 60 * scale;
        const HGDIOBJ oldStatusFont = SelectObject(surface, fonts.body);
        wchar_t text[96] = {};
        SetTextColor(surface, RGB(158, 211, 222));
        const wchar_t* lightLabel = LocalText(gRuntime, L"LIGHT", L"빛");
        const wchar_t* baseLabel = LocalText(gRuntime, L"BASE", L"기지");
        TextOutW(surface, energyLeft + 34 * scale, energyTop + 9 * scale,
                 lightLabel, lstrlenW(lightLabel));
        TextOutW(surface, energyLeft + 34 * scale, energyTop + 39 * scale,
                 baseLabel, lstrlenW(baseLabel));
        DrawHudBar(surface, energyLeft + 98 * scale, energyTop + 11 * scale,
                   160 * scale, 14 * scale,
                   gRuntime.game.personalLight / gRuntime.game.personalLightMax,
                   RGB(79, 225, 255));
        DrawHudBar(surface, energyLeft + 98 * scale, energyTop + 41 * scale,
                   160 * scale, 14 * scale,
                   gRuntime.game.baseEnergy / gRuntime.game.baseEnergyMax,
                   RGB(255, 176, 72));
        SetTextColor(surface, RGB(210, 239, 245));
        wsprintfW(text, IsKorean(gRuntime) ? L"%d일" : L"DAY %d", gRuntime.game.day);
        TextOutW(surface, objectiveLeft + 34 * scale, objectiveTop + 11 * scale,
                 text, lstrlenW(text));
        wsprintfW(text, L"%d/3", gRuntime.game.relaysPowered);
        TextOutW(surface, objectiveLeft + 150 * scale, objectiveTop + 11 * scale,
                 text, lstrlenW(text));

        SetTextColor(surface, RGB(232, 244, 247));
        wsprintfW(text, L"%d", gRuntime.game.carried[static_cast<int>(lc::ResourceKind::Food)]);
        TextOutW(surface, carryLeft + 27 * scale, carryTop + 3 * scale, text, lstrlenW(text));
        wsprintfW(text, L"%d", gRuntime.game.carried[static_cast<int>(lc::ResourceKind::Water)]);
        TextOutW(surface, carryLeft + 81 * scale, carryTop + 3 * scale, text, lstrlenW(text));
        wsprintfW(text, L"%d", gRuntime.game.carried[static_cast<int>(lc::ResourceKind::Material)]);
        TextOutW(surface, carryLeft + 135 * scale, carryTop + 3 * scale, text, lstrlenW(text));

        if (gRuntime.game.blinkCooldown <= 0.0f) {
            DrawCenteredText(surface, blinkLeft + 106 * scale, blinkTop + 15 * scale,
                             LocalText(gRuntime, L"BLINK READY", L"점멸 가능"),
                             fonts.body, RGB(83, 230, 234));
        } else {
            wsprintfW(text, IsKorean(gRuntime) ? L"점멸 %d초" : L"BLINK %dS",
                      static_cast<int>(gRuntime.game.blinkCooldown + 0.99f));
            DrawCenteredText(surface, blinkLeft + 106 * scale, blinkTop + 15 * scale,
                             text, fonts.body, RGB(162, 167, 191));
        }

        const wchar_t* prompt = InteractionPrompt(gRuntime);
        if (prompt != nullptr) {
            DrawCenteredText(surface, centerX, y + height - 58 * scale, prompt, fonts.body,
                             RGB(243, 242, 250));
        }
        SelectObject(surface, oldStatusFont);
    }

    if (gRuntime.uiScreen == UiScreen::Title) {
        const wchar_t* languageItem = LocalText(gRuntime, L"LANGUAGE: ENGLISH", L"언어: 한국어");
        const wchar_t* itemsWithSave[] = {
            LocalText(gRuntime, L"NEW GAME", L"새 게임"),
            LocalText(gRuntime, L"CONTINUE", L"이어하기"),
            LocalText(gRuntime, L"HOW TO PLAY", L"조작법"),
            languageItem,
            LocalText(gRuntime, L"QUIT", L"종료"),
        };
        const wchar_t* itemsNoSave[] = {
            LocalText(gRuntime, L"NEW GAME", L"새 게임"),
            LocalText(gRuntime, L"HOW TO PLAY", L"조작법"),
            languageItem,
            LocalText(gRuntime, L"QUIT", L"종료"),
        };
        const wchar_t* const* items = gRuntime.hasValidSave ? itemsWithSave : itemsNoSave;
        const int itemCount = gRuntime.hasValidSave ? 5 : 4;
        DrawCenteredText(surface, centerX, screenY(252),
                         LocalText(gRuntime, L"FOLLOW THE GROUND-STARS", L"땅의 별을 따라가라"),
                         fonts.body, RGB(83, 230, 234));
        DrawMenu(surface, centerX, screenY(286), 38 * scale, items, itemCount,
                 gRuntime.menuSelection, fonts.menu);
        DrawCenteredText(surface, centerX, screenY(474),
                         LocalText(gRuntime, L"ARROWS SELECT    Z CONFIRM    X BACK",
                                   L"방향키 선택    Z 확인    X 뒤로"),
                         fonts.body, RGB(116, 124, 154));
    } else if (gRuntime.uiScreen == UiScreen::Intro) {
        DrawUiPanel(surface, centerX - 330 * scale, screenY(82), 660 * scale, 366 * scale);
        DrawCenteredText(surface, centerX, screenY(112), L"ACRUX",
                         fonts.heading, RGB(243, 242, 250));
        DrawCenteredText(surface, centerX, screenY(188),
                         LocalText(gRuntime, L"THE DAYS HAVE SHRUNK TO MOMENTS.",
                                   L"낮은 이제 찰나처럼 짧아졌다."),
                         fonts.body, RGB(215, 217, 235));
        DrawCenteredText(surface, centerX, screenY(229),
                         LocalText(gRuntime, L"AT NIGHT, THE GROUND-STARS KEEP US ALIVE.",
                                   L"밤에는 땅의 별만이 우리를 살린다."),
                         fonts.body, RGB(215, 217, 235));
        DrawCenteredText(surface, centerX, screenY(270),
                         LocalText(gRuntime, L"CONNECT THREE RELAYS. RETURN BEFORE THE LIGHT IS GONE.",
                                   L"세 릴레이를 잇고, 빛이 다하기 전에 돌아오라."),
                         fonts.body, RGB(215, 217, 235));
        DrawCenteredText(surface, centerX, screenY(360),
                         LocalText(gRuntime, L"Z  WAKE INTO THE NIGHT", L"Z  밤으로 나가기"),
                         fonts.menu, RGB(83, 230, 234));
        DrawCenteredText(surface, centerX, screenY(405),
                         LocalText(gRuntime, L"X  BACK", L"X  뒤로"),
                         fonts.body, RGB(116, 124, 154));
    } else if (gRuntime.uiScreen == UiScreen::HowToPlay) {
        DrawUiPanel(surface, centerX - 350 * scale, screenY(38), 700 * scale, 464 * scale);
        DrawCenteredText(surface, centerX, screenY(62),
                         LocalText(gRuntime, L"HOW TO PLAY", L"조작법"),
                         fonts.heading, RGB(243, 242, 250));
        const wchar_t* linesEn[] = {
            L"ARROWS   MOVE",
            L"Z        TAKE / REPAIR / OPEN BASE",
            L"C        SELECT PATH; ARROWS AIM; C PLACE / RECOVER",
            L"X        BLINK / CANCEL PATH",
            L"SPACE    HOLD AT BASE, POWERED RELAY, OR POWERED PATH",
            L"ESC PAUSE    F11 FULLSCREEN",
            L"PATH CACHE +20; PLACE OR REPAIR COSTS 2.",
            L"TIME SPENDS LIGHT; CHARGING SPENDS BASE ENERGY.",
            L"BLINK REVEALS NEARBY RESOURCES AND SPENDS LIGHT.",
            L"A CLOSED POWERED LOOP STUNS A CHARGING BOAR.",
            L"STORE FOOD AND WATER BEFORE THEY RUN OUT.",
            L"CARRIED SUPPLIES ARE LOST ON FAILURE; STORAGE IS SAFE.",
            L"CONNECT ALL THREE RELAYS.",
            L"RETURN HOME AND SLEEP TO COMPLETE THE CIRCUIT."
        };
        const wchar_t* linesKo[] = {
            L"방향키   이동",
            L"Z        줍기 / 수리 / 기지 열기",
            L"C        PATH 선택; 방향키 조준; C 설치 / 회수",
            L"X        점멸 / PATH 취소",
            L"SPACE    기지, 켜진 릴레이, 켜진 PATH에서 충전",
            L"ESC 일시정지    F11 전체화면",
            L"PATH 재료 +20; 설치나 수리는 2 소모.",
            L"시간은 개인 빛을, 충전은 기지 에너지를 쓴다.",
            L"점멸은 가까운 자원을 드러내고 빛을 쓴다.",
            L"닫힌 켜진 회로는 돌진하는 멧돼지를 기절시킨다.",
            L"식량과 물이 떨어지기 전에 창고에 저장하라.",
            L"실패하면 배낭 자원만 잃고 창고는 안전하다.",
            L"세 릴레이를 모두 연결하라.",
            L"집으로 돌아와 잠들면 회로가 완성된다."
        };
        const wchar_t* const* lines = IsKorean(gRuntime) ? linesKo : linesEn;
        for (int index = 0; index < 14; ++index) {
            DrawCenteredText(surface, centerX, screenY(96 + index * 26), lines[index],
                             fonts.body, index >= 6 ? RGB(83, 230, 234) : RGB(215, 217, 235));
        }
        DrawCenteredText(surface, centerX, screenY(472),
                         LocalText(gRuntime, L"Z OR X  BACK", L"Z 또는 X  뒤로"),
                         fonts.body, RGB(116, 124, 154));
    } else if (gRuntime.uiScreen == UiScreen::Pause) {
        const wchar_t* items[] = {
            LocalText(gRuntime, L"RESUME", L"계속하기"),
            LocalText(gRuntime, L"HOW TO PLAY", L"조작법"),
            LocalText(gRuntime, L"RETURN TO TITLE", L"타이틀로"),
            LocalText(gRuntime, L"QUIT", L"종료"),
        };
        DrawUiPanel(surface, centerX - 230 * scale, screenY(92), 460 * scale, 356 * scale);
        DrawCenteredText(surface, centerX, screenY(125),
                         LocalText(gRuntime, L"PAUSED", L"일시정지"),
                         fonts.heading, RGB(243, 242, 250));
        DrawMenu(surface, centerX, screenY(205), 46 * scale, items, 4,
                 gRuntime.menuSelection, fonts.menu);
        DrawCenteredText(surface, centerX, screenY(405),
                         LocalText(gRuntime, L"ESC OR X  RESUME", L"ESC 또는 X  계속하기"),
                         fonts.body, RGB(116, 124, 154));
    } else if (gRuntime.uiScreen == UiScreen::BaseMenu) {
        const wchar_t* items[] = {
            LocalText(gRuntime, L"SLEEP", L"수면"),
            LocalText(gRuntime, L"STORE", L"창고"),
        };
        const float cameraX = Clamp(gRuntime.playerX - kScreenWidth * 0.5f, 0.0f,
                                    gRuntime.game.width * kTilePixels - static_cast<float>(kScreenWidth));
        const float cameraY = Clamp(gRuntime.playerY - kScreenHeight * 0.5f, 0.0f,
                                    gRuntime.game.height * kTilePixels - static_cast<float>(kScreenHeight));
        const int baseCenterX = static_cast<int>(gRuntime.game.baseX * kTilePixels - cameraX);
        const int baseTopY = static_cast<int>(gRuntime.game.baseY * kTilePixels - 44 - cameraY);
        int panelLeft = baseCenterX - 40;
        int panelTop = baseTopY - 70;
        if (panelLeft < 8) panelLeft = 8;
        if (panelLeft > kScreenWidth - 88) panelLeft = kScreenWidth - 88;
        if (panelTop < 8) panelTop = 8;
        const HGDIOBJ oldFont = SelectObject(surface, fonts.body);
        SetTextColor(surface, gRuntime.menuSelection == 0 ? RGB(7, 11, 24) : RGB(76, 84, 108));
        TextOutW(surface, x + (panelLeft + 29) * scale, y + (panelTop + 13) * scale,
                 items[0], lstrlenW(items[0]));
        SetTextColor(surface, gRuntime.menuSelection == 1 ? RGB(7, 11, 24) : RGB(76, 84, 108));
        TextOutW(surface, x + (panelLeft + 29) * scale, y + (panelTop + 35) * scale,
                 items[1], lstrlenW(items[1]));
        SelectObject(surface, oldFont);
    } else if (gRuntime.uiScreen == UiScreen::Storage) {
        const int storageTop = screenY(118);
        DrawCenteredText(surface, centerX, storageTop + 11 * scale,
                         LocalText(gRuntime, L"STORAGE", L"창고"),
                         fonts.body, RGB(243, 242, 250));
        const wchar_t* labels[] = {
            LocalText(gRuntime, L"FOOD", L"식량"),
            LocalText(gRuntime, L"WATER", L"물"),
            LocalText(gRuntime, L"PATH", L"길"),
        };
        const int baseCounts[] = {
            gRuntime.game.food,
            gRuntime.game.water,
            static_cast<int>(gRuntime.game.pathInventory),
        };
        for (int index = 0; index < 3; ++index) {
            const int rowY = storageTop + (36 + index * 23) * scale;
            wchar_t line[96] = {};
            wsprintfW(line, gRuntime.storageSelection == index ? L"> %s  %d" : L"  %s  %d",
                      labels[index], baseCounts[index]);
            DrawCenteredText(surface, centerX, rowY, line,
                             fonts.body,
                             gRuntime.storageSelection == index ? RGB(83, 230, 234)
                                                               : RGB(215, 217, 235));
        }
        DrawCenteredText(surface, centerX, storageTop + 88 * scale,
                         gRuntime.storageTargetBase
                             ? LocalText(gRuntime, L"CARRY > STORAGE", L"배낭 > 창고")
                             : LocalText(gRuntime, L"STORAGE > CARRY", L"창고 > 배낭"),
                         fonts.body, RGB(243, 242, 250));
        DrawCenteredText(surface, centerX, screenY(426),
                         LocalText(gRuntime,
                                   L"UP / DOWN ITEM    LEFT TAKE    RIGHT STORE    Z MOVE",
                                   L"위/아래 물품    왼쪽 꺼내기    오른쪽 저장    Z 이동"),
                         fonts.body, RGB(116, 124, 154));
    } else if (gRuntime.uiScreen == UiScreen::ConfirmNewGame) {
        DrawUiPanel(surface, centerX - 310 * scale, screenY(166), 620 * scale, 208 * scale);
        DrawCenteredText(surface, centerX, screenY(198),
                         LocalText(gRuntime, L"START A NEW GAME?", L"새 게임을 시작할까요?"),
                         fonts.heading, RGB(243, 242, 250));
        DrawCenteredText(surface, centerX, screenY(270),
                         LocalText(gRuntime, L"CURRENT PROGRESS WILL BE REPLACED",
                                   L"현재 진행 상황이 바뀝니다"),
                         fonts.body, RGB(230, 163, 74));
        DrawCenteredText(surface, centerX, screenY(326),
                         LocalText(gRuntime, L"Z YES    X NO", L"Z 예    X 아니오"),
                         fonts.menu, RGB(83, 230, 234));
    } else if (gRuntime.uiScreen == UiScreen::ConfirmTitle) {
        DrawUiPanel(surface, centerX - 265 * scale, screenY(176), 530 * scale, 188 * scale);
        DrawCenteredText(surface, centerX, screenY(210),
                         LocalText(gRuntime, L"RETURN TO TITLE?", L"타이틀로 돌아갈까요?"),
                         fonts.heading, RGB(243, 242, 250));
        DrawCenteredText(surface, centerX, screenY(286),
                         LocalText(gRuntime, L"CURRENT RUN WILL BE LOST", L"현재 밤 진행은 사라집니다"),
                         fonts.body, RGB(230, 163, 74));
        DrawCenteredText(surface, centerX, screenY(326),
                         LocalText(gRuntime, L"Z YES    X NO", L"Z 예    X 아니오"),
                         fonts.menu, RGB(83, 230, 234));
    } else if (gRuntime.uiScreen == UiScreen::ConfirmQuit) {
        DrawUiPanel(surface, centerX - 240 * scale, screenY(190), 480 * scale, 160 * scale);
        DrawCenteredText(surface, centerX, screenY(220),
                         LocalText(gRuntime, L"QUIT ACRUX?", L"ACRUX를 종료할까요?"),
                         fonts.heading, RGB(243, 242, 250));
        DrawCenteredText(surface, centerX, screenY(296),
                         LocalText(gRuntime, L"Z YES    X NO", L"Z 예    X 아니오"),
                         fonts.menu, RGB(83, 230, 234));
    } else if (gRuntime.uiScreen == UiScreen::Gameplay &&
               gRuntime.game.phase == lc::GamePhase::Ending) {
        const float elapsed = kEndingTransitionSeconds - gRuntime.dayTimer;
        const wchar_t* heading = nullptr;
        const wchar_t* detail = nullptr;
        const wchar_t* whisper = nullptr;
        float stageStart = 0.0f;
        float stageEnd = 0.0f;
        if (elapsed >= 2.0f && elapsed < 7.0f) {
            stageStart = 2.0f;
            stageEnd = 7.0f;
            heading = LocalText(gRuntime, L"THE CIRCUIT HOLDS.", L"회로는 버텨냈다.");
            detail = LocalText(gRuntime,
                               L"THE PATH REMEMBERS EVERY NIGHT YOU CARRIED IT.",
                               L"네가 짊어진 밤들을 길은 기억한다.");
        } else if (elapsed >= 8.2f && elapsed < 13.2f) {
            stageStart = 8.2f;
            stageEnd = 13.2f;
            heading = LocalText(gRuntime, L"NOT EVERY LIGHT WAS LOST.",
                                L"모든 빛이 사라진 것은 아니었다.");
            detail = LocalText(gRuntime,
                               L"BEYOND THE DARK, OTHER WINDOWS BEGIN TO ANSWER.",
                               L"어둠 너머에서 다른 창들이 하나둘 응답한다.");
        } else if (elapsed >= 14.0f && elapsed < 17.2f) {
            stageStart = 14.0f;
            stageEnd = 17.2f;
            heading = LocalText(gRuntime, L"THE FIRST SNOW WILL NOT TAKE THE LIGHT.",
                                L"첫눈은 더 이상 빛을 빼앗지 못한다.");
            detail = LocalText(gRuntime,
                               L"WHEN IT FALLS, THE GROUND-STARS WILL STILL BE HERE.",
                               L"그 눈이 내려도, 땅의 별들은 여기에 남는다.");
        }
        if (heading != nullptr) {
            const float fadeIn = Clamp((elapsed - stageStart) / 0.65f, 0.0f, 1.0f);
            const float fadeOut = Clamp((stageEnd - elapsed) / 0.75f, 0.0f, 1.0f);
            const float fade = fadeIn < fadeOut ? fadeIn : fadeOut;
            DrawCenteredText(surface, centerX, screenY(410), heading, fonts.heading,
                             FadeColor(Color(243, 242, 250), fade));
            DrawCenteredText(surface, centerX, screenY(454), detail, fonts.body,
                             FadeColor(Color(83, 230, 234), fade));
            if (elapsed >= 8.2f && elapsed < 13.2f) {
                wchar_t days[64] = {};
                wsprintfW(days, IsKorean(gRuntime) ? L"%d일을 버텼다" : L"SURVIVED %d DAYS",
                          gRuntime.game.day);
                whisper = days;
                DrawCenteredText(surface, centerX, screenY(490), whisper, fonts.body,
                                 FadeColor(Color(215, 217, 235), fade));
            }
        }
    } else if (gRuntime.uiScreen == UiScreen::Gameplay &&
               gRuntime.game.phase == lc::GamePhase::GameOver) {
        const wchar_t* items[] = {
            LocalText(gRuntime, L"RETRY", L"다시"),
            LocalText(gRuntime, L"TITLE", L"타이틀"),
            LocalText(gRuntime, L"QUIT", L"종료"),
        };
        const wchar_t* failure = gRuntime.game.lastFailure == lc::FailureReason::Survival
            ? LocalText(gRuntime, L"FOOD OR WATER WAS LOST", L"식량 또는 물을 잃었다")
            : (gRuntime.game.lastFailure == lc::FailureReason::BaseEnergy
                ? LocalText(gRuntime, L"BASE POWER WAS LOST", L"기지의 전력이 사라졌다")
                : LocalText(gRuntime, L"PERSONAL LIGHT WAS LOST", L"개인의 빛이 사라졌다"));
        DrawUiPanel(surface, centerX - 280 * scale, screenY(158), 560 * scale, 300 * scale);
        DrawCenteredText(surface, centerX, screenY(188),
                         LocalText(gRuntime, L"NO MORNING", L"아침은 오지 않았다"),
                         fonts.heading, RGB(243, 242, 250));
        DrawCenteredText(surface, centerX, screenY(242), failure,
                         fonts.body, RGB(230, 163, 74));
        DrawMenu(surface, centerX, screenY(304), 42 * scale, items, 3,
                 gRuntime.menuSelection, fonts.menu);
    } else if (gRuntime.game.phase == lc::GamePhase::Day) {
        const wchar_t* day = gRuntime.dayTimer > 4.0f ? L"SLEEP"
                              : (gRuntime.dayTimer > 2.0f ? L"SOLAR CHARGE" : L"DUSK");
        const wchar_t* detail = gRuntime.dayTimer > 4.0f ? L"SUPPLIES SECURED"
                                 : (gRuntime.dayTimer > 2.0f
                                        ? L"BASE AND CONNECTED RELAYS CHARGING"
                                        : L"THE FOREST IS DARKENING");
        if (IsKorean(gRuntime)) {
            day = gRuntime.dayTimer > 4.0f ? L"수면"
                  : (gRuntime.dayTimer > 2.0f ? L"태양 충전" : L"황혼");
            detail = gRuntime.dayTimer > 4.0f ? L"자원이 보관되었다"
                     : (gRuntime.dayTimer > 2.0f
                            ? L"기지와 연결된 릴레이가 충전된다"
                            : L"숲이 다시 어두워진다");
        }
        const float elapsed = kDayTransitionSeconds - gRuntime.dayTimer;
        const float fadeIn = Clamp(elapsed / 0.25f, 0.0f, 1.0f);
        const float fadeOut = Clamp(gRuntime.dayTimer / 0.25f, 0.0f, 1.0f);
        const float fade = fadeIn < fadeOut ? fadeIn : fadeOut;
        DrawCenteredText(surface, centerX, screenY(412), day, fonts.heading,
                         FadeColor(Color(243, 242, 250), fade));
        DrawCenteredText(surface, centerX, screenY(450), detail, fonts.body,
                         FadeColor(Color(83, 230, 234), fade));
        if (gRuntime.dayLoreIndex >= 0) {
            DrawCenteredText(surface, centerX, screenY(482), gRuntime.dayLoreText,
                             fonts.body, FadeColor(Color(215, 217, 235), fade));
            DrawCenteredText(surface, centerX, screenY(510),
                             LocalText(gRuntime, L"Z SKIP", L"Z 넘기기"),
                             fonts.body, FadeColor(Color(116, 124, 154), fade));
        }
    }

    if (gameplayHudVisible &&
        gRuntime.messageTimer > 0.0f && gRuntime.message != nullptr) {
        DrawCenteredText(surface, centerX, y + height - 104 * scale,
                         gRuntime.message, fonts.body, RGB(210, 239, 245));
    }
#ifdef _DEBUG
    if (gDebugOverlay && gameplayHudVisible) {
        DrawDebugOverlay(surface, x + 12 * scale, y + height - 150 * scale, fonts);
    }
#endif
    BitBlt(dc, 0, 0, clientWidth, clientHeight, surface, 0, 0, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            gRunning = false;
            PostQuitMessage(0);
            return 0;
        case WM_KILLFOCUS:
            std::memset(gInput.down, 0, sizeof(gInput.down));
            std::memset(gInput.pressed, 0, sizeof(gInput.pressed));
            if (gRuntime.uiScreen == UiScreen::Gameplay &&
                (gRuntime.game.phase == lc::GamePhase::Night ||
                 gRuntime.game.phase == lc::GamePhase::Day)) {
                lc::CancelPathPlacement(gRuntime.placement);
                gRuntime.uiScreen = UiScreen::Pause;
                gRuntime.menuSelection = 0;
            }
            return 0;
        case WM_GETMINMAXINFO:
            if (!gFullscreen) {
                RECT minimum = {0, 0, kScreenWidth, kScreenHeight};
                const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE));
                const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE));
                AdjustWindowRectEx(&minimum, style, FALSE, exStyle);
                MINMAXINFO* limits = reinterpret_cast<MINMAXINFO*>(lParam);
                limits->ptMinTrackSize.x = minimum.right - minimum.left;
                limits->ptMinTrackSize.y = minimum.bottom - minimum.top;
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const unsigned key = static_cast<unsigned>(wParam);
            if (key < 256u) {
                const bool wasDown = (lParam & (1LL << 30)) != 0;
                if (!wasDown && key == VK_F11) {
                    ToggleFullscreen(window);
                    return 0;
                }
                gInput.down[key] = true;
                if (!wasDown) gInput.pressed[key] = true;
            }
            return 0;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            const unsigned key = static_cast<unsigned>(wParam);
            if (key < 256u) gInput.down[key] = false;
            return 0;
        }
        default:
            return DefWindowProcW(window, message, wParam, lParam);
    }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDPIAware();
    const wchar_t* className = L"LastCircuitWindow";
    WNDCLASSW windowClass = {};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass)) return 1;

    RECT desired = {0, 0, 960, 540};
    AdjustWindowRect(&desired, WS_OVERLAPPEDWINDOW, FALSE);
    HWND window = CreateWindowExW(0, className, L"ACRUX", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT, CW_USEDEFAULT, desired.right - desired.left,
                                  desired.bottom - desired.top, nullptr, nullptr, instance, nullptr);
    if (window == nullptr) return 2;
    ShowWindow(window, showCommand);

    ResetGame(gRuntime, false);
#if defined(_DEBUG) && defined(ACRUX_ENDING_PREVIEW_BUILD)
    DebugPrepareEndingPreview(gRuntime);
#endif
    AcruxAudio_Init();
    LARGE_INTEGER frequency = {};
    LARGE_INTEGER previous = {};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);
    double accumulator = 0.0;
    constexpr double fixedStep = 1.0 / 60.0;

    while (gRunning) {
        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) gRunning = false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        double frameSeconds = static_cast<double>(now.QuadPart - previous.QuadPart) /
                              static_cast<double>(frequency.QuadPart);
        previous = now;
        if (frameSeconds > 0.1) frameSeconds = 0.1;
#ifdef _DEBUG
        gSmoothedFrameSeconds +=
            (static_cast<float>(frameSeconds) - gSmoothedFrameSeconds) * 0.08f;
#endif
        accumulator += frameSeconds;
        int steps = 0;
        while (accumulator >= fixedStep && steps < 6) {
            Step(gRuntime, static_cast<float>(fixedStep));
            std::memset(gInput.pressed, 0, sizeof(gInput.pressed));
            accumulator -= fixedStep;
            ++steps;
        }

        UpdateAudioFrame(gRuntime, static_cast<float>(frameSeconds));
        RenderWorld(gRuntime);
        HDC dc = GetDC(window);
        Present(window, dc);
        ReleaseDC(window, dc);
        AcruxAudio_Update();
        Sleep(1);
    }
    AcruxAudio_Shutdown();
    return 0;
}
