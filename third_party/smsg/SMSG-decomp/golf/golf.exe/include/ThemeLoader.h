#ifndef THEMELOADER_H
#define THEMELOADER_H

#include "crt.h"
#include "Golfer.h"
#include "IntroVideo.h"
#include "FloatingText.h"
#include "Plot.h"
#include "ListDialog.h"
#include "GolferPairing.h"

void LoadThemeData();

int LoadGolferFromTheme(const char *file, int type, int golfer);

void LoadThemeFiles(int enable);

#endif
