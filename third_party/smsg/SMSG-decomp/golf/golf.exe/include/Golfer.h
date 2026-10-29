#ifndef GOLFER_H
#define GOLFER_H

#include "GameFlags.h"
#include "LandmarkInfluence.h"

#define ACTIVITY_SWING           0
#define ACTIVITY_STRIKE          5
#define ACTIVITY_FOLLOW_THROUGH  6
#define ACTIVITY_IDLE            7
#define ACTIVITY_IDLE_LAST       10
#define ACTIVITY_WALK            11
#define ACTIVITY_PLEASED         12
#define ACTIVITY_ANNOYED         13
#define ACTIVITY_TALK            14
#define ACTIVITY_STEP_UP         16
#define ACTIVITY_SIT_DOWN        17
#define ACTIVITY_GET_UP          18
#define ACTIVITY_WASH_BALL       19

#define TALENT_LENGTH            1
#define TALENT_ACCURACY          2
#define TALENT_IMAGINATION       4
#define TALENT_MATCH_SHIFT       4

#define PERSONA_ROSTER           0x20
#define PERSONA_COMMISSIONER     0x40
#define PERSONA_CEO              0x60
#define PERSONA_HEIRESS          0x80
#define PERSONA_OWN_PRO          4
#define PERSONA_PLAYER           0x24

struct Golfer {
    int x;
    int y;
    int screenX;
    int screenY;
    unsigned flags;
    short walkLeft;
    short skillMask;
    unsigned char persona;
    unsigned char talents;
    signed char dir;
    signed char shotDifficulty;
    signed char club;
    signed char activity;
    unsigned char animTick;
    unsigned char animBank;
    signed char shotStep;
    signed char holeIdx;
    signed char holeStrokes;
    signed char holeScore[19];
    signed char attitude;
    unsigned char holeResult[19];
    unsigned char chapterAtHole[19];
    unsigned char holeEnjoyment[19];
    unsigned char slotKind[10];
    unsigned char slotStamp[10];
    unsigned char talkTimer;
    unsigned char msgLine;
    unsigned char msgStamp;
    unsigned char pad87;
    unsigned short slotParams[10];
    unsigned short msgCode;
    short waitTimer;
    short clubsUsed;
    short partner;
    short enjoyment;
    short hunger;
    short thirst;
    short fatigue;
    short shotType;
    short rapport;
    short storyId;
    short storyChapter;
    short enjoymentFloor;
    short typeIdx;
    short ageFlags;
    signed char talentLevel;
    unsigned char rosterIndex;
    unsigned char bridgesSeen;
    short seq;
    int arrivedAt;
    int shotFromX;
    int shotFromY;
    int targetX;
    int targetY;
    int ballX;
    int ballY;
    int ballZ;
    int ballBearing;
    int ballSpeed;
    int ballClimb;
    int ballCurve;
    union {
        unsigned char ratings[12];
        int ratingWords[3];
    };
    unsigned char padfc[4];
};

struct GolferType {
    char title[0x10];
    char name[0x10];
    signed char caps;
    signed char ageBits;
    signed char head;
    signed char appearance;
    signed char shirtColor;
    signed char pantsColor;
    signed char extraTone;
    signed char skinTone;
    signed char hairColor;
    unsigned char pad29[0x2c - 0x29];
    int fixedKind;
    char bio[560 - 0x30];
};

struct GolferTypeStats {
    unsigned char bestScore;
    signed char avgScore;
    signed char flags;
    unsigned char perHoleFlags[19];
    signed char perHoleStrokes[19];
    signed char finishedStoryId;
    short count;
};
#define HOLE_QUIT                    4

extern Golfer g_golferPool[156];
#define MAX_GOLFERS      152
#define PREVIEW_GOLFER   0x99
#define FIRST_CELEBRITY  0x9c
extern GolferType g_golferTypeArray[];
#define PRO_TYPE         0x4c
extern GolferTypeStats g_golferHistoryPerCharacter[84];
extern char g_printBuffer[4096];
extern signed char g_currentThemeId;
extern int g_clock;
extern int g_curPreviousConvoClock;
extern int g_storiesNum;
extern int g_lastStoryResponse;
extern int g_curNextSpawnPos;
extern int g_talentKindCount[32];

struct ThemeSlot {
    int id;
    int para;
    int sub;
    char text[0x100];
};
extern ThemeSlot g_themeSlots[8];
extern short g_nextThemeSlot;
extern char g_activeThemeDir[];
extern char g_themeList[100][50];
extern char g_gameStoryNameTable[100][50];

int IsGolferMale(int golfer);
void SetGolferAction(int golfer, int action, int actionArg);

void OnTraitMatched(int golfer);

void ResolveStoryResponse(int file, int range, int hole, int gi);
void AppendThemeText(int file, int para, int sub, int mode);

#pragma pack(push, 1)
struct RosterEntry {
    char name[0x20];
    signed char figure;
    signed char skinTone;
    signed char appearanceColor;
    signed char shirtColor;
    signed char pantsColor;
    union {
        unsigned char ratings[12];
        int ratingWords[3];
    };
    unsigned char pad31[5];
    short ratingTotal;
};
#pragma pack(pop)
extern RosterEntry g_golfProList[100];
extern char *g_commissionerName;
extern char *g_ceoName;
extern char *g_celebrityNames[];
extern int g_landPurchaseNum;

void GetPetNameSuffix(int golfer);
int GetGolferMoodIndex(int golfer);
void BuildEmployeeName(int employee);
void AppendGolferName(int golfer, int withComma);

int AdvanceStoryChapter(int golfer, int force);
int GetStoryChapterLine(int story, int hole, int gi);

#define PERSONA_KIND          0xe0
#define SKILLS_ALL                   7
#define GOLFER_PRO_MATCH             0x200
#define GOLFER_VISITED_PRO_SHOP      8
#define GOLFER_LEANS_LEFT            0x20
#define GOLFER_LEANS_RIGHT           0x40
#define GOLFER_IN_CART               0x8000
#define GOLFER_ALT_REACTION          0x20000
#define GOLFER_SLOW_WALK             0x80000
#define GOLFER_STORY_LEAD            0x100000
#define GOLFER_STORY_ADVANCED        0x200000
#define GOLFER_EXACT_AIM             0x10000000
#define MOOD_PLEASED                 0x4000
#define MOOD_ANNOYED                 0x8000
#define MOOD_MASK                    0xc000
#define STORY_RESULT_SPECIAL         99
#define STORY_NONE                   0xff
#define GOLFER_TIRED_LEVEL           0xa0
#define THEME_TROPICAL               2
#define THEME_LINKS                  3
#define MEMBERSHIP_MASK              7
#define GOLFER_TYPE_FEMALE           0x80
#define APPEARANCE_LOW_BITS          0xf
#define GOLFER_BALL_MOVING           0x40000
#define GOLFER_BALL_TRAIL            0x400000
#define STORY_LAST_CHAPTER           4
#define GOLFER_NEED_LEVEL            0x10
#define GOLFER_WALKED_OFF            0x20000000
#define GOLFER_UPGRADE_DUE           0x80000000
#define LINE_STORY                   0x32
#define THEME_PARK                   0
#define THEME_DESERT                 1
#define CLUBHOUSE_HOLE               0x13
#define SKILLS_NO_LENGTH             6
#define GOLFER_PRACTISED_RANGE       4
#define GOLFER_PRACTISED_GREEN       0x10
#define GOLFER_AFTER_PENALTY         1
#define OUTFIT_MASK                  3
#define ROSTER_FEMALE                4
#define TYPE_JUNIOR                  8
#define STAND_IN_POWER_TIER          2
#define HOLE_MARKS            3
#define TOP_TIER              5

#endif
