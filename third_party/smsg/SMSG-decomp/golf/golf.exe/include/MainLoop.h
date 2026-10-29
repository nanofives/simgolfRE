#ifndef MAINLOOP_H
#define MAINLOOP_H

#include "crt.h"
#include "Heap.h"
#include "ClassList.h"
#include "HeapNew.h"
#include "AssetSurface.h"
#include "BinkPlayer.h"
#include "BuildingSearch.h"
#include "BaseButton.h"
#include "ClubName.h"
#include "TileEdit.h"
#include "OpenHole.h"
#include "CourseStart.h"
#include "ClubReports.h"
#include "CourseGrid.h"
#include "CourseReportScreen.h"
#include "CourseScript.h"
#include "Critter.h"
#include "DataPath.h"
#include "DialogFrame.h"
#include "Dialog.h"
#include "Employee.h"
#include "FixedMath.h"
#include "FLCPalette.h"
#include "FlicBanks.h"
#include "FloatingText.h"
#include "GameFlags.h"
#include "Geometry.h"
#include "Golfer.h"
#include "GolferPairing.h"
#include "GolferPanel.h"
#include "GolferSpawn.h"
#include "GolferStats.h"
#include "GraphsyLoader.h"
#include "HireEmployee.h"
#include "HoleStatistics.h"
#include "TileValue.h"
#include "Spot.h"
#include "SpriteBox.h"
#include "ImageButton.h"
#include "ImageSet.h"
#include "InfoScreenAssets.h"
#include "InfoScreens.h"
#include "InputPoll.h"
#include "IntroVideo.h"
#include "IsoView.h"
#include "ListBoxCtl.h"
#include "LoadingScreen.h"
#include "MapTile.h"
#include "filemap.h"
#include "MiscUtil.h"
#include "MultiDebug.h"
#include "Network.h"
#include "NumberText.h"
#include "Plot.h"
#include "PolyFill.h"
#include "ProMatch.h"
#include "RandomSource.h"
#include "RenderQueue.h"
#include "SaveGame.h"
#include "SceneryNames.h"
#include "Scorecard.h"
#include "Screen.h"
#include "Screenshot.h"
#include "ScreenText.h"
#include "Scroll.h"
#include "ShotDifficulty.h"
#include "ShotPower.h"
#include "ShotTrail.h"
#include "SoundDll.h"
#include "SoundEffects.h"
#include "SoundEntry.h"
#include "SoundObject1.h"
#include "SpriteAssets.h"
#include "Sprite.h"
#include "StringPool.h"
#include "StringUtil.h"
#include "Surface.h"
#include "TerrainGrid.h"
#include "TerrainCircle.h"
#include "TerrainInfluence.h"
#include "TerrainSmooth.h"
#include "TerrainView.h"
#include "EditBox.h"
#include "Text.h"
#include "EditGroup.h"
#include "TextRenderer.h"
#include "TextWrap.h"
#include "ThemeLoader.h"
#include "NetFifo.h"
#include "Timer.h"
#include "ToolbarSelect.h"
#include "Top10Courses.h"
#include "TournamentLeaderboard.h"
#include "WaveInSoundDevice.h"
#include "WaveSoundDevice.h"
#include "WidgetBase.h"
#include "Wildlife.h"
#include "WildlifeUpdate.h"
#include "SysMessageHandler.h"


struct HighScore {
    char name[0x40];
    char course[0x40];
    int fun;
    int skill;
    int cash;
    char pad8c[0xa];
    short level;
    int courseId;
};
struct FlowerBedSprite {
    short bank;
    short facing;
};

extern HighScore g_topTenScoresArray[10];
extern FlowerBedSprite g_flowerBedSprites[16];
extern signed char g_keyOrder[];
extern char g_tickerMessageBuf_1[];
extern char g_tickerMessageBuf_2[];
extern int g_nextTutStepIdx;
extern TerrainTypeDef g_terrainTypeDefaults[23];
extern char *g_celebKindNames[];
extern char *g_skillNames[10];
extern int g_tooltipTileRow;
extern int g_waterSprayFxFrame;
extern Font g_hudFont;
extern FLCPalette g_palette;
extern Font g_smallKleptoFont;
extern int g_shotsForTrace;
extern int g_gameCourseRefreshReq;
extern int g_routeTargetX[10];
extern int g_routeTargetY[10];
extern int g_gameElevPanelChoice;
extern int g_scheduledTriggerBits;
extern int g_paintedCameraTileY;
extern int g_paintedCameraTileX;
extern int g_pointerPosX;
extern int g_pointerPosY;
extern int g_buildToolbarShown;
extern int g_elevToolbarShown;
extern int g_allowedExpansionRequestFlag;
extern int g_curPreviousCamScale;
extern int g_trailSavedPlotPosX[152];
extern int g_trailSavedPlotPosY[152];
extern int g_rosterBarButtonGolfer[152];
extern int g_relaxingBuildingId;
extern int g_deferEventStamp;
extern int g_fundsLabelFlag;
extern int g_curAnalysisHoleIndex;
extern int g_waterSprayFxPositionX;
extern int g_waterSprayFxPositionY;
extern int g_speechWindowBottom;
extern int g_simFrameZoomSnapshot;
extern int g_renderedBallCount;
extern signed char g_buttonSlotOfType[23];
extern int g_fastRun;
extern int g_quitToMenuFlag;
extern int g_shotSearchLocked;
extern int g_mouseClickDelay;
extern int g_simCameraZoomStored;
extern int g_visibleBallX[32];
extern int g_visibleBallY[32];
extern int g_previousCamOrientation;
extern int g_tooltipTileColumn;
extern int g_plannedEndX;
extern int g_plannedEndY;
extern int g_updateTimeMs;
extern int g_shotPlanMode;
#define SHOT_PLAN_ROUTE 2


void ProcessHireEmployeeSelection();
int LoadOrGenerateHighScores();
void PlanGolferShot(int gi, int mode, int px, int py, int shape);
void ClearGolferSlotAndNamedScenery(int gi);
void ProcessVIPGolferEvent(int golfer);
void UpdateAllGolfers();
void DisableCapsLock();
int MainBarClick(int x, int y);
void RenderMainBar(int x, int y);
int HandleCourseToolbarClick(int mx, int my);
void RenderTerrainToolbar(int mx, int my);
int HandleFacilityUpgradeClick(int mx, int my);
void RenderFacilityUpgradeToolbar(int mx, int my);
int HandleGolferPairingClick(int x, int y);
void RenderGolferPairingPanel(int mx, int my);
int HandlePeopleToolbarClick(int mx, int my);
void RenderPeopleToolbarWithTooltip(int mx, int my);
void RenderRoutingMapScreen();
void RenderGolferPairingScreen();
void ShowWelcomeMessage();
void SetupTutorialMessage();
void RenderSpriteByType(int type, int level, int x, int y, int x2, int y2, int slot);
void RenderAccomplishmentsScreen();
void ShowAccomplishmentsScreen();
void InitWorldMapCourseList();
int ShowWorldMapScreen();
int ChooseWorldMapCourse(int mode);
void GenerateCourseMap(int course);
void ShowThemeSelect();
void OnFrameIdle();
void RenderHistographScreen();
int RenderTractPurchaseScreen();
void SimulateBallPhysics(int gi, int tx, int ty, int spin);

void MainLoop();

#define CORNER_SE                    3
#define CORNER_NW                    7
#define RELIEF_HILLY                 2
#define SCREEN_WIDTH                 800
#define SCREEN_HEIGHT                600
#define LINE_ALPHA                   7
#define FIRST_BUILDING_TYPE          6
#define LAST_FIXED_LANDMARK          0x10
#define EVENT_CELEBRITY              0xc0
#define SELECTED_GOLFER_MASK         0xff
#define MSG_LINE_MASK                0x7f
#define CLUB_PUTTER                  13
#define LAST_LONG_CLUB               3
#define SHOT_HIGH_BACKSPIN           3
#define SHOT_LOW_PUNCH               4
#define TALK_GREETING                (-2)
#define TALK_HURRY_UP                (-3)
#define TALK_WEEDS                   (-4)
#define TALK_DRINK                   (-5)
#define TALK_DIVOTS                  (-6)
#define TALK_SEND_HOME               (-7)
#define SITE_IRELAND                 10
#define SITE_SPAIN                   13
#define FACILITIES_ALL               0x11
#define TICKS_PER_YEAR               0x2000
#define MOUSE_RIGHT                  2
#define REPEAT_MESSAGE_TICKS         0x80
#define TREE_TRANSPLANT_LEVEL        3
#define ACTION_LIKES_DIFFERENT_HOLE  0x1d
#define ACTION_UPSET                 8
#define AGE_PLUS_20                  1
#define AGE_PLUS_45                  4
#define TRAIT_PLAYFUL                8
#define SHOT_STEP_ADDRESSED          2
#define TALK_COMPLAINT               '#'
#define NAME_DIALOG_X                200
#define MAX_HOLES                    18
#define SCRATCH_GOLFER               0x98
#define MSG_LINE_PARTNER             0x80
#define TALK_SHOW_TICKS              7
#define BANK_SNACK_BAR               0x1ec
#define BANK_CLUBHOUSE               0x1f6
#define PAL_TEE_MARKER               0x5e
#define HEAD_PORTRAIT_SIZE           0x8c
#define SIMFOTO_BORDER               0x10
#define CLUBHOUSE_TILE_FLAGS         0x60
#define NAME_DIALOG_Y                0x20
#define NAME_MAX_LENGTH              0x20
#define BACK_NINE_FIRST_HOLE         10
#define FRONT_NINE_LAST_HOLE         9

#endif
