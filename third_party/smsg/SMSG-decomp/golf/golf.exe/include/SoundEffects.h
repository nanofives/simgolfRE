#ifndef SOUNDEFFECTS_H
#define SOUNDEFFECTS_H

#include "SoundEntry.h"

extern Wave g_sounds[300];

void ConfigureAndPlaySound(int index, int volume, int pan, int pitch, int option);
void FadeOutSound(int index);
void LoadSoundEffects();
void CleanupAllSoundObjects();

#define SOUND_COURSE_AMBIENCE        0x2d
#define SOUND_TIME_TUNNEL            0x26
#define SOUND_REFUSED                0x18
#define SOUND_PURCHASE               0x33
#define SOUND_TADA                   0x38
#define SOUND_CONSTRUCTION           0x90
#define SOUND_CAMERA                 0x95
#define SOUND_FULL_VOLUME            100
#define SOUND_WIN                    35
#define SOUND_LOSE                   36
#define SOUND_APPLAUSE               46
#define SOUND_APPLAUSE_GOOD          47
#define SOUND_APPLAUSE_BAD           48
#define SOUND_TITLE_MUSIC            0x80
#define SOUND_CREDITS_MUSIC          0x81
#define SOUND_FLIES                  0x22
#define SOUND_AMBIENT_LAKE           0x1d
#define SOUND_AMBIENT_WIND           0x1e
#define SOUND_AMBIENT_DESERT         0x1f
#define SOUND_AMBIENT_FOREST         0x20
#define SOUND_AMBIENT_GRASS          0x21
#define SOUND_AMBIENT_SPRING         0x28
#define SOUND_TREE_SPROUT            0x91
#define SOUND_PALM                   0x92
#define SOUND_MAPLE                  0x93
#define SOUND_FLOWER_BED             0x94
#define SOUND_BASS_UP                199
#define SOUND_BASS_DOWN              200
#define SOUND_BUILDING               0xff
#define SOUND_PAINT_ROUGH            0x100
#define SOUND_PAINT_FAIRWAY          0x101
#define SOUND_PLACE_ROCKS            0x102
#define SOUND_PLACE_WATER            0x103
#define SOUND_PAINT_BUNKER           0x104
#define SOUND_PAINT_TEE_GREEN        0x105
#define SOUND_PLACE_BENCH            0x106
#define SOUND_PLACE_BRIDGE           0x107
#define SOUND_PLACE_PATH             0x108

#endif
