#ifndef SHOTTRAIL_H
#define SHOTTRAIL_H

#include "Golfer.h"
#include "FloatingText.h"
#include "TerrainGrid.h"
#include "Geometry.h"
#include "Plot.h"

struct Trail {
    short golferId;
    short partner;
    short count;
    short pad06;
    int x[32];
    int y[32];
    int z[32];
    Golfer golfer;
    Golfer partnerCopy;
};

struct Divot {
    int originX;
    int originY;
    int x;
    int y;
    int z;
    int bearing;
    int speed;
    int climb;
    int spawnTick;
};

void CheckGolferShotDeviation(int slot);
void AddGolferTrailPoint(int slot);
void StartShotTrail(int golfer);
void SpawnGolferDivot(int golfer);
void UpdateDivots(void);

extern Trail g_ballPathSlots[16];
extern Divot g_tornDivots[256];
extern int g_forceBits;
extern int g_selectedGolferIndex;
#define SIMFOTO_NOT_SAVED    0x100
extern int g_watchedPlayerIdx;

void DisplayShotCommentary(int slot, int distPin, int yards, int quiet);

#endif
