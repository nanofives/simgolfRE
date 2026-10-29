#include "jgldefs.h"
#include "crt.h"
#include "RenderQueue.h"
#include "MainLoop.h"
#include "FLCPalette.h"
#include "Golfer.h"
#include "Surface.h"
#include "Screen.h"
#include "DialogFrame.h"
#include "SaveGame.h"
#include "ShotTrail.h"
#include "LotScoreTier.h"
#include "Scorecard.h"
#include "Geometry.h"
#include "CourseGrid.h"
#include "CourseScript.h"
#include "TerrainGrid.h"
#include "FlicBanks.h"
#include "ScreenText.h"
#include "BuildingSearch.h"
#include "GolferDialogue.h"

int g_rqSortKey[1024] = {0};
int g_rqSavedNext[1024] = {0};
int g_rqSavedAnchorX[1024] = {0};
int g_rqSavedAnchorY[1024] = {0};
int g_rqFlags[1024] = {0};
int g_rqScale[1024] = {0};
SpriteObject *g_rqSprite[1024] = {0};
int g_rqSavedCount = 0;
int g_rqAnchorX[1024] = {0};
int g_rqHead = 0;
int g_rqAnchorY[1024] = {0};
int g_rqSavedSortKey[1024] = {0};
int g_rqSavedFlags[1024] = {0};
int g_rqSavedScale[1024] = {0};
int g_rqBlitX[1024] = {0};
int g_rqBlitY[1024] = {0};
int g_rqSavedBlitX[1024] = {0};
int g_rqSavedBlitY[1024] = {0};
char g_golferIdText[12] = {0};
int g_clock = 0;
int g_rqSlotMark[1024] = {0};
static int s_rqSpare[32] = {0};
SpriteObject *g_rqSavedSprite[1024] = {0};
int g_rqNext[1024] = {0};
int g_rqSavedHead = 0;
int g_zoomFromCamera = 0;
int g_rqSavedSlotMark[1024] = {0};
static int s_rqSpare2 = 0;
int g_rqCount = 0;

#define COLOUR_PALETTES        1
#define FEMALE_PALETTE_OFFSET  (FEMALE_FIGURE - COLOUR_PALETTES)
#define FEMALE_HEADS           20
#define FIGURE_COLOURS         0x78
#define SHIRT_RUN              0x00
#define PANTS_RUN              0x14
#define SKIN_RUN               0x28
#define APPEARANCE_RUN         0x3c
#define PART_RUN_LENGTH        20
#define EXTRA_RUN              0x50
#define HAIR_RUN               0x50
#define HAIR_RUN_LENGTH        10
#define FEMALE_EXTRA_RUN       0x5a
#define EXTRA_RUN_LENGTH       7
#define SHIRT_RUN2             0x6b
#define SHIRT_RUN2_LENGTH      15
#define PANTS_RUN2             0x7a
#define PANTS_RUN2_LENGTH      10
#define SKIN_RUN2              0x84
#define SKIN_RUN2_LENGTH       50
#define LAST_RUN               0x90
#define LAST_RUN_LENGTH        28

void SetupGolferAppearance(int gi)
{
    int femalePalette = 0;
    unsigned char colors[256][3];
    int isCelebrity, maleFigure;
    int hairColor, pantsColor, skinTone, extraTone, shirtColor;
    int appearanceColor, type, male;
    FLCPalette *working = &g_palettes[0];
    FLCPalette *part;

    isCelebrity = 0;
    maleFigure = 1;
    if (gi < FIRST_CELEBRITY)
        male = IsGolferMale(gi);
    else
        isCelebrity = 1;
    if (!isCelebrity) {
        maleFigure = male;
        if (gi == 0xff)
            gi = 0;
        type = g_golferPool[gi].typeIdx;
        pantsColor = type % 10;
        shirtColor = (type * 3) % 10;
        if (g_golferTypeArray[type].head < 19) {
            if (male)
                skinTone = g_headPresets[g_golferTypeArray[type].head].skinTone;
            else
                skinTone = g_headPresets[g_golferTypeArray[type].head + FEMALE_HEADS].skinTone;
        } else
            skinTone = type % 4;
        appearanceColor = (type * 3) % 10;
        if (!(g_golferPool[gi].talents & TALENT_IMAGINATION))
            appearanceColor = male ? 0 : 9;
        extraTone = skinTone;
        if (g_golferPool[gi].talents & TALENT_ACCURACY)
            extraTone = 4;
        if (!male)
            shirtColor = (type * 2) % 3 + 1;
    }
    if (gi >= FIRST_CELEBRITY)
        appearanceColor = isCelebrity;

    if (gi >= FIRST_CELEBRITY) {
        skinTone = g_themeCelebsEntries[gi - FIRST_CELEBRITY].skinTone;
        appearanceColor = g_themeCelebsEntries[gi - FIRST_CELEBRITY].appearanceColor + 5;
        shirtColor = g_themeCelebsEntries[gi - FIRST_CELEBRITY].shirtColor;
        isCelebrity = 1;
        extraTone = 4;
        maleFigure = 1;
        pantsColor = g_themeCelebsEntries[gi - FIRST_CELEBRITY].pantsColor;
    } else {
        type = g_golferPool[gi].typeIdx;
        hairColor = g_headPresets[(IsGolferMale(gi) ? 0 : FEMALE_HEADS) + g_golferTypeArray[type].head].hairColor;
        if (g_golferTypeArray[type].fixedKind) {
            shirtColor = g_golferTypeArray[type].shirtColor;
            pantsColor = g_golferTypeArray[type].pantsColor;
            appearanceColor = g_golferTypeArray[type].appearance & APPEARANCE_LOW_BITS;
            skinTone = g_golferTypeArray[type].skinTone;
            extraTone = g_golferTypeArray[type].extraTone;
            hairColor = g_golferTypeArray[type].hairColor;
        }
        if (g_golferPool[gi].persona == PERSONA_ROSTER) {
            extraTone = 4;
            maleFigure = 1;
            int rosterRow = g_golferPool[gi].rosterIndex;
            appearanceColor = g_golfProList[rosterRow].appearanceColor;
            skinTone = g_golfProList[rosterRow].skinTone;
            shirtColor = g_golfProList[rosterRow].shirtColor;
            pantsColor = g_golfProList[rosterRow].pantsColor;
        }
        if (!isCelebrity) {
            g_golferTypeArray[type].appearance =
                (g_golferTypeArray[type].appearance & ~APPEARANCE_LOW_BITS) | appearanceColor;
            g_golferTypeArray[type].shirtColor = shirtColor;
            g_golferTypeArray[type].pantsColor = pantsColor;
            g_golferTypeArray[type].skinTone = skinTone;
            g_golferTypeArray[type].extraTone = extraTone;
            g_golferTypeArray[type].hairColor = hairColor;
        }
        if (!maleFigure) {
            pantsColor += FEMALE_PALETTE_OFFSET;
            skinTone += FEMALE_PALETTE_OFFSET;
            appearanceColor += FEMALE_PALETTE_OFFSET;
            extraTone += FEMALE_PALETTE_OFFSET;
            shirtColor += FEMALE_PALETTE_OFFSET;
            hairColor += FEMALE_PALETTE_OFFSET;
            part = &g_palettes[FEMALE_FIGURE];
            if (part->pal)
                part->pal->get_entries(colors[0], 0, FIGURE_COLOURS);
            if (!working->pal)
                working->init();
            working->pal->set_entries(colors[0], 0, FIGURE_COLOURS);
            femalePalette = 1;
        }
    }
    if (!femalePalette) {
        part = &g_palettes[COLOUR_PALETTES];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, FIGURE_COLOURS);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[0], 0, FIGURE_COLOURS);
    }

    part = &g_palettes[COLOUR_PALETTES + shirtColor];
    if (part->pal)
        part->pal->get_entries(colors[0], 0, SHIRT_RUN + PART_RUN_LENGTH);
    if (!working->pal)
        working->init();
    working->pal->set_entries(colors[SHIRT_RUN], SHIRT_RUN, PART_RUN_LENGTH);

    part = &g_palettes[COLOUR_PALETTES + pantsColor];
    if (part->pal)
        part->pal->get_entries(colors[0], 0, PANTS_RUN + PART_RUN_LENGTH);
    if (!working->pal)
        working->init();
    working->pal->set_entries(colors[PANTS_RUN], PANTS_RUN, PART_RUN_LENGTH);

    part = &g_palettes[COLOUR_PALETTES + skinTone];
    if (part->pal)
        part->pal->get_entries(colors[0], 0, SKIN_RUN + PART_RUN_LENGTH);
    if (!working->pal)
        working->init();
    working->pal->set_entries(colors[SKIN_RUN], SKIN_RUN, PART_RUN_LENGTH);

    part = &g_palettes[COLOUR_PALETTES + appearanceColor];
    if (part->pal)
        part->pal->get_entries(colors[0], 0, APPEARANCE_RUN + PART_RUN_LENGTH);
    if (!working->pal)
        working->init();
    working->pal->set_entries(colors[APPEARANCE_RUN], APPEARANCE_RUN, PART_RUN_LENGTH);

    if (maleFigure) {
        part = &g_palettes[COLOUR_PALETTES + shirtColor];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, SHIRT_RUN2 + SHIRT_RUN2_LENGTH - 1);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[SHIRT_RUN2], SHIRT_RUN2, SHIRT_RUN2_LENGTH);

        part = &g_palettes[COLOUR_PALETTES + pantsColor];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, PANTS_RUN2 + PANTS_RUN2_LENGTH - 1);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[PANTS_RUN2], PANTS_RUN2, PANTS_RUN2_LENGTH);

        part = &g_palettes[COLOUR_PALETTES + skinTone];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, SKIN_RUN2 + 4);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[SKIN_RUN2], SKIN_RUN2, SKIN_RUN2_LENGTH);

        if (!isCelebrity) {
            part = &g_palettes[COLOUR_PALETTES + extraTone];
            if (part->pal)
                part->pal->get_entries(colors[0], 0, EXTRA_RUN + HAIR_RUN_LENGTH);
            if (!working->pal)
                working->init();
            working->pal->set_entries(colors[EXTRA_RUN], EXTRA_RUN, EXTRA_RUN_LENGTH);
        }
    } else {
        part = &g_palettes[COLOUR_PALETTES + hairColor];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, HAIR_RUN + HAIR_RUN_LENGTH);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[HAIR_RUN], HAIR_RUN, HAIR_RUN_LENGTH);

        part = &g_palettes[COLOUR_PALETTES + extraTone];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, FEMALE_EXTRA_RUN + HAIR_RUN_LENGTH);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[FEMALE_EXTRA_RUN], FEMALE_EXTRA_RUN, EXTRA_RUN_LENGTH);

        part = &g_palettes[FEMALE_FIGURE + (gi & 1)];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, LAST_RUN + LAST_RUN_LENGTH);
    }

    if (gi >= 0 && gi < MAX_GOLFERS
        && (g_golferHistoryPerCharacter[g_golferPool[gi].typeIdx].flags & MEMBERSHIP_MASK) >= 4) {
        part = &g_palettes[FEMALE_FIGURE + 1];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, LAST_RUN + LAST_RUN_LENGTH);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[LAST_RUN], LAST_RUN, LAST_RUN_LENGTH);
    } else {
        part = &g_palettes[FEMALE_FIGURE];
        if (part->pal)
            part->pal->get_entries(colors[0], 0, LAST_RUN + LAST_RUN_LENGTH);
        if (!working->pal)
            working->init();
        working->pal->set_entries(colors[LAST_RUN], LAST_RUN, LAST_RUN_LENGTH);
    }
}

void ResetRenderState()
{
    int i;

    g_rqCount = 0;
    g_rqSlotMark[0] = 0;
    g_rqSavedSlotMark[0] = 0;
    g_rqHead = -1;
    for (i = 0; i < 1024; i++)
        g_rqNext[1024] = -1;
}

void SaveRenderQueue()
{
    g_rqSavedCount = g_rqCount;
    memcpy(g_rqSavedBlitX, g_rqBlitX, 0x1000);
    memcpy(g_rqSavedBlitY, g_rqBlitY, 0x1000);
    memcpy(g_rqSavedAnchorX, g_rqAnchorX, 0x1000);
    memcpy(g_rqSavedAnchorY, g_rqAnchorY, 0x1000);
    memcpy(g_rqSavedSortKey, g_rqSortKey, 0x1000);
    memcpy(g_rqSavedScale, g_rqScale, 0x1000);
    memcpy(g_rqSavedNext, g_rqNext, 0x1000);
    memcpy(g_rqSavedSprite, g_rqSprite, 0x1000);
    memcpy(g_rqSavedSlotMark, g_rqSlotMark, 0x1000);
    memcpy(g_rqSavedFlags, g_rqFlags, 0x1000);
    g_rqSavedHead = g_rqHead;
}

void DrawSpriteImmediate(int x, int y, int sortKey, int ox, int oy, SpriteObject *spr, int sz, int id)
{
    int d = 0x10;
    if (g_screenWidth > 0x3e8)
        d = 0xd;
    if (g_screenWidth >= 0x4b0)
        d = 0xa;
    if (id & 0x100)
        d = 0x10;

    if (sz == 0) {
        g_rqBlitX[g_rqCount] = x - g_cameraZoom * ox / 4;
        g_rqBlitY[g_rqCount] = y - g_cameraZoom * oy / 4;
    } else if (sz > 0) {
        g_rqBlitX[g_rqCount] = x - g_cameraZoom * ox * sz / d;
        g_rqBlitY[g_rqCount] = y - g_cameraZoom * oy * sz / d;
    } else {
        d += d;
        g_rqBlitX[g_rqCount] = g_cameraZoom * ox * sz / d + x;
        g_rqBlitY[g_rqCount] = g_cameraZoom * oy * sz / d + y;
    }

    g_rqAnchorY[g_rqCount] = y;
    g_rqScale[g_rqCount] = sz;
    g_rqAnchorX[g_rqCount] = x;
    g_rqSortKey[g_rqCount] = sortKey;
    g_rqSprite[g_rqCount] = spr;
    g_rqFlags[g_rqCount] = id;
    RenderQueuedSprite(g_rqCount);
}

void AddSpriteToRenderQueue(int x, int y, int sortKey, int ox, int oy, SpriteObject *spr, int sz, int id)
{
    int i;
    int prev;

    if (g_rqCount >= 0x3ff)
        return;

    int d = 0x10;
    if (g_screenWidth > 0x3e8)
        d = 0xd;
    if (g_screenWidth >= 0x4b0)
        d = 0xa;
    if (id & 0x100)
        d = 0x10;

    if (sz == 0) {
        g_rqBlitX[g_rqCount] = x - g_cameraZoom * ox / 4;
        g_rqBlitY[g_rqCount] = y - g_cameraZoom * oy / 4;
    } else if (sz > 0) {
        g_rqBlitX[g_rqCount] = x - g_cameraZoom * ox * sz / d;
        g_rqBlitY[g_rqCount] = y - g_cameraZoom * oy * sz / d;
    } else {
        d += d;
        g_rqBlitX[g_rqCount] = g_cameraZoom * ox * sz / d + x;
        g_rqBlitY[g_rqCount] = g_cameraZoom * oy * sz / d + y;
    }

    g_rqAnchorX[g_rqCount] = x;
    g_rqAnchorY[g_rqCount] = y;
    g_rqSortKey[g_rqCount] = sortKey;
    g_rqSprite[g_rqCount] = spr;
    g_rqScale[g_rqCount] = sz;
    g_rqFlags[g_rqCount] = id;

    int hd = g_rqHead;
    i = hd;
    while (i != -1 && g_rqSortKey[i] < sortKey) {
        prev = i;
        i = g_rqNext[i];
    }
    if (i == hd) {
        g_rqNext[g_rqCount] = g_rqHead;
        g_rqHead = g_rqCount;
    } else {
        g_rqNext[g_rqCount] = i;
        g_rqNext[prev] = g_rqCount;
    }
    g_rqCount++;
    if (g_rqCount < 0x400)
        g_rqSlotMark[g_rqCount] = 0;
}

void RenderQueuedSprite(int n)
{
    FLCPalette pal;
    int q, sz, f, v;
    unsigned bar;
    short m;

    f = g_rqFlags[n];
    sz = 0x10;
    if (g_screenWidth > 0x3e8)
        sz = 0xd;
    if (g_screenWidth >= 0x4b0)
        sz = 0xa;
    if (f & 0x100)
        sz = 0x10;

    q = n;
    SpriteObject *spr = g_rqSprite[q];
    n = spr->m_height;
#define RENDERQUEUE_UNRECOVERED(h) while ((h) > 0) (h) -= 8
    RENDERQUEUE_UNRECOVERED(n);

    if (g_rqScale[q] != 0) {
        if (g_rqScale[q] > 0) {
            v = g_rqScale[q] * g_cameraZoom;
            if (graphsys)
                graphsys->set_sprite_scale(v, v, sz);
        } else {
            v = -(g_rqScale[q] * g_cameraZoom);
            sz += sz;
            if (graphsys)
                graphsys->set_sprite_scale(v, v, sz);
        }
        if (g_rqFlags[q] != -1) {
            f = g_rqFlags[q];
            if (f & 0x100) {
                f &= 0xff;
                SetupGolferAppearance(f);
                g_rqSprite[q]->BlitWithScaleOffset(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], g_shadeTable, g_palettes);
                if (f != 0xff && f < 0x98) {
                    g_printBuffer[0] = 0;
                    AppendGolferName(f, 0);
                    if (g_golferPool[f].flags & 0x4000)
                        strcat(g_printBuffer, "!");
                    if (g_runModeFlags & GAME_DEBUG_OVERLAY)
                        strcat(g_printBuffer, itoa(f, g_golferIdText, 10));
                    if (g_toolbarView == TOOLBAR_SHOT && (g_golferPool[f].flags & GOLFER_PRO_MATCH) == 0)
                        g_printBuffer[0] = 0;
                    g_pMenu->SetFontsForStyles(g_cameraZoom > DEFAULT_ZOOM ? &g_smallManualFont : &g_boldArialFont, 0,
                        0, 0);
                    if (((g_configBitMask & PREF_SHOW_NAMES) && g_cameraZoom >= DEFAULT_ZOOM
                            && (g_golferPool[f].talkTimer == 0 || f == g_selectedGolferIndex
                                || f == (g_selectedGolferIndex ^ 1))
                            && g_golferPool[f].activity >= ACTIVITY_FOLLOW_THROUGH
                            && g_golferPool[f].activity != ACTIVITY_STEP_UP
                            && (!(g_golferPool[f].flags & GOLFER_WALKED_OFF) || (g_clock & 6))
                            && !(g_golferPool[f].flags & GOLFER_IN_CART))
                        || (g_runModeFlags & GAME_DEBUG_OVERLAY)) {
                        DrawTextCenteredAtY(g_printBuffer, g_rqAnchorX[q], g_rqAnchorY[q],
                            (g_golferPool[f].flags & GOLFER_WALKED_OFF) ? 0x80007d08u : 0x80007fffu);
                        m = g_golferPool[f].enjoyment;
                        bar = 0x80007d08u;
                        if (m > 2)
                            bar = 0x80006300u;
                        if (m > 4)
                            bar = 0x80001304u;
                        if (m > 6)
                            bar = 0x800023e8u;
                        if (m < 0)
                            bar = 0x80000000u;
                        if (g_toolbarView == TOOLBAR_GOLFERS && !(g_runModeFlags & GAME_TOURNAMENT)) {
                            g_simScreen.line(g_rqAnchorX[q] - 0xc, g_rqAnchorY[q] + 9, g_rqAnchorX[q] + 0xc,
                                g_rqAnchorY[q] + 9, COLOR_BLACK);
                            g_simScreen.line(g_rqAnchorX[q] - 0xc, g_rqAnchorY[q] + 9,
                                g_rqAnchorX[q] + (g_golferPool[f].enjoyment - 4) * 3, g_rqAnchorY[q] + 9, bar);
                        }
                    }
                }
            } else if (f & 0x800) {
                g_rqSprite[q]->BlitWithScaleState(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], 0x80007c00u, v, v, sz,
                    &g_palettes[g_rqFlags[q] & 0xff], 0xf8);
            } else if (f & 0x400) {
                g_rqSprite[q]->BlitWithScaleState(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], 0x80007fffu, v, v, sz,
                    &g_palettes[g_rqFlags[q] & 0xff], 0xf8);
            } else if (f & 0x200) {
                g_rqSprite[q]->draw_translucent(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], 0.6f, &g_palettes[f & 0x1ff], 1);
            } else {
                g_rqSprite[q]->BlitWithScaleOffset(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], g_shadeTable,
                    &g_palettes[g_rqFlags[q]]);
            }
        } else {
            g_rqSprite[q]->BlitScaled(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], g_colorLut, 0);
        }
    } else {
        spr->BlitScaled(g_pMenu, g_rqBlitX[q], g_rqBlitY[q], g_colorLut, 0);
    }

    if (g_rqScale[q] != 0) {
        if (g_zoomFromCamera) {
            v = g_cameraZoom;
            if (graphsys)
                graphsys->set_sprite_scale(v, v, 4);
        } else {
            v = g_simFrameZoomSnapshot;
            if (graphsys)
                graphsys->set_sprite_scale(v, v, 4);
        }
    }
}

void ProcessRenderQueue()
{
    FLCPalette pal;
    int i;

    for (i = g_rqHead; i != -1; i = g_rqNext[i])
        RenderQueuedSprite(i);
}

int IndexBelowOrNone(int i, int n)
{
    return i < n ? i : -1;
}


SpriteObject *g_bldgSprite = 0;

void RenderSpriteByType(int type, int level, int x, int y, int x2, int y2, int slot)
{
    int scale = 4, bounce, row, col, cell, id, phase, lift;
    int i, j, v, f, n, b;
    int landmark, bank, frame, bankOff, whiteFlag, sx, sy;
    short fl;

    {
        lift = g_cameraZoom * 20 / 4;
        row = g_buildingTable[slot].row;
        col = g_buildingTable[slot].col;
        int thr[11] = {0, 100, 180, 300, 430, 500, 550, 700, 810, 900, 1000};
        phase = (g_clock + type * 17) % 1000;
        for (i = 0; i <= 10; i++) {
            if (thr[i] > phase) {
                phase -= thr[i - 1];
                break;
            }
        }
    }
    {
        signed char rise[23] = {-55, -54, -52, -49, -45, -40, -34, -27, -19, -10, 0, -4, -7, -8, -7, -4, 0, -2, 0, 0, 0,
            0, 0};

        cell = row * 50 + col;
        fl = g_tileExtra[cell];
        if (fl & TILE_ANIMATING) {
            bounce = rise[g_tileAnimFrame[cell]];
            if (g_tileAnimFrame[cell] >= 20)
                g_tileExtra[cell] = fl & ~TILE_ANIMATING;
        }
    }

    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
    id = type + 0x90;
    switch (type) {
    case 4:
        landmark = g_buildingTable[slot].landmark;
        bank = landmark + 0x168;
        frame = (row + g_clock) % g_flicBankFrameLens[bank];
        if (landmark != 3)
            g_bldgSprite = GetFlicBankSprite(bank, frame, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
        else
            g_bldgSprite = GetFlicBankSprite(bank, frame, (g_buildingTable[slot].facing + g_camDirIdx) & 3);
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5,
                g_tileAnimFrame[cell] * 0.04 + 0.2, &g_palettes[landmark + 100], 1);
            break;
        }
        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, landmark + 100);
        return;

    case 3:
        f = 0;
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            f = g_tileAnimFrame[cell];
            if (f >= g_flicBankFrameLens[0x183]) {
                f = 0;
                g_tileExtra[cell] &= ~TILE_ANIMATING;
            }
        }
        g_bldgSprite = GetFlicBankSprite(0x183, f, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x1e, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x3d);
        return;

    case 15:
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(BANK_CLUBHOUSE, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[0x19], 1);
            break;
        }
        v = g_nextOpenHoleIndex > BACK_NINE_FIRST_HOLE;
        if (!v)
            id = 0x19;
        if ((g_currentThemeId != THEME_TROPICAL || v) && (g_currentThemeId != THEME_DESERT || !v)) {
            g_bldgSprite = GetFlicBankSprite(v ? 0x1f8 : 0x1f9, 0, g_camDirIdx / 2);
            AddSpriteToRenderQueue(x, y, 0, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        g_bldgSprite = GetFlicBankSprite(v + 0x1f6, 0, g_camDirIdx / 2);
        AddSpriteToRenderQueue(x, y, lift + y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        if (g_currentThemeId == THEME_DESERT)
            return;
        g_bldgSprite = GetFlicBankSprite(v + 0x1fa, g_clock % g_flicBankFrameLens[v + 0x1fa], g_camDirIdx / 2);
        AddSpriteToRenderQueue(x, y, lift + 1 + y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        return;

    case 17:
        g_bldgSprite = GetFlicBankSprite(0x246, 0, (g_camDirIdx / 2 - g_buildingTable[slot].facing - 1) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x50, 0x50, g_bldgSprite, SPRITE_FULL_SCALE, 0xb3);
        n = g_flicBankFrameLens[0x247];
        f = (g_clock / 2 + col * 13 + row * 7) % (n * 4);
        if (f >= n * 2)
            f = n * 3 - f - 1;
        f = Clamp(f, 0, n - 1);
        g_bldgSprite = GetFlicBankSprite(0x247, f, (g_camDirIdx / 2 - g_buildingTable[slot].facing - 1) & 3);
        AddSpriteToRenderQueue(x, y, y + 1, 0x50, 0x50, g_bldgSprite, SPRITE_FULL_SCALE, 0xb3);
        return;

    case 18:
        g_bldgSprite = GetFlicBankSprite(0x248, 0, (g_camDirIdx / 2 - g_buildingTable[slot].facing - 1) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x3c, 0x3c, g_bldgSprite, SPRITE_FULL_SCALE, 0xb3);
        n = g_flicBankFrameLens[0x249];
        f = (g_clock / 2 + col * 13 + row * 7) % (n * 4);
        if (f >= n * 2)
            f = n * 3 - f - 1;
        f = Clamp(f, 0, n - 1);
        g_bldgSprite = GetFlicBankSprite(0x249, f, (g_camDirIdx / 2 - g_buildingTable[slot].facing - 1) & 3);
        AddSpriteToRenderQueue(x, y, y + 1, 0x3c, 0x3c, g_bldgSprite, SPRITE_FULL_SCALE, 0xb3);
        break;

    case 7:
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(BANK_SNACK_BAR, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[0x10], 1);
            break;
        }
        if (g_nextOpenHoleIndex <= BACK_NINE_FIRST_HOLE) {
            if (g_currentThemeId != THEME_TROPICAL) {
                g_bldgSprite = GetFlicBankSprite(0x1ed, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
                DrawSpriteImmediate(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x10);
            }
            g_bldgSprite = GetFlicBankSprite(BANK_SNACK_BAR, g_clock % g_flicBankFrameLens[BANK_SNACK_BAR],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x10);
            if (g_currentThemeId != THEME_PARK)
                return;
            g_bldgSprite = GetFlicBankSprite(0x1d3, g_clock % g_flicBankFrameLens[0x1d3],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x10);
            return;
        }
        if (g_currentThemeId != THEME_TROPICAL && g_currentThemeId != THEME_DESERT) {
            g_bldgSprite = GetFlicBankSprite(0x1ef, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            DrawSpriteImmediate(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x97);
        }
        g_bldgSprite = GetFlicBankSprite(0x1ee, g_clock % g_flicBankFrameLens[BANK_SNACK_BAR],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x97);
        if (g_currentThemeId == THEME_TROPICAL) {
            g_bldgSprite = GetFlicBankSprite(0x1ef, g_clock % g_flicBankFrameLens[0x1ef],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x97);
        }
        if (g_currentThemeId != THEME_PARK)
            return;
        g_bldgSprite = GetFlicBankSprite(0x1d0, g_clock % g_flicBankFrameLens[0x1d0],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x97);
        return;

    case 13:
        if (level == 0)
            id = 0x11;
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1e6, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0xf0, y + (bounce - 0x30) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[id], 1);
            break;
        }
        g_bldgSprite = GetFlicBankSprite(level + 0x1e9, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, 0, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        g_bldgSprite = GetFlicBankSprite(level + 0x1e6, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        if (g_currentThemeId == THEME_PARK) {
            bankOff = level ? 5 : 2;
            g_bldgSprite = GetFlicBankSprite(bankOff + 0x1e6, g_clock % g_flicBankFrameLens[bankOff + 0x1e6],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId == THEME_TROPICAL) {
            if (level == 0)
                return;
            g_bldgSprite = GetFlicBankSprite(0x1eb, g_clock % g_flicBankFrameLens[0x1eb],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId == THEME_DESERT) {
            if (level == 0) {
                g_bldgSprite = GetFlicBankSprite(0x1e8, g_clock % g_flicBankFrameLens[0x1e8],
                    (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            } else {
                n = g_flicBankFrameLens[0x1eb];
                f = (row * 7 + g_clock + col * 13) % (n * 10);
                if (f >= n * 4)
                    f = n * 5 - f - 1;
                f = Clamp(f, 0, n - 1);
                g_bldgSprite = GetFlicBankSprite(0x1eb, f, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
                g_bldgSprite = GetFlicBankSprite(0x1ea, g_clock % g_flicBankFrameLens[0x1ea],
                    (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            }
        }
        if (g_currentThemeId == THEME_LINKS) {
            if (level == 0) {
                g_bldgSprite = GetFlicBankSprite(0x1e8, g_clock % g_flicBankFrameLens[0x1e8],
                    (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 2, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            } else {
                g_bldgSprite = GetFlicBankSprite(0x1eb, g_clock % g_flicBankFrameLens[0x1eb],
                    (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 2, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            }
        }
        return;

    case 9:
        if (level == 0)
            id = 0x1c;
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1d4, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[id], 1);
            break;
        }
        if (g_currentThemeId != THEME_PARK && g_currentThemeId != THEME_DESERT) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1d7, g_clock % g_flicBankFrameLens[level + 0x1d7],
                (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            AddSpriteToRenderQueue(x, y, g_currentThemeId == THEME_TROPICAL ? y + 1 : 0, 0x8c, 0x8c, g_bldgSprite,
                SPRITE_FULL_SCALE, id);
        }
        g_bldgSprite = GetFlicBankSprite(level + 0x1d4, g_clock % g_flicBankFrameLens[level + 0x1d4],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        if (g_currentThemeId != THEME_LINKS)
            return;
        if (level == 0)
            return;
        g_bldgSprite = GetFlicBankSprite(0x1d9, g_clock % g_flicBankFrameLens[0x1d9],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y + 2, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        return;

    case 8:
        if (level == 0)
            id = 0xe;
        v = level;
        if (g_flcFileNames[level + 0x1e0][0] == 0) {
            v = 0;
            scale = 5;
        }
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(v + 0x1e0, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[id], 1);
            break;
        }
        if (g_flcFileNames[v + 0x1e3][0] != 0 && (g_currentThemeId != THEME_TROPICAL || v == 0)) {
            g_bldgSprite = GetFlicBankSprite(v + 0x1e3, g_clock % g_flicBankFrameLens[v + 0x1e3],
                (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            AddSpriteToRenderQueue(x, y, g_currentThemeId == THEME_TROPICAL && v != 0 ? lift + y : 0, 0x8c, 0x8c,
                g_bldgSprite, scale, id);
        }
        g_bldgSprite = GetFlicBankSprite(v + 0x1e0, g_clock % g_flicBankFrameLens[v + 0x1e0],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, scale, id);
        if (g_currentThemeId == THEME_PARK) {
            if (v == 0)
                return;
            g_bldgSprite = GetFlicBankSprite(0x1e2, g_clock % g_flicBankFrameLens[0x1e2],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
            g_bldgSprite = GetFlicBankSprite(0x1e5, g_clock % g_flicBankFrameLens[0x1e5],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId == THEME_TROPICAL) {
            if (v != 0)
                return;
            g_bldgSprite = GetFlicBankSprite(0x1e4, g_clock % g_flicBankFrameLens[0x1e4],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId != THEME_LINKS)
            return;
        if (v == 0) {
            g_bldgSprite = GetFlicBankSprite(0x1e2, g_clock % g_flicBankFrameLens[0x1e2],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 2, 0x8c, 0x8c, g_bldgSprite, scale, id);
            return;
        }
        g_bldgSprite = GetFlicBankSprite(0x1e5, g_clock % g_flicBankFrameLens[0x1e5],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y + 2, 0x8c, 0x8c, g_bldgSprite, scale, id);
        return;

    case 6:
        v = g_buildingLevelPerKind[SCENERY_PUTTING_GREEN] == LEVEL_UPGRADED ? 3 : 0;
        g_bldgSprite = GetFlicBankSprite(v + 0x1f1, g_clock % g_flicBankFrameLens[v + 0x1f1],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x2, y2,
            g_currentThemeId == THEME_DESERT || g_currentThemeId == THEME_LINKS ? 0 : lift + y2, 0x28, 0x28,
            g_bldgSprite, SPRITE_FULL_SCALE, 0x14);
        g_bldgSprite = GetFlicBankSprite(v + 0x1f0, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x2, y2, y2, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x14);
        if (v) {
            int px, py;
            TileToScreenCached(row + 1, col + 1, &px, &py);
            g_bldgSprite = GetFlicBankSprite(0x1f5, g_clock % g_flicBankFrameLens[0x1f5],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            if (g_currentThemeId != THEME_DESERT)
                AddSpriteToRenderQueue(px, py, py, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x14);
            else
                AddSpriteToRenderQueue(px, py, py, 0x78, 0x78, g_bldgSprite, SPRITE_FULL_SCALE, 0x14);
        }
        for (whiteFlag = 0; whiteFlag < g_buildingLevelPerKind[SCENERY_PUTTING_GREEN] * 2 + 3; whiteFlag++) {
            sx = FixedPointSin(whiteFlag * 0x4aaaaaa6, (whiteFlag + 3) * g_cameraZoom * 2) + x;
            sy = FixedPointCos(whiteFlag * 0x4aaaaaa6, (whiteFlag + 3) * g_cameraZoom) + y;
            g_bldgSprite = GetFlicBankSprite(0x191, g_clock % g_flicBankFrameLens[0x191], 0);
            AddSpriteToRenderQueue(sx, sy, sy, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x14);
        }
        return;

    case 10:
        if (level == 0)
            id = 0x1a;
        g_buildingTable[slot].facing = 0;
        for (i = 0; i < level + 5; i++) {
            for (j = 0; j < level + 5; j++) {
                if (!WorldToScreen((row + i) * 1024 + 512, (col + j) * 1024 + 512, &x, &y, 0))
                    continue;
                if (i == 0) {
                    if (j == 0) {
                        g_bldgSprite =
                            GetFlicBankSprite(level + 0x1b6, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
                        if (level != 0 && g_currentThemeId != THEME_DESERT) {
                            g_bldgSprite = GetFlicBankSprite(0x1bf, g_clock % g_flicBankFrameLens[0x1bf],
                                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
                        }
                    } else {
                        g_bldgSprite = GetFlicBankSprite(level ? 0x1c0 : 0x1b8, 0,
                            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                        AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, id);
                        g_bldgSprite = GetFlicBankSprite(level ? 0x1c1 : 0x1b9, 0,
                            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                        DrawSpriteImmediate(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, id);
                    }
                } else if (j == 0 || j == level + 4) {
                    WorldToScreen((row + i) * 1024 + 512, (j ? col + 5 + level : col) * 1024, &x, &y, 0);
                    g_bldgSprite = GetFlicBankSprite(0x1be, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                    AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x1a);
                    WorldToScreen((row + i) * 1024, (j ? col + 5 + level : col) * 1024, &x, &y, 0);
                    AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x1a);
                }
                if (g_gameTileTerrainKind[(row + i) * 50 + col + j] == TERRAIN_GREEN) {
                    g_bldgSprite = GetFlicBankSprite(0x1bd, (i + j + g_clock) % g_flicBankFrameLens[0x1bd], 0);
                    AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, 3, 0x1a);
                } else if (j * 2 - 2 == i) {
                    if (i == 2) {
                        g_bldgSprite =
                            GetFlicBankSprite(0x1ba, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                        AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x1a);
                    } else if (i == 4) {
                        g_bldgSprite =
                            GetFlicBankSprite(0x1bb, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                        AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x1a);
                    } else if (i == 6) {
                        g_bldgSprite =
                            GetFlicBankSprite(0x1bc, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                        AddSpriteToRenderQueue(x, y, y, 0x28, 0x28, g_bldgSprite, SPRITE_FULL_SCALE, 0x1a);
                    }
                }
            }
        }
        return;

    case 11:
        if (level == 0)
            id = 0x17;
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1c2, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[id], 1);
            break;
        }
        g_bldgSprite = GetFlicBankSprite(level + 0x1c5, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        DrawSpriteImmediate(x, y, y, 0x8c, 0x8c, g_bldgSprite, -7, id);
        g_bldgSprite = GetFlicBankSprite(level + 0x1c2, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        if (g_currentThemeId == THEME_PARK) {
            bankOff = level ? 5 : 2;
            g_bldgSprite = GetFlicBankSprite(bankOff + 0x1c2, g_clock % g_flicBankFrameLens[bankOff + 0x1c2],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId == THEME_DESERT && level != 0) {
            g_bldgSprite = GetFlicBankSprite(0x1c7, g_clock % g_flicBankFrameLens[0x1c7],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        return;

    case 14:
        if (level == 0)
            id = 0x12;
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1da, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0xf0, y + (bounce - 0x30) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[id], 1);
            break;
        }
        if (g_currentThemeId != THEME_TROPICAL) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1dd, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            AddSpriteToRenderQueue(x, y, 0, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        g_bldgSprite = GetFlicBankSprite(level + 0x1da, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        if (g_currentThemeId == THEME_PARK) {
            bankOff = level ? 5 : 2;
            g_bldgSprite = GetFlicBankSprite(bankOff + 0x1da, g_clock % g_flicBankFrameLens[bankOff + 0x1da],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId == THEME_TROPICAL) {
            if (level == 0)
                f = IndexBelowOrNone(phase, g_flicBankFrameLens[0x1dc]);
            else
                f = g_clock % g_flicBankFrameLens[level + 0x1dc];
            if (f != -1) {
                g_bldgSprite =
                    GetFlicBankSprite(level + 0x1dc, f, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            }
            f = IndexBelowOrNone(phase, g_flicBankFrameLens[level + 0x1de]);
            if (level == 0)
                f = IndexBelowOrNone(g_clock % 40, g_flicBankFrameLens[0x1de]);
            if (f != -1) {
                g_bldgSprite =
                    GetFlicBankSprite(level + 0x1de, f, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            }
        }
        if (g_currentThemeId == THEME_LINKS) {
            if (level != 1)
                return;
            g_bldgSprite = GetFlicBankSprite(0x1dc, g_clock % g_flicBankFrameLens[0x1dc],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId == THEME_DESERT) {
            if (level == 0) {
                g_bldgSprite = GetFlicBankSprite(0x1dc, g_clock % g_flicBankFrameLens[0x1dc],
                    (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
                AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
                return;
            }
            g_bldgSprite = GetFlicBankSprite(0x1de, g_clock % g_flicBankFrameLens[0x1de],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            DrawSpriteImmediate(x, y, y, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            g_bldgSprite = GetFlicBankSprite(0x1df, g_clock % g_flicBankFrameLens[0x1df],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
            g_bldgSprite = GetFlicBankSprite(0x1d9, g_clock % g_flicBankFrameLens[0x1d9],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 2, 0xf0, 0xf0, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        return;

    case 12:
        if (level == 0)
            id = 0x1b;
        if (g_tileExtra[cell] & TILE_ANIMATING) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1ce, 0, (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            g_bldgSprite->draw_translucent(g_pMenu, x - 0x8c, y + (bounce - 0x1c) * 5, g_tileAnimFrame[cell] * 0.05,
                &g_palettes[id], 1);
            break;
        }
        if (g_currentThemeId != THEME_PARK) {
            g_bldgSprite = GetFlicBankSprite(level + 0x1d1, g_clock % g_flicBankFrameLens[level + 0x1d1],
                (g_buildingTable[slot].facing + g_camDirIdx / 2) & 3);
            DrawSpriteImmediate(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        g_bldgSprite = GetFlicBankSprite(level + 0x1ce, 0, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        if (g_currentThemeId == THEME_DESERT) {
            if (level == 0)
                return;
            g_bldgSprite = GetFlicBankSprite(0x1d0, g_clock % g_flicBankFrameLens[0x1d0],
                (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 1, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
            n = g_flicBankFrameLens[0x1d3];
            f = (row * 7 + g_clock + col * 13) % (n * 10);
            if (f >= n * 4)
                f = n * 5 - f - 1;
            f = Clamp(f, 0, n - 1);
            g_bldgSprite = GetFlicBankSprite(0x1d3, f, (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
            AddSpriteToRenderQueue(x, y, y + 2, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        }
        if (g_currentThemeId != THEME_PARK)
            return;
        g_bldgSprite = GetFlicBankSprite(level + 0x1d1, g_clock % g_flicBankFrameLens[level + 0x1d1],
            (g_camDirIdx / 2 + g_buildingTable[slot].facing) & 3);
        DrawSpriteImmediate(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, id);
        return;

    case 5:
        b = FindBuildingAtTile(row, col);
        if (g_buildingTable[b].count != 0)
            return;
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(g_buildingTable[b].score, g_golferIdText, 10));
        if (g_runModeFlags & GAME_DEBUG_OVERLAY)
            DrawTextCenteredWithShadow(g_printBuffer, x, y + 0xe, COLOR_BRIGHT_GREEN);
        switch (GetLotScoreTier(g_buildingTable[b].score)) {
        case 0:
            g_bldgSprite = GetFlicBankSprite(0x1c8, 0, (g_camDirIdx / 2 + row) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0xf);
            break;
        case 1:
            g_bldgSprite = GetFlicBankSprite(0x1c9, 0, (g_camDirIdx / 2 + row) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x13);
            break;
        case 2:
            g_bldgSprite = GetFlicBankSprite(0x1ca, 0, (g_camDirIdx / 2 + row) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x60);
            break;
        case 3:
            g_bldgSprite = GetFlicBankSprite(0x1ca, 0, (g_camDirIdx / 2 + row) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x60);
            break;
        case 4:
            DrawSpriteImmediate(x - g_tileSpan, y, y, 0x13, 0xd, g_homePlotGroundPics, 8, -1);
            DrawSpriteImmediate(g_tileSpan + x, y, y, 0x13, 0xd, g_homePlotGroundPics, 8, -1);
            DrawSpriteImmediate(x, y - lift, y, 0x13, 0xd, g_homePlotGroundPics, 8, -1);
            DrawSpriteImmediate(x, lift + y, y, 0x13, 0xd, g_homePlotGroundPics, 8, -1);
            g_bldgSprite = GetFlicBankSprite(0x1ca, 0, (g_camDirIdx / 2 + row) & 3);
            AddSpriteToRenderQueue(x, y, y, 0x8c, 0x8c, g_bldgSprite, SPRITE_FULL_SCALE, 0x60);
            break;
        }
        return;
    }
}


int g_rqUnused[4] = {40, 40, 1, 255};
HeadPreset g_headPresets[40] = {{0, {5, 10}, 0, "Praise the Lord for this fabulous day."},
    {1, {248, 9}, 0, "Dig my groovy head covers, PARTNER."},
    {0, {0, 5}, 0, "PARTNER, wouldn't you love a new set of encyclopedias?"},
    {1, {0, 0}, 0, "PARTNER, do you hear an ambulance?"}, {1, {0, 10}, 0, "Surf's up, dude."},
    {1, {4, 6}, 0, "Someday this will all be mine."}, {1, {2, 8}, 0, "I should be studying for finals."},
    {0, {2, 2}, 0, "A couple of aspirin will take care of that."},
    {1, {0, 2}, 0, "PARTNER, keep your head down - swing easy."},
    {1, {0, 0}, 0, "I'm in the mood for a hostile takeover."},
    {2, {0, 4}, 0, "Did you know those mushrooms are edible?"}, {1, {0, 253}, 0, "PARTNER, guess how much I'm worth."},
    {1, {4, 14}, 0, "I'm in the groove. C'mon let's move."}, {0, {0, 0}, 0, "Golf is cool, you know."},
    {1, {253, 252}, 0, "PARTNER, you appear to have some envy issues."},
    {0, {4, 9}, 0, "This course may require further study."},
    {1, {0, 4}, 0, "Get in touch with your inner swing plane."},
    {3, {2, 2}, 0, "PARTNER, just grip it and rip it man."}, {0, {0, 3}, 0, "Look at all shades of green."},
    {0, {0, 0}, 0, ""}, {0, {1, 3}, 0, "One, two, three, SWING!"}, {2, {0, 1}, 3, "This is my good side."},
    {1, {4, 10}, 0, "Could I be more perky?"}, {3, {0, 1}, 3, "Where does it hurt, PARTNER?"},
    {1, {2, 1}, 1, "You say hello, I say bub bye"}, {1, {0, 7}, 0, "...and I'm working on my tan."},
    {0, {1, 3}, 2, "Memo to self: stay in control."}, {0, {3, 3}, 2, "This just in: Yippee! Film at 11."},
    {2, {0, 2}, 0, "YOU CALL THAT A SWING, MAGGOT!"}, {1, {0, 0}, 4, "Now Bobby Jones, HE was a golfer."},
    {1, {2, 4}, 3, "I give myself two thumbs up!"}, {2, {0, 1}, 1, "PARTNER, do you like brownies?"},
    {0, {2, 2}, 0, "Ouch, I hurt my typing finger."}, {1, {1, 2}, 1, "So far, I give myself a B+."},
    {0, {0, 0}, 1, "PARTNER, I'm gonna request a mistrial."}, {0, {2, 2}, 2, "And my hair still looks fabulous."},
    {1, {1, 2}, 3, "I just about got this game figured out."}, {1, {0, 9}, 2, "I give that shot a perfect 10."},
    {2, {3, 3}, 1, "This course is a masterpiece."}, {1, {0, 0}, 0, ""}};
