#ifndef GOLF_FLCPALETTE_H
#define GOLF_FLCPALETTE_H

#include "jglpalettemgr.h"
#include "jglpalette.h"

class Time;

struct FLCPaletteEntry {
    int id;
    Time *timer;
    unsigned char first;
    unsigned char length;
    void *colors;
};

struct Dib {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[256];
};

class FLCPalette : public PaletteBase {
public:
    FLCPalette();
    ~FLCPalette();
    FLCPalette &operator=(const FLCPalette &);
    virtual int init();

    int set();
    void close();
    int set_from_dib(const Dib *bmi, int first, int count);
    int init_cycle(int id, int first, int length, int periodMs);
    static void __cdecl timer_callback(int id, void *palette);
    int start_cycle(int id);
    int get_pos(int id);
    int get_nearest_palette_index(unsigned char r, unsigned char g, unsigned char b, int avoidCycled);

    FLCPaletteEntry m_entries[5];
};

extern FLCPalette g_palettes[190];
extern FLCPalette g_introPalette;

extern FLCPalette *g_activePalettePtr;

extern FLCPalette *g_textPalette;

#endif
