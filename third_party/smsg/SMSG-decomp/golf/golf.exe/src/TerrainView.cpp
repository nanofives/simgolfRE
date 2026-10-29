#define TILE_REVISION 2
#include "jgldefs.h"
#include <math.h>
#include <stdlib.h>
#include <windows.h>
#include "Screen.h"
#include "TerrainView.h"
#include "Sprite.h"

inline Tile *Terrain::tileAt(int x, int y)
{
    if (x >= m_width || x < 0 || y >= m_height || y < 0)
        return 0;
    return &m_tiles[m_width * y + x];
}

inline int Terrain::getElevation(Tile *pTile, int nCorner)
{
    if (pTile)
        return pTile->getCornerElevation(nCorner);
    else
        return 0;
}

inline bool Terrain::getWall(Tile *pTile, int nDir)
{
    return pTile->getWallEnabled(nDir);
}

inline int Terrain::getType(Tile *pTile)
{
    return pTile->getType();
}

Terrain *g_terrain = Terrain::getInstance();
int g_constTwo = (int)fabs(-2.0);
SpriteObject g_terrainSprite;
TileList g_tileDrawQueue;
TileIter g_tileDrawIter;

static int s_unused3[3];
static int s_unusedInt;

bool g_isWin2kOrLater = false;
static bool s_unusedBool = false;
bool g_terrainRebuildPending = false;
bool g_tilePathDirty = false;
bool g_terrainViewDirty = false;
int g_terrainViewWidth = 0;
int g_terrainViewHeight = 0;
int g_lastRot = 0;

unsigned char g_needReinit = 1;
int g_lastViewX = -1;
int g_lastViewY = -1;
int g_lastZoom = 4;

static inline float RotationToDegrees(int rot)
{
    float deg;

    switch (rot) {
    case 0: deg = 0.0f; break;
    case 2: deg = 90.0f; break;
    case 4: deg = 180.0f; break;
    case 6: deg = -90.0f; break;
    default: deg = 0.0f; break;
    }
    return deg;
}

int GetTerrainTypeAt(int row, int col)
{
    return g_gameTileTerrainKind[row * 50 + col];
}

int GetTerrainVariationAt(int row, int col)
{
    return g_tileVariationIdx[row * 50 + col];
}

int GetCornerElevationAt(int row, int col, int corner)
{
    return GetCornerElevation(row, col, corner, 1);
}

int GetTerrainWallHeight(int row, int col, int dir)
{
    signed char m = g_tileUphillMap[row][col];
    if (!((1 << dir) & m))
        return 0;
    signed char h = g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].height[0];
    if (h)
        return h;
    return g_squareTypeTable[g_gameTileTerrainKind[(g_dirRowStep[dir] + row) * 50 + g_dirColStep[dir] + col]].height[0];
}

int IsTilePath(int row, int col)
{
    return g_tileExtra[row * 50 + col] & TILE_PATH;
}

Tile *TerrainTileHit(int x, int y)
{
    return g_terrain->tileHit(x, y);
}

void DrawTerrainLine(int x0, int y0, int x1, int y1, unsigned color, int width, int alpha)
{
    g_terrain->drawLine(x0, y0, x1, y1, color, width, alpha);
}

void DrawTerrainSpline(int x0, int y0, int x1, int y1, int x2, int y2, int colour, int width, int alpha)
{
    g_terrain->drawBezierSpline(x0, y0, x1, y1, x2, y2, colour, width, alpha);
}

void LoadCourseType(int courseType)
{
    int collar[23];
    for (int i = 0; i < 23; i++) {
        collar[i] = g_squareTypeTable[i].category;
        if (g_currentThemeId == THEME_DESERT && g_squareTypeTable[i].lieGrade > 0 && i != 5 && i != 7)
            collar[i] = 4;
    }
    g_terrain->passCollarInfo(collar, 23);
    g_terrain->loadNewCourseType(courseType);
}

void SyncTerrainTypes()
{
    int i, j;

    for (i = 49; i >= 0; i--) {
        do_all_tasks();
        for (j = 49; j >= 0; j--) {
            Tile *t = g_terrain->tileAt(i, j);
            int oldType = t->m_type;
            if (GetTerrainTypeAt(i, j) != oldType || GetTerrainVariationAt(i, j) != g_terrain->getVariation(t))
                g_terrain->setType(t, GetTerrainTypeAt(i, j), GetTerrainVariationAt(i, j));
        }
    }

    g_terrainRebuildPending = true;
}

void ResetAndRebuildTerrain()
{
    g_terrain->resetTerrain();
    BuildFromGameData();
    g_terrainRebuildPending = true;
}

void BuildFromGameData()
{
    int i, j, k, m;

    for (i = 49; i >= 0; i--) {
        do_all_tasks();
        for (j = 49; j >= 0; j--) {
            Tile *t = g_terrain->tileAt(i, j);
            g_terrain->setType(t, GetTerrainTypeAt(i, j), GetTerrainVariationAt(i, j));
        }
    }
    for (i = 49; i >= 0; i--) {
        do_all_tasks();
        for (j = 49; j >= 0; j--) {
            if (GetTerrainTypeAt(i, j) != TERRAIN_WATER) {
                for (k = 1; k < 8; k += 2) {
                    if (GetCornerElevationAt(i, j, k) > 3) {
                        for (m = 0; m < GetCornerElevationAt(i, j, k) - 3; m++) {
                            Tile *t = g_terrain->tileAt(i, j);
                            g_terrain->elevateCorner(t, k);
                        }
                    }
                }
            }
        }
    }
    for (i = 49; i >= 0; i--) {
        for (j = 49; j >= 0; j--) {
            Tile *t = g_terrain->tileAt(i, j);
            g_terrain->calcNormals(t);
        }
    }
    for (i = 49; i >= 0; i--) {
        for (j = 49; j >= 0; j--) {
            for (k = 0; k < 8; k += 2) {
                if (GetTerrainWallHeight(i, j, k)) {
                    Tile *t = g_terrain->tileAt(i, j);
                    g_terrain->setWall(t, k, GetTerrainWallHeight(i, j, k), true);
                }
            }
        }
    }
    for (i = 49; i >= 0; i--) {
        for (j = 49; j >= 0; j--) {
            if (IsTilePath(i, j)) {
                Tile *t = g_terrain->tileAt(i, j);
                g_terrain->layPath(t, IsTilePath(i, j), g_tileExtra[i * 50 + j] & TILE_REACHED);
            }
        }
    }
}

void InitTerrainSystem(int w, int h)
{
    OSVERSIONINFOA osvi;
    int collar[23];
    int i;

    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionExA(&osvi);
    if (osvi.dwMajorVersion >= 5)
        g_isWin2kOrLater = true;

    g_terrainViewWidth = w;
    g_terrainViewHeight = h;

    for (i = 0; i < 23; i++) {
        collar[i] = g_squareTypeTable[i].category;
        if (g_currentThemeId == THEME_DESERT && g_squareTypeTable[i].lieGrade > 0 && i != 5 && i != 7)
            collar[i] = 4;
    }

    g_terrain->passCollarInfo(collar, 23);
    g_terrain->initSystem(w, h, g_simScreen.m_surface->get_hdc(), g_isWin2kOrLater);
    g_terrain->initTerrain();
    BuildFromGameData();
}

void ResizeTerrainView(int w, int h)
{
    g_terrainViewWidth = w;
    g_terrainViewHeight = h;
    g_terrain->resize(w, h);
}

void ShutdownTerrain()
{
    g_terrain->closeSystem();
    delete g_terrain;
    g_terrain = 0;
}

void UpdateTerrainView(int x, int y, int zoom, int rot)
{
    if (!g_needReinit) {
        if (!g_terrainRebuildPending) {
            int dx = x - g_lastViewX;
            int dy = y - g_lastViewY;

            if (rot != g_lastRot) {
                g_simScreen.EraseWholeBackground(0x80000000);
                g_terrain->render(g_terrain->tileAt(x, y), RotationToDegrees(rot));
                g_simScreen.copy(g_terrainSurface, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
                g_lastViewY = y;
                g_lastViewX = x;
                g_lastRot = rot;
                return;
            }
            if (zoom != g_lastZoom) {
                g_terrain->setZoomLevel(zoom);
                g_simScreen.EraseWholeBackground(0x80000000);
                g_terrain->render(g_terrain->tileAt(x, y), RotationToDegrees(rot));
                g_simScreen.copy(g_terrainSurface, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
                g_lastViewY = y;
                g_lastViewX = x;
                g_lastZoom = zoom;
                return;
            }
            if (dy == 0 && dx == 0) {
                g_terrainSurface->copy(&g_simScreen, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
                if (!(g_tileUphillMap[0][0] & VIEW_TILES_SYNCED)) {
                    UpdateTerrainTilesInView(x, y);
                    g_tileUphillMap[0][0] |= VIEW_TILES_SYNCED;
                }
                if (g_tileDrawQueue.empty())
                    return;
                if (g_terrainViewDirty) {
                    g_terrain->calcAllNormals(g_terrain->tileAt(x, y));
                    g_terrainViewDirty = false;
                }
                for (g_tileDrawIter = g_tileDrawQueue.begin(); g_tileDrawIter != g_tileDrawQueue.end();
                    g_tileDrawIter++)
                    g_terrain->localRender(*g_tileDrawIter, g_terrain->tileAt(x, y), RotationToDegrees(rot));
                g_tileDrawQueue.clear();
                if (g_tilePathDirty) {
                    g_terrain->pathUpdateRender(g_terrain->tileAt(x, y), RotationToDegrees(rot));
                    g_tilePathDirty = false;
                }
                g_simScreen.copy(g_terrainSurface, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
                return;
            } else {
                if (abs(dy) + abs(dx) > 2) {
                    g_simScreen.EraseWholeBackground(0x80000000);
                    g_terrain->render(g_terrain->tileAt(x, y), RotationToDegrees(rot));
                    g_simScreen.copy(g_terrainSurface, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
                    g_lastViewY = y;
                    g_lastViewX = x;
                } else {
                    g_simScreen.EraseWholeBackground(0x80000000);
                    ScrollTerrainView(g_terrainSurface, x, y, dx, dy, rot, zoom);
                    g_simScreen.copy(g_terrainSurface, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
                    g_lastViewY = y;
                    g_lastViewX = x;
                }
                return;
            }
        }
    } else {
        InitTerrainSystem(g_screenWidth, g_screenHeight);
    }

    g_simScreen.EraseWholeBackground(0x80000000);
    g_terrain->render(g_terrain->tileAt(x, y), RotationToDegrees(rot));
    g_terrainRebuildPending = false;
    g_needReinit = 0;
    g_lastViewX = x;
    g_lastViewY = y;
    g_simScreen.copy(g_terrainSurface, 0, 0, 0, 0, g_terrainViewWidth, g_terrainViewHeight);
}

bool UpdateTileTerrainType(Tile *t, int row, int col)
{
    int oldType = t->m_type;

    if (GetTerrainTypeAt(row, col) == oldType && GetTerrainVariationAt(row, col) == g_terrain->getVariation(t))
        return false;

    if (GetTerrainTypeAt(row, col) == TERRAIN_SAND_TRAP || t->m_type == TERRAIN_SAND_TRAP
        || GetTerrainTypeAt(row, col) == 6 || t->m_type == 6)
        g_terrainViewDirty = true;

    g_terrain->setType(t, GetTerrainTypeAt(row, col), GetTerrainVariationAt(row, col));
    return true;
}

static inline void LowerCornerStep(Tile *t, int k, int row, int col, int cx, int cy)
{
    if (!IsPositionInvalid(row - 1, col) && !IsPositionInvalid(row, col - 1) && !IsPositionInvalid(row + 1, col)
        && !IsPositionInvalid(row, col + 1) && !IsPositionInvalid(row - 1, col - 1)
        && !IsPositionInvalid(row + 1, col - 1) && !IsPositionInvalid(row + 1, col + 1)
        && !IsPositionInvalid(row - 1, col + 1))
        g_terrain->lowerCorner(t, k);
    else
        g_terrain->lowerEdgeCorner(t, k, g_terrain->tileAt(cx, cy), RotationToDegrees(g_camDirIdx));
}

bool AdjustTileCornerElevations(Tile *t, int row, int col, int cx, int cy)
{
    bool changed = false;

    for (int k = 1; k < 8; k += 2) {
        if (GetCornerElevationAt(row, col, k) - 3 > g_terrain->getElevation(t, k)) {
            g_terrain->elevateCorner(t, k);
            g_terrainViewDirty = true;
            changed = true;
            while (GetCornerElevationAt(row, col, k) - 3 > g_terrain->getElevation(t, k))
                g_terrain->elevateCorner(t, k);
        } else if (GetCornerElevationAt(row, col, k) - 3 < g_terrain->getElevation(t, k)) {
            LowerCornerStep(t, k, row, col, cx, cy);
            g_terrainViewDirty = true;
            changed = true;
            while (GetCornerElevationAt(row, col, k) - 3 < g_terrain->getElevation(t, k))
                LowerCornerStep(t, k, row, col, cx, cy);
        }
    }
    return changed;
}

bool UpdateTileWalls(Tile *t, int row, int col)
{
    bool changed = false;

    for (int dir = 0; dir < 8; dir += 2) {
        if (GetTerrainWallHeight(row, col, dir) && !g_terrain->getWall(t, dir)) {
            g_terrain->setWall(t, dir, GetTerrainWallHeight(row, col, dir), true);
            changed = true;
        } else if (!GetTerrainWallHeight(row, col, dir) && g_terrain->getWall(t, dir)) {
            g_terrain->setWall(t, dir, 0, false);
            changed = true;
        }
    }
    return changed;
}

bool UpdateTilePath(Tile *t, int row, int col)
{
    bool changed = false;
    int i, j;

    if (IsTilePath(row, col) && !g_terrain->hasPath(t)) {
        if (g_tileExtra[row * 50 + col] & TILE_REACHED) {
            for (i = 0; i < 50; i++)
                for (j = 0; j < 50; j++)
                    if (IsTilePath(i, j))
                        g_terrain->updatePath(i, j, g_tileExtra[i * 50 + j] & TILE_REACHED);
            g_tilePathDirty = true;
        }
        g_terrain->layPath(t, IsTilePath(row, col), g_tileExtra[row * 50 + col] & TILE_REACHED);
        changed = true;
    } else if (!IsTilePath(row, col) && g_terrain->hasPath(t)) {
        if (g_terrain->hasConnectedPath(row, col)) {
            for (i = 0; i < 50; i++)
                for (j = 0; j < 50; j++)
                    if (IsTilePath(i, j))
                        g_terrain->updatePath(i, j, g_tileExtra[i * 50 + j] & TILE_REACHED);
            g_tilePathDirty = true;
        }
        g_terrain->layPath(t, IsTilePath(row, col), g_tileExtra[row * 50 + col] & TILE_REACHED);
        changed = true;
    }
    return changed;
}

void UpdateTerrainTilesInView(int cx, int cy)
{
    int i, j;

    for (i = cx - 4 / g_cameraZoom * 13; i < 50; i++) {
        if (i < 0)
            i = 0;
        for (j = cy - 4 / g_cameraZoom * 13; j < 50; j++) {
            if (j < 0)
                j = 0;
            if (IsPositionInvalid(i, j))
                continue;
            Tile *t = g_terrain->tileAt(i, j);
            bool changed = UpdateTileTerrainType(t, i, j);
            changed |= AdjustTileCornerElevations(t, i, j, cx, cy);
            changed |= UpdateTileWalls(t, i, j);
            changed |= UpdateTilePath(t, i, j);
            if (changed)
                g_tileDrawQueue.push_front(t);
        }
    }
}

void ScrollTerrainView(AssetSurface *surf, int x, int y, int dx, int dy, int rot, int zoom)
{
    int tileW, tileH, halfH, halfW;
    int shift, turn, dir, d;
    int sx, sy, dstX, dstY, cx, cy;

    if (g_terrainViewWidth == 800) {
        if (g_terrainViewHeight == 600) {
            tileW = 64;
            halfW = 32;
            tileH = 40;
            halfH = 20;
        }
    } else if (g_terrainViewWidth == 1024) {
        if (g_terrainViewHeight == 768) {
            tileW = 80;
            halfW = 40;
            tileH = 52;
            halfH = 26;
        }
    } else if (g_terrainViewWidth == 1280 && g_terrainViewHeight == 1024) {
        tileW = 104;
        halfW = 52;
        tileH = 68;
        halfH = 34;
    }

    switch (zoom) {
    case 1: shift = 2; break;
    case 2: shift = 1; break;
    case 4: shift = 0; break;
    }

    if (rot == 2)
        turn = 6;
    else if (rot == 6)
        turn = 2;
    else
        turn = rot;

    switch (dx) {
    case 0:
        switch (dy) {
        case 1:
        case 2: dir = 5; break;
        case -2:
        case -1: dir = 1; break;
        }
        break;
    case 1:
        switch (dy) {
        case -1: dir = 2; break;
        case 0: dir = 3; break;
        case 1: dir = 4; break;
        }
        break;
    case -1:
        switch (dy) {
        case -1: dir = 8; break;
        case 0: dir = 7; break;
        case 1: dir = 6; break;
        }
        break;
    case 2:
        if (dy == 0)
            dir = 3;
        break;
    case -2:
        if (dy == 0)
            dir = 7;
        break;
    }

    if (dir + turn > 8)
        d = (dir + turn) % 9 + 1;
    else
        d = dir + turn;

    switch (d) {
    case 1:
        sx = 0;
        sy = 0;
        if (rot == 0 || rot == 4) {
            dstX = abs(dy) * (halfW >> shift);
            dstY = abs(dy) * (halfH >> shift);
        } else {
            dstX = abs(dx) * (halfW >> shift);
            dstY = abs(dx) * (halfH >> shift);
        }
        cx = g_terrainViewWidth - (halfW >> shift);
        cy = g_terrainViewHeight - (halfH >> shift);
        break;
    case 2:
        dstY = tileH >> shift;
        sx = 0;
        sy = 0;
        dstX = 0;
        cx = g_terrainViewWidth;
        cy = g_terrainViewHeight - (tileH >> shift);
        break;
    case 3:
        if (rot == 0 || rot == 4) {
            sx = abs(dx) * (halfW >> shift);
            dstY = abs(dx) * (halfH >> shift);
        } else {
            sx = abs(dy) * (halfW >> shift);
            dstY = abs(dy) * (halfH >> shift);
        }
        sy = 0;
        dstX = 0;
        cx = g_terrainViewWidth - (halfW >> shift);
        cy = g_terrainViewHeight - (halfH >> shift);
        break;
    case 4:
        sx = tileW >> shift;
        sy = 0;
        dstX = 0;
        dstY = 0;
        cx = g_terrainViewWidth - (tileW >> shift);
        cy = g_terrainViewHeight;
        break;
    case 5:
        if (rot == 0 || rot == 4) {
            sx = abs(dy) * (halfW >> shift);
            sy = abs(dy) * (halfH >> shift);
        } else {
            sx = abs(dx) * (halfW >> shift);
            sy = abs(dx) * (halfH >> shift);
        }
        dstX = 0;
        dstY = 0;
        cx = g_terrainViewWidth - (halfW >> shift);
        cy = g_terrainViewHeight - (halfH >> shift);
        break;
    case 6:
        sy = tileH >> shift;
        sx = 0;
        dstX = 0;
        dstY = 0;
        cx = g_terrainViewWidth;
        cy = g_terrainViewHeight - (tileH >> shift);
        break;
    case 7:
        sx = 0;
        if (rot == 0 || rot == 4) {
            sy = abs(dx) * (halfH >> shift);
            dstX = abs(dx) * (halfW >> shift);
        } else {
            sy = abs(dy) * (halfH >> shift);
            dstX = abs(dy) * (halfW >> shift);
        }
        dstY = 0;
        cx = g_terrainViewWidth - (halfW >> shift);
        cy = g_terrainViewHeight - (halfH >> shift);
        break;
    case 8:
        dstX = tileW >> shift;
        sx = 0;
        sy = 0;
        dstY = 0;
        cx = g_terrainViewWidth - (tileW >> shift);
        cy = g_terrainViewHeight;
        break;
    }

    g_terrain->stripRender(g_terrain->tileAt(x, y), dir, RotationToDegrees(rot));
    g_terrainSurface->copy(&g_simScreen, sx, sy, dstX, dstY, cx, cy);
}
