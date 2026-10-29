#ifndef HOLESTATISTICS_H
#define HOLESTATISTICS_H

#include "crt.h"
#include "Golfer.h"
#include "Plot.h"
#include "FloatingText.h"
#include "SaveGame.h"
#include "ShotTrail.h"
#include "TerrainGrid.h"
#include "DialogFrame.h"
#include "SoundEffects.h"
#include "GolferStats.h"
#include "TournamentLeaderboard.h"
#include "Geometry.h"

#include "HoleTypeMessage.h"

extern int g_waitListLength;

void CalculateAllHoleStatistics(void);

#endif
