#ifndef WILDLIFEUPDATE_H
#define WILDLIFEUPDATE_H

#include "crt.h"
#include "Critter.h"
#include "SaveGame.h"
#include "GameFlags.h"
#include "Golfer.h"
#include "Geometry.h"
#include "TerrainGrid.h"
#include "Plot.h"
#include "IsoView.h"
#include "RenderQueue.h"
#include "FlicBanks.h"
#include "FLCPalette.h"
#include "ScreenText.h"
#include "Wildlife.h"

extern SpriteObject *g_curSprite;

void UpdateAndRenderWildlife();

#endif
