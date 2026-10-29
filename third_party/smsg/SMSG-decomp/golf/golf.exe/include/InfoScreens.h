#ifndef INFOSCREENS_H
#define INFOSCREENS_H

#include "crt.h"
#include "BuildingSearch.h"
#include "ClubReports.h"
#include "CourseGrid.h"
#include "FloatingText.h"
#include "GameFlags.h"
#include "Geometry.h"
#include "Golfer.h"
#include "GolferStats.h"
#include "ImageSet.h"
#include "InfoScreenAssets.h"
#include "InputPoll.h"
#include "Plot.h"
#include "SaveGame.h"
#include "Screen.h"
#include "SoundEffects.h"
#include "Surface.h"

void RenderEndOfYearReport();

void RenderKeyboardShortcutsScreen();

void RenderFinancialReportScreen();

int GetHoleCountTier(int n);

extern int g_clubMemberSum;
extern const char *kMonth[];
extern char *g_financeRowLabels[9];

#endif
