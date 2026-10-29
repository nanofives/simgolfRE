#include "jgldefs.h"
#include "Screen.h"
#include "FloatingText.h"
#include "GameMain.h"
#include "MainLoop.h"
#include "Scorecard.h"
#include "SetupScreens.h"
#include "TextWrap.h"

Screen g_simScreen;
AssetSurface g_terrainSurfaceObject;
Font g_dialogFont;
Font g_boldArialFont;
Font g_labelFont;
Font g_smallManualFont;
Font g_hudFont;
Font g_kleptoBigFont;
Font g_smallKleptoFont;
Font g_comicFont;
Font g_timesBoldFont;
FLCPalette g_palette;
Font g_systemFont;
Font g_scriptFont;
Font g_serifFont;
Font g_helveticaFont;
Font g_golfFont;

Rect g_screenRect;

char g_printBuffer[4096];

unsigned long g_startTime = 0;

inline int GlobalsUnrecovered(int a, int b)
{
    int c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    if (c > b)
        c -= b;
    if (c < 0)
        c = a;
    return c + b;
}
