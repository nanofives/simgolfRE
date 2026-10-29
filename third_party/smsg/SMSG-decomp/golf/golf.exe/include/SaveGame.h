#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "crt.h"
#include "GameFlags.h"
#include "Golfer.h"
#include "FloatingText.h"
#include "CourseGrid.h"
#include "BuildingSearch.h"
#include "Geometry.h"
#include "StringPool.h"
#include "Scorecard.h"
#include "TerrainInfluence.h"
#include "DialogFrame.h"
#include "ShotTrail.h"
#include "Critter.h"
#include "SceneryNames.h"
#include "CourseStats.h"

int TransferSaveBlock(void *block, unsigned int count);
void TransferGameState(int reading);
void SaveGame(const char *name);
void SaveCourseToFile();
void LoadCourseFromFile(int stage);
void LoadSavedGame(const char *name, int refreshUi, int headerOnly, int fromThemes);
void TransferCourseState(int reading);

extern int g_gameCurrentHandle;
extern int g_sveFileLoadFlag;

void ResetAllGameState(int stage);
void InitNewGameData();
void LoadThemeAssets();

extern int g_courseId;
extern int g_holeAwardCount;
extern int g_homesSold;
extern int g_winningPtsTotal;
extern int g_celebHomesBought;
extern int g_previousSGAMark;
extern int g_unlockedBldgTypeLimit;
extern short g_monthlySkillScore[500];
extern short g_moneyLine[500];
extern short g_funRatingHistPoints[500];
extern short g_gameMembershipTrendLine[500];
extern int g_calculatedGreenFee;
extern unsigned int g_knownEventsLowMask;
extern unsigned int g_knownEventsHighMask;
extern unsigned int g_freeLandmarks;
extern unsigned int g_simPostedHoleCategories;
extern int g_simLatestGolferNum;
extern int g_shownLandmarkBitmask;
extern int g_pendingProIndex;
extern int g_ceoLadderRung;
extern int g_curLandmarkTotal;
struct StatisticsRow {
    int head[8];
    int holeTotal[19];
    int holeCount[19];
};
extern StatisticsRow g_skillClassStatRows[32];
extern int g_top10CoursePoints[10][6];
struct HoleLandingPoint {
    int x;
    int y;
};
extern HoleLandingPoint g_gameFairwayTargetPoints[19][3];
extern signed char g_golferSkillGrades[32];
extern unsigned char g_miscGameFlags[8];
extern BackgroundActor g_backgroundActors[4];
extern char g_phrasesPerGolferType[0x19a28];
extern int g_winnersPurseK;
extern signed char g_gameForSaleLandGrid[];
struct Accomplishment {
    char name[0x28];
    int awarded;
    int tour;
};
extern Accomplishment g_accomp[22];
extern int g_nextGolferOrder;
extern int g_moneyWarnCnt;
extern int g_curTournPrepBits;
extern signed char g_demolishPreviousRecs[2500];
extern signed char g_gameDemolitionRefunds[2500];
extern unsigned int g_configBitMask;
extern GolferType g_gameGolferTypesBak[84];
extern char g_phrasesPerGolferTypeSave[0x19a28];

extern int g_toolbarView;
extern int g_selectedBuildingKind;
extern int g_paintTerrain;
extern char g_numberBuf[];

#define TOOLBAR_BUILD_COURSE         0
#define TOOLBAR_BUILDINGS            1
#define TOOLBAR_GOLFERS              2
#define TOOLBAR_SHOT                 3
#define TOOLBAR_PEOPLE               4
#define TOOLBAR_CLOSED               5
#define PREF_SHOW_NAMES              0x01
#define PREF_SHOW_THOUGHTS           0x10
#define TOP_COURSE_TOTAL             5

#endif
