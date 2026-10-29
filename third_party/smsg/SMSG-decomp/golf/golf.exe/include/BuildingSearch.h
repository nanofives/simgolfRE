#ifndef BUILDINGSEARCH_H
#define BUILDINGSEARCH_H

#include "Plot.h"
#include "TerrainGrid.h"
#include "TerrainView.h"
#include "CourseGrid.h"
#include "Employee.h"

struct FacilityType {
    char name[0x10];
    signed char side;
    char pad11;
    short cost;
};

int ValidateBuildingPlacement(int row, int col, int span, int type);
int FindNearestBuildingOfType(int type, int wx, int wy);
void FindNearestBench(int x, int y, int maxDist);
int FindBuildingAtTile(int tx, int ty);
int PlaceBuildingOnTerrain(int row, int col, int type, int flags);
void DemolishBuilding(int slot);

extern int g_closestDistanceFound;
extern int g_destRestX;
extern int g_destRestY;
extern FacilityType g_facilityTypes[20];
extern int g_buildingLevelPerKind[20];
extern int g_simOpenFacilityLvl[20];
extern int g_currentFeesOption;

struct YearLedger {
    short v[10];
};
extern YearLedger g_annualStatsRecords[100];
extern int g_gameCashHundreds;
extern short g_careerPlayYear;
extern signed char g_simGolferWalkCostMap[][50];
extern int g_curBuildingTurns;
int CalculateLotScore(int row, int col);

#define LEVEL_UPGRADED               2
#define LEVEL_DELUXE                 3
#define LEDGER_OTHER                 7
#define LEDGER_BUILD_COURSE          3
#define LEDGER_SALARIES              5
#define START_MONEY                  1000

#endif
