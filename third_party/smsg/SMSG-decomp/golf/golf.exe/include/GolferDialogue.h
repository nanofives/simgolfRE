#ifndef GOLFERDIALOGUE_H
#define GOLFERDIALOGUE_H

#include "crt.h"
#include "Golfer.h"
#include "GameFlags.h"
#include "FloatingText.h"
#include "TerrainGrid.h"
#include "TerrainInfluence.h"
#include "CourseGrid.h"
#include "CourseScript.h"
#include "SaveGame.h"
#include "ShotTrail.h"
#include "RandomSource.h"
#include "Geometry.h"
#include "GolferStats.h"
#include "LoadingScreen.h"
#include "DialogFrame.h"
#include "SceneryNames.h"
#include "SoundEffects.h"
#include "Plot.h"
#include "VipEvents.h"
#include "StringPool.h"
#include "Critter.h"
#include "ClubName.h"

extern int g_gameSpeakerIsPartner;

extern int g_lastShownStoryMsgTick;

extern int g_actionArgs[65];

struct HeadPreset {
    signed char skinTone;
    unsigned char pad01[2];
    signed char hairColor;
    char line[64];
};
extern HeadPreset g_headPresets[40];

extern signed char g_typeLineIds[];

extern char g_dialogueNum[12];

#endif
