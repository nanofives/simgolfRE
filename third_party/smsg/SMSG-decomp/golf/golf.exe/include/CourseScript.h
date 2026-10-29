#ifndef COURSESCRIPT_H
#define COURSESCRIPT_H

#include "GameFlags.h"
#include "IsoView.h"
#include "CourseGrid.h"
#include "TerrainGrid.h"
#include "Geometry.h"
#include "BuildingSearch.h"
#include "SoundEffects.h"
#include "RandomSource.h"
#include "Golfer.h"
#include "FloatingText.h"
#include "SaveGame.h"

void AppendScriptCommand(int row, int col, int cmd);
void PlayScriptStep();

extern int g_courseScriptPlayIndex;
extern int g_scriptCount;
extern int g_scriptCommandCellRows[256];
extern int g_scriptCommandCellCols[256];
extern int g_scriptCmd[256];
extern int g_wantedCamTileX;
extern int g_wantedCamTileY;

extern unsigned char g_tileAnimFrame[GRID_SIZE * GRID_SIZE];
extern int g_editedTileColIdx;

#endif
