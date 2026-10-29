#ifndef PATHFINDING_H
#define PATHFINDING_H

#include "crt.h"
#include "Golfer.h"
#include "RandomSource.h"
#include "Screen.h"
#include "Geometry.h"
#include "TerrainGrid.h"
#include "CourseGrid.h"
#include "BuildingSearch.h"
#include "IsoView.h"
#include "ScreenText.h"
#include "GameFlags.h"
#include "SaveGame.h"
#include "InputPoll.h"

extern unsigned char g_pathFindDist[2500];
extern short g_scanOpenListX[1024];
extern short g_scanOpenListY[1024];

int GetCardinalDirection(int dx, int dy);
int FindPathDirection(int x1, int y1, int x2, int y2, int golfer);

#endif
