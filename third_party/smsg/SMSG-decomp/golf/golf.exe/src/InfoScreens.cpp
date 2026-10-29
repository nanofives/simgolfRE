#include "jgldefs.h"

#include "WorldMap.h"
#include "InfoScreens.h"
#include "CourseReportScreen.h"
#include "SceneryNames.h"
#include "ScreenText.h"
#include "MainLoop.h"
#include "MapTile.h"
#include "CourseOverlay.h"
#include "TractPurchase.h"
#include "HireEmployee.h"
#include "FlicBanks.h"
#include "SpriteAssets.h"
#include "SetupScreens.h"
#include "GolferPairing.h"
#include "ImageSet.h"
#include "Screen.h"
#include "InputPoll.h"
#include "Text.h"
#include "GolferStats.h"


Font g_infoTitleFont, g_font20, g_font18, g_font16, g_font14;

SpriteObject g_okButtonStates[5];
SpriteObject g_fadeScreen;
SpritePair g_financialReport;
SpritePair g_sgaReport;
SpritePair g_graphReport;
SpritePair g_hireScreen;
ReportPanels g_holeByHolePanels;
ReportPanels g_commentsSheet;
ReportPanels g_scoreRecords;
CourseReport g_courseOverview;
MemberRoster g_memberRoster;
RouteScreens g_routeReport;
PairScreen g_pairingScreen;
TournamentResult g_tournamentResult;

extern const int g_courseColumnMarkX[12] = {112, 157, 190, 233, 283, 330, 375, 422, 468, 571, 641, 716};
static const int s_reportCellW[12] = {33, 31, 41, 48, 45, 43, 45, 43, 101, 68, 68, 75};
static const int s_reportRowY[3] = {205, 241, 275};
static const int s_routeCellX[12] = {0, 130, 261, 396, 0, 130, 261, 396, 536, 713, 732, 751};
static const int s_routeCellY[12] = {0, 0, 0, 0, 114, 114, 118, 118, 0, 1, 1, 0};
static const int s_routeCellW[12] = {129, 130, 134, 139, 129, 130, 134, 139, 65, 18, 18, 18};
static const int s_routeCellH[12] = {113, 113, 117, 117, 113, 113, 117, 117, 65, 37, 37, 178};

void ShowCreditsScreen()
{
    SpriteObject bg;
    SpriteObject logo;
    AssetSurface img;
    char *lines[512];
    unsigned long start;
    int step;
    int n;
    int i;
    char running;

    n = 0;
    running = 1;
    img.Create(0x320, 0x258, 0, 1, 0, 0);
    img.load_pcx("creditsbckgrd.pcx", 0, 0, PALETTE_COLORS, 1);
    bg.extractPriv(&img, 0, 0, 0x320, 0x258, 1, 0);
    img.load_pcx("bink64.pcx", 0, 0, PALETTE_COLORS, 1);
    logo.extractPriv(&img, 0, 0, img.GetSurfaceWidth(), img.GetSurfaceHeight(), 1, 1);
    img.close();

    if (text_open("CREDITS.txt", "CREDITS") == 0) {
        if (*text_get() != '#') {
            do {
                char *t = (char *)malloc(strlen(text_buffer) + 1);
                lines[n] = t;
                if (t) {
                    n++;
                    strcpy(t, text_buffer);
                }
            } while (*text_get() != '#');
        }
        text_close();
        g_simScreen.SetFontsForStyles(&g_infoTitleFont, 0, 0, 0);
        g_simScreen.set_text_color(0x80007fff, -1, 2, 2);
        start = timeGetTime();
        if (g_infoTitleFont.m_extraLeading >= 0)
            step = g_infoTitleFont.m_capHeight + g_infoTitleFont.m_extraLeading;
        else
            step = g_infoTitleFont.m_lineSpacing;
        flush_input();
        if (n != 0) {
            do {
                int y = 600 - (int)((timeGetTime() - start) / 30);
                bg.BlitScaled(&g_simScreen, 0, 0, 0);
                i = 0;
                if (-y > step * n + 0x96)
                    running = 0;
                while (y < 600) {
                    char *s;
                    if (i >= n)
                        break;
                    s = lines[i];
                    if (*s != 0 && strcmp(s, "$bink") == 0)
                        logo.BlitSpriteWithOffset(&g_simScreen, (0x320 - logo.m_imageWidth) / 2, y, 0);
                    else if (s)
                        g_simScreen.write_l(s, 0x32, y, (int)strlen(s));
                    y += step;
                    i++;
                    ProcessMouseMessages();
                }
                if (g_mouseButton != 0 || PollInputState() != 0)
                    running = 0;
                g_simScreen.RenderFullSurface(0);
                WaitWithTimeout(1, 0);
            } while (running);
            {
                char **q = &lines[n];
                i = n;
                do {
                    char *t = *--q;
                    if (t)
                        free(t);
                } while (--i);
            }
        }
    }
    bg.close();
    logo.close();
}

void LoadInfoScreenAssets()
{
    AssetSurface im;
    int i, j, xa, xb;
    SpriteObject *sp;

    g_infoTitleFont.CreateFace("Klepto ITC TT", 0x18, 0);
    g_font20.CreateFace("Manual SSi", 0x14, 0);
    g_font18.CreateFace("Manual SSi", 0x12, 0);
    g_font16.CreateFace("Manual SSi", 0x10, 0);
    g_font14.CreateFace("Manual SSi", 0xe, 0);
    im.Create(0x320, 0x258, 0, 1, 0, 0);
    im.load_pcx("interface\\infoscreens\\black.pcx", 0, 0, PALETTE_COLORS, 1);
    g_fadeScreen.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 1);
    im.load_pcx("interface\\infoscreens\\okstates.pcx", 0, 0, PALETTE_COLORS, 1);
    for (i = 0; i < 5; i++)
        g_okButtonStates[i].extractPriv(&im, (i % 3) * 45 + 1, i < 4 ? 1 : 44, 44, 42, 1, 1);
    im.load_pcx("interface\\infoscreens\\FINANCEreport.pcx", 0, 0, PALETTE_COLORS, 1);
    g_financialReport.image.extractPriv(&im, 8, 7, 0x310, 0x121, 1, 0);
    im.load_pcx("interface\\infoscreens\\FINANCEreport_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_financialReport.alpha.extractPriv(&im, 8, 7, 0x310, 0x121, 1, 0);
    im.load_pcx("interface\\infoscreens\\coursereport.pcx", 0, 0, PALETTE_COLORS, 1);
    g_courseOverview.header.extractPriv(&im, 0, 0, 0x320, 0x68, 1, 0);
    g_courseOverview.holeRow.extractPriv(&im, 0, 0x81, 0x320, 0x11, 1, 0);
    g_courseOverview.totalRow.extractPriv(&im, 0, 0x1a4, 0x320, 0x1d, 1, 0);
    g_courseOverview.footer.extractPriv(&im, 0, 0x1d4, 0x320, 0x49, 1, 0);
    for (i = 0; i < 5; i++) {
        if (i < 3) {
            for (j = 0; j < 12; j++)
                g_courseOverview.cells[j][i].extractPriv(&im, g_courseColumnMarkX[j], s_reportRowY[i], s_reportCellW[j],
                    0xf, 1, 1);
            g_courseOverview.marks[i].extractPriv(&im, 0x223, i * 0xe + 0x225, 0xb, 0xd, 1, 1);
        }
        g_courseOverview.marks[3 + i].extractPriv(&im, i * 0xf + 0xc2, 0x22d, 0xe, 0xf, 1, 1);
    }
    im.load_pcx("interface\\infoscreens\\coursereport_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_courseOverview.headerAlpha.extractPriv(&im, 0, 0, 0x320, 0x68, 1, 0);
    g_courseOverview.holeRowAlpha.extractPriv(&im, 0, 0x81, 0x320, 0x11, 1, 0);
    g_courseOverview.totalRowAlpha.extractPriv(&im, 0, 0x1a4, 0x320, 0x1d, 1, 0);
    g_courseOverview.footerAlpha.extractPriv(&im, 0, 0x1d4, 0x320, 0x49, 1, 0);
    im.load_pcx("interface\\infoscreens\\SGA.pcx", 0, 0, PALETTE_COLORS, 1);
    g_sgaReport.image.extractPriv(&im, 0x23, 0x1a, 0x2da, 0x1a3, 1, 0);
    im.load_pcx("interface\\infoscreens\\SGAreport_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_sgaReport.alpha.extractPriv(&im, 0x23, 0x1a, 0x2da, 0x1a3, 1, 0);
    im.load_pcx("interface\\infoscreens\\HoleSTAT.pcx", 0, 0, PALETTE_COLORS, 1);
    g_holeByHolePanels.top.extractPriv(&im, 0, 0, 0x320, 0xdc, 1, 0);
    g_holeByHolePanels.middle.extractPriv(&im, 0, 0x11a, 0x320, 0x10, 1, 0);
    g_holeByHolePanels.bottom.extractPriv(&im, 0, 0x15b, 0x320, 0x37, 1, 0);
    im.load_pcx("interface\\infoscreens\\HoleSTAT_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_holeByHolePanels.topAlpha.extractPriv(&im, 0, 0, 0x320, 0xdc, 1, 0);
    g_holeByHolePanels.middleAlpha.extractPriv(&im, 0, 0x11a, 0x320, 0x10, 1, 0);
    g_holeByHolePanels.bottomAlpha.extractPriv(&im, 0, 0x15b, 0x320, 0x37, 1, 0);
    im.load_pcx("interface\\infoscreens\\PlayComt.pcx", 0, 0, PALETTE_COLORS, 1);
    g_commentsSheet.top.extractPriv(&im, 0x94, 0x2d, 0x1f9, 0x66, 1, 0);
    g_commentsSheet.middle.extractPriv(&im, 0x94, 0xe0, 0x1f9, 0x11, 1, 0);
    g_commentsSheet.bottom.extractPriv(&im, 0x94, 0x141, 0x1f9, 0x3d, 1, 0);
    im.load_pcx("interface\\infoscreens\\PlayComt_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_commentsSheet.topAlpha.extractPriv(&im, 0x94, 0x2d, 0x1f9, 0x66, 1, 0);
    g_commentsSheet.middleAlpha.extractPriv(&im, 0x94, 0xe0, 0x1f9, 0x11, 1, 0);
    g_commentsSheet.bottomAlpha.extractPriv(&im, 0x94, 0x141, 0x1f9, 0x3d, 1, 0);
    im.load_pcx("interface\\infoscreens\\memberRoster.pcx", 0, 0, PALETTE_COLORS, 1);
    g_memberRoster.image.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\infoscreens\\memberRoster_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_memberRoster.alpha.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\infoscreens\\memberRoster_buttons.pcx", 0, 0, PALETTE_COLORS, 1);
    for (i = 0, sp = g_memberRoster.buttons, xb = -76, xa = -62; xa < 261; xa += 19, i++, xb += 20, sp++) {
        if (xa < 166)
            sp->extractPriv(&im, (i >> 1) * 20 + 46, (i & 1) ? 1 : 20, 0x13, 0x12, 1, 1);
        else if (xa < 204)
            sp->extractPriv(&im, xa, 1, 0x12, 0x25, 1, 1);
        else
            sp->extractPriv(&im, xb, 1, 0x13, 0x12, 1, 1);
    }
    im.load_pcx("interface\\infoscreens\\memberRoster_scrollbar.pcx", 0, 0, PALETTE_COLORS, 1);
    g_memberRoster.buttons[17].extractPriv(&im, 0, 0, im.GetSurfaceWidth(), im.GetSurfaceHeight(), 1, 1);
    im.load_pcx("interface\\infoscreens\\lowscore.pcx", 0, 0, PALETTE_COLORS, 1);
    g_scoreRecords.top.extractPriv(&im, 0xc3, 0x2d, 0x19b, 0x4f, 1, 0);
    g_scoreRecords.middle.extractPriv(&im, 0xc3, 0xe0, 0x19b, 0x11, 1, 0);
    g_scoreRecords.bottom.extractPriv(&im, 0xc3, 0x141, 0x19b, 0x3d, 1, 0);
    im.load_pcx("interface\\infoscreens\\lowscore_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_scoreRecords.topAlpha.extractPriv(&im, 0xc3, 0x2d, 0x19b, 0x4f, 1, 0);
    g_scoreRecords.middleAlpha.extractPriv(&im, 0xc3, 0xe0, 0x19b, 0x11, 1, 0);
    g_scoreRecords.bottomAlpha.extractPriv(&im, 0xc3, 0x141, 0x19b, 0x3d, 1, 0);
    im.load_pcx("interface\\infoscreens\\histograph.pcx", 0, 0, PALETTE_COLORS, 1);
    g_graphReport.image.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\infoscreens\\histograph_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_graphReport.alpha.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\infoscreens\\route screens_course.pcx", 0, 0, PALETTE_COLORS, 1);
    g_routeReport.tabs[0].extractPriv(&im, 0, 0, 0x320, im.GetSurfaceHeight(), 1, 0);
    im.load_pcx("interface\\infoscreens\\route screens_employ.pcx", 0, 0, PALETTE_COLORS, 1);
    g_routeReport.tabs[1].extractPriv(&im, 0, 0, 0x320, im.GetSurfaceHeight(), 1, 0);
    im.load_pcx("interface\\infoscreens\\route screens_aura.pcx", 0, 0, PALETTE_COLORS, 1);
    g_routeReport.tabs[2].extractPriv(&im, 0, 0, 0x320, im.GetSurfaceHeight(), 1, 0);
    im.load_pcx("interface\\infoscreens\\route screens_value.pcx", 0, 0, PALETTE_COLORS, 1);
    g_routeReport.tabs[3].extractPriv(&im, 0, 0, 0x320, im.GetSurfaceHeight(), 1, 0);
    im.load_pcx("interface\\infoscreens\\route screens_bottom.pcx", 0, 0, PALETTE_COLORS, 1);
    g_routeReport.bottom.extractPriv(&im, 0, 0xfd, 0x320, 0x15b, 1, 0);
    for (i = 0; i < 12; i++)
        g_routeReport.cells[i].extractPriv(&im, s_routeCellX[i], s_routeCellY[i], s_routeCellW[i], s_routeCellH[i], 1,
            0);
    im.load_pcx("interface\\infoscreens\\buy_land.pcx", 0, 0, PALETTE_COLORS, 1);
    g_routeReport.buyLand.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\infoscreens\\buy_land_buttons.pcx", 0, 0, PALETTE_COLORS, 1);
    for (i = 0; i < 21; i++) {
        if (i < 18)
            g_routeReport.buyLandButtons[i].extractPriv(&im, (i % 9) * 60 + 1, (i / 9) * 63 + 1, 0x3b, 0x3d, 1, 0);
        else if (i == 18)
            g_routeReport.buyLandButtons[i].extractPriv(&im, 1, 0x7f, 0xc0, 0x5f, 1, 0);
        else if (i == 19)
            g_routeReport.buyLandButtons[i].extractPriv(&im, 0xc2, 0x7f, 0x40, 0x40, 1, 0);
        else
            g_routeReport.buyLandButtons[i].extractPriv(&im, 0x103, 0x7f, 0x48, 0x3d, 1, 0);
    }
    im.load_pcx("interface\\infoscreens\\hire.pcx", 0, 0, PALETTE_COLORS, 1);
    g_hireScreen.image.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\infoscreens\\hire_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_hireScreen.alpha.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\PairBase.pcx", 0, 0, PALETTE_COLORS, 1);
    g_pairingScreen.base.extractPriv(&im, 0, 0, 0x320, 0x258, 1, 0);
    im.load_pcx("interface\\PairButtons.pcx", 0, 0, PALETTE_COLORS, 1);
    g_pairingScreen.buttons[0].extractPriv(&im, 0, 0, 0x149, 0x88, 1, 0);
    g_pairingScreen.buttons[1].extractPriv(&im, 0, 0x88, 0x149, 0x88, 1, 0);
    g_pairingScreen.buttons[2].extractPriv(&im, 0, 0x110, 0x149, 0x88, 1, 0);
    g_pairingScreen.buttons[3].extractPriv(&im, 0x2b5, 0x1f6, 0x4b, 0x4b, 1, 0);
    im.load_pcx("interface\\infoscreens\\tournament result.pcx", 0, 0, PALETTE_COLORS, 1);
    g_tournamentResult.header.extractPriv(&im, 0, 0, 0x320, 0x6a, 1, 1);
    g_tournamentResult.prizeRow.extractPriv(&im, 0, 0xb4, 0x320, 0x1a, 1, 1);
    g_tournamentResult.cutRow.extractPriv(&im, 0, 0x7d, 0x320, 0x16, 1, 1);
    g_tournamentResult.unpaidRow.extractPriv(&im, 0, 0xe0, 0x320, 0x12, 1, 1);
    g_tournamentResult.prizeFooter.extractPriv(&im, 0, 0x165, 0x320, 0x33, 1, 1);
    g_tournamentResult.unpaidFooter.extractPriv(&im, 0, 0x112, 0x320, 0x33, 1, 1);
    im.load_pcx("interface\\infoscreens\\tournament result_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    g_tournamentResult.headerAlpha.extractPriv(&im, 0, 0, 0x320, 0x6a, 1, 1);
    g_tournamentResult.prizeRowAlpha.extractPriv(&im, 0, 0xb4, 0x320, 0x1a, 1, 1);
    g_tournamentResult.cutRowAlpha.extractPriv(&im, 0, 0x7d, 0x320, 0x16, 1, 1);
    g_tournamentResult.unpaidRowAlpha.extractPriv(&im, 0, 0xe0, 0x320, 0x12, 1, 1);
    g_tournamentResult.prizeFooterAlpha.extractPriv(&im, 0, 0x165, 0x320, 0x33, 1, 1);
    g_tournamentResult.unpaidFooterAlpha.extractPriv(&im, 0, 0x112, 0x320, 0x33, 1, 1);
    im.close();
}

void CleanupAllFontsAndSprites()
{
    g_font14.close();
    g_font16.close();
    g_infoTitleFont.close();
    g_fadeScreen.close();

    for (int i = 0; i < 3; i++)
        g_okButtonStates[i].close();

    g_financialReport.image.close();
    g_financialReport.alpha.close();
    g_courseOverview.header.close();
    g_courseOverview.holeRow.close();
    g_courseOverview.totalRow.close();
    g_courseOverview.footer.close();

    for (int j = 0; j < 5; j++) {
        if (j < 3) {
            for (int k = 0; k < 12; k++)
                g_courseOverview.cells[k][j].close();
            g_courseOverview.marks[j].close();
        }
        g_courseOverview.marks[3 + j].close();
    }

    g_courseOverview.headerAlpha.close();
    g_courseOverview.holeRowAlpha.close();
    g_courseOverview.totalRowAlpha.close();
    g_courseOverview.footerAlpha.close();
    g_sgaReport.image.close();
    g_sgaReport.alpha.close();
    g_holeByHolePanels.top.close();
    g_holeByHolePanels.middle.close();
    g_holeByHolePanels.bottom.close();
    g_holeByHolePanels.topAlpha.close();
    g_holeByHolePanels.middleAlpha.close();
    g_holeByHolePanels.bottomAlpha.close();
    g_commentsSheet.top.close();
    g_commentsSheet.middle.close();
    g_commentsSheet.bottom.close();
    g_commentsSheet.topAlpha.close();
    g_commentsSheet.middleAlpha.close();
    g_commentsSheet.bottomAlpha.close();
    g_memberRoster.image.close();
    g_memberRoster.alpha.close();

    for (int m = 0; m < 18; m++)
        g_memberRoster.buttons[m].close();

    g_scoreRecords.top.close();
    g_scoreRecords.middle.close();
    g_scoreRecords.bottom.close();
    g_scoreRecords.topAlpha.close();
    g_scoreRecords.middleAlpha.close();
    g_scoreRecords.bottomAlpha.close();
    g_graphReport.image.close();
    g_graphReport.alpha.close();
    g_routeReport.buyLand.close();

    for (int n = 0; n < 21; n++) {
        g_routeReport.buyLandButtons[n].close();
        if (n < 4)
            g_routeReport.tabs[n].close();
        if (n < 12)
            g_routeReport.cells[n].close();
    }

    g_routeReport.buyLand.close();
    g_routeReport.bottom.close();
    g_hireScreen.image.close();
    g_hireScreen.alpha.close();
    g_pairingScreen.base.close();
    g_pairingScreen.buttons[0].close();
    g_pairingScreen.buttons[1].close();
    g_pairingScreen.buttons[2].close();
    g_pairingScreen.buttons[3].close();
    g_tournamentResult.header.close();
    g_tournamentResult.prizeRow.close();
    g_tournamentResult.cutRow.close();
    g_tournamentResult.unpaidRow.close();
    g_tournamentResult.prizeFooter.close();
    g_tournamentResult.unpaidFooter.close();
    g_tournamentResult.headerAlpha.close();
    g_tournamentResult.prizeRowAlpha.close();
    g_tournamentResult.cutRowAlpha.close();
    g_tournamentResult.unpaidRowAlpha.close();
    g_tournamentResult.prizeFooterAlpha.close();
    g_tournamentResult.unpaidFooterAlpha.close();
}


#define DrawCentered(f, s, x, y) do { if (s) g_simScreen.DrawTextCenteredWithFont(f, s, x, y, strlen(s)); } while (0)
#define DrawLeft(f, s, x, y) do { if (s) g_simScreen.DrawTextLeftAlignedWithFont(f, s, x, y, strlen(s)); } while (0)
#define DrawRight(f, s, x, y) do { if (s) g_simScreen.DrawTextRightAlignedWithFont(f, s, x, y, strlen(s)); } while (0)

inline int TrendColour(short last, short now)
{
    return last < now ? 0x80001284 : (last == now ? 0x80000000 : 0x80007d08);
}

void RenderEndOfYearReport()
{
    AssetSurface img;
    SpriteObject panel, panelAlpha, band, bandAlpha, line, lineAlpha;
    int cur, prev, t, y, i, mx, my;
    int savedColour;
    char hot, running;

    img.Create(0x320, 0x258, 0, 1, 0, 0);
    img.load_pcx("interface\\infoscreens\\endoyear.pcx", 0, 0, PALETTE_COLORS, 1);
    panel.extractPriv(&img, 0xbb, 0x28, 0x1ad, 0xb0, 1, 0);
    band.extractPriv(&img, 0xbb, 0x176, 0x1ad, 0x37, 1, 0);
    line.extractPriv(&img, 0xbb, 0x123, 0x1ad, 0x0f, 1, 0);
    img.load_pcx("interface\\infoscreens\\endoyear_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    panelAlpha.extractPriv(&img, 0xbb, 0x28, 0x1ad, 0xb0, 1, 0);
    bandAlpha.extractPriv(&img, 0xbb, 0x176, 0x1ad, 0x37, 1, 0);
    lineAlpha.extractPriv(&img, 0xbb, 0x123, 0x1ad, 0x0f, 1, 0);
    img.close();
    ConfigureAndPlaySound(0x7f, SOUND_FULL_VOLUME, 0, 0, 0);
    savedColour = g_simScreen.m_textColors[0][0];
    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    panel.draw_antialiased(&panelAlpha, &g_simScreen, 0xbb, 0x28, 0);

    t = g_clock / 1024;
    cur = t % 500;
    prev = (t - 8) % 500;
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    strcpy(g_printBuffer, "END of  YEAR:  ");
    strcat(g_printBuffer, itoa(g_careerPlayYear + 2000, g_numString, 10));
    DrawCentered(&g_infoTitleFont, g_printBuffer, 0x196, 0x37);
    DrawCentered(&g_font14, "This Year", 0x1d1, 0x5d);
    DrawCentered(&g_font14, "Last Year", 0x236, 0x5d);
    DrawCentered(&g_font14, "Highlights", 0x194, 0xc8);

    strcpy(g_printBuffer, "Cash reserves have ");
    strcat(g_printBuffer, g_moneyLine[prev] < g_moneyLine[cur]
                              ? "increased"
                              : (g_moneyLine[prev] == g_moneyLine[cur] ? "not changed" : "decreased"));
    DrawCentered(&g_font14, g_printBuffer, 0x12d, 0x73);
    g_printBuffer[0] = 0;
    FormatNumberWithCommas(g_moneyLine[prev] * 100);
    DrawRight(&g_font14, g_printBuffer, 0x259, 0x73);
    g_printBuffer[0] = 0;
    FormatNumberWithCommas(g_moneyLine[cur] * 100);
    g_simScreen.set_text_color(TrendColour(g_moneyLine[prev], g_moneyLine[cur]), -1, 2, 2);
    DrawRight(&g_font14, g_printBuffer, 0x1f4, 0x73);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);

    strcpy(g_printBuffer, "Your fun rating has ");
    strcat(g_printBuffer,
        g_funRatingHistPoints[prev] < g_funRatingHistPoints[cur]
            ? "increased"
            : (g_funRatingHistPoints[prev] == g_funRatingHistPoints[cur] ? "not changed" : "decreased"));
    DrawCentered(&g_font14, g_printBuffer, 0x12d, 0x87);
    g_printBuffer[0] = 0;
    strcat(g_printBuffer, itoa(g_funRatingHistPoints[prev], g_numString, 10));
    DrawRight(&g_font14, g_printBuffer, 0x259, 0x87);
    g_printBuffer[0] = 0;
    strcat(g_printBuffer, itoa(g_funRatingHistPoints[cur], g_numString, 10));
    g_simScreen.set_text_color(TrendColour(g_funRatingHistPoints[prev], g_funRatingHistPoints[cur]), -1, 2, 2);
    DrawRight(&g_font14, g_printBuffer, 0x1f4, 0x87);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);

    strcpy(g_printBuffer, "Your skill rating has ");
    strcat(g_printBuffer, g_monthlySkillScore[prev] < g_monthlySkillScore[cur]
                              ? "increased"
                              : (g_monthlySkillScore[prev] == g_monthlySkillScore[cur] ? "not changed" : "decreased"));
    DrawCentered(&g_font14, g_printBuffer, 0x12d, 0x9b);
    g_printBuffer[0] = 0;
    AppendMoney(g_monthlySkillScore[prev]);
    DrawRight(&g_font14, g_printBuffer, 0x259, 0x9b);
    g_printBuffer[0] = 0;
    AppendMoney(g_monthlySkillScore[cur]);
    g_simScreen.set_text_color(TrendColour(g_monthlySkillScore[prev], g_monthlySkillScore[cur]), -1, 2, 2);
    DrawRight(&g_font14, g_printBuffer, 0x1f4, 0x9b);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);

    strcpy(g_printBuffer, "Your membership has ");
    g_gameMembershipTrendLine[cur] = g_clubMemberSum;
    strcat(g_printBuffer,
        g_gameMembershipTrendLine[prev] < g_gameMembershipTrendLine[cur]
            ? "increased"
            : (g_gameMembershipTrendLine[prev] == g_gameMembershipTrendLine[cur] ? "not changed" : "decreased"));
    DrawCentered(&g_font14, g_printBuffer, 0x12d, 0xaf);
    g_printBuffer[0] = 0;
    strcat(g_printBuffer, itoa(g_gameMembershipTrendLine[prev], g_numString, 10));
    DrawRight(&g_font14, g_printBuffer, 0x259, 0xaf);
    g_printBuffer[0] = 0;
    strcat(g_printBuffer, itoa(g_gameMembershipTrendLine[cur], g_numString, 10));
    g_simScreen.set_text_color(TrendColour(g_gameMembershipTrendLine[prev], g_gameMembershipTrendLine[cur]), -1, 2, 2);
    DrawRight(&g_font14, g_printBuffer, 0x1f4, 0xaf);

    y = 0x28 + panel.m_height;
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    for (i = prev; i <= cur; i++) {
        int code, sub;
        if (g_monthEventKindHistory[i] == 0)
            continue;
        strcpy(g_printBuffer, kMonth[Clamp(i - prev, 0, 7)]);
        strcat(g_printBuffer, ": ");
        code = g_monthEventKindHistory[i];
        sub = code & 0x1f;
        switch (code & ~0x1f) {
        case 0x20:
            strcat(g_printBuffer, "Hole ");
            strcat(g_printBuffer, itoa(sub, g_numString, 10));
            strcat(g_printBuffer, " opened.");
            if (g_allHolesArray[sub].par == 0) {
                g_printBuffer[0] = 0;
                continue;
            }
            break;
        case 0x40:
            strcat(g_printBuffer, g_facilityTypes[sub].name);
            strcat(g_printBuffer, " built.");
            break;
        case 0x80:
            strcat(g_printBuffer, "Hole ");
            strcat(g_printBuffer, itoa(sub, g_numString, 10));
            strcat(g_printBuffer, " rated top 100.");
            break;
        case 0xa0:
            strcat(g_printBuffer, "Hole ");
            strcat(g_printBuffer, itoa(sub, g_numString, 10));
            strcat(g_printBuffer, " rated top 18!");
            break;
        case 0x60:
            strcat(g_printBuffer, "Won match vs. ");
            strcat(g_printBuffer, g_golfProList[sub].name);
            break;
        case 0xc0:
            strcat(g_printBuffer, g_themeCelebsEntries[sub].name);
            strcat(g_printBuffer, " buys a home.");
            break;
        case 0xe0:
            strcat(g_printBuffer, g_golferTypeArray[0].name);
            strcat(g_printBuffer, " places ");
            strcat(g_printBuffer, itoa(sub, g_numString, 10));
            strcat(g_printBuffer, " in tournament.");
            break;
        case 0x100:
            strcat(g_printBuffer, g_ceoName);
            strcat(g_printBuffer, " joins the board.");
            break;
        case 0x120:
            strcat(g_printBuffer, "Happy Ending:");
            AppendThemeText(sub, -1, 0, 0);
            break;
        case 0x160:
            strcat(g_printBuffer, "Ivana donates a ");
            AppendLandmarkName(sub, 0);
            break;
        case 0x140: strcat(g_printBuffer, "Additional land purchased."); break;
        }
        if (g_printBuffer[0]) {
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
        }
    }

    if (g_gameCashHundreds < 0 && !(g_runModeFlags & GAME_SANDBOX)) {
        g_simScreen.set_text_color(0x80007d08, -1, 2, 2);
        switch (g_moneyWarnCnt++) {
        case 0:
            strcpy(g_printBuffer, "The board is concerned about our negative cash situation.");
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
            strcpy(g_printBuffer, "You have two years to return to positive cash.");
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
            break;
        case 1:
            strcpy(g_printBuffer, "The board is very worried about our lingering debt.");
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
            strcpy(g_printBuffer, "You have one more year to get out of debt.");
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
            break;
        case 2:
            strcpy(g_printBuffer, "You have been unable to make a profit on this course.");
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
            strcpy(g_printBuffer, "Regrettably, the board has terminated your contract.");
            line.draw_antialiased(&lineAlpha, &g_simScreen, 0xbb, y, 0);
            DrawCentered(&g_font14, g_printBuffer, 0x194, y + 2);
            y += line.m_height;
            break;
        }
    } else {
        g_moneyWarnCnt = 0;
    }

    band.draw_antialiased(&bandAlpha, &g_simScreen, 0xbb, y, 0);
    g_simScreen.set_text_color(savedColour, -1, 2, 2);
    y += 8;
    running = 1;
    hot = 0;
    g_okButtonStates[0].BlitScaled(&g_simScreen, 0x226, y, 0);
    g_simScreen.RenderFullSurface(0);
    WaitWithTimeout(0x3c, 0);
    flush_input();
    do {
        WaitWithTimeout(1, 0);
        ProcessMouseMessages();
        g_simScreen.get_mouse_pos(&mx, &my);
        g_simScreen.RenderFullSurface(0);
        if (g_mouseButton == 1)
            running = 0;
        if (mx >= 0x226 && mx < 0x252 && my >= y && my < y + 0x2c) {
            if (!hot) {
                hot = 1;
                g_okButtonStates[1].BlitScaled(&g_simScreen, 0x226, y, 0);
                g_simScreen.RenderFullSurface(0);
            }
        } else if (hot) {
            hot = 0;
            g_okButtonStates[0].BlitScaled(&g_simScreen, 0x226, y, 0);
            g_simScreen.RenderFullSurface(0);
        }
    } while (!PollInputState() && running);
    WaitForInputRelease();
    if (HighScore_InsertEntry() != -1)
        RenderHighScoreScreen();
    FadeOutSound(0x7f);
    panel.close();
    band.close();
    line.close();
    panelAlpha.close();
    bandAlpha.close();
    lineAlpha.close();
}

void RenderKeyboardShortcutsScreen()
{
    AssetSurface img;
    SpriteObject panel, panelAlpha;
    int mx, my;
    int format;
    char hot, running;

    img.Create(0x320, 0x258, 0, 1, 0, 0);
    img.load_pcx("interface\\infoscreens\\shortcuts.pcx", 0, 0, PALETTE_COLORS, 1);
    panel.extractPriv(&img, 0, 0, 0x320, 0x199, 1, 0);
    img.load_pcx("interface\\infoscreens\\shortcuts_alpha.pcx", 0, 0, PALETTE_COLORS, 1);
    panelAlpha.extractPriv(&img, 0, 0, 0x320, 0x199, 1, 0);
    img.close();
    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    panel.draw_antialiased(&panelAlpha, &g_simScreen, 0, 0, 0);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    DrawCentered(&g_infoTitleFont, "KEYBOARD SHORTCUTS", 0x11f, 0x1c);
    DrawCentered(&g_font14, "F1", 0x44, 0x51);
    DrawLeft(&g_font14, "Course status report.", 0x6c, 0x51);
    DrawCentered(&g_font14, "F2", 0x44, 0x62);
    DrawLeft(&g_font14, "Player comments report.", 0x6c, 0x62);
    DrawCentered(&g_font14, "F3", 0x44, 0x73);
    DrawLeft(&g_font14, "Histograph.", 0x6c, 0x73);
    DrawCentered(&g_font14, "F4", 0x44, 0x84);
    DrawLeft(&g_font14, "Financial Report.", 0x6c, 0x84);
    DrawCentered(&g_font14, "F5", 0x44, 0x95);
    DrawLeft(&g_font14, "Course Overview Map.", 0x6c, 0x95);
    DrawCentered(&g_font14, "F6", 0x44, 0xa6);
    DrawLeft(&g_font14, "World Map.", 0x6c, 0xa6);
    DrawCentered(&g_font14, "F7", 0x44, 0xb7);
    DrawLeft(&g_font14, "SGA Evaluation.", 0x6c, 0xb7);
    DrawCentered(&g_font14, "F8", 0x44, 0xc8);
    format = graphsys->screen_mask();
    g_simScreen.set_text_color(format == 1 ? 0x80000400 : 0x80000200, -1, 2, 2);
    DrawLeft(&g_font14, "Handy keyboard commands screen.", 0x6c, 0xc8);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    DrawCentered(&g_font14, "F9", 0x44, 0xd9);
    DrawLeft(&g_font14, "Membership Roster.", 0x6c, 0xd9);
    DrawCentered(&g_font14, "F10", 0x44, 0xea);
    DrawLeft(&g_font14, "Professional Accomplishments.", 0x6c, 0xea);
    DrawCentered(&g_font14, "?", 0x44, 0xfb);
    DrawLeft(&g_font14, "Repeat last message.", 0x6c, 0xfb);
    DrawCentered(&g_font14, "shift+b", 0x44, 0x10c);
    DrawLeft(&g_font14, "Sell a building lot.", 0x6c, 0x10c);
    DrawCentered(&g_font14, "shift+r", 0x44, 0x11d);
    DrawLeft(&g_font14, "Routing, Aura, and lot values", 0x6c, 0x11d);
    DrawCentered(&g_font14, "shift+w", 0x44, 0x12e);
    DrawLeft(&g_font14, "World map, change courses", 0x6c, 0x12e);
    DrawCentered(&g_font14, "Tab", 0x44, 0x13f);
    DrawLeft(&g_font14, "Rotate buildings/trees before placing.", 0x6c, 0x13f);
    DrawCentered(&g_font14, "Esc", 0x44, 0x157);
    DrawLeft(&g_font14, "Quit the game.", 0x6c, 0x157);
    DrawCentered(&g_font14, "g", 0x1bc, 0x51);
    DrawLeft(&g_font14, "Select GREEN/TEES tool.", 0x1e3, 0x51);
    DrawCentered(&g_font14, "f", 0x1bc, 0x62);
    DrawLeft(&g_font14, "Select FAIRWAY tool.", 0x1e3, 0x62);
    DrawCentered(&g_font14, "r", 0x1bc, 0x73);
    DrawLeft(&g_font14, "Select ROUGH tool.", 0x1e3, 0x73);
    DrawCentered(&g_font14, "s", 0x1bc, 0x84);
    DrawLeft(&g_font14, "Select SAND TRAP tool.", 0x1e3, 0x84);
    DrawCentered(&g_font14, "t", 0x1bc, 0x95);
    DrawLeft(&g_font14, "Select TREES tool.", 0x1e3, 0x95);
    DrawCentered(&g_font14, "w", 0x1bc, 0xa6);
    DrawLeft(&g_font14, "Select WATER tool.", 0x1e3, 0xa6);
    DrawCentered(&g_font14, "p", 0x1bc, 0xb7);
    DrawLeft(&g_font14, "Select PATH tool.", 0x1e3, 0xb7);
    DrawCentered(&g_font14, "b", 0x1bc, 0xc8);
    DrawLeft(&g_font14, "Select BENCHES tool.", 0x1e3, 0xc8);
    DrawCentered(&g_font14, "z", 0x1bc, 0xd9);
    DrawLeft(&g_font14, "Zoom the view.", 0x1e3, 0xd9);
    DrawCentered(&g_font14, "x", 0x1bc, 0xea);
    DrawLeft(&g_font14, "Unzoom the view.", 0x1e3, 0xea);
    DrawCentered(&g_font14, "shift+p", 0x1bc, 0xfb);
    DrawLeft(&g_font14, "Pause/Unpause the game.", 0x1e3, 0xfb);
    DrawCentered(&g_font14, "shift+t", 0x1bc, 0x10c);
    DrawLeft(&g_font14, "Turn trees off/on.", 0x1e3, 0x10c);
    DrawCentered(&g_font14, "shift+n", 0x1bc, 0x11d);
    DrawLeft(&g_font14, "Toggle names display.", 0x1e3, 0x11d);
    DrawCentered(&g_font14, "e", 0x1bc, 0x12e);
    DrawLeft(&g_font14, "Toggle elevation mode.", 0x1e3, 0x12e);
    DrawCentered(&g_font14, "/", 0x1bc, 0x13f);
    DrawLeft(&g_font14, "Instant shot analysis.", 0x1e3, 0x13f);

    hot = 0;
    running = 1;
    g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2c6, 0x16a, 0);
    g_simScreen.RenderFullSurface(0);
    WaitWithTimeout(0x3c, 0);
    flush_input();
    do {
        WaitWithTimeout(1, 0);
        ProcessMouseMessages();
        g_simScreen.get_mouse_pos(&mx, &my);
        g_simScreen.RenderFullSurface(0);
        if (mx >= 0x2c6 && mx < 0x2f2 && my >= 0x16a && my < 0x196) {
            if (!hot) {
                hot = 1;
                g_okButtonStates[1].BlitScaled(&g_simScreen, 0x2c6, 0x16a, 0);
                g_simScreen.RenderFullSurface(0);
            }
            if (g_mouseButton == 1)
                running = 0;
        } else if (hot) {
            hot = 0;
            g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2c6, 0x16a, 0);
            g_simScreen.RenderFullSurface(0);
        }
    } while (!PollInputState() && running);
    g_reportScreenId = 0;
    WaitForInputRelease();
    panel.close();
    panelAlpha.close();
}

void RenderFinancialReportScreen()
{
    int mx, my;
    int hot;
    char running;
    int i;
    int nYears, firstYear, yearIdx, j;

    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    g_financialReport.image.draw_antialiased(&g_financialReport.alpha, &g_simScreen, 8, 7, 0);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    DrawLeft(&g_infoTitleFont, "FINANCIAL REPORT", 0xda, 0x15);
    for (i = 0; i <= 8; i++)
        DrawCentered(&g_font14, g_financeRowLabels[i], 0x62, 0x57 + i * 0x11 + (i == 8 ? 7 : 0));
    nYears = Clamp(g_careerPlayYear, 0, 99);
    firstYear = Clamp(nYears - 8, 0, 999);
    for (yearIdx = 0; yearIdx <= nYears; yearIdx++) {
        if (yearIdx >= firstYear) {
            YearLedger *rec = &g_annualStatsRecords[yearIdx];
            int total = 0;
            int x = 0xde + (yearIdx - firstYear) * 0x4b;
            int y;
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(yearIdx + 2001, g_numString, 10));
            g_simScreen.set_text_color(0x80000000, -1, 2, 2);
            DrawCentered(&g_font14, g_printBuffer, x, 0x3c);
            x += 0x1e;
            y = 0x56;
            for (j = 0; j <= 8; j++) {
                g_printBuffer[0] = 0;
                if (j <= 7) {
                    FormatNumberWithCommas(rec->v[j] * 100);
                    if (rec->v[j] == 0)
                        strcpy(g_printBuffer, g_empty);
                    total += rec->v[j];
                } else {
                    FormatNumberWithCommas(total * 100);
                    rec->v[j] = (short)total;
                }
                g_simScreen.set_text_color(rec->v[j] >= 0 ? 0x80000000 : 0x80006000, -1, 2, 2);
                DrawRight(&g_font14, g_printBuffer, x + 4, y);
                y += (j == 7) ? 0x18 : 0x11;
            }
        }
    }

    hot = 0;
    g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2d6, 0xf9, 0);
    g_simScreen.RenderFullSurface(0);
    WaitWithTimeout(10, 0);
    flush_input();
    running = 1;
    do {
        WaitWithTimeout(1, 0);
        g_simScreen.get_mouse_pos(&mx, &my);
        if (mx >= 0x2d6 && mx < 0x302 && my >= 0xf9 && my < 0x125) {
            if (!hot) {
                hot = 1;
                g_okButtonStates[1].BlitScaled(&g_simScreen, 0x2d6, 0xf9, 0);
                g_simScreen.RenderFullSurface(0);
            }
        } else if (hot) {
            hot = 0;
            g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2d6, 0xf9, 0);
            g_simScreen.RenderFullSurface(0);
        }
        ProcessMouseMessages();
        if (PollInputState())
            running = 0;
    } while (!g_mouseButton && running);
    g_reportScreenId = 0;
    WaitForInputRelease();
}

int GetHoleCountTier(int n)
{
    if (n <= 5)
        return 0;
    if (n <= 9)
        return 1;
    if (n <= 17)
        return 2;
    return (n <= 18) ? 3 : -1;
}



inline void SetTextBlack()
{
    g_simScreen.set_text_color((int)0x80000000, -1, 2, 2);
}
inline void SetTextRed()
{
    g_simScreen.set_text_color((int)0x80007d08, -1, 2, 2);
}

inline void DrawStarRow(int stars, int y, int &grade)
{
    for (int i = 0; i < stars; i++)
        g_bubblePieces[17].BlitScaled(&g_simScreen, 0x12f + i * 0xe, y, 0);
    grade += stars;
    if (!stars) {
        SetTextRed();
        DrawCentered(&g_font14, "- not acceptable -", 0x182, y);
        grade = -999;
        SetTextBlack();
    }
}

void RenderHoleInfoOverlay(int mode)
{
    int wsum, holeType, minVal, nHoles, k, hole, rowY, par, time, minWhich, wwsum, bar, nShort;
    int dLen;
    int nDiff[3];
    int totals[13];
    int mouseY, mouseX;
    int cnt[9], sum[9];
    int dAcc, dImg, fun;

    bar = g_level ? 50 : 25;
    for (int z = 0; z < 13; z++)
        totals[z] = 0;
    for (int i = 0; i < 3; i++)
        nDiff[i] = 0;

    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);

    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    SetTextBlack();
    if (mode)
        g_sgaReport.image.draw_antialiased(&g_sgaReport.alpha, &g_simScreen, 0x23, 0x1a, 0);
    else {
        g_courseOverview.header.draw_antialiased(&g_courseOverview.headerAlpha, &g_simScreen, 0, 0, 0);
        rowY = g_courseOverview.header.m_height;
    }
    SetTextBlack();

    nHoles = 0;
    nShort = 0;

    for (hole = 1; hole < 19; hole++) {
        if (g_allHolesArray[hole].par) {
            par = g_allHolesArray[hole].par;
            wsum = 0;
            for (int f = 0; f < 8; f++)
                sum[f] = par * 8;
            wwsum = 0;
            for (int g = 0; g < 8; g++)
                cnt[g] = 8;

            for (k = 1; k <= 7; k++) {
                int tot = 0;
                for (int band = 0; band < 8; band++) {
                    for (int j = k; j <= (k == 7 ? 9 : k); j++) {
                        int v = g_allHolesArray[hole].dist[band][j];
                        tot += v;
                        wsum += v;
                        wwsum += v * j;
                        cnt[band] += v;
                        sum[band] += v * j;
                    }
                }
            }

            nHoles++;
            if (g_allHolesArray[hole].cmt[22] / 2 + g_allHolesArray[hole].cmt[28] + g_allHolesArray[hole].cmt[11] >= 8)
                nShort++;

            dImg = 0;
            dAcc = 0;
            dLen = 0;
            minWhich = 0;
            holeType = 0;
            minVal = 0x64;
            totals[0] += g_allHolesArray[hole].yards;
            totals[1] += par;
            totals[2] += wsum ? (wwsum * 100) / wsum : 0;
            time =
                g_allHolesArray[hole].rounds ? (g_allHolesArray[hole].playTime / g_allHolesArray[hole].rounds) / 40 : 0;
            totals[3] += time;
            fun = g_allHolesArray[hole].rounds
                      ? (g_allHolesArray[hole].moodSum * 100)
                            / (g_allHolesArray[hole].shots / 2 + g_allHolesArray[hole].rounds + 4)
                      : 0;
            totals[4] += fun;

            if (cnt[7]) {
                if (cnt[6]) {
                    dLen = sum[6] * 100 / cnt[6] - sum[7] * 100 / cnt[7];
                    if (g_runModeFlags & GAME_SECOND_TEES)
                        dLen += sum[0] * 100 / cnt[0] - sum[1] * 100 / cnt[1];
                    totals[5] += dLen;
                    if (dLen >= bar)
                        holeType = 1;
                    if (dLen < minVal) {
                        minVal = dLen;
                        minWhich = 1;
                    }
                    if (dLen >= bar)
                        nDiff[0]++;
                }
                if (cnt[5]) {
                    dAcc = sum[5] * 100 / cnt[5] - sum[7] * 100 / cnt[7];
                    if (g_runModeFlags & GAME_SECOND_TEES)
                        dAcc += sum[0] * 100 / cnt[0] - sum[2] * 100 / cnt[2];
                    totals[6] += dAcc;
                    if (dAcc >= bar)
                        holeType |= 2;
                    if (dAcc < minVal) {
                        minVal = dAcc;
                        minWhich = 2;
                    }
                    if (dAcc >= bar)
                        nDiff[1]++;
                }
                if (cnt[3]) {
                    dImg = sum[3] * 100 / cnt[3] - sum[7] * 100 / cnt[7];
                    if (g_runModeFlags & GAME_SECOND_TEES)
                        dImg += sum[0] * 100 / cnt[0] - sum[4] * 100 / cnt[4];
                    totals[7] += dImg;
                    if (dImg >= bar)
                        holeType |= 4;
                    if (dImg < minVal) {
                        minVal = dImg;
                        minWhich = 4;
                    }
                    if (dImg >= bar)
                        nDiff[2]++;
                }
            }
            if (hole >= 1 && g_allHolesArray[hole].similar < 2)
                totals[8]++;
            totals[9] += wsum ? (g_allHolesArray[hole].revenue * 100) / wsum : 0;
            totals[10] += g_allHolesArray[hole].revenue;
            totals[11] += g_allHolesArray[hole].revenue - g_allHolesArray[hole].upkeep - g_allHolesArray[hole].netCost;

            if (!mode) {
                g_courseOverview.holeRow.draw_antialiased(&g_courseOverview.holeRowAlpha, &g_simScreen, 0, rowY, 0);
                rowY += 2;
                SetTextBlack();
                g_printBuffer[0] = 0;
                AppendHoleName(hole);
                {
                    int t = g_allHolesArray[hole].flags;
                    int ic = ((t & HOLE_TOP_100) ? ((t & HOLE_TOP_18) ? 2 : 1) : 0)
                             + ((g_allHolesArray[hole].cmt[22] / 2 + g_allHolesArray[hole].cmt[28]
                                        + g_allHolesArray[hole].cmt[11]
                                    >= 8)
                                     ? 3
                                     : 0)
                             + 2;
                    if (ic > 2)
                        g_courseOverview.marks[ic].BlitScaled(&g_simScreen, 0x5a, rowY - 2, 0);
                }
                DrawLeft(&g_font14, g_printBuffer, 0xf, rowY);

                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_allHolesArray[hole].yards, g_numString, 10));
                DrawRight(&g_font14, g_printBuffer, 0x97, rowY);

                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(g_allHolesArray[hole].par, g_numString, 10));
                DrawCentered(&g_font14, g_printBuffer, 0xac, rowY);

                g_printBuffer[0] = 0;
                AppendMoney(wsum ? (wwsum * 100) / wsum : 0);
                if (g_allHolesArray[hole].flags & 0xc)
                    g_courseOverview.cells[3][0].BlitScaled(&g_simScreen, g_courseColumnMarkX[3], rowY - 2, 0);
                DrawRight(&g_font14, g_printBuffer, 0xe3, rowY);

                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(time, g_numString, 10));
                strcat(g_printBuffer, "m");
                {
                    int k = (time >= g_allHolesArray[hole].par * 5)
                                ? 0
                                : ((time <= g_allHolesArray[hole].par * 3) ? 1 : -1);
                    if (k >= 0)
                        g_courseOverview.cells[3][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[3], rowY - 2, 0);
                }
                DrawRight(&g_font14, g_printBuffer, 0x116, rowY);

                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(fun, g_numString, 10));
                strcat(g_printBuffer, "%");
                {
                    int k = (fun < 10) ? 0 : ((fun < 50) ? -1 : 1);
                    if (k >= 0)
                        g_courseOverview.cells[4][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[4], rowY - 2, 0);
                }
                DrawRight(&g_font14, g_printBuffer, 0x144, rowY);

                g_printBuffer[0] = 0;
                AppendMoney(dLen);
                {
                    int k = (dLen < 50) ? ((dLen < 0) ? 0 : -1) : ((dLen < 100) ? 2 : 1);
                    if (k >= 0)
                        g_courseOverview.cells[5][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[5], rowY - 2, 0);
                }
                DrawRight(&g_font14, g_printBuffer, 0x171, rowY);

                g_printBuffer[0] = 0;
                AppendMoney(dAcc);
                {
                    int k = (dAcc < 50) ? ((dAcc < 0) ? 0 : -1) : ((dAcc < 100) ? 2 : 1);
                    if (k >= 0)
                        g_courseOverview.cells[6][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[6], rowY - 2, 0);
                }
                DrawRight(&g_font14, g_printBuffer, 0x19f, rowY);

                g_printBuffer[0] = 0;
                AppendMoney(dImg);
                {
                    int k = (dImg < 50) ? ((dImg < 0) ? 0 : -1) : ((dImg < 100) ? 2 : 1);
                    if (k >= 0)
                        g_courseOverview.cells[7][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[7], rowY - 2, 0);
                }
                DrawRight(&g_font14, g_printBuffer, 0x1ce, rowY);

                if (minVal < 0x64)
                    holeType &= ~minWhich;
                g_printBuffer[0] = 0;
                strcpy(g_printBuffer, g_holeTypeNames[holeType]);
                {
                    int k = (g_allHolesArray[hole].similar < 2) ? 1 : ((g_allHolesArray[hole].similar > 2) ? 0 : -1);
                    if (k >= 0)
                        g_courseOverview.cells[8][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[8], rowY - 2, 0);
                }
                DrawLeft(&g_font14, g_printBuffer, 0x1d7, rowY);

                {
                    AssetSurface *sc = &g_simScreen;
                    unsigned t = g_allHolesArray[hole].flags;
                    int k = (t & 0x60) ? ((t >> 4) & 2) : 1;
                    g_courseOverview.marks[k].BlitScaled(sc, 0x22b, rowY - 1, 0);
                }
                g_printBuffer[0] = 0;
                FormatNumberWithCommas(wsum ? (g_allHolesArray[hole].revenue * 100) / wsum : 0);
                DrawRight(&g_font14, g_printBuffer, 0x27b, rowY);

                g_printBuffer[0] = 0;
                FormatNumberWithCommas(g_allHolesArray[hole].revenue * 100);
                DrawRight(&g_font14, g_printBuffer, 0x2c5, rowY);

                g_printBuffer[0] = 0;
                FormatNumberWithCommas(
                    (g_allHolesArray[hole].revenue - g_allHolesArray[hole].upkeep - g_allHolesArray[hole].netCost)
                    * 100);
                if (g_allHolesArray[hole].revenue - g_allHolesArray[hole].upkeep - g_allHolesArray[hole].netCost < 0)
                    g_courseOverview.cells[11][0].BlitScaled(&g_simScreen, g_courseColumnMarkX[11], rowY - 2, 0);
                DrawRight(&g_font14, g_printBuffer, 0x312, rowY);

                rowY += 0xf;
            }
        }
        if (mode)
            continue;
        if (hole % 18)
            continue;
        if (!nHoles)
            continue;

        g_courseOverview.totalRow.draw_antialiased(&g_courseOverview.totalRowAlpha, &g_simScreen, 0, rowY - 2, 0);
        rowY += 9;
        g_printBuffer[0] = 0;
        strcpy(g_printBuffer, hole == 9 ? "Subtotal:" : "Total:");
        DrawLeft(&g_font14, g_printBuffer, 0xf, rowY);

        g_printBuffer[0] = 0;
        FormatNumberWithCommas(totals[0]);
        DrawRight(&g_font14, g_printBuffer, 0x97, rowY);

        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(totals[1], g_numString, 10));
        DrawCentered(&g_font14, g_printBuffer, 0xac, rowY);

        g_printBuffer[0] = 0;
        AppendMoney(totals[2]);
        DrawRight(&g_font14, g_printBuffer, 0xe3, rowY);

        g_printBuffer[0] = 0;
        {
            if (totals[3] >= 60) {
                strcat(g_printBuffer, itoa(totals[3] / 60, g_numString, 10));
                strcat(g_printBuffer, "h ");
            }
            strcat(g_printBuffer, itoa(time % 60, g_numString, 10));
            strcat(g_printBuffer, "m");
        }
        DrawRight(&g_font14, g_printBuffer, 0x116, rowY);

        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(totals[4] / nHoles, g_numString, 10));
        strcat(g_printBuffer, "%");
        DrawRight(&g_font14, g_printBuffer, 0x144, rowY);

        g_printBuffer[0] = 0;
        {
            int v = totals[5] / nHoles;
            AppendMoney(v);
            int k = (v < 50) ? ((v < 0) ? 0 : -1) : ((v < 100) ? 2 : 1);
            if (k > 0)
                g_courseOverview.cells[5][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[5], rowY - 2, 0);
        }
        DrawRight(&g_font14, g_printBuffer, 0x171, rowY);

        g_printBuffer[0] = 0;
        {
            int v = totals[6] / nHoles;
            AppendMoney(v);
            int k = (v < 50) ? ((v < 0) ? 0 : -1) : ((v < 100) ? 2 : 1);
            if (k > 0)
                g_courseOverview.cells[6][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[6], rowY - 2, 0);
        }
        DrawRight(&g_font14, g_printBuffer, 0x19f, rowY);

        g_printBuffer[0] = 0;
        {
            int v = totals[7] / nHoles;
            AppendMoney(v);
            int k = (v < 50) ? ((v < 0) ? 0x80007d08 : 0x80000000) : ((v >= 100) ? 0x800023e8 : 0x80001284);
            if (k > 0)
                g_courseOverview.cells[7][k].BlitScaled(&g_simScreen, g_courseColumnMarkX[7], rowY - 2, 0);
        }
        DrawRight(&g_font14, g_printBuffer, 0x1ce, rowY);

        strcpy(g_printBuffer, "\xa7");
        FormatNumberWithCommas(totals[9] / nHoles);
        DrawRight(&g_font14, g_printBuffer, 0x27b, rowY);

        strcpy(g_printBuffer, "\xa7");
        FormatNumberWithCommas(totals[10] * 100);
        DrawRight(&g_font14, g_printBuffer, 0x2c5, rowY);

        strcpy(g_printBuffer, "\xa7");
        {
            int v = totals[11];
            FormatNumberWithCommas(v * 100);
            if (v < 0)
                g_courseOverview.cells[11][0].BlitScaled(&g_simScreen, g_courseColumnMarkX[11], rowY - 2, 0);
        }
        DrawRight(&g_font14, g_printBuffer, 0x312, rowY);

        rowY += g_courseOverview.totalRow.m_height - 0xb;
    }

    int btnX;
    if (mode) {
        DrawLeft(&g_infoTitleFont, "REPORT of the SIM GOLF ASSOCIATION", 0xbe, 0x2a);

        int tier, hcA, hcB;
        int grade = 0;
        strcpy(g_printBuffer, "Selection Criteria: ");
        switch (GetHoleCountTier(g_nextOpenHoleIndex - 1)) {
        case 0:
            hcA = 5;
            hcB = 5;
            tier = 0;
            break;
        case 1:
            hcA = 9;
            hcB = 9;
            tier = 1;
            break;
        case 2:
            hcB = 17;
            hcA = g_nextOpenHoleIndex - 1;
            tier = 2;
            break;
        case 3:
            hcA = 0x12;
            hcB = 0x12;
            tier = 3;
            break;
        }
        strcat(g_printBuffer, g_courseTierNames[tier]);
        DrawLeft(&g_font14, g_printBuffer, 0x4b, 0x4d);

        strcpy(g_printBuffer, "Grade:");
        DrawLeft(&g_font14, g_printBuffer, 0x132, 0x4d);
        strcpy(g_printBuffer, "Ideal  (Minimum)");
        DrawLeft(&g_font14, g_printBuffer, 0x1f1, 0x4d);

        strcpy(g_printBuffer, "Length of Course");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0x68);
        g_printBuffer[0] = 0;
        FormatNumberWithCommas((hcB * 100 / 18) * (tier * 5 + 57));
        strcat(g_printBuffer, "+  (");
        int idealYds = (hcB * 100 / 18) * (tier * 5 + 57);
        if (hcB == 0x12)
            FormatNumberWithCommas(idealYds - 1000);
        else
            FormatNumberWithCommas(idealYds - (idealYds * 10) / (tier * 5 + 20));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0x68);

        strcpy(g_printBuffer, "Number of Holes");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0x79);
        g_printBuffer[0] = 0;

        strcat(g_printBuffer, itoa(hcB, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(hcB - 9 / (4 - tier), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0x79);

        strcpy(g_printBuffer, "Time to Play");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0x8a);
        strcpy(g_printBuffer, "4 hours or less  (max: 5 hrs)");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0x8a);

        strcpy(g_printBuffer, "Fun Factor");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0x9b);
        strcpy(g_printBuffer, "100%+");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0x9b);

        strcpy(g_printBuffer, "Holes with Variety");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0xac);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(hcA, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(Clamp(hcA - 9, 0, 0x63), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0xac);

        strcpy(g_printBuffer, "Scenic Holes");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0xbd);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(hcA, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(Clamp(hcA - 9, 0, 0x63), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0xbd);

        strcpy(g_printBuffer, "Length Holes");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0xce);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(hcA, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(Clamp(hcA - 9, 0, 0x63), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0xce);

        strcpy(g_printBuffer, "Accuracy Holes: ");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0xdf);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(hcA, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(Clamp(hcA - 9, 0, 0x63), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0xdf);

        strcpy(g_printBuffer, "Imagination Holes: ");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0xf0);
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(hcA, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(Clamp(hcA - 9, 0, 0x63), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0xf0);

        strcpy(g_printBuffer, "Facilities on Site: ");
        DrawCentered(&g_font14, g_printBuffer, 0x77, 0x101);
        int half = hcA / 2, hp1 = half + 1;
        g_printBuffer[0] = 0;
        strcat(g_printBuffer, itoa(hp1, g_numString, 10));
        strcat(g_printBuffer, "  (");
        strcat(g_printBuffer, itoa(Clamp(hp1 - 4, 0, 0x63), g_numString, 10));
        strcat(g_printBuffer, ")");
        DrawLeft(&g_font14, g_printBuffer, 0x1ea, 0x101);

        if (nHoles) {
            int s;

            g_printBuffer[0] = 0;
            FormatNumberWithCommas(totals[0]);
            strcat(g_printBuffer, " yds.");
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0x68);
            if (hcB == 0x12)
                s = 10 - (idealYds - totals[0]) / 100;
            else
                s = ((totals[0] - idealYds) * (tier + 4) * 5) / idealYds + 10;
            {
                int stars = Clamp(s, 0, 10);
                for (int i = 0; i < stars; i++)
                    g_bubblePieces[17].BlitScaled(&g_simScreen, 0x12f + i * 0xe, 0x68, 0);
                grade += stars;
                if (!stars) {
                    SetTextRed();
                    DrawCentered(&g_font14, "- not acceptable -", 0x182, 0x68);
                    SetTextBlack();
                    grade = -999;
                }
            }

            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(g_nextOpenHoleIndex - 1, g_numString, 10));
            s = 10 - abs(hcB - (g_nextOpenHoleIndex - 1)) * (4 - tier);
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0x79);
            if (s < 0)
                s = 0;
            {
                int stars = s;
                for (int i = 0; i < stars; i++)
                    g_bubblePieces[17].BlitScaled(&g_simScreen, 0x12f + i * 0xe, 0x79, 0);
                grade += stars;
                if (!stars) {
                    SetTextRed();
                    DrawCentered(&g_font14, "- not acceptable -", 0x182, 0x79);
                    SetTextBlack();
                    grade = -999;
                }
            }

            g_printBuffer[0] = 0;
            if (totals[3] >= 60) {
                strcat(g_printBuffer, itoa(totals[3] / 60, g_numString, 10));
                strcat(g_printBuffer, " h ");
            }
            strcat(g_printBuffer, itoa(totals[3] % 60, g_numString, 10));
            strcat(g_printBuffer, " m");
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0x8a);
            s = 10 - (totals[3] - 0xeb) / 6;
            if (s == 0 && totals[3] <= 0x12c)
                s++;
            DrawStarRow(Clamp(s, 0, 10), 0x8a, grade);

            {
                int v = totals[4] / nHoles;
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(v, g_numString, 10));
                strcat(g_printBuffer, "%");
                DrawCentered(&g_font14, g_printBuffer, 0xee, 0x9b);
                DrawStarRow(Clamp((v - 0x6d) / 10 + 10, 0, 10), 0x9b, grade);
            }

            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(totals[8], g_numString, 10));
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0xac);
            DrawStarRow(Clamp(totals[8] - hcA + 10, 0, 10), 0xac, grade);

            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(nShort, g_numString, 10));
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0xbd);
            DrawStarRow(Clamp(nShort - hcA + 10, 0, 10), 0xbd, grade);

            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(nDiff[0], g_numString, 10));
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0xce);
            DrawStarRow(Clamp(nDiff[0] - hcA + 10, 0, 10), 0xce, grade);

            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(nDiff[1], g_numString, 10));
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0xdf);
            DrawStarRow(Clamp(nDiff[1] - hcA + 10, 0, 10), 0xdf, grade);

            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(nDiff[2], g_numString, 10));
            DrawCentered(&g_font14, g_printBuffer, 0xee, 0xf0);
            DrawStarRow(Clamp(nDiff[2] - hcA + 10, 0, 10), 0xf0, grade);

            {
                int nFac = 0;
                for (int b = 6; b < 20; b++)
                    if (g_reachableAmenityFlags & (1 << b))
                        nFac++;
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(nFac, g_numString, 10));
                DrawCentered(&g_font14, g_printBuffer, 0xee, 0x101);
                DrawStarRow(Clamp((nFac - half) * 2 + 8, 0, 10), 0x101, grade);
            }
            if (grade >= 0x5a) {
                if (g_nextOpenHoleIndex > FRONT_NINE_LAST_HOLE)
                    TriggerAccomplishment(0xc, (g_allHolesArray[9].teeX << 10) + 0x200,
                        (g_allHolesArray[9].teeY << 10) + 0x200);
                if (g_nextOpenHoleIndex > MAX_HOLES)
                    TriggerAccomplishment(0x10, (g_allHolesArray[18].teeX << 10) + 0x200,
                        (g_allHolesArray[18].teeY << 10) + 0x200);
                if (grade == 0x64 && g_nextOpenHoleIndex > MAX_HOLES)
                    TriggerAccomplishment(0x15, (g_allHolesArray[18].teeX << 10) + 0x200,
                        (g_allHolesArray[18].teeY << 10) + 0x200);
            }
        }

        DrawLeft(&g_font14, "Committee recommendation", 0xae, 0x120);
        g_printBuffer[0] = 0;
        if (grade <= 0) {
            SetTextRed();
            DrawLeft(&g_font14, "0/100", 0x172, 0x50);
            DrawCentered(&g_font16, "Improvement Required.", 0x190, 0x136);
            SetTextBlack();
            g_runModeFlags &= ~GAME_TOURNAMENT_OFFERED;
            btnX = 0x2bd;
            rowY = 0x18e;
        } else {
            strcat(g_printBuffer, itoa(grade, g_numString, 10));
            strcat(g_printBuffer, "/100");
            g_previousSGAMark = grade;
            DrawRight(&g_font14, g_printBuffer, 0x18c, 0x4d);
            g_printBuffer[0] = 0;
            {
                const char *name;
                switch ((grade - 0x28) / 5 - 1) {
                case 9:
                case 10:
                case 11: name = (hcA == 0x12) ? "Grand Slam Championship!" : "Mini Slam Championship!"; break;
                case 8: name = "SGA Championship!"; break;
                case 7: name = "SGA Players Championship"; break;
                case 6: name = "SGA Tour Event"; break;
                case 5: name = "Senior SGA Championship"; break;
                case 4: name = "Senior SGA Tour Event"; break;
                case 3: name = "SGA Amateur Championship"; break;
                case 2: name = "Jr. Tour Championship!"; break;
                case 1: name = "Jr. Tour Event"; break;
                case 0: name = "SGA Qualifying School"; break;
                default: name = "Jr. Qualifying School"; break;
                }
                strcat(g_printBuffer, name);
            }
            DrawCentered(&g_infoTitleFont, g_printBuffer, 0x190, 0x136);
            {
                int prize = (((grade - 0x32) / 5 + 5) * (tier + 1)) * g_nextOpenHoleIndex * 2;
                g_winnersPurseK = prize;
                strcpy(g_printBuffer, "\xa7");
                FormatNumberWithCommas(prize);
            }
            strcat(g_printBuffer, ",000 first prize.");
            DrawCentered(&g_font16, g_printBuffer, 0x190, 0x152);
            g_runModeFlags |= GAME_PRO_CHALLENGE | GAME_TOURNAMENT_OFFERED;
            btnX = 0x2bd;
            rowY = 0x18e;
        }
    } else {
        if (!nHoles)
            rowY -= 2;
        g_courseOverview.footer.draw_antialiased(&g_courseOverview.footerAlpha, &g_simScreen, 0, rowY, 0);
        rowY += g_courseOverview.footer.m_height - 0x3f;
        DrawCentered(&g_infoTitleFont, "COURSE REPORT", 0x13c, 0x23);
        DrawCentered(&g_font14, "Top 100 Hole", 0xb2, rowY);
        DrawCentered(&g_font14, "Top 18 Hole", 0x14f, rowY);
        DrawCentered(&g_font14, "Scenic Hole", 0x1e7, rowY);
        for (int c = 0; c < 12; c++)
            DrawCentered(&g_font14, g_courseColumnNames[c], g_courseColumnX[c], 0x4c);
        rowY += 0xf;
        btnX = 0x2de;
    }

    {
        int hot = 0;
        g_okButtonStates[0].BlitScaled(&g_simScreen, btnX, rowY, 0);
        if (mode == 2)
            return;
        g_simScreen.RenderFullSurface(hot);
        WaitWithTimeout(0xa, hot);
        flush_input();
        for (;;) {
            WaitWithTimeout(1, 0);
            g_simScreen.get_mouse_pos(&mouseX, &mouseY);
            if (mouseX >= btnX && mouseX < btnX + 0x2c && mouseY >= rowY && mouseY < rowY + 0x2c) {
                if (!hot) {
                    hot = 1;
                    g_okButtonStates[1].BlitScaled(&g_simScreen, btnX, rowY, 0);
                    g_simScreen.RenderFullSurface(0);
                }
            } else if (hot) {
                hot = 0;
                g_okButtonStates[0].BlitScaled(&g_simScreen, btnX, rowY, 0);
                g_simScreen.RenderFullSurface(0);
            }
            ProcessMouseMessages();
            if (g_mouseButton)
                break;
            if (PollInputState())
                break;
        }
        g_reportScreenId = 0;
        WaitForInputRelease();
    }
}


int GolferAge(int golfer)
{
    int r = g_golferPool[golfer].typeIdx % 10;
    unsigned short f = g_golferPool[golfer].ageFlags;

    if (f & 1)
        r += 20;
    if (f & 2)
        r += 30;
    if (f & 4)
        r += 45;
    return r;
}

void AppendSkillWord(int v)
{
    const char *s;

    if (v < 0)
        s = " (poor)";
    else
        switch (v / 25) {
        case 0: s = " (fair)"; break;
        case 1: s = " (good)"; break;
        case 2:
        case 3: s = " (very good)"; break;
        default: s = " (outstanding)"; break;
        }
    strcat(g_printBuffer, s);
}


void RenderHoleStatsScreen(int h)
{
    int frame, rounds, div, i, j, c, v, lo, hi, y, best, max, total;
    int sumN, sumW, cnt[8], sw[8], w[64];
    int hover, mx, my, pass;
    int par;
    char firstFrame;
    const char *word;
    int col;

    rounds = g_allHolesArray[h].rounds;
    firstFrame = 1;
    hover = 0;
    frame = (int)((timeGetTime() >> 10) % (rounds ? 5 : 3));
    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);

    for (;;) {
        rounds = g_allHolesArray[h].rounds;
        g_holeByHolePanels.top.draw_antialiased(&g_holeByHolePanels.topAlpha, &g_simScreen, 0, 0, 0);
        y = g_holeByHolePanels.top.m_height;
        g_simScreen.set_text_color(0x80000000, -1, 2, 2);

        strcpy(g_printBuffer, "HOLE STATS for ");
        AppendHoleName(h);
        if (g_allHolesArray[h].flags & 0x81) {
            strcat(g_printBuffer, " (");
            strcat(g_printBuffer, itoa(h, g_numString, 10));
            strcat(g_printBuffer, ")");
        }
        DrawCentered(&g_infoTitleFont, g_printBuffer, 0x181, 0x30);

        strcpy(g_printBuffer, "Yards");
        DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x53);
        g_printBuffer[0] = '\0';
        strcat(g_printBuffer, itoa(g_allHolesArray[h].yards, g_numString, 10));
        DrawCentered(&g_font14, g_printBuffer, 0x218, 0x53);

        if (g_allHolesArray[h].rounds) {
            for (i = 0; i < 152; i++)
                if (g_golferPool[i].holeIdx == h && g_golferPool[i].holeStrokes > 0)
                    rounds--;
            if (rounds == 0)
                rounds = 1;
            switch (frame) {
            case 0:
                strcpy(g_printBuffer, "Avg. Drive: ");
                DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x68);
                g_printBuffer[0] = '\0';
                strcat(g_printBuffer, itoa(g_allHolesArray[h].driveTot / g_allHolesArray[h].rounds, g_numString, 10));
                strcat(g_printBuffer, " yds");
                DrawCentered(&g_font14, g_printBuffer, 0x24f, 0x68);
                break;
            case 1:
                strcpy(g_printBuffer, "Longest Drive");
                DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x68);
                g_printBuffer[0] = '\0';
                strcat(g_printBuffer, itoa(g_allHolesArray[h].longDrive, g_numString, 10));
                strcat(g_printBuffer, " yds");
                DrawCentered(&g_font14, g_printBuffer, 0x24f, 0x68);
                break;
            case 2:
                strcpy(g_printBuffer, "Fairways hit");
                DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x68);
                g_printBuffer[0] = '\0';
                strcat(g_printBuffer,
                    itoa(g_allHolesArray[h].fairways * 100 / g_allHolesArray[h].rounds, g_numString, 10));
                strcat(g_printBuffer, "%");
                DrawCentered(&g_font14, g_printBuffer, 0x24f, 0x68);
                break;
            case 3:
                strcpy(g_printBuffer, "Greens in Reg");
                DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x68);
                g_printBuffer[0] = '\0';
                strcat(g_printBuffer, itoa(g_allHolesArray[h].greens * 100 / rounds, g_numString, 10));
                strcat(g_printBuffer, "%");
                DrawCentered(&g_font14, g_printBuffer, 0x24f, 0x68);
                break;
            case 4:
                strcpy(g_printBuffer, "Average Putts");
                DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x68);
                g_printBuffer[0] = '\0';
                AppendMoney(g_allHolesArray[h].putts * 100 / rounds);
                DrawCentered(&g_font14, g_printBuffer, 0x24f, 0x68);
                break;
            }
        }

        strcpy(g_printBuffer, "Par ");
        DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x7d);
        g_printBuffer[0] = '\0';
        strcat(g_printBuffer, itoa(g_allHolesArray[h].par, g_numString, 10));
        DrawCentered(&g_font14, g_printBuffer, 0x218, 0x7d);

        DrawLeft(&g_font14, "Average shots on this hole", 0xbe, 0xb0);

        par = g_allHolesArray[h].par;
        sumN = 0;
        sumW = 0;
        for (i = 0; i < 8; i++)
            sw[i] = par * 8;
        for (i = 0; i < 8; i++)
            cnt[i] = 8;
        lo = par - 2;
        if (lo < 1)
            lo = 1;
        hi = lo + 5;
        for (v = lo; v <= hi; v++) {
            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(v, g_numString, 10));
            if (v == hi)
                strcat(g_printBuffer, "+");
            DrawCentered(&g_font14, g_printBuffer, 0x1bd + (v - lo) * 34, 0xa8);

            total = 0;
            for (c = 0; c < 8; c++) {
                for (j = v; j <= (v == hi ? 9 : v); j++) {
                    int n = g_allHolesArray[h].dist[c][j];
                    total += n;
                    sumN += n;
                    sumW += n * j;
                    cnt[c] += n;
                    sw[c] += n * j;
                }
            }
            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(total, g_numString, 10));
            if (total)
                DrawCentered(&g_font14, g_printBuffer, 0x1bd + (v - lo) * 34, 0xbc);
        }
        if (sumN) {
            strcpy(g_printBuffer, "Stroke average");
            DrawLeft(&g_font14, g_printBuffer, 0x1b9, 0x92);
            g_printBuffer[0] = '\0';
            AppendMoney(sumW * 100 / sumN);
            DrawCentered(&g_font14, g_printBuffer, 0x24f, 0x92);
        }

        DrawLeft(&g_font14, "Fun Factor", 0xbe, 0x53);
        if (g_allHolesArray[h].shots) {
            v = g_allHolesArray[h].moodSum * 100 / (g_allHolesArray[h].shots / 2 + g_allHolesArray[h].rounds + 4);
            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(v, g_numString, 10));
            strcat(g_printBuffer, "%-");
            if (v < 0)
                word = "(poor)";
            else
                switch (v / 20) {
                case 0: word = "(fair)"; break;
                case 1: word = "(good)"; break;
                case 2: word = "(very good)"; break;
                default: word = "(outstanding)"; break;
                }
            strcat(g_printBuffer, word);
            DrawCentered(&g_font14, g_printBuffer, 0x164, 0x53);
        }

        DrawLeft(&g_font14, "Length", 0xbe, 0x68);
        DrawLeft(&g_font14, "Accuracy", 0xbe, 0x7d);
        DrawLeft(&g_font14, "Imagination", 0xbe, 0x92);

        if (cnt[7]) {
            if (cnt[6]) {
                strcpy(g_printBuffer, "+");
                AppendMoney(sw[6] * 100 / cnt[6] - sw[7] * 100 / cnt[7]);
                AppendSkillWord(g_lastHundredthsShown);
                DrawCentered(&g_font14, g_printBuffer, 0x164, 0x68);
            }
            if (cnt[5]) {
                strcpy(g_printBuffer, "+");
                AppendMoney(sw[5] * 100 / cnt[5] - sw[7] * 100 / cnt[7]);
                AppendSkillWord(g_lastHundredthsShown);
                DrawCentered(&g_font14, g_printBuffer, 0x164, 0x7d);
            }
            if (cnt[3]) {
                strcpy(g_printBuffer, "+");
                AppendMoney(sw[3] * 100 / cnt[3] - sw[7] * 100 / cnt[7]);
                AppendSkillWord(g_lastHundredthsShown);
                DrawCentered(&g_font14, g_printBuffer, 0x164, 0x92);
            }
        }

        DrawLeft(&g_font14, "Comments", 0xbe, 0xca);

        if (g_allHolesArray[h].par == 0) {
            g_holeByHolePanels.middle.draw_antialiased(&g_holeByHolePanels.middleAlpha, &g_simScreen, 0, y, 0);
            if (timeGetTime() & 0x200) {
                g_simScreen.set_text_color(0x80007d08, -1, 2, 2);
                DrawCentered(&g_font14, "Under Construction!", 0x190, y);
            }
            y += g_holeByHolePanels.middle.m_height;
        }

        for (i = 0; i < 64; i++)
            w[i] = g_allHolesArray[h].cmt[i];

        for (pass = 5; pass; pass--) {
            max = 0;
            for (i = 0; i < 64; i++) {
                if (i < 50 && w[i] > max) {
                    max = w[i];
                    best = i;
                }
            }
            if (max && g_allHolesArray[h].rounds) {
                g_holeByHolePanels.middle.draw_antialiased(&g_holeByHolePanels.middleAlpha, &g_simScreen, 0, y, 0);
                g_printBuffer[0] = '\0';
                strcat(g_printBuffer, itoa(w[best] * 100 / g_allHolesArray[h].rounds, g_numString, 10));
                strcat(g_printBuffer, "%   ");
                strcat(g_printBuffer, "'");
                BuildGolferDialogue(best, g_allHolesArray[h].cmtCode[best] & 0x3fff, h * 11, SCRATCH_GOLFER);
                strcat(g_printBuffer, "'");
                col = g_remarkColourValue;
                g_simScreen.set_text_color(
                    col == 0x800023e8 ? 0x80001284 : (col == 0x80007d08 ? 0x80007d08 : 0x80000000), -1, 2, 2);
                DrawCentered(&g_font14, g_printBuffer, 0x190, y);
                w[best] = 0;
                y += g_holeByHolePanels.middle.m_height;
            }
        }

        g_holeByHolePanels.bottom.draw_antialiased(&g_holeByHolePanels.bottomAlpha, &g_simScreen, 0, y, 0);
        y += 8;
        g_okButtonStates[hover ? 1 : 0].BlitScaled(&g_simScreen, 0x23e, y, 0);
        g_simScreen.RenderFullSurface(0);

        if (firstFrame) {
            WaitWithTimeout(10, 0);
            flush_input();
            firstFrame = 0;
        }
        WaitWithTimeout(1, 0);
        g_simScreen.get_mouse_pos(&mx, &my);
        div = rounds ? 5 : 3;
        while (frame == (int)((timeGetTime() >> 10) % div)) {
            if (mx >= 0x23e && mx < 0x26a && my >= y && my < y + 0x2c) {
                if (!hover) {
                    hover = 1;
                    g_okButtonStates[1].BlitScaled(&g_simScreen, 0x23e, y, 0);
                    g_simScreen.RenderFullSurface(0);
                }
            } else if (hover) {
                hover = 0;
                g_okButtonStates[0].BlitScaled(&g_simScreen, 0x23e, y, 0);
                g_simScreen.RenderFullSurface(0);
            }
            ProcessMouseMessages();
            if (g_mouseButton || PollInputState()) {
                g_displayFocus = 0;
                WaitForInputRelease();
                return;
            }
            WaitWithTimeout(1, 0);
            g_simScreen.get_mouse_pos(&mx, &my);
        }
        frame = (int)((timeGetTime() >> 10) % div);
    }
}

void RenderPlayerCommentsReport()
{
    int counts[64];
    int firstHole[64];
    int totalRounds;
    int runningMax;
    int rowsLeft;
    int textY;
    int rowY;
    int y;
    int col;
    int best;
    int hole;
    int mx, my;
    int hover;
    int color;

    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    g_commentsSheet.top.draw_antialiased(&g_commentsSheet.topAlpha, &g_simScreen, 0x94, 0x2d, 0);

    y = g_commentsSheet.top.m_height + 0x16;
    rowY = y;
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);

    DrawCentered(&g_infoTitleFont, "PLAYER COMMENTS REPORT", 0x185, 0x3d);
    DrawLeft(&g_font14, "Comments", 0xb6, 0x60);
    DrawCentered(&g_font14, "Hole", 0x1f8, 0x60);
    DrawCentered(&g_font14, "Frequency", 0x256, 0x60);

    totalRounds = 0;
    for (col = 0; col < 64; col++) {
        runningMax = 0;
        counts[col] = 0;
        for (hole = 1; hole < 19; hole++) {
            int n = g_allHolesArray[hole].cmt[col];
            counts[col] += n;
            if (col == 0)
                totalRounds += g_allHolesArray[hole].rounds;
            if (n > runningMax) {
                runningMax = n;
                firstHole[col] = hole;
            }
        }
    }

    textY = y + 2;
    rowsLeft = 20;
    do {
        runningMax = 0;
        for (col = 0; col < 64; col++) {
            if (col < 50 && counts[col] > runningMax) {
                runningMax = counts[col];
                best = col;
            }
        }
        if (runningMax && totalRounds) {
            g_commentsSheet.middle.draw_antialiased(&g_commentsSheet.middleAlpha, &g_simScreen, 0x94, y, 0);

            strcpy(g_printBuffer, "'");
            BuildGolferDialogue(best, g_allHolesArray[firstHole[best]].cmtCode[best] & 0x3fff, -1, SCRATCH_GOLFER);
            strcat(g_printBuffer, "'");

            color = g_remarkColourValue == 0x800023e8 ? 0x80001284
                                                      : (g_remarkColourValue == 0x80007d08 ? 0x80007d08 : 0x80000000);
            g_simScreen.set_text_color(color, -1, 2, 2);
            DrawLeft(&g_font14, g_printBuffer, 0xb6, textY);

            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(counts[best] * 100 / totalRounds, g_numString, 10));
            strcat(g_printBuffer, "%");
            DrawCentered(&g_font14, g_printBuffer, 0x256, textY);

            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(firstHole[best], g_numString, 10));
            DrawCentered(&g_font14, g_printBuffer, 0x1f8, textY);

            counts[best] = 0;
            y = rowY + 0xf;
            rowY = y;
            textY += 0xf;
        }
    } while (--rowsLeft);

    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    g_commentsSheet.bottom.draw_antialiased(&g_commentsSheet.bottomAlpha, &g_simScreen, 0x94, y, 0);

    y += 0xe;
    hover = 0;
    g_okButtonStates[0].BlitScaled(&g_simScreen, 0x24f, y, 0);
    g_simScreen.RenderFullSurface(hover);
    WaitWithTimeout(10, hover);
    flush_input();

    for (;;) {
        WaitWithTimeout(1, 0);
        g_simScreen.get_mouse_pos(&mx, &my);
        if (mx >= 0x24f && mx < 0x27b && my >= y && my < y + 0x2c) {
            if (!hover) {
                hover = 1;
                g_okButtonStates[1].BlitScaled(&g_simScreen, 0x24f, y, 0);
                g_simScreen.RenderFullSurface(0);
            }
        } else if (hover) {
            hover = 0;
            g_okButtonStates[0].BlitScaled(&g_simScreen, 0x24f, y, 0);
            g_simScreen.RenderFullSurface(0);
        }
        ProcessMouseMessages();
        if (g_mouseButton || PollInputState())
            break;
    }
    g_reportScreenId = 0;
    WaitForInputRelease();
}

#define DrawCenteredIn(f, s, x, y, w) do { if (s) g_simScreen.DrawTextCenteredInWidthWithFont(f, s, x, y, w, strlen(s)); } while (0)

void RenderMembershipRosterScreen()
{
    int top;
    int i;
    int count;
    int skip;
    int mx, my;
    char nm[0x13];
    int cnt[0x13];

    count = 0;
    top = 0;

    for (i = 0; i < 76; i++)
        if (!(g_golferHistoryPerCharacter[i].flags & 0x80) && g_golferHistoryPerCharacter[i].count)
            count++;
    if (!count)
        count = 1;

    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);

    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);

    for (;;) {
        int hot, y;
        char redraw;

        skip = 0;
        g_memberRoster.image.draw_antialiased(&g_memberRoster.alpha, &g_simScreen, 0, 0, 0);

        g_simScreen.set_text_color(0x80000000, -1, 2, 2);
        DrawLeft(&g_infoTitleFont, "Membership Roster", 0xd4, 0x13);
        DrawLeft(&g_font14, "Member", 0x21, 0x3c);
        DrawCentered(&g_font14, "Low", 0xaa, 0x3c);
        DrawCentered(&g_font14, "Hcp", 0xd1, 0x3c);
        DrawCentered(&g_font14, "Rnds", 0xf9, 0x3c);
        DrawCentered(&g_font14, "Status", 0x140, 0x3c);

        {
            int n = 1;
            int x = 0x17d;
            do {
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(n, g_numString, 10));
                DrawCenteredIn(&g_font14, g_printBuffer, x, 0x3c, 0x13);
                x += 0x15;
                n++;
            } while (x < 0x2f7);
        }

        if (count > 0x16) {
            int off = top * 0x16c / count;
            int len;
            g_memberRoster.buttons[17].BlitScaled(&g_simScreen, 0x2ff, 0x50, 0);
            len = 0x16c * 0x16 / count;
            if (len >= 0x16c - off)
                len = 0x16c - off;
            g_simScreen.fill(0x305, 0x7a + off, 6, len, COLOR_WHITE);
        }

        y = 0x59;
        do {
            int sel;

            memcpy(nm, "zzzzzzzzzzzzzzzzzz", sizeof(nm));
            sel = -1;
            i = 0;
            do {
                if (!(g_golferHistoryPerCharacter[i].flags & 0x80) && g_golferHistoryPerCharacter[i].count
                    && strcmp(nm, g_golferTypeArray[i].name) >= 0) {
                    strcpy(nm, g_golferTypeArray[i].name);
                    sel = i;
                }
                i++;
            } while (i < 76);

            if (sel == -1)
                break;

            g_golferHistoryPerCharacter[sel].flags |= 0x80;
            if (skip < top) {
                skip++;
                continue;
            }
            i = g_golferHistoryPerCharacter[sel].flags & MEMBERSHIP_MASK;
            if (g_golferHistoryPerCharacter[sel].finishedStoryId == -1)
                i = -1;

            strcpy(g_printBuffer, g_golferTypeArray[sel].name);
            DrawLeft(&g_font14, g_printBuffer, 0x1c, y);

            if (i != 1)
                g_memberRoster.buttons[(i >= 3 ? (i > 3 ? 2 : 1) : (i == -1 ? 3 : 0)) * 2 + 1].BlitScaled(&g_simScreen,
                    0x7a, y - 4, 0);

            g_printBuffer[0] = 0;
            if (g_golferHistoryPerCharacter[sel].bestScore)
                strcat(g_printBuffer, itoa(g_golferHistoryPerCharacter[sel].bestScore, g_numString, 10));
            else
                strcat(g_printBuffer, "-");
            DrawCentered(&g_font14, g_printBuffer, 0xaa, y);
            {
                signed char lo = g_golferHistoryPerCharacter[sel].avgScore;
                g_printBuffer[0] = 0;
                if (lo > 0)
                    strcat(g_printBuffer, itoa(lo, g_numString, 10));
                else
                    strcat(g_printBuffer, "-");
                DrawLeft(&g_font14, g_printBuffer, 0xd1, y);
            }
            {
                short r = g_golferHistoryPerCharacter[sel].count;
                g_printBuffer[0] = 0;
                if (r)
                    strcat(g_printBuffer, itoa(r, g_numString, 10));
                else
                    strcat(g_printBuffer, "-");
                DrawLeft(&g_font14, g_printBuffer, 0xf9, y);
            }

            {
                int h = 1;
                int x = 0x17d;
                do {
                    unsigned char f = g_golferHistoryPerCharacter[sel].perHoleFlags[h];
                    if (f & HOLE_MARKS)
                        g_memberRoster.buttons[(f & 1) ? ((~h & 1) | 8) : ((~h & 1) | 0xa)].BlitScaled(&g_simScreen, x,
                            y - 4, 0);
                    if (g_golferHistoryPerCharacter[sel].perHoleFlags[h] & HOLE_QUIT)
                        g_memberRoster.buttons[(~h & 1) | 6].BlitScaled(&g_simScreen, x, y - 4, 0);
                    x += 0x15;
                    h++;
                } while (x < 0x2f7);
            }

            switch (i) {
            case -1: DrawCentered(&g_font14, "Resigned", 0x140, y); break;
            case 1: DrawCentered(&g_font14, "Visitor", 0x140, y); break;
            case 2: DrawCentered(&g_font14, "Member", 0x140, y); break;
            case 3: DrawCentered(&g_font14, "Silver Member", 0x140, y); break;
            case 4: DrawCentered(&g_font14, "Gold Member", 0x140, y); break;
            }
            y += 0x14;
        } while (y < 0x211);

        DrawCentered(&g_font14, "Member", 0x52, 0x218);
        DrawCentered(&g_font14, "Silver Member", 0xd0, 0x218);
        DrawCentered(&g_font14, "Gold Member", 0x146, 0x218);
        DrawCentered(&g_font14, "Resigned", 0x1c2, 0x218);
        DrawCentered(&g_font14, "Photo Opp", 0x23d, 0x218);
        DrawCentered(&g_font14, "Happy Ending", 0x2b5, 0x218);

        {
            int seen = 0;
            int c;
            memset(cnt, 0, sizeof(cnt));
            for (c = 0; c < 76; c++) {
                if (g_golferHistoryPerCharacter[c].finishedStoryId == -1) {
                    for (i = 1; i < 0x13; i++)
                        if (g_golferHistoryPerCharacter[c].perHoleFlags[i] & HOLE_QUIT)
                            cnt[i]++;
                    if (seen++)
                        strcat(g_printBuffer, ", ");
                    strcat(g_printBuffer, g_golferTypeArray[c].name);
                    if (seen >= 0xa) {
                        strcat(g_printBuffer, ",");
                        seen = 0;
                        strcpy(g_printBuffer, " ");
                    }
                }
            }
            if (!seen)
                strcat(g_printBuffer, " none");
            strcat(g_printBuffer, ".");
            {
                int *p = &cnt[1];
                int k = 0x12;
                do {
                    if (*p) {
                        g_printBuffer[0] = 0;
                        strcat(g_printBuffer, itoa(*p, g_numString, 10));
                    }
                    p++;
                } while (--k);
            }
        }

        for (i = 0; i < 76; i++)
            g_golferHistoryPerCharacter[i].flags &= 0x7f;

        g_simScreen.get_mouse_pos(&mx, &my);
        hot = -1;
        g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2dc, 0x224, 0);
        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(0xa, 0);
        flush_input();
        redraw = 0;
        do {
            WaitWithTimeout(1, 0);
            g_simScreen.get_mouse_pos(&mx, &my);
            if (mx >= 0x2dc && mx < 0x308 && my >= 0x224 && my < 0x250) {
                if (hot != 0) {
                    if (hot > 0) {
                        redraw = 1;
                        hot = 0;
                        break;
                    }
                    hot = 0;
                    g_okButtonStates[1].BlitScaled(&g_simScreen, 0x2dc, 0x224, 0);
                    g_simScreen.RenderFullSurface(0);
                }
            } else if (mx >= 0x2ff && mx < 0x311 && my >= 0x50 && my < 0x75) {
                if (hot != 1) {
                    if (hot == 0)
                        g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2dc, 0x224, 0);
                    else if (hot > 0) {
                        redraw = 1;
                        hot = 1;
                        break;
                    }
                    hot = 1;
                    g_memberRoster.buttons[12].BlitScaled(&g_simScreen, 0x2ff, 0x50, 0);
                    g_simScreen.RenderFullSurface(0);
                }
            } else if (mx >= 0x2ff && mx < 0x311 && my >= 0x1ea && my < 0x20f) {
                if (hot != 2) {
                    if (hot == 0)
                        g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2dc, 0x224, 0);
                    else if (hot > 0) {
                        redraw = 1;
                        hot = 2;
                        break;
                    }
                    hot = 2;
                    g_memberRoster.buttons[13].BlitScaled(&g_simScreen, 0x2ff, 0x1ea, 0);
                    g_simScreen.RenderFullSurface(0);
                }
            } else {
                if (hot != -1) {
                    if (hot == 0)
                        g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2dc, 0x224, 0);
                    else if (hot > 0) {
                        redraw = 1;
                        hot = -1;
                        break;
                    }
                    hot = -1;
                    g_simScreen.RenderFullSurface(0);
                }
            }
            ProcessMouseMessages();
            if (g_mouseButton)
                break;
        } while (!PollInputState());
        if (g_mouseButton == 1) {
            if (hot == 0)
                break;
            if (hot == 1) {
                if (top > 0)
                    top -= Clamp(top, 1, 3);
                continue;
            }
            if (hot == 2) {
                if (count - 0x16 > top)
                    top += Clamp(count - top - 0x16, 1, 3);
            }
            continue;
        }
        if (!redraw)
            break;
    }

    g_reportScreenId = 0;
    WaitForInputRelease();
}

void RenderBestHoleScoresScreen()
{
    int i, y, hover, mx, my;

    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    g_scoreRecords.top.draw_antialiased(&g_scoreRecords.topAlpha, &g_simScreen, 0xc3, 0x2d, 0);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);

    strcpy(g_printBuffer, "Best ");
    strcat(g_printBuffer, itoa(g_nextOpenHoleIndex - 1, g_numString, 10));
    strcat(g_printBuffer, " Hole Scores");
    DrawCentered(&g_infoTitleFont, g_printBuffer, 0x19d, 0x3b);

    strcpy(g_printBuffer, "Golfer");
    DrawLeft(&g_font14, g_printBuffer, 0xec, 0x60);

    strcpy(g_printBuffer, "Score");
    DrawCentered(&g_font14, g_printBuffer, 0x227, 0x60);

    for (i = 0; i < 10; i++) {
        if (g_bestTenScoresTable[i] == 0)
            break;
        y = g_scoreRecords.middle.m_height * i + g_scoreRecords.top.m_height + 0x2d;
        g_scoreRecords.middle.draw_antialiased(&g_scoreRecords.middleAlpha, &g_simScreen, 0xc3, y, 0);
        g_printBuffer[0] = '\0';
        strcat(g_printBuffer, itoa(g_bestTenScoresTable[i], g_numString, 10));
        DrawCentered(&g_font14, g_printBuffer, 0x227, y);
        g_printBuffer[0] = '\0';
        strcat(g_printBuffer, itoa(i + 1, g_numString, 10));
        strcat(g_printBuffer, ".  ");
        AppendPooledString(i + 0x14);
        DrawLeft(&g_font14, g_printBuffer, 0xf6, y);
    }

    y = g_scoreRecords.middle.m_height * i + g_scoreRecords.top.m_height + 0x2d;
    g_scoreRecords.bottom.draw_antialiased(&g_scoreRecords.bottomAlpha, &g_simScreen, 0xc3, y, 0);
    y += 0xe;

    hover = 0;
    g_okButtonStates[0].BlitScaled(&g_simScreen, 0x220, y, 0);
    g_simScreen.RenderFullSurface(0);

    WaitWithTimeout(10, 0);
    flush_input();

    for (;;) {
        WaitWithTimeout(1, 0);
        g_simScreen.get_mouse_pos(&mx, &my);
        if (mx >= 0x220 && mx < 0x24c && my >= y && my < y + 0x2c) {
            if (!hover) {
                hover = 1;
                g_okButtonStates[1].BlitScaled(&g_simScreen, 0x220, y, 0);
                g_simScreen.RenderFullSurface(0);
            }
        } else if (hover) {
            hover = 0;
            g_okButtonStates[0].BlitScaled(&g_simScreen, 0x220, y, 0);
            g_simScreen.RenderFullSurface(0);
        }
        ProcessMouseMessages();
        if (g_mouseButton)
            break;
        if (PollInputState())
            break;
    }
    g_reportScreenId = 0;
    WaitForInputRelease();
}

void RenderHistographScreen()
{
    int stepCash, stepSkill;
    int y, k, i, prevX, sampleX, step;
    int prevSkill, prevCash, prevFun, prevMembers, labelY;
    int mx, my, hot, samples;

    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    g_graphReport.image.draw_antialiased(&g_graphReport.alpha, &g_simScreen, 0, 0, 0);
    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
    DrawCentered(&g_infoTitleFont, "HISTOGRAPH", 0x177, 0x14);
    g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);

    DrawTextCenteredAtY("Skill", 0x0b1, 0x21e, 0x80000000);
    DrawTextCenteredAtY("Cash", 0x14c, 0x21e, 0x80000000);
    DrawTextCenteredAtY("Fun", 0x1e8, 0x21e, 0x80000000);
    DrawTextCenteredAtY("Event", 0x284, 0x21e, 0x80000000);

    stepCash = 1;
    if (g_gameCashHundreds > 2500)
        stepCash = 2;
    if (g_gameCashHundreds > 5000)
        stepCash = 4;
    if (g_gameCashHundreds > 10000)
        stepCash = 10;
    if (g_gameCashHundreds > 25000)
        stepCash = 20;
    if (g_gameCashHundreds > 50000)
        stepCash = 40;
    if (g_gameCashHundreds > 100000)
        stepCash = 80;

    stepSkill = 1;
    if (g_clubSkillsScore > 2500)
        stepSkill = 2;

    for (y = 0x202, k = 0; y > 0xe; k++, y -= 0x32) {
        g_printBuffer[0] = 0;
        if (y >= 0x108)
            AppendMoney(k * 100 * stepSkill);
        else
            AppendMoney((k * 500 - 2000) * stepSkill);
        DrawTextCenteredAtY(g_printBuffer, 0x4d, y, 0x80004010);
        strcpy(g_printBuffer, "\247");
        if (y >= 0x108)
            strcat(g_printBuffer, itoa(k * 10 * stepCash, g_numString, 10));
        else
            strcat(g_printBuffer, itoa((k * 50 - 200) * stepCash, g_numString, 10));
        strcat(g_printBuffer, "k");
        DrawTextCenteredAtY(g_printBuffer, 0x2d7, y, 0x80000000);
    }

    samples = g_clock / 1024;
    step = Clamp(600 / (samples + 1), 1, 4);
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    prevSkill = 0;
    prevCash = 0;
    prevFun = 0;
    labelY = 0;
    prevMembers = 0;
    g_simScreen.SetClipRect(0x6a, 0x48, 0x256, 0x1c2);

    for (i = 1; i <= g_clock / 1024; i++) {
        short a, b, c, e;
        int q, h, kind, n, lx;
        int memberHeight;

        prevX = 0x6b + (i - 1) * step;
        sampleX = 0x6b + i * step;
        lx = 0x6b + i * step + 10;
        a = g_monthlySkillScore[i];
        q = a / stepSkill;
        h = q <= 500 ? q / 2 : q / 10 + 200;
        g_simScreen.line(prevX, 0x209 - prevSkill, sampleX, 0x209 - h, 0x80004010);
        prevSkill = h;

        b = g_moneyLine[i];
        q = b / stepCash;
        h = q <= 500 ? abs(q) / 2 : q / 10 + 200;
        g_simScreen.line(prevX, 0x209 - prevCash, sampleX, 0x209 - h, b >= 0 ? 0x80000000 : 0x80007d08);
        prevCash = h;

        c = g_funRatingHistPoints[i];
        h = c <= 500 ? c / 2 : c / 10 + 200;
        g_simScreen.line(prevX, 0x209 - prevFun, sampleX, 0x209 - h, 0x800003e0);
        prevFun = h;

        memberHeight = g_gameMembershipTrendLine[i] * 4;
        g_simScreen.line(prevX, 0x209 - prevMembers, sampleX, 0x209 - memberHeight, COLOR_TEAL);
        prevMembers = memberHeight;

        e = g_monthEventKindHistory[i];
        if (e) {
            int ly;
            kind = e & ~0x1f;
            n = e & 0x1f;
            switch (kind) {
            case 0x20:
                strcpy(g_printBuffer, "Hole ");
                strcat(g_printBuffer, itoa(n, g_numString, 10));
                strcat(g_printBuffer, " opened.");
                break;
            case 0x40:
                strcpy(g_printBuffer, g_facilityTypes[n].name);
                strcat(g_printBuffer, " built.");
                break;
            case 0x80:
                strcpy(g_printBuffer, "Hole ");
                strcat(g_printBuffer, itoa(n, g_numString, 10));
                strcat(g_printBuffer, " rated top 100.");
                break;
            case 0xa0:
                strcpy(g_printBuffer, "Hole ");
                strcat(g_printBuffer, itoa(n, g_numString, 10));
                strcat(g_printBuffer, " rated top 18!");
                break;
            case 0x60:
                strcpy(g_printBuffer, "Won match vs. ");
                strcat(g_printBuffer, g_golfProList[n].name);
                break;
            case 0xc0:
                strcpy(g_printBuffer, g_themeCelebsEntries[n].name);
                strcat(g_printBuffer, " buys a home.");
                break;
            case 0xe0:
                strcpy(g_printBuffer, g_golferTypeArray[0].name);
                strcat(g_printBuffer, " places ");
                strcat(g_printBuffer, itoa(n, g_numString, 10));
                strcat(g_printBuffer, " in tournament.");
                break;
            case 0x100:
                strcpy(g_printBuffer, g_ceoName);
                strcat(g_printBuffer, " joins the board.");
                break;
            case 0x120: strcpy(g_printBuffer, "Happy Ending."); break;
            case 0x160:
                strcpy(g_printBuffer, "Ivana donates a ");
                AppendLandmarkName(n, 0);
                break;
            case 0x140: strcpy(g_printBuffer, "Additional land purchased."); break;
            }

            labelY = labelY % 450 + 10;
            g_simScreen.fill(lx, 0x209 - prevSkill, 2, 2, COLOR_TEAL);
            ly = 0x209 - labelY;
            g_simScreen.line(sampleX + 11, 0x209 - prevSkill, sampleX + 11, ly, 0x800003ff);
            g_simScreen.fill(sampleX + 11, ly, 2, 2, COLOR_TEAL);
            ly -= 3;
            DrawTextWithColor(g_printBuffer, sampleX + 14, ly, 0x80000210);
        }
    }

    g_simScreen.ResetSurfaceClip();
    hot = 0;
    g_okButtonStates[3].BlitScaled(&g_simScreen, 0x2c0, 0x227, 0);
    g_simScreen.RenderFullSurface(0);
    WaitWithTimeout(0xa, 0);
    flush_input();

    do {
        WaitWithTimeout(1, 0);
        g_simScreen.get_mouse_pos(&mx, &my);
        if (mx >= 0x2c0 && mx < 0x2ec && my >= 0x227 && my < 0x253) {
            if (hot == 0) {
                hot = 1;
                g_okButtonStates[4].BlitScaled(&g_simScreen, 0x2c0, 0x227, 0);
                g_simScreen.RenderFullSurface(0);
            }
        } else if (hot != 0) {
            hot = 0;
            g_okButtonStates[3].BlitScaled(&g_simScreen, 0x2c0, 0x227, 0);
            g_simScreen.RenderFullSurface(0);
        }
        ProcessMouseMessages();
    } while (g_mouseButton == 0 && PollInputState() == 0);

    g_reportScreenId = 0;
    WaitForInputRelease();
}



void TileToScreenCoords(int row, int col, int *px, int *py)
{
    *px = (row + col) * 6 + 0x6a;
    *py = (col - row) * 3 - (g_inSaleView ? 1 : -1) + 0x1b8;
}

int GetTerrainPropertyAt(int row, int col)
{
    return 2 + (g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].lieGrade > 0);
}

void RenderRoutingMapScreen()
{
    Vert poly1[4], poly2[4];
    int order[20], rank[20];
    int sx, sy;
    int i, r, c, ti;
    signed char t;
    int sel, prevSel;
    int scroll, selRow, pplCount;
    int k, rowIdx, pplIdx, shown, firstOff;
    char more;
    signed char *p;

    scroll = 0;
    selRow = 1;
    for (i = 0; i <= 0x13; i++)
        order[i] = i;

    sel = -1;
    pplCount = 0;
    for (i = 0; i < 64; i++) {
        if (g_staffSlots[i].job != JOB_PLAYER_AVATAR && g_staffSlots[i].flags != 0 && g_staffSlots[i].job < 0)
            pplCount++;
    }

    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
redraw:
    if (g_inSaleView == 0) {
        g_routeReport.bottom.BlitScaled(&g_simScreen, 0, 0xfd, 0);
        if (g_mode >= -1 && g_mode < 3)
            g_routeReport.tabs[g_mode + 1].BlitScaled(&g_simScreen, 0, 0, 0);
        else
            g_routeReport.tabs[0].BlitScaled(&g_simScreen, 0, 0, 0);
    }
    if (g_mode == 1)
        RecalculateMoodMaps();
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 5);

    for (r = 0; r < 0x32; r++) {
        p = &g_gameTileTerrainKind[r * 50];
        for (c = 0; c < 0x32; c++) {
            TileToScreenCoords(r, c, &sx, &sy);
            t = p[c];
            if (t != 0x14 && g_buttonSlotOfType[t] == -1) {
                poly1[0].x = sx - 6;
                poly1[0].y = sy;
                poly1[1].x = sx;
                poly1[1].y = sy - 3;
                poly1[2].x = sx + 6;
                poly1[2].y = sy;
                poly1[3].x = sx;
                poly1[3].y = sy + 3;
                g_simScreen.poly(poly1, 4, 0x80001284);
            }
        }
    }

    for (r = 0; r < 0x32; r++) {
        for (c = 0; c < 0x32; c++) {
            int col;
            TileToScreenCoords(r, c, &sx, &sy);
            t = g_gameTileTerrainKind[r * 50 + c];
            if (t == TERRAIN_OUT_OF_BOUNDS)
                continue;
            ti = t;
            if (g_buttonSlotOfType[ti] != -1) {
                if (g_squareTypeTable[ti].category == CATEGORY_TREES) {
                    int id;
                    id = 0x19d;
                    CalculateTerrainHeight(r << 8, c << 8);
                    if (ti == 0xf) {
                        id = 0x197;
                        CalculateTerrainHeight(r << 7, c << 7);
                    } else if (ti == 0xe) {
                        id = 0x194;
                    } else if (ti == 0x10) {
                        id = 0xf9;
                    }
                    g_objectSprite = GetFlicBankSprite(id, g_flicBankFrameLens[id] - 1, r & 3);
                }
            }
            if (g_mode == 2) {
                int score = CalculateLotScore(r, c);
                int v = ValidateBuildingPlacement(r, c, 2, 5);
                col = PackRgb555(0, Clamp((score / 4 - v) * 3 / 2, 0, 0xff), 0);
                if (v == -1)
                    col = 0x80002000;
            }
            if (g_mode == 1) {
                col = PackRgb555(g_badMoodMap[r][c], g_goodMoodMap[r][c],
                    (unsigned char)(g_goodMoodMap[r][c] + g_badMoodMap[r][c]) >> 1);
                if (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_WATER)
                    col = 0x80000218;
            }
            if (g_mode != 0) {
                if (IsPositionInvalid(r, c))
                    col = 0x80002108;
                GetTerrainPropertyAt(r + 1, c);
                GetTerrainPropertyAt(r, c);
                GetTerrainPropertyAt(r, c + 1);
                GetTerrainPropertyAt(r, c);
            }
            if (g_mode <= 0) {
                switch (g_squareTypeTable[g_gameTileTerrainKind[r * 50 + c]].category) {
                case 0:
                case 1: col = 0x80003394; break;
                case CATEGORY_FAIRWAY: col = 0x80001310; break;
                case CATEGORY_ROUGH: col = 0x80000204; break;
                case CATEGORY_SAND: col = 0x80006310; break;
                case 0xa: col = 0x80005304; break;
                case CATEGORY_WATER: col = 0x80000218; break;
                case CATEGORY_TREES: col = 0x80001184; break;
                case CATEGORY_OUT_OF_BOUNDS: col = 0x800010c8; break;
                default: col = 0x8000318c; break;
                }
                if (IsPositionInvalid(r, c))
                    col = 0x80000000;
            }
            poly2[0].x = sx - 6;
            poly2[0].y = sy;
            poly2[1].x = sx;
            poly2[1].y = sy - 3;
            poly2[2].x = sx + 6;
            poly2[2].y = sy;
            poly2[3].x = sx;
            poly2[3].y = sy + 3;
            g_simScreen.poly(poly2, 4, col);
        }
    }

    if (g_inSaleView == 0) {
        g_simScreen.SetFontsForStyles(&g_font16, 0, 0, 0);
        g_printBuffer[0] = 0;
        AppendCourseName(1);
        if (g_mode == 0)
            strcat(g_printBuffer, " Employees");
        DrawTextCenteredAtY(g_printBuffer, 0x190, g_mode ? 0x3a : 0x30, 0x80000000);
        g_simScreen.SetFontsForStyles(&g_infoTitleFont, 0, 0, 0);
        DrawTextCenteredAtY("ROUTING MAP", 0x190, 0xf, 0x80000000);
        g_simScreen.SetFontsForStyles(&g_font14, 0, 0, 0);
        switch (g_mode) {
        case -1:
            DrawTextCenteredAtY("COURSE ROUTING", 0x190, 0x55, 0x80000000);
            DrawTextCenteredAtY("Left click to select hole.", 0x190, 0x6c, 0x80000000);
            DrawTextCenteredAtY("Right click to swap holes.", 0x190, 0x7e, 0x80000000);
            DrawTextCenteredAtY("Hole #", 0x47, 0x55, 0x80000000);
            DrawTextCenteredAtY("Hole #", 0x234, 0x55, 0x80000000);
            DrawTextCenteredAtY("PAR", 0x84, 0x55, 0x80000000);
            DrawTextCenteredAtY("PAR", 0x276, 0x55, 0x80000000);
            DrawTextCenteredAtY("YDS", 0xb6, 0x55, 0x80000000);
            DrawTextCenteredAtY("YDS", 0x2a8, 0x55, 0x80000000);
            DrawTextCenteredAtY("Time", 0xef, 0x55, 0x80000000);
            DrawTextCenteredAtY("Time", 0x2e1, 0x55, 0x80000000);
            break;
        case 1:
            DrawTextCenteredAtY("COURSE AURA", 0x190, 0xbc, 0x80000000);
            DrawTextCenteredAtY("AURA", 0x190, 0x55, 0x80000000);
            DrawTextWithColor("Happy", 0x270, 0xce, 0x80000000);
            DrawTextWithColor("Unhappy", 0x88, 0xce, 0x80000000);
            break;
        case 2:
            DrawTextCenteredAtY("HOME SITE VALUE", 0x190, 0xbc, 0x80000000);
            DrawTextWithColor("High", 0x277, 0xce, 0x80000000);
            DrawTextWithColor("Low", 0x8e, 0xce, 0x80000000);
            break;
        }
        if (g_mode == 1) {
            g_simScreen.set_text_color(0x80000000, -1, 2, 2);
            strcpy(g_printBuffer, "Course AURA indicates where on your course players have made mostly happy comments "
                                  "and where they have made unhappy comments.");
            g_simScreen.wrap_cent(g_printBuffer, 0xfa, 0x6e, 0x12c);
        }
        if (g_mode == 2) {
            DrawTextWithColor("Things which INCREASE home value:", 0x33, 0x56, 0x80000000);
            DrawTextWithColor("Close to water and trees.", 0x33, 0x68, 0x80000000);
            DrawTextWithColor("Close to a fun golf hole.", 0x33, 0x79, 0x80000000);
            DrawTextWithColor("Close to a top 100 or top 18 hole.", 0x33, 0x8a, 0x80000000);
            strcpy(g_printBuffer, "Building a ");
            strcat(g_printBuffer, g_facilityTypes[SCENERY_MARINA].name);
            DrawTextWithColor(g_printBuffer, 0x33, 0x9c, 0x80000000);
            DrawTextWithColor("Things which DECREASE home value:", 0x1ee, 0x56, 0x80000000);
            DrawTextWithColor("Close to an unfun hole.", 0x1ee, 0x68, 0x80000000);
            DrawTextWithColor("Close to another building.", 0x1ee, 0x79, 0x80000000);
            DrawTextWithColor("Too close to green, fairway, or OB.", 0x1ee, 0x8a, 0x80000000);
            DrawTextWithColor("Far away from the golf course.", 0x1ee, 0x9c, 0x80000000);
        }

        for (rowIdx = 1; rowIdx < 19; rowIdx++) {
            int colX, rowY, m, a, b;
            m = (rowIdx - 1) % 9;
            rowY = m * 0x11 + 0x68;
            colX = (rowIdx > 9) ? 0x239 : 0x47;
            if (g_mode == -1) {
                if (rowIdx == selRow) {
                    g_simScreen.fill(colX - 0x27, rowY - 3, 0xee, 0x11, COLOR_BRIGHT_GREEN);
                    g_simScreen.fill(colX - 0x26, rowY - 2, 0xec, 0xf, COLOR_WHITE);
                }
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(rowIdx, g_numString, 10));
                DrawTextCenteredAtY(g_printBuffer, colX, rowY, g_allHolesArray[rowIdx].par ? 0x80000000 : 0x80006318);
                if (g_allHolesArray[rowIdx].par != 0) {
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(g_allHolesArray[rowIdx].par, g_numString, 10));
                    DrawTextCenteredAtY(g_printBuffer, colX + 0x3d, rowY, 0x80000000);
                    g_printBuffer[0] = 0;
                    strcat(g_printBuffer, itoa(g_allHolesArray[rowIdx].yards, g_numString, 10));
                    DrawTextCenteredAtY(g_printBuffer, colX + 0x6f, rowY, 0x80000000);
                    if (g_allHolesArray[rowIdx].rounds != 0) {
                        g_printBuffer[0] = 0;
                        strcat(g_printBuffer,
                            itoa(g_allHolesArray[rowIdx].playTime / g_allHolesArray[rowIdx].rounds / 40, g_numString,
                                10));
                        strcat(g_printBuffer, "m");
                        DrawTextCenteredAtY(g_printBuffer, colX + 0xa8, rowY, 0x80000000);
                    }
                }
            }
            if (g_allHolesArray[rowIdx].par != 0) {
                int sx2, sy2, id;
                SpriteObject *spr;
                sx2 = g_allHolesArray[rowIdx].teeX * 8 + 0x184;
                sy2 = g_allHolesArray[rowIdx].teeY * 8 + 0x68;
                TileToScreenCoords(g_allHolesArray[rowIdx].teeX, g_allHolesArray[rowIdx].teeY, &sx2, &sy2);
                TerrainMoveTo(sx2, sy2);
                id = 0x18d + Clamp(g_allHolesArray[rowIdx].par - 3, 0, 2);
                spr = GetFlicBankSprite(id, g_flicBankFrameLens[id] - 1, 0);
                g_objectSprite = spr;
                DrawSpriteImmediate(sx2, sy2, sy2, 0x28, 0x28, spr, SPRITE_FULL_SCALE, PAL_TEE_MARKER);
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(rowIdx, g_numString, 10));
                if (g_gameFairwayTargetPoints[rowIdx][2].x != -1) {
                    a = g_gameFairwayTargetPoints[rowIdx][2].x >> 10;
                    sx2 = a * 8 + 0x184;
                    b = g_gameFairwayTargetPoints[rowIdx][2].y >> 10;
                    sy2 = b * 8 + 0x68;
                } else if (g_gameFairwayTargetPoints[rowIdx][1].x != -1) {
                    a = g_gameFairwayTargetPoints[rowIdx][1].x >> 10;
                    sx2 = a * 8 + 0x184;
                    b = g_gameFairwayTargetPoints[rowIdx][1].y >> 10;
                    sy2 = b * 8 + 0x68;
                } else {
                    int ox = sx2, oy = sy2;
                    TileToScreenCoords(g_allHolesArray[rowIdx].aimX, g_allHolesArray[rowIdx].aimY, &sx2, &sy2);
                    if (g_mode != 0)
                        DrawTextCenteredWithShadow(g_printBuffer, (sx2 + ox) / 2, (sy2 + oy) / 2 - 4,
                            COLOR_PALE_YELLOW);
                    goto routeDone;
                }
                TileToScreenCoords(a, b, &sx2, &sy2);
                TerrainLineTo(sx2, sy2, COLOR_WHITE, 2, 0xa);
                if (g_mode != 0)
                    DrawTextCenteredWithShadow(g_printBuffer, sx2, sy2 - 4, COLOR_PALE_YELLOW);
routeDone:
                sx2 = g_allHolesArray[rowIdx].aimX * 8 + 0x184;
                sy2 = g_allHolesArray[rowIdx].aimY * 8 + 0x68;
                TileToScreenCoords(g_allHolesArray[rowIdx].aimX, g_allHolesArray[rowIdx].aimY, &sx2, &sy2);
                TerrainLineTo(sx2, sy2, COLOR_WHITE, 2, 0xa);
                id = g_currentThemeId + 0x189;
                spr = GetFlicBankSprite(id, 0, 3);
                g_objectSprite = spr;
                DrawSpriteImmediate(sx2, sy2, sy2, 0x28, 0x28, spr, 2, g_currentThemeId + 0x60);
            }
        }

        pplIdx = 1;
        shown = 0;
        firstOff = 0;
        more = 0;
        if (g_mode == 0 && pplCount > 8) {
            int thumbTop = scroll * 95 / pplCount;
            int thumbLen;
            g_routeReport.cells[11].BlitScaled(&g_simScreen, 0x307, 0x40, 0);
            thumbLen = 0x2f8 / pplCount;
            if (thumbLen >= 0x5f - thumbTop)
                thumbLen = 0x5f - thumbTop;
            g_simScreen.fill(0x30d, thumbTop + 0x6a, 6, thumbLen, COLOR_WHITE);
        }

        for (i = 0; i < 64; i++) {
            Employee *e = &g_staffSlots[i];
            int dir, base, idv, slot, rad, j;
            unsigned lc;
            if (g_mode != 0)
                continue;
            if (e->flags == 0)
                continue;
            if (e->job >= 0)
                continue;
            dir = (g_camDirIdx - e->dir - 2) & 7;
            base = -2 - e->job;
            if (e->flags & STAFF_UPGRADED)
                base += 4;
            idv = base + 0x82;
            if (e->job != JOB_PLAYER_AVATAR) {
                if (shown < scroll) {
                    shown++;
                    firstOff = shown;
                } else {
                    slot = pplIdx - shown;
                    if (slot > 8) {
                        more = 1;
                    } else {
                        short kind = e->anim;
                        int fid, fr, role, rx, ry;
                        unsigned bit;
                        if (kind < 0xb) {
                            fid = 0x20e;
                        } else {
                            fid = 0x216;
                            if (kind == 0xc)
                                fid = 0x21e;
                        }
                        fid += base;
                        fr = e->animTick % g_flicBankFrameLens[fid];
                        if (fr == 0 && kind == 0xc)
                            e->anim = 0xb;
                        g_objectSprite = GetFlicBankSprite(fid, fr, dir);
                        bit = e->flags & STAFF_UPGRADED;
                        role = (bit >> 3) - (e->job * 2 + 4);
                        rx = (slot < 5) ? 0x29 : 0x19f;
                        ry = ((slot - 1) % 4) * 45 + 0x47;
                        DrawSpriteImmediate(rx + 0x14b, ry + 0x20, ry, 0x28, 0x28, g_objectSprite, SPRITE_FULL_SCALE,
                            idv);
                        g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);
                        g_printBuffer[0] = 0;
                        strcat(g_printBuffer, itoa(pplIdx, g_numString, 10));
                        strcat(g_printBuffer, ". ");
                        BuildEmployeeName(i);
                        DrawTextWithColor(g_printBuffer, rx, ry, 0x80000000);
                        ry += 0xf;
                        rx += 0xf;
                        strcpy(g_printBuffer, "Hired: ");
                        FormatDateString(e->hiredAt << 10);
                        strcat(g_printBuffer, ", paid: \xa7");
                        FormatNumberWithCommas(e->paid * 100);
                        DrawTextWithColor(g_printBuffer, rx, ry, 0x80000000);
                        ry += 0xf;
                        switch (role) {
                        case 0: strcpy(g_printBuffer, "Players greeted: "); break;
                        case 1: strcpy(g_printBuffer, "Players cheered: "); break;
                        case 2: strcpy(g_printBuffer, "Players rushed: "); break;
                        case 3: strcpy(g_printBuffer, "Slackers intimidated: "); break;
                        case 4: strcpy(g_printBuffer, "Weeds destroyed: "); break;
                        case 5: strcpy(g_printBuffer, "Weeds eradicated: "); break;
                        case 6: strcpy(g_printBuffer, "Beverages served: "); break;
                        case 7: strcpy(g_printBuffer, "Satisfied customers: "); break;
                        }
                        strcat(g_printBuffer, itoa(e->tally, g_numString, 10));
                        DrawTextWithColor(g_printBuffer, rx, ry, 0x80000000);
                    }
                }
            }
            sx = e->postRow * 8 + 0x184;
            sy = e->postCol * 8 + 0x68;
            TileToScreenCoords(e->postRow, e->postCol, &sx, &sy);
            switch (e->job + 5) {
            case 1:
                lc = 0x80007ff0;
                rad = 0x30;
                break;
            case 2:
                lc = 0x80000018;
                rad = 0x18;
                break;
            case 0:
                lc = 0x800003ff;
                rad = 0x18;
                break;
            case 3:
                lc = 0x80006318;
                rad = 0x18;
                break;
            default:
                lc = 0x80007fff;
                rad = 0x18;
                break;
            }
            if (e->flags & STAFF_UPGRADED)
                rad += rad / 2;
            for (j = 0; j < 0x19; j++) {
                int ang = j * 0xaaaaaaa;
                int px = FixedPointSin(ang, rad) + sx;
                int py = FixedPointCos(ang, rad / 2) + sy;
                if (j == 0)
                    TerrainMoveTo(px, py);
                else
                    TerrainLineTo(px, py, lc, 2, LINE_ALPHA);
            }
            if (e->job != JOB_PLAYER_AVATAR) {
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(pplIdx++, g_numString, 10));
                strcat(g_printBuffer, ". ");
                DrawTextCenteredWithShadow(g_printBuffer, sx, sy - 0xa, lc);
            }
            g_printBuffer[0] = 0;
            BuildEmployeeName(i);
            g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
            DrawTextCenteredWithShadow(g_printBuffer, sx, sy, lc);
            sx = (e->posX >> 10) * 8 + 0x184;
            sy = (e->posY >> 10) * 8 + 0x68;
        }

        if (graphsys)
            graphsys->set_sprite_scale(1, 1, 1);

        if (sel == -2)
            g_routeReport.cells[8].BlitScaled(&g_simScreen, 0x296, 0x214, 0);
        else if (sel == 1)
            g_routeReport.cells[0].BlitScaled(&g_simScreen, 0x3c, 0x139, 0);
        else if (sel == 0)
            g_routeReport.cells[2].BlitScaled(&g_simScreen, 0xb6, 0xfe, 0);
        else if (sel == 2)
            g_routeReport.cells[3].BlitScaled(&g_simScreen, 0x1e3, 0xfe, 0);
        else if (sel == 3)
            g_routeReport.cells[1].BlitScaled(&g_simScreen, 0x264, 0x139, 0);
        if (g_mode == 0) {
            if (sel == 9)
                g_routeReport.cells[9].BlitScaled(&g_simScreen, 0x307, 0x40, 0);
            else if (sel == 0xa)
                g_routeReport.cells[10].BlitScaled(&g_simScreen, 0x307, 0xcd, 0);
            g_routeReport.cells[4].BlitScaled(&g_simScreen, 0x3c, 0x139, 0);
        } else if (g_mode == -1) {
            g_routeReport.cells[6].BlitScaled(&g_simScreen, 0xb6, 0xfe, 0);
        } else if (g_mode == 1) {
            g_routeReport.cells[7].BlitScaled(&g_simScreen, 0x1e2, 0xfe, 0);
        } else if (g_mode == 2) {
            g_routeReport.cells[5].BlitScaled(&g_simScreen, 0x264, 0x139, 0);
        }
        g_routeReport.buyLandButtons[20].BlitScaled(&g_simScreen, 0x47, 0x215, 0);

        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(0x14, 0);
        flush_input();

        for (;;) {
            int x, y;
            g_simScreen.get_mouse_pos(&x, &y);
            prevSel = sel;
            if (x >= 0x3e && x < 0x8e && y >= 0x13b && y < 0x18b)
                sel = 1;
            else if (x >= 0xb7 && x < 0x107 && y >= 0xfe && y < 0x14e)
                sel = 0;
            else if (x >= 0x218 && x < 0x268 && y >= 0xfe && y < 0x14e)
                sel = 2;
            else if (x >= 0x293 && x < 0x2e3 && y >= 0x13b && y < 0x18b)
                sel = 3;
            else if (x >= 0x296 && x < 0x2e6 && y >= 0x214 && y < 0x255)
                sel = -2;
            else if (g_mode == 0 && pplCount > 8 && x >= 0x307 && x < 0x319 && y >= 0x40 && y < 0x65)
                sel = 9;
            else if (g_mode == 0 && pplCount > 8 && x >= 0x307 && x < 0x319 && y >= 0xcd && y < 0xf2)
                sel = 0xa;
            else
                sel = -1;
            if (prevSel != sel)
                goto redraw;
            ProcessMouseMessages();
            WaitWithTimeout(1, 0);
            if (g_mouseButton == 0) {
                if (PollInputState() == 0)
                    continue;
            }
            if (g_mouseButton != 0 && g_clickY > 0x118) {
                if (sel >= 0)
                    g_mode = sel - 1;
                if (sel == -2)
                    break;
                goto redraw;
            }
            if (sel == 9) {
                if (scroll > 0)
                    scroll--;
            } else if (sel == 0xa) {
                if (more)
                    scroll++;
            }
            if (g_clickY >= 0x101)
                break;
            if (g_mouseButton == 1) {
                int rw = Clamp((g_clickY - 0x68) / 0x11 + 1, 1, 0x13);
                if (g_clickX >= 0x190)
                    rw += 9;
                selRow = Clamp(rw, 1, 0x12);
            }
            if (g_mouseButton == MOUSE_RIGHT) {
                int a, b;
                a = Clamp((g_clickY - 0x68) / 0x11 + 1, 1, 0x13);
                b = selRow;
                if (g_clickX >= 0x190)
                    a += 9;
                a = Clamp(a, 1, 0x12);
                g_allHolesArray[0] = g_allHolesArray[b];
                memcpy(g_gameFairwayTargetPoints[0], g_gameFairwayTargetPoints[b], sizeof g_gameFairwayTargetPoints[0]);
                order[0] = order[b];
                if (a > b) {
                    for (k = b; k < a; k++) {
                        g_allHolesArray[k] = g_allHolesArray[k + 1];
                        memcpy(g_gameFairwayTargetPoints[k], g_gameFairwayTargetPoints[k + 1],
                            sizeof g_gameFairwayTargetPoints[0]);
                        order[k] = order[k + 1];
                        g_tileExtra[g_allHolesArray[k].teeX * 50 + g_allHolesArray[k].teeY] =
                            ((g_tileExtra[g_allHolesArray[k].teeX * 50 + g_allHolesArray[k].teeY] & ~TILE_HOLE_MASK)
                                | k);
                        g_tileExtra[g_allHolesArray[k].aimX * 50 + g_allHolesArray[k].aimY] =
                            ((g_tileExtra[g_allHolesArray[k].aimX * 50 + g_allHolesArray[k].aimY] & ~TILE_HOLE_MASK)
                                | k);
                    }
                }
                if (a < b) {
                    for (k = b; k > a; k--) {
                        g_allHolesArray[k] = g_allHolesArray[k - 1];
                        memcpy(g_gameFairwayTargetPoints[k], g_gameFairwayTargetPoints[k - 1],
                            sizeof g_gameFairwayTargetPoints[0]);
                        order[k] = order[k - 1];
                        g_tileExtra[g_allHolesArray[k].teeX * 50 + g_allHolesArray[k].teeY] =
                            ((g_tileExtra[g_allHolesArray[k].teeX * 50 + g_allHolesArray[k].teeY] & ~TILE_HOLE_MASK)
                                | k);
                        g_tileExtra[g_allHolesArray[k].aimX * 50 + g_allHolesArray[k].aimY] =
                            ((g_tileExtra[g_allHolesArray[k].aimX * 50 + g_allHolesArray[k].aimY] & ~TILE_HOLE_MASK)
                                | k);
                    }
                }
                g_allHolesArray[a] = g_allHolesArray[0];
                memcpy(g_gameFairwayTargetPoints[a], g_gameFairwayTargetPoints[0], sizeof g_gameFairwayTargetPoints[0]);
                order[a] = order[0];
                order[0] = 0;
                g_tileExtra[g_allHolesArray[a].teeX * 50 + g_allHolesArray[a].teeY] =
                    ((g_tileExtra[g_allHolesArray[a].teeX * 50 + g_allHolesArray[a].teeY] & ~TILE_HOLE_MASK) | a);
                g_tileExtra[g_allHolesArray[a].aimX * 50 + g_allHolesArray[a].aimY] =
                    ((g_tileExtra[g_allHolesArray[a].aimX * 50 + g_allHolesArray[a].aimY] & ~TILE_HOLE_MASK) | a);
                memset(&g_allHolesArray[0], 0, sizeof g_allHolesArray[0]);
                memset(g_gameFairwayTargetPoints[0], 0, sizeof g_gameFairwayTargetPoints[0]);
            }
            if (g_mouseButton != 0)
                goto redraw;
            break;
        }
    } else {
        if (graphsys)
            graphsys->set_sprite_scale(1, 1, 1);
    }

    for (i = 0; i <= 0x13; i++)
        rank[order[i]] = i;
    for (i = 0; i < 152; i++) {
        if (g_golferPool[i].holeIdx > 0)
            g_golferPool[i].holeIdx = rank[g_golferPool[i].holeIdx];
    }
    g_nextOpenHoleIndex = CLUBHOUSE_HOLE;
    for (i = 1; i < 19; i++) {
        if (g_allHolesArray[i].par == 0) {
            g_nextOpenHoleIndex = i;
            return;
        }
    }
}


#define TextLeft(s, x, y) do { if (s) g_simScreen.write_l(s, x, y, strlen(s)); } while (0)
#define TextRightIn(s, x, y, w) do { if (s) g_simScreen.RenderTextRightAlignedInWidth(s, x, y, w, strlen(s)); } while (0)

int RenderTractPurchaseScreen()
{
    int i;
    char redraw;
    int sel;
    SpriteObject *sp;
    int found, best;
    int j, tx, first, ty, by, mx, n, row, my, col, sz;
    int price[9];
    int ok;
    int cnt[0x17];
    int idx[9];

    ok = 0;
    first = 1;
    redraw = 1;
    sel = -1;

    for (;;) {
        int old, k;

        if (redraw) {
            int m;
            union {
                int score;
                int sy;
                char label[4];
            };

            redraw = 0;
            g_routeReport.buyLand.BlitScaled(&g_simScreen, 0, 0, 0);
            if (sel >= 0)
                g_routeReport.buyLandButtons[18].BlitScaled(&g_simScreen, g_hiliteX[sel], g_hiliteY[sel], 0);

            g_inSaleView = 1;
            RenderRoutingMapScreen();
            g_inSaleView = 0;
            g_simScreen.SetFontsForStyles(&g_font14, 0, 0, 0);

            for (n = 1; n < 19; n++) {
                int r, c, id, sx;
                SpriteObject *spr;

                if (!g_allHolesArray[n].par)
                    continue;

                sx = g_allHolesArray[n].teeX * 8 + 0x184;
                sy = g_allHolesArray[n].teeY * 8 + 0x68;
                TileToScreenCoords(g_allHolesArray[n].teeX, g_allHolesArray[n].teeY, &sx, &sy);
                TerrainMoveTo(sx, sy);

                id = 0x18d + Clamp(g_allHolesArray[n].par - 3, 0, 2);
                spr = GetFlicBankSprite(id, g_flicBankFrameLens[id] - 1, 0);
                g_objectSprite = spr;
                DrawSpriteImmediate(sx, sy, sy, 0x28, 0x28, spr, SPRITE_FULL_SCALE, PAL_TEE_MARKER);

                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(n, g_numString, 10));

                if (g_gameFairwayTargetPoints[n][2].x != -1) {
                    r = g_gameFairwayTargetPoints[n][2].x >> 10;
                    sx = r * 8 + 0x184;
                    c = g_gameFairwayTargetPoints[n][2].y >> 10;
                    sy = c * 8 + 0x68;
                } else if (g_gameFairwayTargetPoints[n][1].x != -1) {
                    r = g_gameFairwayTargetPoints[n][1].x >> 10;
                    sx = r * 8 + 0x184;
                    c = g_gameFairwayTargetPoints[n][1].y >> 10;
                    sy = c * 8 + 0x68;
                } else {
                    int qx = sx, qy = sy;
                    TileToScreenCoords(g_allHolesArray[n].aimX, g_allHolesArray[n].aimY, &sx, &sy);
                    if (g_mode)
                        DrawTextCenteredWithShadow(g_printBuffer, (sx + qx) / 2, (sy + qy) / 2 - 4, COLOR_PALE_YELLOW);
                    goto second;
                }
                TileToScreenCoords(r, c, &sx, &sy);
                TerrainLineTo(sx, sy, COLOR_WHITE, 2, 0xa);
                if (g_mode)
                    DrawTextCenteredWithShadow(g_printBuffer, sx, sy - 4, COLOR_PALE_YELLOW);

second:
                sx = g_allHolesArray[n].aimX * 8 + 0x184;
                sy = g_allHolesArray[n].aimY * 8 + 0x68;
                TileToScreenCoords(g_allHolesArray[n].aimX, g_allHolesArray[n].aimY, &sx, &sy);
                TerrainLineTo(sx, sy, COLOR_WHITE, 2, 0xa);

                id = g_currentThemeId + 0x189;
                spr = GetFlicBankSprite(id, 0, 3);
                g_objectSprite = spr;
                DrawSpriteImmediate(sx, sy, sy, 0x28, 0x28, spr, 2, g_currentThemeId + 0x60);
            }

            g_simScreen.SetFontsForStyles(&g_infoTitleFont, 0, 0, 0);
            DrawTextCenteredAtY("TRACTS FOR SALE", 0x190, 0x10, 0x80000000);
            g_simScreen.SetFontsForStyles(&g_font14, 0, 0, 0);
            TextLeft("Cash Reserve", 0x224, 0x107);

            strcpy(g_printBuffer, "§");
            FormatNumberWithCommas(g_gameCashHundreds * 100);
            DrawTextCenteredAtY(g_printBuffer, 0x2d0, 0x108, 0x80000000);

            sz = 0x10;
            g_simScreen.set_text_color(0x80000000, -1, 2, 2);

            j = 0;
            for (i = 0; i < 9; i++) {
                int r, c, bx, txx, tyy, v;

                score = 5;
                sp = &g_routeReport.buyLandButtons[i];

                col = i % 3;
                bx = col * 48 / 3 + 1;
                row = i / 3;
                by = row * 48 / 3 + 1;
                found = 0;
                memset(cnt, 0, sizeof cnt);

                for (r = 0; r < 0x10; r++) {
                    for (c = 0; c < 0x10; c++) {
                        if (g_gameTileTerrainKind[(c + bx) * 50 + r + by] != TERRAIN_OUT_OF_BOUNDS)
                            continue;
                        found++;
                        cnt[g_gameForSaleLandGrid[(c + bx) * 50 + r + by]]++;
                        if (g_rng.NextInt(3) == 0)
                            score += 1 << g_landPurchaseNum;
                        txx = (c + bx) * 8 + 0x180;
                        tyy = (by + r) * 8 + 0x64;
                        TileToScreenCoords(c + bx, by + r, &txx, &tyy);
                        if (c == 8 && r == 8) {
                            tx = txx;
                            ty = tyy;
                        }
                    }
                }

                if (found) {
                    v = score * 20 / 100;
                    score = v;
                    memcpy(label, "1", 2);
                    label[0] += i;
                    DrawCentered(&g_font16, label, tx - 7, ty - 4);
                } else {
                    v = score;
                }

                tx = row * 256 + (i < 6 ? 0x4e : 0x58);
                ty = col * 17 * 4 + 0x3e;
                if (first)
                    price[j] = v * 10;
                idx[j] = i;
                j++;

                if (!found) {
                    sp[9].BlitScaled(&g_simScreen, g_cellX[i], g_cellY[i], 0);
                    strcpy(g_printBuffer, "Already purchased.");
                    DrawTextWithColor(g_printBuffer, tx + 4, ty + 2, 0x80002108);
                } else {
                    strcpy(g_printBuffer, "Buy tract #");
                    strcat(g_printBuffer, itoa(i + 1, g_numString, 10));
                    strcat(g_printBuffer, ", ");
                    strcat(g_printBuffer, itoa(found / 10, g_numString, 10));
                    strcat(g_printBuffer, " acres ");
                    strcat(g_printBuffer, "of ");

                    for (m = 0; m < 3; m++) {
                        int k, mx2 = 0;
                        for (k = 0; k < 0x17; k++)
                            if (cnt[k] > mx2) {
                                best = k;
                                mx2 = cnt[k];
                            }
                        if (m == 1)
                            strcat(g_printBuffer, ", ");
                        else if (m == 2)
                            strcat(g_printBuffer, ", and ");
                        strcat(g_printBuffer, g_squareTypeTable[best].category == CATEGORY_TREES
                                                  ? &g_squareTypeTable[best].name[16]
                                                  : g_squareTypeTable[best].name);
                        cnt[best] = 0;
                    }

                    g_simScreen.set_text_color(0x80000000, -1, 2, 2);
                    g_simScreen.wrap(g_printBuffer, tx, ty, 0xa5);
                    {
                        Font *f = g_simScreen.m_fonts[0];
                        int lh = f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing;
                        ty = ty + lh * 2;
                    }
                    strcpy(g_printBuffer, "Price: \xa7");
                    FormatNumberWithCommas(price[i] * 100);
                    TextRightIn(g_printBuffer, tx + 0xa5, ty, 0x80000000);

                    if (sel == i)
                        sp->BlitScaled(&g_simScreen, g_cellX[i], g_cellY[i], 0);
                }
            }

            strcat(g_printBuffer, " I don't think I'll buy any land.\n");
            g_routeReport.buyLandButtons[20].BlitScaled(&g_simScreen, 0x47, 0x215, 0);
            if (sel == -2)
                g_routeReport.buyLandButtons[19].BlitScaled(&g_simScreen, 0x296, 0x215, 0);
            g_simScreen.RenderFullSurface(0);
            first = 0;
        }

        for (;;) {
            g_simScreen.get_mouse_pos(&mx, &my);
            old = sel;
            if (mx >= 0xf && mx < 0x307 && my >= 0x36 && my < 0x102) {
                int r = (my - 0x36) / 0x44;
                if (mx < 0x106)
                    sel = r;
                else if (mx >= 0x21a)
                    sel = r + 6;
                else if (mx >= 0x110 && mx < 0x20c)
                    sel = r + 3;
                else
                    sel = -1;
            } else if (mx >= 0x296 && mx < 0x2d6 && my >= 0x215 && my < 0x255) {
                sel = -2;
            } else {
                sel = -1;
            }
            if (old != sel) {
                redraw = 1;
                break;
            }
            ProcessMouseMessages();
            WaitWithTimeout(1, 0);
            if (g_mouseButton)
                break;
        }
        if (redraw)
            continue;

        if (g_mouseButton == 1) {
            k = sel;
            if (k == -1) {
                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                continue;
            }
        } else {
            if (g_mouseButton == MOUSE_RIGHT)
                break;
            k = sel;
        }
        if (k == -2)
            break;
        if (!price[k])
            continue;
        if (!CheckCanAfford(price[k])) {
            redraw = 1;
            continue;
        }

        {
            int r, c, bx, by;

            g_gameCashHundreds -= price[sel];
            g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_OTHER] -= price[sel];

            bx = sel % 3 * 48 / 3 + 1;
            by = sel / 3 * 48 / 3 + 1;
            for (r = 0; r < sz; r++) {
                for (c = 0; c < sz; c++) {
                    int ix = (c + bx) * 50 + r + by;
                    if (g_gameTileTerrainKind[ix] == TERRAIN_OUT_OF_BOUNDS)
                        g_gameTileTerrainKind[ix] = g_gameForSaleLandGrid[ix];
                }
            }
            NotifyEvent(0x140, sel);
        }
        ok = 1;
        break;
    }
    return ok;
}

int g_inSaleView;
SpriteObject *g_objectSprite;


int RenderHireEmployeeScreen()
{
    int sel, mx, my, frame, sy, odd, y, i, j, jj, hover, k;
    SpriteObject *fig;

    frame = 0;
    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);

    for (;;) {
        g_simScreen.get_mouse_pos(&mx, &my);

        if (my >= 0xab && my < 0xd3)
            sel = (my >= 0xbf);
        else if (my >= 0xf2 && my < 0x11a)
            sel = 2 + (my >= 0x106);
        else if (my >= 0x139 && my < 0x161)
            sel = 4 + (my >= 0x14d);
        else if (my >= 0x180 && my < 0x1a8)
            sel = 6 + (my >= 0x194);
        else
            sel = -1;

        g_hireScreen.image.draw_antialiased(&g_hireScreen.alpha, &g_simScreen, 0, 0, 0);

        AssetSurface *dst = &g_simScreen;
        if (sel == -1 && mx >= 0x229 && mx < 0x251 && my >= 0x1a8 && my < 0x1cf)
            hover = 1;
        else
            hover = 0;
        g_okButtonStates[hover].BlitScaled(dst, 0x229, 0x1a6, 0);

        g_pMenu->SetFontsForStyles(&g_infoTitleFont, 0, 0, 0);
        DrawTextCenteredAtY("HIRE AN EMPLOYEE", 0x179, 0x6b, 0x80000000u);
        g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);

        for (i = 0; i < 8; i++) {
            odd = i & 1;
            if (odd == 0) {
                switch (i / 2) {
                case 0:
                    y = 0x97;
                    DrawTextWithColor("Greeters...", 0xe7, y, 0x80000000u);
                    break;
                case 1:
                    y = 0xde;
                    DrawTextWithColor("Speed up play...", 0xe7, y, 0x80000000u);
                    break;
                case 2:
                    y = 0x125;
                    DrawTextWithColor("Weed Killers...", 0xe7, y, 0x80000000u);
                    break;
                case 3:
                    y = 0x16c;
                    DrawTextWithColor("Thirst quenchers...", 0xe7, y, 0x80000000u);
                    break;
                }
            }
            y += 0x14;
            strcpy(g_printBuffer, g_jobTitles[i]);
            strcat(g_printBuffer, ": ");
            FormatNumberWithCommas(g_jobWages[i] * 100);
            strcat(g_printBuffer, " per week");
            if (sel == i)
                g_simScreen.fill(0xe7, y - 2, 0x123, 0x10,
                    (graphsys->screen_mask() == 1) ? (int)0x8000fea0u : (int)0x80007f40u);
            DrawTextWithColor(g_printBuffer, 0xf6, y, 0x80000000u);
            if (odd)
                y += 8;
        }

        frame++;
        sy = 0xc0;
        for (j = 0, jj = 1; jj < 9; j++, jj += 2, sy += 0x46) {
            k = j;
            if (sel == jj)
                k += 4;
            fig = GetFlicBankSprite(k + 0x20e, frame % g_flicBankFrameLens[k + 0x20e], j);
            DrawSpriteImmediate(0x240, sy, y, 0x28, 0x28, fig, SPRITE_FULL_SCALE, k + 0x82);
        }
        g_simScreen.RenderFullSurface(0);
        ProcessMouseMessages();
        WaitWithTimeout(1, 0);
        if (g_mouseButton == 0) {
            if (PollInputState() == 0)
                continue;
            if (g_mouseButton == 0)
                return -1;
        }
        break;
    }

    WaitForInputRelease();
    if (sel < 0 || sel > 0xc)
        sel = -1;
    return sel / 2 * 3 + (sel & 1) + 1;
}



void RenderGolferPairingScreen()
{
    int slotOfRow[0x98];
    int slotOff, hoverIdx, mask, selCount, row, x, mode, *nextRow, slot, shown;
    int i, cx, cy, savedTraits, y, prevHover;

    mask = 0;
    selCount = 0;
    g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
    hoverIdx = -1;

    for (;;) {
        row = 0;
        g_pairingScreen.base.BlitScaled(&g_simScreen, 0, 0, 0);
        g_pMenu->SetFontsForStyles(&g_infoTitleFont, 0, 0, 0);
        DrawTextCenteredAtY("SELECT THE NEXT PAIR OF GOLFERS", 0x152, 0xe, 0x80000000);
        mode = 1;
        if (selCount == 2) {
            int first = -1;
            int n, t, second;

            for (i = 0; i < 16; i++) {
                if (mask & (1 << i)) {
                    if (first != -1)
                        second = slotOfRow[i];
                    else
                        first = slotOfRow[i];
                }
            }
            n = 5;
            for (i = 0; i < 5; i++)
                if ((g_golferTypeArray[g_golferPool[second].typeIdx].caps
                        ^ g_golferTypeArray[g_golferPool[first].typeIdx].caps)
                    & (1 << i))
                    n--;
            if (n < 2)
                mode = 2;
            if (n > 3)
                mode = 0;
        }

        for (i = 0, nextRow = slotOfRow; i < 0x98; i++) {
            slot = (g_simLatestGolferNum - i + 0x98) % 0x98;
            if (g_golferPool[slot].holeIdx == -1) {
                int k, h;
                int tr;
                char **cp;
                shown = slot;
                x = (row & 1) ? 0x14f : 6;
                y = 136 * (row / 2) + 0x32;
                g_pairingScreen.buttons[(mask & (1 << row)) ? 2 : hoverIdx == row].BlitScaled(&g_simScreen, x, y, 0);
                if (hoverIdx == row && !(mask & (1 << row)))
                    y--;
                if (mask & (1 << row)) {
                    AssetSurface *p = g_pMenu;
                    h = g_golferTypeArray[g_golferPool[slot].typeIdx].head + 72 * IsGolferMale(slot);
                    SpriteObject *s = &g_playerHeadBmpArray[3 * h + mode];
                    s->Draw(p, x - 6, y + 4, 1, 1, 1, 0);
                } else {
                    AssetSurface *p = g_pMenu;
                    h = g_golferTypeArray[g_golferPool[slot].typeIdx].head + 72 * IsGolferMale(slot);
                    SpriteObject *s = &g_playerHeadBmpArray[3 * h + 1];
                    s->BlitSpriteWithScaleState(p, x - 6, y + 4, 0.7f, 1, 1, 1, 0, 1);
                }
                *nextRow = slot;
                row++;
                nextRow++;

                g_pMenu->SetFontsForStyles(&g_font20, 0, 0, 0);
                g_printBuffer[0] = 0;
                AppendGolferName(slot, 0);
                DrawTextCenteredAtY(g_printBuffer, x + 0xd0, y + 9, 0x80000000);

                g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);
                strcpy(g_printBuffer, g_golferTypeArray[g_golferPool[slot].typeIdx].title);
                DrawTextCenteredAtY(g_printBuffer, x + 0x100, y + 0x28, 0x80000000);

                if (g_golferPool[slot].ageFlags & 0x08)
                    strcpy(g_printBuffer, "Single");
                if (g_golferPool[slot].ageFlags & 0x10)
                    strcpy(g_printBuffer, "Married");
                if (g_golferPool[slot].ageFlags & 0x20)
                    strcpy(g_printBuffer, "Divorced");
                if (g_golferPool[slot].ageFlags & 0x40)
                    strcpy(g_printBuffer, "Widowed");
                DrawTextCenteredAtY(g_printBuffer, x + 0x100, y + 0x68, 0x80000000);

                g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(GolferAge(slot), g_numString, 10));
                strcat(g_printBuffer, " years old");
                DrawTextCenteredAtY(g_printBuffer, x + 0x100, y + 0x48, 0x80000000);

                g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);
                tr = g_golferTypeArray[g_golferPool[slot].typeIdx].caps;
                k = 0;
                for (slotOff = 0; slotOff < 5; slotOff++)
                    if (tr & (1 << slotOff))
                        k++;
                k = 9 * (5 - k);
                slotOff = 0;
                for (cp = g_traitNames; (int)cp < (int)(g_traitNames + 5); cp++) {
                    if (tr & (1 << slotOff)) {
                        tr -= (1 << slotOff);
                        strcpy(g_printBuffer, *cp);
                        DrawTextWithColor(g_printBuffer, x + 0x8c, y + k + 0x24, 0x80000000);
                        y += 0x12;
                    }
                    slotOff++;
                }
            }
        }

        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(0xa, 0);
        flush_input();
        do {
            WaitWithTimeout(1, 0);
            prevHover = hoverIdx;
            g_simScreen.get_mouse_pos(&cx, &cy);
            if (cx >= 6 && cy >= 0x32 && cx < g_pairingScreen.buttons[0].m_width + 0x14f
                && cy < g_pairingScreen.buttons[0].m_width + 0x1ca)
                hoverIdx = (cx - 6) / 334 + 2 * ((cy - 0x32) / 136);
            else
                hoverIdx = -1;
            ProcessMouseMessages();
        } while (g_mouseButton == 0 && PollInputState() == 0 && hoverIdx == prevHover);
        if (hoverIdx != prevHover)
            continue;
        if (g_mouseButton == 0)
            break;
        if (hoverIdx == -1)
            break;
        if (hoverIdx < row) {
            if (selCount < 2 || (mask & (1 << hoverIdx))) {
                mask ^= (1 << hoverIdx);
                if (mask & (1 << hoverIdx))
                    selCount++;
                else
                    selCount--;
                continue;
            }
        }
        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
    }

    if (mask != 0) {
        int cnt = 0;
        for (i = 0; i < 16; i++)
            if (mask & (1 << i))
                cnt++;
        if (cnt != 2) {
            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
            return;
        }
        {
            int first = -1;
            int a, second;
            for (i = 0; i < 16; i++) {
                if (mask & (1 << i)) {
                    if (first != -1)
                        second = slotOfRow[i];
                    else
                        first = slotOfRow[i];
                }
            }
            if (second != shown) {
                SwapGolferSlots(first, shown);
                a = shown;
                shown ^= 1;
                SwapGolferSlots(second, shown);
                second = shown;
            } else {
                SwapGolferSlots(first, shown ^ 1);
                a = shown ^ 1;
            }
            g_golferPool[a].flags |= 0x20000;
            g_golferPool[second].flags |= 0x20000;
            PairGolfers(a, second);
        }
    }
}


#define TextRight(s, x, y) do { if (s) g_simScreen.write_right_l(s, x, y, strlen(s)); } while (0)

void RenderTournamentLeaderboard()
{
    int y;
    int mx;
    int colx;
    int idx;
    int any;
    int rank;
    int target;
    int my;
    int tot[152];

    memset(tot, -1, sizeof(tot));
    any = 0;
    g_pMenu->SetFontsForStyles(&g_boldArialFont, 0, 0, 0);

    {
        int i = 0;
        int h = 1;
        int *p = tot;
        signed char *sp = g_golferPool[0].holeScore;
        do {
            int hn = i / 2 + 1;
            if (hn < 19 && g_allHolesArray[hn].par) {
                signed char cv = sp[-2];
                if (cv)
                    any = 1;
                {
                    int t = 0;
                    for (h = 1; h < 19; h++) {
                        signed char pv = g_allHolesArray[h].par;
                        signed char sv;
                        if (!pv)
                            continue;
                        sv = sp[h];
                        if (!sv)
                            continue;
                        if (sp[-2] == h)
                            continue;
                        t += sv - pv;
                    }
                    *p = t;
                }
            }
            sp += 0x100;
            i++;
            p++;
        } while ((int)sp < (int)g_golferPool[152].holeScore);
    }

    rank = 0;
    colx = 6;
    if (any) {
        DrawPanelFrame(0, 8, 0x90, g_nextOpenHoleIndex * 22 + 0x10, 1);
        if (g_runModeFlags & GAME_CHAMPIONSHIP) {
            DrawTextCenteredAtY("LEADER BOARD of the", 0x48, 9, 0x80007ff0);
            switch (g_level) {
            case 0: strcpy(g_printBuffer, "SGA Qualifying School"); break;
            case 1: strcpy(g_printBuffer, "SGA Jr. Championship"); break;
            case 2: strcpy(g_printBuffer, "SGA Tour Championship"); break;
            case 3: strcpy(g_printBuffer, "SGA Open Championship"); break;
            }
            DrawTextCenteredAtY(g_printBuffer, 0x48, 0x15, 0x80007ff0);
            strcpy(g_printBuffer, "at ");
            AppendCourseName(-1);
            DrawTextCenteredAtY(g_printBuffer, 0x48, 0x21, 0x80007ff0);
        } else {
            DrawTextCenteredAtY("LEADER BOARD of", 0x48, 9, 0x80007ff0);
            strcpy(g_printBuffer, "the \xa7");
            strcat(g_printBuffer, itoa(g_winnersPurseK, g_numString, 10));
            strcat(g_printBuffer, ",000");
            DrawTextCenteredAtY(g_printBuffer, 0x48, 0x15, 0x80007ff0);
            g_printBuffer[0] = 0;
            strcat(g_printBuffer, itoa(g_careerPlayYear + 0x7d1, g_numString, 10));
            strcat(g_printBuffer, " ");
            strcat(g_printBuffer, g_mapCourseList[g_myWorldCourseIdx].name);
            strcat(g_printBuffer, " Open");
            DrawTextCenteredAtY(g_printBuffer, 0x48, 0x21, 0x80007ff0);
        }
        y = 0x2d;
    } else {
        int hno;
        int x;

        g_fadeScreen.draw_translucent(&g_simScreen, 0, 0, 0.25f, 0, 1);
        g_tournamentResult.header.draw_antialiased(&g_tournamentResult.headerAlpha, &g_simScreen, 0, 0, 0);
        g_pMenu->SetFontsForStyles(&g_infoTitleFont, 0, 0, 0);
        DrawTextCenteredAtY(g_golferPool[1].holeIdx ? "TOURNAMENT SCORES" : "TOURNAMENT RESULTS", 0x140, 0x10,
            0x80000000);
        g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);
        DrawTextWithColor("Ranking", 0x19, 0x32, 0x80000000);

        colx = 0x94;
        hno = 1;
        x = 0xaf;
        do {
            if (g_allHolesArray[hno].par) {
                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(hno, g_numString, 10));
                DrawTextCenteredAtY(g_printBuffer, x, 0x32, 0x80000000);
            }
            hno++;
            x += 0x1b;
        } while (hno < 19);

        DrawTextCenteredAtY("F", 0x29b, 0x32, 0x80000000);
        DrawTextWithColor("Prize", 0x2bc, 0x32, 0x80000000);
        y = 0x4d;
    }

    if (!g_winnersPurseK)
        g_winnersPurseK = g_nextOpenHoleIndex * 20 - 20;

    for (target = -100; target <= 100; target++) {
        for (idx = 0; idx < 152; idx++) {
            int hn = idx / 2 + 1;
            if (hn >= 19)
                continue;
            if (!g_allHolesArray[hn].par)
                continue;
            if (tot[idx] == target) {
                int col;
                int x;

                rank++;
                if (!any) {
                    if (rank < g_nextOpenHoleIndex) {
                        if (rank)
                            g_tournamentResult.prizeRow.draw_antialiased(&g_tournamentResult.prizeRowAlpha,
                                &g_simScreen, 0, y - 7, 0);
                    } else if (rank == g_nextOpenHoleIndex) {
                        g_tournamentResult.cutRow.draw_antialiased(&g_tournamentResult.cutRowAlpha, &g_simScreen, 0,
                            y - 8, 0);
                    } else {
                        g_tournamentResult.unpaidRow.draw_antialiased(&g_tournamentResult.unpaidRowAlpha, &g_simScreen,
                            0, y - 4, 0);
                    }
                }

                g_printBuffer[0] = 0;
                strcat(g_printBuffer, itoa(rank, g_numString, 10));
                strcat(g_printBuffer, ". ");
                AppendGolferName(idx, 0);

                if (any) {
                    strcat(g_printBuffer, " (");
                    if (target == 0) {
                        strcat(g_printBuffer, "E");
                    } else {
                        if (target > 0)
                            strcat(g_printBuffer, "+");
                        strcat(g_printBuffer, itoa(target, g_numString, 10));
                    }
                    strcat(g_printBuffer, ")");
                }

                if (idx == 1) {
                    col = any ? 0x800023e8 : 0x80001284;
                } else if (any) {
                    col = 0x80007fff;
                } else {
                    col = (rank < g_nextOpenHoleIndex) ? 0x80000000 : 0x80004210;
                }
                if (any) {
                    if (rank < 10)
                        x = colx + 1;
                    else
                        x = colx - 5;
                } else
                    x = 0x19;
                DrawTextWithColor(g_printBuffer, x, y, col);

                if (!any) {
                    signed char *sp;
                    Hole *hp;
                    mx = 0;
                    hp = g_allHolesArray + 1;
                    sp = &g_golferPool[idx].holeScore[1];
                    my = colx + 0x1b;
                    do {
                        if (*sp) {
                            int scoreColor;
                            mx += *sp;
                            g_printBuffer[0] = 0;
                            strcat(g_printBuffer, itoa(*sp, g_numString, 10));
                            scoreColor = 0x80000000;
                            if (*sp < hp->par)
                                scoreColor = 0x80006000;
                            if (*sp > hp->par)
                                scoreColor = 0x80000018;
                            DrawTextCenteredAtY(g_printBuffer, my, y, scoreColor);
                        }
                        hp++;
                        sp++;
                        my += 0x1b;
                    } while ((int)hp < (int)(g_allHolesArray + 19));

                    {
                        int totalColor = 0x80000000;
                        if (target > 0)
                            totalColor = 0x80006000;
                        if (target < 0)
                            totalColor = 0x80000018;
                        g_printBuffer[0] = 0;
                        strcat(g_printBuffer, itoa(mx, g_numString, 10));
                        DrawTextCenteredAtY(g_printBuffer, 0x29d, y, totalColor);
                    }

                    if (rank < g_nextOpenHoleIndex && !g_golferPool[1].holeIdx) {
                        g_pMenu->SetFontsForStyles(&g_font14, 0, 0, 0);
                        strcpy(g_printBuffer, "\xa7");
                        FormatNumberWithCommas(g_winnersPurseK);
                        strcat(g_printBuffer, ",000");
                        g_simScreen.set_text_color(0x80001284, -1, 2, 2);
                        TextRight(g_printBuffer, 0x30c, y);

                        if (idx == 1) {
                            NotifyEvent(0xe0, rank);
                            g_gameCashHundreds += g_winnersPurseK * 1000 / 100;
                            g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_OTHER] +=
                                (short)(g_winnersPurseK * 10);
                            if (rank <= 3)
                                g_winningPtsTotal += 4 - rank;
                            else
                                g_winningPtsTotal++;
                            if (rank == 1) {
                                if (g_nextOpenHoleIndex > FRONT_NINE_LAST_HOLE)
                                    TriggerAccomplishment(11, g_golferPool[1].x, g_golferPool[1].y);
                                if (g_nextOpenHoleIndex > MAX_HOLES)
                                    TriggerAccomplishment(0xf, g_golferPool[1].x, g_golferPool[1].y);
                                if (g_nextOpenHoleIndex > MAX_HOLES && g_winnersPurseK > 0x186a0)
                                    TriggerAccomplishment(g_currentThemeId + 0x11, g_golferPool[1].x,
                                        g_golferPool[1].animTick);
                            }
                        }
                        y += g_tournamentResult.prizeRow.m_height;
                        g_winnersPurseK = g_winnersPurseK * 2 / 3;
                    } else {
                        y += g_tournamentResult.unpaidRow.m_height;
                    }
                } else {
                    y += 0xb;
                }
            }
            if (y >= 0x225)
                break;
            if (rank >= (any ? 0x24 : 0x12))
                break;
        }
    }

    if (!any && !g_golferPool[1].holeIdx) {
        int cx, cy;
        char redraw;

        if (rank == 0x12 && g_nextOpenHoleIndex == CLUBHOUSE_HOLE) {
            g_tournamentResult.prizeFooter.draw_antialiased(&g_tournamentResult.prizeFooterAlpha, &g_simScreen, 0,
                y - 7, 0);
            y += g_tournamentResult.prizeFooter.m_height - 0x36;
        } else {
            g_tournamentResult.unpaidFooter.draw_antialiased(&g_tournamentResult.unpaidFooterAlpha, &g_simScreen, 0,
                y - 4, 0);
            y += g_tournamentResult.unpaidFooter.m_height - 0x33;
        }
        ConfigureAndPlaySound(SOUND_TADA, SOUND_FULL_VOLUME, 0, 0, 0);
        g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2dc, y, 0);
        g_simScreen.RenderFullSurface(0);
        WaitWithTimeout(0x78, 0);
        flush_input();
        redraw = 0;
        do {
            WaitWithTimeout(1, 0);
            g_simScreen.get_mouse_pos(&cx, &cy);
            if (cx >= 0x2dc && cx < 0x2dc + g_okButtonStates[0].m_width && cy >= y
                && cy < y + g_okButtonStates[0].m_height) {
                if (!redraw) {
                    redraw = 1;
                    g_okButtonStates[1].BlitScaled(&g_simScreen, 0x2dc, y, 0);
                    g_simScreen.RenderFullSurface(0);
                }
            } else {
                if (redraw) {
                    g_okButtonStates[0].BlitScaled(&g_simScreen, 0x2dc, y, 0);
                    redraw = 0;
                    g_simScreen.RenderFullSurface(0);
                }
            }
            ProcessMouseMessages();
        } while (!g_mouseButton && !PollInputState());
        g_reportScreenId = 0;
        WaitForInputRelease();
        g_runModeFlags &= ~GAME_TOURNAMENT;
        EndTournament();
        g_winnersPurseK = 0;
    }
}


char g_numString[16];

const int g_cellX[9] = {15, 15, 15, 272, 272, 272, 538, 538, 538};
const int g_cellY[9] = {54, 122, 190, 54, 122, 190, 54, 122, 190};
const int g_hiliteX[9] = {112, 208, 304, 208, 304, 400, 304, 400, 496};
const int g_hiliteY[9] = {392, 344, 296, 440, 392, 344, 488, 440, 392};

char *g_financeRowLabels[9] = {"Greens Fees", "Home Sites", "Food/Drink", "Build course", "Facilities", "Salaries",
    "Maint./Interest", "Other", "Total (\247)"};
char *g_courseColumnNames[13] = {"Yds", "Par", "Avg", "Time", "Fun", "+Len", "+Acc", "+Img", "Type", "Avg.Fee",
    "Revenue", "Profit", "Profit2"};
int g_courseColumnX[12] = {133, 171, 209, 255, 304, 350, 395, 443, 518, 603, 675, 752};
int g_mode = -1;

int g_unusedSlotState;
int g_infoSpare[4] = {0};
