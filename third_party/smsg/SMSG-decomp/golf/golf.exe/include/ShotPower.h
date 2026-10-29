
#ifndef SHOTPOWER_H
#define SHOTPOWER_H

#include "Golfer.h"
#include "TerrainGrid.h"
#include "Geometry.h"
#include "BuildingSearch.h"

int CalculateBallRollDistance(int speed);
int CalculateBallCarryDistance(int speed, int height);
int CalculateShotPower(int distance, int height, int mode);
int CalculateGolferMaxDistance(int golfer);

extern int g_gameMemoDists[10];
extern int g_shotMemoHgt[10];
extern int g_shotPowerMemoSpeed[10];
extern int g_shotCacheNext;


#endif
