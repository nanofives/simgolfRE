#ifndef VIPEVENTS_H
#define VIPEVENTS_H

#include "crt.h"
#include "Golfer.h"
#include "FloatingText.h"
#include "SaveGame.h"
#include "TerrainGrid.h"
#include "BuildingSearch.h"
#include "GolferStats.h"
#include "SceneryNames.h"
#include "DialogFrame.h"
#include "SoundEffects.h"
#include "Geometry.h"
#include "RandomSource.h"
#include "ShotTrail.h"
#include "Plot.h"

extern int g_allowedExpansionRequestFlag;

void AppendPartnerNote(int enjoyment, int mode);

void ProcessVIPGolferEvent(int gi);

#endif
