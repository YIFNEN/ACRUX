#include "../src/game_core.h"
#include "../src/acrux_synth.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {

int failures = 0;

void Check(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    } else {
        std::printf("PASS: %s\n", name);
    }
}

void EmptyGroundAcceptsAPathTile() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.pathInventory = 2;

    Check(lc::CanPlacePath(game, 3, 4), "empty ground accepts a path tile");
}

void LanguageSelectionTogglesBetweenEnglishAndKorean() {
    Check(lc::NextLanguage(lc::Language::English) == lc::Language::Korean,
          "language selection toggles from English to Korean");
    Check(lc::NextLanguage(lc::Language::Korean) == lc::Language::English,
          "language selection toggles from Korean to English");
}

void DisconnectedPathCanBeInstalled() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.pathInventory = 2;

    Check(lc::PlacePath(game, 6, 6), "disconnected path can be installed");
    Check(game.paths[6 + 6 * game.width] == lc::PathState::Uncharged,
          "disconnected path remains uncharged");
    Check(game.pathInventory == 0, "placing consumes two path material units");
}

void DaylightPowersOnlyBaseConnectedPath() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 1, 1);
    game.pathInventory = 6;
    lc::PlacePath(game, 2, 1);
    lc::PlacePath(game, 3, 1);
    lc::PlacePath(game, 6, 6);

    lc::ChargeConnectedPaths(game);

    Check(lc::IsPowered(game, 2, 1), "base-connected path is powered after daylight");
    Check(lc::IsPowered(game, 3, 1), "connected downstream path is powered");
    Check(!lc::IsPowered(game, 6, 6), "disconnected path stays dark");
    Check(game.paths[6 + 6 * game.width] == lc::PathState::Uncharged,
          "disconnected path is not solar charged");
}

void BrokenSerialPathDarkensDownstream() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    game.pathInventory = 8;
    for (int x = 1; x <= 4; ++x) {
        lc::PlacePath(game, x, 1);
    }
    lc::ChargeConnectedPaths(game);

    lc::DamagePath(game, 2, 1);

    Check(lc::IsPowered(game, 1, 1), "serial path before break stays powered");
    Check(!lc::IsPowered(game, 3, 1), "serial path after break goes dark");
    Check(!lc::IsPowered(game, 4, 1), "serial downstream remains dark");
}

void ParallelRouteKeepsDownstreamPowered() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    game.pathInventory = 16;
    const int path[][2] = {{1, 1}, {2, 1}, {3, 1}, {4, 1}, {1, 0}, {2, 0}, {3, 0}};
    for (const auto& point : path) {
        lc::PlacePath(game, point[0], point[1]);
    }
    lc::ChargeConnectedPaths(game);

    lc::DamagePath(game, 2, 1);

    Check(lc::IsPowered(game, 4, 1), "parallel route keeps downstream powered");
}

void ClosedPoweredPathsLightOnlyTheirEnclosedInterior() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 10, 10);
    lc::SetBase(game, 1, 4);
    game.pathInventory = 30;
    const int ring[][2] = {
        {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3},
        {2, 4},                         {6, 4},
        {2, 5},                         {6, 5},
        {2, 6}, {3, 6}, {4, 6}, {5, 6}, {6, 6},
    };
    for (const auto& tile : ring) lc::PlacePath(game, tile[0], tile[1]);
    lc::ChargeConnectedPaths(game);
    Check(lc::IsLoopLit(game, 4, 4) && lc::IsLoopLit(game, 4, 5),
          "closed powered path lights its enclosed interior");
    Check(!lc::IsLoopLit(game, 7, 4), "closed path does not light exterior ground");

    lc::DamagePath(game, 4, 3);
    Check(!lc::IsLoopLit(game, 4, 4), "breaking one boundary tile immediately opens the loop");
    lc::RepairPath(game, 4, 3);
    Check(lc::IsLoopLit(game, 4, 4), "repair immediately restores closed-loop lighting");
}

void StabilizedLoopBoundaryResistsBoarCharge() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 10, 10);
    lc::SetBase(game, 1, 4);
    game.pathInventory = 32;
    const int ring[][2] = {
        {2, 3}, {3, 3}, {4, 3}, {5, 3}, {6, 3},
        {2, 4},                         {6, 4},
        {2, 5},                         {6, 5},
        {2, 6}, {3, 6}, {4, 6}, {5, 6}, {6, 6},
        {7, 4},
    };
    for (const auto& tile : ring) lc::PlacePath(game, tile[0], tile[1]);
    lc::ChargeConnectedPaths(game);
    Check(lc::IsStabilizedPath(game, 6, 4), "closed-loop boundary path is stabilized");
    Check(!lc::IsStabilizedPath(game, 7, 4), "branch attached to a loop is not stabilized");
    game.paths[7 + 4 * game.width] = lc::PathState::Empty;
    lc::RecomputePower(game);

    game.phase = lc::GamePhase::Night;
    game.animalCount = 1;
    lc::Animal& boar = game.animals[0];
    boar.active = true;
    boar.kind = lc::AnimalKind::Boar;
    boar.mode = lc::AnimalMode::Charge;
    boar.x = 8.5f;
    boar.y = 4.5f;
    boar.vx = -7.0f;
    boar.timer = 1.0f;
    lc::AnimalInput input = {0.5f, 0.5f, 0.0f, 0.0f, false, false};
    lc::AnimalEvents events = {};
    lc::StepAnimals(game, input, 0.5f, events);

    Check(game.paths[6 + 4 * game.width] == lc::PathState::Charged,
          "boar charge does not break stabilized path");
    Check(!events.pathBroken, "stabilized collision does not report a path break");
    Check(boar.mode == lc::AnimalMode::Stunned && boar.timer == 7.0f,
          "boar is stunned for seven seconds by a stabilized path");
}

void RemoveAndRepairUseInventory() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    game.pathInventory = 6;
    lc::PlacePath(game, 1, 1);
    lc::PlacePath(game, 2, 1);
    lc::ChargeConnectedPaths(game);
    lc::DamagePath(game, 1, 1);

    Check(lc::RepairPath(game, 1, 1), "charged broken path can be repaired");
    Check(game.pathInventory == 0, "repair consumes two path material units");
    Check(lc::IsPowered(game, 2, 1), "repair immediately restores downstream power");
    Check(lc::RemovePath(game, 2, 1), "intact path can be removed");
    Check(game.pathInventory == 2, "removal refunds two path material units");
}

void BrokenPathRecoveryReturnsOnePathMaterialUnit() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    game.pathInventory = 4;
    lc::PlacePath(game, 1, 1);
    lc::PlacePath(game, 2, 1);
    lc::ChargeConnectedPaths(game);
    lc::DamagePath(game, 1, 1);
    lc::DamagePath(game, 2, 1);

    Check(lc::RecoverBrokenPath(game, 1, 1), "broken path can be recovered as path material");
    Check(game.pathInventory == 1,
          "first broken path recovery returns one path material unit");
    Check(lc::RecoverBrokenPath(game, 2, 1), "second broken path recovery is allowed");
    Check(game.pathInventory == 2,
          "two broken recoveries equal the two material units needed for one path");
}

void SaveSnapshotRoundTripsPersistentNightState() {
    lc::GameState source = {};
    lc::InitializeForTests(source, 8, 8);
    lc::SetBase(source, 1, 1);
    lc::SetRelay(source, 0, 4, 1);
    source.pathInventory = 8;
    source.completed = true;
    source.completionDay = 5;
    source.loreSeenMask = (1ull << 24) | (1ull << 35);
    source.carried[static_cast<int>(lc::ResourceKind::Material)] = 2;
    source.stored[static_cast<int>(lc::ResourceKind::Food)] = 3;
    lc::PlacePath(source, 2, 1);
    lc::PlacePath(source, 3, 1);
    lc::ChargeConnectedPaths(source);
    lc::RefreshRelayWiring(source);
    source.day = 5;
    source.food = 4;
    source.water = 2;
    source.foodCrisis = true;
    source.randomState = 1234u;

    lc::SaveData save = {};
    Check(lc::CreateSaveData(source, save), "valid game state creates a save snapshot");
    save.paths[3 + source.width] = lc::PathState::Broken;

    lc::GameState restored = {};
    lc::InitializeForTests(restored, 8, 8);
    Check(lc::RestoreSaveData(restored, save), "save snapshot restores successfully");

    Check(restored.day == 5 && restored.food == 4 && restored.water == 2,
          "save restores campaign day and survival resources");
    Check(restored.pathInventory == 4,
          "save restores path material inventory");
    Check(restored.completed && restored.completionDay == 5 &&
              restored.loreSeenMask == ((1ull << 24) | (1ull << 35)),
          "save restores epilogue completion and lore progress");
    Check(restored.carried[static_cast<int>(lc::ResourceKind::Material)] == 2 &&
              restored.stored[static_cast<int>(lc::ResourceKind::Food)] == 3,
          "save restores carry and storage state");
    Check(restored.paths[3 + restored.width] == lc::PathState::Broken,
          "save restores path world state");
    Check(!restored.relayPowered[0],
          "restore recomputes powered relay state from broken topology");

    save.version = 999u;
    Check(!lc::RestoreSaveData(restored, save), "unsupported save version is rejected safely");
    save.version = lc::kSaveVersion;
    save.baseX = -1;
    Check(!lc::RestoreSaveData(restored, save), "invalid save base position is rejected safely");
}

void BlinkSpendsLightAndStartsTimers() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::BeginNight(game);

    Check(lc::UseBlink(game), "ready blink can be used");
    Check(game.personalLight == 78.0f, "blink spends twenty-two light");
    Check(game.blinkCooldown == 12.0f, "blink starts twelve second cooldown");
    Check(game.afterimageTime == 1.0f, "blink leaves a one-second afterimage");
    Check(!lc::UseBlink(game), "blink cannot be reused during cooldown");
}

void PoweredPathRequiresManualChargeInput() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::BeginNight(game);
    game.personalLight = 50.0f;
    game.baseEnergy = 100.0f;

    lc::StepNight(game, 1.0f, true, false);

    Check(game.personalLight == 49.0f, "standing on powered path alone still spends passive personal light");
    Check(game.baseEnergy == 100.0f, "idle powered path does not spend base energy");

    lc::StepNight(game, 1.0f, true, true);

    Check(game.personalLight == 63.0f, "holding charge restores personal light after passive night drain");
    Check(game.baseEnergy == 85.0f, "manual charge consumes base energy only for transferred light");
}

void NightTimersLeaveRoomForExploration() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::BeginNight(game);

    Check(game.baseEnergy == 240.0f, "base energy starts as the manual charging reserve");
    lc::StepNight(game, 10.0f, false);
    Check(game.personalLight == 90.0f, "time in darkness passively spends personal light");
    Check(game.baseEnergy == 240.0f, "time in darkness does not passively spend base energy");
}

void PathPlacementPreparationTracksMovement() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::PlacementState placement = {};

    Check(!lc::BeginPathPlacement(game, placement, 3, 3, 1, 0),
          "path preparation cannot start with no inventory");
    game.pathInventory = 2;
    Check(lc::BeginPathPlacement(game, placement, 3, 3, 1, 0),
          "path preparation starts when inventory exists");
    lc::UpdatePathPlacement(placement, 4, 3, 0, -1);
    Check(placement.active && placement.x == 4 && placement.y == 2,
          "path preview follows movement and facing");
    lc::CancelPathPlacement(placement);
    Check(!placement.active, "another action can cancel path preparation");
}

void PathPlacementAllowsRemovalWithoutInventory() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.paths[4 + 3 * game.width] = lc::PathState::Charged;
    lc::PlacementState placement = {};

    Check(lc::BeginPathPlacement(game, placement, 3, 3, 1, 0),
          "intact path can enter removal preparation with no inventory");
    Check(placement.active && placement.x == 4 && placement.y == 3,
          "removal preparation targets the facing intact path");
    lc::CancelPathPlacement(placement);
    Check(!lc::BeginPathPlacement(game, placement, 3, 3, 0, 1),
          "empty tile cannot enter placement preparation with no inventory");
}

void ChargedPathRemovalCanOnlyBeUndoneInPlaceThatNight() {
    lc::GameState restored = {};
    lc::InitializeForTests(restored, 8, 8);
    lc::SetBase(restored, 0, 1);
    restored.pathInventory = 4;
    lc::PlacePath(restored, 1, 1);
    lc::PlacePath(restored, 2, 1);
    lc::BeginNight(restored);
    const int inventoryBefore = restored.pathInventory;
    Check(lc::RemovePath(restored, 1, 1), "charged path can be lifted for an undo");
    Check(!lc::IsPowered(restored, 2, 1), "lifting charged path immediately darkens downstream");
    Check(lc::PlacePath(restored, 1, 1), "lifted path can be returned to the same tile");
    Check(restored.paths[1 + restored.width] == lc::PathState::Charged &&
              lc::IsPowered(restored, 2, 1),
          "same-night same-tile undo restores charge and downstream power");
    Check(restored.pathInventory == inventoryBefore,
          "undo returns path inventory to its exact previous total");

    lc::GameState moved = {};
    lc::InitializeForTests(moved, 8, 8);
    lc::SetBase(moved, 0, 1);
    moved.pathInventory = 2;
    lc::PlacePath(moved, 1, 1);
    lc::BeginNight(moved);
    lc::RemovePath(moved, 1, 1);
    lc::PlacePath(moved, 2, 2);
    Check(moved.paths[2 + 2 * moved.width] == lc::PathState::Uncharged,
          "moving a lifted charged path does not carry charge forward");

    lc::GameState later = {};
    lc::InitializeForTests(later, 8, 8);
    lc::SetBase(later, 0, 1);
    later.pathInventory = 2;
    lc::PlacePath(later, 1, 1);
    lc::BeginNight(later);
    lc::RemovePath(later, 1, 1);
    ++later.day;
    lc::PlacePath(later, 1, 1);
    Check(later.paths[1 + later.width] == lc::PathState::Uncharged,
          "charged undo expires when the day changes");

    lc::GameState uncharged = {};
    lc::InitializeForTests(uncharged, 8, 8);
    uncharged.pathInventory = 2;
    lc::PlacePath(uncharged, 3, 3);
    lc::RemovePath(uncharged, 3, 3);
    lc::PlacePath(uncharged, 3, 3);
    Check(uncharged.paths[3 + 3 * uncharged.width] == lc::PathState::Uncharged,
          "an originally uncharged path cannot gain charge through undo");
}

void DarknessFailureLosesOnlyCarriedResources() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.pathInventory = 2;
    lc::PlacePath(game, 3, 3);
    game.carried[0] = 2;
    game.carried[1] = 1;
    game.stored[0] = 4;
    game.food = 3;
    game.water = 3;
    lc::BeginNight(game);
    game.personalLight = 0.0f;

    lc::StepNight(game, 1.0f, false);

    Check(game.phase == lc::GamePhase::Day, "light depletion advances to day");
    Check(game.carried[0] == 0 && game.carried[1] == 0,
          "failure clears carried resources");
    Check(game.stored[0] == 4, "failure preserves stored resources");
    Check(game.paths[3 + 3 * game.width] != lc::PathState::Empty,
          "failure preserves installed paths");
    Check(game.food == 2 && game.water == 2, "failed night consumes daily survival resources");
    Check(game.nextNightPenalty, "failed night schedules base energy penalty");
    Check(game.lastFailure == lc::FailureReason::PersonalLight,
          "personal light depletion records its distinct failure reason");
}

void BaseEnergyFailureHasADistinctReason() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::BeginNight(game);
    game.personalLight = 50.0f;
    game.baseEnergy = 0.5f;

    lc::StepNight(game, 1.0f, true, true);

    Check(game.phase == lc::GamePhase::Day, "manual charge reserve depletion advances to day");
    Check(game.lastFailure == lc::FailureReason::BaseEnergy,
          "manual charge reserve depletion records its distinct failure reason");
}

void VoluntarySleepStartsARecoverableCrisisOrWins() {
    lc::GameState starving = {};
    lc::InitializeForTests(starving, 8, 8);
    starving.food = 1;
    starving.water = 2;
    lc::BeginNight(starving);
    lc::SleepAtBase(starving);
    Check(starving.phase == lc::GamePhase::Day && starving.foodCrisis,
          "first zero-food day schedules a crisis night instead of immediate game over");
    lc::BeginNight(starving);
    starving.carried[static_cast<int>(lc::ResourceKind::Food)] = 1;
    lc::DepositAtBase(starving);
    Check(!starving.foodCrisis && starving.food == 1,
          "depositing missing food immediately clears the crisis");

    lc::GameState unresolved = {};
    lc::InitializeForTests(unresolved, 8, 8);
    unresolved.food = 1;
    lc::BeginNight(unresolved);
    lc::SleepAtBase(unresolved);
    lc::BeginNight(unresolved);
    lc::SleepAtBase(unresolved);
    Check(unresolved.phase == lc::GamePhase::GameOver,
          "sleeping again with an unresolved crisis ends the campaign");

    lc::GameState thirsty = {};
    lc::InitializeForTests(thirsty, 8, 8);
    thirsty.waterCrisis = true;
    thirsty.water = 0;
    thirsty.phase = lc::GamePhase::Night;
    thirsty.personalLight = 100.0f;
    Check(lc::UseBlink(thirsty) && thirsty.blinkCooldown == 20.0f,
          "water crisis extends blink cooldown to twenty seconds");

    lc::GameState hungry = {};
    lc::InitializeForTests(hungry, 8, 8);
    hungry.foodCrisis = true;
    lc::AddResource(hungry, lc::ResourceKind::Food, 2, 2);
    hungry.carried[0] = 2;
    Check(!lc::CollectResource(hungry, 0),
          "food crisis caps carried resource units at two");

    lc::GameState winning = {};
    lc::InitializeForTests(winning, 8, 8);
    winning.food = 2;
    winning.water = 2;
    lc::SetBase(winning, 1, 1);
    lc::SetRelay(winning, 0, 4, 1);
    lc::SetRelay(winning, 1, 1, 4);
    lc::SetRelay(winning, 2, 4, 4);
    winning.pathInventory = 16;
    const int chargedCircuit[][2] = {
        {2, 1}, {3, 1}, {1, 2}, {1, 3}, {3, 2}, {3, 3},
    };
    for (const auto& point : chargedCircuit) lc::PlacePath(winning, point[0], point[1]);
    lc::BeginNight(winning);
    lc::PlacePath(winning, 3, 4);
    lc::RefreshRelayWiring(winning);
    Check(winning.relaysConnected == 3 && winning.relaysPowered == 2,
          "final night-built branch is physically complete but still uncharged");
    lc::SleepAtBase(winning);
    Check(winning.phase == lc::GamePhase::Ending,
          "voluntary sleep charges and completes the physically connected final circuit");
    Check(winning.relaysPowered == 3,
          "the ending daylight charges the final night-built relay branch");
    Check(winning.food == 2 && winning.water == 2,
          "ending resolves before daily survival loss");
}

void CompletedCampaignSleepsIntoEpilogueWithoutRetriggeringEnding() {
    lc::GameState winning = {};
    lc::InitializeForTests(winning, 8, 8);
    winning.food = 3;
    winning.water = 3;
    lc::SetBase(winning, 1, 1);
    lc::SetRelay(winning, 0, 4, 1);
    lc::SetRelay(winning, 1, 1, 4);
    lc::SetRelay(winning, 2, 4, 4);
    winning.pathInventory = 16;
    const int chargedCircuit[][2] = {
        {2, 1}, {3, 1}, {1, 2}, {1, 3}, {3, 2}, {3, 3},
    };
    for (const auto& point : chargedCircuit) lc::PlacePath(winning, point[0], point[1]);
    lc::BeginNight(winning);
    lc::PlacePath(winning, 3, 4);
    lc::RefreshRelayWiring(winning);

    lc::SleepAtBase(winning);
    Check(winning.phase == lc::GamePhase::Ending && !winning.completed,
          "unfinished campaign triggers the ending once");

    winning.completed = true;
    winning.completionDay = winning.day;
    winning.phase = lc::GamePhase::Night;
    lc::SleepAtBase(winning);

    Check(winning.phase == lc::GamePhase::Day,
          "completed campaign sleeps to epilogue day instead of retriggering ending");
    Check(winning.completed && winning.completionDay == 1,
          "completed state and original completion day survive later sleeps");
}

void SeasonStateStartsAfterCompletionOnly() {
    lc::GameState campaign = {};
    lc::InitializeForTests(campaign, 8, 8);
    campaign.day = 34;
    campaign.completionDay = 34;
    Check(lc::CurrentSeason(campaign) == lc::Season::None,
          "campaign does not show a season before completion");

    campaign.completed = true;
    Check(lc::CurrentSeason(campaign) == lc::Season::Winter,
          "completion day is winter day one");
    campaign.day = 43;
    Check(lc::CurrentSeason(campaign) == lc::Season::Winter,
          "winter lasts through day ten");
    campaign.day = 44;
    Check(lc::CurrentSeason(campaign) == lc::Season::Spring,
          "spring begins ten days after completion");
    campaign.day = 54;
    Check(lc::CurrentSeason(campaign) == lc::Season::Summer,
          "summer begins twenty days after completion");
    campaign.day = 64;
    Check(lc::CurrentSeason(campaign) == lc::Season::Autumn,
          "autumn begins thirty days after completion");
    campaign.day = 74;
    Check(lc::CurrentSeason(campaign) == lc::Season::Winter,
          "season cycle returns to winter after forty days");
}

void BasePuddleIsDerivedAndBlocksTwoByTwoPathPlacement() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 128, 128);
    lc::SetBase(game, 64, 64);
    game.pathInventory = 8;

    const lc::TileRect campaign = lc::GetBasePuddleRect(game);
    Check(campaign.x == 68 && campaign.y == 66 && campaign.width == 2 && campaign.height == 2,
          "base puddle is a two-by-two derived tile rectangle away from base");
    Check(!lc::IsBasePuddleTile(game, 67, 66), "tile beside the puddle remains open");
    for (int y = campaign.y; y < campaign.y + campaign.height; ++y) {
        for (int x = campaign.x; x < campaign.x + campaign.width; ++x) {
            Check(lc::IsBasePuddleTile(game, x, y), "each puddle tile is marked as puddle");
            Check(!lc::CanPlacePath(game, x, y), "path placement rejects puddle tiles");
        }
    }

    game.completed = true;
    const lc::TileRect epilogue = lc::GetBasePuddleRect(game);
    Check(epilogue.x == campaign.x && epilogue.y == campaign.y,
          "completed state keeps the same derived puddle location");

    lc::GameState world = {};
    lc::InitializeGame(world);
    const lc::TileRect authored = lc::GetBasePuddleRect(world);
    bool avoidsResources = true;
    bool avoidsTraps = true;
    for (int y = authored.y; y < authored.y + authored.height; ++y) {
        for (int x = authored.x; x < authored.x + authored.width; ++x) {
            const int tile = y * world.width + x;
            Check(world.terrain[tile] == lc::Terrain::Ground,
                  "authored puddle footprint starts on open ground");
            for (int resource = 0; resource < world.resourceCount; ++resource) {
                if (world.resources[resource].active && world.resources[resource].x == x &&
                    world.resources[resource].y == y) {
                    avoidsResources = false;
                }
            }
            for (int trap = 0; trap < world.trapCount; ++trap) {
                if (world.traps[trap].x == x && world.traps[trap].y == y) {
                    avoidsTraps = false;
                }
            }
        }
    }
    Check(avoidsResources, "authored puddle footprint avoids initial resources");
    Check(avoidsTraps, "authored puddle footprint avoids initial traps");
}

void BaseFootprintBlocksTwoByTwoGameplayTiles() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 12, 12);
    lc::SetBase(game, 4, 4);
    game.pathInventory = 8;

    Check(lc::IsBaseFootprintTile(game, 3, 3) &&
              lc::IsBaseFootprintTile(game, 4, 3) &&
              lc::IsBaseFootprintTile(game, 3, 4) &&
              lc::IsBaseFootprintTile(game, 4, 4),
          "base footprint occupies a two-by-two tile square");
    Check(!lc::CanPlacePath(game, 3, 3) && !lc::CanPlacePath(game, 4, 3) &&
              !lc::CanPlacePath(game, 3, 4),
          "path placement is blocked across the whole base footprint");
    Check(lc::IsChargeNode(game, 3, 3), "base footprint is a manual charge node");
    Check(lc::IsWithinBaseDepositRange(game, 4.0f, 4.0f),
          "base interaction includes the far corner of the two-by-two footprint");
}

void RelayFootprintBlocksTwoByTwoGameplayTiles() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 12, 12);
    lc::SetRelay(game, 0, 5, 5);
    game.pathInventory = 8;

    Check(lc::IsRelayFootprintTile(game, 5, 5) &&
              lc::IsRelayFootprintTile(game, 6, 5) &&
              lc::IsRelayFootprintTile(game, 5, 6) &&
              lc::IsRelayFootprintTile(game, 6, 6),
          "relay footprint occupies a two-by-two tile square");
    Check(!lc::CanPlacePath(game, 6, 5) && !lc::CanPlacePath(game, 5, 6) &&
              !lc::CanPlacePath(game, 6, 6),
          "path placement is blocked across the whole relay footprint");
    Check(!lc::AddResource(game, lc::ResourceKind::Food, 6, 5),
          "resource placement is blocked on relay footprint tiles");
    Check(!lc::AddTrap(game, lc::TrapKind::Rustle, 6, 6),
          "trap placement is blocked on relay footprint tiles");
}

void LorePoolsDoNotRepeatBeforeTheirCycleEnds() {
    Check(std::strcmp(lc::LoreText(0), "THE FIRST SHORT DAY WAS CALLED AN ANOMALY.") == 0,
          "campaign lore text keeps the approved opening line");
    Check(std::strcmp(lc::LoreText(3),
                      "THE FIELDS WERE THE FIRST TO COUNT THE MISSING HOURS.") == 0,
          "campaign lore connects shortened daylight to failing fields");
    Check(std::strcmp(lc::LoreText(7),
                      "THE FIELDS MOVED UNDER GLASS, AND THE CITIES DIMMED TO FEED THEM.") == 0,
          "campaign lore connects enclosed agriculture to energy demand");
    Check(std::strcmp(lc::LoreText(12), "THE SUN WAS RATIONED BEFORE THE BREAD.") == 0,
          "campaign lore introduces rationing without exposition");
    Check(std::strcmp(lc::LoreText(13),
                      "WE GUARDED THE BATTERIES. THEN WE GUARDED THEM FROM EACH OTHER.") == 0,
          "campaign lore implies conflict over stored power");
    Check(std::strcmp(lc::LoreText(18),
                      "THE GRID OUTLIVED THE PEOPLE WHO NAMED ITS STREETS.") == 0,
          "campaign lore keeps the grid physical rather than mnemonic");
    Check(std::strcmp(lc::LoreText(24), "THE MAP ENDS HERE. THE OLD SERVICE LINES DO NOT.") == 0,
          "epilogue lore text opens with the off-map world hint");
    Check(std::strcmp(lc::LoreText(25),
                      "ON CLEAR NIGHTS, A SECOND HUM REPEATS IN THE EAST.") == 0,
          "epilogue lore preserves the later observation mystery");
    Check(std::strcmp(lc::LoreText(35), "IF ANOTHER RELAY IS STILL LISTENING, IT HAS NOT ANSWERED YET.") == 0,
          "epilogue lore text ends without promising a new quest");
    Check(lc::LoreText(-1)[0] == '\0' && lc::LoreText(36)[0] == '\0',
          "invalid lore indices return empty text");
    for (int lore = 0; lore < 36; ++lore) {
        Check(lc::LoreText(lore, lc::Language::Korean)[0] != '\0',
              "every lore line has a Korean translation");
    }
    Check(std::strcmp(lc::LoreText(3, lc::Language::Korean),
                      u8"사라진 시간을 가장 먼저 헤아린 것은 들판이었다.") == 0,
          "revised lore keeps an approved Korean counterpart");
    Check(lc::LoreText(-1, lc::Language::Korean)[0] == '\0' &&
              lc::LoreText(36, lc::Language::Korean)[0] == '\0',
          "localized lore rejects invalid indices");

    lc::GameState relayOne = {};
    lc::InitializeForTests(relayOne, 8, 8);
    relayOne.relaysPowered = 1;
    bool campaignSeen[6] = {};
    for (int step = 0; step < 6; ++step) {
        const int lore = lc::ChooseNextLore(relayOne);
        Check(lore >= 6 && lore < 12, "relay one uses campaign pool B");
        const int local = lore - 6;
        Check(!campaignSeen[local], "campaign lore does not repeat before pool is exhausted");
        campaignSeen[local] = true;
        lc::MarkLoreSeen(relayOne, lore);
    }

    lc::GameState epilogue = {};
    lc::InitializeForTests(epilogue, 8, 8);
    epilogue.completed = true;
    bool epilogueSeen[12] = {};
    for (int step = 0; step < 12; ++step) {
        const int lore = lc::ChooseNextLore(epilogue);
        Check(lore >= 24 && lore < 36, "completed campaign uses epilogue lore");
        const int local = lore - 24;
        Check(!epilogueSeen[local], "epilogue lore does not repeat before all twelve lines");
        epilogueSeen[local] = true;
        lc::MarkLoreSeen(epilogue, lore);
    }
    const int resetLore = lc::ChooseNextLore(epilogue);
    Check(resetLore >= 24 && resetLore < 36,
          "epilogue lore resets only after the twelve-line cycle");
}

void RelaysRequireAnIntactPathToTheBase() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 10, 10);
    lc::SetBase(game, 0, 1);
    lc::SetRelay(game, 0, 4, 1);
    game.pathInventory = 6;
    lc::PlacePath(game, 1, 1);
    lc::PlacePath(game, 2, 1);
    lc::PlacePath(game, 3, 1);

    lc::RefreshRelayWiring(game);
    Check(game.relaysConnected == 1, "intact installed path physically connects adjacent relay");
    Check(game.relayConnected[0], "the connected relay identity is retained");
    Check(game.relaysPowered == 0 && !game.relayPowered[0],
          "uncharged circuit does not power relay light or energy bonus");

    lc::ChargeConnectedPaths(game);
    lc::RefreshRelayWiring(game);
    Check(game.relaysConnected == 1 && game.relaysPowered == 1,
          "daylight promotes a connected relay to powered state");
    Check(game.relayConnected[0] && game.relayPowered[0],
          "physical and powered relay identities are independent");

    lc::DamagePath(game, 2, 1);
    lc::RefreshRelayWiring(game);
    Check(game.relaysConnected == 0 && game.relaysPowered == 0,
          "broken path disconnects and depowers relay");
    Check(!game.relayConnected[0] && !game.relayPowered[0],
          "broken relay branch clears both state identities");
}

void BaseEnergyBonusRequiresPoweredRelay() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    lc::SetRelay(game, 0, 3, 1);
    game.pathInventory = 4;
    lc::PlacePath(game, 1, 1);
    lc::PlacePath(game, 2, 1);
    lc::RefreshRelayWiring(game);

    Check(game.relaysConnected == 1 && game.relaysPowered == 0,
          "night-built circuit is connected before it is powered");
    Check(game.baseEnergyMax == 240.0f,
          "physical connection alone grants no base energy bonus");

    lc::BeginNight(game);
    Check(game.relaysPowered == 1, "daylight powers the connected relay");
    Check(game.baseEnergyMax == 300.0f && game.baseEnergy == 300.0f,
          "only powered relay grants base energy bonus");
}

void ChargeNodesIncludeBasePoweredPathAndPoweredRelays() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    lc::SetRelay(game, 0, 3, 1);
    game.pathInventory = 4;
    lc::PlacePath(game, 1, 1);
    lc::PlacePath(game, 2, 1);
    lc::RefreshRelayWiring(game);

    Check(lc::IsChargeNode(game, 0, 1), "base is always a manual charge node");
    Check(!lc::IsChargeNode(game, 3, 1), "connected but unpowered relay is not charged yet");

    lc::ChargeConnectedPaths(game);
    lc::RefreshRelayWiring(game);

    Check(lc::IsChargeNode(game, 1, 1), "powered path is a manual charge node");
    Check(lc::IsChargeNode(game, 3, 1), "powered relay is a manual charge node");
}

void NearestResourceSignalHonorsRange() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 16, 16);
    lc::AddResource(game, lc::ResourceKind::Water, 8, 8);
    lc::AddResource(game, lc::ResourceKind::Food, 3, 2);

    Check(lc::FindNearestResource(game, 2.0f, 2.0f, 5.0f) == 1,
          "nearest in-range resource is selected");
    Check(lc::FindNearestResource(game, 0.0f, 0.0f, 1.0f) == -1,
          "resource outside signal range is hidden");
    Check(lc::CollectResource(game, 1), "nearby resource can be collected");
    Check(game.carried[0] == 1 && !game.resources[1].active,
          "collection moves resource into carried inventory");

    lc::GameState centered = {};
    lc::InitializeForTests(centered, 10, 10);
    lc::AddResource(centered, lc::ResourceKind::Food, 4, 4);
    Check(lc::FindNearestResource(centered, 3.0f, 4.5f, 1.25f) == -1,
          "resource signal range is measured from the visible tile center");
    Check(lc::FindNearestResource(centered, 3.0f, 4.5f, 1.5f) == 0,
          "resource signal includes the exact center-based range boundary");
}

void InteractionUsesVisibleTargetCentersAndInclusiveRanges() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 12, 12);
    lc::SetBase(game, 8, 8);
    lc::AddResource(game, lc::ResourceKind::Food, 2, 2);
    game.paths[6 + 4 * game.width] = lc::PathState::Broken;
    game.pathInventory = 2;

    lc::Interaction action = lc::ResolveInteraction(game, 1.0f, 2.5f);
    Check(action.kind == lc::InteractionKind::Take && action.resourceIndex == 0,
          "resource is actionable exactly one-and-a-half tiles from its visible center");

    action = lc::ResolveInteraction(game, 4.999f, 4.5f);
    Check(action.kind == lc::InteractionKind::None,
          "broken path beyond one-and-a-half tiles is not actionable");
    action = lc::ResolveInteraction(game, 5.0f, 4.5f);
    Check(action.kind == lc::InteractionKind::Repair && action.pathX == 6 && action.pathY == 4,
          "broken path is actionable at its inclusive center-based range boundary");

    action = lc::ResolveInteraction(game, 7.25f, 8.5f);
    Check(action.kind == lc::InteractionKind::Sleep,
          "base sleep is actionable from outside the two-by-two base footprint");
    Check(lc::IsWithinBaseDepositRange(game, 9.75f, 8.0f),
          "automatic deposit uses the same inclusive base range");
    Check(!lc::IsWithinBaseDepositRange(game, 9.751f, 8.0f),
          "automatic deposit rejects positions beyond its base range");
}

void InteractionChoosesTheNearestCandidateOnly() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 12, 12);
    lc::SetBase(game, 7, 5);
    lc::AddResource(game, lc::ResourceKind::Water, 5, 5);
    game.paths[4 + 5 * game.width] = lc::PathState::Broken;
    game.pathInventory = 2;

    const lc::Interaction action = lc::ResolveInteraction(game, 5.4f, 5.5f);
    Check(action.kind == lc::InteractionKind::Take && action.resourceIndex == 0,
          "closest candidate wins even when repair and sleep ranges overlap");
}

void InteractionUsesSafePriorityForEffectiveTies() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 12, 12);
    lc::SetBase(game, 3, 2);
    lc::AddResource(game, lc::ResourceKind::Food, 1, 2);
    game.paths[3 + 2 * game.width] = lc::PathState::Broken;
    game.pathInventory = 2;

    lc::Interaction action = lc::ResolveInteraction(game, 1.9f, 2.5f);
    Check(action.kind == lc::InteractionKind::Take,
          "strictly closer take candidate wins even when base sleep is available");

    game.paths[3 + 2 * game.width] = lc::PathState::Empty;
    action = lc::ResolveInteraction(game, 1.5f, 2.5f);
    Check(action.kind == lc::InteractionKind::Take,
          "take wins when the resource is closer than base sleep");
}

void InteractionOmitsRepairsThatCannotBeExecuted() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.paths[3 + 3 * game.width] = lc::PathState::Broken;

    const lc::Interaction action = lc::ResolveInteraction(game, 3.5f, 3.5f);
    Check(action.kind == lc::InteractionKind::None,
          "broken path is not advertised when no repair tile is available");
}

void DepositConvertsCarriedResourcesAtBase() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.food = 2;
    game.water = 3;
    game.carried[0] = 2;
    game.carried[1] = 1;
    game.carried[2] = 20;

    lc::DepositAtBase(game);

    Check(game.food == 4 && game.water == 4, "food and water enter survival stores");
    Check(game.pathInventory == 20, "path material units enter path inventory");
    Check(game.carried[0] == 0 && game.carried[1] == 0 && game.carried[2] == 0,
          "deposit empties carried resources");
}

void StorageTransfersOneResourceAtATime() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.food = 1;
    game.water = 1;
    game.pathInventory = 1;
    game.carried[static_cast<int>(lc::ResourceKind::Food)] = 1;
    game.carried[static_cast<int>(lc::ResourceKind::Water)] = 1;
    game.carried[static_cast<int>(lc::ResourceKind::Material)] = 1;

    Check(lc::MoveOneResourceToBase(game, lc::ResourceKind::Food),
          "storage can deposit one carried food");
    Check(game.food == 2 && game.carried[static_cast<int>(lc::ResourceKind::Food)] == 0,
          "food deposit moves exactly one unit");
    Check(lc::MoveOneResourceToCarry(game, lc::ResourceKind::Water),
          "storage can withdraw one water");
    Check(game.water == 0 && game.carried[static_cast<int>(lc::ResourceKind::Water)] == 2,
          "water withdraw moves exactly one unit");
    Check(lc::MoveOneResourceToBase(game, lc::ResourceKind::Material),
          "storage can deposit one carried path material unit");
    Check(game.pathInventory == 2 &&
              game.carried[static_cast<int>(lc::ResourceKind::Material)] == 0,
          "path deposit moves exactly one material unit");
    Check(lc::MoveOneResourceToCarry(game, lc::ResourceKind::Material),
          "storage can withdraw one path material unit");
    Check(game.pathInventory == 1 &&
              game.carried[static_cast<int>(lc::ResourceKind::Material)] == 1,
          "path withdraw moves exactly one material unit");
}

void MaterialCacheProvidesTwentyPathMaterialUnits() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::AddResource(game, lc::ResourceKind::Material, 2, 2);

    lc::CollectResource(game, 0);
    lc::DepositAtBase(game);

    Check(game.pathInventory == 20, "one material cache becomes twenty path material units");
}

int CountBroken(const lc::GameState& game) {
    int count = 0;
    for (int index = 0; index < game.width * game.height; ++index) {
        if (game.paths[index] == lc::PathState::Broken) ++count;
    }
    return count;
}

lc::GameState PoweredDamageFixture(std::uint32_t seed) {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    lc::SetBase(game, 0, 1);
    lc::SetRelay(game, 0, 4, 1);
    game.pathInventory = 6;
    lc::PlacePath(game, 1, 1);
    lc::PlacePath(game, 2, 1);
    lc::PlacePath(game, 3, 1);
    lc::ChargeConnectedPaths(game);
    lc::RefreshRelayWiring(game);
    game.phase = lc::GamePhase::Night;
    game.food = 3;
    game.water = 3;
    game.randomState = seed;
    return game;
}

void SleepingDamageIsDeterministicHalfChance() {
    lc::GameState damaged = PoweredDamageFixture(0u);
    lc::SleepAtBase(damaged);
    Check(CountBroken(damaged) >= 1 && CountBroken(damaged) <= 2,
          "damage-side deterministic roll breaks one or two outer powered paths after relay one");

    lc::GameState spared = PoweredDamageFixture(1u);
    lc::SleepAtBase(spared);
    Check(CountBroken(spared) == 0,
          "safe-side deterministic roll leaves every powered path intact");
}

void BoarDropsOnlyAResourceThePlayerCarries() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 8, 8);
    game.carried[1] = 2;
    game.randomState = 7;

    const int dropped = lc::DropRandomCarriedResource(game);

    Check(dropped == 1, "boar selects an actually carried resource kind");
    Check(game.carried[1] == 1, "boar removes exactly one carried resource");
    Check(lc::DropRandomCarriedResource(game) == 1, "remaining carried resource can be dropped");
    Check(lc::DropRandomCarriedResource(game) == -1, "boar drops nothing from empty inventory");
}

void FixedWorldUsesAThirtyDegreeConstellationLayout() {
    lc::GameState game = {};
    lc::InitializeGame(game);

    Check(game.width == 256 && game.height == 256, "fixed world is two-hundred-fifty-six tiles square");
    Check(game.baseX == 128 && game.baseY == 128, "Acrux base is centered in the wider map");
    Check(game.relayCount == 3, "fixed world contains three solar relays");
    Check(game.relayX[0] != game.baseX && game.relayY[0] != game.baseY,
          "constellation long axis is rotated off cardinal directions");
    for (int relay = 0; relay < game.relayCount; ++relay) {
        const int distance = std::abs(game.relayX[relay] - game.baseX) +
                             std::abs(game.relayY[relay] - game.baseY);
        Check(distance >= 64, "wider world relays are not clustered beside the base");
    }
    Check(game.resourceCount >= 96, "wider world contains four times the resource goals");
    Check(game.pathInventory == 32, "fixed world starts with enough material for sixteen path tiles");
}

int CountActiveResources(const lc::GameState& game, lc::ResourceKind kind) {
    int count = 0;
    for (int index = 0; index < game.resourceCount; ++index) {
        if (game.resources[index].active && game.resources[index].kind == kind) ++count;
    }
    return count;
}

void ResourceZonesAreBalancedButDistinct() {
    lc::GameState game = {};
    lc::InitializeGame(game);
    int totals[3] = {};
    int kinds[3][lc::kResourceKinds] = {};
    for (int index = 0; index < game.resourceCount; ++index) {
        const lc::ResourceNode& resource = game.resources[index];
        const int zone = static_cast<int>(resource.zone);
        ++totals[zone];
        ++kinds[zone][static_cast<int>(resource.kind)];
    }

    Check(totals[0] == totals[1] && totals[1] == totals[2],
          "three relay resource zones begin with equal opportunities");
    Check(game.resourceCount == 120 && kinds[0][0] + kinds[1][0] + kinds[2][0] == 32 &&
              kinds[0][1] + kinds[1][1] + kinds[2][1] == 32 &&
              kinds[0][2] + kinds[1][2] + kinds[2][2] == 56,
          "authored world begins with four times the previous resource population");
    Check(kinds[static_cast<int>(lc::ResourceZone::Food)][0] >
              kinds[static_cast<int>(lc::ResourceZone::Food)][1] &&
          kinds[static_cast<int>(lc::ResourceZone::Food)][0] >
              kinds[static_cast<int>(lc::ResourceZone::Food)][2],
          "food zone has food as its unique specialty");
    Check(kinds[static_cast<int>(lc::ResourceZone::Water)][1] >
              kinds[static_cast<int>(lc::ResourceZone::Water)][0] &&
          kinds[static_cast<int>(lc::ResourceZone::Water)][1] >
              kinds[static_cast<int>(lc::ResourceZone::Water)][2],
          "water zone has water as its unique specialty");
    Check(kinds[static_cast<int>(lc::ResourceZone::Path)][2] >
              kinds[static_cast<int>(lc::ResourceZone::Path)][0] &&
          kinds[static_cast<int>(lc::ResourceZone::Path)][2] >
              kinds[static_cast<int>(lc::ResourceZone::Path)][1],
          "path zone has path material as its unique specialty");

    bool hasNear[3] = {};
    bool hasFar[3] = {};
    bool hasFarPair[3] = {};
    bool candidatesValid = game.resourceCandidateCount > game.resourceCount;
    for (int index = 0; index < game.resourceCandidateCount; ++index) {
        const lc::ResourceCandidate& candidate = game.resourceCandidates[index];
        const int tile = candidate.y * game.width + candidate.x;
        candidatesValid = candidatesValid && candidate.x >= 0 && candidate.y >= 0 &&
            candidate.x < game.width && candidate.y < game.height &&
            game.terrain[tile] == lc::Terrain::Ground;
        const int zone = static_cast<int>(candidate.zone);
        hasNear[zone] = hasNear[zone] || candidate.tier == lc::DistanceTier::Near;
        hasFar[zone] = hasFar[zone] || candidate.tier == lc::DistanceTier::Far;
        if (candidate.tier == lc::DistanceTier::Far) {
            for (int other = index + 1; other < game.resourceCandidateCount; ++other) {
                const lc::ResourceCandidate& pair = game.resourceCandidates[other];
                const int separation = std::abs(candidate.x - pair.x) +
                    std::abs(candidate.y - pair.y);
                if (pair.zone == candidate.zone && pair.tier == lc::DistanceTier::Far &&
                    separation <= 3) hasFarPair[zone] = true;
            }
        }
    }
    Check(candidatesValid, "resource candidates are extra valid ground positions");
    Check(hasNear[0] && hasNear[1] && hasNear[2] && hasFar[0] && hasFar[1] && hasFar[2],
          "every resource zone has near and far candidate positions");
    Check(hasFarPair[0] && hasFarPair[1] && hasFarPair[2],
          "every resource zone offers a compact far-distance reward pair");
}

void DailyResourcesMoveDeterministicallyAndRegenerateBelowConsumption() {
    lc::GameState first = {};
    lc::InitializeGame(first);
    lc::GameState same = first;
    int initialX[lc::kMaxResources] = {};
    int initialY[lc::kMaxResources] = {};
    for (int index = 0; index < first.resourceCount; ++index) {
        initialX[index] = first.resources[index].x;
        initialY[index] = first.resources[index].y;
    }

    lc::BeginNight(first);
    bool firstNightUnchanged = true;
    for (int index = 0; index < first.resourceCount; ++index) {
        firstNightUnchanged = firstNightUnchanged && first.resources[index].x == initialX[index] &&
            first.resources[index].y == initialY[index];
    }
    Check(firstNightUnchanged, "first night preserves the authored resource layout");

    first.phase = lc::GamePhase::Day;
    same.phase = lc::GamePhase::Day;
    first.day = 2;
    same.day = 2;
    first.lastResourceRefreshDay = 1;
    same.lastResourceRefreshDay = 1;
    lc::RefreshResourcesForNight(first);
    lc::RefreshResourcesForNight(same);
    bool deterministic = first.resourceCount == same.resourceCount;
    bool atLeastOneMoved = false;
    bool uniqueAndValid = true;
    for (int index = 0; index < first.resourceCount; ++index) {
        const lc::ResourceNode& left = first.resources[index];
        const lc::ResourceNode& right = same.resources[index];
        deterministic = deterministic && left.x == right.x && left.y == right.y &&
            left.kind == right.kind && left.zone == right.zone && left.active == right.active;
        atLeastOneMoved = atLeastOneMoved || left.x != initialX[index] || left.y != initialY[index];
        if (!left.active) continue;
        const int tile = left.y * first.width + left.x;
        uniqueAndValid = uniqueAndValid && first.terrain[tile] == lc::Terrain::Ground &&
            first.paths[tile] == lc::PathState::Empty;
        for (int other = index + 1; other < first.resourceCount; ++other) {
            if (first.resources[other].active && left.x == first.resources[other].x &&
                left.y == first.resources[other].y) uniqueAndValid = false;
        }
    }
    Check(deterministic, "same day and state produce the same resource relocation");
    Check(atLeastOneMoved, "uncollected resources move within their zones on the next night");
    Check(uniqueAndValid, "relocated resources never overlap blocked tiles or one another");

    lc::GameState cycle = {};
    lc::InitializeGame(cycle);
    for (int index = 0; index < cycle.resourceCount; ++index) {
        if (cycle.resources[index].kind != lc::ResourceKind::Material) {
            cycle.resources[index].active = false;
        } else {
            cycle.resources[index].active = false;
        }
    }
    cycle.lastResourceRefreshDay = 1;
    for (int day = 2; day <= 7; ++day) {
        cycle.day = day;
        lc::RefreshResourcesForNight(cycle);
    }
    Check(CountActiveResources(cycle, lc::ResourceKind::Food) == 5,
          "six-day cycle regenerates five food below six days of consumption");
    Check(CountActiveResources(cycle, lc::ResourceKind::Water) == 5,
          "six-day cycle regenerates five water below six days of consumption");
    Check(CountActiveResources(cycle, lc::ResourceKind::Material) == 56,
          "path material restores its full authored population each night");

    lc::GameState path = {};
    lc::InitializeGame(path);
    int removed = 0;
    int removedX[6] = {};
    int removedY[6] = {};
    int removedIndex[6] = {};
    for (int index = 0; index < path.resourceCount && removed < 6; ++index) {
        if (path.resources[index].kind != lc::ResourceKind::Material) continue;
        removedX[removed] = path.resources[index].x;
        removedY[removed] = path.resources[index].y;
        removedIndex[removed] = index;
        path.resources[index].active = false;
        ++removed;
    }
    path.day = 2;
    path.lastResourceRefreshDay = 1;
    lc::RefreshResourcesForNight(path);
    bool allReturnedElsewhere = removed == 6;
    for (int removedSlot = 0; removedSlot < removed; ++removedSlot) {
        const lc::ResourceNode& restored = path.resources[removedIndex[removedSlot]];
        allReturnedElsewhere = allReturnedElsewhere && restored.active &&
            restored.kind == lc::ResourceKind::Material &&
            (restored.x != removedX[removedSlot] || restored.y != removedY[removedSlot]);
    }
    int restoredByZone[3] = {};
    bool restoredUnique = true;
    for (int index = 0; index < path.resourceCount; ++index) {
        const lc::ResourceNode& resource = path.resources[index];
        if (!resource.active || resource.kind != lc::ResourceKind::Material) continue;
        ++restoredByZone[static_cast<int>(resource.zone)];
        for (int other = index + 1; other < path.resourceCount; ++other) {
            const lc::ResourceNode& candidate = path.resources[other];
            if (candidate.active && resource.x == candidate.x && resource.y == candidate.y) {
                restoredUnique = false;
            }
        }
    }
    const int material = static_cast<int>(lc::ResourceKind::Material);
    const bool zonesAtCap = restoredByZone[0] == path.resourceCaps[0][material] &&
        restoredByZone[1] == path.resourceCaps[1][material] &&
        restoredByZone[2] == path.resourceCaps[2][material];
    Check(allReturnedElsewhere, "every collected path cache returns at a changed position next night");
    Check(zonesAtCap, "path respawn restores every resource zone to its authored cap");
    Check(restoredUnique, "respawned path material never overlaps another resource");
}

void AnimalsRespawnDeterministicallyInsideSafeZones() {
    lc::GameState first = {};
    lc::InitializeGame(first);
    lc::GameState same = first;
    lc::BeginNight(first);
    lc::BeginNight(same);
    int moths = 0;
    int boars = 0;
    bool safe = first.animalCount == 60 && first.animalSpawnCandidateCount > first.animalCount;
    for (int index = 0; index < first.animalCount; ++index) {
        const lc::Animal& animal = first.animals[index];
        moths += animal.kind == lc::AnimalKind::Moth ? 1 : 0;
        boars += animal.kind == lc::AnimalKind::Boar ? 1 : 0;
        const float baseDx = animal.x - (first.baseX + 0.5f);
        const float baseDy = animal.y - (first.baseY + 0.5f);
        const int tileX = static_cast<int>(animal.x);
        const int tileY = static_cast<int>(animal.y);
        const int tile = tileY * first.width + tileX;
        safe = safe && baseDx * baseDx + baseDy * baseDy >= 100.0f &&
            first.terrain[tile] == lc::Terrain::Ground && !first.powered[tile] &&
            animal.x == same.animals[index].x && animal.y == same.animals[index].y;
    }
    Check(moths == 36 && boars == 24, "each night has exactly thirty-six moths and twenty-four boars");
    Check(safe, "animal spawn is deterministic and excludes base obstacles and powered paths");

    float oldX[lc::kMaxAnimals] = {};
    float oldY[lc::kMaxAnimals] = {};
    for (int index = 0; index < first.animalCount; ++index) {
        oldX[index] = first.animals[index].x;
        oldY[index] = first.animals[index].y;
    }
    first.phase = lc::GamePhase::Day;
    first.day = 2;
    lc::BeginNight(first);
    bool allMoved = true;
    for (int index = 0; index < first.animalCount; ++index) {
        allMoved = allMoved && (first.animals[index].x != oldX[index] ||
                                first.animals[index].y != oldY[index]);
    }
    Check(allMoved, "animals use different candidates on consecutive nights");
}

void MothUsesReadableTimedStatesAndBlinkAfterimage() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 16, 16);
    lc::SetBase(game, 0, 0);
    game.phase = lc::GamePhase::Night;
    game.personalLight = 100.0f;
    game.animalCount = 1;
    lc::Animal& moth = game.animals[0];
    moth.active = true;
    moth.kind = lc::AnimalKind::Moth;
    moth.mode = lc::AnimalMode::Idle;
    moth.x = 4.5f;
    moth.y = 4.5f;
    moth.homeX = moth.x;
    moth.homeY = moth.y;

    lc::AnimalInput input = {7.5f, 4.5f, 0.0f, 0.0f, false, false};
    lc::AnimalEvents events = {};
    lc::StepAnimals(game, input, 0.01f, events);
    Check(moth.mode == lc::AnimalMode::Alert && moth.timer > 0.5f,
          "moth gives a six-tenths alert before pursuing");
    lc::StepAnimals(game, input, 0.60f, events);
    Check(moth.mode == lc::AnimalMode::Pursue,
          "moth enters a bounded pursuit after its alert");
    const float pursuitStartY = moth.y;
    lc::StepAnimals(game, input, 0.50f, events);
    Check(moth.y != pursuitStartY, "moth pursuit curves instead of using a straight vector");
    lc::StepAnimals(game, input, 5.0f, events);
    Check(moth.mode != lc::AnimalMode::Pursue,
          "moth pursuit cannot continue beyond five seconds");

    moth.mode = lc::AnimalMode::Charge;
    moth.x = 7.2f;
    moth.y = 4.5f;
    moth.vx = 1.0f;
    moth.vy = 0.0f;
    moth.timer = 0.5f;
    lc::StepAnimals(game, input, 0.01f, events);
    Check(game.personalLight == 85.0f && events.personalLightLost == 15.0f &&
              moth.mode == lc::AnimalMode::Retreat,
          "moth charge contact drains fifteen light once then retreats");
    lc::StepAnimals(game, input, 0.5f, events);
    Check(game.personalLight == 85.0f,
          "retreating moth cannot drain light repeatedly");

    moth.mode = lc::AnimalMode::Pursue;
    moth.x = 6.0f;
    moth.y = 4.5f;
    lc::AnimalInput safeInput = input;
    safeInput.playerSafe = true;
    lc::StepAnimals(game, safeInput, 0.01f, events);
    Check(moth.mode == lc::AnimalMode::Retreat,
          "moth abandons pursuit when the player reaches neon safety");

    moth.mode = lc::AnimalMode::Pursue;
    moth.x = 6.0f;
    moth.y = 4.5f;
    lc::ApplyBlinkToAnimals(game, input.playerX, input.playerY, 8.0f, 5.0f);
    Check(moth.mode == lc::AnimalMode::Stunned && moth.timer == 1.4f,
          "blink stuns a nearby moth for one-point-four seconds");
    input.afterimageActive = true;
    input.afterimageX = 8.0f;
    input.afterimageY = 5.0f;
    lc::StepAnimals(game, input, 1.4f, events);
    Check(moth.mode == lc::AnimalMode::Afterimage,
          "moth follows the blink afterimage after recovering");
    lc::StepAnimals(game, input, 1.0f, events);
    Check(moth.mode == lc::AnimalMode::Retreat,
          "afterimage diversion ends after one second");
}

void BoarTelegraphsLocksDirectionAndDropsOneResource() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 16, 16);
    lc::SetBase(game, 0, 0);
    game.phase = lc::GamePhase::Night;
    game.animalCount = 1;
    lc::Animal& boar = game.animals[0];
    boar.active = true;
    boar.kind = lc::AnimalKind::Boar;
    boar.mode = lc::AnimalMode::Idle;
    boar.zone = lc::ResourceZone::Water;
    boar.x = 4.5f;
    boar.y = 4.5f;
    boar.homeX = boar.x;
    boar.homeY = boar.y;

    lc::AnimalInput input = {7.5f, 4.5f, 0.0f, 0.0f, false, false};
    lc::AnimalEvents events = {};
    lc::StepAnimals(game, input, 0.01f, events);
    Check(boar.mode == lc::AnimalMode::Alert && boar.timer > 0.8f,
          "boar gives a nine-tenths direction warning without moving");
    input.playerX = 4.5f;
    input.playerY = 7.5f;
    lc::StepAnimals(game, input, 0.90f, events);
    Check(boar.mode == lc::AnimalMode::Charge && boar.vy > 0.0f,
          "boar locks its charge direction when the warning ends");
    const float lockedVx = boar.vx;
    const float lockedVy = boar.vy;
    input.playerX = 10.5f;
    input.playerY = 2.5f;
    lc::StepAnimals(game, input, 0.05f, events);
    Check(boar.vx == lockedVx && boar.vy == lockedVy,
          "boar cannot steer toward a moving player during its charge");

    boar.mode = lc::AnimalMode::Charge;
    boar.x = 4.5f;
    boar.y = 4.5f;
    boar.vx = 6.8f;
    boar.vy = 0.0f;
    boar.timer = 1.0f;
    game.terrain[4 * game.width + 5] = lc::Terrain::Rock;
    lc::StepAnimals(game, input, 0.2f, events);
    Check(boar.mode == lc::AnimalMode::Stunned && boar.timer == 1.5f,
          "boar is stunned for one-point-five seconds by an obstacle");

    game.terrain[4 * game.width + 5] = lc::Terrain::Ground;
    boar.mode = lc::AnimalMode::Alert;
    boar.x = 6.0f;
    boar.y = 4.5f;
    lc::ApplyBlinkToAnimals(game, 7.5f, 4.5f, 7.5f, 4.5f);
    Check(boar.mode == lc::AnimalMode::Stunned,
          "blink cancels a boar warning before the charge");
    boar.mode = lc::AnimalMode::Charge;
    lc::ApplyBlinkToAnimals(game, 7.5f, 4.5f, 7.5f, 4.5f);
    Check(boar.mode == lc::AnimalMode::Stunned,
          "blink stops and stuns a charging boar");

    boar.mode = lc::AnimalMode::Charge;
    boar.x = 7.2f;
    boar.y = 4.5f;
    boar.vx = 1.0f;
    boar.vy = 0.0f;
    boar.timer = 0.5f;
    input.playerX = 7.5f;
    input.playerY = 4.5f;
    game.carried[static_cast<int>(lc::ResourceKind::Water)] = 1;
    lc::StepAnimals(game, input, 0.01f, events);
    Check(events.droppedResourceKind == static_cast<int>(lc::ResourceKind::Water) &&
              game.carried[static_cast<int>(lc::ResourceKind::Water)] == 0,
          "boar collision drops exactly one actually carried resource");
    Check(CountActiveResources(game, lc::ResourceKind::Water) == 1 &&
              game.resources[0].zone == lc::ResourceZone::Water &&
              boar.mode == lc::AnimalMode::Retreat,
          "boar drop remains recoverable in its resource zone while the boar retreats");
}

void AnimalContactSweepsMotionAndBoarBreaksOnePoweredPath() {
    lc::GameState mothGame = {};
    lc::InitializeForTests(mothGame, 12, 12);
    mothGame.phase = lc::GamePhase::Night;
    mothGame.personalLight = 100.0f;
    mothGame.animalCount = 1;
    lc::Animal& moth = mothGame.animals[0];
    moth.active = true;
    moth.kind = lc::AnimalKind::Moth;
    moth.mode = lc::AnimalMode::Charge;
    moth.x = 2.5f;
    moth.y = 5.5f;
    moth.vx = 10.0f;
    moth.timer = 1.0f;
    lc::AnimalInput input = {5.5f, 5.5f, 0.0f, 0.0f, false, false};
    lc::AnimalEvents events = {};
    lc::StepAnimals(mothGame, input, 0.5f, events);
    Check(events.personalLightLost == 15.0f && mothGame.personalLight == 85.0f,
          "swept collision catches a moth crossing the player between frames");
    lc::StepAnimals(mothGame, input, 0.1f, events);
    Check(mothGame.personalLight == 85.0f,
          "same animal cannot damage again during its one-second cooldown");

    lc::GameState boarGame = {};
    lc::InitializeForTests(boarGame, 12, 12);
    lc::SetBase(boarGame, 4, 3);
    boarGame.pathInventory = 2;
    lc::PlacePath(boarGame, 4, 4);
    lc::ChargeConnectedPaths(boarGame);
    boarGame.phase = lc::GamePhase::Night;
    boarGame.animalCount = 1;
    lc::Animal& boar = boarGame.animals[0];
    boar.active = true;
    boar.kind = lc::AnimalKind::Boar;
    boar.mode = lc::AnimalMode::Charge;
    boar.x = 2.5f;
    boar.y = 4.5f;
    boar.vx = 8.0f;
    boar.timer = 1.0f;
    lc::AnimalInput distant = {10.5f, 10.5f, 0.0f, 0.0f, false, false};
    lc::StepAnimals(boarGame, distant, 0.5f, events);
    Check(boarGame.paths[4 + 4 * boarGame.width] == lc::PathState::Broken,
          "boar charge breaks the first powered path tile it meets");
    Check(events.pathBroken && events.pathBreakX == 4 && events.pathBreakY == 4,
          "boar charge reports the visible runtime path break for audio");
    Check(boar.mode == lc::AnimalMode::Stunned && boar.timer == 1.5f,
          "boar stops before the broken path and enters a one-point-five-second stun");
}

void TrapsWarnOrSlowWithoutDirectDamageAndResetNightly() {
    lc::GameState game = {};
    lc::InitializeForTests(game, 20, 20);
    lc::SetBase(game, 1, 1);
    Check(lc::AddTrap(game, lc::TrapKind::Rustle, 8, 8), "valid ground accepts a rustle trap");
    Check(lc::AddTrap(game, lc::TrapKind::Snare, 10, 8), "valid ground accepts a snare trap");
    game.phase = lc::GamePhase::Night;
    game.personalLight = 100.0f;
    game.animalCount = 1;
    game.animals[0].active = true;
    game.animals[0].kind = lc::AnimalKind::Moth;
    game.animals[0].mode = lc::AnimalMode::Idle;
    game.animals[0].x = 9.5f;
    game.animals[0].y = 8.5f;
    lc::TrapEvents events = {};
    lc::StepTraps(game, 8.5f, 8.5f, 0.01f, events);
    Check(events.rustleTriggered && game.animals[0].mode == lc::AnimalMode::Alert,
          "rustle alerts nearby idle animals once");
    Check(game.personalLight == 100.0f, "rustle causes no direct light damage");
    lc::StepTraps(game, 8.5f, 8.5f, 0.01f, events);
    Check(!events.rustleTriggered, "rustle cannot retrigger in the same night");

    lc::StepTraps(game, 10.5f, 8.5f, 0.01f, events);
    Check(events.snareTriggered && game.snareTimer == 0.6f,
          "snare applies a six-tenths-second slow timer");
    Check(game.traps[1].triggered, "snare leaves triggered state for visible feedback");
    Check(game.personalLight == 100.0f, "snare causes no direct light damage");
    lc::ResetTrapsForNight(game);
    Check(!game.traps[0].triggered && game.traps[1].cooldown == 0.0f,
          "trap activation state resets for the next night");

    lc::GameState world = {};
    lc::InitializeGame(world);
    int rustles = 0;
    int snares = 0;
    bool valid = world.trapCount == lc::kMaxTraps;
    for (int index = 0; index < world.trapCount; ++index) {
        const lc::Trap& trap = world.traps[index];
        rustles += trap.kind == lc::TrapKind::Rustle ? 1 : 0;
        snares += trap.kind == lc::TrapKind::Snare ? 1 : 0;
        valid = valid && world.terrain[trap.y * world.width + trap.x] == lc::Terrain::Ground;
        for (int resource = 0; resource < world.resourceCount; ++resource) {
            valid = valid && (!world.resources[resource].active ||
                world.resources[resource].x != trap.x || world.resources[resource].y != trap.y);
        }
    }
    Check(rustles == 80 && snares == 64, "fixed world contains eighty rustles and sixty-four snares");
    Check(valid, "fixed traps avoid blocked terrain and initial resources");
}

int BuildGreedyCircuit(const lc::GameState& source, const int order[3]) {
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};
    bool network[lc::kMaxTiles] = {};
    network[source.baseY * source.width + source.baseX] = true;
    int used = 0;

    for (int step = 0; step < 3; ++step) {
        const int relay = order[step];
        int queue[lc::kMaxTiles] = {};
        int previous[lc::kMaxTiles];
        int read = 0;
        int write = 0;
        std::fill(previous, previous + lc::kMaxTiles, -2);
        for (int index = 0; index < source.width * source.height; ++index) {
            if (network[index]) {
                queue[write++] = index;
                previous[index] = -1;
            }
        }
        int target = -1;
        while (read < write && target < 0) {
            const int index = queue[read++];
            const int x = index % source.width;
            const int y = index / source.width;
            if (std::abs(x - source.relayX[relay]) + std::abs(y - source.relayY[relay]) == 1) {
                target = index;
                break;
            }
            for (int direction = 0; direction < 4; ++direction) {
                const int nx = x + kDx[direction];
                const int ny = y + kDy[direction];
                if (nx < 0 || ny < 0 || nx >= source.width || ny >= source.height) continue;
                const int next = ny * source.width + nx;
                if (previous[next] != -2) continue;
                if (source.terrain[next] != lc::Terrain::Ground) continue;
                previous[next] = index;
                queue[write++] = next;
            }
        }
        if (target < 0) return lc::kMaxTiles;
        for (int index = target; index >= 0 && !network[index]; index = previous[index]) {
            network[index] = true;
            ++used;
        }
    }
    return used;
}

void FixedWorldHasSixteenSparePathTiles() {
    lc::GameState game = {};
    lc::InitializeGame(game);
    int materialCaches = 0;
    for (int index = 0; index < game.resourceCount; ++index) {
        if (game.resources[index].kind == lc::ResourceKind::Material) ++materialCaches;
    }
    const int totalSupply = game.pathInventory + materialCaches * 20;
    Check(totalSupply == 1152, "fixed world offers eleven-hundred-fifty-two path material units");
    const int orders[6][3] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0},
    };
    int routeTiles = lc::kMaxTiles;
    for (const auto& order : orders) {
        routeTiles = std::min(routeTiles, BuildGreedyCircuit(game, order));
    }
    Check(routeTiles < lc::kMaxTiles, "fixed world has an obstacle-safe complete circuit");
    Check(totalSupply - routeTiles * 2 >= 32,
          "fixed world material supply leaves at least sixteen spare path tiles");
}

void SynthAudioCreatesTheApprovedRelayLayersAndSfx() {
    short quiet[512 * 2] = {};
    short connected[512 * 2] = {};
    Check(AcruxSynth_Init(), "v8 synth initializes without media files");
    AcruxSynth_ResetPlayback();
    AcruxSynth_SetRelayCount(0);
    AcruxSynth_RenderStereo(quiet, 512);
    AcruxSynth_ResetPlayback();
    AcruxSynth_SetRelayCount(3);
    AcruxSynth_RenderStereo(connected, 512);

    int nonZero = 0;
    int difference = 0;
    for (int index = 0; index < 512 * 2; ++index) {
        if (quiet[index] != 0) ++nonZero;
        if (quiet[index] != connected[index]) ++difference;
    }
    Check(nonZero > 800, "v8 relay-zero layer produces continuous stereo samples");
    Check(difference > 800, "relay-three layer changes the arrangement without media files");
    int peak = 0;
    for (short sample : connected) {
        const int magnitude = sample < 0 ? -static_cast<int>(sample) : static_cast<int>(sample);
        if (magnitude > peak) peak = magnitude;
    }
    Check(peak > 2500, "v8 arrangement is audible without extreme system volume");

    short sfx[4096 * 2] = {};
    AcruxSynth_SetMusicGain(0.0f);
    AcruxSynth_ResetPlayback();
    AcruxSynth_PlaySfx(ACRUX_SFX_BLINK);
    AcruxSynth_RenderStereo(sfx, 4096);
    int sfxNonZero = 0;
    for (short sample : sfx) {
        if (sample != 0) ++sfxNonZero;
    }
    Check(sfxNonZero > 2000, "approved Blink SFX renders as a synthesized one-shot");
    Check(AcruxSynth_LoopFrames() > ACRUX_AUDIO_SAMPLE_RATE * 60,
          "approved music loop lasts more than sixty seconds");

    AcruxSynth_SetMusicGain(0.62f);
    AcruxSynth_ResetPlayback();
    AcruxSynth_SetRelayCount(3);
    AcruxSynth_RenderStereo(connected, 512);
    AcruxSynth_EnterEpilogueImmediate(ACRUX_SEASON_WINTER);
    short winter[512 * 2] = {};
    short summer[512 * 2] = {};
    AcruxSynth_RenderStereo(winter, 512);
    Check(AcruxSynth_IsEpilogue() &&
          AcruxSynth_CurrentSeason() == ACRUX_SEASON_WINTER,
          "epilogue audio can enter winter immediately for continue");
    AcruxSynth_SetEpilogueSeasonImmediate(ACRUX_SEASON_SUMMER);
    AcruxSynth_RenderStereo(summer, 512);
    int seasonalDifference = 0;
    for (int index = 0; index < 512 * 2; ++index) {
        if (winter[index] != summer[index]) ++seasonalDifference;
    }
    Check(seasonalDifference > 600,
          "epilogue seasonal overlays produce distinct procedural audio");
    AcruxSynth_ResetPlayback();
    Check(!AcruxSynth_IsEpilogue(),
          "new game audio reset exits epilogue mode");
    AcruxSynth_Shutdown();
}

}  // namespace

int main() {
    LanguageSelectionTogglesBetweenEnglishAndKorean();
    EmptyGroundAcceptsAPathTile();
    DisconnectedPathCanBeInstalled();
    DaylightPowersOnlyBaseConnectedPath();
    BrokenSerialPathDarkensDownstream();
    ParallelRouteKeepsDownstreamPowered();
    ClosedPoweredPathsLightOnlyTheirEnclosedInterior();
    RemoveAndRepairUseInventory();
    BlinkSpendsLightAndStartsTimers();
    StabilizedLoopBoundaryResistsBoarCharge();
    BrokenPathRecoveryReturnsOnePathMaterialUnit();
    PoweredPathRequiresManualChargeInput();
    NightTimersLeaveRoomForExploration();
    PathPlacementPreparationTracksMovement();
    PathPlacementAllowsRemovalWithoutInventory();
    ChargedPathRemovalCanOnlyBeUndoneInPlaceThatNight();
    DarknessFailureLosesOnlyCarriedResources();
    BaseEnergyFailureHasADistinctReason();
    VoluntarySleepStartsARecoverableCrisisOrWins();
    CompletedCampaignSleepsIntoEpilogueWithoutRetriggeringEnding();
    SeasonStateStartsAfterCompletionOnly();
    BasePuddleIsDerivedAndBlocksTwoByTwoPathPlacement();
    BaseFootprintBlocksTwoByTwoGameplayTiles();
    RelayFootprintBlocksTwoByTwoGameplayTiles();
    LorePoolsDoNotRepeatBeforeTheirCycleEnds();
    RelaysRequireAnIntactPathToTheBase();
    BaseEnergyBonusRequiresPoweredRelay();
    ChargeNodesIncludeBasePoweredPathAndPoweredRelays();
    NearestResourceSignalHonorsRange();
    InteractionUsesVisibleTargetCentersAndInclusiveRanges();
    InteractionChoosesTheNearestCandidateOnly();
    InteractionUsesSafePriorityForEffectiveTies();
    InteractionOmitsRepairsThatCannotBeExecuted();
    DepositConvertsCarriedResourcesAtBase();
    StorageTransfersOneResourceAtATime();
    MaterialCacheProvidesTwentyPathMaterialUnits();
    SleepingDamageIsDeterministicHalfChance();
    BoarDropsOnlyAResourceThePlayerCarries();
    FixedWorldUsesAThirtyDegreeConstellationLayout();
    ResourceZonesAreBalancedButDistinct();
    DailyResourcesMoveDeterministicallyAndRegenerateBelowConsumption();
    AnimalsRespawnDeterministicallyInsideSafeZones();
    MothUsesReadableTimedStatesAndBlinkAfterimage();
    BoarTelegraphsLocksDirectionAndDropsOneResource();
    AnimalContactSweepsMotionAndBoarBreaksOnePoweredPath();
    TrapsWarnOrSlowWithoutDirectDamageAndResetNightly();
    FixedWorldHasSixteenSparePathTiles();
    SaveSnapshotRoundTripsPersistentNightState();
    SynthAudioCreatesTheApprovedRelayLayersAndSfx();
    if (failures != 0) {
        std::printf("%d test(s) failed.\n", failures);
        return 1;
    }
    std::puts("All game core tests passed.");
    return 0;
}
