#include "jgldefs.h"
#include "crt.h"
#include "Golfer.h"
#include "GolferSpawn.h"
#include "SaveGame.h"
#include "FloatingText.h"
#include "Plot.h"
#include "Employee.h"
#include "TerrainGrid.h"
#include "CourseGrid.h"
#include "Geometry.h"
#include "RandomSource.h"
#include "BuildingSearch.h"
#include "ThemeLoader.h"
#include "LoadingScreen.h"
#include "ClubReports.h"
#include "GameFlags.h"
#include "Screen.h"
#include "AssetSurface.h"
#include "TextWrap.h"
#include "ScreenText.h"
#include "DialogFrame.h"
#include "InputPoll.h"
#include "SpriteAssets.h"
#include "Sprite.h"
#include "SoundEffects.h"
#include "SetupScreens.h"
#include "Rgb555.h"
#include "Tournament.h"

char g_tournamentNumBuf[20] = {0};

void InitTournamentGolfers()
{
    int tries, gi, j, n, k, ri, c;
    int score, s, m, tw, d, r;
    char used[100];

    memset(used, 0, sizeof(used));
    memcpy(g_gameGolferTypesBak, g_golferTypeArray, 84 * sizeof(GolferType));
    memcpy(g_phrasesPerGolferTypeSave, g_phrasesPerGolferType, 105000);
    for (c = 0; c < 152; c++) {
        g_golferPool[c].holeIdx = 0;
        g_golferPool[c].partner = -1;
    }
    g_ceoLadderRung &= ~0x80;
    g_nextGolferOrder = 100;
    for (n = 1; n < 19; n++) {
        if (g_allHolesArray[n].par == 0)
            continue;
        for (j = 0; j < 2; j++) {
            gi = (n - 1) * 2 + j;
            memset(&g_golferPool[gi], 0, sizeof(Golfer));
            g_golferPool[gi].x = (g_buildingTable[0].row << 10) + 0x600;
            g_golferPool[gi].y = (g_buildingTable[0].col << 10) + 0x600;
            g_golferPool[gi].holeIdx = n;
            g_golferPool[gi].dir = g_allHolesArray[n].teeDir;
            g_golferPool[gi].ageFlags = g_rng.NextInt(0x7fff);
            g_golferPool[gi].talentLevel = 2;
            g_golferPool[gi].talents = SKILLS_ALL;
            g_golferPool[gi].typeIdx = (gi == 1) ? 0 : 0x4c;
            g_golferPool[gi].waitTimer = ((n - g_nextOpenHoleIndex) * 2 + j) * 4;
            g_golferPool[gi].storyId = -1;
            r = g_rng.NextInt(0x80);
            g_golferPool[gi].ageFlags = (r << 8) + g_golferTypeArray[g_golferPool[gi].typeIdx].ageBits;
            g_golferPool[gi].activity = ACTIVITY_WALK;
            g_golferPool[gi].arrivedAt = g_clock;
            g_golferPool[gi].enjoyment = g_rng.NextInt(4) + 3;
            g_golferPool[gi].seq = g_nextGolferOrder;
            g_nextGolferOrder--;
            if (g_level == 0)
                g_golferPool[gi].enjoyment = 5;
            if (g_simOpenFacilityLvl[SCENERY_SWIM_CLUB])
                g_golferPool[gi].enjoyment = g_simOpenFacilityLvl[SCENERY_SWIM_CLUB] + 4;
            g_golferPool[gi].partner = gi ^ 1;
            tries = 0;
            for (;;) {
                ri = g_rng.NextInt(100);
                d = g_golfProList[ri].ratingTotal - 10;
                score = d * d / ((5 * g_level + 20) * 2);
                if (gi == 0)
                    score += g_level * score / -6;
                if (g_runModeFlags & GAME_CHAMPIONSHIP)
                    score = g_rng.NextInt(score);
                if (g_golfProList[ri].figure == -1)
                    continue;
                if (ri == 0)
                    continue;
                if (++tries < 800 && used[ri])
                    continue;
                m = g_gameCashHundreds / 200;
                s = g_winnersPurseK / 10;
                if (score > tries / (4 - g_level) + s + m)
                    continue;
                if (gi == 0)
                    tw = tries / 4;
                else
                    tw = tries;
                if (score < s - tw + m)
                    continue;
                break;
            }
            if (used[ri])
                ri = 0;
            else
                used[ri] = 1;
            g_golferPool[gi].persona = (gi == 1) ? PERSONA_PLAYER : PERSONA_ROSTER;
            if (gi != 1)
                g_golferPool[gi].rosterIndex = ri;
            g_golferPool[gi].flags &= ~(GOLFER_PRACTISED_RANGE | GOLFER_VISITED_PRO_SHOP | GOLFER_PRACTISED_GREEN);
            g_golferPool[gi].talentLevel = (gi != 1) ? Clamp(g_level, 0, 2) : 2;
            if (gi != 1) {
                k = gi;
                if (gi == 0)
                    k = 1;
                strcpy(g_printBuffer, g_golfProList[ri].name);
                strcat(g_printBuffer, ".pro");
                if (LoadGolferFromTheme(g_printBuffer, k, -1) == 0) {
                    memset(g_phrasesPerGolferType + k * 1250, 0, 1250);
                    g_golferTypeArray[k].pantsColor = g_golfProList[ri].pantsColor;
                    g_golferTypeArray[k].shirtColor = g_golfProList[ri].shirtColor;
                    g_golferTypeArray[k].skinTone = g_golfProList[ri].skinTone;
                    signed char fl = g_golfProList[ri].figure;
                    g_golferTypeArray[k].appearance = g_golfProList[ri].appearanceColor | ((fl & OUTFIT_MASK) << 4);
                    g_golferTypeArray[k].ageBits = 9;
                    if (fl & ROSTER_FEMALE)
                        g_golferTypeArray[k].ageBits = (signed char)0x89;
                    g_golferTypeArray[k].caps = 1 << g_rng.NextInt(5);
                    r = g_rng.NextInt(0x80);
                    g_golferPool[gi].ageFlags = (r << 8) + g_golferTypeArray[k].ageBits;
                    strcpy(g_golferTypeArray[k].name, g_golfProList[ri].name);
                    strcpy(g_golferTypeArray[k].title, "Golf Pro");
                    for (c = 0; c < 3; c++)
                        g_golferPool[gi].ratingWords[c] = g_golfProList[ri].ratingWords[c];
                } else {
                    g_golferPool[gi].ageFlags = g_golferTypeArray[k].ageBits;
                }
                g_golferPool[gi].typeIdx = k;
                g_golferPool[gi].talents = SKILLS_ALL;
                g_golferPool[gi].skillMask = 0xe0;
                for (c = 0; c < 12; c++)
                    if (g_golferPool[gi].ratings[c])
                        g_golferPool[gi].skillMask |= 1 << c;
            } else {
                g_golferPool[1].talents = 0x47;
                g_skillMasks[0] = 0;
                for (c = 0; c < 12; c++) {
                    g_golferPool[1].ratings[c] = g_golferSkillGrades[c];
                    if (g_golferSkillGrades[c])
                        g_skillMasks[0] |= 1 << c;
                }
                g_golferPool[1].skillMask = g_skillMasks[0];
                g_golferPool[1].flags |= GOLFER_PRO_MATCH;
                if (!(g_runModeFlags & GAME_CHAMPIONSHIP)) {
                    g_golferPool[1].x = g_staffSlots[0].posX;
                    g_golferPool[1].y = g_staffSlots[0].posY;
                    g_golferPool[1].dir = g_staffSlots[0].dir;
                }
                g_golferPool[1].waitTimer = 0;
            }
        }
    }
    g_nextGolferOrder = 100;
    g_curTournPrepBits = -1;
    g_runModeFlags |= GAME_TOURNAMENT;
}

void RespawnGolferPairsPerHole()
{
    int i;
    for (i = 0; i < 152; i++)
        g_golferPool[i].holeIdx = 0;
    g_nextGolferOrder = 0;
    g_curNextSpawnPos = 0;
    for (i = 18; i >= 0; i--) {
        if (g_allHolesArray[i].par != 0) {
            int j;
            for (j = 0; j < 2; j++) {
                int k = SpawnGolfer(1);
                g_golferPool[k].holeIdx = i;
                g_golferPool[k].seq = g_nextGolferOrder++;
                g_golferPool[k].partner = k ^ 1;
            }
        }
    }
}

void EndTournament()
{
    int i, d;
    for (i = 0; i < 256; i++) {
        if (g_buildingTable[i].kind == 0x11 || g_buildingTable[i].kind == 0x12)
            DemolishBuilding(i);
    }
    memcpy(g_golferTypeArray, g_gameGolferTypesBak, 84 * sizeof(GolferType));
    memcpy(g_phrasesPerGolferType, g_phrasesPerGolferTypeSave, 105000);
    for (i = 0; i < 18; i++) {
        if (g_allHolesArray[g_nextOpenHoleIndex].par == 0)
            continue;
        g_allHolesArray[g_nextOpenHoleIndex].par = 3;
        d = g_allHolesArray[g_nextOpenHoleIndex].yards;
        if ((g_allHolesArray[g_nextOpenHoleIndex].flags & (HOLE_DOGLEG_LEFT | HOLE_DOGLEG_RIGHT)) && d > 250)
            d += 25;
        if (d > 300)
            d -= 25 * g_buildingLevelPerKind[SCENERY_DRIVING_RANGE];
        if (d <= 50)
            g_allHolesArray[g_nextOpenHoleIndex].par = 2;
        if (d >= 250)
            g_allHolesArray[g_nextOpenHoleIndex].par = 4;
        if (d >= 475)
            g_allHolesArray[g_nextOpenHoleIndex].par = 5;
        if (d > 625)
            g_allHolesArray[g_nextOpenHoleIndex].par = 6;
        if (d <= 50)
            g_allHolesArray[g_nextOpenHoleIndex].par = 2;
        if (d >= 250)
            g_allHolesArray[g_nextOpenHoleIndex].par = 4;
        if (d >= 475)
            g_allHolesArray[g_nextOpenHoleIndex].par = 5;
        if (d > 625)
            g_allHolesArray[g_nextOpenHoleIndex].par = 6;
    }
    g_curTournPrepBits = 0;
    RespawnGolferPairsPerHole();
}

void PrepareTournamentCourse()
{
    short list[30];
    short n;
    int i, k, m, col, dir;
    int par, j, idx, slot, row, dRow, dCol;
    signed char t;

    n = 0;
    strcpy(g_printBuffer, "In preparation for the tournament the\nfollowing changes have been recommended:\n");
    strcat(g_printBuffer, " Install TV towers and booths for live broadcast.\n");
    strcat(g_printBuffer, " Roll your greens extra smooth and fast.\n");
    strcat(g_printBuffer, " Increase the depth of rough and deep rough grass.\n");
    for (i = 1; i < 19; i++) {
        if (g_allHolesArray[i].par == 0)
            continue;
        par = 3;
        if (g_allHolesArray[i].yards >= 300)
            par = 4;
        if (g_allHolesArray[i].yards >= 500)
            par = 5;
        if (par == g_allHolesArray[i].par)
            continue;
        strcat(g_printBuffer, " Change hole ");
        strcat(g_printBuffer, itoa(i, g_tournamentNumBuf, 10));
        strcat(g_printBuffer, " from a par ");
        strcat(g_printBuffer, itoa(g_allHolesArray[i].par, g_tournamentNumBuf, 10));
        strcat(g_printBuffer, " to a par ");
        strcat(g_printBuffer, itoa(par, g_tournamentNumBuf, 10));
        strcat(g_printBuffer, ".\n");
        g_allHolesArray[i].par = par;
        list[n] = i;
        n++;
    }
    if (g_runModeFlags & GAME_CHAMPIONSHIP)
        g_curTournPrepBits = 0xfffff;
    else
        g_curTournPrepBits = ShowActionDialog(0x190, 0x64, 0xfffff, 0, 0);
    j = 0;
    for (; j < n; j++) {
        if (!(g_curTournPrepBits & (1 << (j + 3))))
            g_allHolesArray[list[j]].par++;
    }
    for (i = 1; i < 19; i++) {
        if (g_allHolesArray[i].par == 0)
            continue;
        if (!(g_curTournPrepBits & 1))
            continue;
        for (k = 0; k < 2; k++) {
            if (k != 0 && i < g_nextOpenHoleIndex - 2)
                continue;
            col = g_allHolesArray[i].aimY;
            dir = g_allHolesArray[i].teeDir;
            row = g_allHolesArray[i].aimX;
            if (k != 0)
                dir = (dir + (g_rng.NextInt(2) ? 2 : -2)) & 7;
            m = 0;
            do {
                if (g_rng.NextInt(2))
                    row += g_dirRowStep[dir];
                if (g_rng.NextInt(2))
                    col += g_dirColStep[dir];
                if (m > 1) {
                    idx = row * 50 + col;
                    t = g_gameTileTerrainKind[idx];
                    if ((g_squareTypeTable[t].category == CATEGORY_ROUGH
                            || g_squareTypeTable[t].category == CATEGORY_TREES)
                        && t != TERRAIN_BUILDING && !IsPositionInvalid(row, col) && g_tileExtra[idx] == 0) {
                        slot = PlaceBuildingOnTerrain(row, col, 0x11 + (k != 0), 0);
                        g_buildingTable[slot].flags |= SLOT_REACHABLE;
                        dRow = g_allHolesArray[i].aimX - row;
                        dCol = g_allHolesArray[i].aimY - col;
                        if (abs(dRow) > abs(dCol))
                            g_buildingTable[slot].facing = (dRow > 0) ? 1 : 3;
                        else
                            g_buildingTable[slot].facing = (dCol > 0) ? 2 : 0;
                        m = 99;
                    }
                }
            } while (m++ < 6);
        }
    }
}

int ShowActionDialog(int cx, int top, int sel, int pickOne, int disabled)
{
    int my;
    int mx;
    int idx;
    int chosen;
    unsigned b;
    unsigned g;
    unsigned r;
    int firstOptY;
    int flat;
    int boxW;
    int keyNav;
    int halfW;
    int firstOptLine;
    int lineNo;
    int ticked;
    int boxH;
    int i, w, start, y, tx, color, numOpts, k;
    int boxFrame, highlight;
    char *p;

    start = 0;
    boxW = 0;
    boxH = 0;
    ticked = sel;
    chosen = 0;
    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
    if (pickOne != 0)
        ticked = 0;
    keyNav = 0;
    for (i = 0; i < strlen(g_printBuffer); i++) {
        if (g_printBuffer[i] == '\n') {
            g_printBuffer[i] = 0;
            w = g_pMenu->text_width(&g_printBuffer[start]);
            g_printBuffer[i] = '\n';
            start = i + 1;
            if (w > boxW)
                boxW = w;
            boxH++;
        }
    }
    w = g_pMenu->text_width(&g_printBuffer[start]);
    if (w > boxW)
        boxW = w;
    boxW = ((boxW - 1) | 15) + 49;
    boxH = (boxH * 3 + 3) * 8;
    highlight = -1;
    r = g_simScreen.GetPixelRed(cx, top, 0);
    g = g_simScreen.GetPixelGreen(cx, top, 0);
    b = g_simScreen.GetPixelBlue(cx, top, 0);
    flush_input();
    for (;;) {
        flat = top & 1;
        if (flat == 0) {
            halfW = boxW / 2;
            DrawDialogFrameWithBorder(cx - halfW, top, boxW, boxH, 1);
        } else {
            halfW = boxW / 2;
            g_simScreen.fill(cx - halfW, top, boxW, boxH, 0x80006318);
            color = PackRgb555(r, g, b);
            g_simScreen.fill(cx - halfW + 1, top + 1, boxW - 2, boxH - 2, color);
        }
        if (pickOne == 0)
            g_transPopUpButtonBmps[5].draw_antialiased(&g_transPopUpMaskSprites[5], g_pMenu, cx + halfW - 40,
                top + boxH - 40, 0);
        tx = cx - halfW + 12;
        y = top + 12;
        for (lineNo = 0, firstOptLine = -1, start = 0, i = 0; i < strlen(g_printBuffer); i++) {
            if (g_printBuffer[i] == '\n') {
                g_printBuffer[i] = 0;
                p = &g_printBuffer[start];
                if (*p != ' ') {
                    DrawTextCenteredWithShadow(p, cx, y, pickOne ? 0x80007fff : 0x80007ff0);
                } else {
                    if (firstOptLine == -1) {
                        firstOptY = y;
                        firstOptLine = lineNo;
                    }
                    idx = lineNo - firstOptLine;
                    if (disabled & (1 << idx)) {
                        DrawTextWithColor(p, tx + 24, y + 8, 0x80006318);
                    } else {
                        if (highlight == idx)
                            DrawTextClampedToScreen(p, tx + 24, y + 7,
                                ((1 << idx) & ticked) ? 0x80007fff : (flat ? 0x80002108 : 0x80000210));
                        else
                            DrawTextWithColor(p, tx + 24, y + 8,
                                ((1 << idx) & ticked) ? 0x80007fff : (flat ? 0x80002108 : 0x80000210));
                        if (pickOne != 0)
                            g_transPopUpButtonBmps[((1 << idx) & ticked) ? 0 : 2].draw_antialiased(
                                &g_transPopUpMaskSprites[0], g_pMenu, tx, y + 4, 0);
                        else {
                            if ((1 << idx) & ticked)
                                boxFrame = 1;
                            else
                                boxFrame = 2;
                            g_checkBoxFrames[boxFrame].Draw(g_pMenu, tx - 4, y, 1, 1, 1, 0);
                        }
                        if (((1 << idx) & ticked) != 0)
                            chosen = lineNo - firstOptLine;
                    }
                }
                g_printBuffer[i] = '\n';
                start = i + 1;
                y += (firstOptLine != -1) ? 24 : 18;
                lineNo++;
            }
        }
        numOpts = lineNo - firstOptLine;
        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(10, 0);
        ProcessMouseMessages();
        if (pickOne != 0) {
            if (PollInputState()) {
                switch (TakePendingKey()) {
                case 40:
                    keyNav = 1;
                    if (highlight < numOpts - 1)
                        highlight++;
                    break;
                case 38:
                    keyNav = 1;
                    if (highlight > 0)
                        highlight--;
                    break;
                case 13:
                case 32: flush_input(); return highlight;
                case 27: flush_input(); return -1;
                }
            }
            ticked = 1 << highlight;
            if (g_mouseButton != 0) {
                if ((disabled & ticked) == 0)
                    break;
                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                continue;
            }
        }
        g_simScreen.get_mouse_pos(&mx, &my);
        if (mx > cx - halfW && mx < halfW + cx - 48 && my >= firstOptY && my < firstOptY + numOpts * 24) {
            keyNav = 0;
            highlight = (my - firstOptY) / 24;
        } else if (keyNav == 0) {
            highlight = -1;
        }
        if (g_mouseButton != 1)
            continue;
        if (g_clickX > cx - halfW && g_clickX < halfW + cx - 48 && g_clickY >= firstOptY) {
            k = (g_clickY - firstOptY) / 24;
            if (k < numOpts) {
                if (pickOne != 0)
                    ticked = 1 << k;
                else
                    ticked = ticked ^ (1 << k);
            }
        }
        if (abs(g_clickX - halfW - cx + 20) < 20 && abs(g_clickY - boxH - top + 20) < 20)
            return pickOne ? chosen : ticked;
    }
    if (top & 1) {
        color = PackRgb555(r, g, b);
        g_simScreen.fill(cx - boxW / 2, top, boxW, boxH, color);
    }
    return highlight;
}

int StartTournament()
{
    int savedLevel = g_level;
    if (!ShowLoadGameScreen(1, 1))
        return 0;
    g_level = savedLevel;
    g_gameCashHundreds = START_MONEY;
    g_clock = 0x2c00;
    g_runModeFlags = GAME_CHAMPIONSHIP;
    if (!ShowPickAProScreen()) {
        g_runModeFlags = 0;
        return 0;
    }
    InitTournamentGolfers();
    g_gameCashHundreds = START_MONEY;
    return 1;
}
