
#ifndef TERRAINGRID_H
#define TERRAINGRID_H

#include "Golfer.h"
#include "Geometry.h"
#include "GameFlags.h"

struct TerrainTypeDef {
    char name[0x20];
    signed char bounce;
    signed char rollShift;
    signed char lieGrade;
    signed char price;
    signed char clearCost;
    signed char walk;
    signed char category;
    signed char kind;
    signed char height[4];
    int flags;
};

struct Course {
    char name[0x18];
    short x;
    short y;
    signed char site;
    unsigned char acres;
    signed char hilly;
    signed char coast;
    signed char kind;
    char pad21;
    signed char yearBought;
    signed char yearLeft;
    signed char holeAwards;
    signed char homesSold;
    signed char ratingUnawarded;
    signed char tourPoints;
    signed char storyCount;
    char pad29[3];
    signed char moneyHi;
    signed char moneyLo;
};

void TerrainMoveTo(int x, int y);
void TerrainLineTo(int x, int y, unsigned color, int width, int alpha);
int IsPositionInvalid(int row, int col);
int GetTileAtPosition(int worldRow, int worldCol);
int GetCornerElevation(int row, int col, int corner, int useCache);
int GetTileElevation(int row, int col, int flags);
int GetTerrainRowSlope(int x, int y);
int GetTerrainColSlope(int x, int y);
int CalculateDirectionalOffset(int x, int y, int dir);
int CalculateDistanceInYards(int x, int y, int tileX, int tileY);

extern int g_linePenX;
extern int g_linePenY;

extern signed char g_gameTileTerrainKind[50 * 50];
extern signed char g_cornerElevation[50][50][8];
extern signed char g_flatSquareAltMap[50 * 50];
extern unsigned char g_pointElevationMap[][51];
extern TerrainTypeDef g_squareTypeTable[23];
extern Course g_mapCourseList[];
extern int g_level;
extern int g_myWorldCourseIdx;
extern int g_terrainHeightDivisor;

void DrawTerrainLine(int x0, int y0, int x1, int y1, unsigned color, int width, int alpha);
int CalculateTerrainHeight(int x, int y);
int Math_Distance2D(int dx, int dy);

#define TERRAIN_ROUGH                4
#define TERRAIN_BRUSH                0xb
#define TERRAIN_ROCKS                0xc
#define TERRAIN_TREE                 0xd
#define TERRAIN_PALM_TREE            0xf
#define TERRAIN_ELM_TREE             0x10
#define TERRAIN_WATER                0x11
#define TERRAIN_WETLANDS             0x12
#define TERRAIN_MARSH                0x13
#define TERRAIN_OUT_OF_BOUNDS        0x14
#define TERRAIN_HOME_SITE            0x15
#define TERRAIN_BUILDING             0x16
#define CATEGORY_FAIRWAY             2
#define CATEGORY_ROUGH               4
#define CATEGORY_SAND                7
#define CATEGORY_TREES               0xd
#define CATEGORY_OUT_OF_BOUNDS       0x12
#define CATEGORY_ROCKS               0xc
#define CATEGORY_LIKE_OUT_OF_BOUNDS  0x13
#define TERRAIN_GREEN                1
#define TERRAIN_DEEP_ROUGH           5
#define TERRAIN_SAND_TRAP            7
#define TERRAIN_RAVINE               10
#define ELEV_SEA_LEVEL               3
#define TERRAIN_FAIRWAY              2
#define TERRAIN_PINE_TREE            0xe

#endif
