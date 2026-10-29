#ifndef JGLPIXELFORMAT_H
#define JGLPIXELFORMAT_H
#include "jgltypes.h"
struct PixelFormat {
    PixelFormat();
    int    bpp;
    int    code;
    ushort rShift;
    ushort gShift;
    ushort bShift;
    ushort rMask;
    ushort gMask;
    ushort bMask;
};
#endif
