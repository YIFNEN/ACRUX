#pragma once

#include <cstdint>

namespace lc {

constexpr int kMapSide = 256;
constexpr int kMaxTiles = kMapSide * kMapSide;

enum class Terrain : std::uint8_t {
    Ground,
    Rock,
    Bush,
    Base,
    Relay,
};

enum class PathState : std::uint8_t {
    Empty,
    Uncharged,
    Charged,
    Broken,
};

enum class GamePhase : std::uint8_t {
    Day,
    Night,
    Ending,
    GameOver,
};

enum class FailureReason : std::uint8_t {
    None,
    PersonalLight,
    BaseEnergy,
    Survival,
};

enum class Season : std::uint8_t {
    None,
    Winter,
    Spring,
    Summer,
    Autumn,
};

enum class Language : std::uint8_t {
    English,
    Korean,
};

constexpr int kResourceKinds = 3;
constexpr int kMaxResources = 384;
constexpr int kMaxResourceCandidates = 512;
constexpr int kMaxAnimals = 60;
constexpr int kMaxAnimalSpawnCandidates = 256;
constexpr int kMaxTraps = 144;

enum class ResourceKind : std::uint8_t {
    Food = 0,
    Water = 1,
    Material = 2,
};

enum class ResourceZone : std::uint8_t {
    Food = 0,
    Water = 1,
    Path = 2,
};

enum class DistanceTier : std::uint8_t {
    Near,
    Far,
};

struct ResourceNode {
    int x;
    int y;
    ResourceKind kind;
    ResourceZone zone;
    bool active;
};

struct ResourceCandidate {
    int x;
    int y;
    ResourceZone zone;
    DistanceTier tier;
};

enum class AnimalKind : std::uint8_t {
    Moth,
    Boar,
};

enum class AnimalMode : std::uint8_t {
    Idle,
    Alert,
    Pursue,
    Orbit,
    Charge,
    Retreat,
    Stunned,
    Afterimage,
};

struct Animal {
    AnimalKind kind;
    AnimalMode mode;
    ResourceZone zone;
    float x;
    float y;
    float vx;
    float vy;
    float homeX;
    float homeY;
    float targetX;
    float targetY;
    float timer;
    float contactCooldown;
    bool active;
    bool hasAfterimageTarget;
};

struct AnimalSpawnCandidate {
    int x;
    int y;
    AnimalKind kind;
    ResourceZone zone;
};

struct AnimalInput {
    float playerX;
    float playerY;
    float afterimageX;
    float afterimageY;
    bool playerSafe;
    bool afterimageActive;
};

struct AnimalEvents {
    float personalLightLost;
    int damageSourceKind;
    int droppedResourceKind;
    float droppedX;
    float droppedY;
    bool pathBroken;
    int pathBreakX;
    int pathBreakY;
};

enum class TrapKind : std::uint8_t {
    Rustle,
    Snare,
};

struct Trap {
    int x;
    int y;
    TrapKind kind;
    bool triggered;
    float cooldown;
};

struct TrapEvents {
    bool rustleTriggered;
    bool snareTriggered;
};

struct PlacementState {
    bool active;
    int x;
    int y;
};

struct TileRect {
    int x;
    int y;
    int width;
    int height;
};

enum class InteractionKind : std::uint8_t {
    None,
    Repair,
    Take,
    Sleep,
};

struct Interaction {
    InteractionKind kind;
    int resourceIndex;
    int pathX;
    int pathY;
};

constexpr float kResourceInteractionRadius = 1.5f;
constexpr float kBrokenPathInteractionRadius = 1.5f;
constexpr float kBaseInteractionRadius = 1.75f;

struct GameState {
    int width;
    int height;
    int baseX;
    int baseY;
    std::uint16_t pathInventory;
    int carried[kResourceKinds];
    int stored[kResourceKinds];
    int food;
    int water;
    int day;
    bool completed;
    int completionDay;
    std::uint64_t loreSeenMask;
    int relaysConnected;
    int relaysPowered;
    int relayCount;
    int relayX[3];
    int relayY[3];
    bool relayConnected[3];
    bool relayPowered[3];
    int resourceCount;
    int resourceCandidateCount;
    int resourceCaps[3][kResourceKinds];
    int lastResourceRefreshDay;
    int removedPathX;
    int removedPathY;
    int removedPathDay;
    bool removedPathWasCharged;
    bool removedPathUndoValid;
    int animalCount;
    int animalSpawnCandidateCount;
    int trapCount;
    std::uint32_t randomState;
    float personalLight;
    float personalLightMax;
    float baseEnergy;
    float baseEnergyMax;
    float blinkCooldown;
    float afterimageTime;
    float snareTimer;
    bool nextNightPenalty;
    bool foodCrisis;
    bool waterCrisis;
    FailureReason lastFailure;
    GamePhase phase;
    Terrain terrain[kMaxTiles];
    PathState paths[kMaxTiles];
    bool powered[kMaxTiles];
    bool loopLit[kMaxTiles];
    ResourceNode resources[kMaxResources];
    ResourceCandidate resourceCandidates[kMaxResourceCandidates];
    Animal animals[kMaxAnimals];
    AnimalSpawnCandidate animalSpawnCandidates[kMaxAnimalSpawnCandidates];
    Trap traps[kMaxTraps];
};

constexpr std::uint32_t kSaveVersion = 3;

struct SaveData {
    std::uint32_t version;
    int width;
    int height;
    int baseX;
    int baseY;
    std::uint16_t pathInventory;
    bool completed;
    int completionDay;
    std::uint64_t loreSeenMask;
    int carried[kResourceKinds];
    int stored[kResourceKinds];
    int food;
    int water;
    int day;
    int relayCount;
    int relayX[3];
    int relayY[3];
    int resourceCount;
    int resourceCandidateCount;
    int resourceCaps[3][kResourceKinds];
    int lastResourceRefreshDay;
    int trapCount;
    std::uint32_t randomState;
    bool nextNightPenalty;
    bool foodCrisis;
    bool waterCrisis;
    Terrain terrain[kMaxTiles];
    PathState paths[kMaxTiles];
    ResourceNode resources[kMaxResources];
    ResourceCandidate resourceCandidates[kMaxResourceCandidates];
    AnimalSpawnCandidate animalSpawnCandidates[kMaxAnimalSpawnCandidates];
    int animalSpawnCandidateCount;
    Trap traps[kMaxTraps];
};

void InitializeForTests(GameState& game, int width, int height);
void InitializeGame(GameState& game);
void SetBase(GameState& game, int x, int y);
void SetRelay(GameState& game, int relayIndex, int x, int y);
TileRect GetBaseFootprintRect(const GameState& game);
bool IsBaseFootprintTile(const GameState& game, int x, int y);
bool CanPlacePath(const GameState& game, int x, int y);
bool PlacePath(GameState& game, int x, int y);
bool RemovePath(GameState& game, int x, int y);
bool DamagePath(GameState& game, int x, int y);
bool RepairPath(GameState& game, int x, int y);
bool RecoverBrokenPath(GameState& game, int x, int y);
void ChargeConnectedPaths(GameState& game);
void RecomputePower(GameState& game);
bool IsPowered(const GameState& game, int x, int y);
bool IsLoopLit(const GameState& game, int x, int y);
bool IsStabilizedPath(const GameState& game, int x, int y);
bool IsChargeNode(const GameState& game, int x, int y);
TileRect GetRelayFootprintRect(const GameState& game, int relayIndex);
bool IsRelayFootprintTile(const GameState& game, int x, int y);
TileRect GetBasePuddleRect(const GameState& game);
bool IsBasePuddleTile(const GameState& game, int x, int y);
Season CurrentSeason(const GameState& game);
Language NextLanguage(Language language);
int ChooseNextLore(GameState& game);
void MarkLoreSeen(GameState& game, int loreIndex);
const char* LoreText(int loreIndex);
const char* LoreText(int loreIndex, Language language);
void BeginNight(GameState& game);
void StepNight(GameState& game, float seconds, bool onPoweredPath, bool wantsCharge = false);
bool UseBlink(GameState& game);
void SleepAtBase(GameState& game);
bool CreateSaveData(const GameState& game, SaveData& save);
bool RestoreSaveData(GameState& game, const SaveData& save);
void RefreshRelayWiring(GameState& game);
bool AddResource(GameState& game, ResourceKind kind, int x, int y);
bool AddZonedResource(GameState& game, ResourceZone zone, ResourceKind kind, int x, int y);
bool AddResourceCandidate(GameState& game, ResourceZone zone, DistanceTier tier, int x, int y);
void FinalizeResourceLayout(GameState& game);
void RefreshResourcesForNight(GameState& game);
bool AddAnimalSpawnCandidate(GameState& game, AnimalKind kind, ResourceZone zone, int x, int y);
void ResetAnimalsForNight(GameState& game, float playerX, float playerY);
void StepAnimals(GameState& game, const AnimalInput& input, float seconds, AnimalEvents& events);
void ApplyBlinkToAnimals(GameState& game, float playerX, float playerY,
                         float afterimageX, float afterimageY);
bool AddTrap(GameState& game, TrapKind kind, int x, int y);
void ResetTrapsForNight(GameState& game);
void StepTraps(GameState& game, float playerX, float playerY, float seconds, TrapEvents& events);
int FindNearestResource(const GameState& game, float x, float y, float maxDistance);
Interaction ResolveInteraction(const GameState& game, float playerCenterX, float playerCenterY);
bool IsWithinBaseDepositRange(const GameState& game, float playerCenterX, float playerCenterY);
bool CollectResource(GameState& game, int resourceIndex);
void DepositAtBase(GameState& game);
bool MoveOneResourceToBase(GameState& game, ResourceKind kind);
bool MoveOneResourceToCarry(GameState& game, ResourceKind kind);
int DropRandomCarriedResource(GameState& game);
bool BeginPathPlacement(const GameState& game, PlacementState& placement,
                        int playerTileX, int playerTileY, int facingX, int facingY);
void UpdatePathPlacement(PlacementState& placement, int playerTileX, int playerTileY,
                         int facingX, int facingY);
void CancelPathPlacement(PlacementState& placement);

}  // namespace lc
