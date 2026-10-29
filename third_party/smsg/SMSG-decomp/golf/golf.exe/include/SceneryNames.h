#ifndef SCENERYNAMES_H
#define SCENERYNAMES_H

#include "crt.h"
#include "Golfer.h"
#include "Plot.h"
#include "CourseGrid.h"
#include "TerrainGrid.h"

struct Placed {
    short x;
    short y;
    short owner;
    short pad06;
};

void AppendHoleName(int hole);
int FindNearestHole(int x, int y);
int IsNeighborWalkable(int row, int col, int dir);
void AppendLandmarkName(int kind, int verbose);
int GetSceneryName(int row, int col, int type);
int GetMemberNameAtPosition(int row, int col, int append);

extern Placed g_namedSceneryArray[100];
extern char *g_par3HoleNames[20];
extern char *g_par4HoleNames[20];
extern char *g_par5HoleNames[20];

#endif
