#ifndef SHOTDIFFICULTY_H
#define SHOTDIFFICULTY_H

#include "crt.h"
#include "Golfer.h"
#include "TerrainGrid.h"
#include "Geometry.h"
#include "CourseGrid.h"

int CalculateShotDifficulty(int sx, int sy, int tx, int ty, int maxPower, int curve, int force);

#endif
