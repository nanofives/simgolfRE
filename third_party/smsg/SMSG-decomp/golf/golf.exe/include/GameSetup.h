#ifndef GAMESETUP_H
#define GAMESETUP_H

#include "FlicBanks.h"
#include "AssetSurface.h"

extern AssetSurface g_flicSurface;

extern int g_numHeads[2];

extern struct TerrainTypeDef g_terrainTypeDefaults[23];

extern short g_trophyPartPos[25][2];
extern short g_trophyPartSize[25][2];

extern int g_themeWritePaletteFresh;
extern short g_customizeSpots[][2];
extern short g_customizeDrawSpots[20][2];
extern int g_headSlots[10][2];

#endif
