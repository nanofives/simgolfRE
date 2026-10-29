#ifndef DISPLAY_H
#define DISPLAY_H

struct IDirectDraw;
struct IDirectDrawSurface;

struct DisplayFormat {
    int bpp;
    int is565;
    unsigned short rShift;
    unsigned short gShift;
    unsigned short bShift;
    unsigned short rMask;
    unsigned short gMask;
    unsigned short bMask;
    int reserved;

    DisplayFormat() { is565 = 0; }
};


DisplayFormat *display_format();
int display_depth();
void display_close();
void display_unlock();

void OnDisplayShutdown();

#endif
