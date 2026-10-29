#ifndef GOLFERPANEL_H
#define GOLFERPANEL_H

#include "Golfer.h"

int RenderGolferPortrait(int golfer, int x, int y, int mode);

int GetGolferMoodBalance(int golfer);

void ReplaceGolferNameTokens(int golfer);

void RenderGolferInfoPanel(int shot, int row);

extern char *g_ordinals[20];
extern char *g_clubLetters[14];
extern int g_infoPanelHover;
extern int g_hot;
extern int g_bgEnabled;

#define INFO_BUTTON_SNAPSHOT         2
#define INFO_BUTTON_STORY            3
#define INFO_BUTTON_CLOSE            4
#define INFO_BUTTON_NEXT_CHAPTER     5

#endif
