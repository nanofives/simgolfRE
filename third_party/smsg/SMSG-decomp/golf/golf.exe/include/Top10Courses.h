#ifndef TOP10COURSES_H
#define TOP10COURSES_H

#include "crt.h"
#include "Golfer.h"
#include "Screen.h"
#include "Plot.h"
#include "TextWrap.h"
#include "FloatingText.h"
#include "SpriteAssets.h"
#include "ScreenText.h"
#include "SaveGame.h"
#include "ToolbarSelect.h"
#include "BuildingSearch.h"
#include "Scorecard.h"

void RenderTop10CoursesScreen(void);

extern char *g_greatCourseNames[10];
extern int g_ratingWealth;
extern int g_ratingUnawarded;

#endif
