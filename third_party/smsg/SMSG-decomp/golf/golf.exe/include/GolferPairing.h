#ifndef GOLFERPAIRING_H
#define GOLFERPAIRING_H

#include "crt.h"
#include <ctype.h>
#include "Golfer.h"
#include "GolferSpawn.h"
#include "GolferStats.h"
#include "DialogFrame.h"
#include "SaveGame.h"
#include "Scorecard.h"
#include "TerrainGrid.h"
#include "FloatingText.h"
#include "RandomSource.h"
#include "SoundEffects.h"

extern int g_openingDayStoryId;

void SwapGolferSlots(int first, int second);

void PairGolfers(int golfer, int partner);

#endif
