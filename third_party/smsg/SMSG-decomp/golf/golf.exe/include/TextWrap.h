#ifndef TEXTWRAP_H
#define TEXTWRAP_H

#include "AssetSurface.h"
#include "Plot.h"
#include "ScreenText.h"

int WrapText(int widthChars);

int DrawTextWordWrapped(int widthChars, int x, int y, unsigned color);

extern Font g_dialogFont;

void ShowTextInputDialog(const char *prompt, int x, int y, int maxLen);

#endif
