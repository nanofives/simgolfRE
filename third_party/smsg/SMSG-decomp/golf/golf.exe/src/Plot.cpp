#include "jgldefs.h"
#include "Plot.h"
#include "Golfer.h"
#include "crt.h"
#include "IsoView.h"
#include "RenderQueue.h"
#include "FlicBanks.h"
#include "FloatingText.h"

char g_numBuffer[16] = {0};

BuildingDef g_buildingDefs[16] = {
    {"Summer", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"White House", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"Art Deco", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"Art Deco", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"Art Deco", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"Art Deco", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"Art Deco", {0x07, 0x07, 0x07, 0x07, 0x00, 0x00}, 4},
    {"Mansion", {0x0e, 0x1e, 0x1e, 0x1e, 0x04, 0x00}, 6},
    {"Mansion", {0x0e, 0x1e, 0x1e, 0x1e, 0x04, 0x00}, 6},
    {"Video Game", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {"Grill", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {"Jacuzzi", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {"Punching Bag", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {"Classic Car", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {"BMW Convertible", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
    {"Harley", {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}, 1},
};

void ClearPlot(int plot)
{
    int i, j;
    memset(g_homePlots[plot].building, -1, sizeof g_homePlots[plot].building);
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++)
            g_homePlots[plot].cell[i][j] = -1;
}

int PlaceBuilding(int plot, int def, int x, int y)
{
    int ok = 1;
    int i, j, n;
    for (i = 0; i < 6; i++) {
        if (g_buildingDefs[def].footprint[i] != 0 && ok) {
            int cx = x + i;
            if (cx >= 6)
                break;
            for (j = 0; j < 6; j++) {
                if ((g_buildingDefs[def].footprint[i] & (1 << j)) && ok) {
                    int cy = y + j;
                    if (cy >= 6)
                        ok = 0;
                    if (g_homePlots[plot].cell[cx][cy] != -1)
                        ok = 0;
                }
            }
        }
    }
    for (n = 0; n < 16; n++) {
        if (g_homePlots[plot].building[n] == -1) {
            g_homePlots[plot].building[n] = def;
            g_homePlots[plot].x[n] = x;
            g_homePlots[plot].y[n] = y;
            g_homePlots[plot].frame[n] = -1;
            for (i = 0; i < 6; i++) {
                int cx = x + i;
                for (j = 0; j < 6; j++) {
                    if (g_buildingDefs[def].footprint[i] & (1 << j))
                        g_homePlots[plot].cell[cx][y + j] = n;
                }
            }
            return 1;
        }
    }
    return 0;
}

void RemoveHomeVIPAtPosition(int tileX, int tileY)
{
    for (int i = 0; i < 100; i++)
        if (g_homeVIPsTbl[i].tileX == tileX && g_homeVIPsTbl[i].tileY == tileY)
            g_homeVIPsTbl[i].kind = -1;
}

void SpawnHomeVIP(int plot, int celeb, int tileX, int tileY)
{
    for (int i = 0; i < 100; i++) {
        if (g_homeVIPsTbl[i].kind == -1) {
            memset(&g_homeVIPsTbl[i].reserved, 0, sizeof g_homeVIPsTbl[i].reserved);
            g_homeVIPsTbl[i].plot = (short)plot;
            g_homeVIPsTbl[i].kind = g_themeCelebsEntries[celeb].kind;
            g_homeVIPsTbl[i].tileX = (short)tileX;
            g_homeVIPsTbl[i].tileY = (short)tileY;
            g_homeVIPsTbl[i].anim = 0x140;
            unsigned int rx, ry;
            do {
                rx = g_rng.NextInt(4);
                ry = g_rng.NextInt(4);
            } while (g_homePlots[plot].cell[rx][ry] != -1);
            g_homeVIPsTbl[i].posX = ((int)(rx * 1024) + 512) / 2;
            g_homeVIPsTbl[i].posY = ((int)(ry * 1024) + 512) / 2;
            return;
        }
    }
}

void RenderPlot(int plot, int tileX, int tileY)
{
    int i = 0;
    int wy0 = tileY << 10;
    int wx0 = tileX << 10;
    int gx = plot;
    int gy;
    int j, k;
    for (; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (!WorldToScreen(wx0 + (0x400 + (i << 11)) / 4, wy0 + (0x400 + (j << 11)) / 4, &tileX, &tileY, 0))
                continue;
            if (plot & 1)
                g_homePlotGroundPics[g_homePlots[plot].cell[i][j] >= 1].BlitScaled(g_pMenu, tileX - g_cameraZoom * 4,
                    tileY - g_cameraZoom * 5 / 2, 0);
            else
                g_homePlotGroundPics[0].BlitScaled(g_pMenu, tileX - g_cameraZoom * 4, tileY - g_cameraZoom * 5 / 2, 0);
            switch (g_camDirIdx) {
            case 0:
                gx = i;
                gy = j;
                break;
            case QUARTER_TURN:
                gx = j;
                gy = 5 - i;
                break;
            case HALF_TURN:
                gx = 5 - i;
                gy = 5 - j;
                break;
            case THREE_QUARTER_TURN:
                gx = 5 - j;
                gy = i;
                break;
            }
            if (gx == 0 && (gy & 1) == 0)
                AddSpriteToRenderQueue(tileX, tileY, tileY + 11, 0x10, 8, &g_homePlotGroundPics[4], SPRITE_FULL_SCALE,
                    -1);
            if (gy == 0 && (gx & 1) == 0)
                AddSpriteToRenderQueue(tileX, tileY, tileY - 0x15, 0x10, 0x1d, &g_homePlotGroundPics[5],
                    SPRITE_FULL_SCALE, -1);
            if (gx == 5 && (gy & 1))
                AddSpriteToRenderQueue(tileX, tileY, tileY - 0x15, 0x10, 0x1d, &g_homePlotGroundPics[4],
                    SPRITE_FULL_SCALE, -1);
            if (gy == 5 && (gx & 1))
                AddSpriteToRenderQueue(tileX, tileY, tileY + 11, 0x10, 8, &g_homePlotGroundPics[5], SPRITE_FULL_SCALE,
                    -1);
        }
    }
    for (k = 0; k < 16; k++) {
        short v = g_homePlots[plot].building[k];
        int id, sx, sy;
        if (v == -1)
            continue;
        id = v;
        sx = wx0 + ((g_buildingDefs[id].size + g_homePlots[plot].x[k] * 2) << 10) / 4;
        sy = wy0 + ((g_buildingDefs[id].size + g_homePlots[plot].y[k] * 2) << 10) / 4;
        if (!WorldToScreen(sx, sy, &tileX, &tileY, 0))
            continue;
        if (id < 10) {
            if (id < 3) {
                g_curDecoSprite = GetFlicBankSprite(id + 0x13c, 0, (g_camDirIdx / 2) & 3);
                DrawSpriteImmediate(tileX, tileY, tileY, 0x8c, 0x8c, g_curDecoSprite, SPRITE_FULL_SCALE, id + 0x41);
            }
            g_curDecoSprite = GetFlicBankSprite(id + 0x136, 0, (g_camDirIdx / 2) & 3);
            AddSpriteToRenderQueue(tileX, tileY, tileY, 0x8c, 0x8c, g_curDecoSprite, SPRITE_FULL_SCALE, id + 0x41);
        } else {
            if (g_homePlots[plot].frame[k] == -1) {
                g_curDecoSprite = GetFlicBankSprite(id + 0x136, 0, (g_camDirIdx / 2 + k) & 3);
            } else {
                g_curDecoSprite = GetFlicBankSprite(id + 0x13c, g_homePlots[plot].frame[k], (g_camDirIdx / 2 + k) & 3);
                g_homePlots[plot].frame[k]++;
                if (g_homePlots[plot].frame[k] >= g_flicBankFrameLens[id + 0x13c] - 1)
                    g_homePlots[plot].frame[k] = -1;
            }
            AddSpriteToRenderQueue(tileX, tileY, tileY, 0x1e, 0x28, g_curDecoSprite, SPRITE_FULL_SCALE, id + 0x41);
        }
    }
}

void InitPlots(void)
{
    int i;
    for (i = 0; i < 100; i++)
        g_homeVIPsTbl[i].kind = -1;
    ClearPlot(0);
    PlaceBuilding(0, 0, 0, 0);
    ClearPlot(1);
    PlaceBuilding(1, 1, 0, 0);
    ClearPlot(2);
    PlaceBuilding(2, 2, 0, 0);
    for (i = 3; i < 6; i++) {
        ClearPlot(i);
        PlaceBuilding(i, i, 0, 0);
    }
}

void UpdateHomeVIP(int idx)
{
    int screenX;
    int i = idx;
    int wx = g_homeVIPsTbl[i].tileX * 0x400 + g_homeVIPsTbl[i].posX;
    int wy = g_homeVIPsTbl[i].tileY * 0x400 + g_homeVIPsTbl[i].posY;
    int bank = g_homeVIPsTbl[i].anim + g_homeVIPsTbl[i].kind;
    if (WorldToScreen(wx, wy, &screenX, &idx, 0)) {
        if (g_homeVIPsTbl[i].timer < 0x80) {
            int extra = 0;
            int k;
            for (k = 0; k < 256; k++)
                if (g_buildingTable[k].kind == SCENERY_HOME_SITE && g_buildingTable[k].count > 0
                    && g_buildingTable[k].row == g_homeVIPsTbl[i].tileX
                    && g_buildingTable[k].col == g_homeVIPsTbl[i].tileY)
                    extra = g_buildingTable[k].count - 1;
            int facing = (g_camDirIdx - g_homeVIPsTbl[i].dir - 2) & 7;
            g_homeVIPsTbl[i].frame = g_homeVIPsTbl[i].frame % g_flicBankFrameLens[bank];
            g_curDecoSprite = GetFlicBankSprite(bank, g_homeVIPsTbl[i].frame, facing);
            AddSpriteToRenderQueue(screenX, idx, idx, 0x1e, 0x28, g_curDecoSprite, SPRITE_FULL_SCALE,
                (extra + 0x9c) | 0x100);
            if (!(g_runModeFlags & GAME_PAUSED)) {
                g_homeVIPsTbl[i].frame = g_homeVIPsTbl[i].frame + 1;
                if (g_homeVIPsTbl[i].frame >= g_flicBankFrameLens[bank])
                    g_homeVIPsTbl[i].frame = 0;
            }
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa((g_homeVIPsTbl[i].posX * 2) >> 10, g_numBuffer, 10));
            strcat(g_printBuffer, "/");
            strcat(g_printBuffer, itoa((g_homeVIPsTbl[i].posY * 2) >> 10, g_numBuffer, 10));
        }
    } else {
        if (!(g_runModeFlags & GAME_PAUSED) && g_rng.NextInt(500) == 0)
            g_homeVIPsTbl[i].timer = 500;
    }
    if (g_homeVIPsTbl[i].timer != 0) {
        g_homeVIPsTbl[i].anim = 0x140;
        if (!(g_runModeFlags & GAME_PAUSED))
            g_homeVIPsTbl[i].timer--;
        return;
    }
    if (g_homeVIPsTbl[i].frame == 0 && g_homeVIPsTbl[i].anim != 0x15a && g_homeVIPsTbl[i].anim != 0x140) {
        g_homeVIPsTbl[i].anim = 0x140;
        g_homeVIPsTbl[i].timer = g_rng.NextInt(0x40) + 0x40;
    }
    if (g_homeVIPsTbl[i].moveTicks == 0) {
        if (g_homeVIPsTbl[i].anim == 0x140 && g_rng.NextInt(8) == 0) {
            int nx = (g_homeVIPsTbl[i].posX * 2) >> 10;
            int ny = (g_homeVIPsTbl[i].posY * 2) >> 10;
            int r = g_rng.NextInt(3);
            g_homeVIPsTbl[i].dir = (g_homeVIPsTbl[i].dir + r - 1) & 7;
            nx += g_dirRowStep[g_homeVIPsTbl[i].dir];
            ny += g_dirColStep[g_homeVIPsTbl[i].dir];
            if (nx < 0 || ny < 0 || nx >= 4 || ny >= 4) {
                int lim = ((g_runModeFlags & GAME_TOURNAMENT) ? 0x28 : 0) + 0x18;
                if (g_rng.NextInt(lim) == 0) {
                    PlayPositionedSound(g_homeVIPsTbl[i].kind + 0x82, wx, wy, 0);
                    g_homeVIPsTbl[i].anim = 0x14d;
                    g_homeVIPsTbl[i].frame = 0;
                }
            } else if (g_homePlots[g_homeVIPsTbl[i].plot].cell[nx][ny] == -1) {
                g_homeVIPsTbl[i].moveTicks = 0x10;
            }
        }
        if (g_homeVIPsTbl[i].moveTicks == 0)
            return;
    }
    g_homeVIPsTbl[i].anim = 0x15a;
    g_homeVIPsTbl[i].posX += (g_dirRowStep[g_homeVIPsTbl[i].dir] * 0x400) / 32;
    g_homeVIPsTbl[i].posY += (g_dirColStep[g_homeVIPsTbl[i].dir] * 0x400) / 32;
    g_homeVIPsTbl[i].moveTicks = g_homeVIPsTbl[i].moveTicks - 1;
    if (g_homeVIPsTbl[i].moveTicks <= 0)
        g_homeVIPsTbl[i].anim = 0x140;
}

void UpdateAllHomeVIPs(void)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (g_homeVIPsTbl[i].kind != -1)
            UpdateHomeVIP(i);
    }
}

int GetPlotCrowding(int plot, float *crowding)
{
    int i, j, cells = 0, vips = 0;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            if (g_homePlots[plot].cell[i][j] == -1)
                cells++;
    if (cells == 0)
        return 0;
    for (i = 0; i < 100; i++)
        if (g_homeVIPsTbl[i].kind != -1 && g_homeVIPsTbl[i].plot == plot)
            vips++;
    *crowding = (float)vips / cells;
    return 1;
}

void RestorePlot(int plot, HomePlot *saved)
{
    if (saved == 0)
        return;
    g_homePlots[plot] = *saved;
    delete saved;
}

#include "ListBoxCtl.h"

#define BUILDING_FACINGS 4

struct BuildingOrder : virtual HeapNew {
    int plot;
    int def;
    int x, y;
    int facing;
    double progress;
    BuildingOrder()
    {
        facing = g_rng.NextInt(BUILDING_FACINGS);
        progress = g_rng.get();
        if (progress >= 1.0 || progress <= 0.0)
            progress = 0.0;
    }
};

struct BuildingOrderList : ClassList<BuildingOrder> {
    int m_stagePlot;
    int m_stageDef;
    int m_stageX, m_stageY;

    virtual int fill_entry(BuildingOrder *order);
};

#include "PaletteList.h"
#include "SpriteBox.h"
template struct ClassList<StringStruct>;
template struct ClassList<PalStruct>;
template struct ClassList<SpriteStruct>;

int BuildingOrderList::fill_entry(BuildingOrder *order)
{
    order->plot = m_stagePlot;
    order->def = m_stageDef;
    order->x = m_stageX;
    order->y = m_stageY;
    return 0;
}

int OrderBuilding(BuildingOrderList *orders, int plot, int def, int x, int y)
{
    orders->m_stagePlot = plot;
    orders->m_stageDef = def;
    orders->m_stageX = x;
    orders->m_stageY = y;
    return orders->add(plot);
}

HomePlot g_homePlots[100];
SpriteObject *g_curDecoSprite = 0;
