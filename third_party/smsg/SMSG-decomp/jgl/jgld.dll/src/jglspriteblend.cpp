#include "jgldefs.h"
#include "jgltypes.h"
#include "jglsprite.h"
#include "jglbuffertyped.h"
#include "jglwin.h"
#include <string.h>
#include "jglalloc.h"

int g_spriteCompress = 0;
int g_spriteBytesRaw = 0;
int g_spriteBytesSaved = 0;

int JGLSprite::draw_multi_table(JGLBuffer *pSurf, int x, int y, ushort *pBlend, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    if (pSurf->get_pixformat()->bpp != 16 || get_color_depth() != 8)
        return 23;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    return draw_multi_table_8_16(pSurf, x, y, pBlend);
}
int JGLSprite::draw_tinted(JGLBuffer *pSurf, int x, int y, short tint, JGLPalette *pPal, char skipFrom)
{
    JGLPalette *pSavedPal;
    if (pSurf->get_pixformat()->bpp != 16 || get_color_depth() != 8)
        return 23;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    return draw_tinted_8_16(pSurf, x, y, tint, skipFrom);
}
int JGLSprite::draw_color_scaled(JGLBuffer *pSurf, int x, int y, short tint, JGLPalette *pPal, char skipFrom)
{
    JGLPalette *pSavedPal;
    if (pSurf->get_pixformat()->bpp != 16 || get_color_depth() != 8)
        return 23;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    return draw_color_scaled_8_16(pSurf, x, y, tint, skipFrom);
}
int JGLSprite::draw_antialiased(GSSprite *pMask, JGLBuffer *pSurf, int x, int y, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    if (pSurf->get_pixformat()->bpp != 16 || get_color_depth() != 8 || pMask->get_color_depth() != 8)
        return 23;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    return MaskedBlit8to16((JGLSprite *)pMask, pSurf, x, y);
}
int JGLSprite::MaskedBlitToTarget(GSSprite *pMask, JGLBuffer *pSurf, JGLBuffer *pTarget, int x, int y, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    if (pSurf->get_pixformat()->bpp != 16 || get_color_depth() != 8 || pMask->get_color_depth() != 8)
        return 23;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    return draw_antialiased_8_16((JGLSprite *)pMask, pSurf, pTarget, x, y);
}
int JGLSprite::draw_with_alpha(GSSprite *pAlphaSrc, JGLBuffer *pDst, int x, int y, JGLPalette *pPal)
{
    JGLPalette   *savedPal;
    int           dstIndex = 0;
    int           srcIndex = 0;
    PALETTEENTRY *rgb;
    ushort        alpha;
    int           limit;
    int           rowAdvance;
    uchar        *srcp;
    uchar        *alphap;
    ushort       *dstp;
    int           row;
    int           col;
    limit = pDst->get_width() * pDst->get_height() - (x + pDst->get_width() * y);
    if (pDst->get_pixformat()->bpp != 16 || this->get_color_depth() != 8 || pAlphaSrc->get_color_depth() != 8)
        return 0x17;
    rowAdvance = pDst->get_width() - this->m_width;
    srcp = this->get_data();
    alphap = pAlphaSrc->get_data();
    dstp = (ushort *)pDst->SURF_GetPixelPtr(x, y);
    savedPal = this->m_palette;
    if (pPal != 0)
        this->m_palette = pPal;
    for (row = 0; row < this->m_height; row++) {
        for (col = 0; col < this->m_width; col++, srcIndex++, dstIndex++) {
            if (dstIndex > limit)
                return 0x1b;
            if (srcp[srcIndex] != 0xff) {
                rgb = this->m_palette->get_entry(srcp[srcIndex]);
                alpha = alphap[srcIndex];
                if (alpha == 0) {
                    dstp[dstIndex] = (ushort)PackRGB16(rgb->peRed, rgb->peGreen, rgb->peBlue);
                } else if (alpha == 0xff) {
                    continue;
                }
                alpha++;
                dstp[dstIndex] = (ushort)PackRGB16(
                    ((uchar)ExtractRed16(dstp[dstIndex]) * alpha >> 8) + (rgb->peRed * (0x100 - alpha) >> 8),
                    ((uchar)ExtractGreen16(dstp[dstIndex]) * alpha >> 8) + (rgb->peGreen * (0x100 - alpha) >> 8),
                    ((uchar)ExtractBlue16(dstp[dstIndex]) * alpha >> 8) + (rgb->peBlue * (0x100 - alpha) >> 8));
            }
        }
        dstIndex += rowAdvance;
    }
    return 0;
}
JGLSprite *JGLSprite::CopyFrom(JGLSprite *pSrc)
{
    close();
    JGLSprite *src = pSrc;
    m_palette = src->m_palette;
    m_flags = src->m_flags;
    m_height = src->m_height;
    m_width = src->m_width;
    m_srcStride = src->m_srcStride;
    m_colorKey = src->m_colorKey;
    m_bpp = src->m_bpp;
    m_dataSize = src->m_dataSize;
    m_pixels = (uchar *)jgl_malloc(m_dataSize);
    memcpy(m_pixels, src->m_pixels, m_dataSize);
    if (src->m_runs) {
        m_runs = (Run *)jgl_malloc(m_height * 4);
        memcpy(m_runs, src->m_runs, m_height * 4);
    }
    return this;
}
