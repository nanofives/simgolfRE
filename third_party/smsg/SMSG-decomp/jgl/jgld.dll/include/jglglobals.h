#ifndef JGLGLOBALS_H
#define JGLGLOBALS_H
#include "jgltypes.h"
#include "jgllist.h"
class SysMessage;
class JGLSys;
struct PaletteBase;
extern "C" SysMessage *g_config;
extern "C" int (*g_pfnCopyData)(unsigned int, long);
extern "C" JGLSys  *sys;
extern "C" void    *g_hPalette;
extern int          g_scaleDenom;
extern int          g_scaleX;
extern int          g_scaleY;
extern PaletteBase *def_palette;
extern "C" JGLSys  *graphsys;
extern int          g_keyBlitDstAdj8;
extern int          g_blitTailPixels8;
extern int          g_keyBlitDwords8;
extern int          g_keyBlitSrcAdj8;
extern int          g_spriteCompress;
#endif
