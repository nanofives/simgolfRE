#ifndef TUTORIALMESSAGE_H
#define TUTORIALMESSAGE_H

#include "crt.h"
#include "DialogFrame.h"
#include "FloatingText.h"
#include "GameFlags.h"
#include "Golfer.h"
#include "IsoView.h"
#include "Plot.h"
#include "SaveGame.h"

extern int g_nextTutStepIdx;
extern int g_paintedCameraTileX;
void SetupTutorialMessage();

#define TUTORIAL_ZOOM_WIDE 2

#endif
