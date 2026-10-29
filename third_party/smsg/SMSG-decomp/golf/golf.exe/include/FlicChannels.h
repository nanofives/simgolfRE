#ifndef FLICCHANNELS_H
#define FLICCHANNELS_H

struct SpriteObject;

SpriteObject *FLC_GetNextFrameSurface(int channel);
SpriteObject *FLC_LoadAndGetSurface(const char *path, int channel, int *frameCount);
void FLC_ResetAndCleanup(int channel);

#endif
