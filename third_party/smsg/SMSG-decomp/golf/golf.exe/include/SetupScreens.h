#ifndef SETUPSCREENS_H
#define SETUPSCREENS_H

int FileExists(const char *path);

int SaveGolferToTheme(int type, int golfer, int champ);

int HitTestGolferCustomizationButton(int x, int y);

int HitTestCircularButtonGrid(int x, int y);

int ShowPaginatedCircularListDialog(int sex);

extern int g_headPageTopIndex;

int ShowDifficultySelectScreen();

int ShowPickAProScreen();

int ShowLoadGameScreen(int browsing, int courseMode);

struct Font;
extern Font g_kleptoBigFont;
extern char g_numText[12];

#include "Tournament.h"
int ShowListDialogWithParams(const char *text, int x, int y);

struct GolferType;

void RenderGolferCustomizationScreen(int type, int golfer);

extern GolferType g_golferTypeBackup;

extern char *g_talentNames[3];
extern char *g_traitNames[5];

extern char *g_quoteSituationNames[];
#endif
