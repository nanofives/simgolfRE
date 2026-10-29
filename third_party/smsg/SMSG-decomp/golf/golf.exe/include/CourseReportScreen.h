#ifndef COURSEREPORTSCREEN_H
#define COURSEREPORTSCREEN_H

#include "crt.h"
#include "Golfer.h"
#include "Screen.h"
#include "Surface.h"
#include "InputPoll.h"
#include "Geometry.h"
#include "GolferStats.h"
#include "FloatingText.h"
#include "SaveGame.h"
#include "GameFlags.h"
#include "TerrainGrid.h"
#include "TournamentLeaderboard.h"
#include "InfoScreens.h"
#include "InfoScreenAssets.h"

void RenderHoleInfoOverlay(int mode);

extern char *g_courseTierNames[4];
extern char *g_holeTypeNames[8];
extern char *g_courseColumnNames[13];
extern int g_courseColumnX[12];
extern const int g_courseColumnMarkX[12];
extern unsigned int g_reachableAmenityFlags;

#endif
