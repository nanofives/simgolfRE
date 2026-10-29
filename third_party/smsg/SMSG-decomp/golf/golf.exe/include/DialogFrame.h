#ifndef DIALOGFRAME_H
#define DIALOGFRAME_H

#include "Sprite.h"
#include "Screen.h"
#include "Plot.h"
#include "Golfer.h"

#include "SpriteAssets.h"

void TilePatternToArea(int x, int y, int w, int h, int pattern);

int ShowStoryMessage(unsigned int color, int priority, int speaker);
#define MESSAGE_NORMAL   0
#define MESSAGE_URGENT   1

void DrawDialogFrameWithBorder(int x, int y, int w, int h, int bg);
void DrawDialogShadow(int x, int y, int w, int h, int bg);
void DrawPanelFrame(int x, int y, int w, int h, int bg);
void RenderUIFrameWithCorners(int x, int y, int w, int h, int bg);

extern int g_messageSpeaker;

extern char g_dialogString[1024];
extern unsigned int g_storyMessageFontColor;
extern int g_messageCountdown;
extern int g_msgDelay;
extern int g_dialogBubbleRandomNumberA;
extern int g_dialogBubbleRandomNumberB;

extern int g_bubbleStep;
#define BUBBLE_OPEN      3

extern unsigned short *g_shadeTable;

void RenderSpeechBubbleWithPortrait(int x, int y, int color, int mode);

#endif
