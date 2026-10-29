#include <string.h>
#include "ScreenText.h"

inline void DrawLeft(const char *s, int x, int y)
{
    if (s)
        g_pMenu->write_l(s, x, y, (int)strlen(s));
}

inline void DrawCentered(const char *s, int x, int y)
{
    if (s)
        g_pMenu->RenderTextCenteredInWidth(s, x, y, 0, (int)strlen(s));
}

inline void ClampTextY(int &y)
{
    int bottom = g_pMenu->GetSurfaceHeight();
    if (g_pMenu->text_line_height() + y >= bottom)
        y = g_pMenu->GetSurfaceHeight() - g_pMenu->text_line_height() - 1;
}

int ScaleXCoordinate(int x)
{
    return (g_screenWidth * x + 160) / 320;
}

void DrawSurfaceLine(AssetSurface *s, int x1, int y1, int x2, int y2, int color)
{
    s->line(x1, y1, x2, y2, color & 0xff);
}

void DrawTextWithColor(const char *s, int x, int y, int color)
{
    g_pMenu->set_text_color(color, -1, 2, 2);
    DrawLeft(s, x, y);
}

void DrawTextScaledAtPosition(const char *s, int x, int y, int color)
{
    g_pMenu->set_text_color(color, -1, 2, 2);
    ClampTextY(y);
    DrawLeft(s, ScaleXCoordinate(x), ScaleXCoordinate(y));
}

void DrawTextClampedToScreen(const char *s, int x, int y, int color)
{
    g_pMenu->set_text_color(color, 1, 0, 1);
    ClampTextY(y);
    DrawLeft(s, x, y);
}

void DrawTextCenteredAtY(const char *s, int x, int y, int color)
{
    g_pMenu->set_text_color(color, -1, 2, 2);
    DrawCentered(s, x, y);
}

void DrawTextCenteredWithShadow(const char *s, int x, int y, int color)
{
    g_pMenu->set_text_color(color, 1, 0, 1);
    DrawCentered(s, x, y);
}

char *CopyText(const char *s)
{
    char *copy = new char[strlen(s) + 1];
    strcpy(copy, s);
    return copy;
}
