#ifndef FLICBANKS_H
#define FLICBANKS_H

#include "Sprite.h"

extern SpriteObject g_flicSprites[0xdd40];

extern int g_memBases[100];
extern int g_memSizes[100];

extern int g_flicStartFrame[590];
extern int g_gameAnimNumAngles[590];

void MemPool_Free(int p, int sz);
int MemPool_Alloc(int size);
void FreeSpritePoolByIndex(int bank);
void LoadFLCAnimation(int slot, const char *name, int facings, int mode, int wantShadow);

void LoadThemeAssets();

extern int g_bankSlotOffset;
extern char g_flcFileNames[590][50];
extern int g_unusedFlag;
struct FlicBanksUnrecovered;

extern int g_flicNextFree;
extern SpriteObject *g_currentFlic;
extern int g_flicNumLoads;

#include "FlicChannels.h"
int ShowListDialogWithParams(const char *title, int x, int y);

#define GOLF_BALL_PIC_X              0xc0
#define GOLF_BALL_PIC_SIZE           2
#define FEMALE_FIGURE          140

#endif
