#ifndef TERRAINSMOOTH_H
#define TERRAINSMOOTH_H

#include "TerrainGrid.h"

void SmoothTerrainRaise(int row, int col);
void SmoothTerrainLower(int row, int col);

extern short g_slopeLimit[5][5];

#endif
