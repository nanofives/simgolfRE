#ifndef INTROVIDEO_H
#define INTROVIDEO_H

#include "BinkPlayer.h"

void PlayBinkVideoWithTimeout(const char *file, int timeoutMS);
void PlayIntroVideo();
int FindFilesWithFilter(const char *pattern, const char *mustHave, const char *exclude);

extern char g_fileList[100][100];

extern BinkPlayer g_binkPlayer;

class FLCPalette;

#endif
