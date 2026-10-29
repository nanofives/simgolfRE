#ifndef JGLBUFFERTYPED_H
#define JGLBUFFERTYPED_H
#include "jglbuffer.h"
#include "jgl.h"
#ifdef _USRDLL
inline GSBuffer::GSBuffer(int unrecovered) {}
#endif
extern "C" inline int ExtractRed16(int pixel)
{
    if (graphsys->screen_mask() == 1)
        return (pixel >> 8) & 0xf8;
    return (pixel >> 7) & 0xf8;
}
extern "C" inline int ExtractGreen16(int pixel)
{
    if (graphsys->screen_mask() == 1)
        return (pixel >> 3) & 0xfc;
    return (pixel >> 2) & 0xf8;
}
extern "C" inline int ExtractBlue16(int pixel)
{
    return (pixel << 3) & 0xf8;
}
#endif
