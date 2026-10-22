#pragma once
#include <stdint.h>

enum AcruxSeason {
    ACRUX_SEASON_WINTER = 0,
    ACRUX_SEASON_SPRING,
    ACRUX_SEASON_SUMMER,
    ACRUX_SEASON_AUTUMN,
    ACRUX_SEASON_COUNT
};

bool AcruxEpilogue_Init(int loopFrames);
void AcruxEpilogue_Shutdown();
int16_t AcruxEpilogue_Sample(AcruxSeason season, int frame);
