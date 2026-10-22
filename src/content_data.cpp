#include "game_core.h"

namespace lc {

namespace {

struct Rect {
    int x;
    int y;
    int width;
    int height;
    Terrain terrain;
};

void FillRect(GameState& game, const Rect& rect) {
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        for (int x = rect.x; x < rect.x + rect.width; ++x) {
            if (x >= 0 && y >= 0 && x < game.width && y < game.height) {
                game.terrain[y * game.width + x] = rect.terrain;
            }
        }
    }
}

}  // namespace

void InitializeGame(GameState& game) {
    InitializeForTests(game, 256, 256);
    game.pathInventory = 32;
    game.food = 5;
    game.water = 5;
    game.randomState = 0x144C17u;

    SetBase(game, 128, 128);
    SetRelay(game, 0, 178, 82);
    SetRelay(game, 1, 73, 91);
    SetRelay(game, 2, 185, 174);

    constexpr Rect kObstacles[] = {
        {8, 10, 13, 3, Terrain::Rock},   {29, 7, 3, 12, Terrain::Bush},
        {45, 14, 15, 2, Terrain::Rock},  {70, 7, 3, 13, Terrain::Bush},
        {91, 12, 8, 3, Terrain::Rock},   {102, 12, 6, 3, Terrain::Rock},
        {115, 18, 3, 12, Terrain::Bush}, {12, 29, 3, 14, Terrain::Bush},
        {25, 31, 13, 3, Terrain::Rock},  {48, 27, 2, 12, Terrain::Bush},
        {88, 29, 14, 2, Terrain::Rock},  {107, 37, 3, 15, Terrain::Bush},
        {5, 55, 16, 3, Terrain::Rock},   {29, 51, 2, 14, Terrain::Bush},
        {43, 58, 12, 3, Terrain::Rock},  {73, 58, 3, 13, Terrain::Bush},
        {94, 55, 8, 3, Terrain::Rock},   {105, 55, 6, 3, Terrain::Rock},
        {116, 63, 3, 15, Terrain::Bush}, {9, 77, 14, 2, Terrain::Rock},
        {31, 75, 3, 14, Terrain::Bush},  {46, 84, 15, 3, Terrain::Rock},
        {74, 79, 2, 14, Terrain::Bush},  {91, 85, 7, 3, Terrain::Rock},
        {101, 85, 6, 3, Terrain::Rock},  {112, 91, 3, 14, Terrain::Bush},
        {13, 101, 16, 2, Terrain::Rock}, {36, 98, 3, 15, Terrain::Bush},
        {55, 108, 13, 3, Terrain::Rock}, {82, 102, 2, 15, Terrain::Bush},
        {99, 111, 8, 3, Terrain::Rock},  {110, 111, 6, 3, Terrain::Rock},
        {18, 118, 13, 2, Terrain::Bush}, {68, 120, 14, 3, Terrain::Rock},
    };
    constexpr int kObstacleOffsets[4][2] = {{0, 0}, {128, 0}, {0, 128}, {128, 128}};
    for (const auto& offset : kObstacleOffsets) {
        for (const Rect& rect : kObstacles) {
            Rect shifted = rect;
            shifted.x += offset[0];
            shifted.y += offset[1];
            FillRect(game, shifted);
        }
    }

    // Reassert objectives in case a later obstacle edit overlaps them.
    SetBase(game, 128, 128);
    SetRelay(game, 0, 178, 82);
    SetRelay(game, 1, 73, 91);
    SetRelay(game, 2, 185, 174);

    constexpr int kZoneCenters[3][2] = {{31, 38}, {97, 39}, {64, 106}};
    constexpr int kClusterOffsets[4][2] = {{-64, -54}, {64, -54}, {-64, 48}, {64, 48}};
    constexpr int kOffsets[30][2] = {
        {-12, -12}, {-4, -14}, {6, -13}, {13, -9}, {-15, -3}, {-7, -5},
        {3, -6}, {11, -2}, {-14, 5}, {-5, 3}, {5, 2}, {14, 6},
        {-12, 12}, {-3, 10}, {7, 11}, {13, 15}, {-20, -16}, {-12, -20},
        {0, -22}, {15, -18}, {21, -9}, {-22, 1}, {22, 4}, {-20, 14},
        {-19, -15}, {2, 22}, {21, -8}, {-9, 21}, {-2, -2}, {1, 15},
    };
    for (int zone = 0; zone < 3; ++zone) {
        for (const auto& cluster : kClusterOffsets) {
            const int centerX = game.baseX + (kZoneCenters[zone][0] - 64) + cluster[0];
            const int centerY = game.baseY + (kZoneCenters[zone][1] - 64) + cluster[1];
            for (const auto& offset : kOffsets) {
                const int x = centerX + offset[0];
                const int y = centerY + offset[1];
                const int dx = x - game.baseX;
                const int dy = y - game.baseY;
                const DistanceTier tier = dx * dx + dy * dy > 56 * 56
                    ? DistanceTier::Far : DistanceTier::Near;
                AddResourceCandidate(game, static_cast<ResourceZone>(zone), tier, x, y);
            }
        }
    }

    constexpr ResourceKind kKinds[3][10] = {
        {ResourceKind::Food, ResourceKind::Food, ResourceKind::Material,
         ResourceKind::Food, ResourceKind::Water, ResourceKind::Food,
         ResourceKind::Material, ResourceKind::Water, ResourceKind::Food,
         ResourceKind::Material},
        {ResourceKind::Water, ResourceKind::Water, ResourceKind::Material,
         ResourceKind::Water, ResourceKind::Food, ResourceKind::Water,
         ResourceKind::Material, ResourceKind::Food, ResourceKind::Water,
         ResourceKind::Material},
        {ResourceKind::Material, ResourceKind::Water, ResourceKind::Material,
         ResourceKind::Food, ResourceKind::Material, ResourceKind::Material,
         ResourceKind::Material, ResourceKind::Material, ResourceKind::Material,
         ResourceKind::Material},
    };
    constexpr int kSeedRanks[10] = {0, 2, 4, 6, 8, 11, 14, 17, 20, 21};
    for (int zone = 0; zone < 3; ++zone) {
        int placed = 0;
        int zoneRank = 0;
        for (int index = 0; index < game.resourceCandidateCount && placed < 40; ++index) {
            const ResourceCandidate& candidate = game.resourceCandidates[index];
            if (static_cast<int>(candidate.zone) != zone) continue;
            const int localPlaced = placed % 10;
            const int clusterRankBase = (placed / 10) * 30;
            if (zoneRank == clusterRankBase + kSeedRanks[localPlaced]) {
                AddZonedResource(game, static_cast<ResourceZone>(zone), kKinds[zone][localPlaced],
                                 candidate.x, candidate.y);
                ++placed;
            }
            ++zoneRank;
        }
    }
    FinalizeResourceLayout(game);

    for (int kind = 0; kind < 2; ++kind) {
        const int target = kind == 0 ? 80 : 64;
        int placed = 0;
        for (int tierPass = 0; tierPass < 2 && placed < target; ++tierPass) {
            for (int index = game.resourceCandidateCount - 1; index >= 0 && placed < target; --index) {
                const ResourceCandidate& candidate = game.resourceCandidates[index];
                if ((tierPass == 0) != (candidate.tier == DistanceTier::Far)) continue;
                if (AddTrap(game, kind == 0 ? TrapKind::Rustle : TrapKind::Snare,
                            candidate.x, candidate.y)) ++placed;
            }
        }
    }

    for (int index = 0; index < game.resourceCandidateCount; ++index) {
        const ResourceCandidate& candidate = game.resourceCandidates[index];
        if ((index % 2) == 0) {
            AddAnimalSpawnCandidate(game, AnimalKind::Moth, candidate.zone,
                                    candidate.x + 1, candidate.y + 2);
        } else if ((index % 3) == 0) {
            AddAnimalSpawnCandidate(game, AnimalKind::Boar, candidate.zone,
                                    candidate.x - 2, candidate.y + 1);
        }
        if (game.animalSpawnCandidateCount >= 192) break;
    }
}

}  // namespace lc
