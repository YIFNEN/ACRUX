#include "game_core.h"

#include <cstdlib>
#include <cmath>

namespace lc {

namespace {

constexpr int kPathBuildCost = 2;
constexpr int kPathRecoverUnits = 1;
constexpr int kPathCacheUnits = 20;
constexpr int kCampaignLorePoolSize = 6;
constexpr int kEpilogueLoreStart = 24;
constexpr int kEpilogueLoreCount = 12;
constexpr int kLoreCount = 36;

const char* const kLoreText[kLoreCount] = {
    "THE FIRST SHORT DAY WAS CALLED AN ANOMALY.",
    "BY THE TENTH, WE HAD STOPPED COUNTING.",
    "NIGHT GREW LONGER BEFORE THE MAPS COULD CHANGE.",
    "THE FIELDS WERE THE FIRST TO COUNT THE MISSING HOURS.",
    "EVERY WINDOW ONCE WAITED FOR MORNING.",
    "WE MEASURED DAYS BY WHAT THE SUN FAILED TO REACH.",
    "EVERY ROOFTOP ONCE FACED THE SUN.",
    "THE FIELDS MOVED UNDER GLASS, AND THE CITIES DIMMED TO FEED THEM.",
    "THE FIRST RELAYS WERE BUILT TO SAVE MINUTES, NOT NIGHTS.",
    "POWER LINES BECAME ROADS BEFORE ROADS BECAME SAFE.",
    "WE STORED LIGHT BEFORE WE LEARNED TO LIVE WITHOUT IT.",
    "SOME MACHINES ARE STILL WAITING FOR MORNING.",
    "THE SUN WAS RATIONED BEFORE THE BREAD.",
    "WE GUARDED THE BATTERIES. THEN WE GUARDED THEM FROM EACH OTHER.",
    "WHEN THE RELAYS FAILED, THE WILD FOUND THE DARK FIRST.",
    "MOTH SWARMS LEARNED TO FOLLOW CHARGE AS THEY ONCE FOLLOWED MOONLIGHT.",
    "BOARS STOPPED FEARING FENCES WHEN THE FENCES STOPPED HUMMING.",
    "A BROKEN LINE WAS CHEAPER, UNTIL NO ONE REMAINED TO REPAIR IT.",
    "THE GRID OUTLIVED THE PEOPLE WHO NAMED ITS STREETS.",
    "A CLOSED CIRCUIT WAS ONCE ONLY AN ENGINEERING TERM.",
    "THE SAFEST LIGHT IS THE LIGHT THAT FINDS ITS WAY BACK.",
    "WE ONCE BUILT OUTWARD. NOW THE LIGHT RETURNS.",
    "THE SOUTHERN CROSS WAS ALWAYS ABOVE US. WE FORGOT TO LOOK.",
    "MORNING IS STILL BRIEF. IT IS NO LONGER WASTED.",
    "THE MAP ENDS HERE. THE OLD SERVICE LINES DO NOT.",
    "ON CLEAR NIGHTS, A SECOND HUM REPEATS IN THE EAST.",
    "SOME SURVEY MARKERS POINT BEYOND EVERY ROAD WE RESTORED.",
    "THE NORTHERN BASIN STILL SENDS STATIC BEFORE DAWN.",
    "WEATHER TOWERS REPORT WINDS FROM PLACES NO ONE HERE HAS SEEN.",
    "THE COASTAL RELAYS WERE BUILT FOR A GRID WE NEVER REACHED.",
    "TRACKS SOMETIMES CROSS THE OLD BORDER AND DO NOT RETURN.",
    "SOMEWHERE PAST THE RIDGE, ANOTHER CIRCUIT MAY STILL BE CLOSED.",
    "THE OLD MAPS MARK SETTLEMENTS WHERE OUR MAP SHOWS ONLY DARK.",
    "MORNING DOES NOT END AT THE EDGE OF OUR GRID.",
    "ONE RESTORED LIGHT MAKES DISTANT DARKNESS EASIER TO NOTICE.",
    "IF ANOTHER RELAY IS STILL LISTENING, IT HAS NOT ANSWERED YET.",
};

const char* const kLoreTextKo[kLoreCount] = {
    u8"처음 짧아진 낮은 이상 현상이라 불렸다.",
    u8"열흘째에는 세기를 그만두었다.",
    u8"지도가 바뀌기도 전에 밤이 더 길어졌다.",
    u8"사라진 시간을 가장 먼저 헤아린 것은 들판이었다.",
    u8"모든 창문은 한때 아침을 기다렸다.",
    u8"우리는 햇빛이 닿지 못한 것으로 하루를 헤아렸다.",
    u8"모든 지붕은 한때 태양을 향했다.",
    u8"들판은 유리 아래로 옮겨졌고, 그것을 살리려 도시의 불이 꺼졌다.",
    u8"최초의 릴레이는 밤이 아니라 몇 분을 아끼기 위해 세워졌다.",
    u8"도로가 안전해지기 전에 전력선이 길이 되었다.",
    u8"빛 없이 사는 법을 배우기 전에 우리는 빛을 저장했다.",
    u8"어떤 기계들은 아직도 아침을 기다린다.",
    u8"빵보다 먼저 햇빛이 배급되었다.",
    u8"우리는 축전지를 지켰다. 이윽고 서로에게서 그것을 지켰다.",
    u8"릴레이가 멈추자 야생이 먼저 어둠을 차지했다.",
    u8"나방 떼는 달빛을 좇던 것처럼 전하를 좇는 법을 배웠다.",
    u8"울타리의 울림이 멎자 멧돼지는 더는 울타리를 두려워하지 않았다.",
    u8"끊어진 선로는 더 저렴했다. 수리할 사람이 아무도 남지 않을 때까지.",
    u8"전력망은 그 거리에 이름을 붙인 사람들보다 오래 남았다.",
    u8"폐회로는 한때 그저 공학 용어였다.",
    u8"가장 안전한 빛은 돌아오는 길을 찾는 빛이다.",
    u8"우리는 한때 바깥으로 지었다. 이제 빛이 돌아온다.",
    u8"남십자성은 언제나 위에 있었다. 우리가 올려다보는 것을 잊었을 뿐.",
    u8"아침은 여전히 짧다. 이제는 낭비되지 않는다.",
    u8"지도는 여기서 끝나지만 오래된 관리선은 그렇지 않다.",
    u8"맑은 밤이면 동쪽에서 또 하나의 울림이 반복된다.",
    u8"어떤 측량 표지는 우리가 복구한 모든 길 너머를 가리킨다.",
    u8"북쪽 분지는 아직도 새벽 전에 잡음을 보낸다.",
    u8"기상탑은 이곳의 누구도 본 적 없는 곳에서 불어오는 바람을 보고한다.",
    u8"해안 릴레이는 우리가 끝내 닿지 못한 전력망을 위해 세워졌다.",
    u8"선로는 때때로 옛 경계를 넘어가 돌아오지 않는다.",
    u8"산등성이 너머 어딘가에서 또 하나의 회로가 아직 닫혀 있을지 모른다.",
    u8"옛 지도는 우리 지도에 어둠만 있는 곳에 정착지를 표시한다.",
    u8"아침은 우리 전력망의 끝에서 끝나지 않는다.",
    u8"복구된 빛 하나는 먼 어둠을 더 잘 보이게 한다.",
    u8"다른 릴레이가 아직 듣고 있다면, 아직 응답하지 않았다.",
};

bool IsInside(const GameState& game, int x, int y) {
    return x >= 0 && y >= 0 && x < game.width && y < game.height;
}

int IndexOf(const GameState& game, int x, int y) {
    return y * game.width + x;
}

float Minimum(float left, float right) {
    return left < right ? left : right;
}

float Maximum(float left, float right) {
    return left > right ? left : right;
}

void AdvanceDay(GameState& game, FailureReason failure) {
    const bool failed = failure != FailureReason::None;
    const bool unresolvedFoodCrisis = game.foodCrisis && game.food <= 0;
    const bool unresolvedWaterCrisis = game.waterCrisis && game.water <= 0;
    for (int index = 0; index < kResourceKinds; ++index) {
        if (failed) {
            game.carried[index] = 0;
        }
    }
    game.food = game.food > 0 ? game.food - 1 : 0;
    game.water = game.water > 0 ? game.water - 1 : 0;
    ++game.day;
    game.nextNightPenalty = failed;
    game.lastFailure = failure;
    game.removedPathUndoValid = false;
    game.foodCrisis = game.food <= 0;
    game.waterCrisis = game.water <= 0;
    game.phase = (unresolvedFoodCrisis || unresolvedWaterCrisis)
        ? GamePhase::GameOver : GamePhase::Day;
    if (game.phase == GamePhase::GameOver && failure == FailureReason::None) {
        game.lastFailure = FailureReason::Survival;
    }
}

void DamageOuterPoweredPath(GameState& game) {
    int chosen = -1;
    int farthest = -1;
    for (int y = 0; y < game.height; ++y) {
        for (int x = 0; x < game.width; ++x) {
            const int index = IndexOf(game, x, y);
            if (!game.powered[index] || IsStabilizedPath(game, x, y)) {
                continue;
            }
            const int distance = std::abs(x - game.baseX) + std::abs(y - game.baseY);
            if (distance > farthest) {
                farthest = distance;
                chosen = index;
            }
        }
    }
    if (chosen >= 0) {
        game.paths[chosen] = PathState::Broken;
        RecomputePower(game);
    }
}

int SleepDamageCount(const GameState& game) {
    if (game.relaysPowered <= 0) return 1;
    if (game.relaysPowered == 1) return (game.randomState & 2u) ? 2 : 1;
    return (game.randomState & 2u) ? 1 : 2;
}

void FloodPower(GameState& game, bool chargeUncharged) {
    for (int index = 0; index < game.width * game.height; ++index) {
        game.powered[index] = false;
    }
    if (!IsInside(game, game.baseX, game.baseY)) {
        return;
    }

    int queue[kMaxTiles] = {};
    int read = 0;
    int write = 0;
    bool visited[kMaxTiles] = {};
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};

    const TileRect baseRect = GetBaseFootprintRect(game);
    for (int side = 0; side < 4; ++side) {
        const bool horizontal = side < 2;
        const int fixed = side == 0 ? baseRect.y - 1 :
            side == 1 ? baseRect.y + baseRect.height :
            side == 2 ? baseRect.x - 1 : baseRect.x + baseRect.width;
        const int start = horizontal ? baseRect.x : baseRect.y;
        const int end = start + (horizontal ? baseRect.width : baseRect.height);
        for (int offset = start; offset < end; ++offset) {
            const int x = horizontal ? offset : fixed;
            const int y = horizontal ? fixed : offset;
            if (!IsInside(game, x, y)) continue;
            const int index = IndexOf(game, x, y);
            const PathState state = game.paths[index];
            const bool passable = state == PathState::Charged ||
                (chargeUncharged && state == PathState::Uncharged);
            if (passable && !visited[index]) {
                visited[index] = true;
                queue[write++] = index;
            }
        }
    }

    while (read < write) {
        const int index = queue[read++];
        const int x = index % game.width;
        const int y = index / game.width;
        if (chargeUncharged && game.paths[index] == PathState::Uncharged) {
            game.paths[index] = PathState::Charged;
        }
        game.powered[index] = game.paths[index] == PathState::Charged;

        for (int direction = 0; direction < 4; ++direction) {
            const int nextX = x + kDx[direction];
            const int nextY = y + kDy[direction];
            if (!IsInside(game, nextX, nextY)) {
                continue;
            }
            const int next = IndexOf(game, nextX, nextY);
            const PathState state = game.paths[next];
            const bool passable = state == PathState::Charged || (chargeUncharged && state == PathState::Uncharged);
            if (passable && !visited[next]) {
                visited[next] = true;
                queue[write++] = next;
            }
        }
    }
}

void RecomputeLoopLighting(GameState& game) {
    bool outside[kMaxTiles] = {};
    int queue[kMaxTiles] = {};
    int read = 0;
    int write = 0;
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};

    const auto enqueue = [&](int x, int y) {
        if (!IsInside(game, x, y)) return;
        const int index = IndexOf(game, x, y);
        if (outside[index] || game.powered[index]) return;
        outside[index] = true;
        queue[write++] = index;
    };
    for (int x = 0; x < game.width; ++x) {
        enqueue(x, 0);
        enqueue(x, game.height - 1);
    }
    for (int y = 1; y + 1 < game.height; ++y) {
        enqueue(0, y);
        enqueue(game.width - 1, y);
    }
    while (read < write) {
        const int index = queue[read++];
        const int x = index % game.width;
        const int y = index / game.width;
        for (int direction = 0; direction < 4; ++direction) {
            enqueue(x + kDx[direction], y + kDy[direction]);
        }
    }
    for (int index = 0; index < game.width * game.height; ++index) {
        game.loopLit[index] = !game.powered[index] && !outside[index];
    }
}

bool AnimalPositionAllowed(const GameState& game, float x, float y) {
    const int tileX = static_cast<int>(x);
    const int tileY = static_cast<int>(y);
    if (!IsInside(game, tileX, tileY)) return false;
    if (IsBaseFootprintTile(game, tileX, tileY)) return false;
    if (IsBasePuddleTile(game, tileX, tileY)) return false;
    if (IsRelayFootprintTile(game, tileX, tileY)) return false;
    const int tile = IndexOf(game, tileX, tileY);
    return game.terrain[tile] == Terrain::Ground && !game.powered[tile] && !game.loopLit[tile];
}

void MoveAnimal(GameState& game, Animal& animal, float vx, float vy, float seconds) {
    const float nextX = animal.x + vx * seconds;
    const float nextY = animal.y + vy * seconds;
    if (AnimalPositionAllowed(game, nextX, animal.y)) animal.x = nextX;
    if (AnimalPositionAllowed(game, animal.x, nextY)) animal.y = nextY;
}

bool MoveAnimalStraight(GameState& game, Animal& animal, float vx, float vy, float seconds) {
    const float nextX = animal.x + vx * seconds;
    const float nextY = animal.y + vy * seconds;
    if (!AnimalPositionAllowed(game, nextX, nextY)) return false;
    animal.x = nextX;
    animal.y = nextY;
    return true;
}

bool SegmentTouchesCircle(float startX, float startY, float endX, float endY,
                          float centerX, float centerY, float radius) {
    const float segmentX = endX - startX;
    const float segmentY = endY - startY;
    const float lengthSquared = segmentX * segmentX + segmentY * segmentY;
    float t = 0.0f;
    if (lengthSquared > 0.000001f) {
        t = ((centerX - startX) * segmentX + (centerY - startY) * segmentY) / lengthSquared;
        t = Minimum(1.0f, Maximum(0.0f, t));
    }
    const float dx = startX + segmentX * t - centerX;
    const float dy = startY + segmentY * t - centerY;
    return dx * dx + dy * dy <= radius * radius;
}

bool MoveBoarCharge(GameState& game, Animal& animal, float seconds, AnimalEvents& events) {
    const float startX = animal.x;
    const float startY = animal.y;
    const float targetX = startX + animal.vx * seconds;
    const float targetY = startY + animal.vy * seconds;
    const float distance = std::sqrt((targetX - startX) * (targetX - startX) +
                                     (targetY - startY) * (targetY - startY));
    const int steps = static_cast<int>(distance / 0.2f) + 1;
    float lastX = startX;
    float lastY = startY;
    for (int step = 1; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const float x = startX + (targetX - startX) * t;
        const float y = startY + (targetY - startY) * t;
        const int tileX = static_cast<int>(x);
        const int tileY = static_cast<int>(y);
        if (!IsInside(game, tileX, tileY)) return false;
        const int tile = IndexOf(game, tileX, tileY);
        if (game.powered[tile]) {
            if (IsStabilizedPath(game, tileX, tileY)) {
                animal.x = lastX;
                animal.y = lastY;
                animal.timer = 7.0f;
                return false;
            }
            DamagePath(game, tileX, tileY);
            events.pathBroken = true;
            events.pathBreakX = tileX;
            events.pathBreakY = tileY;
            animal.x = lastX;
            animal.y = lastY;
            return false;
        }
        const Terrain terrain = game.terrain[tile];
        if (terrain == Terrain::Rock || terrain == Terrain::Bush) {
            animal.x = lastX;
            animal.y = lastY;
            return false;
        }
        lastX = x;
        lastY = y;
    }
    animal.x = targetX;
    animal.y = targetY;
    return true;
}

void BeginRetreat(Animal& animal) {
    animal.mode = AnimalMode::Retreat;
    animal.timer = 2.0f;
    animal.hasAfterimageTarget = false;
}

}  // namespace

void InitializeForTests(GameState& game, int width, int height) {
    game = {};
    game.width = width;
    game.height = height;
    game.baseX = -1;
    game.baseY = -1;
    game.food = 6;
    game.water = 6;
    game.day = 1;
    game.personalLightMax = 100.0f;
    game.baseEnergyMax = 240.0f;
    game.phase = GamePhase::Day;
}

void SetBase(GameState& game, int x, int y) {
    if (!IsInside(game, x, y)) {
        return;
    }
    game.baseX = x;
    game.baseY = y;
    game.terrain[IndexOf(game, x, y)] = Terrain::Base;
}

TileRect GetBaseFootprintRect(const GameState& game) {
    TileRect rect = {0, 0, 0, 0};
    if (!IsInside(game, game.baseX, game.baseY)) {
        return rect;
    }
    rect.x = game.baseX - 1;
    rect.y = game.baseY - 1;
    rect.width = 2;
    rect.height = 2;
    if (rect.x < 0) {
        rect.width += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.height += rect.y;
        rect.y = 0;
    }
    if (rect.x + rect.width > game.width) rect.width = game.width - rect.x;
    if (rect.y + rect.height > game.height) rect.height = game.height - rect.y;
    return rect;
}

bool IsBaseFootprintTile(const GameState& game, int x, int y) {
    const TileRect rect = GetBaseFootprintRect(game);
    return rect.width > 0 && rect.height > 0 &&
        x >= rect.x && y >= rect.y &&
        x < rect.x + rect.width && y < rect.y + rect.height;
}

void SetRelay(GameState& game, int relayIndex, int x, int y) {
    if (relayIndex < 0 || relayIndex >= 3 || !IsInside(game, x, y)) {
        return;
    }
    game.relayX[relayIndex] = x;
    game.relayY[relayIndex] = y;
    if (game.relayCount <= relayIndex) {
        game.relayCount = relayIndex + 1;
    }
    game.terrain[IndexOf(game, x, y)] = Terrain::Relay;
}

TileRect GetRelayFootprintRect(const GameState& game, int relayIndex) {
    TileRect rect = {0, 0, 0, 0};
    if (relayIndex < 0 || relayIndex >= game.relayCount ||
        !IsInside(game, game.relayX[relayIndex], game.relayY[relayIndex])) {
        return rect;
    }
    rect.x = game.relayX[relayIndex];
    rect.y = game.relayY[relayIndex];
    rect.width = 2;
    rect.height = 2;
    if (rect.x > game.width - rect.width) rect.x = game.width - rect.width;
    if (rect.y > game.height - rect.height) rect.y = game.height - rect.height;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    return rect;
}

bool IsRelayFootprintTile(const GameState& game, int x, int y) {
    for (int relay = 0; relay < game.relayCount; ++relay) {
        const TileRect rect = GetRelayFootprintRect(game, relay);
        if (rect.width > 0 && rect.height > 0 &&
            x >= rect.x && y >= rect.y &&
            x < rect.x + rect.width && y < rect.y + rect.height) {
            return true;
        }
    }
    return false;
}

TileRect GetBasePuddleRect(const GameState& game) {
    TileRect rect = {0, 0, 0, 0};
    if (!IsInside(game, game.baseX, game.baseY) || game.width < 32 || game.height < 32) {
        return rect;
    }
    rect.width = 2;
    rect.height = 2;
    rect.x = game.baseX + 4;
    rect.y = game.baseY + 2;
    if (rect.x > game.width - rect.width) rect.x = game.width - rect.width;
    if (rect.y > game.height - rect.height) rect.y = game.height - rect.height;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    return rect;
}

bool IsBasePuddleTile(const GameState& game, int x, int y) {
    const TileRect rect = GetBasePuddleRect(game);
    return rect.width > 0 && rect.height > 0 &&
        x >= rect.x && y >= rect.y &&
        x < rect.x + rect.width && y < rect.y + rect.height;
}

bool CanPlacePath(const GameState& game, int x, int y) {
    if (x < 0 || y < 0 || x >= game.width || y >= game.height) {
        return false;
    }
    if (game.pathInventory < kPathBuildCost) {
        return false;
    }
    if (IsBaseFootprintTile(game, x, y) || IsBasePuddleTile(game, x, y)) {
        return false;
    }
    if (IsRelayFootprintTile(game, x, y)) {
        return false;
    }
    const int index = y * game.width + x;
    return game.terrain[index] == Terrain::Ground && game.paths[index] == PathState::Empty;
}

bool PlacePath(GameState& game, int x, int y) {
    if (!CanPlacePath(game, x, y)) {
        return false;
    }
    const bool restoreCharge = game.removedPathUndoValid && game.removedPathWasCharged &&
        game.phase == GamePhase::Night && game.removedPathDay == game.day &&
        game.removedPathX == x && game.removedPathY == y;
    game.paths[IndexOf(game, x, y)] = restoreCharge ? PathState::Charged : PathState::Uncharged;
    game.pathInventory = static_cast<std::uint16_t>(game.pathInventory - kPathBuildCost);
    game.removedPathUndoValid = false;
    if (restoreCharge) RecomputePower(game);
    return true;
}

bool RemovePath(GameState& game, int x, int y) {
    if (!IsInside(game, x, y)) {
        return false;
    }
    const int index = IndexOf(game, x, y);
    if (game.paths[index] == PathState::Empty || game.paths[index] == PathState::Broken) {
        return false;
    }
    game.removedPathX = x;
    game.removedPathY = y;
    game.removedPathDay = game.day;
    game.removedPathWasCharged = game.paths[index] == PathState::Charged;
    game.removedPathUndoValid = game.phase == GamePhase::Night && game.removedPathWasCharged;
    game.paths[index] = PathState::Empty;
    game.pathInventory = static_cast<std::uint16_t>(game.pathInventory + kPathBuildCost);
    RecomputePower(game);
    return true;
}

bool DamagePath(GameState& game, int x, int y) {
    if (!IsInside(game, x, y)) {
        return false;
    }
    const int index = IndexOf(game, x, y);
    if (game.paths[index] == PathState::Empty || game.paths[index] == PathState::Broken) {
        return false;
    }
    game.paths[index] = PathState::Broken;
    RecomputePower(game);
    return true;
}

bool RepairPath(GameState& game, int x, int y) {
    if (!IsInside(game, x, y) || game.pathInventory < kPathBuildCost) {
        return false;
    }
    const int index = IndexOf(game, x, y);
    if (game.paths[index] != PathState::Broken) {
        return false;
    }
    game.paths[index] = PathState::Charged;
    game.pathInventory = static_cast<std::uint16_t>(game.pathInventory - kPathBuildCost);
    RecomputePower(game);
    return true;
}

bool RecoverBrokenPath(GameState& game, int x, int y) {
    if (!IsInside(game, x, y)) {
        return false;
    }
    const int index = IndexOf(game, x, y);
    if (game.paths[index] != PathState::Broken) {
        return false;
    }
    game.paths[index] = PathState::Empty;
    game.pathInventory = static_cast<std::uint16_t>(game.pathInventory + kPathRecoverUnits);
    RecomputePower(game);
    return true;
}

void ChargeConnectedPaths(GameState& game) {
    FloodPower(game, true);
    RecomputeLoopLighting(game);
}

void RecomputePower(GameState& game) {
    FloodPower(game, false);
    RecomputeLoopLighting(game);
}

bool IsPowered(const GameState& game, int x, int y) {
    return IsInside(game, x, y) && game.powered[IndexOf(game, x, y)];
}

bool IsLoopLit(const GameState& game, int x, int y) {
    return IsInside(game, x, y) && game.loopLit[IndexOf(game, x, y)];
}

bool IsStabilizedPath(const GameState& game, int x, int y) {
    if (!IsInside(game, x, y)) return false;
    const int blocked = IndexOf(game, x, y);
    if (!game.powered[blocked]) return false;

    bool hasLoopInterior = false;
    for (int index = 0; index < game.width * game.height; ++index) {
        if (game.loopLit[index]) {
            hasLoopInterior = true;
            break;
        }
    }
    if (!hasLoopInterior) return false;

    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};
    int neighbors[4] = {};
    int neighborCount = 0;
    for (int direction = 0; direction < 4; ++direction) {
        const int nx = x + kDx[direction];
        const int ny = y + kDy[direction];
        if (!IsInside(game, nx, ny)) continue;
        const int next = IndexOf(game, nx, ny);
        if (game.powered[next]) neighbors[neighborCount++] = next;
    }
    if (neighborCount < 2) return false;

    for (int pairA = 0; pairA < neighborCount; ++pairA) {
        for (int pairB = pairA + 1; pairB < neighborCount; ++pairB) {
            bool visited[kMaxTiles] = {};
            int queue[kMaxTiles] = {};
            int read = 0;
            int write = 0;
            visited[blocked] = true;
            visited[neighbors[pairA]] = true;
            queue[write++] = neighbors[pairA];
            while (read < write) {
                const int index = queue[read++];
                if (index == neighbors[pairB]) return true;
                const int cx = index % game.width;
                const int cy = index / game.width;
                for (int direction = 0; direction < 4; ++direction) {
                    const int nx = cx + kDx[direction];
                    const int ny = cy + kDy[direction];
                    if (!IsInside(game, nx, ny)) continue;
                    const int next = IndexOf(game, nx, ny);
                    if (!visited[next] && game.powered[next]) {
                        visited[next] = true;
                        queue[write++] = next;
                    }
                }
            }
        }
    }
    return false;
}

bool IsChargeNode(const GameState& game, int x, int y) {
    if (!IsInside(game, x, y)) return false;
    if (IsBaseFootprintTile(game, x, y)) return true;
    for (int relay = 0; relay < game.relayCount; ++relay) {
        const TileRect rect = GetRelayFootprintRect(game, relay);
        if (x >= rect.x && y >= rect.y &&
            x < rect.x + rect.width && y < rect.y + rect.height) {
            return game.relayPowered[relay];
        }
    }
    return IsPowered(game, x, y);
}

Season CurrentSeason(const GameState& game) {
    if (!game.completed) return Season::None;
    int daysSinceCompletion = game.day - game.completionDay;
    if (daysSinceCompletion < 0) daysSinceCompletion = 0;
    switch ((daysSinceCompletion / 10) % 4) {
        case 0: return Season::Winter;
        case 1: return Season::Spring;
        case 2: return Season::Summer;
        default: return Season::Autumn;
    }
}

Language NextLanguage(Language language) {
    return language == Language::English ? Language::Korean : Language::English;
}

void MarkLoreSeen(GameState& game, int loreIndex) {
    if (loreIndex < 0 || loreIndex >= 64) return;
    game.loreSeenMask |= (1ull << loreIndex);
}

const char* LoreText(int loreIndex) {
    if (loreIndex < 0 || loreIndex >= kLoreCount) return "";
    return kLoreText[loreIndex];
}

const char* LoreText(int loreIndex, Language language) {
    if (loreIndex < 0 || loreIndex >= kLoreCount) return "";
    return language == Language::Korean ? kLoreTextKo[loreIndex] : kLoreText[loreIndex];
}

int ChooseNextLore(GameState& game) {
    int start = 0;
    int count = kCampaignLorePoolSize;
    if (game.completed) {
        start = kEpilogueLoreStart;
        count = kEpilogueLoreCount;
    } else {
        int relayPool = game.relaysPowered;
        if (relayPool < 0) relayPool = 0;
        if (relayPool > 3) relayPool = 3;
        start = relayPool * kCampaignLorePoolSize;
    }

    std::uint64_t poolMask = 0;
    for (int offset = 0; offset < count; ++offset) {
        poolMask |= (1ull << (start + offset));
    }
    if ((game.loreSeenMask & poolMask) == poolMask) {
        game.loreSeenMask &= ~poolMask;
    }

    game.randomState = game.randomState * 1664525u + 1013904223u;
    const int first = static_cast<int>(game.randomState % static_cast<std::uint32_t>(count));
    for (int offset = 0; offset < count; ++offset) {
        const int local = (first + offset) % count;
        const int loreIndex = start + local;
        if ((game.loreSeenMask & (1ull << loreIndex)) == 0) return loreIndex;
    }
    return start;
}

void BeginNight(GameState& game) {
    RefreshResourcesForNight(game);
    ChargeConnectedPaths(game);
    RefreshRelayWiring(game);
    ResetAnimalsForNight(game, static_cast<float>(game.baseX) + 0.5f,
                         static_cast<float>(game.baseY) + 0.5f);
    ResetTrapsForNight(game);
    game.baseEnergyMax = 240.0f + 60.0f * static_cast<float>(game.relaysPowered);
    game.personalLight = game.personalLightMax;
    const float penalty = game.nextNightPenalty ? 0.8f : 1.0f;
    game.baseEnergy = game.baseEnergyMax * penalty;
    game.nextNightPenalty = false;
    game.lastFailure = FailureReason::None;
    game.blinkCooldown = 0.0f;
    game.afterimageTime = 0.0f;
    game.snareTimer = 0.0f;
    game.phase = GamePhase::Night;
}

void StepNight(GameState& game, float seconds, bool onPoweredPath, bool wantsCharge) {
    if (game.phase != GamePhase::Night || seconds <= 0.0f) {
        return;
    }

    game.blinkCooldown = Maximum(0.0f, game.blinkCooldown - seconds);
    game.afterimageTime = Maximum(0.0f, game.afterimageTime - seconds);
    game.personalLight -= seconds;

    if (onPoweredPath && wantsCharge && game.personalLight < game.personalLightMax) {
        const float desired = 15.0f * seconds;
        const float missing = game.personalLightMax - game.personalLight;
        const float transfer = Minimum(Minimum(desired, missing), Maximum(0.0f, game.baseEnergy));
        game.personalLight += transfer;
        game.baseEnergy -= transfer;
    }

    game.personalLight = Maximum(0.0f, game.personalLight);
    game.baseEnergy = Maximum(0.0f, game.baseEnergy);
    if (game.personalLight <= 0.0f) {
        AdvanceDay(game, FailureReason::PersonalLight);
    } else if (game.baseEnergy <= 0.0f) {
        AdvanceDay(game, FailureReason::BaseEnergy);
    }
}

bool UseBlink(GameState& game) {
    constexpr float kBlinkCost = 22.0f;
    if (game.phase != GamePhase::Night || game.blinkCooldown > 0.0f || game.personalLight < kBlinkCost) {
        return false;
    }
    game.personalLight -= kBlinkCost;
    game.blinkCooldown = game.waterCrisis ? 20.0f : 12.0f;
    game.afterimageTime = 1.0f;
    return true;
}

void SleepAtBase(GameState& game) {
    if (game.phase != GamePhase::Night) {
        return;
    }
    RefreshRelayWiring(game);
    if (!game.completed && game.relaysConnected >= 3) {
        ChargeConnectedPaths(game);
        RefreshRelayWiring(game);
        game.phase = GamePhase::Ending;
        return;
    }
    if (game.relaysPowered >= 0) {
        game.randomState = game.randomState * 1664525u + 1013904223u;
        if ((game.randomState & 1u) != 0u) {
            const int damageCount = SleepDamageCount(game);
            for (int index = 0; index < damageCount; ++index) {
                DamageOuterPoweredPath(game);
            }
        }
        RefreshRelayWiring(game);
    }
    AdvanceDay(game, FailureReason::None);
}

bool CreateSaveData(const GameState& game, SaveData& save) {
    if (game.width <= 0 || game.height <= 0 || game.width > kMapSide || game.height > kMapSide) {
        return false;
    }
    save = {};
    save.version = kSaveVersion;
    save.width = game.width;
    save.height = game.height;
    save.baseX = game.baseX;
    save.baseY = game.baseY;
    save.pathInventory = game.pathInventory;
    save.completed = game.completed;
    save.completionDay = game.completionDay;
    save.loreSeenMask = game.loreSeenMask;
    for (int index = 0; index < kResourceKinds; ++index) {
        save.carried[index] = game.carried[index];
        save.stored[index] = index == static_cast<int>(ResourceKind::Material) ? 0 : game.stored[index];
    }
    save.food = game.food;
    save.water = game.water;
    save.day = game.day;
    save.relayCount = game.relayCount;
    for (int index = 0; index < 3; ++index) {
        save.relayX[index] = game.relayX[index];
        save.relayY[index] = game.relayY[index];
    }
    save.resourceCount = game.resourceCount;
    save.resourceCandidateCount = game.resourceCandidateCount;
    save.lastResourceRefreshDay = game.lastResourceRefreshDay;
    save.trapCount = game.trapCount;
    save.randomState = game.randomState;
    save.nextNightPenalty = game.nextNightPenalty;
    save.foodCrisis = game.foodCrisis;
    save.waterCrisis = game.waterCrisis;
    for (int zone = 0; zone < 3; ++zone) {
        for (int kind = 0; kind < kResourceKinds; ++kind) {
            save.resourceCaps[zone][kind] = game.resourceCaps[zone][kind];
        }
    }
    for (int index = 0; index < kMaxTiles; ++index) {
        save.terrain[index] = game.terrain[index];
        save.paths[index] = game.paths[index];
    }
    for (int index = 0; index < kMaxResources; ++index) save.resources[index] = game.resources[index];
    for (int index = 0; index < kMaxResourceCandidates; ++index) {
        save.resourceCandidates[index] = game.resourceCandidates[index];
    }
    save.animalSpawnCandidateCount = game.animalSpawnCandidateCount;
    for (int index = 0; index < kMaxAnimalSpawnCandidates; ++index) {
        save.animalSpawnCandidates[index] = game.animalSpawnCandidates[index];
    }
    for (int index = 0; index < kMaxTraps; ++index) save.traps[index] = game.traps[index];
    return true;
}

bool RestoreSaveData(GameState& game, const SaveData& save) {
    if (save.version != kSaveVersion ||
        save.width <= 0 || save.height <= 0 ||
        save.width > kMapSide || save.height > kMapSide ||
        save.baseX < 0 || save.baseY < 0 ||
        save.baseX >= save.width || save.baseY >= save.height ||
        save.resourceCount < 0 || save.resourceCount > kMaxResources ||
        save.resourceCandidateCount < 0 || save.resourceCandidateCount > kMaxResourceCandidates ||
        save.animalSpawnCandidateCount < 0 ||
        save.animalSpawnCandidateCount > kMaxAnimalSpawnCandidates ||
        save.trapCount < 0 || save.trapCount > kMaxTraps ||
        save.relayCount < 0 || save.relayCount > 3) {
        return false;
    }
    for (int relay = 0; relay < save.relayCount; ++relay) {
        if (save.relayX[relay] < 0 || save.relayY[relay] < 0 ||
            save.relayX[relay] >= save.width || save.relayY[relay] >= save.height) {
            return false;
        }
    }
    game = {};
    game.width = save.width;
    game.height = save.height;
    game.baseX = save.baseX;
    game.baseY = save.baseY;
    game.pathInventory = save.pathInventory;
    game.completed = save.completed;
    game.completionDay = save.completionDay;
    game.loreSeenMask = save.loreSeenMask;
    for (int index = 0; index < kResourceKinds; ++index) {
        game.carried[index] = save.carried[index];
        game.stored[index] = index == static_cast<int>(ResourceKind::Material) ? 0 : save.stored[index];
    }
    game.food = save.food;
    game.water = save.water;
    game.day = save.day;
    game.relayCount = save.relayCount;
    for (int index = 0; index < 3; ++index) {
        game.relayX[index] = save.relayX[index];
        game.relayY[index] = save.relayY[index];
    }
    game.resourceCount = save.resourceCount;
    game.resourceCandidateCount = save.resourceCandidateCount;
    game.lastResourceRefreshDay = save.lastResourceRefreshDay;
    game.trapCount = save.trapCount;
    game.randomState = save.randomState;
    game.nextNightPenalty = save.nextNightPenalty;
    game.foodCrisis = save.foodCrisis;
    game.waterCrisis = save.waterCrisis;
    game.personalLightMax = 100.0f;
    game.baseEnergyMax = 240.0f;
    game.phase = GamePhase::Day;
    for (int zone = 0; zone < 3; ++zone) {
        for (int kind = 0; kind < kResourceKinds; ++kind) {
            game.resourceCaps[zone][kind] = save.resourceCaps[zone][kind];
        }
    }
    for (int index = 0; index < kMaxTiles; ++index) {
        game.terrain[index] = save.terrain[index];
        game.paths[index] = save.paths[index];
    }
    for (int index = 0; index < kMaxResources; ++index) game.resources[index] = save.resources[index];
    for (int index = 0; index < kMaxResourceCandidates; ++index) {
        game.resourceCandidates[index] = save.resourceCandidates[index];
    }
    game.animalSpawnCandidateCount = save.animalSpawnCandidateCount;
    for (int index = 0; index < kMaxAnimalSpawnCandidates; ++index) {
        game.animalSpawnCandidates[index] = save.animalSpawnCandidates[index];
    }
    for (int index = 0; index < kMaxTraps; ++index) game.traps[index] = save.traps[index];
    RecomputePower(game);
    RefreshRelayWiring(game);
    return true;
}

void RefreshRelayWiring(GameState& game) {
    bool wired[kMaxTiles] = {};
    int queue[kMaxTiles] = {};
    int read = 0;
    int write = 0;
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};

    if (IsInside(game, game.baseX, game.baseY)) {
        for (int direction = 0; direction < 4; ++direction) {
            const int x = game.baseX + kDx[direction];
            const int y = game.baseY + kDy[direction];
            if (!IsInside(game, x, y)) {
                continue;
            }
            const int index = IndexOf(game, x, y);
            const PathState state = game.paths[index];
            if (state != PathState::Empty && state != PathState::Broken && !wired[index]) {
                wired[index] = true;
                queue[write++] = index;
            }
        }
    }

    while (read < write) {
        const int index = queue[read++];
        const int x = index % game.width;
        const int y = index / game.width;
        for (int direction = 0; direction < 4; ++direction) {
            const int nextX = x + kDx[direction];
            const int nextY = y + kDy[direction];
            if (!IsInside(game, nextX, nextY)) {
                continue;
            }
            const int next = IndexOf(game, nextX, nextY);
            const PathState state = game.paths[next];
            if (state != PathState::Empty && state != PathState::Broken && !wired[next]) {
                wired[next] = true;
                queue[write++] = next;
            }
        }
    }

    game.relaysConnected = 0;
    game.relaysPowered = 0;
    for (int relay = 0; relay < game.relayCount; ++relay) {
        bool connected = false;
        bool powered = false;
        const TileRect rect = GetRelayFootprintRect(game, relay);
        for (int side = 0; side < 4; ++side) {
            const bool horizontal = side < 2;
            const int fixed = side == 0 ? rect.y - 1 :
                side == 1 ? rect.y + rect.height : side == 2 ? rect.x - 1 : rect.x + rect.width;
            const int start = horizontal ? rect.x : rect.y;
            const int end = start + (horizontal ? rect.width : rect.height);
            for (int offset = start; offset < end; ++offset) {
                const int x = horizontal ? offset : fixed;
                const int y = horizontal ? fixed : offset;
                if (!IsInside(game, x, y)) continue;
                const int index = IndexOf(game, x, y);
                connected = connected || wired[index];
                powered = powered || game.powered[index];
            }
        }
        game.relayConnected[relay] = connected;
        game.relayPowered[relay] = powered;
        if (connected) {
            ++game.relaysConnected;
        }
        if (powered) {
            ++game.relaysPowered;
        }
    }
}

bool AddResource(GameState& game, ResourceKind kind, int x, int y) {
    ResourceZone zone = ResourceZone::Path;
    if (kind == ResourceKind::Food) zone = ResourceZone::Food;
    else if (kind == ResourceKind::Water) zone = ResourceZone::Water;
    return AddZonedResource(game, zone, kind, x, y);
}

bool AddZonedResource(GameState& game, ResourceZone zone, ResourceKind kind, int x, int y) {
    if (!IsInside(game, x, y)) {
        return false;
    }
    if (IsBaseFootprintTile(game, x, y)) {
        return false;
    }
    if (IsRelayFootprintTile(game, x, y)) {
        return false;
    }
    int slot = -1;
    for (int index = 0; index < game.resourceCount; ++index) {
        if (!game.resources[index].active) {
            slot = index;
            break;
        }
    }
    if (slot < 0) {
        if (game.resourceCount >= kMaxResources) return false;
        slot = game.resourceCount++;
    }
    ResourceNode& resource = game.resources[slot];
    resource.x = x;
    resource.y = y;
    resource.kind = kind;
    resource.zone = zone;
    resource.active = true;
    return true;
}

bool AddResourceCandidate(GameState& game, ResourceZone zone, DistanceTier tier, int x, int y) {
    if (game.resourceCandidateCount >= kMaxResourceCandidates || !IsInside(game, x, y) ||
        IsBaseFootprintTile(game, x, y) || IsBasePuddleTile(game, x, y) ||
        IsRelayFootprintTile(game, x, y) ||
        game.terrain[IndexOf(game, x, y)] != Terrain::Ground) {
        return false;
    }
    ResourceCandidate& candidate = game.resourceCandidates[game.resourceCandidateCount++];
    candidate.x = x;
    candidate.y = y;
    candidate.zone = zone;
    candidate.tier = tier;
    return true;
}

void FinalizeResourceLayout(GameState& game) {
    for (int zone = 0; zone < 3; ++zone) {
        for (int kind = 0; kind < kResourceKinds; ++kind) game.resourceCaps[zone][kind] = 0;
    }
    for (int index = 0; index < game.resourceCount; ++index) {
        const ResourceNode& resource = game.resources[index];
        if (resource.active) {
            ++game.resourceCaps[static_cast<int>(resource.zone)][static_cast<int>(resource.kind)];
        }
    }
    game.lastResourceRefreshDay = 0;
}

void RefreshResourcesForNight(GameState& game) {
    if (game.lastResourceRefreshDay == game.day) return;
    if (game.lastResourceRefreshDay == 0 && game.day == 1) {
        game.lastResourceRefreshDay = 1;
        return;
    }

    bool occupied[kMaxTiles] = {};
    int oldX[kMaxResources] = {};
    int oldY[kMaxResources] = {};
    ResourceKind oldKind[kMaxResources] = {};
    ResourceZone oldZone[kMaxResources] = {};
    for (int index = 0; index < game.resourceCount; ++index) {
        oldX[index] = game.resources[index].x;
        oldY[index] = game.resources[index].y;
        oldKind[index] = game.resources[index].kind;
        oldZone[index] = game.resources[index].zone;
    }

    const auto candidateUsable = [&](const ResourceCandidate& candidate, int avoidX, int avoidY) {
        if (candidate.x == avoidX && candidate.y == avoidY) return false;
        const int tile = IndexOf(game, candidate.x, candidate.y);
        if (game.terrain[tile] != Terrain::Ground || game.paths[tile] != PathState::Empty ||
            occupied[tile] || IsBaseFootprintTile(game, candidate.x, candidate.y) ||
            IsBasePuddleTile(game, candidate.x, candidate.y) ||
            IsRelayFootprintTile(game, candidate.x, candidate.y)) return false;
        for (int trap = 0; trap < game.trapCount; ++trap) {
            if (game.traps[trap].x == candidate.x && game.traps[trap].y == candidate.y) return false;
        }
        return true;
    };

    for (int index = 0; index < game.resourceCount; ++index) {
        ResourceNode& resource = game.resources[index];
        if (!resource.active) continue;
        int chosen = -1;
        const std::uint32_t hash = static_cast<std::uint32_t>(game.day * 97 + index * 37 +
            static_cast<int>(resource.zone) * 17 + static_cast<int>(resource.kind) * 11);
        for (int offset = 0; offset < game.resourceCandidateCount; ++offset) {
            const int candidateIndex = static_cast<int>((hash + offset) %
                static_cast<std::uint32_t>(game.resourceCandidateCount));
            const ResourceCandidate& candidate = game.resourceCandidates[candidateIndex];
            if (candidate.zone == resource.zone && candidateUsable(candidate, oldX[index], oldY[index])) {
                chosen = candidateIndex;
                break;
            }
        }
        if (chosen >= 0) {
            resource.x = game.resourceCandidates[chosen].x;
            resource.y = game.resourceCandidates[chosen].y;
        }
        const int tile = IndexOf(game, resource.x, resource.y);
        occupied[tile] = true;
    }

    int activeCounts[3][kResourceKinds] = {};
    for (int index = 0; index < game.resourceCount; ++index) {
        const ResourceNode& resource = game.resources[index];
        if (resource.active) {
            ++activeCounts[static_cast<int>(resource.zone)][static_cast<int>(resource.kind)];
        }
    }

    const int cycle = (game.day - 2) % 6;
    bool regenerate[kResourceKinds] = {};
    regenerate[static_cast<int>(ResourceKind::Food)] = cycle != 5;
    regenerate[static_cast<int>(ResourceKind::Water)] = cycle != 2;
    for (int kind = 0; kind < 2; ++kind) {
        if (!regenerate[kind]) continue;
        for (int zoneOffset = 0; zoneOffset < 3; ++zoneOffset) {
            const int zone = (game.day + kind + zoneOffset) % 3;
            if (activeCounts[zone][kind] >= game.resourceCaps[zone][kind]) continue;
            int chosen = -1;
            const std::uint32_t start = static_cast<std::uint32_t>(game.day * 53 + zone * 19 + kind * 7);
            for (int offset = 0; offset < game.resourceCandidateCount; ++offset) {
                const int candidateIndex = static_cast<int>((start + offset) %
                    static_cast<std::uint32_t>(game.resourceCandidateCount));
                const ResourceCandidate& candidate = game.resourceCandidates[candidateIndex];
                if (static_cast<int>(candidate.zone) == zone && candidateUsable(candidate, -1, -1)) {
                    chosen = candidateIndex;
                    break;
                }
            }
            if (chosen >= 0 && AddZonedResource(game, static_cast<ResourceZone>(zone),
                                                static_cast<ResourceKind>(kind),
                                                game.resourceCandidates[chosen].x,
                                                game.resourceCandidates[chosen].y)) {
                occupied[IndexOf(game, game.resourceCandidates[chosen].x,
                                 game.resourceCandidates[chosen].y)] = true;
                ++activeCounts[zone][kind];
                break;
            }
        }
    }

    const int material = static_cast<int>(ResourceKind::Material);
    for (int zone = 0; zone < 3; ++zone) {
        while (activeCounts[zone][material] < game.resourceCaps[zone][material]) {
            int chosen = -1;
            int fallback = -1;
            const std::uint32_t start = static_cast<std::uint32_t>(
                game.day * 71 + zone * 29 + activeCounts[zone][material] * 13);
            for (int offset = 0; offset < game.resourceCandidateCount; ++offset) {
                const int candidateIndex = static_cast<int>((start + offset) %
                    static_cast<std::uint32_t>(game.resourceCandidateCount));
                const ResourceCandidate& candidate = game.resourceCandidates[candidateIndex];
                if (static_cast<int>(candidate.zone) != zone ||
                    !candidateUsable(candidate, -1, -1)) continue;
                bool previousMaterialPosition = false;
                for (int old = 0; old < game.resourceCount; ++old) {
                    if (oldKind[old] == ResourceKind::Material &&
                        static_cast<int>(oldZone[old]) == zone &&
                        oldX[old] == candidate.x && oldY[old] == candidate.y) {
                        previousMaterialPosition = true;
                        break;
                    }
                }
                if (!previousMaterialPosition) {
                    chosen = candidateIndex;
                    break;
                } else if (fallback < 0) {
                    fallback = candidateIndex;
                }
            }
            if (chosen < 0) chosen = fallback;
            if (chosen < 0 ||
                !AddZonedResource(game, static_cast<ResourceZone>(zone),
                                  ResourceKind::Material,
                                  game.resourceCandidates[chosen].x,
                                  game.resourceCandidates[chosen].y)) {
                break;
            }
            occupied[IndexOf(game, game.resourceCandidates[chosen].x,
                             game.resourceCandidates[chosen].y)] = true;
            ++activeCounts[zone][material];
        }
    }
    game.lastResourceRefreshDay = game.day;
}

bool AddAnimalSpawnCandidate(GameState& game, AnimalKind kind, ResourceZone zone, int x, int y) {
    if (game.animalSpawnCandidateCount >= kMaxAnimalSpawnCandidates || !IsInside(game, x, y) ||
        game.terrain[IndexOf(game, x, y)] != Terrain::Ground) {
        return false;
    }
    AnimalSpawnCandidate& candidate = game.animalSpawnCandidates[game.animalSpawnCandidateCount++];
    candidate.x = x;
    candidate.y = y;
    candidate.kind = kind;
    candidate.zone = zone;
    return true;
}

void ResetAnimalsForNight(GameState& game, float playerX, float playerY) {
    if (game.animalSpawnCandidateCount <= 0) {
        game.animalCount = 0;
        return;
    }
    float previousX[kMaxAnimals] = {};
    float previousY[kMaxAnimals] = {};
    for (int index = 0; index < kMaxAnimals; ++index) {
        previousX[index] = game.animals[index].x;
        previousY[index] = game.animals[index].y;
    }
    bool occupied[kMaxAnimalSpawnCandidates] = {};
    game.animalCount = kMaxAnimals;
    for (int slot = 0; slot < kMaxAnimals; ++slot) {
        const AnimalKind kind = slot < 36 ? AnimalKind::Moth : AnimalKind::Boar;
        int chosen = -1;
        const int start = (game.day * 3 + slot * 5) % game.animalSpawnCandidateCount;
        for (int offset = 0; offset < game.animalSpawnCandidateCount; ++offset) {
            const int candidateIndex = (start + offset) % game.animalSpawnCandidateCount;
            const AnimalSpawnCandidate& candidate = game.animalSpawnCandidates[candidateIndex];
            if (occupied[candidateIndex] || candidate.kind != kind) continue;
            const float x = static_cast<float>(candidate.x) + 0.5f;
            const float y = static_cast<float>(candidate.y) + 0.5f;
            const float baseDx = x - (static_cast<float>(game.baseX) + 0.5f);
            const float baseDy = y - (static_cast<float>(game.baseY) + 0.5f);
            const float playerDx = x - playerX;
            const float playerDy = y - playerY;
            if (baseDx * baseDx + baseDy * baseDy < 100.0f ||
                playerDx * playerDx + playerDy * playerDy < 25.0f ||
                !AnimalPositionAllowed(game, x, y) ||
                (x == previousX[slot] && y == previousY[slot])) {
                continue;
            }
            chosen = candidateIndex;
            break;
        }
        Animal& animal = game.animals[slot];
        animal = {};
        animal.kind = kind;
        animal.mode = AnimalMode::Idle;
        animal.active = chosen >= 0;
        if (chosen >= 0) {
            const AnimalSpawnCandidate& candidate = game.animalSpawnCandidates[chosen];
            occupied[chosen] = true;
            animal.zone = candidate.zone;
            animal.x = static_cast<float>(candidate.x) + 0.5f;
            animal.y = static_cast<float>(candidate.y) + 0.5f;
            animal.homeX = animal.x;
            animal.homeY = animal.y;
        }
    }
}

void StepAnimals(GameState& game, const AnimalInput& input, float seconds, AnimalEvents& events) {
    events = {};
    events.damageSourceKind = -1;
    events.droppedResourceKind = -1;
    if (game.phase != GamePhase::Night || seconds <= 0.0f) return;
    for (int index = 0; index < game.animalCount; ++index) {
        Animal& animal = game.animals[index];
        if (!animal.active) continue;
        animal.contactCooldown = Maximum(0.0f, animal.contactCooldown - seconds);
        const float previousX = animal.x;
        const float previousY = animal.y;
        const bool wasCharging = animal.mode == AnimalMode::Charge;
        const float dx = input.playerX - animal.x;
        const float dy = input.playerY - animal.y;
        const float distanceSquared = dx * dx + dy * dy;
        const float distance = std::sqrt(distanceSquared + 0.0001f);

        if (input.playerSafe &&
            (animal.kind == AnimalKind::Moth || animal.mode != AnimalMode::Charge) &&
            animal.mode != AnimalMode::Idle &&
            animal.mode != AnimalMode::Retreat && animal.mode != AnimalMode::Stunned) {
            BeginRetreat(animal);
        }

        if (animal.kind == AnimalKind::Moth) switch (animal.mode) {
            case AnimalMode::Idle:
                if (!input.playerSafe && distanceSquared < 15.0f * 15.0f) {
                    animal.mode = AnimalMode::Alert;
                    animal.timer = 0.6f;
                }
                break;
            case AnimalMode::Alert:
                animal.timer -= seconds;
                if (animal.timer <= 0.0f) {
                    animal.mode = AnimalMode::Pursue;
                    animal.timer = 5.0f;
                }
                break;
            case AnimalMode::Pursue: {
                animal.timer -= seconds;
                if (animal.timer <= 0.0f || distance < 1.5f) {
                    animal.mode = AnimalMode::Orbit;
                    animal.timer = 1.0f;
                    break;
                }
                const float curve = (index & 1) == 0 ? 0.35f : -0.35f;
                const float moveX = dx / distance - dy / distance * curve;
                const float moveY = dy / distance + dx / distance * curve;
                const float moveLength = std::sqrt(moveX * moveX + moveY * moveY);
                MoveAnimal(game, animal, moveX / moveLength * 2.0f,
                           moveY / moveLength * 2.0f, seconds);
                break;
            }
            case AnimalMode::Orbit:
                animal.timer -= seconds;
                MoveAnimal(game, animal, -dy / distance * 1.7f, dx / distance * 1.7f, seconds);
                if (animal.timer <= 0.0f) {
                    animal.mode = AnimalMode::Charge;
                    animal.timer = 0.7f;
                    animal.vx = dx / distance * 4.0f;
                    animal.vy = dy / distance * 4.0f;
                }
                break;
            case AnimalMode::Charge:
                animal.timer -= seconds;
                MoveAnimal(game, animal, animal.vx, animal.vy, seconds);
                if (animal.timer <= 0.0f) BeginRetreat(animal);
                break;
            case AnimalMode::Retreat: {
                animal.timer -= seconds;
                const float homeDx = animal.homeX - animal.x;
                const float homeDy = animal.homeY - animal.y;
                const float homeDistance = std::sqrt(homeDx * homeDx + homeDy * homeDy + 0.0001f);
                MoveAnimal(game, animal, homeDx / homeDistance * 2.3f,
                           homeDy / homeDistance * 2.3f, seconds);
                if (animal.timer <= 0.0f) animal.mode = AnimalMode::Idle;
                break;
            }
            case AnimalMode::Stunned:
                animal.timer -= seconds;
                if (animal.timer <= 0.0f) {
                    if (animal.hasAfterimageTarget) {
                        animal.mode = AnimalMode::Afterimage;
                        animal.timer = 1.0f;
                    } else {
                        BeginRetreat(animal);
                    }
                }
                break;
            case AnimalMode::Afterimage: {
                animal.timer -= seconds;
                const float targetDx = animal.targetX - animal.x;
                const float targetDy = animal.targetY - animal.y;
                const float targetDistance = std::sqrt(targetDx * targetDx + targetDy * targetDy + 0.0001f);
                MoveAnimal(game, animal, targetDx / targetDistance * 2.0f,
                           targetDy / targetDistance * 2.0f, seconds);
                if (animal.timer <= 0.0f) BeginRetreat(animal);
                break;
            }
        } else switch (animal.mode) {
            case AnimalMode::Idle:
                if (!input.playerSafe && distanceSquared < 10.0f * 10.0f) {
                    animal.mode = AnimalMode::Alert;
                    animal.timer = 0.9f;
                } else {
                    if (animal.timer <= 0.0f) {
                        animal.timer = 1.5f;
                        animal.vx = (index & 1) == 0 ? 0.65f : -0.65f;
                        animal.vy = (index & 2) == 0 ? 0.35f : -0.35f;
                    }
                    animal.timer -= seconds;
                    if (!MoveAnimalStraight(game, animal, animal.vx, animal.vy, seconds)) {
                        animal.vx = -animal.vx;
                        animal.vy = -animal.vy;
                    }
                }
                break;
            case AnimalMode::Alert:
                animal.timer -= seconds;
                if (animal.timer <= 0.0f) {
                    animal.mode = AnimalMode::Charge;
                    animal.timer = 1.2f;
                    animal.vx = dx / distance * 6.8f;
                    animal.vy = dy / distance * 6.8f;
                }
                break;
            case AnimalMode::Charge:
                animal.timer -= seconds;
                if (!MoveBoarCharge(game, animal, seconds, events)) {
                    animal.mode = AnimalMode::Stunned;
                    if (animal.timer < 6.5f) animal.timer = 1.5f;
                    animal.vx = 0.0f;
                    animal.vy = 0.0f;
                } else if (animal.timer <= 0.0f) {
                    BeginRetreat(animal);
                }
                break;
            case AnimalMode::Retreat: {
                animal.timer -= seconds;
                const float homeDx = animal.homeX - animal.x;
                const float homeDy = animal.homeY - animal.y;
                const float homeDistance = std::sqrt(homeDx * homeDx + homeDy * homeDy + 0.0001f);
                MoveAnimal(game, animal, homeDx / homeDistance * 2.3f,
                           homeDy / homeDistance * 2.3f, seconds);
                if (animal.timer <= 0.0f) animal.mode = AnimalMode::Idle;
                break;
            }
            case AnimalMode::Stunned:
                animal.timer -= seconds;
                if (animal.timer <= 0.0f) BeginRetreat(animal);
                break;
            case AnimalMode::Pursue:
            case AnimalMode::Orbit:
            case AnimalMode::Afterimage:
                BeginRetreat(animal);
                break;
        }

        if (!input.playerSafe && animal.contactCooldown <= 0.0f &&
            SegmentTouchesCircle(previousX, previousY, animal.x, animal.y,
                                 input.playerX, input.playerY, 0.65f)) {
            const float loss = animal.kind == AnimalKind::Moth
                ? (wasCharging ? 15.0f : 5.0f)
                : (wasCharging ? 12.0f : 4.0f);
            game.personalLight = Maximum(0.0f, game.personalLight - loss);
            events.personalLightLost += loss;
            events.damageSourceKind = static_cast<int>(animal.kind);
            animal.contactCooldown = 1.0f;
            if (animal.kind == AnimalKind::Boar && wasCharging) {
                const int dropped = DropRandomCarriedResource(game);
                if (dropped >= 0) {
                    const int dropX = static_cast<int>(animal.x);
                    const int dropY = static_cast<int>(animal.y);
                    if (AddZonedResource(game, animal.zone, static_cast<ResourceKind>(dropped),
                                         dropX, dropY)) {
                        events.droppedResourceKind = dropped;
                        events.droppedX = animal.x;
                        events.droppedY = animal.y;
                    } else {
                        ++game.carried[dropped];
                    }
                }
            }
            BeginRetreat(animal);
        }
    }
}

void ApplyBlinkToAnimals(GameState& game, float playerX, float playerY,
                         float afterimageX, float afterimageY) {
    for (int index = 0; index < game.animalCount; ++index) {
        Animal& animal = game.animals[index];
        if (!animal.active) continue;
        const float dx = animal.x - playerX;
        const float dy = animal.y - playerY;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared >= 4.5f * 4.5f) continue;
        const float distance = std::sqrt(distanceSquared + 0.0001f);
        MoveAnimal(game, animal, dx / distance * 1.125f, dy / distance * 1.125f, 1.0f);
        animal.mode = AnimalMode::Stunned;
        animal.timer = 1.4f;
        animal.hasAfterimageTarget = animal.kind == AnimalKind::Moth;
        animal.targetX = afterimageX;
        animal.targetY = afterimageY;
    }
}

bool AddTrap(GameState& game, TrapKind kind, int x, int y) {
    if (game.trapCount >= kMaxTraps || !IsInside(game, x, y)) return false;
    if (IsBaseFootprintTile(game, x, y)) return false;
    if (IsBasePuddleTile(game, x, y)) return false;
    if (IsRelayFootprintTile(game, x, y)) return false;
    const int tile = IndexOf(game, x, y);
    if (game.terrain[tile] != Terrain::Ground) return false;
    if (std::abs(x - game.baseX) <= 1 && std::abs(y - game.baseY) <= 1) return false;
    for (int relay = 0; relay < game.relayCount; ++relay) {
        if (std::abs(x - game.relayX[relay]) <= 1 &&
            std::abs(y - game.relayY[relay]) <= 1) return false;
    }
    for (int resource = 0; resource < game.resourceCount; ++resource) {
        if (game.resources[resource].active && game.resources[resource].x == x &&
            game.resources[resource].y == y) return false;
    }
    for (int trap = 0; trap < game.trapCount; ++trap) {
        if (game.traps[trap].x == x && game.traps[trap].y == y) return false;
    }
    game.traps[game.trapCount++] = {x, y, kind, false, 0.0f};
    return true;
}

void ResetTrapsForNight(GameState& game) {
    for (int index = 0; index < game.trapCount; ++index) {
        game.traps[index].triggered = false;
        game.traps[index].cooldown = 0.0f;
    }
}

void StepTraps(GameState& game, float playerX, float playerY, float seconds, TrapEvents& events) {
    events = {};
    game.snareTimer = Maximum(0.0f, game.snareTimer - seconds);
    if (game.phase != GamePhase::Night || seconds <= 0.0f) return;
    for (int index = 0; index < game.trapCount; ++index) {
        Trap& trap = game.traps[index];
        trap.cooldown = Maximum(0.0f, trap.cooldown - seconds);
        const float dx = static_cast<float>(trap.x) + 0.5f - playerX;
        const float dy = static_cast<float>(trap.y) + 0.5f - playerY;
        if (dx * dx + dy * dy > 0.55f * 0.55f) continue;
        if (trap.kind == TrapKind::Rustle) {
            if (trap.triggered) continue;
            trap.triggered = true;
            events.rustleTriggered = true;
            for (int animalIndex = 0; animalIndex < game.animalCount; ++animalIndex) {
                Animal& animal = game.animals[animalIndex];
                const float animalDx = animal.x - playerX;
                const float animalDy = animal.y - playerY;
                if (animal.active && animal.mode == AnimalMode::Idle &&
                    animalDx * animalDx + animalDy * animalDy <= 8.0f * 8.0f) {
                    animal.mode = AnimalMode::Alert;
                    animal.timer = animal.kind == AnimalKind::Moth ? 0.6f : 0.9f;
                }
            }
        } else if (trap.cooldown <= 0.0f) {
            trap.triggered = true;
            game.snareTimer = Maximum(game.snareTimer, 0.6f);
            trap.cooldown = 1.0f;
            events.snareTriggered = true;
        }
    }
}

int FindNearestResource(const GameState& game, float x, float y, float maxDistance) {
    int nearest = -1;
    float bestDistanceSquared = maxDistance * maxDistance;
    for (int index = 0; index < game.resourceCount; ++index) {
        const ResourceNode& resource = game.resources[index];
        if (!resource.active) {
            continue;
        }
        const float dx = static_cast<float>(resource.x) + 0.5f - x;
        const float dy = static_cast<float>(resource.y) + 0.5f - y;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared <= bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            nearest = index;
        }
    }
    return nearest;
}

Interaction ResolveInteraction(const GameState& game, float playerCenterX, float playerCenterY) {
    Interaction best = {InteractionKind::None, -1, -1, -1};
    float bestDistanceSquared = 0.0f;
    int bestPriority = 3;

    const auto consider = [&](Interaction candidate, float targetX, float targetY,
                              float radius, int priority) {
        const float dx = targetX - playerCenterX;
        const float dy = targetY - playerCenterY;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared > radius * radius) {
            return;
        }
        if (best.kind == InteractionKind::None || distanceSquared < bestDistanceSquared ||
            (distanceSquared == bestDistanceSquared && priority < bestPriority)) {
            best = candidate;
            bestDistanceSquared = distanceSquared;
            bestPriority = priority;
        }
    };

    if (game.pathInventory >= kPathBuildCost) {
        for (int y = 0; y < game.height; ++y) {
            for (int x = 0; x < game.width; ++x) {
                if (game.paths[IndexOf(game, x, y)] == PathState::Broken) {
                    consider({InteractionKind::Repair, -1, x, y},
                             static_cast<float>(x) + 0.5f,
                             static_cast<float>(y) + 0.5f,
                             kBrokenPathInteractionRadius, 0);
                }
            }
        }
    }

    for (int index = 0; index < game.resourceCount; ++index) {
        const ResourceNode& resource = game.resources[index];
        if (resource.active) {
            consider({InteractionKind::Take, index, -1, -1},
                     static_cast<float>(resource.x) + 0.5f,
                     static_cast<float>(resource.y) + 0.5f,
                     kResourceInteractionRadius, 1);
        }
    }

    const TileRect baseRect = GetBaseFootprintRect(game);
    if (baseRect.width > 0 && baseRect.height > 0) {
        consider({InteractionKind::Sleep, -1, -1, -1},
                 static_cast<float>(baseRect.x) + static_cast<float>(baseRect.width) * 0.5f,
                 static_cast<float>(baseRect.y) + static_cast<float>(baseRect.height) * 0.5f,
                 kBaseInteractionRadius, 2);
    }
    return best;
}

bool IsWithinBaseDepositRange(const GameState& game, float playerCenterX, float playerCenterY) {
    const TileRect baseRect = GetBaseFootprintRect(game);
    if (baseRect.width == 0 || baseRect.height == 0) {
        return false;
    }
    const float dx = static_cast<float>(baseRect.x) + static_cast<float>(baseRect.width) * 0.5f -
        playerCenterX;
    const float dy = static_cast<float>(baseRect.y) + static_cast<float>(baseRect.height) * 0.5f -
        playerCenterY;
    return dx * dx + dy * dy <= kBaseInteractionRadius * kBaseInteractionRadius;
}

bool CollectResource(GameState& game, int resourceIndex) {
    if (resourceIndex < 0 || resourceIndex >= game.resourceCount) {
        return false;
    }
    ResourceNode& resource = game.resources[resourceIndex];
    if (!resource.active) {
        return false;
    }
    const int amount = resource.kind == ResourceKind::Material ? kPathCacheUnits : 1;
    if (game.foodCrisis) {
        int carriedTotal = 0;
        for (int kind = 0; kind < kResourceKinds; ++kind) carriedTotal += game.carried[kind];
        if (carriedTotal + amount > 2) return false;
    }
    game.carried[static_cast<int>(resource.kind)] += amount;
    resource.active = false;
    return true;
}

void DepositAtBase(GameState& game) {
    game.food += game.carried[static_cast<int>(ResourceKind::Food)];
    game.water += game.carried[static_cast<int>(ResourceKind::Water)];
    game.pathInventory = static_cast<std::uint16_t>(
        game.pathInventory + game.carried[static_cast<int>(ResourceKind::Material)]);
    for (int index = 0; index < kResourceKinds; ++index) {
        if (index != static_cast<int>(ResourceKind::Material)) {
            game.stored[index] += game.carried[index];
        }
        game.carried[index] = 0;
    }
    if (game.food > 0) game.foodCrisis = false;
    if (game.water > 0) game.waterCrisis = false;
}

bool MoveOneResourceToBase(GameState& game, ResourceKind kind) {
    const int index = static_cast<int>(kind);
    if (index < 0 || index >= kResourceKinds || game.carried[index] <= 0) {
        return false;
    }
    --game.carried[index];
    if (kind == ResourceKind::Food) {
        ++game.food;
        game.foodCrisis = false;
    } else if (kind == ResourceKind::Water) {
        ++game.water;
        game.waterCrisis = false;
    } else {
        ++game.pathInventory;
    }
    if (kind != ResourceKind::Material) ++game.stored[index];
    return true;
}

bool MoveOneResourceToCarry(GameState& game, ResourceKind kind) {
    const int index = static_cast<int>(kind);
    if (index < 0 || index >= kResourceKinds) {
        return false;
    }
    if (kind == ResourceKind::Food) {
        if (game.food <= 0) return false;
        --game.food;
    } else if (kind == ResourceKind::Water) {
        if (game.water <= 0) return false;
        --game.water;
    } else {
        if (game.pathInventory == 0) return false;
        --game.pathInventory;
    }
    if (kind != ResourceKind::Material && game.stored[index] > 0) --game.stored[index];
    ++game.carried[index];
    return true;
}

int DropRandomCarriedResource(GameState& game) {
    int available[kResourceKinds] = {};
    int count = 0;
    for (int kind = 0; kind < kResourceKinds; ++kind) {
        if (game.carried[kind] > 0) {
            available[count++] = kind;
        }
    }
    if (count == 0) {
        return -1;
    }
    game.randomState = game.randomState * 1664525u + 1013904223u;
    const int chosen = available[game.randomState % static_cast<std::uint32_t>(count)];
    --game.carried[chosen];
    return chosen;
}

bool BeginPathPlacement(const GameState& game, PlacementState& placement,
                        int playerTileX, int playerTileY, int facingX, int facingY) {
    const int targetX = playerTileX + facingX;
    const int targetY = playerTileY + facingY;
    const bool intactPath = IsInside(game, targetX, targetY) &&
        (game.paths[IndexOf(game, targetX, targetY)] == PathState::Uncharged ||
         game.paths[IndexOf(game, targetX, targetY)] == PathState::Charged);
    if (game.pathInventory < kPathBuildCost && !intactPath) {
        placement = {};
        return false;
    }
    placement.active = true;
    UpdatePathPlacement(placement, playerTileX, playerTileY, facingX, facingY);
    return true;
}

void UpdatePathPlacement(PlacementState& placement, int playerTileX, int playerTileY,
                         int facingX, int facingY) {
    if (!placement.active) return;
    placement.x = playerTileX + facingX;
    placement.y = playerTileY + facingY;
}

void CancelPathPlacement(PlacementState& placement) {
    placement.active = false;
}

}  // namespace lc
