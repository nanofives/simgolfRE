#ifndef JGLPALETTETYPES_H
#define JGLPALETTETYPES_H
#include "jglwin.h"
struct LogPalette256 {
    unsigned short palVersion;
    unsigned short palNumEntries;
    PALETTEENTRY   palPalEntry[256];
};
struct LargeInt {
    unsigned int LowPart;
    int          HighPart;
};
#endif
