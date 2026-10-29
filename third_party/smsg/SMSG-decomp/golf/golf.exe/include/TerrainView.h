#ifndef TERRAINVIEW_H
#define TERRAINVIEW_H

#include "Terrain.h"
#include "Screen.h"
#include "Plot.h"
#include "IsoView.h"
#include "MapTile.h"

extern Terrain *g_terrain;
extern TileList g_tileDrawQueue;
extern bool g_isWin2kOrLater;
extern bool g_terrainRebuildPending;
extern bool g_tilePathDirty;
extern bool g_terrainViewDirty;
extern int g_terrainViewWidth;
extern int g_terrainViewHeight;
extern int g_lastRot;
extern unsigned char g_needReinit;
extern int g_lastViewX;
extern int g_lastViewY;
extern int g_lastZoom;

int GetTerrainTypeAt(int row, int col);
int GetTerrainVariationAt(int row, int col);
int GetCornerElevationAt(int row, int col, int corner);
int GetTerrainWallHeight(int row, int col, int dir);
int IsTilePath(int row, int col);
void LoadCourseType(int courseType);
void SyncTerrainTypes();
void ResetAndRebuildTerrain();
void BuildFromGameData();
void InitTerrainSystem(int w, int h);
void ShutdownTerrain();
void UpdateTerrainView(int x, int y, int zoom, int rot);
bool UpdateTileTerrainType(Tile *t, int row, int col);
bool AdjustTileCornerElevations(Tile *t, int row, int col, int cx, int cy);
bool UpdateTileWalls(Tile *t, int row, int col);
bool UpdateTilePath(Tile *t, int row, int col);
void UpdateTerrainTilesInView(int cx, int cy);
void ScrollTerrainView(AssetSurface *surf, int x, int y, int dx, int dy, int rot, int zoom);

enum TerrainViewUnrecovered {
    TERRAIN_VIEW_UNRECOVERED_1,
    TERRAIN_VIEW_UNRECOVERED_2,
    TERRAIN_VIEW_UNRECOVERED_3,
    TERRAIN_VIEW_UNRECOVERED_4,
    TERRAIN_VIEW_UNRECOVERED_5,
    TERRAIN_VIEW_UNRECOVERED_6,
    TERRAIN_VIEW_UNRECOVERED_7
};

extern AssetSurface *g_terrainSurface;

#endif
