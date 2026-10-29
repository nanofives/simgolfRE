#ifndef ISOVIEW_H
#define ISOVIEW_H

#include "CourseGrid.h"

enum {
    TILE_WORLD_SIZE = 1024,
    TILE_OFFSCREEN = -99
};

enum {
    ELEV_FLAT_ZERO = 8
};

extern int g_screenWidth;
extern int g_screenHeight;
extern int g_cameraTileX;
extern int g_cameraTileY;
extern int g_tileSpan;
extern int g_elevStep;
extern int g_zoomDouble;
extern short g_cellScreenPosX[GRID_SIZE][GRID_SIZE];
extern short g_cellScreenPosY[GRID_SIZE][GRID_SIZE];

int TileToScreenCached(int row, int col, int *px, int *py);
int GetTerrainHeightAtWorld(int wx, int wy);
int WorldToScreen(int wx, int wy, int *px, int *py, int margin);
void ScreenToTileCoords(int screenX, int screenY, int *pRow, int *pCol, int useWorld);

#endif
