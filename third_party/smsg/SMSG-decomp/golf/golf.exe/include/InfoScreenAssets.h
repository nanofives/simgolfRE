#ifndef INFOSCREENASSETS_H
#define INFOSCREENASSETS_H

#include "Sprite.h"

struct SpritePair {
    SpriteObject image;
    SpriteObject alpha;
};

struct ReportPanels {
    SpriteObject top;
    SpriteObject topAlpha;
    SpriteObject middle;
    SpriteObject middleAlpha;
    SpriteObject bottom;
    SpriteObject bottomAlpha;
};

struct CourseReport {
    SpriteObject header;
    SpriteObject headerAlpha;
    SpriteObject holeRow;
    SpriteObject holeRowAlpha;
    SpriteObject totalRow;
    SpriteObject totalRowAlpha;
    SpriteObject footer;
    SpriteObject footerAlpha;
    SpriteObject marks[8];
    SpriteObject cells[12][3];
};

struct MemberRoster {
    SpriteObject image;
    SpriteObject alpha;
    SpriteObject buttons[18];
};

struct RouteScreens {
    SpriteObject bottom;
    SpriteObject tabs[4];
    SpriteObject cells[12];
    SpriteObject buyLand;
    SpriteObject buyLandButtons[21];
};

struct PairScreen {
    SpriteObject base;
    SpriteObject buttons[4];
};

struct TournamentResult {
    SpriteObject header;
    SpriteObject headerAlpha;
    SpriteObject prizeRow;
    SpriteObject prizeRowAlpha;
    SpriteObject cutRow;
    SpriteObject cutRowAlpha;
    SpriteObject unpaidRow;
    SpriteObject unpaidRowAlpha;
    SpriteObject prizeFooter;
    SpriteObject prizeFooterAlpha;
    SpriteObject unpaidFooter;
    SpriteObject unpaidFooterAlpha;
};

extern Font g_infoTitleFont;
extern Font g_font20;
extern Font g_font18;
extern Font g_font16;
extern Font g_font14;

extern SpriteObject g_okButtonStates[5];
extern SpriteObject g_fadeScreen;
extern SpritePair g_financialReport;
extern SpritePair g_sgaReport;
extern SpritePair g_graphReport;
extern SpritePair g_hireScreen;
extern ReportPanels g_holeByHolePanels;
extern ReportPanels g_commentsSheet;
extern ReportPanels g_scoreRecords;
extern CourseReport g_courseOverview;
extern MemberRoster g_memberRoster;
extern RouteScreens g_routeReport;
extern PairScreen g_pairingScreen;
extern TournamentResult g_tournamentResult;

void ShowCreditsScreen();
void LoadInfoScreenAssets();
void CleanupAllFontsAndSprites();

#endif
