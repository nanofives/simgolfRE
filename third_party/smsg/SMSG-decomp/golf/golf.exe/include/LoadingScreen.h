
#ifndef LOADINGSCREEN_H
#define LOADINGSCREEN_H

#include "crt.h"
#include "Golfer.h"
#include "SaveGame.h"
#include "RandomSource.h"
#include "ImageSet.h"
#include "Sprite.h"
#include "Screen.h"
#include "Plot.h"
#include "TextWrap.h"
#include "ScreenText.h"
#include "DialogFrame.h"
#include "ShotTrail.h"
#include "FloatingText.h"
#include "BuildingSearch.h"
#include "ClubReports.h"
#include "TerrainGrid.h"
#include "Geometry.h"
#include "GameFlags.h"

extern AssetSurface g_introBackdrop;
extern AssetSurface g_saveGameSnapArt;

void DrawTerrainLineWrapper(int x0, int y0, int x1, int y1, int color, int width, int alpha);
void ShowLoadingScreen(int percent);
void ShowSkillPointsIntro();
void ProcessMembershipUpgrade(int golfer);
int CheckCanAfford(int cost);
void FormatGolferMessage(int golfer, int priority);


extern short g_skillMasks[5];

extern char *g_membershipName[];

extern char g_simGolferStatusTxt[];
extern char g_statusPriority;

int RenderSkillPointsScreen(char *name, int skillMask, int points, int golfer, int xOff);

enum LoadingScreenUnrecovered {
    LOADING_SCREEN_UNRECOVERED_1
};

#endif
