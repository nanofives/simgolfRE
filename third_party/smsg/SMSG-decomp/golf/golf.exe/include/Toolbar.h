#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "Sprite.h"

extern short g_mainBarButtonPos[][2];
extern short g_lowerLeftTabPos[][2];
extern short g_zoomMapIconPos[2];
extern short g_unzoomMapIconPos[2];
extern short g_pauseIconPos[2];
extern signed char g_mainBarTabImage[];
extern char *g_preferenceNames[6];

extern signed char g_terrainToolbarTerrain[];
extern short g_terrainToolbarPos[][2];
extern int g_themeToolbarPos[][3][2];
extern int g_panelHoverButton;
extern int g_panelHoverTicks;

extern short g_elevationToolbarPos[][2];
extern int g_toolbarClickResult;

extern short g_buildingToolbarPos[][2];
extern int g_rowHoverButton;

extern int g_peopleListGolfer[0x98];
extern int g_peopleListScroll;
extern int g_employeeListActive;
extern int g_staffHireButtonIdle;
extern short g_employeePanelPos[][2];
extern int g_employeeListPick;
extern int g_employeeListScroll;
extern int g_employeeListIds[];
extern int g_employeeListCount;

extern short g_peopleToolbarPos[][2];

extern int g_toolbarHoverButton;
extern int g_toolbarHoverTicks;
extern int g_pauseBlinkTicks;
extern int g_selectedRowButton;
extern SpriteObject *g_rowIconSprite;

int RenderToolbarRow(int cx, int baseId, int count, int idStep, int idBase, int sz, int mx, int my, int scale,
    int shown, int flagged);
void BuildAndShowPreferencesDialog();
void RenderToolbarTooltip(int x, int y);
int FindNearestToolbarButton(int x, int y);
int HitTestTerrainToolbar(int x, int y);
int HitTestElevationToolbarButtons(int x, int y);
int HandleElevationToolbarClick(int x, int y);
void RenderElevationToolbarWithTooltip(int x, int y);
int HitTestFacilityUpgradeButtons(int x, int y);
int HitTestBuildingToolbar(int x, int y);
int HandleBuildingToolbarClick(int x, int y);
void RenderBuildingToolbar(int x, int y);
int HitTestGolferPairingGrid(int x, int y);
int HitTestPeopleToolbarButtons(int x, int y);
int HitTestEmployeePanel(int x, int y);
int HandleEmployeeListClick(int x, int y);
void RenderEmployeePanel(int x, int y);

#endif
