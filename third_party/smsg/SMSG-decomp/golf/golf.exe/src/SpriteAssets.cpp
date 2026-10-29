#include <sys/stat.h>
#include "Golfer.h"
#include "SpriteAssets.h"
#include "Plot.h"

SpriteObject g_themePaintImgs[80], g_notReadPic, g_gameDummyFrameArray[16], g_simUnusedSpritePair[2],
    g_gameGolferBodyGfx[16], g_juniorsBarrelArt, g_simUnreferencedBitmaps[20], g_tinyBallPicture, g_groundShadowPixel,
    g_bigGolfBallDot, g_speechStripTile, g_talkBubblePointSpr, g_selectHaloSprite;

SpriteObject g_themeFrame[4][8], g_themeAlphaFrames[4][8], g_bulldozeDimCursor, g_bulldozeLitCursor, g_bmpReserved,
    g_simReservedGfx[4], g_gameReservedSpriteObjs[16], g_headBodyBgPic;

SpriteObject g_simUnreferencedImages[32], g_courseBigSprs[12], g_bubblePieces[40], g_homePlotGroundPics[8],
    g_playerHeadBmpArray[432], g_allExpressionHeads[120], g_simHoleMarkArt[72];

SpriteObject g_simFlowerNeighborGfx[20], g_courseCliffArt[224], g_transPopUpButtonBmps[8], g_transPopUpMaskSprites[8],
    g_amenityPanelParts[12], g_amenitiesMaskSprites[4];

SpriteObject g_toolPanelBgImages[16], g_toolbarPanelBackings_A[16], g_themeTerrainButtonBmps[96],
    g_lowerLeftBarParts[42], g_lowerLeftBarAlphaSprites[28], g_buildingPnlFrames[24], g_amenitiesIcons[30],
    g_peopleButtonBmps[44], g_joeCoolToolbarBmpsAlpha[2];

SpriteObject g_staffPanelSpr[40], g_gameGlowingPanelDiscs[10], g_simElevationPanelButtonFrames[15],
    g_memberWindowSprite[17], g_trophyPartCells[70], g_trophiesMasksBmps[70];

SpriteObject g_mapSiteMarkerPics[32], g_mapMarkerButtonAlphas[32], g_simTopBoxes[4], g_courseInfoBoxPicsAlphaMask[4],
    g_s_courseInfo[4], g_cgScreenSprites[50];

SpriteObject g_gameGolferStatsBtnSprs[30], g_golferStatsBarCellsMask[30], g_popupEdgeBmps[36],
    g_transPopupCellsAlpha[36], g_infoBtnImages[39], g_infoButtonBitmapMask[39];

SpriteObject g_checkBoxFrames[3], g_siteMapSprs[16], g_regionAlphaFrames[16], g_bldgSprites[10][4],
    g_s_transPopUpSprites[9], g_messageIconPics[5], g_speakerIconsAlpha[5], g_simTitlePageGraphics[14];

AssetSurface g_golferHeadPage;

#include "SaveGame.h"
#include "TerrainGrid.h"
#include "FloatingText.h"
#include "BuildingSearch.h"
#include "CourseGrid.h"
#include "GolferStats.h"
#include "TerrainInfluence.h"
#include "GolferDialogue.h"
#include "ToolbarSelect.h"
#include "MainLoop.h"
#include "CourseStats.h"
#include "Screenshot.h"
#include "CourseScript.h"
#include "IsoView.h"
#include "SceneryNames.h"
#include "DialogFrame.h"
#include "GolferUpdate.h"
#include "GolferShot.h"
#include "ShotTrail.h"
#include "HoleStatistics.h"
#include "StringPool.h"
#include "Critter.h"
#include "LoadingScreen.h"
#include "Employee.h"
#include "InfoScreens.h"
#include "PathFinding.h"
#include "Scorecard.h"
#include "Toolbar.h"
#include "CourseReportScreen.h"
#include "Top10Courses.h"
#include "WildlifeUpdate.h"
#include "SetupScreens.h"
#include "TerrainView.h"
#include "Geometry.h"
#include "GolferPanel.h"
#include "TerrainSmooth.h"
#include "NewGame.h"
#include "GameMain.h"
#include "Screen.h"
#include "EmptyString.h"
#include "GameFlags.h"
#include "ShotPower.h"
#include "FlicBanks.h"
#include "Wildlife.h"
#include "TextWrap.h"
#include "SoundEffects.h"
#include "LandmarkInfluence.h"
#include "RandomSource.h"
#include "TileObstacle.h"
#include "TerrainCircle.h"
#include "crt.h"
#include "ScreenText.h"
#include "ClubName.h"
#include "TournamentLeaderboard.h"
#include "InputPoll.h"
#include "CourseStart.h"
#include "GolferSpawn.h"
#include "TileEdit.h"
#include "HireEmployee.h"
#include <math.h>
#include <windows.h>
#include "ClubReports.h"
#include "OpenHole.h"
#include "LotScoreTier.h"
#include "ThemeLoader.h"
#include "GameSetup.h"
#include "ProMatch.h"

short g_moneyLine[500];
signed char g_cornerElevation[50][50][8];
short g_gameMembershipTrendLine[500];
char g_phrasesPerGolferTypeSave[0x19a28];
int g_landPurchaseNum;
char g_gameStoryNameTable[100][50];
int g_oldestFloatingTxtSlot;
signed char g_simGolferWalkCostMap[50][50];
short g_tileExtra[GRID_SIZE * GRID_SIZE];
int g_displayFocus;
int g_sveFileLoadFlag;
short g_funRatingHistPoints[500];
unsigned char g_ballStopCountMap[2500];
int g_flicBankFrameLens[590];
int g_shotPowerMemoSpeed[10];
int g_gameSpeakerIsPartner;
short g_monthlySkillScore[500];
int g_simListItemRefs[153];
BackgroundActor g_backgroundActors[4];
int g_shotsForTrace;
int g_clubSkillsScore;
HighScore g_topTenScoresArray[10];
int g_top10CoursePoints[10][6];
signed char g_gameForSaleLandGrid[2500];
int g_gameCashPopupDelta[8];
int g_gameCourseRefreshReq;
int g_routeTargetX[10];
int g_routeTargetY[10];
int g_simMoneyPopupTime[8];
int g_gameElevPanelChoice;
int g_moneyPopCoordXs[8];
int g_moneyPopCoordYs[8];
signed char g_flatSquareAltMap[50 * 50];
int g_simOpenFacilityLvl[20];
unsigned short *g_sampleSrcPtr;
int g_currentFeesOption;
int g_scheduledTriggerBits;
int g_shownLandmarkBitmask;
int g_paintedCameraTileY;
int g_paintedCameraTileX;
int g_pointerPosX;
int g_pointerPosY;
char g_phrasesPerGolferType[0x19a28];
CelebRec g_themeCelebsEntries[100];
int g_storiesNum;
int g_lastShownStoryMsgTick;
int g_courseScriptPlayIndex;
int g_buildToolbarShown;
int g_elevToolbarShown;
short g_cellScreenPosX[GRID_SIZE][GRID_SIZE];
short g_cellScreenPosY[GRID_SIZE][GRID_SIZE];
int g_previousSGAMark;
int g_employeeListActive;
int g_jpegSurfacePitch;
int g_toolbarItemIDCnt;
signed char g_tileUphillMap[50][GRID_SIZE];
char g_activeThemeDir[104];
int g_nextTutStepIdx;
int g_allowedExpansionRequestFlag;
int g_toolbarView;
int g_curPreviousCamScale;
int g_winnersPurseK;
int g_trailSavedPlotPosX[152];
int g_trailSavedPlotPosY[152];
int g_rosterBarButtonGolfer[152];
int g_shotMemoHgt[10];
int g_nextOpenHoleIndex;
int g_camDirIdx;
unsigned int g_simPostedHoleCategories;
int g_wantedCamTileY;
short g_monthEventKindHistory[500];
Placed g_namedSceneryArray[100];
int g_gameCurrentHandle;
int g_closestDistanceFound;
char g_reportScreenId;
int g_gameCourseAccurPointsTotal;
int g_relaxingBuildingId;
int g_msgDelay;
char g_tickerMessageBuf_1[128];
char g_tickerMessageBuf_2[128];
char g_simGolferStatusTxt[128];
int g_lastHundredthsShown;
int g_gameStruckClock[152];
unsigned char g_tileVariationIdx[GRID_SIZE * GRID_SIZE];
unsigned char *g_jpegOutRowPtr;
int g_selectedVariant;
int g_deferEventStamp;
int g_nextGolferOrder;
int g_fundsLabelFlag;
int g_bestTenScoresTable[10];
int g_simBallStopPosX;
int g_simBallStopPosY;
unsigned int g_floatingMessageTextColors[8];
int g_destRestX;
int g_destRestY;
int g_gameFloaterFrames[8];
StatisticsRow g_skillClassStatRows[32];
char g_gameFloatStrBufs[8][64];
int g_risingMsgLocXs[8];
int g_curAnalysisHoleIndex;
int g_risingMsgLocYs[8];
int g_forceBits;
unsigned char g_gameDispleasedTally[2500];
int g_dialogBubbleRandomNumberA;
int g_dialogBubbleRandomNumberB;
int g_waitListLength;
HomeVIP g_homeVIPsTbl[100];
char g_themeList[100][50];
char g_txtHeapBuffer[0x1002];
unsigned int g_knownEventsHighMask;
int g_gameCashHundreds;
Course g_mapCourseList[17];
signed char g_gameTileTerrainKind[50 * 50];
int g_ceoLadderRung;
Critter g_gameAnimals[128];
Divot g_tornDivots[256];
Hole g_allHolesArray[20];
TerrainTypeDef g_squareTypeTable[23];
int g_winningPtsTotal;
int g_waterSprayFxPositionX;
int g_waterSprayFxPositionY;
unsigned char g_tileAnimFrame[GRID_SIZE * GRID_SIZE];
Golfer g_golferPool[156];
signed char g_demolishPreviousRecs[2500];
YearLedger g_annualStatsRecords[100];
GolferTypeStats g_golferHistoryPerCharacter[84];
Employee g_staffSlots[64];
int g_scriptCommandCellRows[256];
int g_scriptCommandCellCols[256];
char g_gameCoursePath[256];
int g_speechWindowBottom;
char g_numberBuf[16];
int g_remarkColourValue;
int g_simFrameZoomSnapshot;
int g_clubMemberSum;
int g_renderedBallCount;
Building g_buildingTable[256];
int g_simTotalLengthYards;
unsigned char g_pathFindDist[2500];
signed char g_buttonSlotOfType[23];
RosterEntry g_golfProList[100];
int g_shotSelectedKind;
GolferType g_gameGolferTypesBak[84];
int g_curLandmarkTotal;
int g_gameCourseTotalPar;
int g_popUpRingCursor;
int g_courseTotalFunRate;
int g_curNextSpawnPos;
HoleLandingPoint g_gameFairwayTargetPoints[19][3];
int g_curPreviousConvoClock;
int g_fastRun;
int g_proMatchStakeLvl;
int g_quitToMenuFlag;
int g_shotSearchLocked;
int g_golferPassScratchWord;
int g_numHeads[2];
int g_myWorldCourseIdx;
int g_brushPicks[8];
unsigned int g_knownEventsLowMask;
signed char g_gameDemolitionRefunds[2500];
int g_staffHireButtonIdle;
short g_strOffsetTbl[128];
int g_talentKindCount[32];
int g_runModeFlags;
Trail g_ballPathSlots[16];
signed char g_currentThemeId;
int g_mouseClickDelay;
int g_simCameraZoomStored;
unsigned int g_storyMessageFontColor;
int g_curBuildingTurns;
short g_stringPoolLens[128];
int g_gameMemoDists[10];
int g_curTournPrepBits;
int g_editedTileColIdx;
int g_visibleBallX[32];
int g_visibleBallY[32];
unsigned char g_pointElevationMap[51][51];
int g_watchedPlayerIdx;
int g_previousCamOrientation;
unsigned int g_configBitMask;
signed char g_golferSkillGrades[32];
int g_simLatestGolferNum;
int g_gameAnimNumAngles[590];
int g_unlockedBldgTypeLimit;
int g_tooltipTileColumn;
int g_totalImagination;
unsigned int g_reachableAmenityFlags;
int g_moneyWarnCnt;
unsigned char g_posReactionTileCount[2500];
short g_careerPlayYear;
char g_dialogString[1024];
int g_shotDoubts;
int g_messageCountdown;
short g_scanOpenListX[1024];
short g_scanOpenListY[1024];
int g_plannedEndX;
int g_lengthAbilitySum;
int g_plannedEndY;
int g_scriptCmd[256];
int g_buildingLevelPerKind[20];
int g_updateTimeMs;
int g_flicStartFrame[590];

AssetSurface *g_pMenu = &g_simScreen;
AssetSurface *g_terrainSurface = &g_terrainSurfaceObject;
Accomplishment g_accomp[22] = {{"1st Challenge hole", 0, 0}, {"1st Heroic hole", 0, 0}, {"1st skill upgrade", 0, 0},
    {"1st Tournament", 0, 0}, {"1st Strategic hole", 0, 0}, {"First match victory", 0, 0},
    {"First 9+ hole course", 0, 0}, {"1st Top 100 hole", 0, 0}, {"1st $500,000 Tournament", 0, 0},
    {"1st Classic hole", 0, 0}, {"1st Top 18 hole", 0, 0}, {"First tournament victory (9+ holes)", 0, 0},
    {"1st Grand Slam course (9+ holes)", 0, 0}, {"1st $1,000,000 Tournament", 0, 0}, {"First 18 hole course", 0, 0},
    {"First tournament victory (18 hole)", 0, 0}, {"1st Grand Slam course (18 hole)", 0, 0},
    {"Grand Slam Victory (Parkland)", 0, 0}, {"Grand Slam Victory (Desert)", 0, 0},
    {"Grand Slam Victory (Tropical)", 0, 0}, {"Grand Slam Victory (Links)", 0, 0}, {"1st 100 star rating", 0, 0}};
AnimalKind g_animalKinds[9] = {{"Elk", 14, 9}, {"Crane", 4, 1}, {"Flamingo", 15, 4}, {"Sheep", 4, 8},
    {"Crocodile", 4, 4}, {"Snake", 12, 6}, {"Gila", 4, 2}, {"Road runner", 4, 2}, {"Duck", 4, 12}};
TerrainTypeDef g_terrainTypeDefaults[23] = {
    {"tees\000\000\000\000\000\000\000\000\000\000\000\000tees", 4, 3, 0, 5, 0, 2, 0, 0, {2, 0, 0, 0}, 4},
    {"green\000\000\000\000\000\000\000\000\000\000\000greens", 3, 3, -1, 10, 0, 1, 1, 1, {2, 0, 0, 0}, 0},
    {"fairway\000\000\000\000\000\000\000\000\000fairways", 4, 3, 0, 3, 0, 1, 2, 2, {2, 0, 0, 0}, 0},
    {"firm fairway\000\000\000\000firm fairways", 5, 4, 0, 3, 0, 1, 2, 15, {2, 0, 0, 0}, 0},
    {"rough\000\000\000\000\000\000\000\000\000\000\000rough", 2, 2, 1, 1, 0, 2, 4, 3, {2, 0, 0, 0}, 0},
    {"deep rough\000\000\000\000\000\000deep rough", 1, 2, 2, 2, 0, 3, 4, 17, {2, 0, 0, 0}, 0},
    {"mound\000\000\000\000\000\000\000\000\000\000\000mounds", 2, 2, 2, 4, 0, 3, 4, 4, {2, 0, 0, 0}, 0},
    {"sand trap\000\000\000\000\000\000\000sand traps", 1, 1, 3, 6, 0, 4, 7, 5, {2, 0, 0, 0}, 0},
    {"waste bunker\000\000\000\000wbs", 2, 2, 2, 4, 0, 3, 8, 5, {2, 0, 0, 0}, 0},
    {"pot bunker\000\000\000\000\000\000pot bunkers", 1, 1, 5, 8, 0, 5, 7, 5, {2, 0, 0, 0}, 2},
    {"ravine\000\000\000\000\000\000\000\000\000\000ravines", 2, 2, 5, 10, 0, 3, 4, 6, {2, 0, 0, 0}, 0},
    {"brush\000\000\000\000\000\000\000\000\000\000\000brush", 1, 1, 3, 4, 0, 3, 4, 6, {2, 0, 0, 0}, 0},
    {"rocks\000\000\000\000\000\000\000\000\000\000\000rocks", 5, 1, 3, 4, 5, 4, 4, 9, {2, 0, 0, 0}, 256},
    {"tree\000\000\000\000\000\000\000\000\000\000\000\000trees", 2, 2, 4, 10, 5, 4, 13, 7, {2, 0, 0, 0}, 1024},
    {"pine tree\000\000\000\000\000\000\000pine trees", 2, 2, 4, 10, 5, 4, 13, 7, {2, 0, 0, 0}, 512},
    {"palm tree\000\000\000\000\000\000\000palm trees", 2, 2, 4, 10, 5, 4, 13, 6, {2, 0, 0, 0}, 512},
    {"elm tree\000\000\000\000\000\000\000\000elm trees", 2, 2, 4, 25, 10, 4, 13, 7, {2, 0, 0, 0}, 1024},
    {"water\000\000\000\000\000\000\000\000\000\000\000water", 0, 0, 8, 50, 10, 16, 17, 8, {1, 0, 0, 0}, 2},
    {"wetlands\000\000\000\000\000\000\000\000wetlands", 2, 1, 3, 2, 50, 4, 4, 14, {2, 0, 0, 0}, 8},
    {"marsh\000\000\000\000\000\000\000\000\000\000\000marsh", 2, 1, 3, 6, 100, 4, 4, 14, {2, 0, 0, 0}, 8},
    {"out of bounds\000\000\000out of bounds", 2, 2, 8, 10, 0, 12, 18, 6, {2, 0, 0, 0}, 0},
    {"building\000\000\000\000\000\000\000\000building", 2, 2, 4, 0, 0, 4, 18, 16, {2, 0, 0, 0}, 5},
    {"building\000\000\000\000\000\000\000\000building", 2, 2, 4, 0, 0, 4, 18, 16, {2, 0, 0, 0}, 5}};
TourSite g_tourSites[16] = {{"Monterey", 9, "Ocean's Edge", 53, 226, 0, 1, 1, "Scenic Cypress"},
    {"San Diego", 4, "Dolphin Coast", 53, 244, 0, 1, 0, "Dolphins"},
    {"Rocky Mtns.", 11, "Jurassic Springs", 92, 226, 0, 0, 2, "Free Hotel"},
    {"Las Vegas", 3, "Ace in the Hole", 73, 241, 1, 0, 0, "Fun, Fun, Fun"},
    {"Phoenix", 0, "Coyote Flats", 70, 262, 1, 0, 0, "Free Spa"},
    {"Hawaii", 7, "Flamingo Shores", 26, 327, 2, 1, 1, "Scenic Waterfall"},
    {"Oahu", 6, "Island Palms", 23, 311, 2, 2, 0, "Japanese Garden"},
    {"Nova Scotia", 8, "Windy Point", 158, 250, 3, 1, 1, "Scenic Lighthouse"},
    {"Northeast", 15, "Ravenwood Farms", 138, 254, 0, 0, 1, "Civil War Battlefield"},
    {"Carolina", 10, "Christmas Pines", 133, 272, 0, 0, 0, "Free Putting Green"},
    {"Ireland", 14, "County Kincaide", 262, 232, 3, 0, 2, "Leprechauns"},
    {"Scotland", 12, "Harold's Keep", 266, 222, 3, 1, 0, "Free Castle"},
    {"Wales", 13, "Thistle Runes", 272, 239, 3, 2, 1, "Stonehenge"},
    {"Spain", 2, "Sangria Bay", 275, 270, 1, 1, 1, "Scenic Vineyards"},
    {"Florida", 5, "Ocean Grove", 125, 298, 2, 1, 0, "Free Pro Shop"},
    {"Jamaica", 1, "Scorpion Cove", 129, 342, 2, 0, 1, "Scenic Statues"}};
FacilityType g_facilityTypes[20] = {{"Pathway", 1, 0, 1}, {"Benches", 1, 0, 2}, {"Flower Bed", 1, 0, 5},
    {"Ball Washer", 1, 0, 50}, {"Landmark", 1, 0, 15}, {"Home Site", 2, 0, 10}, {"Putting Green", 3, 0, 100},
    {"Snack Bar", 2, 0, 150}, {"Pro Shop", 2, 0, 200}, {"Swim Club", 3, 0, 300}, {"Driving Range", 5, 0, 250},
    {"Cart Garage", 2, 0, 400}, {"Marina", 2, 0, 1000}, {"Resort Hotel", 4, 0, 2500}, {"Airstrip", 6, 0, 5000},
    {"Clubhouse", 4, 0, 200}, {"Willow Tree", 1, 0, 25}, {"TV Tower", 1, 0, 10}, {"TV Booth", 1, 0, 10},
    {"Scenic Bridge", 1, 0, 100}};
int g_tileSpan = 16;
int g_cameraZoom = 4;
int g_paintTerrain = -1;
int g_celebHomesBought = 1;
int g_calculatedGreenFee = 5;
int g_selectedBuildingKind = -1;
char *g_talentNames[3] = {"length", "accuracy", "imagination"};
char *g_traitNames[5] = {"Neat", "Outgoing", "Active", "Playful", "Nice"};
int g_dirRowStep[8] = {0, 1, 1, 1, 0, -1, -1, -1};
int g_dirColStep[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
char *g_ordinals[20] = {"Drive", "1st", "2nd", "3rd", "4th", "5th", "6th", "7th", "8th", "9th", "10th", "11th", "12th",
    "13th", "14th", "15th", "16th", "17th", "18th", "Done"};
const char *kMonth[8] = {"March", "April", "May", "June", "July", "August", "September", "October"};
char *g_clubAbbrevs[14] = {"Drvr", "3W", "4W", "2i", "3i", "4i", "5i", "6i", "7i", "8i", "9i", "Wg", "Sw", "Ptr"};
char *g_clubLetters[14] = {"D", "3", "4", "2", "3", "4", "5", "6", "7", "8", "9", "W", "S", "P"};
char *g_golferNicknames[32] = {"Bunny", "Bobbie", "Bettie", "Bonnie", "Leo", "Lefty", "Les", "Lon", "Ann", "Abby",
    "Amy", "Ava", "Abner", "Alvin", "Sid", "Andy", "Smartie", "Specs", "Sally", "Sandy", "Sam", "Stan", "Slade", "Will",
    "Karrie", "Jan", "Julie", "Nancy", "Harry", "Eldrick", "Bing", "Luc"};
char *g_courseTierNames[4] = {"Municipal", "Daily Fee", "Country Club", "Championship"};
char *g_holeTypeNames[8] = {"Breather", "Freeway", "Precise", "Challenge", "Creative", "Heroic", "Strategic",
    "Classic"};
char *g_amateurEventNames[6] = {"new Junior Putters of America", "local Elk's Club Invitational",
    "regional Oral Hygiene Open", "prestigious U.S Amateur Championship", "SGA Senior Tour",
    "hallowed United States Open"};
char *g_greatCourseNames[10] = {"Pebble Beach", "Pine Valley", "St. Andrews", "Augusta National", "Baltusrol",
    "Winged Foot", "Oakland Hills", "Sawgrass", "Carnoustie", "                    "};
char *g_membershipName[10] = {g_empty, "Visitor", g_empty, "Silver ", "Gold ", "Platinum ", g_empty, g_empty, g_empty,
    g_empty};
char *g_proFirstNames[32] = {g_empty, g_empty, g_empty, g_empty, g_empty, g_empty, g_empty, g_empty, "Craig", "Colin",
    "Greg", "David", g_empty, g_empty, g_empty, g_empty, "Jean", "Arnie", "Tom", "John", "Karrie", "Jan", "Julie",
    "Nancy", "Ben", "Ernie", "Jack", "Tiger", g_empty, g_empty, g_empty, g_empty};
char *g_proLastNames[28] = {g_empty, g_empty, g_empty, g_empty, g_empty, g_empty, g_empty, g_empty, "Stadler",
    "Montgomerie", "Norman", "Duval", g_empty, g_empty, g_empty, g_empty, "VanDeVelde", "Palmer", "Watson", "Daly",
    "Fisher", "Stephenson", "Inkster", "Lopez", "Hogan", "Els", "Nicklaus", "Woods"};
int g_cameraTileX = 25;
int g_cameraTileY = 25;
char *g_celebKindNames[11] = {"action star", "pop sensation", "noted statesman", "funny man", "supermodel",
    "fitness guru", "funny lady", "leading man", "beauty", "rocker", "superstar"};
char *g_celebrityNames[12] = {g_empty, "Sylvester Stallion", "Britney Aguilera", "Bill Clinton", "William Robins",
    "Claudia Chiffre", "Jane Hanoye", "Rosie O'Doul", "George Hamiltan", "Pam Andersen", "Bruce Springstone",
    "Julius Jordan"};
signed char g_celebrityOrder[9] = {0, 4, 2, 1, 0, 5, 6, 3, 7};
char *g_commissionerName = "I.M.Picky";
char *g_brokerName = "E.F.Mutton";
char *g_ceoName = "J.P.Bigdome";
char *g_investorNames[8] = {"B.Gates", "J.W.Bigshot", "Don Trump", "J.P.Bigwig", "L.Probst", "J.W.Bigguy",
    "Bigdome Construction", "Dewey Associates"};
char *g_skillNames[10] = {"Power Hitter", "Long Driver", "Accurate Driver", "Accurate Irons", "Accurate Putter",
    "Draw Shot (R to L)", "Fade Shot (L to R)", "High Backspin Shot", "Recovery Skills", "Luck"};
char *g_mentalSkillNames[3] = {"Positive Attitude", "Mental Toughness", "Stamina"};
char *g_proOpponentNames[9] = {"Joe Pro", "Palmer Arnold", "Elsie Erns", "Tree Levino", "Joe Cool", "Seve Garcia",
    "Watt Thomson", "Jack Nickols", "Tiger Forest"};
unsigned char g_miscGameFlags[8] = {0, 0, 0, 2, 2, 0, 8, 12};
short g_skillMasks[5] = {0, 0x1409, 0x080d, 0x0c1d, -1};
char *g_preferenceNames[6] = {"Display golfer names on screen.", "Show snapshots of great shots.",
    "Full explanation of golfer comments.", "Display message for new members.", "Display golfer thoughts on screen.",
    "Include animals and habitats."};
char *g_quoteSituationNames[20] = {"Signature saying", "Made good shot", "Too easy shot", "Hard shot",
    "Really hard shot", "Hit bad shot", "Hit worse shot", "Almost hit by ball", "Hit a tree", "Hit into the water",
    "Notices scenic improvement", "Notices ugly item", "Notices animal", "Long wait at tee", "Getting thirsty",
    "Found a drink", "Getting hungry", "Found some food", "Getting tired", "Found a bench"};
signed char g_typeLineIds[24] = {62, 1, 4, 5, 31, 2, 3, 9, 12, 13, 28, 20, 39, 21, 14, 25, 15, 18, 26, 27, -1, 0, 0, 0};
signed char g_keyOrder[16] = {0, 1, 7, 4, 9, 10, 17, 13, 2, 3, 5, 8, 11, 12, 14, 18};
short g_trophyPartPos[25][2] = {{400, 97}, {400, 133}, {400, 168}, {400, 207}, {400, 239}, {400, 273}, {400, 310},
    {400, 341}, {400, 381}, {400, 413}, {400, 448}, {400, 480}, {400, 519}, {400, 556}, {610, 97}, {610, 130},
    {610, 168}, {610, 202}, {610, 239}, {610, 274}, {610, 307}, {610, 341}, {610, 381}, {610, 415}, {610, 450}};
short g_trophyPartSize[25][2] = {{135, 22}, {117, 21}, {125, 21}, {124, 18}, {133, 20}, {135, 21}, {137, 20}, {129, 25},
    {133, 32}, {110, 22}, {112, 24}, {177, 37}, {160, 35}, {141, 31}, {129, 20}, {180, 36}, {161, 36}, {144, 40},
    {144, 33}, {142, 38}, {144, 35}, {155, 24}, {132, 20}, {126, 20}, {109, 21}};
int g_elevStep = 5;
int g_editedTileRowIdx = -1;
int g_messageSpeaker = -1;
int g_selectedGolferIndex = -1;
int g_selectedItemIndex = -1;
int g_pendingProIndex = -1;
int g_unusedMinusOneA = -1;
int g_simGolferFlags = 7;
int g_pickState = -1;
int g_tooltipTileRow = -1;
int g_waterSprayFxFrame = -1;
signed char g_jobWages[12] = {3, 7, 2, 3, 2, 4, 2, 5, 0, 0, 0, 0};
char *g_par4HoleNames[20] = {"0", "Olive", "Dogwood", "Peach", "Crab Apple", "Magnolia", "Juniper", "Pampas", "Jasmime",
    "Cherry", "Camellia", "Daisy", "Golden Bell", "Azalea", "Chinese Fir", "Firethorn", "Redbud", "Nandina", "Holly",
    "19"};
char *g_par3HoleNames[20] = {"0", "Alexandra", "Belinda", "Carmen", "Dorothy", "Elizabeth", "Faith", "Gisele", "Hope",
    "Ingrid", "Jacqueline", "Lara", "Melinda", "Naomi", "Priscilla", "Roxanne", "Sarah", "Valerie", "Zelda", "19"};
char *g_par5HoleNames[20] = {"0", "Pride", "Inferno", "Fortress", "The Siren", "Centaur", "Styx", "Flame", "Serpent",
    "Paradise", "Eden", "Valley", "Torment", "Misery", "Archangel", "Smoke", "Gryphon", "Beast", "Purgatory", "19"};
FlowerBedSprite g_flowerBedSprites[16] = {{418, -1}, {419, 3}, {419, 2}, {423, 2}, {419, 1}, {420, 1}, {423, 1},
    {421, 2}, {419, 0}, {423, 3}, {420, 0}, {421, 3}, {423, 0}, {421, 0}, {421, 1}, {422, -1}};
short g_slopeLimit[5][5] = {{6, 4, 3, 4, 6}, {4, 2, 1, 2, 4}, {3, 1, 0, 1, 3}, {4, 2, 1, 2, 3}, {6, 4, 3, 4, 6}};
int g_wantedCamTileX = -1;
int g_terrainHeightDivisor = 16;
signed char g_effectPitches[36] = {0, 2, 4, 7, 5, 5, 9, 7, 7, 12, 11, 12, 7, 4, 0, 2, 4, 5, 7, 9, 7, 5, 4, 2, 4, 0, -1,
    0, 2, -5, -1, 2, 5, 4, 2, 4};
int g_debugBestShotGolfer = -1;
int g_debugShotGolfer = -1;
int g_unusedMinusOneB = -1;
char *g_golferLikes[40] = {"puppies", "old movies", "chocolate", "dinosaurs", "broccoli", "jazz", "the Beatles",
    "bunnies", "CNN", "fast cars", "motorcycles", "Bach", "jelly beans", "world peace", "bagels", "Picasso", "burritos",
    "mystery novels", "travel", "sailboats", "cooking", "sunsets", "turn-based strategy games", "funny hats",
    "line dancing", "strawberries", "the circus", "pumpkin pie", "goldfish", "disco dancing", "milkshakes",
    "Valentine's Day", "gardening", "shopping", "neon lights", "afternoon naps", "old sneakers", "Super Bowl parties",
    "meditation", "bingo"};
int g_jpegBlockSize = 1;
char *g_courseTypeNames[4] = {"Park", "Desert", "Tropical", "Links"};

int CalculateRandomShotVariance()
{
    int v = (int)g_rng.NextInt(101) - 50;
    int s = Sign(v);
    int a = abs(v);
    if (a < 20)
        a = a / 2;
    else if (a < 40)
        a = a - 10;
    else
        a = a * 2 - 50;
    if (s != 1)
        a = -a;
    return a;
}

int SpawnCritter(int x, int y, unsigned char kind)
{
    int i;
    for (i = 0; i < 128; i++) {
        if (g_gameAnimals[i].kind == -1) {
            g_gameAnimals[i].x = x * 1024 + 512;
            g_gameAnimals[i].y = y * 1024 + 512;
            g_gameAnimals[i].dir = g_rng.NextInt(4);
            g_gameAnimals[i].sprite = 0x10c;
            g_gameAnimals[i].state = -20;
            g_gameAnimals[i].kind = kind;
            g_gameAnimals[i].leader = -1;
            return i;
        }
    }
    return -1;
}

void PlayCritterCall(int kind, int x, int y)
{
    if (kind == 0) {
        PlayPositionedSound(0x3b, x, y, 0);
        return;
    }
    if (kind == 1) {
        PlayPositionedSound(0x3a, x, y, 0);
        return;
    }
    if (kind == 3) {
        PlayPositionedSound(0xc3, x, y, 0);
        return;
    }
    if (kind == 4) {
        PlayPositionedSound(0xc4, x, y, 0);
        return;
    }
    if (kind == 6) {
        PlayPositionedSound(0xc5, x, y, 0);
        return;
    }
    if (kind == 5) {
        PlayPositionedSound(0xc6, x, y, 0);
    }
}

int ValidateAndTrimString(char *s)
{
    if (strpbrk(s, "*|:<>?/\\") != 0)
        return 0;
    int n = 1;
    while (n > 0) {
        n = strlen(s);
        if (n == 0)
            break;
        if (s[n - 1] != ' ')
            break;
        s[n - 1] = 0;
    }
    return n > 0;
}

void ShowSaveGameDialog(void)
{
    char name[100];

    DrawDialogFrameWithBorder(0x36, 0x50, 0x274, 0x50, 0);
    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
    DrawTextCenteredWithShadow("SAVE GAME: edit name then press Enter.", 0x180, 0x58, COLOR_WHITE);
    g_printBuffer[0] = 0;
    AppendCourseName(-1);
    strcat(g_printBuffer, " ");
    strcat(g_printBuffer, itoa(g_clock % 1024 * 30 / 1024 + 1, g_numberBuf, 10));
    strcat(g_printBuffer, " ");
    FormatDateString(g_clock);
    ShowTextInputDialog("Save File Name...", 0x40, 0x66, 0x30);
    if (g_printBuffer[0] != 0 && ValidateAndTrimString(g_printBuffer)) {
        strcpy(name, g_printBuffer);
        strcpy(g_printBuffer, "saved games\\");
        strcat(g_printBuffer, name);
        strcat(g_printBuffer, ".sve");
        if (FileExists(g_printBuffer)) {
            strcpy(g_printBuffer, name);
            strcat(g_printBuffer, "\nalready exists!\n Overwrite the old version.\n Cancel\n");
            if (ShowActionDialog(0x190, 0x8c, 1, 1, 0))
                return;
        }
        strcpy(g_printBuffer, name);
        SaveGame(g_printBuffer);
        strcpy(g_printBuffer, "Game Saved.");
        ShowStoryMessage(0x80001284, 1, -4);
    } else {
        strcpy(g_printBuffer, "Invalid File Name");
        ShowStoryMessage(0x80001284, 1, -4);
    }
}

void TrySpawnWildlifeHabitat(int x, int y, int type)
{
    g_gameTileTerrainKind[x * 50 + y] = type;
    g_tileExtra[x * 50 + y] &= ~(TILE_SCENIC | TILE_NAMED);

    x += g_rng.NextInt(3) - 1;
    y += g_rng.NextInt(3) - 1;

    if (g_rng.NextInt(8) == 0 && g_gameAnimals[g_rng.NextInt(0x80)].kind == -1 && (g_configBitMask & 0x20)) {
        int kind = g_rng.NextInt(9);
        int tries;
        for (tries = 1; tries <= 9; ++tries) {
            if (g_gameTileTerrainKind[x * 50 + y] == g_animalKinds[kind].habitat
                && ((1 << g_currentThemeId) & g_animalKinds[kind].themes) && !IsPositionInvalid(x, y))
                break;
            kind = g_rng.NextInt(9);
        }
        if (tries <= 9) {
            int first = SpawnCritter(x, y, kind);
            PlayPositionedSound(0x8f, x * 1024 + 512, y * 1024 + 512, 0);
            int cur = first;
            int n = g_rng.NextInt(2);
            if (first != -1) {
                while (cur != -1) {
                    if (g_gameAnimals[cur + 1].kind != -1)
                        break;
                    if (n >= 3)
                        break;
                    int dx = g_rng.NextInt(3);
                    int next = SpawnCritter(x + dx - 1, y + g_rng.NextInt(3) - 1, kind);
                    cur = g_gameAnimals[next].leader = first;
                }
                strcpy(g_printBuffer, g_animalKinds[kind].name);
                strcat(g_printBuffer, " habitat");
                AddFloatingMessage(x * 1024 + 512, y * 1024 + 512, 0x800003ff);
                PlayCritterCall(kind, x * 1024 + 512, y * 1024 + 512);
            }
        }
    }
}

int CalculateHoleTalentStrokeGap(int hole, int missingTalents)
{
    int strokes[8], rounds[8];
    int allRounds, allStrokes;
    int i, j;

    for (j = 0; j < 8; j++)
        strokes[j] = g_allHolesArray[hole].par * 8;
    for (j = 0; j < 8; j++)
        rounds[j] = 8;
    allRounds = 0;
    allStrokes = 0;
    for (i = 1; i <= 9; i++)
        for (j = 0; j < 8; j++) {
            int n = g_allHolesArray[hole].dist[j][i];
            rounds[j] += n;
            strokes[j] += n * i;
            allRounds += n;
            allStrokes += n * i;
        }

    if (rounds[7] == 0)
        return 0;
    if (rounds[6] != 0 && missingTalents == 1)
        return strokes[6] * 100 / rounds[6] - strokes[7] * 100 / rounds[7];
    if (rounds[5] != 0 && missingTalents == 2)
        return strokes[5] * 100 / rounds[5] - strokes[7] * 100 / rounds[7];
    if (rounds[3] != 0 && missingTalents == 4)
        return strokes[3] * 100 / rounds[3] - strokes[7] * 100 / rounds[7];
    return 0;
}

AssetSurface g_introBackdrop;
AssetSurface g_saveGameSnapArt;

void DrawTerrainLineWrapper(int x0, int y0, int x1, int y1, int color, int width, int alpha)
{
    DrawTerrainLine(x0, y0, x1, y1, color, width, alpha);
}

void ShowLoadingScreen(int percent)
{
    SpriteObject picture;
    AssetSurface sheet;

    switch (g_rng.NextInt(12)) {
    case 0: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen01", 0, 0, PALETTE_COLORS, 2); break;
    case 1: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen02", 0, 0, PALETTE_COLORS, 2); break;
    case 2: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen03", 0, 0, PALETTE_COLORS, 2); break;
    case 3: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen04", 0, 0, PALETTE_COLORS, 2); break;
    case 4: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen05", 0, 0, PALETTE_COLORS, 2); break;
    case 5: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen06", 0, 0, PALETTE_COLORS, 2); break;
    case 6: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen07", 0, 0, PALETTE_COLORS, 2); break;
    case 7: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen08", 0, 0, PALETTE_COLORS, 2); break;
    case 8: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen09", 0, 0, PALETTE_COLORS, 2); break;
    case 9: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen10", 0, 0, PALETTE_COLORS, 2); break;
    case 10: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen11", 0, 0, PALETTE_COLORS, 2); break;
    case 11: sheet.load_pcx("interface\\Loading_Screens\\LoadingScreen12", 0, 0, PALETTE_COLORS, 2); break;
    }
    picture.extract(&sheet, 0x5a, 0xaf, 0x26c, 0xca, 1, 1);
    picture.BlitScaled(g_pMenu, 0x5a, 0xaf, 0);

    g_printBuffer[0] = 0;
    switch (g_rng.NextInt(11)) {
    case 0:
        strcat(g_printBuffer, "'Every hole should have a different character', from GOLF ARCHITECTURE\n"
                              "- Alister Mackenzie (1870-1934)");
        break;
    case 1:
        strcat(g_printBuffer, "'There should be infinite variety in the strokes required to play the various holes.', "
                              "from GOLF ARCHITECTURE\n- Alister Mackenzie (1870-1934)");
        break;
    case 2:
        strcat(g_printBuffer,
            "'All really great golf holes involve a contest of wit and risks.', from CONCERNING GOLF\n"
            "- John Laing Low (1869-1929)");
        break;
    case 3:
        strcat(g_printBuffer, "'A golf course should provide entertainment for the high and medium handicapper...', "
                              "from CONCERNING GOLF\n- John Laing Low (1869-1929)");
        break;
    case 4:
        strcat(g_printBuffer,
            "'Basic criteria: Safety, Shot value, Fairness, Flow, Aesthetics, Tournament qualities.', "
            "from GOLF COURSE ARCHITECTURE\n- Michael Hurdzan (1943- )\n"
            "copyright 1996, Sleeping Bear Press");
        break;
    case 5:
        strcat(g_printBuffer, "'Golf courses are playfields of infinite variety...', from GOLF COURSE ARCHITECTURE\n"
                              "- Michael Hurdzan (1943- )\ncopyright 1996, Sleeping Bear Press");
        break;
    case 6:
        strcat(g_printBuffer, "'There are 118 golf holes here. All I have to do is eliminate the 100.'\n"
                              "- course designer Perry Maxwell");
        break;
    case 7:
        strcat(g_printBuffer, "'When you get those dudes thinking, they're in trouble.'\n- course designer Pete Dye");
        break;
    case 8:
        strcat(g_printBuffer, "'The spirit of golf is to dare a hazard, and by negotiating it reap a reward.', "
                              "from GOLF ARCHITECTURE in AMERICA\n- George Thomas, 1927");
        break;
    case 9: strcat(g_printBuffer, "'I hate this stupid course, I'm leaving!'\n- Jan, hole #5"); break;
    case 10: strcat(g_printBuffer, "'Hey, that ball almost hit me!'\n- Abner, hole #17"); break;
    }
    DrawTextWordWrapped(0x32, 0xc8, 0xf0, 0x80007fff);

    if (percent != -1) {
        int w = Clamp((100 - percent) * 2, 0, 200);
        g_simScreen.fill(0x12b, 0x190, 0xca, 0x18, COLOR_WHITE);
        g_simScreen.fill(0x1f4 - w, 0x191, w, 0x16, 0x80007b20);
        DrawTextCenteredAtY("Loading...", 0x190, 0x194, 0x80000000);
    }
    picture.close();
}

void ShowSkillPointsIntro()
{
    strcpy(g_printBuffer, "Before you play your course you may customize your character by improving his or her "
                          "golf skills. ");
    strcat(g_printBuffer, "You can also win additional skill points for each accomplishment added to your trophy.");
    int points = 10;
    for (int i = 0; i < 22; i++)
        if (g_accomp[i].awarded != 0)
            points += 3;
    g_messageSpeaker = -1;
    RenderSpeechBubbleWithPortrait(0xc8, 0x172, 0x80007fff, -2);
    g_skillMasks[0] = (short)RenderSkillPointsScreen(g_golferTypeArray[0].name, 0, points, -1, 0xc8);
}

void ProcessMembershipUpgrade(int golfer)
{
    int total, remaining;
    int tier, type, tries, friendType, k, i, members, inTier;
    unsigned short pick;

    g_printBuffer[0] = 0;
    g_golferPool[golfer].flags &= ~GOLFER_UPGRADE_DUE;
    type = g_golferPool[golfer].typeIdx;
    tier = ++g_golferHistoryPerCharacter[type].flags & MEMBERSHIP_MASK;
    AppendGolferName(golfer, 0);
    if (tier >= 3) {
        strcat(g_printBuffer, " has decided to upgrade to a");
        switch (tier) {
        case 3: strcat(g_printBuffer, " prestigious "); break;
        case 4: strcat(g_printBuffer, " coveted "); break;
        case 5: strcat(g_printBuffer, "n exclusive "); break;
        }
        strcat(g_printBuffer, g_membershipName[tier]);
    } else {
        strcat(g_printBuffer, " applies for ");
    }
    strcat(g_printBuffer, "membership at your club! ");
    if (tier == 3) {
        strcat(g_printBuffer, g_membershipName[3]);
        strcat(g_printBuffer, " members will pay big bucks for an attractive home site on your course.");
    } else if (tier > 3) {
        strcat(g_printBuffer, g_membershipName[tier]);
        strcat(g_printBuffer, " members like to drive around in shiny customized golf carts and pay \xa7");
        strcat(g_printBuffer, itoa(tier == 4 ? 2 : 5, g_numberBuf, 10));
        strcat(g_printBuffer, "00 extra greens fees per hole.");
    }

    for (tries = 0; tries < 1000; tries++) {
        pick = g_rng.NextInt(0x4b);
        friendType = pick + 1;
        if (g_golferHistoryPerCharacter[friendType].flags == 0)
            break;
    }
    if (tries < 1000) {
        g_golferHistoryPerCharacter[friendType].flags = 1;
        strcat(g_printBuffer, IsGolferMale(golfer) ? " He" : " She");
        strcat(g_printBuffer, " is also planning to invite ");
        strcat(g_printBuffer, IsGolferMale(golfer) ? "his friend " : "her friend ");
        strcat(g_printBuffer, g_golferTypeArray[friendType].name);
        strcat(g_printBuffer, " to play your course.");
    }

    strcat(g_printBuffer, "\nCurrent membership...\n   ");
    remaining = 100;
    for (k = 5; k >= -1; k--) {
        if (k == 0)
            continue;
        members = 0;
        inTier = 0;
        total = 0;
        for (i = 0; i < 76; i++) {
            if (g_golferHistoryPerCharacter[i].count != 0)
                members++;
            if (g_golferHistoryPerCharacter[i].count != 0
                && (g_golferHistoryPerCharacter[i].flags & MEMBERSHIP_MASK) == k)
                inTier++;
            if (g_golferHistoryPerCharacter[i].finishedStoryId == -1 && k == -1)
                inTier++;
        }
        total = members;
        if (inTier == 0)
            continue;
        if (k == -1) {
            strcat(g_printBuffer, "Resigned: ");
        } else {
            strcat(g_printBuffer, g_membershipName[k]);
            if (k == 2)
                strcat(g_printBuffer, "Basic ");
            if (k >= 2)
                strcat(g_printBuffer, "members: ");
            else
                strcat(g_printBuffer, "s: ");
        }
        strcat(g_printBuffer, itoa(inTier, g_numberBuf, 10));
        strcat(g_printBuffer, "\n   ");
        remaining += inTier * -100 / total;
    }

    if (g_configBitMask & 8) {
        if (g_selectedGolferIndex == (golfer | 0x100)) {
            g_bubbleStep = BUBBLE_OPEN;
            g_reportScreenId = 0;
        }
        if (ShowStoryMessage(COLOR_PURPLE, 0, golfer) != 0)
            ConfigureAndPlaySound(SOUND_TADA, 0x46, 0, 0, 0);
    }
}

int CheckCanAfford(int cost)
{
    if (cost <= 0 || cost <= g_gameCashHundreds || (g_runModeFlags & GAME_SANDBOX) || g_nextOpenHoleIndex <= 3)
        return 1;
    strcpy(g_printBuffer, "This change costs \xa7");
    FormatNumberWithCommas(cost * 100);
    strcat(g_printBuffer, ". You have only \xa7");
    FormatNumberWithCommas(g_gameCashHundreds * 100);
    strcat(g_printBuffer, ".");
    RenderSpeechBubbleWithPortrait(0xc8, 0x96, 0x80007fff, -2);
    g_simScreen.RenderFullSurface(0);
    WaitForInputWithTimeout(0);
    return 0;
}

void FormatGolferMessage(int golfer, int priority)
{
    char saved[512];

    if (priority < g_statusPriority)
        return;
    g_statusPriority = (char)priority;
    if (golfer != -1) {
        strcpy(saved, g_printBuffer);
        strcpy(g_printBuffer, g_empty);
        AppendGolferName(golfer, 0);
        strcat(g_printBuffer, " (");
        strcat(g_printBuffer, itoa(g_golferPool[golfer].holeIdx, g_numberBuf, 10));
        strcat(g_printBuffer, "): ");
        strcpy(g_simGolferStatusTxt, g_printBuffer);
        strcat(g_simGolferStatusTxt, saved);
        strcpy(g_printBuffer, saved);
    } else {
        strcpy(g_simGolferStatusTxt, g_printBuffer);
    }
}

void SmoothTerrainRaise(int row, int col)
{
    int i, j;
    int centre = g_pointElevationMap[row][col];
    for (i = 0; i < 5; i++)
        for (j = 0; j < 5; j++)
            if (g_pointElevationMap[row - 2 + i][col - 2 + j] < centre - g_slopeLimit[i][j])
                g_pointElevationMap[row - 2 + i][col - 2 + j]++;
}

void SmoothTerrainLower(int row, int col)
{
    int i, j;
    int centre = g_pointElevationMap[row][col];
    for (i = 0; i < 5; i++)
        for (j = 0; j < 5; j++)
            if (g_pointElevationMap[row - 2 + i][col - 2 + j] > centre + g_slopeLimit[i][j])
                g_pointElevationMap[row - 2 + i][col - 2 + j]--;
}

int g_gameNearbyLandmarkGroups;

int InLandmarkInfluence(int x, int y, int groups)
{
    int i, yards, landmark;
    int found = 0;

    for (i = 0; i < 256; i++) {
        if (g_buildingTable[i].kind != SCENERY_LANDMARK)
            continue;
        if (g_buildingTable[i].landmark >= 16)
            continue;

        yards = CalculateDistanceInYards(x, y, g_buildingTable[i].row, g_buildingTable[i].col);
        landmark = g_buildingTable[i].landmark;

        if (yards < (landmark + 8) * 25 / 3) {
            switch (landmark & 3) {
            case 0: found |= 1; break;
            case 1: found |= 2; break;
            case 2: found |= 4; break;
            case 3: found |= 8; break;
            }
        }
    }

    g_gameNearbyLandmarkGroups = found;
    return (found & groups) == groups;
}

int BallHitsObstacle(int tileType, int ballZ, int row, int col)
{
    int hi;
    int lo;

    if (tileType == TERRAIN_BUILDING || tileType == TERRAIN_HOME_SITE) {
        hi = 200;
        lo = 0;
        if ((g_tileExtra[row * GRID_SIZE + col] & TILE_FACILITY_MASK) - 1 <= 3)
            hi = 0;
    }

    switch (g_currentThemeId) {
    case THEME_PARK:
        if (tileType == 13) {
            hi = g_rng.NextInt(100) + 400;
            lo = 50;
        } else if (tileType == 14) {
            hi = g_rng.NextInt(100) + 100;
            lo = 20;
        } else if (tileType == 15) {
            hi = g_rng.NextInt(100) + 100;
            lo = 50;
        } else if (tileType == 16) {
            hi = g_rng.NextInt(200) + 400;
            lo = 75;
        }
        break;
    case THEME_DESERT:
        if (tileType == 13) {
            hi = g_rng.NextInt(100) + 400;
            lo = 50;
        } else if (tileType == 14) {
            hi = g_rng.NextInt(100) + 100;
            lo = 20;
        } else if (tileType == 15) {
            hi = g_rng.NextInt(100) + 300;
            lo = 100;
        } else if (tileType == 16) {
            hi = g_rng.NextInt(200) + 400;
            lo = 75;
        }
        break;
    case THEME_TROPICAL:
        if (tileType == 13) {
            hi = g_rng.NextInt(100) + 100;
            lo = 20;
        } else if (tileType == 14) {
            hi = g_rng.NextInt(100) + 400;
            lo = 50;
        } else if (tileType == 15) {
            hi = g_rng.NextInt(100) + 100;
            lo = 50;
        } else if (tileType == 16) {
            hi = g_rng.NextInt(200) + 400;
            lo = 75;
        }
        break;
    case THEME_LINKS:
        if (tileType == 13 || tileType == 14 || tileType == 15 || tileType == 16) {
            hi = g_rng.NextInt(100) + 400;
            lo = 20;
        }
        break;
    }

    return ballZ < hi && ballZ > lo;
}

void AppendHoleName(int hole)
{
    if (g_allHolesArray[hole].flags & 0x81) {
        if (AppendPooledString(hole))
            return;
        char *name;
        switch (g_allHolesArray[hole].par) {
        case 3: name = g_par3HoleNames[hole]; break;
        case 4: name = g_par4HoleNames[hole]; break;
        default: name = g_par5HoleNames[hole]; break;
        }
        strcat(g_printBuffer, name);
    } else {
        strcat(g_printBuffer, "Hole ");
        strcat(g_printBuffer, itoa(hole, g_numberBuf, 10));
    }
}

int FindNearestHole(int x, int y)
{
    int best = 0xffff;
    int nearest = -1;
    for (int h = 1; h < 19; h++) {
        int d = Math_Distance2D(g_allHolesArray[h].teeX - x, g_allHolesArray[h].teeY - y);
        if (d < best) {
            best = d;
            nearest = h;
        }
        d = Math_Distance2D(g_allHolesArray[h].aimX - x, g_allHolesArray[h].aimY - y);
        if (d < best) {
            best = d;
            nearest = h;
        }
        if (g_gameFairwayTargetPoints[h][2].x != -1) {
            d = Math_Distance2D(x - (g_gameFairwayTargetPoints[h][2].x >> 10),
                y - (g_gameFairwayTargetPoints[h][2].y >> 10));
            if (d < best) {
                best = d;
                nearest = h;
            }
        }
    }
    return nearest;
}

int IsNeighborWalkable(int row, int col, int dir)
{
    signed char t = g_gameTileTerrainKind[row * GRID_SIZE + col];
    if (g_squareTypeTable[t].lieGrade > 0 && t != TERRAIN_BUILDING && t != TERRAIN_HOME_SITE) {
        int nrow = row + g_dirRowStep[dir];
        int ncol = col + g_dirColStep[dir];
        if ((g_tileExtra[nrow * GRID_SIZE + ncol] & (TILE_PATH | TILE_SCENIC)) == 0
            && g_squareTypeTable[g_gameTileTerrainKind[nrow * GRID_SIZE + ncol]].lieGrade > 0
            && !IsPositionInvalid(nrow, ncol))
            return 1;
    }
    return 0;
}

void AppendLandmarkName(int kind, int verbose)
{
    switch (kind) {
    case 0: strcat(g_printBuffer, verbose ? " garden sundial" : "sundial"); break;
    case 1: strcat(g_printBuffer, verbose ? " traditional barn" : "barn"); break;
    case 2: strcat(g_printBuffer, verbose ? "n authentic Civil War cannon" : "Civil War cannon"); break;
    case 3: strcat(g_printBuffer, verbose ? " ancient stonehenge rock" : "stonehenge"); break;
    case 4: strcat(g_printBuffer, verbose ? "n operating water mill" : "water mill"); break;
    case 5: strcat(g_printBuffer, verbose ? "n unusual rock face" : "rock face"); break;
    case 6: strcat(g_printBuffer, verbose ? "n authentic Civil War statue" : "Civil War statue"); break;
    case 7: strcat(g_printBuffer, verbose ? " scenic New England lighthouse" : "lighthouse"); break;
    case 8: strcat(g_printBuffer, verbose ? " peaceful Buddha" : "Buddha"); break;
    case 9: strcat(g_printBuffer, verbose ? " Dutch windmill" : "windmill"); break;
    case 10: strcat(g_printBuffer, " historic statue"); break;
    case 11: strcat(g_printBuffer, verbose ? " haunting Easter Island head" : "Easter Island head"); break;
    case 12: strcat(g_printBuffer, verbose ? " exquisite pagoda" : "pagoda"); break;
    case 13: strcat(g_printBuffer, verbose ? "n historic Hatteras lighthouse" : "historic lighthouse"); break;
    case 14: strcat(g_printBuffer, verbose ? "n ornate oriental house" : "oriental house"); break;
    case 15: strcat(g_printBuffer, verbose ? " dusty dinosaur tarpit" : "dinosaur tarpit"); break;
    case 16: strcat(g_printBuffer, verbose ? "n unsightly water tower" : "water tower"); break;
    case 17: strcat(g_printBuffer, verbose ? "n unsightly radio antenna" : "radio antenna"); break;
    case 18: strcat(g_printBuffer, verbose ? "n unsightly oil pump" : "oil pump"); break;
    default: strcat(g_printBuffer, "landmark"); break;
    }
}

int GetSceneryName(int row, int col, int type)
{
    if (type == -1) {
        type = g_gameTileTerrainKind[row * GRID_SIZE + col];
    } else if (type & 0x100) {
        int v = type & 0xff;
        row = g_buildingTable[v].row;
        col = g_buildingTable[v].col;
        type = g_gameTileTerrainKind[row * GRID_SIZE + col];
    }
    if (type != TERRAIN_BUILDING && type != TERRAIN_HOME_SITE) {
        switch (g_squareTypeTable[type].category) {
        case 21:
        case 22: strcat(g_printBuffer, "landmark"); break;
        case CATEGORY_LIKE_OUT_OF_BOUNDS: strcat(g_printBuffer, "swamp tree"); break;
        case CATEGORY_WATER:
            if (type == TERRAIN_MARSH)
                strcat(g_printBuffer, "flower bed");
            else if (g_tileExtra[row * GRID_SIZE + col] & TILE_PATH)
                strcat(g_printBuffer, "scenic bridge");
            else if (g_tileVariationIdx[row * GRID_SIZE + col]
                     && g_gameTileTerrainKind[row * GRID_SIZE + col] == TERRAIN_WATER)
                strcat(g_printBuffer, "dolphin");
            else
                strcat(g_printBuffer, g_currentThemeId == THEME_DESERT ? "rock formation" : "fountain");
            break;
        case CATEGORY_OUT_OF_BOUNDS: strcat(g_printBuffer, "fountain"); break;
        case CATEGORY_TREES:
            if (type == TERRAIN_ELM_TREE)
                strcat(g_printBuffer, "scenic tree");
            else if (g_currentThemeId == THEME_DESERT)
                strcat(g_printBuffer, "scenic cactus");
            else if (type == TERRAIN_TREE)
                strcat(g_printBuffer, "scenic tree");
            else if (type == TERRAIN_PALM_TREE)
                strcat(g_printBuffer, "statue");
            else if (type == TERRAIN_PINE_TREE)
                strcat(g_printBuffer, "scenic tree");
            break;
        case CATEGORY_ROUGH:
            if (type != TERRAIN_ROUGH) {
                if (type == TERRAIN_RAVINE)
                    strcat(g_printBuffer, g_currentThemeId == THEME_DESERT ? "rock formation" : "scenic tree");
                else if (type == TERRAIN_ROCKS)
                    strcat(g_printBuffer, g_currentThemeId == THEME_DESERT ? "natural bridge" : "rock formation");
                else
                    strcat(g_printBuffer,
                        g_mapCourseList[g_myWorldCourseIdx].site == SITE_SPAIN ? "vineyard" : "rose bush");
            } else {
                if (g_tileExtra[row * GRID_SIZE + col] & TILE_FLOWERS) {
                    if (g_currentThemeId == THEME_DESERT) {
                        switch (g_tileVariationIdx[row * GRID_SIZE + col] % 5) {
                        case 0: strcat(g_printBuffer, "red poppy bed"); break;
                        case 1: strcat(g_printBuffer, "orange flowerbed"); break;
                        case 2: strcat(g_printBuffer, "purple iris patch"); break;
                        case 3: strcat(g_printBuffer, "blue foxglove"); break;
                        case 4: strcat(g_printBuffer, "pink sage bush"); break;
                        }
                    } else if (g_currentThemeId == THEME_TROPICAL) {
                        switch (g_tileVariationIdx[row * GRID_SIZE + col] % 5) {
                        case 0: strcat(g_printBuffer, "aqua hybiscus"); break;
                        case 1: strcat(g_printBuffer, "orange bougainvillea"); break;
                        case 2: strcat(g_printBuffer, "purple orchid"); break;
                        case 3: strcat(g_printBuffer, "blue hydrangea"); break;
                        case 4: strcat(g_printBuffer, "red rose bush"); break;
                        }
                    } else if (g_currentThemeId == THEME_PARK || g_currentThemeId == THEME_LINKS) {
                        switch (g_tileVariationIdx[row * GRID_SIZE + col] % 5) {
                        case 0: strcat(g_printBuffer, "green flowerbed"); break;
                        case 1: strcat(g_printBuffer, "yellow daisy bed"); break;
                        case 2: strcat(g_printBuffer, "purple iris patch"); break;
                        case 3: strcat(g_printBuffer, "white carnation"); break;
                        case 4: strcat(g_printBuffer, "red rose bush"); break;
                        }
                    }
                } else if (g_mapCourseList[g_myWorldCourseIdx].site == SITE_IRELAND) {
                    strcat(g_printBuffer, "Pot o'Gold");
                } else {
                    strcat(g_printBuffer, g_currentThemeId == THEME_DESERT ? "mysterious bones" : "ornamental grass");
                }
            }
            break;
        case CATEGORY_ROCKS: strcat(g_printBuffer, "rock formation"); break;
        case CATEGORY_SAND: strcat(g_printBuffer, "rhododedron"); break;
        default: strcat(g_printBuffer, "wildflower"); break;
        }
    } else {
        int v = g_tileVariationIdx[row * GRID_SIZE + col];
        short k = g_buildingTable[v].kind;
        if (k == SCENERY_LANDMARK) {
            AppendLandmarkName(g_buildingTable[v].landmark, 0);
            return 1;
        }
        if (k == SCENERY_FLOWER_BED)
            strcat(g_printBuffer, "flowerbed");
        else
            strcat(g_printBuffer, type == TERRAIN_BUILDING ? "landmark" : "home site");
        return 1;
    }
    return 1;
}

int GetMemberNameAtPosition(int row, int col, int append)
{
    int i;
    for (i = 0; i < 100; i++) {
        if (g_namedSceneryArray[i].x == row && g_namedSceneryArray[i].y == col) {
            if (append == 0)
                return 1;
            strcat(g_printBuffer, g_golferTypeArray[g_namedSceneryArray[i].owner].name);
            strcat(g_printBuffer, "'s ");
            GetSceneryName(row, col, -1);
            return 1;
        }
    }
    if (append)
        strcat(g_printBuffer, "Scenic");
    return 0;
}

void DrawCircleOnTerrain(int cx, int cy, int radius, unsigned color)
{
    int r = (radius << 10) / 25;
    int prevX = -1;
    int angle = 0;
    int n = 25;
    do {
        int sx, sy;
        int wx = cx + FixedPointSin(angle, r);
        int wy = cy + FixedPointCos(angle, r);
        if (WorldToScreen(wx, wy, &sx, &sy, 0)) {
            if (prevX == -1)
                TerrainMoveTo(sx, sy);
            else
                TerrainLineTo(sx, sy, color, 2, LINE_ALPHA);
            prevX = sx;
        } else {
            prevX = -1;
        }
        angle += 0x0aaaaaaa;
    } while (--n);
}

void StartNewGameOnCourse(int mapMode)
{
    if (g_runModeFlags & GAME_CHAMPIONSHIP)
        return;

    int course = ChooseWorldMapCourse(mapMode);
    OnWorldMapClosed();
    if (course == -1)
        return;

    int savedClock = g_clock;
    g_mapCourseList[g_myWorldCourseIdx].yearLeft = (char)g_careerPlayYear;
    g_clock = 0;
    g_currentThemeId = g_mapCourseList[course].kind;
    ResetAllGameState(0);
    InitNewGameData();
    g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_OTHER] = (short)g_gameCashHundreds;
    GenerateCourseMap(course);
    RebuildTerrainElevation();
    g_clock = savedClock;
    LoadCourseType(g_currentThemeId);
    ResetAndRebuildTerrain();

    int i = 0;
    while (1) {
        i++;
        if (i >= 12)
            break;
    }
}


void DisplayShotCommentary(int slot, int distPin, int yards, int quiet)
{
    int onScreen = 1;
    int subject = g_ballPathSlots[slot].golferId;
    int golfer = subject;
    int minX, maxX, minY, maxY;
    int sx, sy, tile, i;
    int savedMask;
    int side, tier, cap, pick, bit, tries, fromTile;
    int noneSeen;
    signed char level;
    signed char club;

    minX = maxX = g_golferPool[subject].screenX;
    minY = maxY = g_golferPool[subject].screenY;
    if (g_golferPool[g_golferPool[subject].partner].screenX < minX)
        minX = g_golferPool[g_golferPool[subject].partner].screenX;
    if (g_golferPool[g_golferPool[subject].partner].screenX > maxX)
        maxX = g_golferPool[g_golferPool[subject].partner].screenX;
    if (g_golferPool[g_golferPool[subject].partner].screenY < minY)
        minY = g_golferPool[g_golferPool[subject].partner].screenY;
    if (g_golferPool[g_golferPool[subject].partner].screenY > maxY)
        maxY = g_golferPool[g_golferPool[subject].partner].screenY;

    if (!quiet && subject != g_watchedPlayerIdx) {
        if (g_golferTypeArray[g_golferPool[subject].typeIdx].caps & TRAIT_PLAYFUL)
            OnTraitMatched(subject);
        if (g_golferTypeArray[g_golferPool[g_golferPool[subject].partner].typeIdx].caps & 0x10)
            OnTraitMatched(subject ^ 1);
    }

    if (minX > 0 && minY > 0) {
        g_golferPool[SCRATCH_GOLFER] = g_ballPathSlots[slot].golfer;
        g_golferPool[0x99] = g_ballPathSlots[slot].partnerCopy;
        subject = 0x98;
        g_golferPool[SCRATCH_GOLFER].partner = 0x99;
        for (i = 0; i < g_ballPathSlots[slot].count; i++) {
            if (WorldToScreen(g_ballPathSlots[slot].x[i], g_ballPathSlots[slot].y[i], &sx, &sy, 0)) {
                if (i == 0)
                    TerrainMoveTo(sx, sy + 1);
                else
                    TerrainLineTo(sx, sy + 1, 0x80000000, 2, LINE_ALPHA);
            }
        }
        for (i = 0; i < g_ballPathSlots[slot].count; i++) {
            if (WorldToScreen(g_ballPathSlots[slot].x[i], g_ballPathSlots[slot].y[i], &sx, &sy, 0)) {
                sy = sy + (-(g_ballPathSlots[slot].z[i] * g_cameraZoom * g_elevStep / 4)) / 16;
                if (i == 0)
                    TerrainMoveTo(sx, sy);
                else
                    TerrainLineTo(sx, sy, COLOR_WHITE, 2, LINE_ALPHA);
                if (sx < minX)
                    minX = sx;
                if (sx > maxX)
                    maxX = sx;
                if (sy < minY)
                    minY = sy;
                if (sy > maxY)
                    maxY = sy;
            }
        }
    } else {
        if (subject != g_watchedPlayerIdx) {
            g_golferHistoryPerCharacter[g_golferPool[subject].typeIdx].perHoleFlags[g_golferPool[subject].holeIdx] |= 1;
            return;
        }
        onScreen = 0;
    }

    g_printBuffer[0] = '\0';
    AppendGolferName(golfer, 0);
    tile = GetTileAtPosition(g_golferPool[golfer].ballX, g_golferPool[golfer].ballY);

    if (quiet) {
        if (g_golferPool[subject].club > 4) {
            switch (g_golferPool[subject].holeIdx % 2) {
            case 0: strcat(g_printBuffer, " scuffs a "); break;
            case 1: strcat(g_printBuffer, " fluffs a "); break;
            }
        } else {
            switch (g_golferPool[subject].holeIdx % 2) {
            case 0: strcat(g_printBuffer, " yanks a "); break;
            case 1: strcat(g_printBuffer, " flubs a "); break;
            }
        }
        AppendClubName(g_golferPool[subject].club);
        strcat(g_printBuffer, " into the ");
        strcat(g_printBuffer, g_squareTypeTable[tile].name);
    } else {
        club = g_golferPool[subject].club;
        if (club == 0)
            strcat(g_printBuffer, " launches a ");
        else if (club < 7)
            strcat(g_printBuffer, " knocks a ");
        else if (club != CLUB_PUTTER)
            strcat(g_printBuffer, " lofts a ");
        else
            strcat(g_printBuffer, " snakes a ");
        if (g_golferPool[subject].shotDifficulty > 10)
            strcat(g_printBuffer, "tricky ");
        if (g_golferPool[subject].shotDifficulty == 10)
            strcat(g_printBuffer, "tough ");
        if (club != 0) {
            if (club == CLUB_PUTTER)
                strcat(g_printBuffer, "putt ");
            else {
                AppendClubName(club);
                strcat(g_printBuffer, " shot ");
            }
        } else
            strcat(g_printBuffer, "drive ");
        strcat(g_printBuffer, itoa(yards, g_numberBuf, 10));
        strcat(g_printBuffer, " yards ");
        if (distPin) {
            if (GetTileAtPosition(g_golferPool[golfer].ballX, g_golferPool[golfer].ballY) == TERRAIN_GREEN) {
                strcat(g_printBuffer, "to within ");
                if (distPin <= 7) {
                    strcat(g_printBuffer, itoa(distPin * 2, g_numberBuf, 10));
                    strcat(g_printBuffer, " feet");
                } else {
                    strcat(g_printBuffer, itoa(distPin, g_numberBuf, 10));
                    strcat(g_printBuffer, " yards");
                }
                strcat(g_printBuffer, " of the pin");
            } else {
                strcat(g_printBuffer, "safely onto the fairway");
            }
        } else {
            if (g_golferPool[golfer].ballX == (g_allHolesArray[g_golferPool[golfer].holeIdx].aimX << 10) + 0x200
                && g_golferPool[golfer].ballY == (g_allHolesArray[g_golferPool[golfer].holeIdx].aimY << 10) + 0x200)
                strcat(g_printBuffer, "into the hole");
            else
                strcat(g_printBuffer, "inches from the hole");
        }
    }

    strcat(g_printBuffer, " at ");
    AppendHoleName(g_golferPool[subject].holeIdx);
    strcat(g_printBuffer, "!  ");

    if (!quiet && g_golferPool[g_golferPool[subject].partner].holeIdx > 0) {
        switch (g_golferPool[subject].rapport) {
        case 0:
            AppendGolferName(g_golferPool[subject].partner, 0);
            strcat(g_printBuffer, " looks on in amazement.");
            break;
        case 4:
            AppendGolferName(g_golferPool[subject].partner, 0);
            strcat(g_printBuffer, " can not believe ");
            strcat(g_printBuffer, IsGolferMale(g_golferPool[subject].partner) ? "his" : "her");
            strcat(g_printBuffer, " eyes.");
            break;
        case 3:
        case 5:
            AppendGolferName(g_golferPool[subject].partner, 0);
            strcat(g_printBuffer, " congratulates ");
            strcat(g_printBuffer, IsGolferMale(g_golferPool[subject].partner) ? "his" : "her");
            strcat(g_printBuffer, " partner.");
            break;
        case 1:
            AppendGolferName(g_golferPool[subject].partner, 0);
            strcat(g_printBuffer, " is equally delighted.");
            break;
        case 2:
            strcat(g_printBuffer, "'Rats', ");
            AppendGolferName(g_golferPool[subject].partner, 0);
            strcat(g_printBuffer, " mutters to ");
            strcat(g_printBuffer, IsGolferMale(g_golferPool[subject].partner) ? "himself" : "herself");
            strcat(g_printBuffer, ".");
            break;
        }
    }

    if (minX < 0x38) {
        if (minX < 0)
            onScreen = 0;
        else
            minX = 0x38;
    }
    if (minY < 0x38) {
        if (minY < 0)
            onScreen = 0;
        else
            minY = 0x38;
    }
    if (maxX > 0x2f8)
        onScreen = 0;
    if (maxY > 0x230)
        onScreen = 0;
    if (!onScreen && golfer != g_watchedPlayerIdx)
        return;

    savedMask = g_golferPool[subject].skillMask;
    g_golferPool[subject].skillMask = -1;
    if (g_ballPathSlots[slot].golferId == g_watchedPlayerIdx
        || (!(g_runModeFlags & GAME_TOURNAMENT) && g_ballPathSlots[slot].golferId == (g_watchedPlayerIdx ^ 1))) {

        side = (g_ballPathSlots[slot].golferId != g_watchedPlayerIdx);
        tier = GetHoleCountTier(g_nextOpenHoleIndex - 1);
        cap = tier * 2 + 6;
        if (tier == 3)
            cap = 0x63;
        pick = 0;
        for (i = 0; i < 12; i++)
            if (g_golferSkillGrades[side * 16 + i] >= cap)
                g_golferPool[subject].skillMask &= ~(1 << i);
        if (quiet) {
            g_golferPool[subject].skillMask = 0;
            for (i = 0; i < 12; i++)
                if (g_golferSkillGrades[side * 16 + i] > 3)
                    g_golferPool[subject].skillMask |= (short)(1 << i);
        }
        tries = 0;
        fromTile = GetTileAtPosition(g_golferPool[subject].shotFromX, g_golferPool[subject].shotFromY);
        if (fromTile == TERRAIN_GREEN && (g_golferPool[subject].skillMask & 0x10)) {
            pick = 0x10;
        } else {
            do {
                pick = 0;
                switch (g_rng.NextInt(12)) {
                case 0:
                    if (g_squareTypeTable[fromTile].lieGrade > 0 && (g_golferPool[subject].skillMask & 0x100))
                        pick = 0x100;
                    break;
                case 1:
                    if (((g_golferPool[subject].flags & 2) || !distPin) && (g_golferPool[subject].skillMask & 0x200))
                        pick = 0x200;
                    break;
                case 2:
                    if (!g_golferPool[subject].holeStrokes && g_golferPool[subject].club < 2
                        && (g_golferPool[subject].skillMask & 2))
                        pick = 2;
                    break;
                case 3:
                    if (!g_golferPool[subject].holeStrokes && g_golferPool[subject].club <= LAST_LONG_CLUB
                        && (g_golferPool[subject].skillMask & 4))
                        pick = 4;
                    break;
                case 4:
                    if (g_golferPool[subject].shotType == 1 && (g_golferPool[subject].skillMask & 0x20))
                        pick = 0x20;
                    break;
                case 5:
                    if (g_golferPool[subject].shotType == -1 && (g_golferPool[subject].skillMask & 0x40))
                        pick = 0x40;
                    break;
                case 6:
                    if (g_golferPool[subject].shotType == SHOT_HIGH_BACKSPIN
                        && (g_golferPool[subject].skillMask & 0x80))
                        pick = 0x80;
                    break;
                case 7:
                    if (yards > CalculateGolferMaxDistance(subject) && (g_golferPool[subject].skillMask & 1))
                        pick = 1;
                    break;
                case 8:
                    if (g_golferPool[subject].club >= LAST_LONG_CLUB && g_golferPool[subject].club < 0xb)
                        pick = 8;
                    break;
                case 9:
                    if (tries > 0x32 || !distPin) {
                        if (GetTileAtPosition(g_golferPool[subject].ballX, g_golferPool[subject].ballY)
                            == TERRAIN_GREEN) {
                            pick = 0x10;
                            if (distPin)
                                break;
                        }
                        pick = 0x200;
                    }
                    break;
                }
            } while (++tries < 0x64 && (pick == 0 || (pick & g_golferPool[subject].skillMask) == 0));
        }

        if (quiet && !pick)
            return;
        if (pick && g_golferPool[subject].persona) {
            bit = 0;
            while (!(pick & (1 << bit)))
                bit++;
            if (quiet) {
                if (g_golferSkillGrades[side * 16 + bit] <= 2)
                    return;
                g_golferSkillGrades[side * 16 + bit]--;
                strcat(g_printBuffer, " ");
                AppendGolferName(subject, 0);
                strcat(g_printBuffer, IsGolferMale(subject) ? " loses 10% of his " : " loses 10% of her ");
                strcat(g_printBuffer, g_skillNames[bit]);
                strcat(g_printBuffer, " skill.");
            } else if (g_golferSkillGrades[side * 16 + bit] < 10) {
                strcat(g_printBuffer, " ");
                AppendGolferName(subject, 0);
                if (g_golferSkillGrades[side * 16 + bit] != 0) {
                    level = g_golferSkillGrades[side * 16 + bit] + 1;
                    g_golferSkillGrades[side * 16 + bit] = level;
                    strcat(g_printBuffer, "'s '");
                    strcat(g_printBuffer, g_skillNames[bit]);
                    strcat(g_printBuffer, "' skill improves to ");
                    strcat(g_printBuffer, itoa(level, g_numberBuf, 10));
                    strcat(g_printBuffer, "0%! ");
                    if (g_golferSkillGrades[side * 16 + bit] == cap) {
                        strcat(g_printBuffer, "To further improve this skill you will need expand your course to a ");
                        strcat(g_printBuffer, g_courseTierNames[tier + 1]);
                        strcat(g_printBuffer, " course. ");
                    }
                } else {
                    g_golferSkillGrades[side * 16 + bit] = Clamp(3 - g_level, 1, 3);
                    strcat(g_printBuffer, " acquires a new skill: ");
                    strcat(g_printBuffer, g_skillNames[bit]);
                    strcat(g_printBuffer, "!");
                    savedMask |= 1 << bit;
                    if (golfer == g_watchedPlayerIdx)
                        g_skillMasks[0] |= (short)(1 << bit);
                    g_golferPool[golfer].skillMask |= (short)(1 << bit);
                }
                if (golfer == g_watchedPlayerIdx) {
                    g_golferPool[g_watchedPlayerIdx].ratings[bit] = g_golferSkillGrades[side * 16 + bit];
                    TriggerAccomplishment(2, g_golferPool[subject].x, g_golferPool[subject].y);
                    ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD, SOUND_FULL_VOLUME, 0, 0, 0);
                }
            }
        }
    }

    g_golferPool[subject].skillMask = savedMask;
    if (!onScreen) {
        ShowStoryMessage(0x800023e8, 1, golfer);
        ConfigureAndPlaySound(SOUND_CAMERA, SOUND_FULL_VOLUME, 0, 0, 0);
        return;
    }

    noneSeen = 1;
    for (i = 0; i < 0x4c; i++)
        if (g_golferHistoryPerCharacter[i].perHoleFlags[g_golferPool[golfer].holeIdx] & 1)
            noneSeen = 0;
    if (!(g_configBitMask & 2))
        noneSeen = 0;
    if ((noneSeen && g_watchedPlayerIdx == -1) || golfer == g_watchedPlayerIdx) {
        g_messageSpeaker = golfer;
        if (minY >= 0xc0)
            RenderSpeechBubbleWithPortrait(0x96, minY - 0xc0, 0x80002108, -2);
        else
            RenderSpeechBubbleWithPortrait(0x96, Clamp(maxY + 0x28, 0, 0x1c2), 0x80002108, -2);
        g_messageSpeaker = -1;
    }

    minX -= 0x28;
    minY -= 0x28;
    maxX += 0x28;
    maxY += 0x28;
    {
        int w = maxX - minX;
        int h = maxY - minY;
        unsigned color = quiet ? 0x80007d08 : 0x80007fff;
        g_simScreen.fill(minX - SIMFOTO_BORDER, minY - SIMFOTO_BORDER, w + 2 * SIMFOTO_BORDER, SIMFOTO_BORDER, color);
        g_simScreen.fill(minX - SIMFOTO_BORDER, minY, SIMFOTO_BORDER, h, color);
        g_simScreen.fill(maxX, minY, SIMFOTO_BORDER, h, color);
        g_simScreen.fill(minX - SIMFOTO_BORDER, maxY, w + 2 * SIMFOTO_BORDER, SIMFOTO_BORDER, color);
        g_simScreen.line(minX, minY, maxX, minY, COLOR_BLACK);
        g_simScreen.line(minX, minY, minX, maxY, COLOR_BLACK);
        g_simScreen.line(minX, maxY, maxX, maxY, COLOR_BLACK);
        g_simScreen.line(maxX, minY, maxX, maxY, COLOR_BLACK);
        DrawTextCenteredAtY("SimFoto", (minX + maxX) / 2, maxY + 4, COLOR_LIGHT_GREY);

        {
            int t = g_clock % 0x2000;
            g_printBuffer[0] = '\0';
            strcat(g_printBuffer, itoa(t / 0x400 + 3, g_numberBuf, 10));
            strcat(g_printBuffer, "'");
            strcat(g_printBuffer, itoa((t % 0x400) * 30 / 0x400 + 1, g_numberBuf, 10));
            strcat(g_printBuffer, "'");
            strcat(g_printBuffer, itoa(g_careerPlayYear + 0x7d1, g_numberBuf, 10));
            DrawTextClampedToScreen(g_printBuffer, maxX - 0x50, maxY - 0x10, COLOR_PALE_YELLOW);
        }
        ConfigureAndPlaySound(SOUND_CAMERA, SOUND_FULL_VOLUME, 0, 0, 0);
        g_golferHistoryPerCharacter[g_golferPool[golfer].typeIdx].perHoleFlags[g_golferPool[golfer].holeIdx] |= 1;
        g_simScreen.RenderFullSurface(0);
        if ((noneSeen && g_watchedPlayerIdx == -1) || golfer == g_watchedPlayerIdx) {
            WaitForInputWithTimeout(0);
            if (g_waitChar == 'y') {
                Rect r;
                strcpy(g_printBuffer, "snapshots\\");
                AppendGolferName(subject, 0);
                strcat(g_printBuffer, itoa(g_clock, g_numberBuf, 10));
                strcat(g_printBuffer, ".bmp");
                r.left = minX - 2;
                r.right = maxX + 2;
                r.top = minY - 2;
                r.bottom = maxY + 2;
                g_simScreen.SaveSnapshot(g_printBuffer, &r);
            }
        }
    }

    g_golferPool[golfer].flags |= GOLFER_ALT_REACTION;
    if (!quiet) {
        if ((g_golferPool[golfer].slotKind[0] & MSG_LINE_MASK) != 1) {
            SetGolferAction(golfer, 1, 1);
            g_golferPool[golfer].activity = ACTIVITY_PLEASED;
            g_golferPool[golfer].waitTimer = -0x63;
            g_golferPool[golfer].dir = (g_golferPool[golfer].dir - 2) & 7;
        }
        return;
    }
    if (g_golferPool[golfer].attitude >= 0)
        g_golferPool[golfer].attitude = -1;
}



void CheckGolferShotDeviation(int slot)
{
    if (g_cameraZoom < DEFAULT_ZOOM)
        return;
    int gi = g_ballPathSlots[slot].golferId;
    if (gi == (g_selectedGolferIndex & SELECTED_GOLFER_MASK))
        return;
    if ((g_golferPool[gi].persona & PERSONA_KIND) == PERSONA_ROSTER && gi != g_watchedPlayerIdx)
        return;
    int yards = Math_Distance2D(g_golferPool[gi].shotFromX - g_golferPool[gi].ballX,
                    g_golferPool[gi].shotFromY - g_golferPool[gi].ballY)
                * 25 / 1024;
    int dp = CalculateDistanceInYards(g_golferPool[gi].ballX, g_golferPool[gi].ballY,
        g_allHolesArray[g_golferPool[gi].holeIdx].aimX, g_allHolesArray[g_golferPool[gi].holeIdx].aimY);
    int tileFrom = GetTileAtPosition(g_golferPool[gi].shotFromX, g_golferPool[gi].shotFromY);
    int distPin = dp;
    int sum = 0;
    if (gi == g_watchedPlayerIdx) {
        for (int i = 0; i < 10; i++)
            sum += g_golferPool[gi].ratings[i];
    }
    int tileAt = GetTileAtPosition(g_golferPool[gi].ballX, g_golferPool[gi].ballY);
    if (g_squareTypeTable[tileAt].lieGrade > 0) {
        if (gi != g_watchedPlayerIdx)
            return;
        int r = g_rng.NextInt((g_squareTypeTable[tileFrom].lieGrade + 4) * 250 / (g_level + 2));
        if (r < g_squareTypeTable[tileAt].lieGrade * sum)
            DisplayShotCommentary(slot, dp, yards, 1);
        return;
    }
    int v = 20;
    if (g_golferPool[gi].shotDifficulty < 5)
        v = 25;
    if (g_golferPool[gi].shotDifficulty < 10)
        v += 5;
    if (g_golferPool[gi].shotDifficulty > 10)
        v -= 5;
    if (dp == 0) {
        v = v / 2;
    } else if (dp > 25 && g_golferPool[gi].shotDifficulty > 0
               && g_squareTypeTable[GetTileAtPosition(g_golferPool[gi].ballX, g_golferPool[gi].ballY)].lieGrade <= 0) {
        dp = 25;
        v = 200 / g_golferPool[gi].shotDifficulty;
    }
    if (gi == g_watchedPlayerIdx) {
        if (g_squareTypeTable[tileFrom].lieGrade > 0) {
            int t = Clamp(g_squareTypeTable[tileFrom].lieGrade, 0, 3);
            v += t * v / -6;
        }
        if (gi == g_watchedPlayerIdx) {
            int r = g_rng.NextInt(sum);
            v = (r + g_level * 4 + 5) * v / 32;
            if (InLandmarkInfluence(g_golferPool[gi].x, g_golferPool[gi].y, 4))
                v = v / 2;
        }
    }
    if (yards > (dp + 1) * v || ((g_forceBits & 0x10) && (gi == g_watchedPlayerIdx || g_watchedPlayerIdx == -1))) {
        DisplayShotCommentary(slot, distPin, yards, 0);
        g_forceBits &= ~0x10;
    }
}

void AddGolferTrailPoint(int slot)
{
    int n = g_ballPathSlots[slot].count;
    int gi = g_ballPathSlots[slot].golferId;
    g_ballPathSlots[slot].x[n] = n ? g_golferPool[gi].ballX : g_golferPool[gi].shotFromX;
    g_ballPathSlots[slot].y[n] = n ? g_golferPool[gi].ballY : g_golferPool[gi].shotFromY;
    g_ballPathSlots[slot].z[n] = n ? g_golferPool[gi].ballZ : 0;
    if (n < 31)
        g_ballPathSlots[slot].count++;
}

void StartShotTrail(int golfer)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (g_ballPathSlots[i].golferId == -1) {
            g_ballPathSlots[i].golferId = (short)golfer;
            g_ballPathSlots[i].partner = g_golferPool[golfer].partner;
            g_ballPathSlots[i].count = 0;
            AddGolferTrailPoint(i);
            g_ballPathSlots[i].golfer = g_golferPool[golfer];
            g_ballPathSlots[i].partnerCopy = g_golferPool[g_golferPool[golfer].partner];
            return;
        }
    }
}

void SpawnGolferDivot(int golfer)
{
    int n;
    int i;
    int r;
    n = 0;
    for (i = 0; i < 256; i++) {
        if (g_tornDivots[i].x != -1) {
            r = g_rng.NextInt(g_level * 1024 / 4 + 0x600);
            if (Math_Distance2D(g_tornDivots[i].originX - g_golferPool[golfer].ballX,
                    g_tornDivots[i].originY - g_golferPool[golfer].ballY)
                < r)
                n++;
        }
        if (g_forceBits & 1) {
            n = 0x62;
            g_forceBits &= ~1;
            SetGolferAction(golfer, 0x18, 0x14);
        }
        if (InLandmarkInfluence(g_golferPool[golfer].ballX, g_golferPool[golfer].ballY, 6))
            n = 0;
    }
    for (i = 0; i < 256; i++) {
        if (g_tornDivots[i].x == -1) {
            g_tornDivots[i].originX = g_golferPool[golfer].ballX;
            g_tornDivots[i].x = g_golferPool[golfer].ballX;
            g_tornDivots[i].originY = g_golferPool[golfer].ballY;
            g_tornDivots[i].y = g_golferPool[golfer].ballY;
            g_tornDivots[i].z = g_golferPool[golfer].ballZ;
            g_tornDivots[i].bearing = g_golferPool[golfer].ballBearing;
            g_tornDivots[i].speed = g_golferPool[golfer].ballSpeed / 2;
            g_tornDivots[i].climb = g_golferPool[golfer].ballClimb / 3;
            g_tornDivots[i].spawnTick = g_clock;
            return;
        }
    }
}

void UpdateDivots(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        if (g_tornDivots[i].x == -1)
            continue;
        g_tornDivots[i].x += FixedPointSin(g_tornDivots[i].bearing, g_tornDivots[i].speed / 16);
        g_tornDivots[i].y -= FixedPointCos(g_tornDivots[i].bearing, g_tornDivots[i].speed / 16);
        g_tornDivots[i].z += g_tornDivots[i].climb / 32;
        if (g_tornDivots[i].z <= 0) {
            int now = g_clock;
            g_tornDivots[i].climb = 0;
            g_tornDivots[i].speed = 0;
            g_tornDivots[i].z = 0;
            if (now > g_tornDivots[i].spawnTick + 0x400)
                g_tornDivots[i].x = -1;
        }
        g_tornDivots[i].speed -= g_tornDivots[i].speed >> 4;
        if (g_tornDivots[i].z != 0 || g_tornDivots[i].climb != 0)
            g_tornDivots[i].climb -= 0x40;
    }
}

void AppendScriptCommand(int row, int col, int cmd)
{
    int n = g_scriptCount;
    if (n < 0x100) {
        g_scriptCommandCellRows[n] = row;
        g_scriptCommandCellCols[n] = col;
        g_scriptCmd[n] = cmd;
        g_scriptCount = n + 1;
    }
}

void PlayScriptStep()
{
    int i, row, col;
    int cmd, hole, id;

    if (g_wantedCamTileX != -1) {
        if (g_cameraTileX != g_wantedCamTileX || g_cameraTileY != g_wantedCamTileY) {
            g_cameraTileX += Sign(g_wantedCamTileX - g_cameraTileX);
            g_cameraTileY += Sign(g_wantedCamTileY - g_cameraTileY);
            return;
        }
    }

    i = g_courseScriptPlayIndex++;
    if (g_courseScriptPlayIndex > g_scriptCount) {
        g_runModeFlags &= ~GAME_SCRIPT_PLAYING;
        g_wantedCamTileX = -1;
        return;
    }

    row = g_scriptCommandCellRows[i];
    col = g_scriptCommandCellCols[i];

    if (g_scriptCmd[i] & 0x80) {
        g_tileExtra[g_scriptCommandCellRows[i] * GRID_SIZE + g_scriptCommandCellCols[i]] ^= g_scriptCmd[i] & ~0x80;
    } else if (g_scriptCmd[i] & 0x100) {
        PlaceBuildingOnTerrain(row, col, g_scriptCmd[i] & ~0x100, 0);
        cmd = g_scriptCmd[i] & ~0x100;
        g_selectedBuildingKind = cmd;
        if (cmd == 8 || cmd == 7 || cmd == 0xc || cmd == 9 || cmd == 0xb || cmd == 0xd || cmd == 0xe || cmd == 4) {
            g_tileExtra[row * GRID_SIZE + col] |= TILE_ANIMATING;
            g_tileAnimFrame[row * GRID_SIZE + col] = 0;
            ConfigureAndPlaySound(SOUND_CONSTRUCTION, SOUND_FULL_VOLUME, 0, -g_rng.NextInt(300), 0);
        }
        g_selectedBuildingKind = -1;
    } else {
        if (g_scriptCmd[i] == 0x7f)
            g_pointElevationMap[g_scriptCommandCellRows[i]][g_scriptCommandCellCols[i]]++;
        if (g_scriptCmd[i] == 0x7e)
            g_pointElevationMap[g_scriptCommandCellRows[i]][g_scriptCommandCellCols[i]]--;
        if (g_scriptCmd[i] < 0x7d) {
            if (g_scriptCmd[i] == 0)
                hole = g_nextOpenHoleIndex;
            else
                hole = g_nextOpenHoleIndex - 1;

            if (g_scriptCmd[i] == 0 || (g_scriptCmd[i] == 2 && g_clock % 6 == 0)) {
                g_allHolesArray[g_nextOpenHoleIndex].par = 4;
                id = SpawnGolfer(0);
                g_golferPool[id].holeIdx = (signed char)hole;
                g_golferPool[id].ballX = (g_scriptCommandCellRows[i] << 10) + 0x200;
                g_golferPool[id].x = (g_scriptCommandCellRows[i] << 10) + 0x200;
                g_golferPool[id].ballY = (g_scriptCommandCellCols[i] << 10) + 0x200;
                g_golferPool[id].y = (g_scriptCommandCellCols[i] << 10) + 0x200;
                g_golferPool[id].holeStrokes = 1;
                g_golferPool[id].waitTimer = 2;
                g_golferPool[id].partner = (short)(id ^ 1);

                id = SpawnGolfer(0);
                g_golferPool[id].holeIdx = (signed char)hole;
                g_golferPool[id].ballX = g_scriptCommandCellRows[i] << 10;
                g_golferPool[id].x = g_scriptCommandCellRows[i] << 10;
                g_golferPool[id].ballY = g_scriptCommandCellCols[i] << 10;
                g_golferPool[id].y = g_scriptCommandCellCols[i] << 10;
                g_golferPool[id].holeStrokes = 1;
                g_golferPool[id].waitTimer = 8;
                g_golferPool[id].partner = (short)(id ^ 1);
            }

            g_gameTileTerrainKind[g_scriptCommandCellRows[i] * GRID_SIZE + g_scriptCommandCellCols[i]] =
                (signed char)g_scriptCmd[i];
            if (g_squareTypeTable[g_scriptCmd[i]].category == CATEGORY_TREES) {
                g_tileExtra[g_scriptCommandCellRows[i] * GRID_SIZE + g_scriptCommandCellCols[i]] |= TILE_ANIMATING;
                g_tileAnimFrame[g_scriptCommandCellRows[i] * GRID_SIZE + g_scriptCommandCellCols[i]] = 0;
            }

            if (g_scriptCmd[i] == 1) {
                if (g_allHolesArray[g_nextOpenHoleIndex].aimX == 0) {
                    g_allHolesArray[g_nextOpenHoleIndex].aimX = row;
                    g_tileExtra[row * GRID_SIZE + col] = g_nextOpenHoleIndex | TILE_PIN;
                    g_allHolesArray[g_nextOpenHoleIndex].aimY = col;
                    if (g_allHolesArray[g_nextOpenHoleIndex].teeX != 0) {
                        g_wantedCamTileX = (g_allHolesArray[g_nextOpenHoleIndex].teeX + row) / 2;
                        g_wantedCamTileY = (g_allHolesArray[g_nextOpenHoleIndex].teeY + col) / 2;
                        g_allHolesArray[g_nextOpenHoleIndex].par = 4;
                        g_nextOpenHoleIndex = g_nextOpenHoleIndex + 1;
                    }
                }
            }
            if (g_scriptCmd[i] == 0) {
                g_allHolesArray[g_nextOpenHoleIndex].teeX = row;
                g_allHolesArray[g_nextOpenHoleIndex].teeY = col;
                g_tileExtra[row * GRID_SIZE + col] |= g_nextOpenHoleIndex;
                if (g_allHolesArray[g_nextOpenHoleIndex].aimX != 0) {
                    g_wantedCamTileX = (g_allHolesArray[g_nextOpenHoleIndex].aimX + row) / 2;
                    g_wantedCamTileY = (g_allHolesArray[g_nextOpenHoleIndex].aimY + col) / 2;
                    g_allHolesArray[g_nextOpenHoleIndex].par = 4;
                    g_nextOpenHoleIndex = g_nextOpenHoleIndex + 1;
                }
            }
        }
    }

    RecalculateAllWaterShoreDepths();
    UpdateBuildingAccessibility();
    g_editedTileRowIdx = g_scriptCommandCellRows[i];
    g_editedTileColIdx = g_scriptCommandCellCols[i];
    RebuildTerrainElevation();
}

int TransferCourseScript(const char *file, int reading)
{
    if (!reading) {
        g_gameCurrentHandle = _open(file, 0x8301, _S_IWRITE);
        _write(g_gameCurrentHandle, &g_scriptCount, 4);
        _write(g_gameCurrentHandle, g_scriptCommandCellRows, g_scriptCount * 4);
        _write(g_gameCurrentHandle, g_scriptCommandCellCols, g_scriptCount * 4);
        _write(g_gameCurrentHandle, g_scriptCmd, g_scriptCount * 4);
        _close(g_gameCurrentHandle);
        return 1;
    }
    g_gameCurrentHandle = _open(file, 0x8000);
    if (g_gameCurrentHandle == -1)
        return 0;
    _read(g_gameCurrentHandle, &g_scriptCount, 4);
    _read(g_gameCurrentHandle, g_scriptCommandCellRows, g_scriptCount * 4);
    _read(g_gameCurrentHandle, g_scriptCommandCellCols, g_scriptCount * 4);
    _read(g_gameCurrentHandle, g_scriptCmd, g_scriptCount * 4);
    _close(g_gameCurrentHandle);
    g_courseScriptPlayIndex = 0;
    return 1;
}

void SetTileDataAt(int row, int col, int undo, unsigned char refund)
{
    g_demolishPreviousRecs[row * 50 + col] = undo;
    g_gameDemolitionRefunds[row * 50 + col] = refund;
}

int BuildDemolishTooltipText(int row, int col)
{
    int b;
    char *s;
    if (IsPositionInvalid(row, col)) {
        strcpy(g_printBuffer, "Out of bounds");
        return 0;
    }
    g_printBuffer[0] = 0;
    if (g_tileExtra[row * 50 + col] & TILE_PERMANENT) {
        strcpy(g_printBuffer, "Permanent");
        return 0;
    }
    if ((g_tileExtra[row * 50 + col] & TILE_BUILDING) && (b = FindBuildingAtTile(row, col)) != -1) {
        strcpy(g_printBuffer, "Demolish ");
        s = g_facilityTypes[g_buildingTable[b].kind].name;
        strcat(g_printBuffer, s);
        return 1;
    } else {
        signed char t = g_demolishPreviousRecs[row * 50 + col];
        if (t != -1) {
            if (t & 0x80) {
                strcpy(g_printBuffer, "Remove Flowers");
                if ((t & 0x7f) == 0)
                    strcpy(g_printBuffer, "Remove Path");
                if ((t & 0x7f) == 1)
                    strcpy(g_printBuffer, "Remove Bench");
                if ((t & 0x7f) == 0x10)
                    strcpy(g_printBuffer, "Remove Scenic Tree");
                if ((t & 0x7f) == 0x13)
                    strcpy(g_printBuffer, "Remove Scenic Bridge");
                return 1;
            }
            if (t < 0x7d) {
                strcat(g_printBuffer, "Reset to ");
                s = g_squareTypeTable[t].name;
                strcat(g_printBuffer, s);
                return 1;
            }
        }
    }
    if (g_tileExtra[row * 50 + col] & TILE_BENCH) {
        strcpy(g_printBuffer, "Remove Bench");
        return 0;
    }
    if ((g_tileExtra[row * 50 + col] & TILE_BUILDING) && (b = FindBuildingAtTile(row, col)) != -1) {
        strcpy(g_printBuffer, "Remove ");
        strcat(g_printBuffer, g_facilityTypes[g_buildingTable[b].kind].name);
        return 0;
    }
    if (g_tileExtra[row * 50 + col] & TILE_PATH) {
        strcpy(g_printBuffer, "Remove Path");
        return 0;
    }
    if (g_tileExtra[row * 50 + col] & TILE_FLOWERS) {
        strcpy(g_printBuffer, "Remove Flowers");
        return 0;
    }
    g_printBuffer[0] = 0;
    return 0;
}

void DemolishTileAtPosition(int row, int col)
{
    if (IsPositionInvalid(row, col))
        return;

    g_tileExtra[row * 50 + col] &= ~TILE_SCENIC;
    short w = g_tileExtra[row * 50 + col];
    if (w & TILE_PERMANENT) {
        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
        return;
    }
    if (w & TILE_BUILDING) {
        int n = FindBuildingAtTile(row, col);
        if (n != -1) {
            strcpy(g_printBuffer, "Do you really want to\ndemolish your ");
            strcat(g_printBuffer, g_facilityTypes[g_buildingTable[n].kind].name);
            strcat(g_printBuffer, "?\n Yes I do.\n Ooops, no.\n");
            if (ShowActionDialog(0x190, 0x64, 1, 1, 0))
                return;
            DemolishBuilding(n);
            return;
        }
    }

    char c = g_demolishPreviousRecs[row * 50 + col];
    if (c != -1) {
        if (c & 0x80) {
            if ((c & 0x7f) == 0x10)
                TrySpawnWildlifeHabitat(row, col, 4);

            if ((char)(g_demolishPreviousRecs[row * 50 + col] & 0x7f) == 0x13)
                g_tileExtra[row * 50 + col] &= ~TILE_PATH;
            char item = (char)(g_demolishPreviousRecs[row * 50 + col] & 0x7f);
            if (item == 1) {
                g_tileExtra[row * 50 + col] &= ~TILE_BENCH;
            } else if (item == 0) {
                g_tileExtra[row * 50 + col] &= ~TILE_PATH;
            } else {
                short v = g_tileExtra[row * 50 + col];
                if (v & TILE_FLOWERS)
                    g_tileExtra[row * 50 + col] = v & ~TILE_FLOWERS;
                else
                    g_tileExtra[row * 50 + col] = v & ~0x1220;
            }
        } else if (c < 0x7d) {
            short d = g_tileExtra[row * 50 + col];
            int hole = d & TILE_HOLE_MASK;
            if ((g_runModeFlags & GAME_TOURNAMENT)
                && (g_gameTileTerrainKind[row * 50 + col] == 0
                    || (g_gameTileTerrainKind[row * 50 + col] == TERRAIN_GREEN && (d & TILE_PIN)))) {
                ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
                return;
            }

            int tt = g_gameTileTerrainKind[row * 50 + col];
            switch (tt) {
            case 0:
                if (hole) {
                    g_allHolesArray[hole].teeX = 0;
                    g_allHolesArray[hole].par = 0;
                    g_tileExtra[row * 50 + col] = d & ~TILE_HOLE_MASK;
                    if (hole < g_nextOpenHoleIndex)
                        g_nextOpenHoleIndex = hole;
                }
                break;
            case 1:
                if (hole) {
                    g_allHolesArray[hole].par = 0;
                    g_allHolesArray[hole].aimX = 0;
                    g_tileExtra[row * 50 + col] = d & ~TILE_HOLE_MASK;
                    if (hole < g_nextOpenHoleIndex)
                        g_nextOpenHoleIndex = hole;
                }
                break;
            }
            TrySpawnWildlifeHabitat(row, col, c);
        }

        g_gameCashHundreds += g_gameDemolitionRefunds[row * 50 + col];
        AddMoneyPopup(g_gameDemolitionRefunds[row * 50 + col], row * 1024 + 512, col * 1024 + 512, -1);
        g_demolishPreviousRecs[row * 50 + col] = -1;
        g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_BUILD_COURSE] += g_gameDemolitionRefunds[row * 50 + col];
        g_editedTileRowIdx = row;
        g_editedTileColIdx = col;
        RebuildTerrainElevation();
        RecalculateAllWaterShoreDepths();
        return;
    } else {
        short extra = g_tileExtra[row * 50 + col];
        if (extra & TILE_BENCH) {
            g_tileExtra[row * 50 + col] = extra & ~TILE_BENCH;
            return;
        }
        if (extra & TILE_BUILDING) {
            int n = FindBuildingAtTile(row, col);
            if (n != -1) {
                strcpy(g_printBuffer, "Do you really want to\ndemolish your ");
                strcat(g_printBuffer, g_facilityTypes[g_buildingTable[n].kind].name);
                strcat(g_printBuffer, "?\n Yes I do.\n Ooops, no.\n");
                if (ShowActionDialog(0x190, 0x64, 1, 1, 0))
                    return;
                DemolishBuilding(n);
                return;
            }
        }
        short extraNow = g_tileExtra[row * 50 + col];
        if (extraNow & TILE_PATH) {
            g_tileExtra[row * 50 + col] = extraNow & ~TILE_PATH;
            return;
        }
        if (extraNow & TILE_FLOWERS) {
            g_tileExtra[row * 50 + col] = extraNow & ~TILE_FLOWERS;
            return;
        }
        int tt = g_gameTileTerrainKind[row * 50 + col];
        if (tt == 0) {
            int hole = extraNow & TILE_HOLE_MASK;
            g_allHolesArray[hole].par = 0;
            g_allHolesArray[hole].teeX = 0;
            if (hole < g_nextOpenHoleIndex)
                g_nextOpenHoleIndex = hole;
        } else if (tt == TERRAIN_GREEN && (extraNow & TILE_PIN)) {
            int hole = extraNow & TILE_HOLE_MASK;
            g_allHolesArray[hole].par = 0;
            g_allHolesArray[hole].aimX = 0;
            if (hole < g_nextOpenHoleIndex)
                g_nextOpenHoleIndex = hole;
        }
    }

    g_editedTileRowIdx = row;
    g_editedTileColIdx = col;
    RebuildTerrainElevation();
    RecalculateAllWaterShoreDepths();
}

void AppendClubName(int club)
{
    switch (club) {
    case 0: strcat(g_printBuffer, "Driver"); break;
    case 1: strcat(g_printBuffer, "3 Wood"); break;
    case 2: strcat(g_printBuffer, "4 Wood"); break;
    case 3: strcat(g_printBuffer, "2 Iron"); break;
    case 4: strcat(g_printBuffer, "3 Iron"); break;
    case 5: strcat(g_printBuffer, "4 Iron"); break;
    case 6: strcat(g_printBuffer, "5 Iron"); break;
    case 7: strcat(g_printBuffer, "6 Iron"); break;
    case 8: strcat(g_printBuffer, "7 Iron"); break;
    case 9: strcat(g_printBuffer, "8 Iron"); break;
    case 10: strcat(g_printBuffer, "9 Iron"); break;
    case 11: strcat(g_printBuffer, "Lob Wedge"); break;
    case 12: strcat(g_printBuffer, "Sand Wedge"); break;
    case 13: strcat(g_printBuffer, "Putter"); break;
    }
}

void ProcessHireEmployeeSelection()
{
    int sel = RenderHireEmployeeScreen();
    flush_input();
    if (sel >= 0 && sel < 12) {
        if (GetHoleCountTier(g_nextOpenHoleIndex - 1) < 1 && sel % 3 == 2) {
            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
            strcpy(g_printBuffer, "You need to build up to a Daily Fee course (6 or more holes) before you can hire "
                                  "skilled employees.");
            ShowStoryMessage(COLOR_DARK_RED, 1, -1);
            return;
        }
        if (sel != -1) {
            int cost = 0;
            if (CheckCanAfford(cost)) {
                int i, a;
                g_gameCashHundreds -= cost;
                g_annualStatsRecords[g_careerPlayYear % 100].v[LEDGER_SALARIES] -= cost;
                i = SpawnEmployee(-2 - sel / 3);
                if (g_allHolesArray[1].par != 0) {
                    g_staffSlots[i].postRow =
                        g_dirRowStep[(g_allHolesArray[1].teeDir + 2) & 7] + g_allHolesArray[1].teeX;
                    g_staffSlots[i].postCol =
                        g_dirColStep[(g_allHolesArray[1].teeDir + 2) & 7] + g_allHolesArray[1].teeY;
                } else {
                    g_staffSlots[i].postRow = (g_rng.NextInt(2) ? 3 : -3) + g_allHolesArray[CLUBHOUSE_HOLE].teeX;
                    g_staffSlots[i].postCol = (g_rng.NextInt(2) ? 3 : -3) + g_allHolesArray[CLUBHOUSE_HOLE].teeY;
                }
                if (sel % 3 == 2)
                    g_staffSlots[i].flags |= STAFF_UPGRADED;
                a = CalculateAngle(((int)g_staffSlots[i].postRow << 10) - g_staffSlots[i].posX + 0x200,
                    ((int)g_staffSlots[i].postCol << 10) - g_staffSlots[i].posY + 0x200);
                g_staffSlots[i].dir = ((((a >> 28) & 0xf) + 1) >> 1 & 7);
                g_staffSlots[i].posX += (g_dirRowStep[g_staffSlots[i].dir] << 10) + 0x200;
                g_staffSlots[i].posY += (g_dirColStep[g_staffSlots[i].dir] << 10) + 0x200;
                g_staffSlots[i].hiredAt = (short)(g_clock / 1024);
            }
        }
    }
}

int Math_Distance2D(int dx, int dy)
{
    int scale = 1;

    if (abs(dx) > 0x4000) {
        scale = 8;
        dx /= 8;
    }
    if (abs(dy) > 0x4000) {
        dy /= 8;
        scale *= 8;
    }
    return (int)(sqrt((double)(dx * dx + dy * dy)) * scale);
}

int LoadOrGenerateHighScores()
{
    WIN32_FIND_DATAA fd;
    int i;

    if (FindFirstFileA("top10.sve", &fd) == INVALID_HANDLE_VALUE) {
        memset(g_topTenScoresArray, 0, sizeof(g_topTenScoresArray));
        for (i = 0; i < 10; i++) {
            int rank = 9 - i;
            g_topTenScoresArray[i].cash = g_rng.NextInt(800) + rank * 800;
            g_topTenScoresArray[i].fun = g_rng.NextInt(100) + rank * 100;
            g_topTenScoresArray[i].skill = g_rng.NextInt(90) + rank * 90;
            g_topTenScoresArray[i].level = (short)(rank / 3);
            g_topTenScoresArray[i].courseId = -1;
            strcpy(g_printBuffer, "a.c. dye");
            g_printBuffer[0] = (char)(g_printBuffer[0] + g_rng.NextInt(26));
            g_printBuffer[2] = (char)(g_printBuffer[2] + g_rng.NextInt(24));
            strcpy(g_topTenScoresArray[i].name, g_printBuffer);
            strcpy(g_topTenScoresArray[i].course, "Harbour Lights GC");
        }
        return 0;
    }

    strcpy(g_printBuffer, "top10.sve");
    g_gameCurrentHandle = _open(g_printBuffer, 0x8000);
    _read(g_gameCurrentHandle, g_topTenScoresArray, sizeof(g_topTenScoresArray));
    _close(g_gameCurrentHandle);
    return 1;
}

int TransferSaveBlock(void *block, unsigned int count)
{
    if (g_sveFileLoadFlag)
        return _read(g_gameCurrentHandle, block, count);
    return _write(g_gameCurrentHandle, block, count);
}

void TransferGameState(int reading)
{
    g_sveFileLoadFlag = reading;
    TransferSaveBlock(&g_courseId, 4);
    TransferSaveBlock(&g_nextOpenHoleIndex, 4);
    TransferSaveBlock(&g_simTotalLengthYards, 4);
    TransferSaveBlock(&g_gameCourseTotalPar, 4);
    TransferSaveBlock(&g_currentThemeId, 1);
    TransferSaveBlock(&g_courseTotalFunRate, 4);
    TransferSaveBlock(&g_clubSkillsScore, 4);
    TransferSaveBlock(&g_lengthAbilitySum, 4);
    TransferSaveBlock(&g_gameCourseAccurPointsTotal, 4);
    TransferSaveBlock(&g_totalImagination, 4);
    TransferSaveBlock(&g_holeAwardCount, 4);
    TransferSaveBlock(&g_homesSold, 4);
    TransferSaveBlock(&g_winningPtsTotal, 4);
    TransferSaveBlock(&g_storiesNum, 4);
    TransferSaveBlock(&g_gameCashHundreds, 4);
    TransferSaveBlock(&g_myWorldCourseIdx, 4);

    TransferSaveBlock(g_allHolesArray, 0x28a0);
    TransferSaveBlock(g_gameTileTerrainKind, 0x9c4);
    TransferSaveBlock(g_tileVariationIdx, 0x9c4);
    TransferSaveBlock(g_tileExtra, 0x1388);
    TransferSaveBlock(g_simGolferWalkCostMap, 0x9c4);
    TransferSaveBlock(g_pointElevationMap, 0xa29);
    TransferSaveBlock(g_bilerpGrid, 0x169);
    TransferSaveBlock(g_buildingTable, 0x1000);
    TransferSaveBlock(&g_celebHomesBought, 4);
    TransferSaveBlock(&g_previousSGAMark, 4);
    TransferSaveBlock(g_golferPool, 0x9c00);
    TransferSaveBlock(g_staffSlots, 0x1300);

    TransferSaveBlock(&g_clock, 4);
    TransferSaveBlock(&g_unlockedBldgTypeLimit, 4);
    TransferSaveBlock(&g_landPurchaseNum, 4);
    TransferSaveBlock(&g_level, 4);
    TransferSaveBlock(g_monthlySkillScore, 0x3e8);
    TransferSaveBlock(g_moneyLine, 0x3e8);
    TransferSaveBlock(g_funRatingHistPoints, 0x3e8);
    TransferSaveBlock(&g_calculatedGreenFee, 4);
    TransferSaveBlock(&g_proMatchStakeLvl, 4);
    TransferSaveBlock(&g_curNextSpawnPos, 4);
    TransferSaveBlock(g_talentKindCount, 0x80);
    TransferSaveBlock(&g_knownEventsLowMask, 4);
    TransferSaveBlock(g_monthEventKindHistory, 0x3e8);
    TransferSaveBlock(&g_freeLandmarks, 4);
    TransferSaveBlock(&g_simPostedHoleCategories, 4);
    TransferSaveBlock(&g_simLatestGolferNum, 4);
    TransferSaveBlock(g_annualStatsRecords, 0x7d0);
    TransferSaveBlock(&g_shownLandmarkBitmask, 4);
    TransferSaveBlock(&g_pendingProIndex, 4);
    TransferSaveBlock(&g_ceoLadderRung, 4);
    TransferSaveBlock(&g_curLandmarkTotal, 4);

    TransferSaveBlock(g_skillClassStatRows, 0x1700);
    TransferSaveBlock(g_top10CoursePoints, 0xf0);
    TransferSaveBlock(g_gameFairwayTargetPoints, 0x1c8);
    TransferSaveBlock(&g_curPreviousConvoClock, 4);
    TransferSaveBlock(g_tornDivots, 0x2400);
    TransferSaveBlock(g_ballPathSlots, 0x3880);
    TransferSaveBlock(g_golferHistoryPerCharacter, 0xe70);
    TransferSaveBlock(&g_watchedPlayerIdx, 4);
    TransferSaveBlock(g_golferSkillGrades, 0x20);
    TransferSaveBlock(g_homeVIPsTbl, 0x1770);
    TransferSaveBlock(g_miscGameFlags, 0x12);
    TransferSaveBlock(g_namedSceneryArray, 0x320);
    TransferSaveBlock(g_backgroundActors, 0x40);
    TransferSaveBlock(g_gameAnimals, 0xa00);
    TransferSaveBlock(g_golferTypeArray, 0xa640);
    TransferSaveBlock(g_phrasesPerGolferType, 0x19a28);
    TransferSaveBlock(&g_winnersPurseK, 4);

    TransferSaveBlock(g_posReactionTileCount, 0x9c4);
    TransferSaveBlock(g_gameDispleasedTally, 0x9c4);
    TransferSaveBlock(g_ballStopCountMap, 0x9c4);
    TransferSaveBlock(g_gameForSaleLandGrid, 0x9c4);
    TransferSaveBlock(&g_runModeFlags, 4);
    TransferSaveBlock(g_accomp, 0x420);
    TransferSaveBlock(g_mapCourseList, 0x30e);
    TransferSaveBlock(g_gameMembershipTrendLine, 0x3e8);
    TransferSaveBlock(g_activeThemeDir, 0x64);
    TransferSaveBlock(&g_knownEventsHighMask, 4);
    TransferSaveBlock(g_txtHeapBuffer, 0x1002);
    TransferSaveBlock(g_strOffsetTbl, 0x100);
    TransferSaveBlock(g_stringPoolLens, 0x100);
    TransferSaveBlock(&g_nextGolferOrder, 4);
    TransferSaveBlock(&g_moneyWarnCnt, 4);
    TransferSaveBlock(g_bestTenScoresTable, 0x28);
    TransferSaveBlock(&g_curTournPrepBits, 4);
    TransferSaveBlock(g_demolishPreviousRecs, 0x9c4);
    TransferSaveBlock(g_gameDemolitionRefunds, 0x9c4);
    TransferSaveBlock(&g_configBitMask, 4);

    if (g_runModeFlags & GAME_SAVE_CUSTOM_TYPES)
        TransferSaveBlock(&g_golferTypeArray[76], 0x8c0);
    if (g_runModeFlags & GAME_TOURNAMENT) {
        TransferSaveBlock(g_gameGolferTypesBak, 0xb7c0);
        TransferSaveBlock(g_phrasesPerGolferTypeSave, 0x19a28);
    }
}

void SaveGame(const char *name)
{
    char saveName[120];

    strcpy(saveName, name);
    strcpy(g_printBuffer, "saved games\\");
    strcat(g_printBuffer, saveName);
    strcat(g_printBuffer, ".sve");
    g_gameCurrentHandle = _open(g_printBuffer, 0x8301, _S_IWRITE);
    strcpy(g_printBuffer, "-");
    AppendCourseName(0);
    strcat(g_printBuffer, ", ");
    strcat(g_printBuffer, itoa(g_clock % 1024 * 30 / 1024 + 1, g_numberBuf, 10));
    strcat(g_printBuffer, " ");
    FormatDateString(g_clock);
    _write(g_gameCurrentHandle, g_printBuffer, 100);
    TransferGameState(0);
    _close(g_gameCurrentHandle);
}

void SaveCourseToFile()
{
    char fileName[100];

    strcpy(fileName, g_printBuffer);
    g_simScreen.SetFontsForStyles(&g_boldArialFont, 0, 0, 0);
    SetCourseFileName(fileName);
    g_gameCurrentHandle = _open(fileName, 0x8301, _S_IWRITE);
    g_printBuffer[0] = '\0';
    AppendCourseName(0);
    strcat(g_printBuffer, " designed by ");
    strcat(g_printBuffer, g_golferTypeArray[0].name);
    _write(g_gameCurrentHandle, g_printBuffer, 100);
    strcpy(g_printBuffer, "Course Record: ");
    strcat(g_printBuffer, itoa(g_bestTenScoresTable[0], g_numberBuf, 10));
    strcat(g_printBuffer, " by ");
    AppendPooledString(0x14);
    _write(g_gameCurrentHandle, g_printBuffer, 100);
    TransferCourseState(0);
    _write(g_gameCurrentHandle, "*PCXFILE", 8);
    _close(g_gameCurrentHandle);
}

void LoadCourseFromFile(int stage)
{
    char name[100];
    int hdr[4];

    strcpy(name, g_printBuffer);
    g_gameCurrentHandle = _open(name, 0x8000);
    g_printBuffer[0] = '\0';
    _read(g_gameCurrentHandle, g_printBuffer, 100);
    if (stage == 2) {
        _close(g_gameCurrentHandle);
        return;
    }
    if (stage == 0) {
        strcpy(g_printBuffer, "Loading Course");
        g_messageSpeaker = -1;
        RenderSpeechBubbleWithPortrait(200, 200, (int)0x80001284u, -2);
        g_simScreen.RenderFullSurface(stage);
        ResetAllGameState(stage);
        InitNewGameData();
    }
    _read(g_gameCurrentHandle, g_printBuffer, 100);
    if (stage == 3) {
        _close(g_gameCurrentHandle);
        return;
    }
    TransferCourseState(1);
    _read(g_gameCurrentHandle, hdr, 8);
    _tell(g_gameCurrentHandle);
    _close(g_gameCurrentHandle);
    if (stage == 0) {
        g_toolbarView = stage;
        g_selectedBuildingKind = -1;
        g_paintTerrain = -1;
        RebuildTerrainElevation();
    }
}

void LoadSavedGame(const char *name, int refreshUi, int headerOnly, int fromThemes)
{
    char saveName[120];
    int savedTheme;
    int savedFlags;

    savedTheme = g_currentThemeId;
    strcpy(saveName, name);
    if (fromThemes) {
        savedFlags = g_runModeFlags;
        strcpy(g_printBuffer, "Themes\\Championship\\");
    } else {
        savedFlags = g_runModeFlags;
        strcpy(g_printBuffer, "saved games\\");
    }
    strcat(g_printBuffer, saveName);
    g_gameCurrentHandle = _open(g_printBuffer, 0x8000);
    g_printBuffer[0] = '\0';
    _read(g_gameCurrentHandle, g_printBuffer, 100);
    if (fromThemes)
        _read(g_gameCurrentHandle, g_printBuffer, 100);
    if (headerOnly) {
        _close(g_gameCurrentHandle);
        return;
    }
    TransferGameState(1);
    _close(g_gameCurrentHandle);
    g_runModeFlags =
        g_runModeFlags & ~(GAME_PAUSED | GAME_FAST_CLOCK) | savedFlags & GAME_PAUSED | GAME_SAVE_CUSTOM_TYPES;
    g_toolbarView = TOOLBAR_BUILD_COURSE;
    g_selectedBuildingKind = -1;
    g_paintTerrain = -1;
    RebuildTerrainElevation();
    UpdateBuildingAccessibility();
    RecalculateAllWaterShoreDepths();
    if (refreshUi) {
        if (strcmp(saveName, "While_Browsing.sve")) {
            strcpy(g_printBuffer, "Loading Game");
            g_messageSpeaker = -1;
            RenderSpeechBubbleWithPortrait(200, 200, (int)0x80001284u, -2);
            g_simScreen.RenderFullSurface(0);
        }
        LoadCourseType(g_currentThemeId);
        if (g_currentThemeId != savedTheme)
            LoadThemeAssets();
        ResetAndRebuildTerrain();
    }
}

void TransferCourseState(int reading)
{
    g_sveFileLoadFlag = reading;
    TransferSaveBlock(&g_courseId, 4);
    TransferSaveBlock(&g_nextOpenHoleIndex, 4);
    TransferSaveBlock(&g_simTotalLengthYards, 4);
    TransferSaveBlock(&g_gameCourseTotalPar, 4);
    TransferSaveBlock(&g_currentThemeId, 1);
    TransferSaveBlock(&g_courseTotalFunRate, 4);
    TransferSaveBlock(&g_clubSkillsScore, 4);
    TransferSaveBlock(&g_lengthAbilitySum, 4);
    TransferSaveBlock(&g_gameCourseAccurPointsTotal, 4);
    TransferSaveBlock(&g_totalImagination, 4);
    TransferSaveBlock(&g_holeAwardCount, 4);
    TransferSaveBlock(&g_homesSold, 4);
    TransferSaveBlock(&g_winningPtsTotal, 4);
    TransferSaveBlock(&g_storiesNum, 4);
    TransferSaveBlock(&g_gameCashHundreds, 4);
    TransferSaveBlock(g_bestTenScoresTable, 0x28);
    TransferSaveBlock(&g_myWorldCourseIdx, 4);

    TransferSaveBlock(g_allHolesArray, 0x28a0);
    TransferSaveBlock(g_gameTileTerrainKind, 0x9c4);
    TransferSaveBlock(g_tileVariationIdx, 0x9c4);
    TransferSaveBlock(g_tileExtra, 0x1388);
    TransferSaveBlock(g_simGolferWalkCostMap, 0x9c4);
    TransferSaveBlock(g_pointElevationMap, 0xa29);
    TransferSaveBlock(g_bilerpGrid, 0x169);
    TransferSaveBlock(g_buildingTable, 0x1000);
    TransferSaveBlock(&g_celebHomesBought, 4);
    TransferSaveBlock(&g_previousSGAMark, 4);
    TransferSaveBlock(&g_clock, 4);
    TransferSaveBlock(&g_unlockedBldgTypeLimit, 4);
    TransferSaveBlock(&g_landPurchaseNum, 4);
    TransferSaveBlock(&g_level, 4);
    TransferSaveBlock(&g_proMatchStakeLvl, 4);
    TransferSaveBlock(&g_curNextSpawnPos, 4);
    TransferSaveBlock(g_talentKindCount, 0x80);
    TransferSaveBlock(&g_knownEventsLowMask, 4);
    TransferSaveBlock(&g_freeLandmarks, 4);
    TransferSaveBlock(&g_simPostedHoleCategories, 4);
    TransferSaveBlock(&g_simLatestGolferNum, 4);
    TransferSaveBlock(&g_shownLandmarkBitmask, 4);
    TransferSaveBlock(&g_ceoLadderRung, 4);
    TransferSaveBlock(&g_curLandmarkTotal, 4);

    TransferSaveBlock(g_gameFairwayTargetPoints, 0x1c8);
    TransferSaveBlock(g_homeVIPsTbl, 0x1770);
    TransferSaveBlock(g_miscGameFlags, 0x12);
    TransferSaveBlock(g_namedSceneryArray, 0x320);
    TransferSaveBlock(g_backgroundActors, 0x40);
    TransferSaveBlock(g_gameAnimals, 0xa00);
    TransferSaveBlock(g_golferTypeArray, 0xa640);
    TransferSaveBlock(g_gameForSaleLandGrid, 0x9c4);
    TransferSaveBlock(&g_runModeFlags, 4);
    TransferSaveBlock(g_activeThemeDir, 0x64);
    TransferSaveBlock(&g_knownEventsHighMask, 4);
    TransferSaveBlock(g_txtHeapBuffer, 0x1002);
    TransferSaveBlock(g_strOffsetTbl, 0x100);
    TransferSaveBlock(g_stringPoolLens, 0x100);
    TransferSaveBlock(g_staffSlots, 0x1300);
    TransferSaveBlock(g_mapCourseList, 0x30e);
}

int g_linePenX;
int g_linePenY;

void TerrainMoveTo(int x, int y)
{
    g_linePenX = x;
    g_linePenY = y;
}

void TerrainLineTo(int x, int y, unsigned color, int width, int alpha)
{
    DrawTerrainLine(x, y, g_linePenX, g_linePenY, color, width, alpha);
    g_linePenX = x;
    g_linePenY = y;
}

int IsPositionInvalid(int row, int col)
{
    if (row < 0 || row >= 50 || col < 0 || col >= 50 || g_gameTileTerrainKind[row * 50 + col] == TERRAIN_OUT_OF_BOUNDS)
        return 1;
    return 0;
}

int GetTileAtPosition(int worldRow, int worldCol)
{
    int row = worldRow >> 10;
    int col = worldCol >> 10;
    if (IsPositionInvalid(row, col))
        return TERRAIN_OUT_OF_BOUNDS;
    return g_gameTileTerrainKind[row * 50 + col];
}

int GetCornerElevation(int row, int col, int corner, int useCache)
{
    int hi, lo;
    int squareFlags, corner1, corner3, corner5, corner7, h;
    if (row < 0 || row >= 50 || col < 0 || col >= 50 || (corner & 1) == 0)
        return 3;
    if (useCache) {
        signed char v = g_cornerElevation[row][col][corner];
        if (v != 0)
            return v;
    }
    squareFlags = g_squareTypeTable[g_gameTileTerrainKind[row * 50 + col]].flags;
    if (squareFlags & ELEV_FROM_MIN) {
        if (squareFlags & 1)
            return g_flatSquareAltMap[row * 50 + col];
        GetTileElevationMinMax(row, col, &hi, &lo);
        return lo;
    }
    if (squareFlags & ELEV_FROM_MAX) {
        if (squareFlags & 1)
            return g_flatSquareAltMap[row * 50 + col];
        GetTileElevationMinMax(row, col, &hi, &lo);
        return hi;
    }
    if (squareFlags & ELEV_FLAT_ZERO)
        return 3;
    corner1 = GetTileElevation(row + 1, col - 1, 0);
    corner3 = GetTileElevation(row + 1, col, 0);
    corner5 = GetTileElevation(row, col, 0);
    corner7 = GetTileElevation(row, col - 1, 0);
    corner &= 7;
    lo = 99;
    if (corner != 5 && corner1 < lo)
        lo = corner1;
    if (corner != 7 && corner3 < lo)
        lo = corner3;
    if (corner != 1 && corner5 < lo)
        lo = corner5;
    if (corner != 3 && corner7 < lo)
        lo = corner7;
    switch (corner & 7) {
    case 1: h = corner1; break;
    case 3: h = corner3; break;
    case 5: h = corner5; break;
    case 7: h = corner7; break;
    }
    return h;
}

int GetTileElevation(int row, int col, int flags)
{
    signed char tt;
    int e;
    if (g_runModeFlags & 1)
        return 3;
    if (row < 0 || row >= 50)
        return 3;
    if (col < 0 || col >= 50)
        return 3;
    if (g_clock != 0) {
        if (g_gameTileTerrainKind[row * 50 + col] == TERRAIN_OUT_OF_BOUNDS && IsPositionInvalid(row - 1, col)
            && IsPositionInvalid(row, col + 1))
            return 3;
        if (g_clock != 0)
            return g_pointElevationMap[row][col];
    }
    tt = g_gameTileTerrainKind[row * 50 + col];
    if (tt == TERRAIN_WATER || g_gameTileTerrainKind[(row + 1) * 50 + col] == TERRAIN_WATER || tt == TERRAIN_WETLANDS
        || tt == TERRAIN_MARSH)
        return 3;
    if (g_mapCourseList[g_myWorldCourseIdx].hilly == RELIEF_HILLY)
        e = CalculateTerrainHeight(row << 7, col << 7);
    else
        e = CalculateTerrainHeight(row << 6, col << 6);
    if (g_mapCourseList[g_myWorldCourseIdx].kind == 1 && col < 16)
        e += (16 - col) * g_terrainHeightDivisor / -6;
    return Clamp(e / g_terrainHeightDivisor + 1, (g_level != 0) + 3, 15);
}

int GetTerrainRowSlope(int x, int y)
{
    int row, col;
    signed char tt;
    int corner5, corner7, corner1, corner3;
    if (g_runModeFlags & 1)
        return 0;
    row = x >> 10;
    col = y >> 10;
    tt = g_gameTileTerrainKind[row * 50 + col];
    if (tt == TERRAIN_SAND_TRAP || tt == 9)
        return 0;
    if (g_squareTypeTable[tt].flags & (ELEV_FROM_MIN | ELEV_FROM_MAX | ELEV_FLAT_ZERO))
        return 0;
    corner5 = GetCornerElevation(row, col, 5, 1);
    corner7 = GetCornerElevation(row, col, CORNER_NW, 1);
    corner1 = GetCornerElevation(row, col, 1, 1);
    corner3 = GetCornerElevation(row, col, CORNER_SE, 1);
    if (y > (col << 10) + 0x200)
        return corner3 - corner5;
    return corner1 - corner7;
}

int GetTerrainColSlope(int x, int y)
{
    int row, col;
    signed char tt;
    int corner5, corner7, corner1, corner3;
    if (g_runModeFlags & 1)
        return 0;
    row = x >> 10;
    col = y >> 10;
    tt = g_gameTileTerrainKind[row * 50 + col];
    if (tt == TERRAIN_SAND_TRAP || tt == 9)
        return 0;
    if (g_squareTypeTable[tt].flags & (ELEV_FROM_MIN | ELEV_FROM_MAX | ELEV_FLAT_ZERO))
        return 0;
    corner5 = GetCornerElevation(row, col, 5, 1);
    corner7 = GetCornerElevation(row, col, CORNER_NW, 1);
    corner1 = GetCornerElevation(row, col, 1, 1);
    corner3 = GetCornerElevation(row, col, CORNER_SE, 1);
    if (x > (row << 10) + 0x200)
        return corner3 - corner1;
    return corner5 - corner7;
}

int CalculateDirectionalOffset(int x, int y, int dir)
{
    int rowSlope = GetTerrainRowSlope(x, y);
    int colSlope = GetTerrainColSlope(x, y);
    if (dir & 1) {
        rowSlope = Clamp(rowSlope, -1, 1);
        colSlope = Clamp(colSlope, -1, 1);
    }
    return g_dirColStep[dir] * colSlope + g_dirRowStep[dir] * rowSlope;
}

int CalculateDistanceInYards(int x, int y, int tileX, int tileY)
{
    int dx = x - (tileX << 10) - 0x200;
    int dy = y - (tileY << 10) - 0x200;
    return (Math_Distance2D(dx, dy) * 25) / 1024;
}

void PlayPositionedSound(int sound, int x, int y, int delay)
{
    int sx, sy;

    if (!WorldToScreen(x, y, &sx, &sy, 0) && delay != -1)
        return;
    if (delay == -1) {
        sx = Clamp(sx, 0, 799);
        sy = Clamp(sy, 0, 599);
        delay = 0;
    } else {
        if (g_zoomView) {
            sx = sx * 2 - 400;
            sy = sy * 2 - 400;
        }
        if (sy < 0)
            return;
        if (sy > 500)
            return;
        if (sx < 0)
            return;
        if (sx >= 800)
            return;
    }
    if (g_useEffectPitches == 0) {
        ConfigureAndPlaySound(sound, abs(sx - 400) / 16 + (g_cameraZoom < DEFAULT_ZOOM ? 10 : 50), sx * 127 / 800 - 64,
            300 - g_rng.NextInt(600), delay);
    } else {
        ConfigureAndPlaySound(sound, sy / 12 + (g_cameraZoom < DEFAULT_ZOOM ? 10 : 50), sx * 127 / 800 - 64,
            (g_effectPitches[g_effectPitchNext++ % 36] - 5) * 100, delay);
    }
    g_useEffectPitches = 0;
}

void NotifyEvent(int kind, int subject)
{
    g_monthEventKindHistory[(g_clock / 1024) % 500] = (short)(kind | subject);
}

void AddFloatingMessage(int x, int y, unsigned int color)
{
    int i = g_oldestFloatingTxtSlot;

    g_risingMsgLocXs[i] = x;
    g_risingMsgLocYs[i] = y;
    g_floatingMessageTextColors[i] = color;
    g_gameFloaterFrames[i] = 48;
    strcpy(g_gameFloatStrBufs[i], g_printBuffer);
    g_oldestFloatingTxtSlot = (i + 1) % 8;
}

void RenderFloatingMessages(void)
{
    int i;
    int sy, sx;

    for (i = 0; i < 8; i++) {
        if (g_gameFloaterFrames[i] == 0)
            continue;
        g_gameFloaterFrames[i]--;
        if (WorldToScreen(g_risingMsgLocXs[i], g_risingMsgLocYs[i], &sx, &sy, 0) == 0)
            continue;
        strcpy(g_printBuffer, g_gameFloatStrBufs[i]);
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawTextCenteredWithShadow(g_printBuffer, sx, sy - 4, g_floatingMessageTextColors[i]);
    }
}

void ClearFloatingMessageAtPosition(int x, int y)
{
    int i;

    for (i = 0; i < 8; i++)
        if (g_risingMsgLocXs[i] == x && g_risingMsgLocYs[i] == y)
            g_gameFloaterFrames[i] = 0;
}

void AddMoneyPopup(int amount, int x, int y, int hole)
{
    if (amount == 0)
        return;
    if (g_runModeFlags & GAME_SANDBOX)
        return;

    if (hole != -1)
        g_allHolesArray[hole].netCost -= amount;

    g_moneyPopCoordXs[g_popUpRingCursor] = x;
    g_moneyPopCoordYs[g_popUpRingCursor] = y;
    g_gameCashPopupDelta[g_popUpRingCursor] = amount;
    g_simMoneyPopupTime[g_popUpRingCursor] = 24;
    g_popUpRingCursor = (g_popUpRingCursor + 1) % 8;
}

void RenderMoneyPopups(void)
{
    int i;
    int sy, sx;

    for (i = 0; i < 8; i++) {
        if (g_simMoneyPopupTime[i] == 0)
            continue;
        if (g_runModeFlags & GAME_SANDBOX)
            continue;
        g_simMoneyPopupTime[i]--;
        if (WorldToScreen(g_moneyPopCoordXs[i], g_moneyPopCoordYs[i], &sx, &sy, 0) == 0)
            continue;
        strcpy(g_printBuffer, g_gameCashPopupDelta[i] > 0 ? "+" : g_empty);
        FormatNumberWithCommas(g_gameCashPopupDelta[i] * 100);
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawTextCenteredWithShadow(g_printBuffer, sx, sy - 4, g_gameCashPopupDelta[i] > 0 ? 0x800023e8 : 0x80007d08);
    }
}

void TilePatternToArea(int x, int y, int w, int h, int pattern)
{
    int i, j;

    for (i = x; i < x + w; i += 16) {
        for (j = y; j < y + h; j += 16) {
            if (pattern == 0) {
                g_popupEdgeBmps[9].BlitScaled(g_pMenu, i, j, g_shadeTable, 0);
            } else {
                int col = 1;
                if (i == x)
                    col = 0;
                if (i + 16 >= x + w)
                    col = 2;
                int row = 1;
                if (j == y)
                    row = 0;
                if (j + 16 >= y + h)
                    row = 2;
                g_s_transPopUpSprites[row * 3 + col].BlitScaled(g_pMenu, i, j, g_shadeTable, 0);
            }
        }
    }
}

int ShowStoryMessage(unsigned int color, int priority, int speaker)
{
    if (((g_reportScreenId || g_displayFocus) && priority <= 0) || g_toolbarView == TOOLBAR_SHOT)
        return 0;

    strcpy(g_dialogString, g_printBuffer);
    g_displayFocus = 0;
    g_reportScreenId = 1;
    g_storyMessageFontColor = color;
    g_messageCountdown = strlen(g_printBuffer) / ((g_level != 0) + 3) + 16;
    g_messageSpeaker = speaker;
    if (priority < 0)
        g_msgDelay = -priority;
    g_dialogBubbleRandomNumberA = g_rng.NextInt(600);
    g_dialogBubbleRandomNumberB = g_rng.NextInt(200) + 200;
    return 1;
}

static inline void SnapToPieceGrid(int &pos, int &size)
{
    if (size & 15) {
        int m = size & 15;
        size += 15 - m;
        pos -= (15 - m) / 2;
    }
}

static inline void BeginFrame(int &x, int &y, int &w, int &h)
{
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    SnapToPieceGrid(x, w);
    SnapToPieceGrid(y, h);
}

void DrawDialogFrameWithBorder(int x, int y, int w, int h, int bg)
{
    int i;
    BeginFrame(x, y, w, h);
    g_simScreen.fill(x + 4, y + 4, w - 8, h - 8, (int)0x80004e79u);
    g_infoBtnImages[30 + 0].draw_antialiased(&g_infoButtonBitmapMask[30 + 0], g_pMenu, x, y, 0);
    for (i = 16; i < w - 16; i += 16) {
        g_infoBtnImages[30 + 1].draw_antialiased(&g_infoButtonBitmapMask[30 + 1], g_pMenu, x + i, y, 0);
        g_infoBtnImages[30 + 7].draw_antialiased(&g_infoButtonBitmapMask[30 + 7], g_pMenu, x + i, y + h - 16, 0);
    }
    for (i = 16; i < h - 16; i += 16) {
        g_infoBtnImages[30 + 3].draw_antialiased(&g_infoButtonBitmapMask[30 + 3], g_pMenu, x, y + i, 0);
        g_infoBtnImages[30 + 5].draw_antialiased(&g_infoButtonBitmapMask[30 + 5], g_pMenu, x + w - 16, y + i, 0);
    }
    g_infoBtnImages[30 + 2].draw_antialiased(&g_infoButtonBitmapMask[30 + 2], g_pMenu, x + w - 16, y, 0);
    g_infoBtnImages[30 + 8].draw_antialiased(&g_infoButtonBitmapMask[30 + 8], g_pMenu, x + w - 16, y + h - 16, 0);
    g_infoBtnImages[30 + 6].draw_antialiased(&g_infoButtonBitmapMask[30 + 6], g_pMenu, x, y + h - 16, 0);
}

void DrawDialogShadow(int x, int y, int w, int h, int bg)
{
    int i;
    BeginFrame(x, y, w, h);
    if (bg)
        TilePatternToArea(x, y - 1, w, h, 0);
    for (i = 12; i < w - 16; i += 16)
        g_popupEdgeBmps[13].draw_antialiased(&g_transPopupCellsAlpha[13], g_pMenu, x + i, y + h - 16, 0);
    g_popupEdgeBmps[14].draw_antialiased(&g_transPopupCellsAlpha[14], g_pMenu, x + w - 12, y + h - 16, 0);
    g_popupEdgeBmps[12].draw_antialiased(&g_transPopupCellsAlpha[12], g_pMenu, x - 4, y + h - 16, 0);
}

void DrawPanelFrame(int x, int y, int w, int h, int bg)
{
    int i;
    BeginFrame(x, y, w, h);
    if (bg)
        TilePatternToArea(x, y, w, h, 1);
    g_popupEdgeBmps[0].draw_antialiased(&g_transPopupCellsAlpha[0], g_pMenu, x, y, 0);
    for (i = 16; i < w - 16; i += 16) {
        g_popupEdgeBmps[1].draw_antialiased(&g_transPopupCellsAlpha[1], g_pMenu, x + i, y, 0);
        g_popupEdgeBmps[7].draw_antialiased(&g_transPopupCellsAlpha[7], g_pMenu, x + i, y + h - 16, 0);
    }
    for (i = 16; i < h - 16; i += 16) {
        g_popupEdgeBmps[3].draw_antialiased(&g_transPopupCellsAlpha[3], g_pMenu, x, y + i, 0);
        g_popupEdgeBmps[5].draw_antialiased(&g_transPopupCellsAlpha[5], g_pMenu, x + w - 16, y + i, 0);
    }
    g_popupEdgeBmps[2].draw_antialiased(&g_transPopupCellsAlpha[2], g_pMenu, x + w - 16, y, 0);
    g_popupEdgeBmps[8].draw_antialiased(&g_transPopupCellsAlpha[8], g_pMenu, x + w - 16, y + h - 16, 0);
    g_popupEdgeBmps[6].draw_antialiased(&g_transPopupCellsAlpha[6], g_pMenu, x, y + h - 16, 0);
}

void RenderUIFrameWithCorners(int x, int y, int w, int h, int bg)
{
    int i;
    BeginFrame(x, y, w, h);
    if (bg)
        g_simScreen.fill(x + 2, y + 2, w - 4, h - 4, (int)0x80007fdc);
    g_themeFrame[g_currentThemeId][1].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][1], g_pMenu, x, y, 0);
    for (i = 16; i < w - 16; i += 16) {
        g_themeFrame[g_currentThemeId][4].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][4], g_pMenu, x + i, y,
            0);
        g_themeFrame[g_currentThemeId][0].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][0], g_pMenu, x + i,
            y + h - 16, 0);
    }
    for (i = 16; i < h - 16; i += 16) {
        g_themeFrame[g_currentThemeId][2].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][2], g_pMenu, x, y + i,
            0);
        g_themeFrame[g_currentThemeId][6].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][6], g_pMenu,
            x + w - 16, y + i, 0);
    }
    g_themeFrame[g_currentThemeId][5].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][5], g_pMenu, x + w - 16, y,
        0);
    g_themeFrame[g_currentThemeId][7].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][7], g_pMenu, x + w - 16,
        y + h - 16, 0);
    g_themeFrame[g_currentThemeId][3].draw_antialiased(&g_themeAlphaFrames[g_currentThemeId][3], g_pMenu, x, y + h - 16,
        0);
}

void RenderSpeechBubbleWithPortrait(int x, int y, int color, int mode)
{
    int hRaw;
    int hUse;

    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
    hRaw = WrapText(g_messageSpeaker != -1 ? 37 : 44) * 15 + 32;
    hUse = hRaw;
    if (g_messageSpeaker != -1) {
        if (hUse < 0x60)
            hUse = 0x60;
        y += 0x2c;
        mode = -2;
        x += 0x1c;
    }
    g_speechWindowBottom = hUse;

    if (mode >= 0) {
        DrawDialogShadow(x - 8, y - 8, 0x1a0, hUse * mode / 4, 1);
        if (mode == 0)
            ConfigureAndPlaySound(0x10b, SOUND_FULL_VOLUME, 0, 0, 0);
        return;
    }

    if (mode != -3) {
        if (mode != -2)
            DrawDialogShadow(x - 8, y - 8, 0x1a0, hUse + 0x10, 1);
        else
            DrawPanelFrame(x - 8, y - 8, 0x1a0, hUse + 0x10, 1);
    } else {
        RenderUIFrameWithCorners(x - 8, y - 8, 0x1a0, hUse + 0x10, -1);
    }

    if (g_messageSpeaker == -1)
        y += 0x10;
    if (g_messageSpeaker == -1) {
        g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
        DrawTextWordWrapped(0x63, x + 0xc, y, mode != -3 ? 0x80007fffu : 0x80000018u);
        return;
    }

    if (g_messageSpeaker >= 0) {
        g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].draw_antialiased(&g_transPopupCellsAlpha[PANEL_PORTRAIT_FRAME], g_pMenu,
            x - 0x32, y - 0x28, 0);
        RenderGolferPortrait(g_messageSpeaker, x - 0x32, y - 0x28, -1);
    }
    if (g_messageSpeaker <= -6 && g_messageSpeaker > -0xf)
        g_bldgSprites[-6 - g_messageSpeaker][1].Draw(g_pMenu, x + 8, y - 0x14, 1, 1, 1, 0);
    if (g_messageSpeaker == -5)
        g_popupEdgeBmps[PANEL_PORTRAIT_FRAME].draw_antialiased(&g_transPopupCellsAlpha[PANEL_PORTRAIT_FRAME], g_pMenu,
            x - 0x28, y - 0x28, 0);
    if (g_messageSpeaker == -4) {
        int region = g_tourSites[g_mapCourseList[g_myWorldCourseIdx].site].id;
        g_siteMapSprs[region].draw_antialiased(&g_regionAlphaFrames[region], g_pMenu, x + 8, y, 0);
    }
    y += 0x10;
    if (g_messageSpeaker == -3)
        g_messageIconPics[1].draw_antialiased(&g_speakerIconsAlpha[1], g_pMenu, x, y, 0);
    if (g_messageSpeaker == -2)
        g_messageIconPics[2].draw_antialiased(&g_speakerIconsAlpha[2], g_pMenu, x, y, 0);
    if (g_messageSpeaker == -0x14)
        g_messageIconPics[0].draw_antialiased(&g_speakerIconsAlpha[0], g_pMenu, x, y, 0);
    if (g_messageSpeaker == -0x15)
        g_messageIconPics[4].draw_antialiased(&g_speakerIconsAlpha[4], g_pMenu, x, y, 0);
    if (g_messageSpeaker == -0x16)
        g_messageIconPics[3].draw_antialiased(&g_speakerIconsAlpha[3], g_pMenu, x, y, 0);

    if (hRaw < 0x60)
        y += (hUse - hRaw - 0x10) / 2;

    g_pMenu->SetFontsForStyles(&g_smallManualFont, 0, 0, 0);
    DrawTextWordWrapped(0x63, x + 0x64, y, mode != -3 ? 0x80007fffu : 0x80000018u);
}



void RenderDialogueBubble()
{
    if (g_msgDelay != 0) {
        g_msgDelay--;
        return;
    }
    strcpy(g_printBuffer, g_dialogString);
    int frame = -1;
    if (g_reportScreenId != 1) {
        if (g_bubbleStep != 0) {
            frame = g_bubbleStep--;
            if (frame == 0)
                return;
        }
    } else if (g_bubbleStep < BUBBLE_OPEN) {
        frame = ++g_bubbleStep;
    }
    RenderSpeechBubbleWithPortrait(0xf0, -8, g_storyMessageFontColor, frame);
    if ((g_runModeFlags & GAME_TUTORIAL) == 0 && g_clock % 3 != 0) {
        if (--g_messageCountdown <= 0)
            g_reportScreenId = 0;
    }
}

unsigned int GetColorForSkillSet(int skills)
{
    switch (skills) {
    case 6: return 0x80007ff0;
    case 5: return 0x8000421f;
    case 3: return 0x800043f0;
    case 7: return 0x80007fff;
    }
    return 0;
}

void FormatDateString(int clock)
{
    int year = clock / 8192;
    int month = (clock / 1024) % 8;
    g_careerPlayYear = year;
    strcat(g_printBuffer, kMonth[month]);
    strcat(g_printBuffer, " ");
    strcat(g_printBuffer, itoa(year + 2001, g_numberBuf, 10));
}

void SelectScreenItem(int index, int useExplicitId)
{
    g_runModeFlags &= ~GAME_ELEVATION_TOOL;

    if (g_toolbarView == TOOLBAR_SHOT) {
        switch (index) {
        case 3: g_shotSelectedKind = 0; break;
        case 4: g_shotSelectedKind = -1; break;
        case 5: g_shotSelectedKind = 1; break;
        case 6: g_shotSelectedKind = SHOT_HIGH_BACKSPIN; break;
        case 7: g_shotSelectedKind = SHOT_LOW_PUNCH; break;
        }
    } else if (g_toolbarView == TOOLBAR_GOLFERS) {
        if (index < g_toolbarItemIDCnt) {
            if (g_displayFocus <= 0 && g_golferPool[g_simListItemRefs[index]].screenX == -1)
                g_displayFocus = 0;
            else
                g_displayFocus = g_simListItemRefs[index] + 1;
            g_cameraTileX = g_golferPool[g_simListItemRefs[index]].x >> 10;
            g_cameraTileY = g_golferPool[g_simListItemRefs[index]].y >> 10;
        } else {
            g_displayFocus = 0;
        }
    } else if (g_toolbarView == TOOLBAR_BUILDINGS) {
        int pick = g_simListItemRefs[index];
        g_selectedBuildingKind = pick;
        if (pick >= g_unlockedBldgTypeLimit)
            g_selectedBuildingKind = -1;
        else if (pick == SCENERY_LANDMARK && g_shownLandmarkBitmask == 0)
            g_selectedBuildingKind = -1;
        g_selectedVariant = g_pickState;
        g_pickState = -1;
        ConfigureAndPlaySound(0xfc, 0x50, 0, 0, 0);
    } else {
        int saved = g_paintTerrain;
        if (useExplicitId)
            g_paintTerrain = index;
        else
            g_paintTerrain = g_brushPicks[index];
        ConfigureAndPlaySound(0xfb, 0x50, 0, 0, 0);
        if (g_squareTypeTable[g_paintTerrain].category == CATEGORY_TREES && g_level > TREE_TRANSPLANT_LEVEL) {
            strcpy(g_printBuffer, "Trees must be transplanted.");
            ShowStoryMessage(0x80004210, 1, -1);
            ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
            g_paintTerrain = saved;
        }
    }
}

void AppendCourseName(int form)
{
    const char *suffix;

    if (AppendPooledString(0) == 0)
        strcat(g_printBuffer, g_tourSites[g_mapCourseList[g_myWorldCourseIdx].site].courseName);

    if (form == -1)
        return;

    switch (GetHoleCountTier(g_nextOpenHoleIndex - 1)) {
    default:
        suffix = " Municipal";
        if (form != 1)
            suffix = " MC";
        break;
    case 3:
        suffix = " Championship";
        if (form != 1)
            suffix = g_empty;
        break;
    case 2:
        suffix = " Country Club";
        if (form != 1)
            suffix = " CC";
        break;
    case 1:
        suffix = " Golf Club";
        if (form != 1)
            suffix = " GC";
        break;
    }
    strcat(g_printBuffer, suffix);
}

int ValidateBuildingPlacement(int row, int col, int span, int type)
{
    int total = 0;
    int edge = 0;
    for (int i = -1; i <= span; i++) {
        for (int j = -1; j <= span; j++) {
            int r = row + i;
            int c = col + j;
            if (i == -1 || j == -1 || i == span || j == span) {
                if (g_gameTileTerrainKind[r * 50 + c] != TERRAIN_WATER)
                    edge = 1;
                continue;
            }
            if (type == 12 && (g_currentThemeId == THEME_PARK || g_currentThemeId == THEME_TROPICAL)
                && g_gameTileTerrainKind[r * 50 + c] != TERRAIN_WATER)
                return -1;
            if (type != 0) {
                signed char t = g_gameTileTerrainKind[r * 50 + c];
                if (t == TERRAIN_HOME_SITE)
                    return -1;
                if (t == TERRAIN_BUILDING || (g_tileExtra[r * 50 + c] & TILE_BUILDING)) {
                    if (g_buildingTable[FindBuildingAtTile(r, c)].kind != type)
                        return -1;
                }
                if (g_gameTileTerrainKind[r * 50 + c] == 0)
                    return -1;
                if (g_tileExtra[r * 50 + c] & (TILE_PIN | TILE_PERMANENT))
                    return -1;
            }
            if (IsPositionInvalid(r, c))
                return -1;
            signed char u = g_gameTileTerrainKind[r * 50 + c];
            if (g_squareTypeTable[u].category == CATEGORY_TREES)
                total += g_squareTypeTable[u].clearCost;
            if (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_ROCKS)
                total += g_squareTypeTable[u].clearCost;
            if (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_WATER && type != 12)
                total += g_squareTypeTable[u].clearCost;
            if (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_WETLANDS)
                total += g_squareTypeTable[u].clearCost;
            if (g_gameTileTerrainKind[r * 50 + c] == TERRAIN_MARSH)
                total += g_squareTypeTable[u].clearCost;
            if (g_tileExtra[r * 50 + c] & TILE_PERMANENT)
                return -1;
        }
    }
    if (edge == 0 && type != 0 && type != 4)
        return -1;
    return total;
}

int FindNearestBuildingOfType(int type, int wx, int wy)
{
    int best = -1;
    int i;
    g_closestDistanceFound = 0xffff;
    for (i = 0; i < 256; i++) {
        if (g_buildingTable[i].kind == type) {
            if (type >= 6 && (g_buildingTable[i].flags & SLOT_REACHABLE) == 0)
                return best;
            int h = g_facilityTypes[type].side / 2;
            int d = Math_Distance2D((g_buildingTable[i].row + h) * 1024 - wx + 512,
                (g_buildingTable[i].col + h) * 1024 - wy + 512);
            if (d < g_closestDistanceFound) {
                best = i;
                g_closestDistanceFound = d;
            }
        }
    }
    return best;
}

void FindNearestBench(int x, int y, int maxDist)
{
    int centerRow = x >> 10;
    int centerCol = y >> 10;
    g_closestDistanceFound = maxDist << 10;
    g_destRestX = -1;
    for (int r = centerRow - 4; r <= centerRow + 4; r++) {
        for (int c = centerCol - 4; c <= centerCol + 4; c++) {
            if (IsPositionInvalid(r, c))
                continue;
            if ((g_tileExtra[r * 50 + c] & TILE_BENCH) == 0)
                continue;
            int d = Math_Distance2D((r << 10) - x + 0x200, (c << 10) - y + 0x200);
            if (d < g_closestDistanceFound) {
                g_closestDistanceFound = d;
                g_destRestX = r;
                g_destRestY = c;
            }
        }
    }
}

int FindBuildingAtTile(int tx, int ty)
{
    int i;
    for (i = 0; i < 256; i++) {
        if (g_buildingTable[i].kind != -1) {
            int size = g_facilityTypes[g_buildingTable[i].kind].side;
            if (g_buildingTable[i].kind >= FIRST_BUILDING_TYPE && g_buildingTable[i].kind != SCENERY_HOME_SITE
                && g_buildingTable[i].kind != SCENERY_SNACK_BAR)
                size += g_buildingLevelPerKind[g_buildingTable[i].kind] - 1;
            if (tx >= g_buildingTable[i].row && tx < g_buildingTable[i].row + size && ty >= g_buildingTable[i].col
                && ty < g_buildingTable[i].col + size)
                return i;
        }
    }
    return -1;
}

int PlaceBuildingOnTerrain(int row, int col, int type, int flags)
{
    int n;
    int i, j, r, c;
    int a, b;
    n = g_facilityTypes[type].side;
    if (flags == -2)
        n++;
    if (type >= 6 && type != 5)
        n += g_buildingLevelPerKind[type];
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            r = row + i;
            c = col + j;
            if (j < n - 1 || i != 0)
                g_pointElevationMap[r][c] = g_pointElevationMap[row][col];
            g_gameTileTerrainKind[r * 50 + c] = type == SCENERY_HOME_SITE ? TERRAIN_HOME_SITE : TERRAIN_BUILDING;
            g_simGolferWalkCostMap[r][c] = g_squareTypeTable[TERRAIN_BUILDING].walk;
            if (type == 6) {
                if (i != 0 || j != 0)
                    g_gameTileTerrainKind[r * 50 + c] = 1;
            } else if (type == 10) {
                if (i != 0 || j != 0)
                    g_gameTileTerrainKind[r * 50 + c] = i == n - 1 ? 5 : 4;
            } else if (type == 12 && (g_currentThemeId == THEME_PARK || g_currentThemeId == THEME_TROPICAL)) {
                if (i != 0 || j != 0)
                    g_gameTileTerrainKind[r * 50 + c] = TERRAIN_WATER;
            }
            g_tileExtra[r * 50 + c] &= ~(TILE_SCENIC | TILE_BENCH | TILE_FLOWERS);
            if (flags > 0)
                g_tileExtra[r * 50 + c] |= flags;
            g_tileVariationIdx[r * 50 + c] = 0xff;
        }
    }
    if (type == 10) {
        int span = n - 2;
        a = g_rng.NextInt(span);
        b = g_rng.NextInt(span);
        g_gameTileTerrainKind[(row + 1 + b) * 50 + col + 1 + a] = 1;
        a = g_rng.NextInt(span);
        b = g_rng.NextInt(span);
        g_gameTileTerrainKind[(row + 1 + b) * 50 + col + 1 + a] = 1;
        a = g_rng.NextInt(span);
        b = g_rng.NextInt(span);
        g_gameTileTerrainKind[(row + 1 + b) * 50 + col + 1 + a] = 1;
        a = g_rng.NextInt(span);
        b = g_rng.NextInt(span);
        g_gameTileTerrainKind[(row + 1 + b) * 50 + col + 1 + a] = TERRAIN_SAND_TRAP;
    } else if (type == 15) {
        g_staffSlots[0].postRow = row + 1;
        g_staffSlots[0].postCol = col + 1;
    }
    if (flags < 0)
        return 0;
    g_tileExtra[row * 50 + col] |= type + 1;
    for (n = 0; g_buildingTable[n].kind != -1; n++)
        ;
    if (n >= 256)
        return 0;
    g_buildingTable[n].kind = type;
    g_buildingTable[n].row = row;
    g_buildingTable[n].col = col;
    g_buildingTable[n].flags = 0;
    g_buildingTable[n].score = 0;
    g_buildingTable[n].count = 0;
    g_buildingTable[n].facing = g_curBuildingTurns & 3;
    for (i = 0; i < g_facilityTypes[type].side + g_buildingLevelPerKind[type]; i++) {
        for (j = 0; j < g_facilityTypes[type].side + g_buildingLevelPerKind[type]; j++) {
            g_tileVariationIdx[(row + i) * 50 + col + j] = (unsigned char)n;
            g_tileExtra[(row + i) * 50 + col + j] =
                (g_tileExtra[(row + i) * 50 + col + j] & ~TILE_SCENIC) | TILE_BUILDING;
        }
    }
    RebuildTerrainElevation();
    return n;
}

void DemolishBuilding(int slot)
{
    int type, row, col;
    int amount;
    int size;
    int v;
    int i, j, r, c;
    type = g_buildingTable[slot].kind;
    col = g_buildingTable[slot].col;
    row = g_buildingTable[slot].row;
    if (type == SCENERY_HOME_SITE) {
        amount = CalculateLotScore(row, col) / 2 + g_buildingTable[slot].count / 50;
        g_gameCashHundreds -= amount;
        AddMoneyPopup(-amount, row * 1024 + 512, col * 1024 + 512, -1);
        g_annualStatsRecords[g_careerPlayYear % 100].v[1] -= amount;
        if (g_buildingTable[slot].count != 0)
            RemoveHomeVIPAtPosition(g_buildingTable[slot].row, g_buildingTable[slot].col);
    }
    g_buildingTable[slot].kind = -1;
    if (type == SCENERY_CLUBHOUSE)
        g_runModeFlags |= GAME_NO_CLUBHOUSE;
    g_tileExtra[row * 50 + col] &= ~(TILE_FACILITY_MASK | TILE_BENCH);
    size = g_facilityTypes[type].side;
    if (type >= FIRST_BUILDING_TYPE && type != SCENERY_SNACK_BAR && type != SCENERY_HOME_SITE)
        size += g_buildingLevelPerKind[type] - 1;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            r = row + i;
            c = col + j;
            if (type == SCENERY_MARINA && (g_currentThemeId == THEME_PARK || g_currentThemeId == THEME_TROPICAL))
                v = 0x11;
            else
                v = (g_myWorldCourseIdx >= 12) + 4;
            g_tileExtra[r * 50 + c] &= ~TILE_WALKABLE;
            g_gameTileTerrainKind[r * 50 + c] = (signed char)v;
            g_tileVariationIdx[r * 50 + c] = 0;
        }
    }
    RebuildTerrainElevation();
}

int GetLotScoreTier(int score)
{
    int tier = score / 200;

    if (tier == 0)
        return 0;
    if (tier <= 2)
        return 1;
    if (tier <= 5)
        return 2;
    return tier > 9 ? 4 : 3;
}

void BuildCourseUpgradeMessage(int tier)
{
    strcat(g_printBuffer, " Your course is now upgraded from a ");
    strcat(g_printBuffer, g_courseTierNames[tier]);
    strcat(g_printBuffer, " to a ");
    strcat(g_printBuffer, g_courseTierNames[tier + 1]);
    strcat(g_printBuffer, " course. This will DOUBLE your tournament prize monies if your course passes the more "
                          "stringent SGA evaluation. ");
    if (tier == 0)
        strcat(g_printBuffer, "In addition you may now hire more experienced employees, for a slightly higher salary "
                              "of course.");
    else if (tier == 1)
        strcat(g_printBuffer, "In addition you may build upgraded buildings which are more effective.");
}

#define H g_allHolesArray[g_nextOpenHoleIndex]

void OpenHoleForPlay()
{
    char kind[16];
    int dist;
    int n;
    int speaker;
    int idx;

    if (g_allHolesArray[g_nextOpenHoleIndex].teeX == 0 || g_allHolesArray[g_nextOpenHoleIndex].aimX == 0
        || ((g_runModeFlags & GAME_SECOND_TEES) && g_allHolesArray[g_nextOpenHoleIndex].secondTeeX == 0)) {
        ConfigureAndPlaySound(SOUND_REFUSED, SOUND_FULL_VOLUME, 0, 0, 0);
        return;
    }

    memset(g_bestTenScoresTable, 0, sizeof(g_bestTenScoresTable));
    NotifyEvent(0x20, g_nextOpenHoleIndex);
    strcpy(kind, "scenic ");
    if (g_clock)
        ConfigureAndPlaySound(0x2a, SOUND_FULL_VOLUME, 0, 0, 0);

    if (H.yards == 0) {
        int dx = abs(H.teeX - H.aimX);
        int dy = abs(H.teeY - H.aimY);
        H.yards = (short)sqrt((double)((dy * dy + dx * dx) * 625));
        if (H.yards > 250)
            H.yards = (short)(H.yards + (H.yards - 250) / 4);
    }

    dist = H.yards;
    if ((H.flags & 0x60) != 0 && dist > 250)
        dist += 25;
    if (dist > 300)
        dist -= g_buildingLevelPerKind[SCENERY_DRIVING_RANGE] * 25;
    H.par = 3;
    if (dist <= 50)
        H.par = 2;
    if (dist >= 250)
        H.par = 4;
    if (dist >= 475)
        H.par = 5;
    if (dist > 625)
        H.par = 6;

    if (g_level < 2) {
        if (H.flags & 0x40)
            TriggerAccomplishment(0, (H.aimX << 10) + 512, (H.aimY << 10) + 512);
        if (H.flags & 0x20)
            TriggerAccomplishment(1, (H.aimX << 10) + 512, (H.aimY << 10) + 512);
        if (H.par == 5)
            TriggerAccomplishment(4, (H.aimX << 10) + 512, (H.aimY << 10) + 512);
    }

    switch (H.par) {
    case 3:
        if (dist < 125)
            strcpy(kind, "short ");
        if (dist > 200)
            strcpy(kind, "long ");
        break;
    case 4:
        if (dist < 350)
            strcpy(kind, "short ");
        if (dist > 425)
            strcpy(kind, "long ");
        break;
    case 5:
        if (dist < 500)
            strcpy(kind, "short ");
        if (dist > 575)
            strcpy(kind, "long ");
        break;
    }

    H.revenue = 0;
    H.shots = 0;
    H.rounds = 0;
    H.playTime = 0;
    H.quitCount = 0;
    H.moodSum = 0;
    {
        int i, j;
        for (i = 0; i < 10; i++)
            for (j = 0; j < 8; j++)
                H.dist[j][i] = 0;
        for (i = 0; i < 64; i++)
            H.cmt[i] = 0;
    }

    H.aimDir = (char)((((CalculateAngle(H.aimX - H.secondTeeX, H.aimY - H.secondTeeY) >> 28) & 15) + 1) >> 1 & 7);

    idx = H.teeY + H.teeX * 50;
    g_tileExtra[idx] |= TILE_ANIMATING;
    g_tileAnimFrame[idx] = 0;

    strcpy(g_printBuffer, "Hole #");
    strcat(g_printBuffer, itoa(g_nextOpenHoleIndex, g_numberBuf, 10));
    strcat(g_printBuffer, ", a ");
    strcat(g_printBuffer, kind);
    strcat(g_printBuffer, itoa(H.yards, g_numberBuf, 10));
    strcat(g_printBuffer, " yard ");
    if (H.flags & 0x1000)
        strcat(g_printBuffer, "uphill ");
    if (H.flags & 0x2000)
        strcat(g_printBuffer, "downhill ");
    strcat(g_printBuffer, "par ");
    strcat(g_printBuffer, itoa(H.par, g_numberBuf, 10));
    if (H.flags & 0x20)
        strcat(g_printBuffer, " with a dogleg to the left");
    if (H.flags & 0x40)
        strcat(g_printBuffer, " with a dogleg to the right");
    strcat(g_printBuffer, ", is now open for play.");

    speaker = -1;
    if (g_nextOpenHoleIndex != 6) {
        if (g_nextOpenHoleIndex != BACK_NINE_FIRST_HOLE) {
            n = g_unlockedBldgTypeLimit;
            int lim = n - 5;
            if (g_nextOpenHoleIndex > lim && g_nextOpenHoleIndex != lim && n < 15) {
                if (g_nextOpenHoleIndex & 1)
                    strcat(g_printBuffer, " New players are flocking to your golf course. ");
                else
                    strcat(g_printBuffer, " As your course grows you can add new facilities. ");
                strcat(g_printBuffer, "You may now build a ");
                char *fname = g_facilityTypes[n].name;
                strcat(g_printBuffer, fname);
                strcat(g_printBuffer, ". A ");
                strcat(g_printBuffer, fname);
                switch (n) {
                case 3:
                    strcat(g_printBuffer,
                        " a ball washer near the tee will improve the accuracy of golfers on the hole in "
                        "addition to cheering them up.");
                    break;
                case SCENERY_HOME_SITE:
                    strcat(g_printBuffer,
                        " is a quick way to raise some cash. Depending on the quality of the location, "
                        "your golfers ");
                    strcat(g_printBuffer, "may build luxurious villas or unsightly shacks.");
                    break;
                case SCENERY_SNACK_BAR:
                    strcat(g_printBuffer, " will feed golfers who get hungry and provide some extra revenue.");
                    break;
                case SCENERY_PUTTING_GREEN:
                    strcat(g_printBuffer, " allows your golfers with imagination ability to improve their putting.");
                    break;
                case SCENERY_DRIVING_RANGE:
                    strcat(g_printBuffer, " allows your long hitters to improve the length of their shots.");
                    break;
                case SCENERY_PRO_SHOP:
                    strcat(g_printBuffer, " allows your accurate golfers to improve their equipment.");
                    break;
                case SCENERY_SWIM_CLUB:
                    strcat(g_printBuffer, " helps your players to begin play with a better attitude.");
                    break;
                case SCENERY_RESORT_HOTEL:
                    strcat(g_printBuffer, " keeps your players from getting tired towards the end of their round.");
                    break;
                case SCENERY_MARINA:
                    strcat(g_printBuffer, " increases the value of all homes and lots at your course.");
                    break;
                case SCENERY_AIRSTRIP:
                    strcat(g_printBuffer, " allows you to charge higher greens fees at your course.");
                    break;
                case SCENERY_CART_GARAGE: strcat(g_printBuffer, " increases your players speed of play."); break;
                }
                speaker = -n;
                g_unlockedBldgTypeLimit = n + 1;
            }
        }
    } else {
        BuildCourseUpgradeMessage(0);
    }
    if (g_nextOpenHoleIndex == BACK_NINE_FIRST_HOLE)
        BuildCourseUpgradeMessage(1);
    if (g_nextOpenHoleIndex == MAX_HOLES)
        BuildCourseUpgradeMessage(2);

    if (g_clock)
        ShowStoryMessage(0x80004210u, 1, speaker);

    if (g_nextOpenHoleIndex == 1) {
        g_staffSlots[0].postRow =
            (signed char)((g_allHolesArray[1].teeX * 2 + g_allHolesArray[CLUBHOUSE_HOLE].teeX) / 3);
        g_staffSlots[0].postCol =
            (signed char)((g_allHolesArray[CLUBHOUSE_HOLE].teeY + g_allHolesArray[1].teeY * 2) / 3);
    } else {
        if (g_nextOpenHoleIndex == FRONT_NINE_LAST_HOLE)
            TriggerAccomplishment(6, (g_allHolesArray[9].aimX << 10) + 512, (g_allHolesArray[9].aimY << 10) + 512);
        if (g_nextOpenHoleIndex == MAX_HOLES)
            TriggerAccomplishment(0xe, (g_allHolesArray[18].aimX << 10) + 512, (g_allHolesArray[18].aimY << 10) + 512);
    }

    int k = 1;
    g_nextOpenHoleIndex = k;
    if (g_allHolesArray[1].par) {
        while (g_allHolesArray[k].par)
            k++;
        g_nextOpenHoleIndex = k;
    }
    g_paintTerrain = -1;
}



void StartProMatch(int withPro)
{
    int a, b, row, i;

    ConfigureAndPlaySound(SOUND_APPLAUSE_GOOD, SOUND_FULL_VOLUME, 0, 0, 0);

    for (i = 0; i < 12; i++) {
        if (g_golferSkillGrades[i] != 0)
            g_skillMasks[0] |= 1 << i;
    }
    if (g_skillMasks[0] == 0)
        ShowSkillPointsIntro();

    row = g_pendingProIndex;
    if (g_curNextSpawnPos & 2)
        g_curNextSpawnPos = (g_curNextSpawnPos + 2) % 152;

    for (i = 0; i < 152; i++) {
        if ((g_golferPool[i].holeIdx == 1 && g_golferPool[i].holeStrokes == 0) || g_golferPool[i].holeIdx == -1)
            ClearGolferSlotAndNamedScenery(i);
    }

    a = SpawnGolfer(0);
    if (a & 1)
        a = SpawnGolfer(0);
    if (a != -1)
        g_simLatestGolferNum = a;
    g_simLatestGolferNum = SpawnGolfer(0);
    b = g_simLatestGolferNum;

    g_golferPool[a].partner = b;
    g_golferPool[b].partner = a;
    g_golferPool[a].waitTimer = 0;
    g_golferPool[b].waitTimer = 6;

    if (withPro != 0) {
        g_golferPool[a].persona = PERSONA_ROSTER;
        g_golferPool[a].rosterIndex = row;
        g_golferPool[a].typeIdx = PRO_TYPE;

        strcpy(g_printBuffer, g_golfProList[row].name);
        strcat(g_printBuffer, ".pro");

        if (LoadGolferFromTheme(g_printBuffer, PRO_TYPE, -1) == 0) {
            memset(g_phrasesPerGolferType + PRO_TYPE * 1250, 0, 1250);
            g_golferTypeArray[PRO_TYPE].ageBits = 9;
            g_golferTypeArray[PRO_TYPE].pantsColor = g_golfProList[row].pantsColor;
            g_golferTypeArray[PRO_TYPE].shirtColor = g_golfProList[row].shirtColor;
            g_golferTypeArray[PRO_TYPE].skinTone = g_golfProList[row].skinTone;
            g_golferTypeArray[PRO_TYPE].appearance =
                ((g_golfProList[row].figure & OUTFIT_MASK) << 4) | g_golfProList[row].appearanceColor;
            if (g_golfProList[row].figure & ROSTER_FEMALE)
                g_golferTypeArray[PRO_TYPE].ageBits = (signed char)0x89;
            g_golferTypeArray[PRO_TYPE].caps = 1 << (g_rng.NextInt(5) & 0xffff);
            strcpy(g_golferTypeArray[PRO_TYPE].name, g_golfProList[row].name);
            strcpy(g_golferTypeArray[PRO_TYPE].title, "Golf Pro");
        }

        g_golferPool[a].ageFlags = g_golferTypeArray[PRO_TYPE].ageBits;
        g_golferPool[a].flags &= ~(GOLFER_PRACTISED_RANGE | GOLFER_VISITED_PRO_SHOP | GOLFER_PRACTISED_GREEN);
        g_golferPool[a].talentLevel = Clamp(g_level, 0, 2);
        g_golferPool[a].talents =
            (g_proMatchStakeLvl << TALENT_MATCH_SHIFT) + (TALENT_LENGTH | TALENT_ACCURACY | TALENT_IMAGINATION);
        g_golferPool[a].skillMask = 0xe0;
        for (i = 0; i < 12; i++) {
            g_golferPool[a].ratings[i] = g_golfProList[row].ratings[i];
            if (g_golferPool[a].ratings[i] != 0)
                g_golferPool[a].skillMask |= 1 << i;
        }
        g_pendingProIndex = -1;
    }

    g_golferPool[b].flags &= ~(GOLFER_PRACTISED_RANGE | GOLFER_VISITED_PRO_SHOP | GOLFER_PRACTISED_GREEN);
    g_golferPool[b].persona = PERSONA_PLAYER;
    g_golferPool[b].typeIdx = 0;
    g_golferPool[b].ageFlags = g_golferTypeArray[0].ageBits;
    g_golferPool[b].holeIdx = 1;
    g_golferPool[a].holeIdx = 1;
    g_golferPool[b].arrivedAt = g_clock;
    g_golferPool[a].arrivedAt = g_clock;
    g_golferPool[b].storyId = -1;
    g_golferPool[a].storyId = -1;
    g_golferPool[b].rapport = 0;
    g_golferPool[a].rapport = 0;
    g_golferPool[a].flags &= ~GOLFER_STORY_LEAD;
    g_runModeFlags &= ~(GAME_PRO_CHALLENGE | GAME_START_MATCH);
    g_golferPool[b].flags &=
        ~(GOLFER_PRACTISED_RANGE | GOLFER_VISITED_PRO_SHOP | GOLFER_PRACTISED_GREEN | GOLFER_STORY_LEAD);
    g_golferPool[b].talentLevel = STAND_IN_POWER_TIER;
    g_golferPool[b].talents = 0x47;
    g_golferPool[b].skillMask = g_skillMasks[0];
    for (i = 0; i < 12; i++) {
        if (g_golferPool[b].skillMask & (1 << i))
            g_golferPool[b].ratings[i] = g_golferSkillGrades[i];
    }
    g_golferPool[b].flags |= GOLFER_PRO_MATCH;
    g_watchedPlayerIdx = b;
    g_golferPool[b].x = g_staffSlots[0].posX;
    g_golferPool[b].y = g_staffSlots[0].posY;
    g_golferPool[b].waitTimer = 0;
    g_golferPool[b].dir = g_staffSlots[0].dir;
}

int g_unusedIntA;
int g_leftoverIntsA[2];
int g_unreadDwordsA[42];
int g_idleFieldD;
int g_unreadWord2;
int g_unusedNumA;
int g_unreadWordY;
int g_spareTailWord = 0;
