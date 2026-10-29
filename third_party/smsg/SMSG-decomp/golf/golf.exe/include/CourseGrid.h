#ifndef COURSEGRID_H
#define COURSEGRID_H

enum {
    GRID_SIZE = 50
};

enum {
    TILE_REACHED = 0x40,
    TILE_WALKABLE = 0x420
};

#include "TerrainGrid.h"
#include "Plot.h"
#include "GameFlags.h"

enum {
    CATEGORY_WATER = 0x11,
    ELEV_PROPAGATE = 1,
    ELEV_FROM_MIN = 2,
    ELEV_FROM_MAX = 4
};

enum {
    BUILDING_SLOT_COUNT = 256,
    SCENERY_CLUBHOUSE = 15
};

extern short g_tileExtra[GRID_SIZE * GRID_SIZE];
extern signed char g_tileUphillMap[][GRID_SIZE];
extern unsigned char g_tileVariationIdx[GRID_SIZE * GRID_SIZE];
extern int g_editedTileRowIdx;

void FloodFillWalkableArea(int row, int col);
void UpdateWaterShoreDepth(int row, int col);
void RecalculateAllWaterShoreDepths();
void UpdateBuildingAccessibility();
void GetTileElevationMinMax(int row, int col, int *pMax, int *pMin);
void CalculateTileSlopeFlags(int row, int col);
int PropagateMinElevation(int row, int col);
int PropagateMaxElevation(int row, int col, int sameVariation);
void RebuildTerrainElevation();

#define TILE_HOLE_MASK               0x1f
#define TILE_FACILITY_MASK           0x1f
#define TILE_PATH                    0x20
#define TILE_PIN                     0x80
#define TILE_SCENIC                  0x100
#define TILE_BENCH                   0x200
#define TILE_BUILDING                0x400
#define TILE_WEEDS                   0x800
#define TILE_FLOWERS                 0x1000
#define TILE_NAMED                   0x2000
#define TILE_ANIMATING               0x4000
#define TILE_PERMANENT               0x8000
#define VIEW_TILES_SYNCED            2
#define VIEW_ELEVATION_REBUILT       8

#endif
