#ifndef GOLFERSHOT_H
#define GOLFERSHOT_H

#include "Golfer.h"
#include "GameFlags.h"
#include "ToolbarSelect.h"

extern int g_debugShotGolfer;
extern int g_shotTargetExact;
extern int g_shotDoubts;
extern int g_hazardCommentary;

extern int g_simBallStopPosX;
extern int g_simBallStopPosY;
extern int g_simGolferFlags;
extern int g_debugBestShotGolfer;

void SimulateBallPhysics(int gi, int tx, int ty, int spin);
int CalculateBestShot(int gi);

void PlanGolferShot(int gi, int mode, int px, int py, int shape);

void ClearGolferSlotAndNamedScenery(int gi);

#endif
