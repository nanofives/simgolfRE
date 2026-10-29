#ifndef RENDERQUEUE_H
#define RENDERQUEUE_H

#include "Plot.h"
#include "IsoView.h"

struct SpriteObject;

extern int g_rqSortKey[1024];
extern int g_rqFlags[1024];
extern int g_rqScale[1024];
extern SpriteObject *g_rqSprite[1024];
extern int g_rqAnchorX[1024];
extern int g_rqAnchorY[1024];
extern int g_rqBlitX[1024];
extern int g_rqBlitY[1024];
extern int g_rqSlotMark[1024];
extern int g_rqNext[1024];
extern int g_rqHead;
extern int g_rqCount;

extern int g_rqSavedSortKey[1024];
extern int g_rqSavedFlags[1024];
extern int g_rqSavedScale[1024];
extern SpriteObject *g_rqSavedSprite[1024];
extern int g_rqSavedAnchorX[1024];
extern int g_rqSavedAnchorY[1024];
extern int g_rqSavedBlitX[1024];
extern int g_rqSavedBlitY[1024];
extern int g_rqSavedSlotMark[1024];
extern int g_rqSavedNext[1024];
extern int g_rqSavedHead;
extern int g_rqSavedCount;

extern int g_zoomFromCamera;
extern char g_golferIdText[12];
extern unsigned short *g_colorLut;

void SetupGolferAppearance(int gi);

void RenderQueuedSprite(int n);

void ProcessRenderQueue();

void ResetRenderState();

void SaveRenderQueue();

void DrawSpriteImmediate(int x, int y, int sortKey, int ox, int oy, SpriteObject *spr, int sz, int id);

void AddSpriteToRenderQueue(int x, int y, int sortKey, int ox, int oy, SpriteObject *spr, int sz, int id);

#define SPRITE_FULL_SCALE            4

#endif
