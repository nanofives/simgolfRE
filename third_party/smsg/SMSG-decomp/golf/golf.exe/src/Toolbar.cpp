#include <memory.h>
#include <stdlib.h>
#include <string.h>

#include "crt.h"
#include "BuildingSearch.h"
#include "ClubReports.h"
#include "DialogFrame.h"
#include "Employee.h"
#include "FloatingText.h"
#include "GameFlags.h"
#include "Geometry.h"
#include "Golfer.h"
#include "GolferStats.h"
#include "HoleStatistics.h"
#include "IsoView.h"
#include "LoadingScreen.h"
#include "Plot.h"
#include "RenderQueue.h"
#include "SaveGame.h"
#include "Scorecard.h"
#include "SoundEffects.h"
#include "SpriteAssets.h"
#include "StringPool.h"
#include "Surface.h"
#include "TerrainCircle.h"
#include "TerrainGrid.h"
#include "Wildlife.h"
#include "WorldMap.h"
#include "Toolbar.h"
#include "MainLoop.h"

void DisableCapsLock()
{
    unsigned char keys[256];

    GetKeyboardState(keys);
    if (keys[VK_CAPITAL] & 1)
        keybd_event(VK_CAPITAL, 0x45, 1, 0);
}

int RenderToolbarRow(int cx, int baseId, int count, int idStep, int idBase, int sz, int mx, int my, int scale,
    int shown, int flagged)
{
    int hover = -1;
    int xLeft, x, i, cap;
    int id, bx, by, state;

    if (baseId == 0x1a3)
        count = count * 3;
    x = cx - count * 47 / 4 + 0x220;
    xLeft = x;
    for (i = 0; i < count / 2; i++) {
        g_amenityPanelParts[i != 0].draw_antialiased(&g_amenitiesMaskSprites[i != 0], g_pMenu, x, 0x1dd, 0);
        x += i != 0 ? 0x2f : 0x44;
    }
    cap = (count & 1) | 2;
    g_amenityPanelParts[cap].draw_antialiased(&g_amenitiesMaskSprites[cap], g_pMenu, x, 0x1dd, 0);

    if (scale == -1)
        scale = sz < -2 ? 0x8c : 0xf0;

    for (i = 0; i < count; i++) {
        if (shown != 0 && (shown & (1 << i)) == 0)
            continue;
        id = i * idStep + baseId;
        bx = xLeft + i * 47 / 2 + 0x2c;
        by = ((i & 1) ? 0x1b : 0) + 500;
        if (ApproxDistance(mx - bx, (my - by) * 2) < 0x14)
            hover = i;
        state = 3;
        if (hover == i)
            state = 0;
        if (g_selectedRowButton == i)
            state = 1;
        if (i & 1)
            g_amenityPanelParts[8 + state].Draw(g_pMenu, bx - 0x16, by - 0x14, 1, 1, 1, 0);
        else
            g_amenityPanelParts[4 + state].Draw(g_pMenu, bx - 0x16, by - 0x14, 1, 1, 1, 0);
        if (count == 1 && id != 0x168) {
            int under = id + 3;
            if (id == 0x1ec || id == 0x1ca)
                under = id + 1;
            g_rowIconSprite = GetFlicBankSprite(under, g_flicBankFrameLens[under] - 1, 0);
            DrawSpriteImmediate(bx, by, 0, scale, scale, g_rowIconSprite, sz, i + idBase);
        }
        g_rowIconSprite = GetFlicBankSprite(id, g_flicBankFrameLens[id] - 1, 0);
        if (flagged & (1 << i))
            DrawSpriteImmediate(bx, by, 0, scale, scale, g_rowIconSprite, sz, (i + idBase) | 0x400);
        else
            DrawSpriteImmediate(bx, by, 0, scale, scale, g_rowIconSprite, sz, i + idBase);
        if (baseId == 0x1a9) {
            if (i + 1 == count * 2 / 3) {
                idBase += count / -3;
                baseId = 0x1af;
            }
        } else if (baseId == 0x1a3) {
            if (i + 1 == count / 3) {
                idBase -= count / 3;
                baseId = 0x1a9;
            }
        } else if (baseId == 0x226) {
            idBase--;
        }
    }
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    return hover;
}

void BuildAndShowPreferencesDialog()
{
    strcpy(g_printBuffer, "Preferences...\n ");
    for (int i = 0; i < 6; i++) {
        strcat(g_printBuffer, g_preferenceNames[i]);
        strcat(g_printBuffer, "\n ");
    }
    g_configBitMask = ShowActionDialog(400, 200, g_configBitMask, 0, 0);
}

void RenderToolbarTooltip(int x, int y)
{
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    int n = strlen(g_printBuffer);
    if (x < n * 3)
        x = n * 3;
    if (x > 800 - n * 3)
        x = 800 - n * 3;
    DrawTerrainLine(x - n * 3, y, n * 3 + x, y, COLOR_BLACK, 10, 5);
    DrawTextCenteredAtY(g_printBuffer, x, y - 5, COLOR_WHITE);
}

int FindNearestToolbarButton(int x, int y)
{
    int hit = -1;
    int i = 0;
    int best = 30;
    do {
        int d = ApproxDistance(x - g_mainBarButtonPos[i][0], y - g_mainBarButtonPos[i][1]) * (i + 6) / 8;
        if (d < best) {
            best = d;
            hit = i;
        }
        i++;
    } while (g_mainBarButtonPos[i][0] != -1);
    if (hit == 6)
        hit = -1;
    return hit;
}

int MainBarClick(int x, int y)
{
    switch (FindNearestToolbarButton(x, y)) {
    case 0:
        g_toolbarView = g_toolbarView != TOOLBAR_BUILD_COURSE ? TOOLBAR_BUILD_COURSE : TOOLBAR_CLOSED;
        g_paintTerrain = -1;
        g_displayFocus = 0;
        g_selectedBuildingKind = -1;
        return -1;
    case 1:
        g_toolbarView = g_toolbarView != TOOLBAR_BUILDINGS ? TOOLBAR_BUILDINGS : TOOLBAR_CLOSED;
        g_paintTerrain = -1;
        g_displayFocus = 0;
        g_selectedBuildingKind = -1;
        return -1;
    case 2:
        g_toolbarView = g_toolbarView != TOOLBAR_GOLFERS ? TOOLBAR_GOLFERS : TOOLBAR_CLOSED;
        g_paintTerrain = -1;
        g_displayFocus = 0;
        g_selectedBuildingKind = -1;
        return -1;
    case 3:
        if (g_mouseButton == MOUSE_RIGHT)
            return -0x70;
        strcpy(g_printBuffer, "Information...\n Repeat Last Message\n Course Report\n Player Comments\n Routing Map\n"
                              " Histograph\n");
        strcat(g_printBuffer, " SGA Evaluation\n Financial Report\n Membership Roster\n Professional Accomplishments\n"
                              " World Map\n Best Scores\n Top 10 Designers\n");
        switch (ShowActionDialog(200, 0xfa, 1, 1, 0)) {
        case 0: return 0x3f;
        case 1: return -0x70;
        case 2: return -0x71;
        case 3: return 0x52;
        case 4: return -0x72;
        case 5: return -0x76;
        case 6: return -0x73;
        case 7: return -0x78;
        case 8: return -0x79;
        case 9: return 0x57;
        case 10: RenderBestHoleScoresScreen(); return -1;
        case 11: RenderHighScoreScreen(); return -1;
        default: return -1;
        }
        break;
    case 4: return 0x50;
    case 5:
        for (;;) {
            strcpy(g_printBuffer,
                " Save the Current Game\n Load a Previous Game\n Save Course for Championship\n Save ");
            strcat(g_printBuffer, g_golferTypeArray[0].name);
            strcat(g_printBuffer,
                " for Championship\n Rename Your Course\n Edit Preferences\n Cancel match/tournament\n"
                " Exit the Game\n");
            switch (ShowActionDialog(0xfa, 0x154, 1, 1, 0)) {
            case 0: return 0x53;
            case 1: return 0x4c;
            case 2: return 0x43;
            case 3:
                if (SaveGolferToTheme(0, 1, 1)) {
                    strcpy(g_printBuffer, g_golferTypeArray[0].name);
                    strcat(g_printBuffer, " saved for championship play.");
                    ShowStoryMessage(COLOR_DARK_RED, 0, -1);
                    return -1;
                }
                break;
            case 4:
                g_printBuffer[0] = 0;
                AppendCourseName(-1);
                ShowTextInputDialog("Rename Course...", NAME_DIALOG_X, 0x14c, NAME_MAX_LENGTH);
                if (g_printBuffer[0] && ValidateAndTrimString(g_printBuffer))
                    StoreStringInPool(0);
                else
                    ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                return -1;
            case 5: BuildAndShowPreferencesDialog(); return -1;
            case 6: return 0x6e;
            case 7: g_quitToMenuFlag = 1; return 0x1b;
            default: return -1;
            }
        }
        break;
    case 6: return 0x3f;
    case 7: return 0x7a;
    case 8: return 0x78;
    case 9: return -0x24;
    case 10: return -0x21;
    }
    return -1;
}

void RenderMainBar(int x, int y)
{
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    g_toolPanelBgImages[0].draw_antialiased(&g_toolbarPanelBackings_A[0], g_pMenu, 0, 0x1b3, 0);
    int hover = FindNearestToolbarButton(x, y);
    int hit = hover;
    if (hover != -1 && hover != g_toolbarView) {
        hover = g_mainBarTabImage[hover];
        g_lowerLeftBarParts[hover * 3].draw_antialiased(&g_lowerLeftBarAlphaSprites[hover * 2], g_pMenu,
            g_lowerLeftTabPos[hover][0], g_lowerLeftTabPos[hover][1], 0);
    }
    if (g_cameraZoom == MAX_ZOOM) {
        if (g_zoomView)
            g_lowerLeftBarParts[14].Draw(g_pMenu, g_zoomMapIconPos[0], g_zoomMapIconPos[1], 1, 1, 1, 0);
    }
    if (g_cameraZoom == 1)
        g_lowerLeftBarParts[17].Draw(g_pMenu, g_unzoomMapIconPos[0], g_unzoomMapIconPos[1], 1, 1, 1, 0);
    if ((g_runModeFlags & GAME_PAUSED) && (++g_pauseBlinkTicks & 4))
        g_lowerLeftBarParts[28].Draw(g_pMenu, g_pauseIconPos[0], g_pauseIconPos[1], 1, 1, 1, 0);

    switch (g_toolbarView) {
    case 0:
        g_lowerLeftBarParts[1].draw_antialiased(&g_lowerLeftBarAlphaSprites[1], g_pMenu, 7, 0x1b4, 0);
        g_lowerLeftBarParts[9].Draw(g_pMenu, 0x22, 0x1fd, 1, 1, 1, 0);
        break;
    case 1:
        g_lowerLeftBarParts[4].draw_antialiased(&g_lowerLeftBarAlphaSprites[3], g_pMenu, 0x54, 0x1cf, 0);
        g_lowerLeftBarParts[10].Draw(g_pMenu, 0x5f, 0x20d, 1, 1, 1, 0);
        break;
    case TOOLBAR_GOLFERS:
    case TOOLBAR_SHOT:
    case TOOLBAR_PEOPLE:
        g_lowerLeftBarParts[7].draw_antialiased(&g_lowerLeftBarAlphaSprites[5], g_pMenu, 0x92, 0x1f8, 0);
        g_lowerLeftBarParts[11].Draw(g_pMenu, 0x8e, 0x22b, 1, 1, 1, 0);
        break;
    }

    if (hover == g_toolbarHoverButton) {
        g_toolbarHoverTicks++;
        if (hover != -1 && g_toolbarHoverTicks > 10) {
            g_printBuffer[0] = 0;
            switch (hit) {
            case 0: strcpy(g_printBuffer, "Build Course"); break;
            case 1: strcpy(g_printBuffer, "Add Buildings"); break;
            case 2: strcpy(g_printBuffer, "People/Employees"); break;
            case 3: strcpy(g_printBuffer, "Information"); break;
            case 4: strcpy(g_printBuffer, (g_runModeFlags & GAME_PAUSED) ? "Unpause" : "Pause"); break;
            case 5: strcpy(g_printBuffer, "System Functions"); break;
            case 6: strcpy(g_printBuffer, "Repeat last message"); break;
            case 7: strcpy(g_printBuffer, "Zoom Map"); break;
            case 8: strcpy(g_printBuffer, "Unzoom Map"); break;
            case 9: strcpy(g_printBuffer, "Rotate Map"); break;
            case 10: strcpy(g_printBuffer, "Rotate Map"); break;
            }
            RenderToolbarTooltip(g_mainBarButtonPos[hit][0], g_mainBarButtonPos[hit][1]);
        }
    } else {
        g_toolbarHoverTicks = 0;
        g_toolbarHoverButton = hover;
    }
}

int HitTestTerrainToolbar(int x, int y)
{
    int hit = -1;
    int i = 0;

    if (ApproxDistance(x - 0xed, y - 0x20d) < 0x14)
        hit = 0x16;
    if (ApproxDistance(x - 0x105, y - 0x242) < 0xf)
        hit = 0x15;
    do {
        int bottom = g_themeTerrainButtonBmps[i * 4].m_height - 10;
        if (x >= g_terrainToolbarPos[i][0] && x < g_terrainToolbarPos[i][0] + 0x3e
            && y >= g_terrainToolbarPos[i][1] + bottom - 0x2c && y < g_terrainToolbarPos[i][1] + bottom)
            return i;
    } while (g_terrainToolbarTerrain[++i] != -1);
    return hit;
}

int HandleCourseToolbarClick(int x, int y)
{
    if (g_buildToolbarShown)
        return HandleBuildingToolbarClick(x, y);
    int hit = HitTestTerrainToolbar(x, y);
    if (hit != -1) {
        if (hit < 0x14) {
            g_paintTerrain = g_terrainToolbarTerrain[hit];
        } else if (hit == 0x14) {
            g_paintTerrain = -1;
        } else if (hit == 0x15) {
            strcpy(g_printBuffer,
                "To UNDO a previous action, right click on the map to display the effect of the undo. ");
            g_messageSpeaker = -0x16;
            strcat(g_printBuffer, "Right click again to effect the change. Your money will be refunded!");
            RenderSpeechBubbleWithPortrait(200, 0x154, 0x80007fff, -2);
            g_simScreen.RenderFullSurface(0);
            WaitForInputWithTimeout(0);
        } else if (hit == 0x16) {
            g_buildToolbarShown = 1;
            g_toolbarView = TOOLBAR_BUILD_COURSE;
        }
    }
    return g_rng.NextInt(3);
}

void RenderTerrainToolbar(int x, int y)
{
    int i, hit, t, bx, r, n;
    AssetSurface *pm;
    SpriteObject *fp;
    int button[20];
    char section[32];

    if (g_buildToolbarShown) {
        RenderBuildingToolbar(x, y);
        return;
    }
    for (i = 0; i < 0x10; i++)
        button[g_terrainToolbarTerrain[i]] = i;
    for (i = 0; i <= 2; i++) {
        g_terrainToolbarPos[13 + i][0] = (short)g_themeToolbarPos[g_currentThemeId][i][0];
        g_terrainToolbarPos[13 + i][1] = (short)g_themeToolbarPos[g_currentThemeId][i][1];
    }
    hit = HitTestTerrainToolbar(x, y);
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    if (hit == g_panelHoverButton && hit != -1 && hit < 0x14 && g_panelHoverTicks > 10) {
        bx = g_terrainToolbarPos[hit][0] + 0x20;
        bx = Clamp(bx, 0x50, 0x2d0);
        DrawPanelFrame(bx - 0x50, 0x192, 0xa0, 0x70, 1);
        g_popupEdgeBmps[18].draw_antialiased(&g_transPopupCellsAlpha[18], g_pMenu, bx - 0x5c, 0x185, 0);
        for (i = 0; i <= 7; i++) {
            int k = 1 + (i == 7);
            g_popupEdgeBmps[18 + k].draw_antialiased(&g_transPopupCellsAlpha[18 + k], g_pMenu, bx - 0x4c + i * 0x10,
                0x185, 0);
        }
        t = g_terrainToolbarTerrain[hit];
        strcpy(g_printBuffer, g_squareTypeTable[t].name);
        strcat(g_printBuffer, " $");
        strcat(g_printBuffer, itoa(g_squareTypeTable[t].price * 100, g_numText, 10));
        g_printBuffer[0] += 0xe0;
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawTextCenteredAtY(g_printBuffer, bx - 0xc, 0x18a, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        pm = g_pMenu;
        r = Clamp((4 - g_squareTypeTable[t].lieGrade) * 2, 1, 0xa);
        fp = &g_memberWindowSprite[r];
        fp->Draw(pm, bx - 0x5a, 0x187, 1, 1, 1, 0);
        strcpy(g_printBuffer, "*");
        strcat(g_printBuffer, g_squareTypeTable[t].name);
        strcpy(section, g_printBuffer);
        g_printBuffer[0] = 0;
        switch (g_currentThemeId) {
        case THEME_PARK: ParseFileForSection("interface\\parkland.txt", section); break;
        case THEME_LINKS: ParseFileForSection("interface\\links.txt", section); break;
        case THEME_DESERT: ParseFileForSection("interface\\desert.txt", section); break;
        case THEME_TROPICAL: ParseFileForSection("interface\\tropical.txt", section); break;
        }
        DrawTextWordWrapped(0x10, bx - 0x46, 0x19a, 0x80007fff);
        strcpy(g_printBuffer, "Lie: ");
        switch (g_squareTypeTable[t].lieGrade) {
        case -1:
        case 0: strcat(g_printBuffer, "good"); break;
        case 1: strcat(g_printBuffer, "fair"); break;
        case 2: strcat(g_printBuffer, "bad"); break;
        case 3: strcat(g_printBuffer, "buried"); break;
        case 4: strcat(g_printBuffer, "obstructed"); break;
        default: strcat(g_printBuffer, "impossible"); break;
        }
        g_simScreen.line(bx + 0x40, 0x1a9, bx + 0x40, 0x1d2, COLOR_LIGHT_GREY);
        g_simScreen.line(bx + 0x40, 0x1d2 - g_squareTypeTable[t].bounce * 6, bx + 0x40, 0x1d2, 0x80004210);
        g_simScreen.line(bx + 0x3c, 0x1d2, bx + 0x44, 0x1d2, 0x80001284);
        g_bubblePieces[13].BlitScaled(g_pMenu, bx + 0x3c, 0x1cd - g_squareTypeTable[t].bounce * 6, 0);
        if (g_squareTypeTable[t].rollShift != 0)
            g_simScreen.line(bx - 0x48, 0x1ea, bx - 0x48 + (6 << g_squareTypeTable[t].rollShift), 0x1ea, 0x80004210);
        g_bubblePieces[13].BlitScaled(g_pMenu, bx - 0x4a + (6 << g_squareTypeTable[t].rollShift), 0x1e2, 0);
    }
    g_toolPanelBgImages[1].draw_antialiased(&g_toolbarPanelBackings_A[1], g_pMenu, 0xd8, 0x1e2, 0);
    if (hit == 0x15)
        g_themeTerrainButtonBmps[84].Draw(g_pMenu, 0xf6, 0x232, 1, 1, 1, 0);
    else if (hit == 0x16)
        g_themeTerrainButtonBmps[88].Draw(g_pMenu, 0xdc, 0x1fe, 1, 1, 1, 0);
    for (i = 0; i < 0x10; i++) {
        int f = 0;
        if (hit == i)
            f = 1;
        if (g_paintTerrain == g_terrainToolbarTerrain[i])
            f = 2;
        g_themeTerrainButtonBmps[i * 4 + f].Draw(g_pMenu, g_terrainToolbarPos[i][0], g_terrainToolbarPos[i][1], 1, 1, 1,
            0);
    }
    if (g_allHolesArray[g_nextOpenHoleIndex].teeX != 0 || g_nextOpenHoleIndex >= CLUBHOUSE_HOLE)
        g_themeTerrainButtonBmps[3].Draw(g_pMenu, g_terrainToolbarPos[0][0], g_terrainToolbarPos[0][1], 1, 1, 1, 0);
    if (g_allHolesArray[g_nextOpenHoleIndex].aimX == 0 && g_nextOpenHoleIndex < CLUBHOUSE_HOLE) {
        int frame;
        if (g_paintTerrain == TERRAIN_GREEN)
            frame = g_clock % g_flicBankFrameLens[g_currentThemeId + 0x189];
        else
            frame = 0;
        g_rowIconSprite = GetFlicBankSprite(g_currentThemeId + 0x189, frame, 3);
        DrawSpriteImmediate(g_terrainToolbarPos[1][0] + 0x1f, g_terrainToolbarPos[1][1] + 0x18, 0, 0x28, 0x28,
            g_rowIconSprite, SPRITE_FULL_SCALE, 0x63);
    }
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    for (i = 0; i <= 0xf; i++) {
        t = g_terrainToolbarTerrain[i];
        strcpy(g_printBuffer, g_squareTypeTable[t].name);
        g_printBuffer[0] += 0xe0;
        if (g_level == 0 || g_paintTerrain == t)
            DrawTextCenteredAtY(g_printBuffer, g_terrainToolbarPos[i][0] + 0x20, g_terrainToolbarPos[i][1] + 4,
                COLOR_WHITE);
    }
    if (hit == g_panelHoverButton) {
        g_panelHoverTicks = g_panelHoverTicks + 1;
        if (hit < 0x14)
            return;
        if (g_panelHoverTicks <= 10)
            return;
        t = g_terrainToolbarTerrain[hit];
        strcpy(g_printBuffer, g_squareTypeTable[t].name);
        g_printBuffer[0] += 0xe0;
        strcat(g_printBuffer, ", Cost: ");
        strcat(g_printBuffer, itoa(g_squareTypeTable[t].price * 100, g_numText, 10));
        if (hit == 0x14)
            strcpy(g_printBuffer, "Info Cursor");
        else if (hit == 0x15)
            strcpy(g_printBuffer, "Undo");
        else if (hit == 0x16)
            strcpy(g_printBuffer, "Improvements");
        n = strlen(g_printBuffer);
        {
            int cy = g_terrainToolbarPos[hit][1] + 1;
            int cx = g_terrainToolbarPos[hit][0] + 0x14;
            DrawTerrainLine(cx - n * 3, cy, cx + n * 3, cy, COLOR_BLACK, 0xa, 5);
        }
        DrawTextCenteredAtY(g_printBuffer, g_terrainToolbarPos[hit][0] + 0x14, g_terrainToolbarPos[hit][1] - 4,
            COLOR_WHITE);
        strcpy(g_printBuffer, "Lie: ");
        switch (g_squareTypeTable[t].lieGrade) {
        case -1:
        case 0: strcat(g_printBuffer, "good"); break;
        case 1: strcat(g_printBuffer, "fair"); break;
        case 2: strcat(g_printBuffer, "bad"); break;
        case 3: strcat(g_printBuffer, "buried"); break;
        case 4: strcat(g_printBuffer, "obstructed"); break;
        default: strcat(g_printBuffer, "impossible"); break;
        }
    } else {
        g_panelHoverTicks = 0;
        g_panelHoverButton = hit;
    }
}

int HitTestElevationToolbarButtons(int x, int y)
{
    int hit = -1;
    int yy;

    if (ApproxDistance(x - 0x10e, y - 0x1f3) < 0x10)
        hit = -2;
    yy = y - 0x223;
    if (ApproxDistance(x - 0x160, yy) < 0x19)
        hit = 0;
    if (ApproxDistance(x - 0x1d3, yy) < 0x19)
        hit = 1;
    if (ApproxDistance(x - 0x24b, yy) < 0x19)
        hit = 2;
    if (ApproxDistance(x - 0x2cb, yy) < 0x10)
        hit = 3;
    if (ApproxDistance(x - 0x10b, y - 0x231) < 0xc)
        hit = -3;
    return hit;
}

int HandleElevationToolbarClick(int x, int y)
{
    int hit = HitTestElevationToolbarButtons(x, y);
    if (hit == -2) {
        g_elevToolbarShown = 0;
        g_toolbarView = TOOLBAR_BUILDINGS;
        g_runModeFlags &= ~GAME_ELEVATION_TOOL;
        return -1;
    }
    if (hit == -3) {
        strcpy(g_printBuffer, "To UNDO a previous action, right click on the map to display the effect of the undo. ");
        g_messageSpeaker = -0x16;
        strcat(g_printBuffer, "Right click again to effect the change. Your money will be refunded!");
        RenderSpeechBubbleWithPortrait(200, 0x154, 0x80007fff, -2);
        g_elevToolbarShown = 0;
        g_toolbarView = TOOLBAR_BUILDINGS;
        g_runModeFlags &= ~GAME_ELEVATION_TOOL;
        g_simScreen.RenderFullSurface(0);
        WaitForInputWithTimeout(0);
    }
    if (hit >= 0) {
        g_gameElevPanelChoice = hit;
        g_paintTerrain = -1;
        return g_runModeFlags |= GAME_ELEVATION_TOOL;
    }
}

void RenderElevationToolbarWithTooltip(int x, int y)
{
    int hit = HitTestElevationToolbarButtons(x, y);

    g_toolPanelBgImages[8].draw_antialiased(&g_toolbarPanelBackings_A[8], g_pMenu, 0xd7, 0x1e2, 0);
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    if (hit == -2)
        g_gameGlowingPanelDiscs[6].BlitScaled(g_pMenu, 0xfe, 0x1e3, 0);
    if (g_runModeFlags & GAME_DEMOLISH_TOOL)
        g_simElevationPanelButtonFrames[13].BlitScaled(g_pMenu, 0xfd, 0x222, 0);
    if (hit == -3)
        g_simElevationPanelButtonFrames[12].BlitScaled(g_pMenu, 0xfd, 0x222, 0);
    else if (hit >= 0)
        g_simElevationPanelButtonFrames[hit * 3].BlitScaled(g_pMenu, g_elevationToolbarPos[hit][0],
            g_elevationToolbarPos[hit][1], 0);
    g_simElevationPanelButtonFrames[g_gameElevPanelChoice * 3 + 1].BlitScaled(g_pMenu,
        g_elevationToolbarPos[g_gameElevPanelChoice][0], g_elevationToolbarPos[g_gameElevPanelChoice][1], 0);
    if (hit == g_panelHoverButton) {
        g_panelHoverTicks++;
        if (hit == -1 || g_panelHoverTicks <= 10)
            return;
        g_printBuffer[0] = 0;
        switch (hit) {
        case -3: strcpy(g_printBuffer, "Undo"); break;
        case -2: strcpy(g_printBuffer, "Buildings"); break;
        case 0: strcpy(g_printBuffer, "Raise/lower vertex"); break;
        case 1: strcpy(g_printBuffer, "Raise/lower square"); break;
        case 2: strcpy(g_printBuffer, "Raise/lower area"); break;
        case 3: strcpy(g_printBuffer, "Analyze Golf Shot"); break;
        }
        RenderToolbarTooltip(x, y - 5);
    } else {
        g_panelHoverTicks = 0;
        g_panelHoverButton = hit;
    }
}

int HitTestFacilityUpgradeButtons(int x, int y)
{
    int i, dx, dy;

    if (ApproxDistance(x - 0xec, y - 0x20e) < 0x10)
        return -2;
    if (ApproxDistance(x - 0x115, y - 0x237) < 0xc)
        return -3;
    i = 0;
    dx = x - 0x17f;
    dy = (y - 0x213) * 2;
    for (; i < 9; i++) {
        if (ApproxDistance(dx, dy) >= 0x28) {
            if (i != 4)
                dx -= 0x50;
            else {
                dy -= 0x50;
                dx += 0x118;
            }
        } else
            return i;
    }
    return -1;
}

int HandleFacilityUpgradeClick(int x, int y)
{
    int sel;

    if (g_elevToolbarShown != 0)
        return HandleElevationToolbarClick(x, y);
    sel = HitTestFacilityUpgradeButtons(x, y);
    if (sel == -3) {
        strcpy(g_printBuffer, "To UNDO a previous action, right click on the map to display the effect of the undo. ");
        g_messageSpeaker = -0x16;
        strcat(g_printBuffer, "Right click again to effect the change. Your money will be refunded!");
        RenderSpeechBubbleWithPortrait(200, 0x154, 0x80007fff, -2);
        g_simScreen.RenderFullSurface(0);
        WaitForInputWithTimeout(0);
        return 0;
    }
    if (sel == -2) {
        g_elevToolbarShown = 1;
        g_toolbarView = TOOLBAR_BUILDINGS;
        return 0;
    }
    if (sel == -1)
        return -1;
    g_selectedBuildingKind = sel + 6;
    if ((g_buildingLevelPerKind[g_selectedBuildingKind] != 0 && GetHoleCountTier(g_nextOpenHoleIndex - 1) < 2)
        || g_buildingLevelPerKind[g_selectedBuildingKind] >= LEVEL_UPGRADED) {
        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
        if (g_buildingLevelPerKind[g_selectedBuildingKind] >= LEVEL_UPGRADED)
            strcpy(g_printBuffer, "You cannot upgrade this building any further.");
        else
            strcpy(g_printBuffer, "Sorry, upgraded buildings are not available until you build beyond 9 holes.");
        ShowStoryMessage(COLOR_DARK_RED, 1, -1);
        g_selectedBuildingKind = -1;
    }
    if (g_selectedBuildingKind >= g_unlockedBldgTypeLimit) {
        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
        strcpy(g_printBuffer, "This building is will become available as you build additional holes.");
        ShowStoryMessage(COLOR_DARK_RED, 1, -1);
        g_selectedBuildingKind = -1;
    }
    return g_toolbarClickResult;
}

void RenderFacilityUpgradeToolbar(int x, int y)
{
    int sel, cx, hole, i, j, bx, by, lift, b, base;

    if (g_elevToolbarShown) {
        RenderElevationToolbarWithTooltip(x, y);
        return;
    }
    sel = HitTestFacilityUpgradeButtons(x, y);
    if (sel == g_panelHoverButton && sel >= 0 && g_panelHoverTicks > 0xa) {
        base = 0x187;
        if (sel > 4)
            base = 0x1af;
        cx = Clamp((sel % 5) * 80 + base, 0x50, 0x2d0);
        RenderUIFrameWithCorners(cx - 0x50, 0x1cc, 0xa0, 0x64, 1);
        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        hole = sel + 6;
        g_printBuffer[0] = 0;
        if (hole != 7 && g_buildingLevelPerKind[hole] != 0)
            strcpy(g_printBuffer, "Upgraded ");
        strcat(g_printBuffer, g_facilityTypes[sel + 6].name);
        DrawTextCenteredAtY(g_printBuffer, cx, 0x1ce, 0x80000000);
        strcpy(g_printBuffer, "Cost: ");
        if (hole != 7 && g_buildingLevelPerKind[hole] != 0)
            FormatNumberWithCommas(g_facilityTypes[sel + 6].cost * 300 / 2);
        else
            FormatNumberWithCommas(g_facilityTypes[sel + 6].cost * 100);
        DrawTextCenteredAtY(g_printBuffer, cx, 0x1d8, 0x80000000);
        g_printBuffer[0] = 0;
        switch (hole) {
        case 6: strcpy(g_printBuffer, "Helps imaginative golfers"); break;
        case 10: strcpy(g_printBuffer, "Helps long hitters"); break;
        case 7: strcpy(g_printBuffer, "Feeds hungry golfers"); break;
        case 8: strcpy(g_printBuffer, "Improves accurate golfers"); break;
        case 5: strcpy(g_printBuffer, "Sell lot for cash"); break;
        case 11: strcpy(g_printBuffer, "Golfers play faster"); break;
        case 12: strcpy(g_printBuffer, "Increases property values"); break;
        case 9: strcpy(g_printBuffer, "Golfers become members"); break;
        case 13: strcpy(g_printBuffer, "Golfers stay happier"); break;
        case 14: strcpy(g_printBuffer, "Higher greens fees"); break;
        }
        DrawTextCenteredAtY(g_printBuffer, cx, 0x1e2, 0x80000000);
    }
    g_toolPanelBgImages[2].draw_antialiased(&g_toolbarPanelBackings_A[2], g_pMenu, 0xd8, 0x1e2, 0);
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    if (sel == -2)
        g_gameGlowingPanelDiscs[4].BlitScaled(g_pMenu, 0xdc, 0x1fe, 0);
    else if (sel == -3)
        g_buildingPnlFrames[8].BlitScaled(g_pMenu, 0x105, 0x229, 0);
    if (g_runModeFlags & GAME_DEMOLISH_TOOL)
        g_buildingPnlFrames[9].BlitScaled(g_pMenu, 0x105, 0x229, 0);
    bx = 0x157;
    by = 0x1f5;
    for (i = 0; i < 9; i++) {
        if (i + 6 < g_unlockedBldgTypeLimit - g_buildingLevelPerKind[i + 6]
            && g_buildingLevelPerKind[i + 6] < LEVEL_UPGRADED) {
            g_buildingPnlFrames[(i > 4) * 4 + 3].Draw(g_pMenu, bx, by, 1, 1, 1, 0);
            if (i == sel)
                g_buildingPnlFrames[(i > 4) * 4].Draw(g_pMenu, bx, by, 1, 1, 1, 0);
        }
        if (i + 6 == g_selectedBuildingKind)
            g_buildingPnlFrames[(i > 4) * 4 + 1].Draw(g_pMenu, bx, by, 1, 1, 1, 0);
        if (i != 4)
            bx += 0x50;
        else {
            by += 0x29;
            bx -= 0x117;
        }
    }
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    for (j = 0; j < 9; j++) {
        lift = 0;
        if (j == sel && j + 6 < g_unlockedBldgTypeLimit - g_buildingLevelPerKind[j + 6]
            && g_buildingLevelPerKind[j + 6] < LEVEL_UPGRADED)
            lift = -1;
        if (j + 6 < g_unlockedBldgTypeLimit && g_buildingLevelPerKind[j + 6] < LEVEL_UPGRADED
            && (g_buildingLevelPerKind[j + 6] < 1 || g_nextOpenHoleIndex > BACK_NINE_FIRST_HOLE))
            b = 1;
        else
            b = 0;
        g_bldgSprites[j][(g_buildingLevelPerKind[j + 6] != 0) * 2 + b].Draw(g_pMenu,
            ((j / 5) + (j % 5) * 2) * 40 + 0x158, (j / 5) * 40 + 0x1c5 + lift, 1, 1, 1, 0);
    }
    if (sel == g_panelHoverButton) {
        g_panelHoverTicks++;
        if (sel == -2) {
            strcpy(g_printBuffer, "Elevation");
            RenderToolbarTooltip(x, y);
        } else if (sel == -3) {
            strcpy(g_printBuffer, "Undo");
            RenderToolbarTooltip(x, y);
        }
    } else {
        g_panelHoverTicks = 0;
        g_panelHoverButton = sel;
    }
}

int HitTestBuildingToolbar(int x, int y)
{
    int button = -1;

    if (ApproxDistance(x - 269, y - 500) < 16)
        button = -2;
    if (ApproxDistance(x - 302, y - 563) < 20)
        button = 0;
    if (ApproxDistance(x - 333, y - 526) < 20)
        button = 1;
    if (ApproxDistance(x - 363, y - 563) < 20)
        button = 2;
    if (ApproxDistance(x - 470, y - 575) < 18)
        button = 3;
    if (ApproxDistance(x - 591, y - 575) < 18)
        button = 4;
    if (ApproxDistance(x - 652, y - 575) < 18)
        button = 5;
    if (ApproxDistance(x - 714, y - 575) < 18)
        button = 6;
    if (ApproxDistance(x - 248, y - 567) < 12)
        button = 7;
    if (ApproxDistance(x - 531, y - 580) < 12)
        button = 9;
    return button;
}

int HandleBuildingToolbarClick(int x, int y)
{
    int sel, v, m;

    sel = HitTestBuildingToolbar(x, y);
    switch (sel) {
    case 0: g_selectedBuildingKind = 3; return -1;
    case 1: g_selectedBuildingKind = 0; return -1;
    case 2: g_selectedBuildingKind = (g_waitListLength > 0) ? 5 : -1; return -1;
    case 3:
        g_selectedRowButton = 0;
        g_rowHoverButton = 0;
        g_selectedBuildingKind = 1;
        return 0;
    case 4:
        g_selectedBuildingKind = SCENERY_LANDMARK;
        g_selectedRowButton = -1;
        g_rowHoverButton = -1;
        return -1;
    case 5:
        g_selectedBuildingKind = SCENERY_FLOWER_BED;
        v = g_rng.NextInt(4);
        g_selectedRowButton = v;
        g_rowHoverButton = v;
        return v;
    case 6:
        g_selectedBuildingKind = SCENERY_WILLOW_TREE;
        v = g_rng.NextInt(7);
        g_selectedRowButton = v;
        g_rowHoverButton = v;
        return v;
    case 7:
    case 8:
        strcpy(g_printBuffer, "To UNDO a previous action, right click on the map to display the effect of the undo. ");
        g_messageSpeaker = -22;
        strcat(g_printBuffer, "Right click again to effect the change. Your money will be refunded!");
        RenderSpeechBubbleWithPortrait(0xc8, 0x154, 0x80007fff, -2);
        g_simScreen.RenderFullSurface(0);
        WaitForInputWithTimeout(0);
        return g_toolbarClickResult;
    case 9:
        g_selectedBuildingKind = SCENERY_SCENIC_BRIDGE;
        v = g_rng.NextInt(8);
        g_selectedRowButton = v;
        g_rowHoverButton = v;
        return v;
    }
    if (sel == -2) {
        g_buildToolbarShown = 0;
        g_toolbarView = TOOLBAR_BUILD_COURSE;
    }
    m = g_selectedBuildingKind;
    if (m == SCENERY_LANDMARK || m == SCENERY_FLOWER_BED || m == 1 || m == SCENERY_WILLOW_TREE
        || m == SCENERY_SCENIC_BRIDGE) {
        v = g_rowHoverButton;
        if (v != -1 || m == SCENERY_LANDMARK) {
            g_selectedRowButton = v;
            return v;
        }
    }
    return 0;
}

void RenderBuildingToolbar(int x, int y)
{
    int sel, lit, id, tx, ty, price, n, cx;

    g_toolPanelBgImages[3].draw_antialiased(&g_toolbarPanelBackings_A[3], g_pMenu, 0xd6, 0x1e2, 0);
    sel = HitTestBuildingToolbar(x, y);
    if (sel > -1)
        g_amenitiesIcons[sel * 3].BlitScaled(g_pMenu, g_buildingToolbarPos[sel][0], g_buildingToolbarPos[sel][1], 0);
    if (sel == -2)
        g_gameGlowingPanelDiscs[5].BlitScaled(g_pMenu, 0xfe, 0x1e3, 0);
    lit = -1;
    if (g_selectedBuildingKind == 3)
        lit = 0;
    else if (g_selectedBuildingKind == 0)
        lit = 1;
    else if (g_selectedBuildingKind == SCENERY_HOME_SITE)
        lit = 2;
    else if (g_selectedBuildingKind == 1)
        lit = 3;
    else if (g_selectedBuildingKind == SCENERY_LANDMARK)
        lit = g_selectedBuildingKind;
    else if (g_selectedBuildingKind == SCENERY_FLOWER_BED)
        lit = 5;
    if (g_paintTerrain == TERRAIN_ELM_TREE || g_selectedBuildingKind == SCENERY_WILLOW_TREE)
        lit = 6;
    if (g_selectedBuildingKind == SCENERY_SCENIC_BRIDGE)
        lit = 9;
    if (lit != -1)
        g_amenitiesIcons[lit * 3 + 1].BlitScaled(g_pMenu, g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],
            0);
    if (g_waitListLength <= 0)
        g_amenitiesIcons[8].BlitScaled(g_pMenu, g_buildingToolbarPos[2][0], g_buildingToolbarPos[2][1], 0);
    if (g_selectedBuildingKind == 1) {
        id = RenderToolbarRow(-0x54, 0x208, 5, g_selectedBuildingKind, 0xaa, 4, x, y, 0x28, 0, 0);
        if (id != -1)
            g_rowHoverButton = id;
        g_amenitiesIcons[lit * 3 + 1].BlitScaled(g_pMenu, g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],
            0);
    }
    if (g_selectedBuildingKind == SCENERY_LANDMARK) {
        if (g_shownLandmarkBitmask != 0) {
            id = RenderToolbarRow(0, 0x168, 0xe, 1, 0x64, -4, x, y, -1, g_shownLandmarkBitmask, 0);
            if (id != -1) {
                g_rowHoverButton = id;
                g_printBuffer[0] = 0;
                AppendLandmarkName(id, 0);
                if (g_printBuffer[0] > 0x5a)
                    g_printBuffer[0] += 0xe0;
                strcat(g_printBuffer, ", ");
                if (g_freeLandmarks & (1 << id))
                    strcat(g_printBuffer, "FREE!");
                else
                    strcat(g_printBuffer, itoa((id + 5) * 1000, g_numText, 10));
                RenderToolbarTooltip(x, y - 0xc);
                switch (id & 3) {
                case 0: strcpy(g_printBuffer, "Happy Golfers"); break;
                case 1: strcpy(g_printBuffer, "No Dandelions"); break;
                case 2: strcpy(g_printBuffer, "Skill Upgrade"); break;
                case 3: strcpy(g_printBuffer, "Happy Endings"); break;
                }
                RenderToolbarTooltip(x, y - 2);
            }
        }
        g_amenitiesIcons[lit * 3 + 1].BlitScaled(g_pMenu, g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],
            0);
    }
    if (g_selectedBuildingKind == SCENERY_SCENIC_BRIDGE) {
        id = RenderToolbarRow(-0x10, 0x226, 8, 1, 0xa9, 4, x, y, 0x28, 0, 0);
        if (id != -1)
            g_rowHoverButton = id;
        g_amenitiesIcons[lit * 3 + 1].BlitScaled(g_pMenu, g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],
            0);
    }
    if (g_selectedBuildingKind == SCENERY_FLOWER_BED) {
        id = RenderToolbarRow(0xd, 0x1a3, 5, 0, 0x2d, -6, x, y, 0x28, 0, 0);
        if (id != -1)
            g_rowHoverButton = id;
        g_amenitiesIcons[lit * 3 + 1].BlitScaled(g_pMenu, g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],
            0);
    }
    if (g_selectedBuildingKind == SCENERY_WILLOW_TREE) {
        id = RenderToolbarRow(0x64, 0x12f, 7, 1, 0xb4, 2, x, y, 0x64, 0, 0);
        if (id != -1)
            g_rowHoverButton = id;
        g_amenitiesIcons[lit * 3 + 1].BlitScaled(g_pMenu, g_buildingToolbarPos[lit][0], g_buildingToolbarPos[lit][1],
            0);
    }
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    if (sel == g_panelHoverButton) {
        g_panelHoverTicks++;
        if (sel == -1)
            return;
        if (g_panelHoverTicks <= 10)
            return;
        g_printBuffer[0] = 0;
        tx = g_buildingToolbarPos[sel][0];
        ty = g_buildingToolbarPos[sel][1];
        price = 0;
        switch (sel) {
        case -2:
            tx = x;
            ty = y;
            strcpy(g_printBuffer, "Course Terrain");
            break;
        case 0:
            strcpy(g_printBuffer, "Ballwasher");
            price = g_facilityTypes[3].cost;
            break;
        case 1:
            strcpy(g_printBuffer, "Pathway");
            price = g_facilityTypes[0].cost;
            break;
        case 2:
            strcpy(g_printBuffer, "Building Lot");
            price = g_facilityTypes[SCENERY_HOME_SITE].cost;
            break;
        case 3:
            strcpy(g_printBuffer, "Benches");
            price = g_facilityTypes[1].cost;
            break;
        case 4:
            strcpy(g_printBuffer, "Landmarks");
            price = g_facilityTypes[SCENERY_LANDMARK].cost;
            break;
        case 5:
            strcpy(g_printBuffer, "Flower Bed");
            price = g_facilityTypes[SCENERY_FLOWER_BED].cost;
            break;
        case 6:
            strcpy(g_printBuffer, "Scenic Trees");
            price = g_squareTypeTable[TERRAIN_ELM_TREE].price;
            break;
        case 7:
        case 8:
            strcpy(g_printBuffer, "Undo");
            price = 0;
            break;
        case 9:
            strcpy(g_printBuffer, "Scenic Bridge");
            price = g_facilityTypes[SCENERY_SCENIC_BRIDGE].cost;
            break;
        }
        n = strlen(g_printBuffer);
        DrawTerrainLine(tx - n * 3 + 0x14, ty + 0xa, tx + n * 3 + 0x14, ty + 0xa, COLOR_BLACK, 0xa, 5);
        cx = tx + 0x14;
        DrawTextCenteredAtY(g_printBuffer, cx, ty + 5, COLOR_WHITE);
        if (price != 0) {
            DrawTerrainLine(tx - n * 3 + 0x14, ty + 0x14, tx + n * 3 + 0x14, ty + 0x14, COLOR_BLACK, 0xa, 5);
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(price * 100, g_numText, 10));
            if (sel == 4)
                strcpy(g_printBuffer, "5000-20,000");
            DrawTextCenteredAtY(g_printBuffer, cx, ty + 0xf, COLOR_WHITE);
        }
    } else {
        g_panelHoverTicks = 0;
        g_panelHoverButton = sel;
    }
}

int HitTestGolferPairingGrid(int x, int y)
{
    int hit = -1;
    int yy;

    if (x >= 0x136 && y >= 0x1f2) {
        hit = Clamp((y - 0x1f2) / 0x15, 0, 3);
        hit += Clamp((x - 0x136) / 0x79, 0, 3) * 4;
    }
    if (ApproxDistance(x - 0x11d, y - 0x1ec) < 0x14)
        hit = -2;
    if (ApproxDistance(x - 0x100, y - 0x1fe) < 0x14)
        hit = -3;
    if (ApproxDistance(x - 0xe8, y - 0x21a) < 0x14)
        hit = -4;
    yy = y * 3 - 0x6f0;
    if (ApproxDistance(x - 0x14c, yy) < 0x14)
        hit = -5;
    if (ApproxDistance(x - 0x304, yy) < 0x14)
        hit = -6;
    return hit;
}

int HandleGolferPairingClick(int x, int y)
{
    int hit;
    int g;

    if (g_employeeListActive)
        return HandleEmployeeListClick(x, y);
    hit = HitTestGolferPairingGrid(x, y);
    if (hit >= 0) {
        g = g_peopleListGolfer[hit];
        if (g != -1) {
            if (g_golferPool[g].screenX != -1 || g_displayFocus > 0)
                g_displayFocus = g + 1;
            g_cameraTileX = g_golferPool[g].x >> 10;
            g_cameraTileY = g_golferPool[g].y >> 10;
        }
    }
    if (hit == -3) {
        g_toolbarView = TOOLBAR_PEOPLE;
        return -1;
    }
    if (hit == -4) {
        g_employeeListActive = g_staffHireButtonIdle = 1;
        return -1;
    }
    if (hit == -5) {
        if (g_peopleListScroll)
            g_peopleListScroll -= 0x10;
        return -1;
    }
    if (hit == -6) {
        g_peopleListScroll += 0x10;
        return -1;
    }
    return -1;
}

void RenderGolferPairingPanel(int x, int y)
{
    int count = 0;
    int i, rec, colCount, show, ry, colour, c, hit, len, a, b;
    int faceX, pipX, textXA, textXB, faceXB;
    int *p;
    AssetSurface *pm;
    SpriteObject *fp;

    if (g_employeeListActive != 0) {
        RenderEmployeePanel(x, y);
        return;
    }
    g_toolPanelBgImages[4].draw_antialiased(&g_toolbarPanelBackings_A[4], g_pMenu, 0xd7, 0x1da, 0);
    hit = HitTestGolferPairingGrid(x, y);
    if (hit == -3)
        g_gameGlowingPanelDiscs[0].BlitScaled(g_pMenu, 0xf0, 0x1ee, 0);
    else if (hit == -4)
        g_gameGlowingPanelDiscs[1].BlitScaled(g_pMenu, 0xd9, 0x20a, 0);
    else if (hit == -5)
        g_memberWindowSprite[11].BlitScaled(g_pMenu, 0x135, 0x24b, 0);
    else if (hit == -6)
        g_memberWindowSprite[12].BlitScaled(g_pMenu, 0x2f4, 0x24b, 0);
    memset(g_peopleListGolfer, 0, sizeof(g_peopleListGolfer));
    faceX = 0x136;
    ry = 0x1f2;
    colCount = 0;
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    faceXB = 0x19c;
    i = 0;
    textXB = 0x146;
    pipX = 0x156;
    textXA = 0x145;
    p = g_peopleListGolfer;
    for (; i < 0x98; i++) {
        rec = (g_simLatestGolferNum - i + 0x98) % 0x98;
        show = 0;
        if (g_golferPool[rec].holeIdx > 0)
            show = 1;
        if (g_golferPool[rec].partner == -1 || g_golferPool[g_golferPool[rec].partner].partner != rec
            || g_golferPool[g_golferPool[rec].partner].holeIdx <= 0) {
            if (show == 0)
                continue;
        }
        count++;
        if (count <= g_peopleListScroll)
            continue;
        if (g_cameraZoom < DEFAULT_ZOOM) {
            if (colCount++ == 6) {
                colCount = 1;
                faceX += 0x3c;
                textXA += 0x3c;
                pipX += 0x3c;
                textXB += 0x3c;
                faceXB += 0x3c;
                ry = 0x1f2;
                if (textXA > 0x2f3)
                    continue;
            }
            colour = 0x80000000;
            if (g_golferPool[rec].fatigue > GOLFER_TIRED_LEVEL)
                colour = 0x80004210;
            if (g_golferPool[rec].thirst > GOLFER_NEED_LEVEL)
                colour = 0x80000018;
            if (g_golferPool[rec].hunger > GOLFER_NEED_LEVEL)
                colour = 0x80006000;
            if (g_golferPool[rec].flags & GOLFER_WALKED_OFF)
                colour = 0x80007d08;
            pm = g_pMenu;
            c = Clamp(g_golferPool[rec].enjoyment + 2, 1, 0xa);
            fp = &g_memberWindowSprite[c];
            fp->Draw(pm, faceX, ry, 1, 1, 1, 0);
            g_printBuffer[0] = 0;
            AppendGolferName(rec, 0);
            DrawTextWithColor(g_printBuffer, textXA, ry + 1, colour);
            *p = rec;
            p++;
            ry += 0xe;
        } else {
            if (colCount++ == 4) {
                colCount = 1;
                faceX += 0x79;
                textXA += 0x79;
                pipX += 0x79;
                textXB += 0x79;
                faceXB += 0x79;
                ry = 0x1f2;
                if (textXA > 0x2b7)
                    continue;
            }
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
            if (rec & 1) {
                int xx, n;
                g_memberWindowSprite[0].Draw(g_pMenu, faceX, ry, 1, 1, 1, 0);
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_golferPool[rec].holeIdx, g_numText, 10));
                if (g_golferPool[rec].holeIdx == CLUBHOUSE_HOLE)
                    strcpy(g_printBuffer, "x");
                if (g_golferPool[rec].holeIdx <= 0) {
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(g_golferPool[g_golferPool[rec].partner].holeIdx, g_numText, 10));
                }
                DrawTextWithColor(g_printBuffer, faceX + (g_golferPool[rec].holeIdx < 0xa ? 8 : 5), ry + 0xf,
                    0x80000000u);
                if (g_golferPool[rec].storyId != -1) {
                    xx = pipX;
                    for (n = 4; n != 0; n--) {
                        g_memberWindowSprite[16].Draw(g_pMenu, xx, ry + 0x11, 1, 1, 1, 0);
                        xx += 0xa;
                    }
                    n = 0;
                    if (n < g_golferPool[rec].storyChapter) {
                        xx = pipX;
                        do {
                            g_memberWindowSprite[13].Draw(g_pMenu, xx, ry + 0x11, 1, 1, 1, 0);
                            n++;
                            xx += 0xa;
                        } while (n < g_golferPool[rec].storyChapter);
                    }
                }
            }
            g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
            colour = 0x80000000;
            if (g_golferPool[rec].fatigue > GOLFER_TIRED_LEVEL)
                colour = 0x80004210;
            if (g_golferPool[rec].thirst > GOLFER_NEED_LEVEL)
                colour = 0x80000018;
            if (g_golferPool[rec].hunger > GOLFER_NEED_LEVEL)
                colour = 0x80006000;
            if (g_golferPool[rec].flags & GOLFER_WALKED_OFF)
                colour = 0x80007d08;
            g_printBuffer[0] = 0;
            AppendGolferName(rec, 0);
            if (g_golferPool[rec].holeIdx > 0) {
                DrawTextWithColor(g_printBuffer, textXB, ry + 5, colour);
                pm = g_pMenu;
                c = Clamp(g_golferPool[rec].enjoyment + 2, 1, 0xa);
                fp = &g_memberWindowSprite[c];
                fp->Draw(pm, faceXB, ry + 2, 1, 1, 1, 0);
            }
            *p = rec;
            p++;
            ry += 0x15;
        }
    }
    if (count != 0) {
        a = Clamp(g_peopleListScroll * 400 / count, 0, 0x18f);
        b = Clamp((g_peopleListScroll + (g_cameraZoom < DEFAULT_ZOOM ? 0x30 : 0x10)) * 400 / count, 1, 0x190);
        g_simScreen.fill(a + 0x15f, 0x24e, b - a - 1, 6, 0x8000739f);
        if (g_peopleListScroll > count)
            g_peopleListScroll -= 0x10;
    }
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    hit = HitTestGolferPairingGrid(x, y);
    if (hit == g_panelHoverButton) {
        g_panelHoverTicks++;
        if (hit == -1)
            return;
        if (g_panelHoverTicks <= 0xa)
            return;
        g_printBuffer[0] = 0;
        switch (hit) {
        case -2: strcpy(g_printBuffer, "Golfers"); break;
        case -3: strcpy(g_printBuffer, g_golferTypeArray[0].name); break;
        case -4: strcpy(g_printBuffer, "Hire Employees"); break;
        }
        len = strlen(g_printBuffer);
        DrawTerrainLine(x - len * 3, y - 5, x + len * 3, y - 5, COLOR_BLACK, 0xa, 5);
        DrawTextCenteredAtY(g_printBuffer, x, y - 0xa, COLOR_WHITE);
        return;
    }
    g_panelHoverTicks = 0;
    g_panelHoverButton = hit;
}

int HitTestPeopleToolbarButtons(int x, int y)
{
    int hit = -1;

    if (ApproxDistance(x - 0x111, y - 0x22f) < 0x14)
        hit = 0;
    if (ApproxDistance(x - 0x139, y - 0x207) < 0xf)
        hit = 1;
    if (ApproxDistance(x - 0x139, y - 0x224) < 0xf)
        hit = 2;
    if (ApproxDistance(x - 0x139, y - 0x244) < 0xf)
        hit = 3;
    if (g_toolbarView == TOOLBAR_SHOT) {
        if (ApproxDistance(x - 0x19e, y - 0x1f9) < 0x14)
            hit = 4;
        if (ApproxDistance(x - 0x1ed, y - 0x1f9) < 0x14)
            hit = 5;
        if (ApproxDistance(x - 0x23c, y - 0x1f9) < 0x14)
            hit = 6;
        if (ApproxDistance(x - 0x28b, y - 0x1f9) < 0x14)
            hit = 7;
        if (ApproxDistance(x - 0x2da, y - 0x1f9) < 0x14)
            hit = 8;
    }
    if (ApproxDistance(x - 0x11e, y - 0x1ea) < 0xf)
        hit = 9;
    if (ApproxDistance(x - 0xe7, y - 0x21c) < 0xf)
        return 10;
    return hit;
}

int HandlePeopleToolbarClick(int x, int y)
{
    int sel;
    int none = -1;

    if (g_employeeListActive != 0)
        return HandleEmployeeListClick(x, y);
    sel = HitTestPeopleToolbarButtons(x, y);
    if (g_nextOpenHoleIndex <= 1 || (g_runModeFlags & GAME_ANY_TOURNAMENT) != 0)
        g_peopleButtonBmps[6].BlitScaled(g_pMenu, g_peopleToolbarPos[1][0], g_peopleToolbarPos[1][1], 0);
    if ((g_runModeFlags & GAME_PRO_CHALLENGE) == 0 || g_nextOpenHoleIndex <= 1 || g_pendingProIndex == -1
        || (g_runModeFlags & GAME_ANY_TOURNAMENT) != 0)
        g_peopleButtonBmps[10].BlitScaled(g_pMenu, g_peopleToolbarPos[2][0], g_peopleToolbarPos[2][1], 0);
    if ((g_runModeFlags & GAME_TOURNAMENT_OFFERED) == 0 || (g_runModeFlags & GAME_ANY_TOURNAMENT) != 0)
        g_peopleButtonBmps[14].BlitScaled(g_pMenu, g_peopleToolbarPos[3][0], g_peopleToolbarPos[3][1], 0);
    switch (sel) {
    case 0:
        g_golferPool[SCRATCH_GOLFER].persona = PERSONA_ROSTER;
        g_golferPool[SCRATCH_GOLFER].typeIdx = 0;
        if (g_golferTypeArray[0].fixedKind != 0)
            g_golferPool[SCRATCH_GOLFER].talents = (unsigned char)(g_golferTypeArray[0].fixedKind & SKILLS_ALL);
        else
            g_golferPool[SCRATCH_GOLFER].talents = SKILLS_ALL;
        RenderGolferCustomizationScreen(0, SCRATCH_GOLFER);
        break;
    case 1:
        if (g_nextOpenHoleIndex > 1 && (g_runModeFlags & GAME_ANY_TOURNAMENT) == 0 && g_watchedPlayerIdx == -1)
            g_runModeFlags = (g_runModeFlags & ~GAME_PRO_CHALLENGE) | GAME_START_MATCH;
        break;
    case 2:
        if ((g_runModeFlags & GAME_PRO_CHALLENGE) != 0 && g_nextOpenHoleIndex > 1 && g_pendingProIndex != -1
            && g_watchedPlayerIdx == -1 && (g_runModeFlags & GAME_ANY_TOURNAMENT) == 0)
            g_runModeFlags |= GAME_START_MATCH;
        break;
    case 3:
        if ((g_runModeFlags & GAME_TOURNAMENT_OFFERED) != 0 && (g_runModeFlags & GAME_ANY_TOURNAMENT) == 0
            && g_watchedPlayerIdx == -1)
            return 0x4a;
        break;
    case 4: g_shotSelectedKind = 0; break;
    case 5: g_shotSelectedKind = -1; break;
    case 6: g_shotSelectedKind = 1; break;
    case 7: g_shotSelectedKind = SHOT_HIGH_BACKSPIN; break;
    case 8: g_shotSelectedKind = SHOT_LOW_PUNCH; break;
    case 9: g_toolbarView = TOOLBAR_GOLFERS; break;
    case 10:
        g_staffHireButtonIdle = 1;
        g_employeeListActive = 1;
        break;
    }
    return none;
}

void RenderPeopleToolbarWithTooltip(int x, int y)
{
    int sel, dim, i, tx, ty;

    if (g_employeeListActive != 0 && g_toolbarView != TOOLBAR_SHOT) {
        RenderEmployeePanel(x, y);
        return;
    }
    g_toolPanelBgImages[5].draw_antialiased(&g_toolbarPanelBackings_A[5], g_pMenu, 0xd6, 0x1da, 0);
    sel = HitTestPeopleToolbarButtons(x, y);
    if (sel != -1) {
        if (sel >= 4 && sel <= 8)
            g_peopleButtonBmps[sel * 4].draw_antialiased(&g_joeCoolToolbarBmpsAlpha[0], g_pMenu,
                g_peopleToolbarPos[sel][0], g_peopleToolbarPos[sel][1], 0);
        else
            g_peopleButtonBmps[sel * 4].BlitScaled(g_pMenu, g_peopleToolbarPos[sel][0], g_peopleToolbarPos[sel][1], 0);
    }
    if (g_watchedPlayerIdx != -1) {
        g_peopleButtonBmps[1 * 4 + 2].BlitScaled(g_pMenu, g_peopleToolbarPos[1][0], g_peopleToolbarPos[1][1], 0);
        g_peopleButtonBmps[2 * 4 + 2].BlitScaled(g_pMenu, g_peopleToolbarPos[2][0], g_peopleToolbarPos[2][1], 0);
        g_peopleButtonBmps[3 * 4 + 2].BlitScaled(g_pMenu, g_peopleToolbarPos[3][0], g_peopleToolbarPos[3][1], 0);
        if (g_golferPool[g_watchedPlayerIdx].targetX == -1) {
            dim = g_squareTypeTable[GetTileAtPosition(g_golferPool[g_watchedPlayerIdx].ballX,
                                        g_golferPool[g_watchedPlayerIdx].y)]
                      .lieGrade
                  <= 0;
            g_peopleButtonBmps[4 * 4 + ((g_shotSelectedKind == 0) ? 1 : ((sel != 4) ? 3 : 0))].draw_antialiased(
                &g_joeCoolToolbarBmpsAlpha[(g_shotSelectedKind == 0) ? 1 : (sel != 4)], g_pMenu,
                g_peopleToolbarPos[4][0], g_peopleToolbarPos[4][1], 0);
            if (dim == 0) {
                g_peopleButtonBmps[5 * 4 + 2].draw_antialiased(&g_joeCoolToolbarBmpsAlpha[1], g_pMenu,
                    g_peopleToolbarPos[5][0], g_peopleToolbarPos[5][1], 0);
                g_peopleButtonBmps[6 * 4 + 2].draw_antialiased(&g_joeCoolToolbarBmpsAlpha[1], g_pMenu,
                    g_peopleToolbarPos[6][0], g_peopleToolbarPos[6][1], 0);
                g_peopleButtonBmps[7 * 4 + 2].draw_antialiased(&g_joeCoolToolbarBmpsAlpha[1], g_pMenu,
                    g_peopleToolbarPos[7][0], g_peopleToolbarPos[7][1], 0);
            } else {
                g_peopleButtonBmps[5 * 4 + ((g_shotSelectedKind == -1) ? 1 : ((sel != 5) ? 3 : 0))].draw_antialiased(
                    &g_joeCoolToolbarBmpsAlpha[(g_shotSelectedKind == -1) ? 1 : (sel != 4)], g_pMenu,
                    g_peopleToolbarPos[5][0], g_peopleToolbarPos[5][1], 0);
                g_peopleButtonBmps[6 * 4 + ((g_shotSelectedKind == 1) ? 1 : ((sel != 6) ? 3 : 0))].draw_antialiased(
                    &g_joeCoolToolbarBmpsAlpha[(g_shotSelectedKind == 1) ? 1 : (sel != 4)], g_pMenu,
                    g_peopleToolbarPos[6][0], g_peopleToolbarPos[6][1], 0);
                g_peopleButtonBmps[7 * 4 + ((g_shotSelectedKind == SHOT_HIGH_BACKSPIN) ? 1 : ((sel != 7) ? 3 : 0))]
                    .draw_antialiased(
                        &g_joeCoolToolbarBmpsAlpha[(g_shotSelectedKind == SHOT_HIGH_BACKSPIN) ? 1 : (sel != 4)],
                        g_pMenu, g_peopleToolbarPos[7][0], g_peopleToolbarPos[7][1], 0);
            }
            g_peopleButtonBmps[8 * 4 + ((g_shotSelectedKind == SHOT_LOW_PUNCH) ? 1 : ((sel != 8) ? 3 : 0))]
                .draw_antialiased(&g_joeCoolToolbarBmpsAlpha[(g_shotSelectedKind == 0) ? 1 : (sel != 4)], g_pMenu,
                    g_peopleToolbarPos[8][0], g_peopleToolbarPos[8][1], 0);
        } else {
            g_toolPanelBgImages[6].BlitScaled(g_pMenu, 0x129, 0x1ed, 0);
            RenderScorecard(g_watchedPlayerIdx);
        }
    } else {
        if (g_nextOpenHoleIndex <= 1)
            g_peopleButtonBmps[1 * 4 + 2].BlitScaled(g_pMenu, g_peopleToolbarPos[1][0], g_peopleToolbarPos[1][1], 0);
        if ((g_runModeFlags & GAME_PRO_CHALLENGE) == 0 || g_nextOpenHoleIndex <= 1 || g_pendingProIndex == -1)
            g_peopleButtonBmps[2 * 4 + 2].BlitScaled(g_pMenu, g_peopleToolbarPos[2][0], g_peopleToolbarPos[2][1], 0);
        if ((g_runModeFlags & GAME_TOURNAMENT_OFFERED) == 0)
            g_peopleButtonBmps[3 * 4 + 2].BlitScaled(g_pMenu, g_peopleToolbarPos[3][0], g_peopleToolbarPos[3][1], 0);
        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
        tx = 0x1ee;
        ty = 0x21c;
        for (i = 0; i < 10; i++) {
            if (g_golferSkillGrades[i] != 0) {
                strcpy(g_printBuffer, g_skillNames[i]);
                strcat(g_printBuffer, " +");
                strcat(g_printBuffer, itoa(g_golferSkillGrades[i] * 10, g_numText, 10));
                strcat(g_printBuffer, "%");
                DrawTextWithColor(g_printBuffer, tx, ty, COLOR_WHITE);
                ty += 0xc;
                if (ty >= 0x258) {
                    tx = 0x281;
                    ty = 0x21c;
                }
            }
        }
    }
    if (sel == g_panelHoverButton) {
        g_panelHoverTicks++;
        if (sel == -1)
            return;
        if (g_panelHoverTicks <= 0xa)
            return;
        g_printBuffer[0] = 0;
        switch (sel) {
        case 1: strcpy(g_printBuffer, "Practice Round"); break;
        case 2:
            strcpy(g_printBuffer, "Play ");
            if (g_pendingProIndex != -1)
                strcat(g_printBuffer, g_golfProList[g_pendingProIndex].name);
            else
                strcat(g_printBuffer, "vs. a pro");
            break;
        case 3: strcpy(g_printBuffer, "Begin Tournament"); break;
        case 4: strcpy(g_printBuffer, "Straight shot"); break;
        case 5: strcpy(g_printBuffer, "Fade shot, L to R"); break;
        case 6: strcpy(g_printBuffer, "Draw shot, R to L"); break;
        case 7: strcpy(g_printBuffer, "High backspin shot"); break;
        case 8: strcpy(g_printBuffer, "Low punch shot"); break;
        case 9: strcpy(g_printBuffer, "Golfers"); break;
        case 10: strcpy(g_printBuffer, "Hire employee"); break;
        }
        RenderToolbarTooltip(x, y - 5);
    } else {
        g_panelHoverTicks = 0;
        g_panelHoverButton = sel;
    }
}

int HitTestEmployeePanel(int x, int y)
{
    int r = -1;
    if (ApproxDistance(x - 286, y - 492) < 16)
        r = -2;
    if (ApproxDistance(x - 256, y - 510) < 16)
        r = -3;
    int i = 0;
    short *p = g_employeePanelPos[0];
    for (;;) {
        if (p != g_employeePanelPos[2] && p != g_employeePanelPos[3]) {
            if (ApproxDistance(x - (i < 8 ? 16 : 32) - p[0], y - p[1] - 16) < 16)
                return i;
        } else {
            if (ApproxDistance((x - p[0]) * 4 - 32, y - p[1] - 30) < 30)
                return i;
        }
        p += 2;
        i++;
        if (p[0] == -1)
            break;
    }
    if (r == 1)
        r = 0;
    return r;
}

int HandleEmployeeListClick(int x, int y)
{
    int sel;
    int none;
    sel = HitTestEmployeePanel(x, y);
    if (sel == -3) {
        g_employeeListActive = 0;
        g_toolbarView = TOOLBAR_PEOPLE;
        return -1;
    }
    if (sel == -2) {
        g_employeeListActive = 0;
        g_toolbarView = TOOLBAR_GOLFERS;
        return -1;
    }
    none = -1;
    switch (sel) {
    case 0:
    case 1:
        g_employeeListPick = none;
        g_staffHireButtonIdle = 0;
        break;
    case 2:
        if (g_employeeListScroll != 0) {
            g_employeeListPick = none;
            g_employeeListScroll -= 2;
        }
        break;
    case 3:
        g_employeeListPick = none;
        g_employeeListScroll += 2;
        break;
    case 4:
        if (g_staffHireButtonIdle != 0 && g_employeeListPick != -1)
            g_selectedItemIndex = g_employeeListIds[g_employeeListPick];
        break;
    case 6:
        if (g_staffHireButtonIdle != 0 && g_employeeListPick != -1) {
            int id = g_employeeListIds[g_employeeListPick];
            g_printBuffer[0] = 0;
            BuildEmployeeName(id);
            ShowTextInputDialog("Rename Employee...", NAME_DIALOG_X, 0x1b0, 0x1f);
            strcpy(g_staffSlots[id].customName, g_printBuffer);
        }
        break;
    case 5:
        if (g_staffHireButtonIdle != 0 && g_employeeListPick != -1) {
            int id = g_employeeListIds[g_employeeListPick];
            g_staffSlots[id].flags = 0;
            g_gameCashHundreds -= 25;
            AddMoneyPopup(-25, g_staffSlots[id].posX, g_staffSlots[id].posY, -1);
            g_selectedItemIndex = none;
            g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_SALARIES] -= 25;
        }
        break;
    default:
        if (sel >= 8) {
            g_employeeListPick = sel - 8;
            if (g_employeeListPick >= g_employeeListCount) {
                g_employeeListPick = none;
            } else {
                int id = g_employeeListIds[g_employeeListPick];
                g_cameraTileX = g_staffSlots[id].posX >> 10;
                g_cameraTileY = g_staffSlots[id].posY >> 10;
            }
        }
        break;
    }
    return none;
}

void RenderEmployeePanel(int x, int y)
{
    int sel, i, e, sz, which, k, rem, loc, v, phase, ex, ey;
    signed char fl;
    unsigned col;
    unsigned bit;

    g_toolPanelBgImages[7].draw_antialiased(&g_toolbarPanelBackings_A[7], g_pMenu, 0xd7, 0x1da, 0);

    if (g_employeeListScroll > 0) {
        if (g_employeeListCount < 7)
            g_employeeListScroll -= 2;
        if (g_employeeListScroll > 0)
            g_staffPanelSpr[10].Draw(g_pMenu, g_employeePanelPos[2][0], g_employeePanelPos[2][1], 1, 1, 1, 0);
    }
    if (g_employeeListCount > 8)
        g_staffPanelSpr[14].Draw(g_pMenu, g_employeePanelPos[3][0], g_employeePanelPos[3][1], 1, 1, 1, 0);

    sel = HitTestEmployeePanel(x, y);
    if (sel == 2 && g_employeeListScroll == 0)
        sel = -1;
    if (sel == 3 && g_employeeListCount <= 8)
        sel = -1;

    if (sel != -1) {
        if (sel < 0) {
            if (sel == -2)
                g_gameGlowingPanelDiscs[2].Draw(g_pMenu, 0x10e, 0x1db, 1, 1, 1, 0);
            else if (sel == -3)
                g_gameGlowingPanelDiscs[3].Draw(g_pMenu, 0xf0, 0x1ee, 1, 1, 1, 0);
        } else if (sel < 8) {
            g_staffPanelSpr[sel * 4].Draw(g_pMenu, g_employeePanelPos[sel][0], g_employeePanelPos[sel][1], 1, 1, 1, 0);
        } else {
            g_staffPanelSpr[32 + (sel & 1) * 4].Draw(g_pMenu, g_employeePanelPos[sel][0], g_employeePanelPos[sel][1], 1,
                1, 1, 0);
        }
    }
    g_staffPanelSpr[5].Draw(g_pMenu, g_employeePanelPos[1][0], g_employeePanelPos[1][1], 1, 1, 1, 0);

    if (g_employeeListPick != -1) {
        g_staffPanelSpr[33 + (g_employeeListPick & 1) * 4].Draw(g_pMenu, g_employeePanelPos[8 + g_employeeListPick][0],
            g_employeePanelPos[8 + g_employeeListPick][1], 1, 1, 1, 0);
        e = g_employeeListIds[g_employeeListPick];
        sz = (g_staffSlots[e].job != -4) ? 4 : 6;
        if (g_staffSlots[e].job == -4 && (g_staffSlots[e].flags & STAFF_UPGRADED))
            sz += 2;
        switch (g_staffSlots[e].job) {
        case -4: col = 0x80007ff0; break;
        case -3: col = 0x80000018; break;
        case -5: col = 0x800003ff; break;
        case -2: col = 0x80006318; break;
        default: col = 0x80006318; break;
        }
        g_cameraZoom = g_simCameraZoomStored;
        DrawCircleOnTerrain((g_staffSlots[e].postRow << 10) + 0x200, (g_staffSlots[e].postCol << 10) + 0x200, sz * 25,
            col);
        g_cameraZoom = DEFAULT_ZOOM;
    }

    g_employeeListCount = -g_employeeListScroll;
    for (i = 0; i < 64; i++) {
        if (g_staffSlots[i].job >= 0)
            continue;
        fl = g_staffSlots[i].flags;
        if (fl == 0)
            continue;
        if (g_staffSlots[i].job == JOB_PLAYER_AVATAR)
            continue;
        if (g_employeeListCount < 0) {
            g_employeeListCount++;
        } else if (g_employeeListCount >= 8) {
            g_employeeListCount++;
        } else {
            phase = (g_camDirIdx - g_staffSlots[i].dir - 2) & 7;
            v = -2 - g_staffSlots[i].job;
            if (fl & STAFF_UPGRADED)
                v += 4;
            loc = v + 0x82;
            if (g_staffSlots[i].anim < 0xb) {
                k = 0x20e;
            } else {
                k = 0x216;
                if (g_staffSlots[i].anim == 0xc)
                    k = 0x21e;
            }
            k += v;
            rem = g_staffSlots[i].animTick % g_flicBankFrameLens[k];
            if (rem == 0 && g_staffSlots[i].anim == 0xc)
                g_staffSlots[i].anim = 0xb;
            ex = g_employeePanelPos[8 + g_employeeListCount][0] + 0x1e;
            ey = g_employeePanelPos[8 + g_employeeListCount][1] + 0x18;
            g_rowIconSprite = GetFlicBankSprite(k, rem, phase);
            DrawSpriteImmediate(ex, ey, ey, 0x1e, 0x28, g_rowIconSprite, SPRITE_FULL_SCALE, loc);
            g_employeeListIds[g_employeeListCount] = i;
            g_employeeListCount++;
        }
    }

    if (g_employeeListPick != -1) {
        e = g_employeeListIds[g_employeeListPick];
        if (g_staffSlots[e].flags == 0) {
            g_employeeListPick = -1;
            return;
        }
        g_staffPanelSpr[16 + ((sel != 4) ? 3 : 0)].Draw(g_pMenu, g_employeePanelPos[4][0], g_employeePanelPos[4][1], 1,
            1, 1, 0);
        if (g_selectedItemIndex == e)
            g_staffPanelSpr[17].Draw(g_pMenu, g_employeePanelPos[4][0], g_employeePanelPos[4][1], 1, 1, 1, 0);
        g_staffPanelSpr[20 + ((sel != 5) ? 3 : 0)].Draw(g_pMenu, g_employeePanelPos[5][0], g_employeePanelPos[5][1], 1,
            1, 1, 0);
        g_staffPanelSpr[24 + ((sel != 6) ? 3 : 0)].Draw(g_pMenu, g_employeePanelPos[6][0], g_employeePanelPos[6][1], 1,
            1, 1, 0);

        bit = g_staffSlots[e].flags & STAFF_UPGRADED;
        which = (bit >> 3) - (g_staffSlots[e].job * 2 + 4);
        g_printBuffer[0] = 0;
        BuildEmployeeName(e);
        DrawTextCenteredAtY(g_printBuffer, 0x2bc, 0x216, 0x80000000);
        g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);

        strcpy(g_printBuffer, "Hired: ");
        FormatDateString(g_staffSlots[e].hiredAt << 10);
        DrawTextCenteredAtY(g_printBuffer, 0x2bc, 0x222, 0x80000000);

        strcpy(g_printBuffer, "Paid: ");
        FormatNumberWithCommas(g_staffSlots[e].paid * 100);
        DrawTextCenteredAtY(g_printBuffer, 0x2bc, 0x22e, 0x80000000);

        switch (which) {
        case 0: strcpy(g_printBuffer, "Players greeted:"); break;
        case 1: strcpy(g_printBuffer, "Players cheered:"); break;
        case 2: strcpy(g_printBuffer, "Players rushed:"); break;
        case 3: strcpy(g_printBuffer, "Slackers intimidated:"); break;
        case 4: strcpy(g_printBuffer, "Weeds destroyed:"); break;
        case 5: strcpy(g_printBuffer, "Weeds eradicated:"); break;
        case 6: strcpy(g_printBuffer, "Beverages served:"); break;
        case 7: strcpy(g_printBuffer, "Satisfied customers:"); break;
        }
        DrawTextCenteredAtY(g_printBuffer, 0x2bc, 0x23c, 0x80000000);

        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(g_staffSlots[e].tally, g_numText, 10));
        DrawTextCenteredAtY(g_printBuffer, 0x2bc, 0x248, 0x80000000);
        FormatDateString(g_clock);
    } else {
        g_staffPanelSpr[18].Draw(g_pMenu, g_employeePanelPos[4][0], g_employeePanelPos[4][1], 1, 1, 1, 0);
        g_staffPanelSpr[22].Draw(g_pMenu, g_employeePanelPos[5][0], g_employeePanelPos[5][1], 1, 1, 1, 0);
        g_staffPanelSpr[26].Draw(g_pMenu, g_employeePanelPos[6][0], g_employeePanelPos[6][1], 1, 1, 1, 0);
    }

    if (g_employeeListCount + g_employeeListScroll >= 0x14)
        g_staffPanelSpr[2].Draw(g_pMenu, g_employeePanelPos[0][0], g_employeePanelPos[0][1], 1, 1, 1, 0);

    if (g_staffHireButtonIdle == 0) {
        g_staffPanelSpr[1].Draw(g_pMenu, g_employeePanelPos[0][0], g_employeePanelPos[0][1], 1, 1, 1, 0);
        if (g_employeeListCount + g_employeeListScroll < 0x14)
            ProcessHireEmployeeSelection();
        else
            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
        g_staffHireButtonIdle = 1;
    }

    if (sel == g_panelHoverButton) {
        g_panelHoverTicks = g_panelHoverTicks + 1;
        if (sel == -1)
            return;
        if (g_panelHoverTicks <= 10)
            return;
        g_printBuffer[0] = 0;
        switch (sel) {
        case -2: strcpy(g_printBuffer, "Golfers"); break;
        case -3: strcpy(g_printBuffer, g_golferTypeArray[0].name); break;
        case 0: strcpy(g_printBuffer, "Hire employees"); break;
        case 1: strcpy(g_printBuffer, "View employees"); break;
        case 4: strcpy(g_printBuffer, "Move this employee"); break;
        case 6: strcpy(g_printBuffer, "Rename this employee"); break;
        case 5: strcpy(g_printBuffer, "Fire this employee"); break;
        }
        RenderToolbarTooltip(x, y - 5);
    } else {
        g_panelHoverTicks = 0;
        g_panelHoverButton = sel;
    }
}

short g_mainBarButtonPos[12][2] = {{44, 474}, {117, 496}, {176, 537}, {72, 549}, {106, 563}, {130, 579}, {77, 586},
    {31, 542}, {31, 584}, {16, 564}, {45, 565}, {-1, -1}};
short g_lowerLeftTabPos[4][2] = {{7, 436}, {84, 463}, {146, 503}, {0, 0}};
short g_zoomMapIconPos[2] = {19, 530};
short g_unzoomMapIconPos[2] = {19, 573};
short g_lowerLeftIconPosB[3][2] = {{4, 551}, {33, 552}, {59, 535}};
short g_pauseIconPos[2] = {94, 553};
short g_lowerLeftIconPosC[3][2] = {{121, 570}, {0, 0}, {63, 573}};
signed char g_mainBarTabImage[12] = {0, 1, 2, 8, 9, 10, 12, 4, 5, 6, 7, 0};
signed char g_terrainToolbarTerrain[20] = {0, 1, 7, 4, 9, 10, 17, 2, 3, 5, 8, 11, 12, 13, 14, 15, -1, 0, 0, 0};
short g_terrainToolbarPos[29][2] = {{270, 508}, {332, 508}, {394, 508}, {456, 508}, {518, 508}, {580, 508}, {642, 508},
    {301, 546}, {363, 546}, {425, 546}, {487, 546}, {549, 546}, {611, 546}, {704, 477}, {673, 530}, {735, 511},
    {-1, -1}, {0, 0}, {0, 0}, {0, 0}, {228, 551}, {263, 565}, {219, 508}, {-1, -1}, {11308, 11308}, {11308, 11308},
    {11308, 11308}, {16172, 16448}, {42, 0}};
int g_themeToolbarPos[4][3][2] = {{{704, 477}, {673, 530}, {735, 511}}, {{704, 488}, {673, 533}, {735, 496}},
    {{704, 483}, {673, 519}, {735, 496}}, {{704, 478}, {673, 512}, {735, 524}}};
short g_elevationToolbarPos[4][2] = {{312, 501}, {429, 501}, {546, 501}, {688, 501}};
short g_buildingToolbarPos[10][2] = {{274, 536}, {302, 504}, {334, 538}, {449, 553}, {571, 553}, {632, 553}, {693, 553},
    {234, 553}, {762, 545}, {510, 553}};
short g_peopleToolbarPos[11][2] = {{255, 542}, {298, 503}, {298, 535}, {298, 565}, {374, 485}, {453, 485}, {532, 485},
    {611, 485}, {690, 485}, {270, 475}, {217, 522}};
int g_employeeListPick = -1;
short g_employeePanelPos[17][2] = {{254, 542}, {254, 542}, {313, 519}, {596, 519}, {631, 497}, {683, 494}, {740, 497},
    {683, 494}, {336, 507}, {336, 550}, {399, 507}, {399, 550}, {462, 507}, {462, 550}, {525, 507}, {525, 550},
    {-1, -1}};

int g_themeWritePaletteFresh = 1;
short g_customizeSpots[22][2] = {{0xd2, 0x96}, {0xd2, 0xb9}, {0xd2, 0xdc}, {0x5d, 0x7f}, {0x5d, 0x9c}, {0x5d, 0xb6},
    {0x5d, 0xd3}, {0x5d, 0xef}, {0x30, 0x16}, {0x102, 0x16}, {0x1c6, 0x1e}, {0x1c6, 0x50}, {0x1c6, 0x82}, {0x1c6, 0xb4},
    {0x1c6, 0xe6}, {0x148, 0x38}, {0x148, 0x6b}, {0x148, 0x9d}, {0x2fa, 0x21}, {0x2f8, 0xdc}, {0x244, 0x62}, {0, 0}};
short g_customizeDrawSpots[20][2] = {{0x9d, 0x87}, {0x9d, 0xaa}, {0x9d, 0xcd}, {0x27, 0x73}, {0x27, 0x8f}, {0x27, 0xab},
    {0x27, 0xc7}, {0x27, 0xe3}, {0x21, 0x07}, {0xf4, 0x07}, {0x1b4, 0x0b}, {0x1b4, 0x3d}, {0x1b4, 0x6f}, {0x1b4, 0xa1},
    {0x1b4, 0xd3}, {0x136, 0x25}, {0x136, 0x57}, {0x136, 0x89}, {0x2e7, 0x10}, {0x2e0, 0xc0}};
int g_headSlots[10][2] = {{0x1d, 0x131}, {0x61, 0x1a9}, {0xa5, 0x131}, {0xe9, 0x1a9}, {0x12d, 0x131}, {0x171, 0x1a9},
    {0x1b5, 0x131}, {0x1f9, 0x1a9}, {0x23d, 0x131}, {0x281, 0x1a9}};

int g_peopleListGolfer[0x98] = {0};
int g_rowHoverButton = 0;
int g_panelHoverButton = 0;
static int s_spareWord = 0;
int g_pauseBlinkTicks = 0;
int g_selectedRowButton = 0;
int g_toolbarClickResult = 0;
int g_toolbarHoverTicks = 0;
char g_numText[12] = {0};
