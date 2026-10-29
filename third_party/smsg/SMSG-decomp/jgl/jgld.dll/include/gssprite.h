#ifndef GSSPRITE_H
#define GSSPRITE_H
#include "gsobject.h"
#include "jglwin.h"
class JGLBuffer;
class JGLPalette;
class JGLSprite;
struct Run {
    uchar  start;
    uchar  len;
    ushort offset;
};
extern int g_spriteBytesRaw;
extern int g_spriteBytesSaved;



#define JGL_MSG_FILE    "c:\\jackdev\\jglsprite.cpp"
#define JGL_MSG_FMT     "%s"
#define JGL_MSG_DEPTH   "LIBERR_NOT_THIS_DEPTH"
#define JGL_MSG_COMPR   "NOT WRITTEN FOR COMPRESSION"
#define JGL_MSG_NOSURF  "NOT AVAILABLE TO THIS SURFACE"
#define JGL_RPTF0(rptno, msg) \
        do { if ((1 == _CrtDbgReport(rptno, JGL_MSG_FILE, __LINE__, 0, JGL_MSG_FMT, msg))) \
                _CrtDbgBreak(); } while (0)
class GSSprite : public GSObject {
public:
    virtual ~GSSprite();
    virtual int         init(void *px, int w, int h, int bpp, int memType, JGLPalette *pal) = 0;
    virtual void        close() = 0;
    virtual JGLSprite  *CopyFrom(JGLSprite *src) = 0;
    virtual void        set_palette(JGLPalette *pal) = 0;
    virtual JGLPalette *get_palette() = 0;
    virtual void        set_seethru(int colorKey) = 0;
    virtual int         get_seethru() = 0;
    virtual uchar      *get_data() = 0;
    virtual void        release_data(int bDone) = 0;
    virtual int         get_color_depth() = 0;
    virtual int         get_pitch() = 0;
    virtual int         get_width() = 0;
    virtual int         get_height() = 0;
    virtual int         change_color(int nSrcIndex, int nDstIndex) = 0;
    virtual int         AlphaBlit8to16(JGLBuffer *pSurf, int x, int y, ushort *pAlphaLut, JGLPalette *pPal) = 0;
    virtual int         RemappedBlit16(JGLBuffer *pSurf, int x, int y, ushort *pShadow, JGLPalette *pPal) = 0;
    virtual int         draw(JGLBuffer *pSurf, int x, int y, JGLPalette *pPal) = 0;
    virtual int         draw_tinted(JGLBuffer *pSurf, int x, int y, short tint, JGLPalette *pPal, char skipFrom) = 0;
    virtual int draw_color_scaled(JGLBuffer *pSurf, int x, int y, short tint, JGLPalette *pPal, char skipFrom) = 0;
    virtual int MaskedBlitToTarget(GSSprite *pMask, JGLBuffer *pSurf, JGLBuffer *pTarget, int x, int y,
                                   JGLPalette *pPal) = 0;
    virtual int draw_antialiased(GSSprite *pMask, JGLBuffer *pSurf, int x, int y, JGLPalette *pPal) = 0;
    virtual int draw_with_alpha(GSSprite *pAlphaSrc, JGLBuffer *pDst, int x, int y, JGLPalette *pPal) = 0;
    virtual int draw_254(JGLBuffer *pSurf, int x, int y, JGLPalette *pPal) = 0;
    virtual int Blit24(JGLBuffer *pSurf, int x, int y, int, int, JGLPalette *pPal) = 0;
    virtual int RecoloredBlit(JGLBuffer *pSurf, int x, int y, uchar *pRemap, ushort *pTint, JGLPalette *pPalette) = 0;
    virtual int SourceLutRecolorKeyBlit(JGLBuffer *pSurf, int x, int y, uchar *pRemap, uchar *pTint,
                                        JGLPalette *pPalette) = 0;
    virtual int ShadowBlit(JGLBuffer *pSurf, int x, int y, ushort *pShadow, JGLPalette *pPalette) = 0;
    virtual int draw_dest(JGLBuffer *pSurf, int x, int y, uchar *pLut, JGLPalette *pPalette) = 0;
    virtual int draw_mono(JGLBuffer *pSurf, int x, int y, int nColor, JGLPalette *pPal) = 0;
    virtual int draw_multi_table(JGLBuffer *pSurf, int x, int y, ushort *pBlend, JGLPalette *pPal) = 0;
    virtual int ShadowMaskBlit(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal) = 0;
    virtual int draw_multi_table_dest(JGLBuffer *pSurf, int x, int y, uchar *pLut, JGLPalette *pPal) = 0;
    virtual int draw_16_shadows(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal) = 0;
    virtual int DrawWithPaletteAndShadowLutScaled(JGLBuffer *dst, JGLBuffer *target, int x, int y, int denomX,
                                                  int denomY, int num, ushort *shadow, JGLPalette *pal) = 0;
    virtual int DrawWithPaletteAndShadowLut(JGLBuffer *dst, JGLBuffer *target, int x, int y, ushort *shadow,
                                            JGLPalette *pal) = 0;
    virtual int draw_civ3_units(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal) = 0;
    virtual int draw_translucent(JGLBuffer *pSurf, int x, int y, float fOpacity, JGLPalette *pPal, char bFlip) = 0;
    virtual int draw_sudo_trans(JGLBuffer *pSurf, int x, int y, int nPercent, JGLPalette *pPal) = 0;
    virtual int draw_z_outline(JGLBuffer *pSurf, int x, int y, JGLBuffer *pSrc, unsigned char cCutoff, int nColour,
                               JGLPalette *pPal) = 0;
    virtual int draw_z(JGLBuffer *pSurf, int x, int y, JGLBuffer *pSrc, unsigned char cCutoff, JGLPalette *pPal) = 0;
};
class JGLSprite : public GSSprite {
public:
    JGLSprite();
    JGLSprite(JGLSprite **ppOwner);
    virtual ~JGLSprite();
    int        init(void *px, int w, int h, int bpp, int memType, JGLPalette *pal);
    void       close();
    JGLSprite *CopyFrom(JGLSprite *src);
    void       set_seethru(int colorKey);
    int        draw_tinted(JGLBuffer *pSurf, int x, int y, short tint, JGLPalette *pPal, char skipFrom);
    int        draw_color_scaled(JGLBuffer *pSurf, int x, int y, short tint, JGLPalette *pPal, char skipFrom);
    int    MaskedBlitToTarget(GSSprite *pMask, JGLBuffer *pSurf, JGLBuffer *pTarget, int x, int y, JGLPalette *pPal);
    int    draw_antialiased(GSSprite *pMask, JGLBuffer *pSurf, int x, int y, JGLPalette *pPal);
    int    draw_with_alpha(GSSprite *pAlphaSrc, JGLBuffer *pDst, int x, int y, JGLPalette *pPal);
    int    draw_multi_table(JGLBuffer *pSurf, int x, int y, ushort *pBlend, JGLPalette *pPal);
    uchar *get_data();
    void   release_data(int bDone);
    void   set_palette(JGLPalette *pal);
    JGLPalette *get_palette();
    int         get_seethru();
    int         get_color_depth();
    int         get_pitch();
    int         get_width();
    int         get_height();
    int         draw_8_16c(JGLBuffer *dst, int x, int y, ushort *alpha);
    int         blit8to16_compressed(JGLBuffer *dst, int x, int y);
    int         draw_z_outline_8_16c(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff, int colour);
    int         draw_z_8_16c(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff);
    int         draw_dest_8_16c(JGLBuffer *dst, int x, int y, ushort *darken);
    int         draw_mono_8_16c(JGLBuffer *dst, int x, int y, int colour);
    int         blitRecolored8to16_compressed(JGLBuffer *dst, int x, int y, uchar *xlat, ushort *darken);
    int         blitRecoloredAfterPalette8to16_compressed(JGLBuffer *dst, int x, int y, uchar *xlat, ushort *darken);
    int         draw_16_shadows_16_16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int         draw_civ3_units_16_16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int         draw_sudo_trans_16_16_50(JGLBuffer *dst, int x, int y);
    int         draw_sudo_trans_16_16_25(JGLBuffer *dst, int x, int y);
    int         init_8(const uchar *pixels, int width, int height, int bpp, int memType);
    int         init_16(ushort *pixels, int width, int height, int bpp, int memType);
    int         draw_16_16(JGLBuffer *dst, int x, int y);
    int         draw_dest_16_16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int         draw_multi_table_dest_16_16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int         draw_254_8_8(JGLBuffer *dst, int x, int y);
    int         draw_8_8(JGLBuffer *dst, int x, int y);
    int         draw_mono_8_8(JGLBuffer *dst, int x, int y, int colour);
    int         draw_dest_8_8(JGLBuffer *dst, int x, int y, uchar *lut);
    int         SourceLutBlit(JGLBuffer *dst, int x, int y, uchar *lut);
    int         SourceLutBlitWithDestRecolorKey(JGLBuffer *dst, int x, int y, uchar *lut, uchar *shadowLut);
    int         draw_multi_table_dest_8_8(JGLBuffer *dst, int x, int y, uchar *lut);
    int         change_color(int nSrcIndex, int nDstIndex);
    int         AlphaBlit8to16(JGLBuffer *pSurf, int x, int y, ushort *pAlphaLut, JGLPalette *pPal);
    int         RemappedBlit16(JGLBuffer *pSurf, int x, int y, ushort *pShadow, JGLPalette *pPal);
    int         draw(JGLBuffer *pSurf, int x, int y, JGLPalette *pPal);
    int         draw_254(JGLBuffer *pSurf, int x, int y, JGLPalette *pPal);
    int         Blit24(JGLBuffer *pSurf, int x, int y, int, int, JGLPalette *pPal);
    int         RecoloredBlit(JGLBuffer *pSurf, int x, int y, uchar *pRemap, ushort *pTint, JGLPalette *pPalette);
    int SourceLutRecolorKeyBlit(JGLBuffer *pSurf, int x, int y, uchar *pRemap, uchar *pTint, JGLPalette *pPalette);
    int ShadowBlit(JGLBuffer *pSurf, int x, int y, ushort *pShadow, JGLPalette *pPalette);
    int draw_dest(JGLBuffer *pSurf, int x, int y, uchar *pLut, JGLPalette *pPalette);
    int draw_mono(JGLBuffer *pSurf, int x, int y, int nColor, JGLPalette *pPal);
    int ShadowMaskBlit(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal);
    int draw_z_outline(JGLBuffer *pSurf, int x, int y, JGLBuffer *pSrc, unsigned char cCutoff, int nColour,
                       JGLPalette *pPal);
    int draw_z(JGLBuffer *pSurf, int x, int y, JGLBuffer *pSrc, unsigned char cCutoff, JGLPalette *pPal);
    int draw_multi_table_dest(JGLBuffer *pSurf, int x, int y, uchar *pLut, JGLPalette *pPal);
    int draw_16_shadows(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal);
    int draw_civ3_units(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal);
    int draw_translucent(JGLBuffer *pSurf, int x, int y, float fOpacity, JGLPalette *pPal, char bFlip);
    int draw_sudo_trans(JGLBuffer *pSurf, int x, int y, int nPercent, JGLPalette *pPal);
    int DrawWithPaletteAndShadowLut(JGLBuffer *dst, JGLBuffer *target, int x, int y, ushort *shadow, JGLPalette *pal);
    int DrawWithPaletteAndShadowLutScaled(JGLBuffer *dst, JGLBuffer *target, int x, int y, int denomX, int denomY,
                                          int num, ushort *shadow, JGLPalette *pal);
    int IsCompressed();
    int change_color_8(int nOldIndex, int nNewIndex);
    int draw_254_8_16(JGLBuffer *dst, int x, int y);
    int draw_8_16(JGLBuffer *dst, int x, int y);
    int draw_z_outline_8_16(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff, int colour);
    int draw_z_8_16(JGLBuffer *dst, int x, int y, JGLBuffer *src, uchar cutoff);
    int draw_antialiased_8_16(JGLSprite *alpha, JGLBuffer *dst, JGLBuffer *target, int x, int y);
    int MaskedBlit8to16(JGLSprite *alpha, JGLBuffer *dst, int x, int y);
    int draw_civ3_units_8_16(JGLBuffer *dst, int x, int y, ushort *shadow, JGLBuffer *target);
    int draw_mono_8_16(JGLBuffer *dst, int x, int y, int colour);
    int draw_dest_8_16(JGLBuffer *dst, int x, int y, uchar *remap, ushort *tint);
    int Complex(JGLBuffer *dst, int x, int y, ushort *shadow, ushort *blend);
    int draw_tinted_8_16(JGLBuffer *dst, int x, int y, ushort tint, uchar skipFrom);
    int draw_color_scaled_8_16(JGLBuffer *dst, int x, int y, ushort tint, uchar skipFrom);
    int SourceLutBlit8to16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int blitAlpha8to16_uncompressed(JGLBuffer *dst, int x, int y, ushort *shadow);
    int SPR_DrawSprite_WithShadow(JGLBuffer *dst, int x, int y, ushort *shadow);
    int draw_multi_table_8_16(JGLBuffer *dst, int x, int y, ushort *blend);
    int draw_multi_table_dest_8_16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int draw_16_shadows_8_16(JGLBuffer *dst, int x, int y, ushort *shadow);
    int ShadowLutBlitScaledToTarget(JGLBuffer *dst, int x, int y, int denom, int num, ushort *shadow,
                                    JGLBuffer *target);
    int draw_translucent_555_8_16_1_2(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_1_4(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_3_4(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_1_8(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_7_8(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_1_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_15_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_3_8(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_5_8(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_7_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_9_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_3_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_5_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_555_8_16_11_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_1_2(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_1_4(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_3_4(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_3_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_13_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_5_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_translucent_565_8_16_11_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int draw_sudo_trans_8_16_25(JGLBuffer *dst, int x, int y);
    int draw_translucent_565_8_16_1_8(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_7_8(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_1_16(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_15_16(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_3_8(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_5_8(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_7_16(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_translucent_565_8_16_9_16(JGLBuffer *dst, int x, int y, char skipHigh);
    int draw_sudo_trans_8_16_50(JGLBuffer *, int, int);
    int draw_translucent_555_8_16_13_16(JGLBuffer *dst, int x, int y, char alphaTest);
    int m_field08;
    JGLSprite      **m_ppOwner;
    JGLPalette      *m_palette;
    uchar           *m_pixels;
    int              m_flags;
    Run             *m_runs;
    int              m_bpp;
    unsigned         m_dataSize;
    int              m_colorKey;
    int              m_srcStride;
    int              m_width;
    int              m_height;
    CRITICAL_SECTION m_lock;
};
#endif
