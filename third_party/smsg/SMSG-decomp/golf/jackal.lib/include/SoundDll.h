#ifndef SOUNDDLL_H
#define SOUNDDLL_H

#include "WaveSoundDevice.h"
#include "SoundObject1.h"
#include "WaveInSoundDevice.h"


extern void *gbl_fntable[MAX_SOUND_FUNCS];
extern void *gbl_hsoundinst;
extern int gbl_init_flag;

int init_sound_timer(int ms_resolution, int flags);
int get_sound_version();
void clear_fntable();
int unload_sound_dll();
int load_sound_dll();

extern Wave_Device wave_device;
extern Midi_Device midi_device;
extern Wave_In_Device wave_in_device;

int init_sound(HWND hwnd, int flags);
int release_sound();

#include "Dialog.h"

#endif
