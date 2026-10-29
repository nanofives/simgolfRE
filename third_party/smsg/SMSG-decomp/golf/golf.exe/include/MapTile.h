#ifndef MAPTILE_H
#define MAPTILE_H

#include "TerrainGrid.h"

extern int g_inSaleView;
extern char g_themeDesc[];

void TileToScreenCoords(int row, int col, int *px, int *py);

int GetTerrainPropertyAt(int row, int col);

#endif
