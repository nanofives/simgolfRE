#ifndef PLOT_H
#define PLOT_H

#include "Sprite.h"
#include "RandomSource.h"
#include "GameFlags.h"
#include "Geometry.h"

struct BuildingDef {
    char name[32];
    signed char footprint[6];
    signed char size;
};

struct HomePlot {
    short building[16];
    signed char x[16];
    signed char y[16];
    signed char frame[16];
    signed char cell[6][6];
};

struct HomeVIP {
    int reserved;
    unsigned char pad04[0x1c];
    short kind;
    short plot;
    short pad24;
    short tileX;
    short tileY;
    short timer;
    short anim;
    short pad2e;
    int posX;
    int posY;
    signed char dir;
    signed char moveTicks;
    signed char frame;
    unsigned char pad3b;
};

struct CelebRec {
    char name[0x20];
    char kind;
    char skinTone;
    char appearanceColor, shirtColor, pantsColor;
};

struct Building {
    short kind;
    short row;
    short col;
    char facing;
    unsigned char flags;
    union {
        int count;
        int landmark;
        int resident;
    };
    int score;
};

void ClearPlot(int plot);
int PlaceBuilding(int plot, int def, int x, int y);
void RemoveHomeVIPAtPosition(int tileX, int tileY);
void SpawnHomeVIP(int plot, int celeb, int tileX, int tileY);
void RenderPlot(int plot, int tileX, int tileY);
void InitPlots(void);
void UpdateHomeVIP(int idx);
void UpdateAllHomeVIPs(void);

extern HomeVIP g_homeVIPsTbl[100];
extern char g_numBuffer[16];
extern BuildingDef g_buildingDefs[16];

extern HomePlot g_homePlots[];
extern CelebRec g_themeCelebsEntries[100];
extern Building g_buildingTable[256];
extern int g_cameraZoom;
#define DEFAULT_ZOOM 4
#define MAX_ZOOM 4
extern AssetSurface *g_pMenu;
extern int g_camDirIdx;
extern SpriteObject g_homePlotGroundPics[8];
extern SpriteObject *g_curDecoSprite;
extern int g_flicBankFrameLens[590];

SpriteObject *GetFlicBankSprite(int bank, int frame, int facing);

#define HALF_TURN                    4
#define SLOT_REACHABLE               0x40
#define QUARTER_TURN                 2
#define THREE_QUARTER_TURN           6
#define SCENERY_FLOWER_BED           2
#define SCENERY_LANDMARK             4
#define SCENERY_HOME_SITE            5
#define SCENERY_WILLOW_TREE          0x10
#define SCENERY_SCENIC_BRIDGE        0x13
#define SCENERY_PUTTING_GREEN        6
#define SCENERY_SNACK_BAR            7
#define SCENERY_PRO_SHOP             8
#define SCENERY_DRIVING_RANGE        10
#define SCENERY_SWIM_CLUB            9
#define SCENERY_CART_GARAGE          0xb
#define SCENERY_MARINA               0xc
#define SCENERY_RESORT_HOTEL         0xd
#define SCENERY_AIRSTRIP             0xe
#define BANK_FLOWERS                 400

#endif
