#ifndef JGLPCX_H
#define JGLPCX_H

#include "jgltypes.h"

typedef unsigned short PcxWord;

struct PcxHeader {
    uchar   manufacturer;
    uchar   version;
    uchar   encoding;
    uchar   bitsPerPixel;
    PcxWord xmin;
    PcxWord ymin;
    PcxWord xmax;
    PcxWord ymax;
    PcxWord hdpi;
    PcxWord vdpi;
    uchar   colormap[48];
    uchar   reserved;
    uchar   numPlanes;
    PcxWord bytesPerLine;
    PcxWord paletteInfo;
    PcxWord hScreenSize;
    PcxWord vScreenSize;
    uchar   filler[54];
};

static char Civ3encrypt[] = "monkey sweats on a tuesday";

#endif
