#include "jgldefs.h"
#include "crt.h"
#include "MainLoop.h"
#include "DataPath.h"
#include "GameSetup.h"
#include "Geometry.h"
#include "GolferUpdate.h"
#include "InputPoll.h"
#include "ListDialog.h"
#include "Toolbar.h"
#include "WorldMap.h"

int g_pendingAccomp = -1;
int g_pendingWorldX = -1;
short g_mapMarker[16][2] = {
    {251, 13},
    {302, 64},
    {346, 115},
    {384, 166},
    {416, 220},
    {437, 277},
    {454, 337},
    {465, 398},
    {469, 458},
    {463, 520},
    {466, 13},
    {509, 64},
    {546, 115},
    {580, 166},
    {606, 220},
    {623, 277},
};
int g_trophySlot[10] = {2, 1, 3, 0, 4, 7, 6, 8, 5, 9};

int g_pendingWorldY = 0;
int g_lastInserted = 0;
char g_worldMapNumBuf[0x80] = {0};
static int s_unused3[3] = {0};
int g_accompTimer = 0;

void TriggerAccomplishment(int id, int worldX, int worldY)
{
    if (g_runModeFlags & GAME_CHAMPIONSHIP)
        return;
    if (g_accomp[id].awarded != 0)
        return;
    g_accomp[id].awarded = g_clock;
    g_accomp[id].tour = g_myWorldCourseIdx;
    g_pendingAccomp = id;
    g_accompTimer = 0;
    g_pendingWorldX = worldX;
    g_pendingWorldY = worldY;
}

void RenderAccomplishmentsScreen()
{
    int order[22];
    char nameBuf[0x28];
    Rect snapRect;
    char used[22];
    int row, won, n, sy, sx, savedTour, savedDay, i, k, x, y, best, bestVal, hit, yt, yu, yv;
    int snapX, snapY, stackY;
    memset(used, 0, sizeof(used));
    if (g_pendingAccomp == -1)
        return;
    if (g_pendingWorldX != -1) {
        if (WorldToScreen(g_pendingWorldX, g_pendingWorldY, &sx, &sy, 0)) {
            snapX = Clamp(sx - 100, 0, 600);
            snapY = Clamp(sy - 100, 0, 440);
            strcpy(nameBuf, "snapshots\\accomp");
            strcat(nameBuf, itoa(g_pendingAccomp, g_worldMapNumBuf, 10));
            strcat(nameBuf, ".bmp");
            snapRect.left = snapX;
            snapX += 0xc8;
            snapRect.top = snapY;
            snapRect.right = snapX;
            snapRect.bottom = snapY + 0xa0;
            g_simScreen.SaveSnapshot(nameBuf, &snapRect);
            g_pendingWorldX = -1;
        } else {
            g_cameraTileX = g_pendingWorldX >> 10;
            g_cameraTileY = g_pendingWorldY >> 10;
        }
    }
    if (++g_accompTimer < 0x14)
        return;
    if (g_pendingAccomp != 99)
        ConfigureAndPlaySound(SOUND_TADA, SOUND_FULL_VOLUME, 0, 0, 0);
    AssetSurface board;
    board.load_pcx("interface\\bulletinboard&mantlewood", &g_palette, 0, PALETTE_COLORS, 0);
    board.BlitRectToRect(&g_simScreen, 0, 0, 0x320, 0x258);
    row = 0;
    AssetSurface thumb;
    n = 0;
    for (;;) {
        best = -1;
        bestVal = 0x7ffffff;
        for (i = 0; i < 22; i++) {
            if (g_accomp[i].awarded != 0 && used[i] == 0 && g_accomp[i].awarded < bestVal) {
                bestVal = g_accomp[i].awarded;
                best = i;
            }
        }
        if (best == -1)
            break;
        order[n] = best;
        n++;
        used[best] = 1;
    }
    if (n > 0) {
        y = 0x1bb;
        yt = 0x10e;
        yu = 0x115;
        yv = 0x108;
        for (won = 0; won < n; y -= 0xe, yt -= 0xe, yu -= 0xe, yv -= 0xe, won++) {
            i = order[won];
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
            savedDay = g_clock;
            savedTour = g_myWorldCourseIdx;
            g_printBuffer[0] = 0;
            g_clock = g_accomp[i].awarded;
            g_myWorldCourseIdx = g_accomp[i].tour;
            AppendCourseName(0);
            strcat(g_printBuffer, "  ");
            strcat(g_printBuffer, itoa((g_clock % 1024) * 30 / 1024 + 1, g_worldMapNumBuf, 10));
            strcat(g_printBuffer, " ");
            FormatDateString(g_clock);
            g_myWorldCourseIdx = savedTour;
            g_clock = savedDay;
            x = ((row & 1) ? 0x217 : 0x14) + row * 0x23 % 50;
            g_trophyPartCells[33].draw_antialiased(&g_trophiesMasksBmps[33], g_pMenu, x - 7, yt, 0);
            DrawTextWithColor(g_printBuffer, x, y, 0x80002108);
            strcpy(g_printBuffer, "snapshots\\accomp");
            strcat(g_printBuffer, itoa(order[won], g_worldMapNumBuf, 10));
            strcat(g_printBuffer, ".bmp");
            thumb.LoadBMPToSurface16(g_printBuffer);
            row++;
            thumb.copy(&g_simScreen, 0, 0, x, yu, 0xc8, 0xa0);
            g_mapSiteMarkerPics[24 + row % 3].draw_antialiased(&g_mapMarkerButtonAlphas[24], g_pMenu, x + 0x64, yv, 0);
        }
    }
    g_trophyPartCells[34].draw_antialiased(&g_trophiesMasksBmps[34], g_pMenu, 0x258, 0x15e, 0);
    y = 0;
    hit = 0;
    for (i = 0; i < 22; i++) {
        if (g_accomp[i].awarded == 0) {
            k = i;
            if (g_level < 2) {
                if (i == 0)
                    k = 0x1d;
                else if (i == 1)
                    k = 0x1e;
                else if (i == 4)
                    k = 0x1f;
            }
            g_trophyPartCells[38 + k].draw_antialiased(&g_trophiesMasksBmps[38 + k], g_pMenu, y / 2 + 0x268, y + 0x190,
                0);
            hit++;
            y += g_trophyPartSize[i][1] - 4;
            if (hit >= 3)
                break;
        }
    }
    g_trophyPartCells[27].draw_antialiased(&g_trophiesMasksBmps[27], g_pMenu, 0x207, 0x18b, 0);
    g_trophyPartCells[28].draw_antialiased(&g_trophiesMasksBmps[28], g_pMenu, 0x0b1, 0x18b, 0);
    g_trophyPartCells[35].draw_antialiased(&g_trophiesMasksBmps[35], g_pMenu, 0x033, 0x1e2, 0);
    g_trophyPartCells[36].draw_antialiased(&g_trophiesMasksBmps[36], g_pMenu, 0x2dc, 0x1ff, 0);
    stackY = 0x14f - n * 14;
    g_trophyPartCells[23].draw_antialiased(&g_trophiesMasksBmps[23], g_pMenu, 0x11c, stackY - 0x1f, 0);
    g_trophyPartCells[26].draw_antialiased(&g_trophiesMasksBmps[26], g_pMenu, 0x120, stackY + 4, 0);
    g_trophyPartCells[26].draw_antialiased(&g_trophiesMasksBmps[26], g_pMenu, 0x120, stackY, 0);
    stackY += 0xe;
    for (i = 0; i < n; i++) {
        k = order[n - i - 1];
        if (g_level < 2) {
            if (k == 0)
                k = 0x1d;
            else if (k == 1)
                k = 0x1e;
            else if (k == 4)
                k = 0x1f;
        }
        g_trophyPartCells[k].draw_antialiased(&g_trophiesMasksBmps[k], g_pMenu, 0x120, stackY, 0);
        stackY += 0xe;
        if (i == 0) {
            g_trophyPartCells[25].draw_antialiased(&g_trophiesMasksBmps[25], g_pMenu, 0x120, stackY, 0);
            stackY += 2;
        }
    }
    g_trophyPartCells[22].draw_antialiased(&g_trophiesMasksBmps[22], g_pMenu, 0x120, 0x15f, 0);
    g_trophyPartCells[24].draw_antialiased(&g_trophiesMasksBmps[24], g_pMenu, 0x139, 0x1e4, 0);
    g_simScreen.RenderFullSurface(0);
    WaitWithTimeout(0x3c, 0);
    flush_input();
    do {
        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(0x1e, 0);
        do_all_tasks();
    } while (g_mouseButton == 0 && PollInputState() == 0);
    flush_input();
    g_mouseButton = 0;
    if (g_skillMasks[0] != 0 && g_pendingAccomp != 99) {
        g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.4f, 0, 1);
        k = 0;
        for (i = 0; i < 12; i++)
            k += g_golferSkillGrades[i];
        strcpy(g_printBuffer, "Add three skill points to your player...\n");
        g_menuJustDraw = 1;
        ShowListDialogWithParams(g_printBuffer, 0x50, 5);
        g_golferPool[0x9c].typeIdx = 0;
        g_golferPool[0x9c].ageFlags = g_golferTypeArray[0].ageBits;
        g_skillMasks[0] = RenderSkillPointsScreen(g_golferTypeArray[0].name, g_skillMasks[0],
            g_skillMasks[0] != 0 ? 3 : 13, 0x9c, 0xc8);
    }
    g_pendingAccomp = -1;
    flush_input();
    g_mouseButton = 0;
}

void ShowAccomplishmentsScreen()
{
    ConfigureAndPlaySound(0x7e, SOUND_FULL_VOLUME, 0, 0, 0);
    g_pendingAccomp = 99;
    g_accompTimer = 20;
    RenderAccomplishmentsScreen();
    g_pendingAccomp = -1;
    g_accompTimer = 0;
    FadeOutSound(0x7e);
}

int GetCoursePurchasePrice(int course)
{
    int price;
    price = 500;
    switch (course) {
    case 0: price = 500; break;
    case 1: price = 600; break;
    case 2: price = 700; break;
    case 3: price = 800; break;
    case 4: price = 1200; break;
    case 5: price = 1500; break;
    case 6: price = 2000; break;
    case 7: price = 2500; break;
    case 8: price = 3000; break;
    case 9: price = 4000; break;
    case 10: price = 5000; break;
    case 11: price = 6000; break;
    case 12: price = 7000; break;
    case 13: price = 8000; break;
    case 14: price = 9000; break;
    case 15: price = 10000;
    }
    return price * 100 / 100;
}

void InitWorldMapCourseList()
{
    int i, k, r, ok;
    signed char acres;
    memset(g_mapCourseList, 0, 17 * sizeof(Course));
    for (i = 0; i < 16; i++) {
        g_mapCourseList[i].kind = -1;
        g_mapCourseList[i].site = -1;
    }
    for (i = 12; i < 16; i++) {
        do {
            r = g_rng.NextInt(16);
            ok = 1;
            for (k = 12; k < i; k++)
                if (g_mapCourseList[k].kind == g_tourSites[r].landscape)
                    ok = 0;
        } while (!ok);
        g_mapCourseList[i].site = r;
        acres = (i + 4) * 10;
        g_mapCourseList[i].acres = acres;
        if (g_tourSites[r].coast == 0)
            g_mapCourseList[i].acres = acres + (i < 4 ? 10 : 20);
        if (g_tourSites[r].coast == 2)
            g_mapCourseList[i].acres -= (i < 4 ? 10 : 20);
        if (g_runModeFlags & GAME_SANDBOX)
            g_mapCourseList[i].acres = 250;
        g_mapCourseList[i].coast = g_tourSites[r].coast;
        g_mapCourseList[i].hilly = g_tourSites[r].relief;
        g_mapCourseList[i].kind = g_tourSites[r].landscape;
        g_mapCourseList[i].x = g_tourSites[r].x;
        g_mapCourseList[i].y = g_tourSites[r].y;
        g_mapCourseList[i].yearLeft = -1;
        g_mapCourseList[i].yearBought = -1;
        strcpy(g_mapCourseList[i].name, g_tourSites[r].city);
    }
    for (i = 0; i < 12; i++) {
        do {
            r = g_rng.NextInt(16);
            ok = 1;
            for (k = 0; k < 16; k++)
                if (g_mapCourseList[k].site == r)
                    ok = 0;
        } while ((i < 4 && i == 0 && g_tourSites[r].landscape != 0) || !ok);
        g_mapCourseList[i].site = r;
        acres = (i + 4) * 10;
        g_mapCourseList[i].acres = acres;
        if (g_tourSites[r].coast == 0)
            g_mapCourseList[i].acres = acres + (i < 4 ? 10 : 20);
        if (g_tourSites[r].coast == 2)
            g_mapCourseList[i].acres -= (i < 4 ? 10 : 20);
        if (g_runModeFlags & GAME_SANDBOX)
            g_mapCourseList[i].acres = 250;
        g_mapCourseList[i].coast = g_tourSites[r].coast;
        g_mapCourseList[i].hilly = g_tourSites[r].relief;
        g_mapCourseList[i].kind = g_tourSites[r].landscape;
        g_mapCourseList[i].x = g_tourSites[r].x;
        g_mapCourseList[i].y = g_tourSites[r].y;
        g_mapCourseList[i].yearLeft = -1;
        g_mapCourseList[i].yearBought = -1;
        strcpy(g_mapCourseList[i].name, g_tourSites[r].city);
    }
}

#define COURSE_PURCHASE_DISABLED 0

int ShowWorldMapScreen()
{
    int hover, sel, my, off, mx, tick, hold, tickmod, prev;
    int slot[16];
    int i, k, ci, state, price, best, d, qt, x, y, dx, dy, full;

    sel = -1;
    SpriteObject spr[8];
    FLCPalette pal;
    AssetSurface img;
    img.load_pcx("interface\\WorldButton.pcx", 0, 0, PALETTE_COLORS, 2);
    spr[0].extract(&img, 0xfa, 0x198, 0x4c, 0x72, 1, 1);
    spr[1].extract(&img, 0x2d4, 0x198, 0x4c, 0x72, 1, 1);
    spr[2].extract(&img, 0x226, 0x1ca, 0x4c, 0x40, 1, 1);
    spr[3].extract(&img, 0x190, 0x1ca, 0x4c, 0x40, 1, 1);
    spr[4].extract(&img, 0x226, 0x198, 0x4c, 0x32, 1, 1);
    spr[5].extract(&img, 0x190, 0x198, 0x4c, 0x32, 1, 1);
    spr[7].extract(&img, 0x2dc, 0x214, 0x44, 0x44, 1, 1);
    img.load_pcx("interface\\WorldBase.pcx", &pal, 0, PALETTE_COLORS, 0);
    spr[6].extract(&img, 0x2dc, 0x214, 0x44, 0x44, 1, 1);

    for (;;) {
        for (i = 0; i < 16; i++)
            slot[g_mapCourseList[i].site] = i;

        img.BlitRectToRect(&g_simScreen, 0, 0, 0x320, 0x258);
        if (g_clock == 0) {
            spr[0].BlitScaled(g_pMenu, 0x2d4, 0x198, 0);
        } else {
            spr[2].BlitScaled(g_pMenu, 0x2d4, 0x1ca, 0);
            spr[4].BlitScaled(g_pMenu, 0x2d4, 0x198, 0);
        }
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawTextCenteredAtY("Where will you build", 0x7e, 0x13, 0x80000000);
        DrawTextCenteredAtY("your golf course?", 0x7e, 0x21, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
        strcpy(g_printBuffer, "\xa7");
        FormatNumberWithCommas(g_gameCashHundreds * 100);
        if (g_runModeFlags & GAME_SANDBOX) {
            strcpy(g_printBuffer, "Unlimited ");
            strcat(g_printBuffer, "\xa7");
        }
        DrawTextCenteredAtY(g_printBuffer, 0x2e4, 0x12, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        g_mapSiteMarkerPics[17].draw_antialiased(&g_mapMarkerButtonAlphas[17], g_pMenu, 0x18, 0x226, 0);
        DrawTextWithColor("Available", 0x2c, 0x230, 0x80000000);
        g_mapSiteMarkerPics[16].draw_antialiased(&g_mapMarkerButtonAlphas[16], g_pMenu, 0x7c, 0x226, 0);
        DrawTextWithColor("Insufficient funds", 0x90, 0x230, 0x80000000);
        g_mapSiteMarkerPics[18].draw_antialiased(&g_mapMarkerButtonAlphas[18], g_pMenu, 0xf4, 0x226, 0);
        DrawTextWithColor("Already purchased", 0x108, 0x230, 0x80000000);

        i = 15;
        tickmod = sel % 16;
        for (; i >= 0; i--) {
            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
            strcpy(g_printBuffer, g_tourSites[i].city);
            ci = slot[i];
            state = 0;
            price = GetCoursePurchasePrice(ci);
            if (price <= g_gameCashHundreds)
                state = 1;
            off = ci;
            if (g_mapCourseList[ci].yearBought != -1)
                state = 0;
            if (sel == i) {
                if (state == 0)
                    g_mapSiteMarkerPics[g_tourSites[i].landscape * 4 + 2].BlitScaled(g_pMenu, g_mapMarker[i][0],
                        g_mapMarker[i][1], 0);
                else
                    state = 2;
            }
            g_mapSiteMarkerPics[g_tourSites[i].landscape * 4 + state].BlitScaled(g_pMenu, g_mapMarker[i][0],
                g_mapMarker[i][1], 0);
            DrawTextCenteredAtY(g_printBuffer, g_mapMarker[i][0] + 0x60, g_mapMarker[i][1] + 4, 0x80000000);
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
            strcpy(g_printBuffer, g_tourSites[i].feature);
            DrawTextCenteredAtY(g_printBuffer, g_mapMarker[i][0] + 0x68, g_mapMarker[i][1] + 0x10, 0x80000000);

            if (sel == i && g_mapCourseList[off].yearBought == -1) {
                strcpy(g_printBuffer, "Buy ");
                strcat(g_printBuffer, itoa(g_mapCourseList[off].acres, g_worldMapNumBuf, 10));
                strcat(g_printBuffer, " acres of ");
                switch (g_tourSites[i].relief) {
                case 0: strcat(g_printBuffer, "flat"); break;
                case 1: strcat(g_printBuffer, "rolling"); break;
                case 2: strcat(g_printBuffer, "hilly"); break;
                }
                DrawTextCenteredAtY(g_printBuffer, g_mapMarker[i][0] + 0x70, g_mapMarker[i][1] + 0x1e, COLOR_DARK_GREY);
                g_printBuffer[0] = 0;
                switch (g_tourSites[i].coast) {
                case 0: strcat(g_printBuffer, " inland "); break;
                case 1: strcat(g_printBuffer, " coastal "); break;
                case 2: strcat(g_printBuffer, " island "); break;
                }
                switch (g_tourSites[i].landscape) {
                case 2: strcat(g_printBuffer, "tropical"); break;
                case 0: strcat(g_printBuffer, "parkland"); break;
                case 3: strcat(g_printBuffer, "links"); break;
                case 1: strcat(g_printBuffer, "desert"); break;
                }
                strcat(g_printBuffer, " property");
                DrawTextCenteredAtY(g_printBuffer, g_mapMarker[i][0] + 0x6a, g_mapMarker[i][1] + 0x28, COLOR_DARK_GREY);
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, "for only \xa7");
                FormatNumberWithCommas(GetCoursePurchasePrice(slot[i]) * 100);
                strcat(g_printBuffer, ".");
                if (!(g_runModeFlags & GAME_SANDBOX))
                    DrawTextCenteredAtY(g_printBuffer, g_mapMarker[i][0] + 0x68, g_mapMarker[i][1] + 0x32,
                        COLOR_DARK_GREY);
                g_printBuffer[0] = 0;
            } else {
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_mapCourseList[off].acres, g_worldMapNumBuf, 10));
                strcat(g_printBuffer, " acres: \xa7");
                FormatNumberWithCommas(GetCoursePurchasePrice(slot[i]) * 100);
                if (g_mapCourseList[off].yearBought != -1) {
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(g_mapCourseList[off].yearBought + 2001, g_worldMapNumBuf, 10));
                    strcat(g_printBuffer, " - ");
                    if (g_mapCourseList[off].yearLeft != -1)
                        strcat(g_printBuffer, itoa(g_mapCourseList[off].yearLeft + 2001, g_worldMapNumBuf, 10));
                    else
                        strcat(g_printBuffer, "????");
                    if (sel == i)
                        DrawTextCenteredAtY("Already Purchased.", g_mapMarker[i][0] + 0x70, g_mapMarker[i][1] + 0x2d,
                            COLOR_DARK_GREY);
                }
                if (!(g_runModeFlags & GAME_SANDBOX))
                    DrawTextCenteredAtY(g_printBuffer, g_mapMarker[i][0] + 0x70, g_mapMarker[i][1] + 0x1e,
                        COLOR_DARK_GREY);
            }

            state = 0;
            price = GetCoursePurchasePrice(slot[i]);
            if (price <= g_gameCashHundreds)
                state = 1;
            if (g_mapCourseList[off].yearBought != -1)
                state = 2;
            if (tickmod == i && (tick & 8))
                state = 3;
            x = g_tourSites[i].x;
            y = g_tourSites[i].y;
            dx = 200 - x;
            dy = 300 - y;
            if (dx > 0) {
                if (dy > 0) {
                    qt = 4;
                    g_mapSiteMarkerPics[16 + state].draw_antialiased(&g_mapMarkerButtonAlphas[16 + state], g_pMenu,
                        x - 12, y - 14, 0);
                } else {
                    qt = 5;
                    g_mapSiteMarkerPics[20 + state].draw_antialiased(&g_mapMarkerButtonAlphas[20 + state], g_pMenu,
                        x - 15, y, 0);
                }
            } else {
                if (dy > 0) {
                    qt = 6;
                    g_mapSiteMarkerPics[24 + state].draw_antialiased(&g_mapMarkerButtonAlphas[24 + state], g_pMenu, x,
                        y - 14, 0);
                } else {
                    qt = 7;
                    g_mapSiteMarkerPics[28 + state].draw_antialiased(&g_mapMarkerButtonAlphas[28 + state], g_pMenu, x,
                        y, 0);
                }
            }
            g_mapSiteMarkerPics[qt * 4 + state].draw_antialiased(&g_mapMarkerButtonAlphas[qt * 4 + state], g_pMenu,
                g_mapMarker[i][0] + 0xa0, g_mapMarker[i][1] + 0xa, 0);
        }

        g_simScreen.get_mouse_pos(&mx, &my);
        hover = -1;
        if (ApproxDistance(mx - 0x300, my - 0x22d) < 0x19)
            hover = 3;
        if (ApproxDistance(mx - 0x2f8, my - 0x1dc) < 0x19)
            hover = (g_clock != 0);
        if (g_clock && ApproxDistance(mx - 0x306, my - 0x1a9) < 0x14)
            hover = 2;
        if (hover != -1) {
            switch (hover) {
            case 0: spr[1].BlitScaled(g_pMenu, 0x2d4, 0x198, 0); break;
            case 1: spr[3].BlitScaled(g_pMenu, 0x2d4, 0x1ca, 0); break;
            case 2: spr[5].BlitScaled(g_pMenu, 0x2d4, 0x198, 0); break;
            case 3: spr[7].BlitScaled(g_pMenu, 0x2dc, 0x214, 0); break;
            }
            if (prev == hover) {
                if (++hold > 0x14) {
                    switch (hover) {
                    case 0:
                        strcpy(g_printBuffer, "Reset World");
                        RenderToolbarTooltip(0x2f8, 0x1dc);
                        break;
                    case 1:
                        strcpy(g_printBuffer, "Save Game");
                        RenderToolbarTooltip(0x2f8, 0x1dc);
                        break;
                    case 2:
                        strcpy(g_printBuffer, "Load Game");
                        RenderToolbarTooltip(0x306, 0x1a9);
                        break;
                    case 3:
                        strcpy(g_printBuffer, "Cancel");
                        RenderToolbarTooltip(0x300, 0x22d);
                        break;
                    }
                }
            } else {
                hold = 0;
            }
            prev = hover;
        }

        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(5, 0);
        tick++;
        g_simScreen.get_mouse_pos(&mx, &my);
        best = 0x1e;
        sel = -1;
        for (k = 0; k < 16; k++) {
            d = ApproxDistance(mx - g_mapMarker[k][0] - 0x19, my - g_mapMarker[k][1] - 0x19);
            if (d < best) {
                best = d;
                sel = k;
            }
            d = ApproxDistance(mx - g_tourSites[k].x, my - g_tourSites[k].y);
            if (d < best) {
                best = d;
                sel = k;
            }
        }
        ProcessMouseMessages();
        if (g_mouseButton == 0)
            continue;
        if (hover == 0) {
            InitWorldMapCourseList();
            continue;
        }
        if (hover == 1) {
            DrawDialogFrameWithBorder(0x78, 0x50, 0x210, 0x50, 0);
            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
            DrawTextCenteredWithShadow("SAVE GAME: edit name then press Enter.", 0x180, 0x58, COLOR_WHITE);
            g_printBuffer[0] = 0;
            AppendCourseName(-1);
            strcat(g_printBuffer, " ");
            strcat(g_printBuffer, itoa((g_clock % 1024) * 30 / 1024 + 1, g_worldMapNumBuf, 10));
            strcat(g_printBuffer, " ");
            FormatDateString(g_clock);
            ShowTextInputDialog("Save File Name...", 0x80, 0x66, 0x28);
            if (g_printBuffer[0] == 0)
                continue;
            SaveGame(g_printBuffer);
            strcpy(g_printBuffer, "Game Saved.");
            ShowStoryMessage(0x80001284, 1, -4);
            continue;
        }
        if (hover == 2) {
            if (ShowLoadGameScreen(g_clock == 0, 0))
                break;
            continue;
        }
        if (hover == 3)
            break;
        if (g_mouseButton != 1)
            continue;
        if (sel == -1)
            continue;
        ci = slot[sel];
        full = 0;
        if (g_mapCourseList[ci].yearBought != -1)
            continue;
        strcpy(g_printBuffer, "You need more money before ");
        strcat(g_printBuffer, "you can purchase this property.");
        if (g_unlockedBldgTypeLimit == FACILITIES_ALL)
            full = 1;
        price = GetCoursePurchasePrice(ci);
        if (g_gameCashHundreds < price && !full) {
            RenderSpeechBubbleWithPortrait(0xc8, 0xc8, 0x80006000, -2);
            g_simScreen.RenderFullSurface(full);
            WaitForInputWithTimeout(full);
            continue;
        }
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawPanelFrame(0xcc, 0x114, 0xc0, 0x30, 1);
        strcpy(g_printBuffer, "We're off to ");
        strcat(g_printBuffer, g_mapCourseList[ci].name);
        strcat(g_printBuffer, "!");
        DrawTextCenteredWithShadow(g_printBuffer, 0x12c, 0x11d, COLOR_WHITE);
        strcpy(g_printBuffer, "... one moment please ...");
        DrawTextCenteredWithShadow(g_printBuffer, 0x12c, 0x12c, COLOR_WHITE);
        g_simScreen.RenderFullSurface(0);
        if (COURSE_PURCHASE_DISABLED)
            break;
        g_gameCashHundreds -= GetCoursePurchasePrice(ci);
        g_shownLandmarkBitmask |= 1 << ci;
        switch (g_mapCourseList[ci].kind) {
        case 3: ConfigureAndPlaySound(g_rng.NextInt(3) + 0x73, SOUND_FULL_VOLUME, 0, 0, 0); break;
        case 2: ConfigureAndPlaySound(g_rng.NextInt(3) + 0x6e, SOUND_FULL_VOLUME, 0, 0, 0); break;
        case 1: ConfigureAndPlaySound(g_rng.NextInt(3) + 0x78, SOUND_FULL_VOLUME, 0, 0, 0); break;
        case 0: ConfigureAndPlaySound(SOUND_PURCHASE, SOUND_FULL_VOLUME, 0, 0, 0); break;
        }
        flush_input();
        g_mouseButton = 0;
        return ci;
    }
    flush_input();
    g_mouseButton = 0;
    return -1;
}

int ChooseWorldMapCourse(int mode)
{
    return ShowWorldMapScreen();
}

void SetTerrainTileType(int row, int col)
{
    char coast = g_mapCourseList[g_myWorldCourseIdx].coast;
    g_gameTileTerrainKind[row * 50 + col] = (coast == 2) ? 17 : 20;
    if (coast == 2)
        g_tileExtra[row * 50 + col] &= ~(TILE_PATH | TILE_SCENIC | TILE_BENCH);
}

int SpawnEmployee(int job);

void GenerateCourseMap(int course)
{
    int site, fill, unlimited, i;
    signed char theme;

    g_myWorldCourseIdx = course;
    site = g_mapCourseList[course].site;
    theme = g_mapCourseList[course].kind;
    g_mapCourseList[course].yearBought = (signed char)g_careerPlayYear;
    g_currentThemeId = theme;
    switch (g_mapCourseList[course].hilly) {
    case 0: g_terrainHeightDivisor = 0x30; break;
    case 1: g_terrainHeightDivisor = 0x20; break;
    case RELIEF_HILLY: g_terrainHeightDivisor = 0x10; break;
    }
    switch (course / 4) {
    case 0: g_terrainHeightDivisor += g_terrainHeightDivisor / 2; break;
    case 1: g_terrainHeightDivisor += g_terrainHeightDivisor / 4; break;
    case 3: g_terrainHeightDivisor += -(g_terrainHeightDivisor / 4); break;
    }
    unlimited = g_runModeFlags & GAME_SANDBOX;
    if (unlimited)
        g_terrainHeightDivisor += 0x10;

    switch (g_currentThemeId) {
    case THEME_PARK: fill = 4; break;
    case THEME_DESERT: fill = 4; break;
    case THEME_TROPICAL: fill = 4; break;
    case THEME_LINKS: fill = 4; break;
    case 4: memset(g_gameTileTerrainKind, 14, sizeof g_gameTileTerrainKind); break;
    }
    if (!unlimited) {
        if (course >= 8)
            fill = (theme == THEME_DESERT) ? 12 : 5;
        if (course >= 12)
            fill = (theme == THEME_DESERT) ? 12 : 11;
    }

    {
        int type, budget, total, n, row, col, i;

        i = 0;
        total = 0;
        memset(g_gameTileTerrainKind, fill, sizeof g_gameTileTerrainKind);
        budget = 5000;
        memset(g_simGolferWalkCostMap, g_squareTypeTable[g_gameTileTerrainKind[0]].walk, 2500);
        memset(g_ballStopCountMap, 0, 2500);
        memset(g_posReactionTileCount, 0, 2500);
        memset(g_gameDispleasedTally, 0, 2500);
        memset(g_tileVariationIdx, 0, sizeof g_tileVariationIdx);
        do {
            n = 0;
            row = g_rng.NextInt(50);
            col = g_rng.NextInt(50);
            switch (g_currentThemeId) {
            case THEME_PARK:
                type = (col >= 25) ? 14 : 13;
                if (site == 9)
                    type = 14;
                break;
            case THEME_LINKS: type = (i & 1) ? 11 : 5; break;
            case THEME_DESERT: type = (i & 1) ? 12 : 14; break;
            case THEME_TROPICAL: type = (i & 1) ? ((i & 2) ? 13 : 12) : 14; break;
            case 4: type = (i & 1) ? 12 : 4; break;
            }
            while (row >= 0 && col >= 0 && row < 50 && col < 50 && n < budget / 128) {
                g_gameTileTerrainKind[row * 50 + col] = type;
                n++;
                if (n == 48)
                    g_tileExtra[row * 50 + col] |= TILE_NAMED;
                g_simGolferWalkCostMap[row][col] = g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].walk;
                if (g_rng.NextInt(64) == 0)
                    g_tileExtra[row * 50 + col] |= TILE_SCENIC;
                else
                    g_tileExtra[row * 50 + col] &= ~TILE_SCENIC;
                row += g_rng.NextInt(3) - 1;
                col += g_rng.NextInt(3) - 1;
            }
            total += n;
            if (total > 1250)
                break;
            i++;
            budget += 2500;
        } while (budget < 45000);
    }

    {
        int rivers = 1;

        if (g_mapCourseList[course].coast == 2)
            rivers = 0;

        for (i = 0; i < rivers; i++) {
            int row, col, nr, nc, best, dir, prevDir, bestDir;

            row = ((i * 5 + 5) * 10) / 3 + g_rng.NextInt(16);
            col = 49;
            prevDir = 0;
            while (1) {
                signed char t;
                if (row < 0 || row > 49)
                    break;
                t = (g_currentThemeId == THEME_DESERT) ? TERRAIN_BRUSH : TERRAIN_WATER;
                g_gameTileTerrainKind[row * 50 + col] = t;
                g_simGolferWalkCostMap[row][col] = g_squareTypeTable[t].walk;
                g_tileExtra[row * 50 + col] = (g_tileExtra[row * 50 + col] & ~TILE_SCENIC) | TILE_FLOWERS;
                if (g_rng.NextInt(64) == 0)
                    g_tileExtra[row * 50 + col] |= TILE_SCENIC;
                best = 999;
                nc = col + 1;
                nr = row + (g_rng.NextInt(2) ? 1 : -1);
                if (g_gameTileTerrainKind[nr * 50 + nc] != TERRAIN_WATER && GetTileElevation(nr, nc, 0) == 3) {
                    g_gameTileTerrainKind[nr * 50 + nc] = i ? 12 : ((~((nr + nc) * 2) & 4) | 8);
                    if (g_currentThemeId == THEME_DESERT)
                        g_gameTileTerrainKind[nr * 50 + nc] = TERRAIN_RAVINE;
                }
                for (dir = -2; dir <= 2; dir += 2) {
                    int tr = g_dirRowStep[dir & 7] + row;
                    int tc = g_dirColStep[dir & 7] + col;
                    if (i == 0 || tr < 48) {
                        int h = CalculateTerrainHeight(tr * 128, tc * 128);
                        if (h < best) {
                            best = h;
                            nr = tr;
                            nc = tc;
                            bestDir = dir;
                        }
                    }
                }
                if (bestDir == -prevDir)
                    col--;
                else {
                    row = nr;
                    col = nc;
                }
                prevDir = bestDir;
                if (g_gameTileTerrainKind[row * 50 + col] == TERRAIN_WATER)
                    break;
                if (col < 0)
                    break;
            }
        }
    }

    {
        int tiles = 0;
        int w = 5;

        if (g_mapCourseList[course].coast == 1) {
            for (int i = 0; i < 50; i++) {
                int c;
                for (c = 0; c < w; c++) {
                    if (g_gameTileTerrainKind[i * 50 + c] != TERRAIN_WATER) {
                        signed char t = (g_currentThemeId == 4) ? 12 : 17;
                        g_gameTileTerrainKind[i * 50 + c] = t;
                        g_simGolferWalkCostMap[i][c] = g_squareTypeTable[t].walk;
                        g_tileExtra[i * 50 + c] &= ~TILE_SCENIC;
                        if (c == w - 1 && g_rng.NextInt(16) == 0)
                            g_tileExtra[i * 50 + c] |= TILE_SCENIC;
                    }
                    if (c == 0)
                        g_gameTileTerrainKind[i * 50 + 0] = TERRAIN_OUT_OF_BOUNDS;
                    tiles++;
                }
                w += g_rng.NextInt(5) - 2;
                if (w <= 1)
                    w = 2;
                else if (w > 12)
                    w--;
            }
        }
    }

    {
        int r, c;

        for (r = 0; r < 50; r++)
            for (c = 0; c < 50; c++)
                g_tileExtra[r * 50 + c] &= ~TILE_FLOWERS;

        for (r = 0; r <= 50; r++)
            for (c = 0; c <= 50; c++)
                g_pointElevationMap[r][c] = GetTileElevation(r, c, 0);
    }

    for (i = 0; i < 24; i++) {
        int r, c, k, type;

        r = g_rng.NextInt(4) + (i % 4) * 50 / 4 + 4;
        c = g_rng.NextInt(4) + (i / 4) * 50 / 4 + 4;
        switch (g_currentThemeId) {
        case THEME_PARK: type = (g_pointElevationMap[r][c] > ELEV_SEA_LEVEL) ? 12 : 17; break;
        case THEME_DESERT: type = (g_pointElevationMap[r][c] > ELEV_SEA_LEVEL) ? 12 : 15; break;
        case 4: type = (g_pointElevationMap[r][c] > 4) ? 12 : 17; break;
        case THEME_TROPICAL: type = (g_pointElevationMap[r][c] > ELEV_SEA_LEVEL) ? 15 : 17; break;
        case THEME_LINKS: type = (g_pointElevationMap[r][c] > ELEV_SEA_LEVEL) ? 15 : 12; break;
        }
        if ((g_level >= 2 && g_rng.NextInt(8 / (g_level - 1)) == 0) || i == 0)
            type = 18;
        if (site == 2 && (type == 8 || type == 18))
            type = 12;
        k = 0;
        do {
            if ((g_tileExtra[r * 50 + c] & TILE_FLOWERS) == 0) {
                g_gameTileTerrainKind[r * 50 + c] = type;
                if (g_rng.NextInt(8) == 0 && type == 18)
                    g_tileExtra[r * 50 + c] |= TILE_SCENIC;
                g_simGolferWalkCostMap[r][c] = g_squareTypeTable[g_gameTileTerrainKind[r * 50 + c]].walk;
                if (type == 17)
                    g_pointElevationMap[r][c] = ELEV_SEA_LEVEL;
            }
            if (g_rng.NextInt(2))
                r += g_rng.NextInt(2) ? 1 : -1;
            else
                c += g_rng.NextInt(2) ? 1 : -1;
        } while (r >= 0 && r < 50 && c >= 0 && c < 50 && (++k <= 4 || g_rng.NextInt(16) != 0));
    }

    {
        int r, c, placed;

        for (int i = 0; i < 16; i++) {
            c = g_rng.NextInt(42);
            r = g_rng.NextInt(42);
            g_tileExtra[(r + 4) * 50 + c + 4] |= TILE_SCENIC;
        }

        for (r = 0; r < 50; r++)
            for (c = 0; c < 50; c++) {
                g_tileExtra[r * 50 + c] &= ~TILE_SCENIC;
                if (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_ELM_TREE)
                    g_gameTileTerrainKind[r * 50 + c] = TERRAIN_TREE;
            }

        placed = 0;
        do {
            r = g_rng.NextInt(46) + 2;
            c = g_rng.NextInt(46) + 2;
            if (!IsPositionInvalid(r, c)) {
                signed char t = g_gameTileTerrainKind[r * 50 + c];
                if (t != TERRAIN_HOME_SITE && t != TERRAIN_BUILDING && t != TERRAIN_ROUGH && t != TERRAIN_WETLANDS
                    && t != ((g_currentThemeId == THEME_DESERT) ? TERRAIN_BRUSH : TERRAIN_RAVINE)
                    && (t != TERRAIN_WATER || c > 6)) {
                    g_tileExtra[r * 50 + c] |= TILE_SCENIC;
                    placed++;
                }
            }
        } while (placed < (4 - g_level) * 9);
    }

    for (i = 0; i <= 50; i++) {
        int c;
        for (c = 0; c <= 50; c++) {
            if (g_gameTileTerrainKind[i * 50 + c] == TERRAIN_WATER
                || g_gameTileTerrainKind[i * 50 + c] == TERRAIN_WETLANDS) {
                signed char dr[5] = {0, 1, 1, 0, 0};
                signed char dc[5] = {-1, -1, 0, 0, -1};
                int k;
                for (k = 0; k < 4; k++) {
                    int nc = dc[k] + c;
                    if (dr[k] + i >= 0 && dr[k] + i < 50 && nc >= 0 && nc < 50
                        && g_gameTileTerrainKind[(g_dirRowStep[k * 2] + i) * 50 + g_dirColStep[k * 2] + c]
                               != TERRAIN_WATER) {
                        unsigned char *v = &g_pointElevationMap[dr[k] + i][dc[k] + c];
                        if (*v > 3 && *v == g_pointElevationMap[dr[k + 1] + i][dc[k + 1] + c]) {
                            if (*v == 4)
                                *v = 3;
                            else {
                                unlimited = g_rng.NextInt(2);
                                *v += unlimited ? 1 : -1;
                            }
                        }
                    }
                }
            }
        }
    }

    {
        int hr, hc, bt;

        do {
            hr = g_rng.NextInt(17) + 15;
            hc = g_rng.NextInt(17) + 15;
        } while (
            g_gameTileTerrainKind[hr * 50 + hc] != fill
            || ValidateBuildingPlacement(hr, hc, g_facilityTypes[SCENERY_CLUBHOUSE].side, SCENERY_CLUBHOUSE) == -1);
        PlaceBuildingOnTerrain(hr, hc, SCENERY_CLUBHOUSE, CLUBHOUSE_TILE_FLAGS);
        g_buildingTable[0].flags |= SLOT_REACHABLE;
        g_allHolesArray[CLUBHOUSE_HOLE].secondTeeX = hr;
        g_allHolesArray[CLUBHOUSE_HOLE].teeX = hr;
        g_tileExtra[hr * 50 + hc] &= ~TILE_PATH;
        g_allHolesArray[CLUBHOUSE_HOLE].secondTeeY = hc;
        g_allHolesArray[CLUBHOUSE_HOLE].teeY = hc;
        g_cameraTileX = hr + 6;
        g_cameraTileY = hc - 4;

        switch (site) {
        case 9: bt = 6; goto gate;

        case 14: bt = 8; goto gate;

        case 2: bt = 13; goto gate;

        case 11: bt = 14; goto gate;

        case 4:
            bt = 9;
gate:
            if (!IsPositionInvalid(hr, hc + 5)) {
                PlaceBuildingOnTerrain(hr - 1, hc + 4, bt, 0);
                g_tileExtra[hr * 50 + hc + 2] |= TILE_PATH;
                g_tileExtra[hr * 50 + hc + 3] |= TILE_PATH;
                g_tileExtra[hr * 50 + hc + 4] |= TILE_PATH;
            } else {
                PlaceBuildingOnTerrain(hr - 1, hc - 5, bt, 0);
                g_tileExtra[hr * 50 + hc - 1] |= TILE_PATH;
                g_tileExtra[hr * 50 + hc - 2] |= TILE_PATH;
                g_tileExtra[hr * 50 + hc - 3] |= TILE_PATH;
            }
            UpdateBuildingAccessibility();
            if (site == 4 || site == 3) {
                int r, c;
                do {
                    r = g_rng.NextInt(40) + 5;
                    c = g_rng.NextInt(40) + 5;
                } while (ValidateBuildingPlacement(r, c, 2, 4) == -1);
                g_buildingTable[PlaceBuildingOnTerrain(r, c, 4, 0)].landmark = 14;
            }
            break;

        case 7: {
            int n;
            for (n = 0; n < 2; n++) {
                int r, c, id;
                r = (n ? g_rng.NextInt(10) : -g_rng.NextInt(10)) + 25;
                for (c = 0; (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_WATER
                                || g_gameTileTerrainKind[r * 50 + c] == TERRAIN_OUT_OF_BOUNDS)
                            && c < 48;
                    c++)
                    ;
                id = PlaceBuildingOnTerrain(r, c, 4, 0);
                g_buildingTable[id].landmark = g_rng.NextInt(2) ? 7 : 13;
                g_pointElevationMap[r][c] += 2;
            }
            break;
        }

        case 1: {
            int n;
            for (n = 0; n < 8 - g_level; n++) {
                int r, c;
                do {
                    r = g_rng.NextInt(30) + 10;
                    c = g_rng.NextInt(30);
                } while (g_gameTileTerrainKind[r * 50 + c] != TERRAIN_WATER);
                g_tileExtra[r * 50 + c] |= TILE_SCENIC;
            }
            break;
        }

        case 5:
        case 6:
        case 8:
        case 12:
        case SITE_SPAIN:
        case 15: {
            int r, c, d, id;

            do {
                r = g_rng.NextInt(40) + 5;
                c = g_rng.NextInt(40) + 5;
            } while (ValidateBuildingPlacement(r - 2, c - 2, 5, SCENERY_CLUBHOUSE) == -1);
            for (int i = 0; i < 8 - g_level; i++) {
                g_rng.NextInt(7);
                g_rng.NextInt(7);
                r = g_rng.NextInt(40) + 5;
                c = g_rng.NextInt(40) + 5;
                if (ValidateBuildingPlacement(r - 1, c - 1, 3, 7) == -1
                    || g_gameTileTerrainKind[r * 50 + c] == TERRAIN_WATER)
                    continue;
                if (site == 5) {
                    g_gameTileTerrainKind[r * 50 + c] = TERRAIN_WATER;
                    g_pointElevationMap[r + 1][c - 1] = 6;
                    g_rng.NextInt(4);
                    for (d = 0; d < 8; d++) {
                        int k = d & 7;
                        int tr = g_dirRowStep[k] + r;
                        int tc = g_dirColStep[k] + c;
                        switch (d) {
                        case 0:
                            g_gameTileTerrainKind[tr * 50 + tc] = TERRAIN_WATER;
                            g_pointElevationMap[tr + 1][tc - 1] = 6;
                            break;
                        case 1:
                        case 7:
                            g_gameTileTerrainKind[tr * 50 + tc] = TERRAIN_DEEP_ROUGH;
                            g_pointElevationMap[tr + 1][tc - 1] = 6;
                            break;
                        case 6:
                            g_gameTileTerrainKind[tr * 50 + tc] = TERRAIN_WATER;
                            g_pointElevationMap[tr + 1][tc - 1] = 6;
                            break;
                        case 2:
                            g_gameTileTerrainKind[tr * 50 + tc] = TERRAIN_WATER;
                            g_pointElevationMap[tr + 1][tc - 1] = ELEV_SEA_LEVEL;
                            break;
                        default:
                            if (g_rng.NextInt(2))
                                g_gameTileTerrainKind[tr * 50 + tc] = TERRAIN_WATER;
                            g_pointElevationMap[tr][tc - 1] = ELEV_SEA_LEVEL;
                            break;
                        }
                    }
                } else {
                    if (site == 15 || site == 12 || site == 8) {
                        id = PlaceBuildingOnTerrain(r, c, 4, 0);
                        g_buildingTable[id].landmark = 11;
                        if (site == 8)
                            g_buildingTable[id].landmark = g_rng.NextInt(2) ? 6 : 2;
                        else if (site == 12)
                            g_buildingTable[id].landmark = 3;
                        g_buildingTable[id].facing = g_rng.NextInt(4);
                        for (d = 0; d < 8; d++) {
                            if (g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c]
                                    != TERRAIN_BUILDING
                                && g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c]
                                       != TERRAIN_WATER) {
                                if (i & 1) {
                                    g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] =
                                        (site == 12) ? 12 : 4;
                                    if (site == 15)
                                        g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] =
                                            TERRAIN_WATER;
                                    else if (d == 0) {
                                        g_pointElevationMap[r][c] += 2;
                                        g_pointElevationMap[r + 1][c] += 2;
                                        g_pointElevationMap[r][c - 1] += 2;
                                        g_pointElevationMap[r + 1][c - 1] += 2;
                                    }
                                } else {
                                    g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] =
                                        TERRAIN_DEEP_ROUGH;
                                    g_tileExtra[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] |=
                                        ((d & 1) << 9) | 0x20;
                                }
                            }
                        }
                    }
                    if (site == SITE_IRELAND) {
                        id = PlaceBuildingOnTerrain(r, c, 4, 0);
                        g_buildingTable[id].landmark = 1;
                        g_buildingTable[id].facing = g_rng.NextInt(4);
                        for (d = 0; d < 8; d++) {
                            if (g_squareTypeTable[g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d]
                                                                        + c]]
                                    .category
                                == CATEGORY_ROUGH) {
                                g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] =
                                    TERRAIN_DEEP_ROUGH;
                                if (g_rng.NextInt(6) == 0)
                                    g_tileExtra[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] |= TILE_SCENIC;
                            }
                        }
                    } else if (site == SITE_SPAIN) {
                        id = PlaceBuildingOnTerrain(r, c, 4, 0);
                        g_buildingTable[id].landmark = 1;
                        g_buildingTable[id].facing = g_rng.NextInt(4);
                        for (d = 0; d < 8; d++) {
                            if (g_squareTypeTable[g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d]
                                                                        + c]]
                                    .category
                                == CATEGORY_TREES)
                                g_buildingTable[id].landmark = 10;
                            if (g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c]
                                == TERRAIN_WATER) {
                                g_buildingTable[id].landmark = 9;
                                break;
                            }
                        }
                        for (d = 0; d < 8; d++) {
                            if (g_buildingTable[id].landmark == 1) {
                                g_tileExtra[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] |= TILE_SCENIC;
                                g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] = TERRAIN_BRUSH;
                            }
                            if (g_buildingTable[id].landmark == 3
                                && g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c]
                                       == TERRAIN_ROUGH)
                                g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] =
                                    TERRAIN_RAVINE;
                            if (g_buildingTable[id].landmark == 9
                                && g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c]
                                       == TERRAIN_ROUGH
                                && g_rng.NextInt(3) == 0)
                                g_tileExtra[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] |= TILE_SCENIC;
                        }
                    } else if (site == 6) {
                        id = PlaceBuildingOnTerrain(r, c, 4, 0);
                        g_buildingTable[id].landmark = ((i & 1) ? 0x174 : 0x170) - 0x168;
                        g_buildingTable[id].facing = g_rng.NextInt(4);
                        for (d = 0; d < 8; d++) {
                            if (i & 1)
                                g_tileExtra[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] |= TILE_FLOWERS;
                            else if (g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c]
                                     != TERRAIN_WATER)
                                g_tileExtra[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] |= ((d & 1) << 9) | 0x20;
                        }
                    }
                }
            }
            break;
        }

        case 0:
        case SITE_IRELAND: {
            int n = 0;
            do {
                int r = g_rng.NextInt(46) + 2;
                int c = g_rng.NextInt(46) + 2;
                if (!IsPositionInvalid(r, c)) {
                    signed char t = g_gameTileTerrainKind[r * 50 + c];
                    if (t != TERRAIN_HOME_SITE && t != TERRAIN_BUILDING && t != TERRAIN_WATER
                        && (g_tileExtra[r * 50 + c] & TILE_BUILDING) == 0) {
                        g_tileExtra[r * 50 + c] |= TILE_SCENIC;
                        if (site == 0)
                            g_gameTileTerrainKind[r * 50 + c] = TERRAIN_BRUSH;
                        else if (site == SITE_SPAIN)
                            g_gameTileTerrainKind[r * 50 + c] = TERRAIN_BRUSH;
                        else if (site == 15)
                            g_gameTileTerrainKind[r * 50 + c] = TERRAIN_PALM_TREE;
                        else if (site == SITE_IRELAND)
                            g_gameTileTerrainKind[r * 50 + c] = TERRAIN_ROUGH;
                        n++;
                    }
                }
            } while (n < 16);
            break;
        }
        }
    }

    for (i = 0; i < g_level; i++) {
        int r, c, id, d;
        do {
            r = g_rng.NextInt(39) + 5;
            c = g_rng.NextInt(39) + 5;
        } while (g_gameTileTerrainKind[r * 50 + c] != fill);
        id = PlaceBuildingOnTerrain(r, c, 4, 0);
        switch (g_currentThemeId) {
        case 0: g_buildingTable[id].landmark = 16; break;
        case 1: g_buildingTable[id].landmark = 18; break;
        case THEME_LINKS: g_buildingTable[id].landmark = 17; break;
        case THEME_TROPICAL: g_buildingTable[id].landmark = 17; break;
        }
        g_buildingTable[id].facing = g_rng.NextInt(4);
        g_tileExtra[r * 50 + c] |= TILE_PERMANENT;
        for (d = 0; d < 8; d++)
            if (g_currentThemeId == THEME_DESERT)
                g_gameTileTerrainKind[(g_dirRowStep[d] + r) * 50 + g_dirColStep[d] + c] = TERRAIN_BRUSH;
    }

    {
        int e, span, frame, r, c;
        int width;

        e = SpawnEmployee(-6);
        g_staffSlots[e].postRow = g_allHolesArray[CLUBHOUSE_HOLE].teeX;
        g_staffSlots[e].postCol = g_allHolesArray[CLUBHOUSE_HOLE].teeY;
        span = 25;
        frame = 0;
        do {
            frame++;
            span--;
        } while (span * span * 4 > g_mapCourseList[course].acres * 10);
        memcpy(g_gameForSaleLandGrid, g_gameTileTerrainKind, 2500);
        if (g_mapCourseList[course].coast == 2) {
            for (r = 0; r < 50; r++) {
                e = g_rng.NextInt(3);
                width = Clamp(abs(r - 25), 8, 50) + frame - 12 + e;
                for (c = 0; c < width; c++) {
                    SetTerrainTileType(c, r);
                    SetTerrainTileType(r, c);
                    SetTerrainTileType(r, 49 - c);
                    SetTerrainTileType(49 - c, r);
                }
            }
        } else {
            for (r = 0; r < frame; r++)
                for (c = 0; c < 50; c++) {
                    SetTerrainTileType(r, c);
                    if (g_mapCourseList[course].coast == 0)
                        SetTerrainTileType(c, r);
                    SetTerrainTileType(c, 49 - r);
                    SetTerrainTileType(49 - r, c);
                }
        }
        RecalculateAllWaterShoreDepths();
        RebuildTerrainElevation();
    }
}

void UnlockLandmarkByGolfer(int golfer, int theme)
{
    int x, y, id, lm;
    signed char t;
    x = g_golferPool[golfer].x >> 10;
    y = g_golferPool[golfer].y >> 10;
    for (;;) {
        t = g_gameTileTerrainKind[x * 50 + y];
        if (g_squareTypeTable[t].category == CATEGORY_ROUGH && t != TERRAIN_BUILDING
            && (g_tileExtra[x * 50 + y] & (TILE_PATH | TILE_SCENIC | TILE_BENCH)) == 0)
            break;
        x += g_rng.NextInt(3) - 1;
        y += g_rng.NextInt(3) - 1;
        if (IsPositionInvalid(x, y))
            return;
    }
    switch (g_gameStoryNameTable[theme][0]) {
    case 'C': id = 0x168; break;
    case 'F':
    case 'R': id = 0x170; break;
    case 'G': id = 0x16f; break;
    case 'H': id = 0x16d; break;
    case 'L': id = 0x16c; break;
    case 'P': id = 0x169; break;
    case 'X': id = 0x171; break;
    case 'M': id = 0x16b; break;
    case 'S': id = 0x173; break;
    case 'A': id = 0x16a; break;
    default: id = g_rng.NextInt(10) + 0x168; break;
    }
    lm = id - 0x168;
    g_shownLandmarkBitmask |= 1 << lm;
    g_freeLandmarks |= 1 << lm;
    strcat(g_printBuffer, "A");
    AppendLandmarkName(lm, 1);
    strcat(g_printBuffer, " is now available in your landmarks menu.");
    ShowStoryMessage(COLOR_TEAL, 0, -1);
}

int BuildThemeList()
{
    int count = 0;
    FindFilesWithFilter("Themes\\*.*", g_empty, g_empty);
    for (int i = 1; i < 100; i++) {
        if (strstr(g_fileList[i], ".") == 0 && strlen(g_fileList[i]) != 0
            && strcmp(g_fileList[i], "Championship") != 0) {
            strcpy(g_themeList[count++], g_fileList[i]);
        }
    }
    strcpy(g_activeThemeDir, "Standard");
    return count;
}

void ShowThemeSelect()
{
    int i, count, mx, my, found;
    short colx[5];
    char path[140];
    char mask[100];
    WIN32_FIND_DATAA fd;
    int hover, prev, hold, cur, b, x, y, k;
    const char *nm;

    colx[0] = 0x15a;
    colx[1] = 0x1b4;
    colx[2] = 0x20e;
    colx[3] = 0x268;
    colx[4] = 0x2c2;

    count = BuildThemeList();
    i = 0;
    if (count > 0) {
        for (nm = g_themeList[0]; i < count; i++, nm += 50) {
            mask[i] = 0;
            strcpy(path, "Themes\\");
            strcat(path, nm);
            strcat(path, "\\*.txt");
            if (FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE)
                mask[i] = 1;
            strcpy(path, "Themes\\");
            strcat(path, nm);
            strcat(path, "\\*.chr");
            if (FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE)
                mask[i] |= 2;
            strcpy(path, "Themes\\");
            strcat(path, nm);
            strcat(path, "\\celebrities.dta");
            if (FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE)
                mask[i] |= 4;
            strcpy(path, "Themes\\");
            strcat(path, nm);
            strcat(path, "\\progolfers.dta");
            if (FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE)
                mask[i] |= 8;
            strcpy(path, "Themes\\");
            strcat(path, nm);
            strcat(path, "\\*.cse");
            if (FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE)
                mask[i] |= 0x20;
            strcpy(path, "Themes\\");
            strcat(path, nm);
            strcat(path, "\\landmark*.flc");
            if (FindFirstFileA(path, &fd) != INVALID_HANDLE_VALUE)
                mask[i] |= 0x40;
        }
    }

    int by[4];
    int bx[4];

    bx[0] = 0x245;
    bx[1] = 0x2d6;
    bx[2] = 0x301;
    bx[3] = 0x301;
    by[0] = 0x208;
    by[1] = 0x215;
    by[2] = 0x51;
    by[3] = 0x1d7;

    SpriteObject sprHi[8];
    SpriteObject sprBg[4];
    AssetSurface img;

    img.load_pcx("interface\\Title_ThemePacks_MO", 0, 0, PALETTE_COLORS, 2);
    sprHi[4].extract(&img, 1, 1, 0x57, 0x1f, 1, 1);
    sprHi[5].extract(&img, 0x59, 1, 0x57, 0x1f, 1, 1);
    sprHi[6].extract(&img, 0xb1, 1, 0x57, 0x1f, 1, 1);
    sprHi[7].extract(&img, 1, 0x21, 0xdd, 0x1f, 1, 1);
    sprHi[0].extract(&img, 0x109, 1, 0x47, 0x50, 1, 1);
    sprHi[1].extract(&img, 0x151, 1, 0x30, 0x31, 1, 1);
    sprHi[2].extract(&img, 0x182, 1, 0x13, 0x23, 1, 1);
    sprHi[3].extract(&img, 0x196, 1, 0x13, 0x23, 1, 1);
    img.load_pcx("interface\\Title_ThemePacks", 0, 0, PALETTE_COLORS, 2);
    sprBg[0].extract(&img, 0x245, 0x208, 0x47, 0x50, 1, 1);
    sprBg[1].extract(&img, 0x2d6, 0x215, 0x30, 0x31, 1, 1);
    sprBg[2].extract(&img, 0x301, 0x51, 0x13, 0x23, 1, 1);
    sprBg[3].extract(&img, 0x301, 0x1d7, 0x13, 0x23, 1, 1);

    flush_input();
    g_mouseButton = 0;

    for (;;) {
        g_pMenu->SetFontsForStyles(&g_smallKleptoFont, 0, 0, 0);
        img.copy(&g_simScreen, 0, 0, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        DrawTextWithColor("Select a Theme Pack", 0x4e, 0x22, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawTextCenteredAtY("Stories", 0x15a, 0x3e, 0x80000000);
        DrawTextCenteredAtY("Characters", 0x1b4, 0x25, 0x80000000);
        DrawTextCenteredAtY("Celebrities", 0x20e, 0x3e, 0x80000000);
        DrawTextCenteredAtY("Pro Golfers", 0x268, 0x25, 0x80000000);
        DrawTextCenteredAtY("Courses", 0x2c2, 0x3e, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_smallKleptoFont, 0, 0, 0);

        found = -1;
        i = 0;
        if (count > 0)
            for (y = 0x5c; i < count; y += 0x20, i++) {
                strcpy(g_printBuffer, g_themeList[i]);
                if (strcmp(g_printBuffer, g_activeThemeDir) == 0) {
                    sprHi[7].BlitScaled(g_pMenu, 0x48, y, 0);
                    DrawTextWithColor(g_printBuffer, 0x49, y + 0xb, 0x80000000);
                    cur = 1;
                    found = i;
                } else {
                    DrawTextWithColor(g_printBuffer, 0x49, y + 0xb, 0x80004210);
                    cur = 0;
                }
                for (b = 0; b < 6; b++) {
                    if ((1 << b) & mask[i]) {
                        x = colx[b] - 0x2b;
                        sprHi[cur ? 5 : 6].Draw(g_pMenu, x, y, 1, 1, 1, 0);
                    } else if (cur) {
                        x = colx[b] - 0x2b;
                        sprHi[4].Draw(g_pMenu, x, y, 1, 1, 1, 0);
                    }
                }
            }

        g_simScreen.RenderFullSurface(0);
        hover = -1;
        hold = 0;
        for (;;) {
            WaitWithTimeout(2, 0);
            ProcessMouseMessages();
            g_simScreen.get_mouse_pos(&mx, &my);
            prev = hover;
            hover = -1;
            if (ApproxDistance(mx - 0x267, my - 0x22b) < 0x1a)
                hover = 0;
            if (ApproxDistance(mx - 0x2ed, my - 0x22c) < 0x18)
                hover = 1;
            if (ApproxDistance(mx - 0x30a, my - 0x63) < 0x14)
                hover = 2;
            if (ApproxDistance(mx - 0x30a, my - 0x1e7) < 0x14)
                hover = 3;
            if (hover != prev) {
                if (prev != -1)
                    sprBg[prev].BlitScaled(g_pMenu, bx[prev], by[prev], 0);
                if (hover != -1)
                    sprHi[hover].BlitScaled(g_pMenu, bx[hover], by[hover], 0);
                g_simScreen.RenderFullSurface(0);
                hold = 0;
            } else if (++hold > 0x1e && hover != -1) {
                g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
                g_printBuffer[0] = 0;
                switch (hover) {
                case 0: strcpy(g_printBuffer, found == -1 ? g_empty : "Load"); break;
                case 1: strcpy(g_printBuffer, "Cancel"); break;
                }
                if (hold == 0x1f)
                    TilePatternToArea(bx[hover], by[hover] + 0xe, 0x30, 0x10, 0);
                DrawTextCenteredWithShadow(g_printBuffer, bx[hover] + 0x18, by[hover] + 0x10, COLOR_WHITE);
                g_simScreen.RenderFullSurface(0);
            }
            if (g_mouseButton == 0) {
                if (!PollInputState())
                    continue;
                if (g_mouseButton == 0)
                    hover = 1;
            }
            if (hover != -1) {
                g_mouseButton = 0;
                switch (hover) {
                case 0: return;
                case 1: strcpy(g_activeThemeDir, "Standard"); return;
                }
                break;
            }
            if (g_mouseButton != 0) {
                k = (g_clickY - 0x5b) / 32;
                if (k >= 0 && k < count) {
                    strcpy(g_activeThemeDir, g_themeList[k]);
                    break;
                }
            }
            return;
        }
    }
}

int HighScore_InsertEntry()
{
    int i, j, last;
    int score = (g_gameCashHundreds / 10 + g_courseTotalFunRate + g_clubSkillsScore) * (g_level + 1);
    g_lastInserted = -1;
    for (i = 0; i < 10; i++) {
        HighScore *e = &g_topTenScoresArray[i];
        if (score >= (e->cash / 10 + e->skill + e->fun) * (e->level + 1)) {
            last = 9;
            j = 9;
            if (i <= 9) {
                for (; j >= i; j--)
                    if (g_topTenScoresArray[j].courseId == g_courseId)
                        last = j;
            }
            if (last > i) {
                for (j = last; j > i; j--)
                    g_topTenScoresArray[j] = g_topTenScoresArray[j - 1];
            }
            g_printBuffer[0] = 0;
            g_topTenScoresArray[i].cash = g_gameCashHundreds;
            g_topTenScoresArray[i].fun = g_courseTotalFunRate;
            g_topTenScoresArray[i].skill = g_clubSkillsScore;
            g_topTenScoresArray[i].level = (short)g_level;
            g_topTenScoresArray[i].courseId = g_courseId;
            strcpy(g_topTenScoresArray[i].name, g_golferTypeArray[0].name);
            AppendCourseName(0);
            strcpy(g_topTenScoresArray[i].course, g_printBuffer);
            g_lastInserted = i;
            return i;
        }
        if (e->courseId == g_courseId)
            return -1;
    }
    return -1;
}

void RenderHighScoreScreen()
{
    int i;
    HighScore_InsertEntry();
    AssetSurface img;
    img.load_pcx("interface\\Top10_Blank", &g_palette, 0, PALETTE_COLORS, 0);
    img.BlitRectToRect(&g_simScreen, 0, 0, 0x320, 0x258);
    img.load_pcx("interface\\Top10_Trophies", &g_palette, 0, PALETTE_COLORS, 0);
    for (i = 0; i < 10; i++) {
        int slot, row, col, cx, ny, ty;
        unsigned tc;
        slot = g_trophySlot[i];
        if (g_topTenScoresArray[i].name[0] == 0)
            continue;
        row = slot / 5;
        AssetSurface *dst = &g_simScreen;
        col = slot % 5;
        img.BlitRectToRect(dst, col * 0xa0, row ? 0x158 : 0x28, 0xa0, row ? 0x100 : 0x130);
        cx = col * 0xa0 + 0x50;
        ny = row ? 0x23f : 0x142;
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        if (i == g_lastInserted) {
            int half = (g_pMenu->text_width(g_topTenScoresArray[i].name) + 4) / 2;
            g_simScreen.line(cx - half, ny + 0xd, cx + half, ny + 0xd, 0x80007ff0);
            g_simScreen.line(cx - half, ny + 0xc, cx + half, ny + 0xc, 0x80004206);
        }
        DrawTextCenteredAtY(g_topTenScoresArray[i].name, cx, ny - 1, 0x80000000u);
        DrawTextCenteredAtY(g_topTenScoresArray[i].name, cx, ny + 1, 0x80007ff0u);
        tc = (i == g_lastInserted) ? 0x80007fffu : 0x80004206u;
        DrawTextCenteredAtY(g_topTenScoresArray[i].name, cx, ny, tc);
        ty = row ? 0x1db : 0xc2;
        if (row == 0) {
            ty += (i - 3) * 10;
        } else {
            ty += (i - 8) * 4;
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        }
        strcpy(g_printBuffer, "Cash: ");
        strcat(g_printBuffer, itoa(g_topTenScoresArray[i].cash / 10, g_worldMapNumBuf, 10));
        DrawTextCenteredAtY(g_printBuffer, cx, ty, 0x80004206u);
        ty -= row ? 0xa : 0xc;
        strcpy(g_printBuffer, "Skill: ");
        AppendMoney(g_topTenScoresArray[i].skill);
        DrawTextCenteredAtY(g_printBuffer, cx, ty, 0x80004206u);
        ty -= row ? 0xa : 0xc;
        strcpy(g_printBuffer, "Fun: ");
        strcat(g_printBuffer, itoa(g_topTenScoresArray[i].fun, g_worldMapNumBuf, 10));
        DrawTextCenteredAtY(g_printBuffer, cx, ty, 0x80004206u);
        ty -= (row ? 2 : 8) + (row ? 0xa : 0xc);
        if (row == 0)
            g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
        else
            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer,
            itoa((g_topTenScoresArray[i].cash / 10 + g_topTenScoresArray[i].skill + g_topTenScoresArray[i].fun)
                     * (g_topTenScoresArray[i].level + 1),
                g_worldMapNumBuf, 10));
        DrawTextCenteredAtY(g_printBuffer, cx, ty - 1, 0x80000000u);
        DrawTextCenteredAtY(g_printBuffer, cx, ty + 1, 0x80007ff0u);
        DrawTextCenteredAtY(g_printBuffer, cx, ty, 0x80005288u);
        ty -= 0xd;
        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        strcpy(g_printBuffer, "Total Score (x");
        strcat(g_printBuffer, itoa(g_topTenScoresArray[i].level + 1, g_worldMapNumBuf, 10));
        strcat(g_printBuffer, ")");
        DrawTextCenteredAtY(g_printBuffer, cx, ty, 0x80004206u);
        ty -= 0xc;
        if (i == 0) {
            strcpy(g_printBuffer, g_topTenScoresArray[0].course);
            DrawTextCenteredAtY(g_printBuffer, cx, ty - 2, 0x80004206u);
        }
    }
    g_simScreen.RenderFullSurface(0);
    WaitForInputWithTimeout(0);
}

void OnWorldMapClosed()
{
}

int ThemeFileExists(const char *theme, const char *file)
{
    char path[140];
    strcpy(path, "Themes\\");
    strcat(path, theme);
    strcat(path, file);
    return filefind_get(path) != 0;
}
