#ifndef JGLBUFFER_H
#define JGLBUFFER_H
#include "jglwin.h"
#include "gsobject.h"
class JGLPalette;
class JGLBuffer;
#include "jglpalettemgr.h"
class JGLSys;
#include "jglpalette.h"
#include "bufferbase.h"
#include "jglpixelformat.h"
#include <stdlib.h>
extern "C" JGLSys    *graphsys;
extern "C" inline int GetRectWidth(Rect *r)
{
    return r->right - r->left;
}
extern "C" inline int GetRectHeight(Rect *r)
{
    return r->bottom - r->top;
}
extern "C" int jgl_IntersectRect(Rect *dst, const Rect *src1, const Rect *src2);
extern "C" int RectEqual(const Rect *rect1, const Rect *rect2);
extern "C" int PackRGB16(uchar r, uchar g, uchar b);
#include "jglalloc.h"
extern "C" void memsetword(ushort *dst, ushort val, int count);
extern int      g_fill8DstSkip;
namespace jgl {
extern "C" void SetRect(Rect *r, int x, int y, int w, int h);
}
extern "C" void *memset(void *, int, unsigned int);
#include "gsbuffer.h"
#endif
