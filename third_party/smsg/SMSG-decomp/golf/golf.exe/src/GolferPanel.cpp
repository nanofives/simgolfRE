#include "jgldefs.h"
#include "RandomSource.h"
#include "GolferPanel.h"
#include "StringPool.h"
#include "Golfer.h"
#include "SpriteAssets.h"
#include "FLCPalette.h"
#include "RenderQueue.h"
#include "Plot.h"
#include "FloatingText.h"
#include "Scorecard.h"
#include "ScreenText.h"
#include "Screen.h"
#include "TextWrap.h"
#include "Geometry.h"
#include "GolferStats.h"
#include "LoadingScreen.h"
#include "GolferDialogue.h"
#include "SetupScreens.h"
#include "Toolbar.h"
#include "HoleCompletion.h"
#include "ShadeTable.h"

RandomSource g_rng;



double RandomSource::get()
{
    m_seed = m_seed * 1103515245 + 12345;
    return ((m_seed >> 16) & 0x7fff) / 32768.0;
}

unsigned short RandomSource::NextInt(int n)
{
    return (unsigned short)((n & 0xffff) * get());
}



int RenderGolferPortrait(int golfer, int x, int y, int mode)
{
    short t = g_golferPool[golfer].typeIdx;
    int style = (g_golferTypeArray[g_golferPool[golfer].typeIdx].appearance >> 4) & 3;
    int head = g_golferTypeArray[g_golferPool[golfer].typeIdx].head;

    if (mode == 0)
        g_pMenu->fill(x + 4, y, 0x50, 0x60, (int)0x80007fff);
    SetupGolferAppearance(golfer);

    int by = y + 0x4c;

    if (mode != 1) {
        AssetSurface *p = g_pMenu;
        if (head < 20) {
            int n = (head + IsGolferMale(golfer) * 72) * 3 + GetGolferMoodIndex(golfer);
            SpriteObject *s = &g_playerHeadBmpArray[n];
            return s->Draw(p, x, y, 1, 1, 1, 0);
        }
        int m = (head + IsGolferMale(golfer) * 72) * 3 + GetGolferMoodIndex(golfer);
        SpriteObject *s = &g_playerHeadBmpArray[m];
        return s->Draw(p, x, y, 1, 1, 1, 0);
    }

    if (g_golferTypeArray[t].fixedKind & TYPE_JUNIOR)
        g_juniorsBarrelArt.BlitScaled(g_pMenu, x + 0x10, by, 0);

    SetupGolferAppearance(golfer);

    int s = (g_golferTypeArray[t].fixedKind & TYPE_JUNIOR) ? style + 4 : style;

    g_gameGolferBodyGfx[s + IsGolferMale(golfer) * 8].BlitScaled(g_pMenu, x + 0x13, by, g_palettes);

    AssetSurface *q = g_pMenu;
    if (head < 20) {
        int n = (head + IsGolferMale(golfer) * 20) * 3 + GetGolferMoodIndex(golfer);
        SpriteObject *e = &g_allExpressionHeads[n];
        return e->Draw(q, x, y - 20, 1, 1, 1, 0);
    }
    int n = (head + IsGolferMale(golfer) * 72) * 3 + GetGolferMoodIndex(golfer);
    SpriteObject *e = &g_playerHeadBmpArray[n];
    return e->Draw(q, x - 20, y - 20, 1, 1, 1, 0);
}


int GetGolferMoodBalance(int golfer)
{
    int n = 0;
    int i;

    for (i = 0; i < 5; i++) {
        if ((g_golferPool[golfer].slotParams[i] & MOOD_MASK) == MOOD_PLEASED)
            n++;
        if ((g_golferPool[golfer].slotParams[i] & MOOD_MASK) == MOOD_MASK)
            n--;
    }
    return n;
}

void ReplaceGolferNameTokens(int golfer)
{
    char saved[1024];
    char myName[64];
    char partnerName[64];

    strcpy(saved, g_printBuffer);
    g_printBuffer[0] = 0;
    AppendGolferName(golfer, 0);
    strcpy(myName, g_printBuffer);
    g_printBuffer[0] = 0;
    AppendGolferName(golfer ^ 1, 0);
    strcpy(partnerName, g_printBuffer);
    strcpy(g_printBuffer, saved);
    StringReplace("MYNAME", myName);
    StringReplace("PARTNER", partnerName);
}

void RenderGolferInfoPanel(int shot, int row)
{
    int i, j, n, talents, lines, v, other, side, t, saved, total, x, y, limitY, colY;
    int curX, curY, twoPlayer, hole;
    char **cp;
    unsigned color;
    short mask;
    signed char sc, par;
    int hcode, len;
    Hole *hp;
    unsigned short *wp;
    char *sep;

    twoPlayer = 0;
    if (g_golferPool[shot].persona) {
        g_printBuffer[0] = '\0';
        AppendGolferName(shot, 0);
        RenderSkillPointsScreen(g_printBuffer, g_golferPool[shot].skillMask, -1, shot, -0x32);
    }
    if (g_golferPool[shot].storyId != -1 && g_bgEnabled)
        twoPlayer = 1;
    if (!twoPlayer) {
        g_toolPanelBgImages[11].BlitScaled(g_pMenu, 0xec, 0x1a, g_colorLut, 0);
        g_toolPanelBgImages[9].draw_antialiased(&g_toolbarPanelBackings_A[9], g_pMenu, 0xec, 0x1a, 0);
    } else {
        g_toolPanelBgImages[9].BlitScaled(g_pMenu, 0xec, 0x1a, 0);
        g_toolPanelBgImages[10].draw_antialiased(&g_toolbarPanelBackings_A[10], g_pMenu, 0xec, 0xa5, 0);
    }
    g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].draw_antialiased(&g_transPopupCellsAlpha[PANEL_PORTRAIT_FRAME], g_pMenu, 0xac,
        -2, 0);
    RenderGolferPortrait(shot, 0xac, -2, -1);
    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);

    g_printBuffer[0] = '\0';
    AppendGolferName(shot, 0);
    strcat(g_printBuffer, " (");
    strcat(g_printBuffer, g_golferTypeArray[g_golferPool[shot].typeIdx].title);
    strcat(g_printBuffer, ")");
    DrawTextCenteredWithShadow(g_printBuffer, 0x1a4, 0x28, 0x80000210);
    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

    mask = g_golferPool[shot].ageFlags;
    g_printBuffer[0] = '\0';
    if (mask & 8)
        strcpy(g_printBuffer, "Single");
    if (mask & 0x10)
        strcpy(g_printBuffer, "Married");
    if (mask & 0x20)
        strcpy(g_printBuffer, "Divorced");
    if (mask & 0x40)
        strcpy(g_printBuffer, "Widowed");
    strcat(g_printBuffer, ", age ");
    strcat(g_printBuffer, itoa(GolferAge(shot), g_numBuf, 10));
    DrawTextCenteredAtY(g_printBuffer, 0x1a4, 0x3e, 0x80000210);

    v = g_golferTypeArray[g_golferPool[shot].typeIdx].caps;
    n = 0;
    g_printBuffer[0] = '\0';
    i = 0;
    for (cp = g_traitNames; (int)cp < (int)(g_traitNames + 5); cp++) {
        if (v & (1 << i)) {
            v -= (1 << i);
            if (n++) {
                sep = v ? ", " : " and ";
                strcat(g_printBuffer, sep);
            }
            strcat(g_printBuffer, *cp);
        }
        i++;
    }
    DrawTextCenteredAtY(g_printBuffer, 0x1a4, 0x4b, 0x80000210);

    strcpy(g_printBuffer, "'");
    AppendPartnerNote(g_golferPool[shot].enjoyment, 0);
    strcat(g_printBuffer, "'");
    DrawTextCenteredWithShadow(g_printBuffer, 0x1a4, 0x5b, 0x80004210);

    g_printBuffer[0] = '\0';
    i = 0;
    talents = g_golferPool[shot].talents;
    for (; i < 3; i++) {
        if (talents & (1 << i)) {
            if (g_printBuffer[0])
                strcat(g_printBuffer, ", ");
            strcat(g_printBuffer, g_talentNames[i]);
        }
    }
    if (g_golferPool[shot].talentLevel == 1)
        strcat(g_printBuffer, "+");
    if (g_golferPool[shot].talentLevel == 2)
        strcat(g_printBuffer, "++");
    DrawTextCenteredAtY(g_printBuffer, 0x1a4, 0x69, 0x80000210);

    g_simScreen.SetFontsForStyles(&g_boldArialFont, 0, 0, 0);

    DrawTextCenteredAtY("Fun", 0x24a, 0x2c, 0x80000000);
    v = Clamp((8 - g_golferPool[shot].enjoyment) * 10, 0, 0x50);
    g_simScreen.fill(0x222, 0x36, 0x50, 4, v <= 0x28 ? 0x800023e8 : 0x80007d08);
    g_simScreen.fill(0x272 - v, 0x36, v, 4, v <= 0x28 ? 0x80001284 : 0x80006000);

    v = Clamp((4 - GetGolferMoodBalance(shot)) * 10, 0, 0x50);
    DrawTextCenteredAtY("Attitude", 0x24a, 0x3c, 0x80000000);
    g_simScreen.fill(0x222, 0x46, 0x50, 4, v <= 0x28 ? 0x800023e8 : 0x80007d08);
    g_simScreen.fill(0x272 - v, 0x46, v, 4, v <= 0x28 ? 0x80001284 : 0x80006000);

    DrawTextCenteredAtY("Energy", 0x24a, 0x4c, 0x80000000);
    v = Clamp(g_golferPool[shot].fatigue / 4, 0, 0x50);
    g_simScreen.fill(0x222, 0x56, 0x50, 4, v <= 0x28 ? 0x800023e8 : 0x80007d08);
    g_simScreen.fill(0x272 - v, 0x56, v, 4, v <= 0x28 ? 0x80001284 : 0x80006000);

    DrawTextCenteredAtY("Hunger", 0x24a, 0x5c, 0x80000000);
    v = Clamp(g_golferPool[shot].hunger * 5 / 2, 0, 0x50);
    g_simScreen.fill(0x222, 0x66, 0x50, 4, v <= 0x28 ? 0x800023e8 : 0x80007d08);
    g_simScreen.fill(0x272 - v, 0x66, v, 4, v <= 0x28 ? 0x80001284 : 0x80006000);

    DrawTextCenteredAtY("Thirst", 0x24a, 0x6c, 0x80000000);
    v = Clamp(g_golferPool[shot].thirst * 5 / 2, 0, 0x50);
    g_simScreen.fill(0x222, 0x76, 0x50, 4, v <= 0x28 ? 0x800023e8 : 0x80007d08);
    g_simScreen.fill(0x272 - v, 0x76, v, 4, v <= 0x28 ? 0x80001284 : 0x80006000);

    g_printBuffer[0] = '\0';
    j = 0;
    for (cp = g_clubLetters; (int)cp < (int)(g_clubLetters + 14); cp++) {
        strcat(g_printBuffer, (g_golferPool[shot].clubsUsed & (1 << j)) ? *cp : ".");
        j++;
    }

    if (g_golferPool[shot].holeIdx == -1)
        return;
    strcpy(g_printBuffer, g_ordinals[g_golferPool[shot].holeIdx]);
    strcat(g_printBuffer, " hole,");
    strcat(g_printBuffer, g_ordinals[g_golferPool[shot].holeStrokes]);
    strcat(g_printBuffer, " shot.");
    if (g_golferPool[shot].holeIdx < CLUBHOUSE_HOLE)
        g_golferPool[shot].holeEnjoyment[g_golferPool[shot].holeIdx] = (unsigned char)g_golferPool[shot].enjoyment;

    total = 0;
    hole = 1;
    hp = &g_allHolesArray[1];
    x = 0x10a;
    do {
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        g_printBuffer[0] = '\0';
        strcat(g_printBuffer, itoa(hole, g_numBuf, 10));
        DrawTextCenteredAtY(g_printBuffer, x, 0x84, 0x80000000);
        if ((g_golferPool[shot].holeScore[hole] != 0 || g_golferPool[shot].holeIdx == hole)
            && g_golferPool[shot].holeScore[hole] != 0) {
            sc = g_golferPool[shot].holeScore[hole];
            par = hp->par;
            color = 0x80000000;
            if (sc < par)
                color = 0x80007d08;
            if (sc < par - 1)
                color = 0x80007ff0;
            if (sc > par)
                color = 0x8000211f;
            if (sc > par + 1)
                color = 0x80004010;
            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(sc, g_numBuf, 10));
            g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
            DrawTextCenteredAtY(g_printBuffer, x, 0x94, color);
            total += g_golferPool[shot].holeScore[hole];
        }
        if (g_golferPool[shot].holeIdx == hole) {
            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(g_golferPool[shot].holeStrokes, g_numBuf, 10));
            g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
            DrawTextCenteredAtY(g_printBuffer, x, 0x94, COLOR_GREY);
            total += g_golferPool[shot].holeStrokes;
        }
        hole++;
        hp++;
        x += 0x12;
    } while (x < 0x24e);

    g_pMenu->SetFontsForStyles(&g_dialogFont, 0, 0, 0);
    g_printBuffer[0] = '\0';
    strcat(g_printBuffer, itoa(total, g_numBuf, 10));
    DrawTextCenteredAtY(g_printBuffer, 0x256, 0x94, 0x80000000);
    saved = g_hot;
    g_hot = -1;
    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);

    if (twoPlayer) {
        limitY = 0x118;
        other = shot ^ 1;
        for (;;) {
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
            g_printBuffer[0] = '\0';
            AppendThemeText(g_golferPool[shot].storyId, -1, 0, 0);
            g_printBuffer[0] = '\'';
            strcat(g_printBuffer, "' with ");
            AppendGolferName(shot, 0);
            strcat(g_printBuffer, " and ");
            AppendGolferName(other, 0);
            g_simScreen.line(0x148, 0xaf, 0x210, 0xaf, COLOR_TEAL);
            DrawTextCenteredAtY(g_printBuffer, 0x1ac, 0xa7, 0x800003ff);

            y = 0xb3;
            row = 1;
            if (g_golferPool[shot].holeIdx >= 1) {
                x = 0xf8;
                do {
                    if (g_golferPool[shot].holeResult[row]) {
                        side = (g_golferPool[shot].flags & GOLFER_STORY_LEAD) ? other : shot;
                        t = side ^ 1;
                        g_printBuffer[0] = '\0';
                        if (GetStoryChapterLine(g_golferPool[shot].storyId, row, t)) {
                            ReplaceGolferNameTokens(t);
                            DrawTextWithColor(g_printBuffer, x, y, shot == side ? 0x80004210 : 0x80007c1f);
                            y += 9;
                        }
                        g_printBuffer[0] = '\0';
                        if (GetStoryChapterLine(g_golferPool[shot].storyId, row, side)) {
                            ReplaceGolferNameTokens(side);
                            DrawTextWithColor(g_printBuffer, x, y, shot == side ? 0x80007c1f : 0x80004210);
                            y += 10;
                        }
                    }
                    row++;
                    x += 10;
                } while (row <= g_golferPool[shot].holeIdx);
            }
            if (y <= limitY)
                break;
            limitY += 0x13;
        }

        g_simScreen.get_mouse_pos(&curY, &curX);
        if (ApproxDistance(0x126 - curY, 0x11f - curX) < 0x19)
            g_hot = 0;
        if (ApproxDistance(0x167 - curY, 0x11f - curX) < 0x19)
            g_hot = INFO_BUTTON_NEXT_CHAPTER;
        if (ApproxDistance(0x1a8 - curY, 0x11f - curX) < 0x19)
            g_hot = INFO_BUTTON_SNAPSHOT;
        if (ApproxDistance(0x1e9 - curY, 0x11f - curX) < 0x19)
            g_hot = INFO_BUTTON_STORY;
        if (ApproxDistance(0x221 - curY, 0x11f - curX) < 0x19)
            g_hot = INFO_BUTTON_CLOSE;
        if (g_golferPool[shot].storyChapter == STORY_LAST_CHAPTER
            && g_golferPool[g_golferPool[shot].partner].storyChapter == STORY_LAST_CHAPTER
            && g_hot == INFO_BUTTON_NEXT_CHAPTER)
            g_hot = -1;
        t = (g_hot == 0);
        g_gameGolferStatsBtnSprs[t].draw_antialiased(&g_golferStatsBarCellsMask[t], g_pMenu, 0x10d, 0x10e, 0);
        if (!(g_golferPool[shot].storyChapter == STORY_LAST_CHAPTER
                && g_golferPool[g_golferPool[shot].partner].storyChapter == STORY_LAST_CHAPTER)) {
            t = (g_hot == INFO_BUTTON_NEXT_CHAPTER);
            g_gameGolferStatsBtnSprs[0x12 + t].draw_antialiased(&g_golferStatsBarCellsMask[0x12 + t], g_pMenu, 0x14e,
                0x110, 0);
        }
        t = (g_hot == INFO_BUTTON_SNAPSHOT);
        g_gameGolferStatsBtnSprs[0x18 + t].draw_antialiased(&g_golferStatsBarCellsMask[0x18 + t], g_pMenu, 0x18f, 0x110,
            0);
        t = (g_hot == INFO_BUTTON_STORY);
        g_gameGolferStatsBtnSprs[0x15 + t].draw_antialiased(&g_golferStatsBarCellsMask[0x15 + t], g_pMenu, 0x1d0, 0x110,
            0);
        t = (g_hot == INFO_BUTTON_CLOSE);
        g_gameGolferStatsBtnSprs[0x1b + t].draw_antialiased(&g_golferStatsBarCellsMask[0x1b + t], g_pMenu, 0x20e, 0x110,
            0);
        g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].draw_antialiased(&g_transPopupCellsAlpha[PANEL_PORTRAIT_FRAME], g_pMenu,
            0x228, 0xb0, 0);
        RenderGolferPortrait(other, 0x228, 0xb0, -1);
        if (g_hot == saved) {
            g_printBuffer[0] = '\0';
            if (++g_infoPanelHover > 6) {
                g_printBuffer[0] = '\0';
                switch (g_hot) {
                case 0: strcpy(g_printBuffer, "Customize"); break;
                case 1: strcpy(g_printBuffer, "Move/Eject Golfer"); break;
                case INFO_BUTTON_SNAPSHOT: strcpy(g_printBuffer, "Take Snapshot"); break;
                case INFO_BUTTON_STORY: strcpy(g_printBuffer, "Golfer Comments"); break;
                case INFO_BUTTON_NEXT_CHAPTER: strcpy(g_printBuffer, "Next Chapter"); break;
                }
                RenderToolbarTooltip(curY, curX);
            }
            return;
        }
        g_infoPanelHover = 0;
        return;
    }

    g_simScreen.get_mouse_pos(&curY, &curX);
    if (ApproxDistance(0x13e - curY, 0x106 - curX) < 0x19)
        g_hot = 0;
    if (ApproxDistance(0x17f - curY, 0x106 - curX) < 0x19)
        g_hot = 1;
    if (ApproxDistance(0x1c0 - curY, 0x106 - curX) < 0x19)
        g_hot = INFO_BUTTON_SNAPSHOT;
    if (ApproxDistance(0x201 - curY, 0x106 - curX) < 0x19)
        g_hot = INFO_BUTTON_STORY;
    if (ApproxDistance(0x253 - curY, 0x106 - curX) < 0x19)
        g_hot = INFO_BUTTON_CLOSE;
    t = (g_hot == 0);
    g_gameGolferStatsBtnSprs[t].draw_antialiased(&g_golferStatsBarCellsMask[t], g_pMenu, 0x125, 0xed, 0);
    t = (g_hot == 1);
    g_gameGolferStatsBtnSprs[6 + t].draw_antialiased(&g_golferStatsBarCellsMask[6 + t], g_pMenu, 0x166, 0xed, 0);
    t = (g_hot == INFO_BUTTON_SNAPSHOT);
    g_gameGolferStatsBtnSprs[9 + t].draw_antialiased(&g_golferStatsBarCellsMask[9 + t], g_pMenu, 0x1a7, 0xed, 0);
    if (g_golferPool[shot].storyId != -1) {
        t = (g_hot == INFO_BUTTON_STORY);
        g_gameGolferStatsBtnSprs[0xc + t].draw_antialiased(&g_golferStatsBarCellsMask[0xc + t], g_pMenu, 0x1e8, 0xed,
            0);
    }
    t = (g_hot == INFO_BUTTON_CLOSE);
    g_gameGolferStatsBtnSprs[0xf + t].draw_antialiased(&g_golferStatsBarCellsMask[0xf + t], g_pMenu, 0x23a, 0xed, 0);
    if (g_hot == saved) {
        if (++g_infoPanelHover > 6) {
            if (g_hot != -1) {
                g_printBuffer[0] = '\0';
                switch (g_hot) {
                case 0: strcpy(g_printBuffer, "Customize"); break;
                case 1: strcpy(g_printBuffer, "Move/Eject Golfer"); break;
                case INFO_BUTTON_SNAPSHOT: strcpy(g_printBuffer, "Take Snapshot"); break;
                case INFO_BUTTON_STORY: strcpy(g_printBuffer, "View Story"); break;
                case INFO_BUTTON_NEXT_CHAPTER: strcpy(g_printBuffer, "Next Chapter"); break;
                }
                RenderToolbarTooltip(curY, curX);
            }
        }
    } else {
        g_infoPanelHover = 0;
    }

    lines = 0;
    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
    colY = 0xaa;
    for (i = row ? 9 : 4; i >= 0; i--) {
        if (g_golferPool[shot].slotKind[i] == 0)
            continue;
        hcode = g_golferPool[shot].slotStamp[i];
        strcpy(g_printBuffer, g_ordinals[hcode / 11]);
        strcat(g_printBuffer, " hole, ");
        if (g_golferPool[shot].slotKind[i] != 0x13) {
            strcat(g_printBuffer, g_ordinals[hcode % 11]);
            strcat(g_printBuffer, " shot- ");
        }
        len = strlen(g_printBuffer);
        wp = &g_golferPool[shot].slotParams[i];
        BuildGolferDialogue(g_golferPool[shot].slotKind[i] & 0xff, *wp & 0x3fff, hcode, shot);
        if (g_gameSpeakerIsPartner) {
            g_printBuffer[len] = '\0';
            strcat(g_printBuffer, "\"");
            BuildGolferDialogue(g_golferPool[shot].slotKind[i], *wp & 0x3fff, g_golferPool[shot].slotStamp[i], shot);
            strcat(g_printBuffer, "\"");
        }
        color = 0x80006318;
        if ((*wp & 0xc000) == 0x4000)
            color = 0x800023e8;
        if ((*wp & 0xc000) == 0xc000)
            color = 0x80007d08;
        if (g_golferPool[shot].slotKind[i] == LINE_STORY)
            color = 0x80007c1f;
        if (row && color != 0x80007d08)
            continue;
        if (g_pMenu->text_width(g_printBuffer) > 0x180) {
            if (g_golferPool[shot].slotKind[i] == LINE_STORY) {
                g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
                DrawTextClampedToScreen(g_printBuffer, 0xf8, colY, color);
            } else {
                DrawTextClampedToScreen(g_printBuffer + 0x12, 0xf8, colY, color);
            }
        } else {
            DrawTextClampedToScreen(g_printBuffer, 0xf8, colY, color);
        }
        colY += 0xd;
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        if (++lines >= 5)
            return;
    }
}


char *g_statLabels[] = {"Drive", "Long", "FWay", "GIR", "Putts"};
