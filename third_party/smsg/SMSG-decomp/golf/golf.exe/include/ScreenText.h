#ifndef SCREENTEXT_H
#define SCREENTEXT_H

#include "Plot.h"
#include "RenderQueue.h"

int ScaleXCoordinate(int x);
void DrawSurfaceLine(AssetSurface *s, int x1, int y1, int x2, int y2, int color);
void DrawTextWithColor(const char *s, int x, int y, int color);
void DrawTextScaledAtPosition(const char *s, int x, int y, int color);
void DrawTextClampedToScreen(const char *s, int x, int y, int color);
void DrawTextCenteredAtY(const char *s, int x, int y, int color);
void DrawTextCenteredWithShadow(const char *s, int x, int y, int color);

#define COLOR_BLACK                  0x80000000
#define COLOR_WHITE                  0x80007fff
#define COLOR_LIGHT_GREY             0x80006318
#define COLOR_GREY                   0x80004210
#define COLOR_DARK_GREY              0x80002108
#define COLOR_PALE_YELLOW            0x80007ff0
#define COLOR_BRIGHT_GREEN           0x800023e8
#define COLOR_TEAL                   0x80000210
#define COLOR_DARK_RED               0x80006000
#define COLOR_DARK_GREEN             0x80001204
#define COLOR_PURPLE                 0x80004010

#endif
