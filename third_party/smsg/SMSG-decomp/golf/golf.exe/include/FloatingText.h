#ifndef FLOATINGTEXT_H
#define FLOATINGTEXT_H

#include "Golfer.h"
#include "Plot.h"
#include "AssetSurface.h"
#include "GameFlags.h"
#include "ScreenText.h"
#include "IsoView.h"
#include "SoundEffects.h"

struct Hole {
    char par;
    signed char teeDir;
    char aimDir;
    unsigned char pad003;
    short yards;
    unsigned char pad006[0x08 - 0x06];
    int teeX;
    int teeY;
    int secondTeeX;
    int secondTeeY;
    int aimX;
    int aimY;
    int rounds;
    int shots;
    short dist[8][11];
    short cmt[64];
    short moodSum;
    short quitCount;
    union {
        struct {
            short driveTot;
            short fairways;
            short greens;
            short putts;
            short drives;
            short longDrive;
            short pad168[2];
        };
        short stats[8];
    };
    short cmtCode[64];
    int playTime;
    int netCost;
    int revenue;
    int upkeep;
    int similar;
    int flags;
    unsigned char pad204[0x208 - 0x204];
};

void PlayPositionedSound(int sound, int x, int y, int delay);
void NotifyEvent(int kind, int subject);
void AddFloatingMessage(int x, int y, unsigned int color);
void RenderFloatingMessages(void);
void ClearFloatingMessageAtPosition(int x, int y);
void AddMoneyPopup(int amount, int x, int y, int hole);
void RenderMoneyPopups(void);

extern short g_monthEventKindHistory[];
extern char g_gameFloatStrBufs[8][64];
extern int g_risingMsgLocXs[8];
extern int g_risingMsgLocYs[8];
extern unsigned int g_floatingMessageTextColors[8];
extern int g_gameFloaterFrames[8];
extern int g_oldestFloatingTxtSlot;
extern int g_gameCashPopupDelta[8];
extern int g_simMoneyPopupTime[8];
extern int g_moneyPopCoordXs[8];
extern int g_moneyPopCoordYs[8];
extern int g_popUpRingCursor;
extern Hole g_allHolesArray[20];
extern Font g_smallManualFont;
#include "EmptyString.h"

void FormatNumberWithCommas(int n);

extern int g_zoomView;
extern int g_useEffectPitches;
extern int g_effectPitchNext;
extern signed char g_effectPitches[36];

#define HOLE_TOP_100                 0x01
#define HOLE_TOP_18                  0x02
#define HOLE_DOGLEG_LEFT             0x20
#define HOLE_DOGLEG_RIGHT            0x40

#endif
