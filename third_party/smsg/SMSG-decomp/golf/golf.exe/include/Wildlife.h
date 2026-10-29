#ifndef WILDLIFE_H
#define WILDLIFE_H

#include "crt.h"
#include "Critter.h"
#include "RandomSource.h"
#include "CourseGrid.h"
#include "TerrainGrid.h"
#include "ToolbarSelect.h"
#include "SaveGame.h"
#include "Golfer.h"
#include "Plot.h"
#include "AssetSurface.h"
#include "FloatingText.h"
#include "ScreenText.h"
#include "DialogFrame.h"
#include "SetupScreens.h"

int CalculateRandomShotVariance();
int SpawnCritter(int x, int y, unsigned char kind);
void PlayCritterCall(int kind, int x, int y);
int ValidateAndTrimString(char *s);
void ShowSaveGameDialog(void);
void TrySpawnWildlifeHabitat(int x, int y, int type);
int CalculateHoleTalentStrokeGap(int hole, int missingTalents);

#endif
