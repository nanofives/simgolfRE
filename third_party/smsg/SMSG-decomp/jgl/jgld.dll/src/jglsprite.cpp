#include "jgldefs.h"
























#include <crtdbg.h>
#include "jglsprite.h"













const float SCALE_FUZZ = 0.03125f;

#include "jglalloc.h"






JGLSprite::JGLSprite()
{
    m_ppOwner = 0;
    m_field08 = 0;
    m_palette = 0;
    m_pixels = 0;
    m_flags = 0;
    m_runs = 0;
    m_bpp = 0;
    m_dataSize = 0;
    m_colorKey = 0xff;
    m_srcStride = 0;
    m_width = 0;
    m_height = 0;
    InitializeCriticalSection(&m_lock);
}
JGLSprite::JGLSprite(JGLSprite **ppOwner)
{
    m_ppOwner = ppOwner;
    m_field08 = 0;
    m_palette = 0;
    m_pixels = 0;
    m_flags = 0;
    m_runs = 0;
    m_bpp = 0;
    m_dataSize = 0;
    m_colorKey = 0xff;
    m_srcStride = 0;
    m_width = 0;
    m_height = 0;
    InitializeCriticalSection(&m_lock);
}
JGLSprite::~JGLSprite()
{
    if (m_ppOwner != 0)
        *m_ppOwner = 0;
    DeleteCriticalSection(&m_lock);
    close();
}
void JGLSprite::close()
{
    if (m_pixels != 0) {
        if (m_flags & 1)
            g_spriteBytesSaved -= m_width * m_height - m_dataSize - m_height * sizeof(Run);
        g_spriteBytesRaw -= m_width * m_height;
        jgl_free(m_pixels);
        m_pixels = 0;
    }
    if (m_runs != 0) {
        jgl_free(m_runs);
        m_runs = 0;
    }
    m_srcStride = 0;
    m_width = 0;
    m_height = 0;
    m_flags = 0;
    m_dataSize = 0;
}
int JGLSprite::init(void *pixels, int width, int height, int bpp, int memType, JGLPalette *pal)
{
    int rc;
    switch (bpp) {
    case 8:
        rc = init_8((const uchar *)pixels, width, height, bpp, memType);
        break;
    case 16:
        rc = init_16((ushort *)pixels, width, height, bpp, memType);
        break;
    }
    if (rc != 0)
        return rc;
    set_palette(pal);
    return 0;
}



int JGLSprite::change_color(int nSrcIndex, int nDstIndex)
{
    switch (m_bpp) {
    case 8:
        return change_color_8(nSrcIndex, nDstIndex);
    default:




        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        return 23;
    }
}

int JGLSprite::draw_254(JGLBuffer *pSurf, int x, int y, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    if (pSurf == 0 || !get_data())
        return 7;

    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_254_8_8(pSurf, x, y);
            }
            break;
        case 16:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_254_8_16(pSurf, x, y);
            }
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:

        switch (pSurf->get_pixformat()->bpp) {
        default:




            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::draw(JGLBuffer *pSurf, int x, int y, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    if (pSurf == 0 || !get_data())
        return 7;

    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_8_8(pSurf, x, y);
            }
            break;
        case 16:
            if (IsCompressed())
                nResult = blit8to16_compressed(pSurf, x, y);
            else
                nResult = draw_8_16(pSurf, x, y);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_16_16(pSurf, x, y);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}

int JGLSprite::draw_z_outline(JGLBuffer *pSurf, int x, int y, JGLBuffer *pSrc, unsigned char cCutoff, int nColour,
                              JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    if (pSurf == 0 || !get_data())
        return 7;

    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
                nResult = 23;
            }
            break;
        case 16:
            if (IsCompressed()) {
                nResult = draw_z_outline_8_16c(pSurf, x, y, pSrc, cCutoff, nColour);
            } else {
                nResult = draw_z_outline_8_16(pSurf, x, y, pSrc, cCutoff, nColour);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
                nResult = 23;
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::draw_z(JGLBuffer *pSurf, int x, int y, JGLBuffer *pSrc, unsigned char cCutoff, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    if (pSurf == 0 || !get_data())
        return 7;

    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
                nResult = 23;
            }
            break;
        case 16:
            if (IsCompressed()) {
                nResult = draw_z_8_16c(pSurf, x, y, pSrc, cCutoff);
            } else {
                nResult = draw_z_8_16(pSurf, x, y, pSrc, cCutoff);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
                nResult = 23;
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}



int JGLSprite::draw_mono(JGLBuffer *pSurf, int x, int y, int nColor, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0 || !get_data())
        return 16;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_mono_8_8(pSurf, x, y, nColor);
            }
            break;
        case 16:
            if (IsCompressed()) {
                nResult = draw_mono_8_16c(pSurf, x, y, nColor);
            } else {
                nResult = draw_mono_8_16(pSurf, x, y, nColor);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        default:






            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::draw_dest(JGLBuffer *pSurf, int x, int y, uchar *pLut, JGLPalette *pPalette)
{
    JGLPalette *pSavedPalette;
    int         nResult;

    pSavedPalette = m_palette;
    if (pPalette != 0)
        m_palette = pPalette;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_dest_8_8(pSurf, x, y, pLut);
            }
            break;
        case 16:
            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        default:








            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
        case 16:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        default:








            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPalette;
    release_data(1);
    return nResult;
}


int JGLSprite::ShadowBlit(JGLBuffer *pSurf, int x, int y, ushort *pShadow, JGLPalette *pPalette)
{
    JGLPalette *pSavedPalette;
    int         nResult;

    pSavedPalette = m_palette;
    if (pPalette != 0)
        m_palette = pPalette;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:

            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed())
                nResult = draw_dest_8_16c(pSurf, x, y, pShadow);
            else
                nResult = SPR_DrawSprite_WithShadow(pSurf, x, y, pShadow);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
        case 16:
            if (IsCompressed()) {


                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_dest_16_16(pSurf, x, y, pShadow);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }
    m_palette = pSavedPalette;
    release_data(1);
    return nResult;
}

GSSprite::~GSSprite() {}

int JGLSprite::RemappedBlit16(JGLBuffer *pSurf, int x, int y, ushort *pShadow, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = SourceLutBlit(pSurf, x, y, (uchar *)pShadow);
            }
            break;
        case 16:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = SourceLutBlit8to16(pSurf, x, y, pShadow);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:

        switch (pSurf->get_pixformat()->bpp) {
        default:





            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::AlphaBlit8to16(JGLBuffer *pSurf, int x, int y, ushort *pAlphaLut, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed())
                nResult = draw_8_16c(pSurf, x, y, pAlphaLut);
            else
                nResult = blitAlpha8to16_uncompressed(pSurf, x, y, pAlphaLut);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:

            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::SourceLutRecolorKeyBlit(JGLBuffer *pSurf, int x, int y, uchar *pRemap, uchar *pTint, JGLPalette *pPal)
{
    JGLPalette *pSavedPalette;
    int         nResult;

    nResult = 0;
    pSavedPalette = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = SourceLutBlitWithDestRecolorKey(pSurf, x, y, pRemap, pTint);
            }
            break;
        case 16:
            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        default:








            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:

        switch (pSurf->get_pixformat()->bpp) {
        default:




            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPalette;
    release_data(1);
    return nResult;
}


int JGLSprite::RecoloredBlit(JGLBuffer *pSurf, int x, int y, uchar *pRemap, ushort *pTint, JGLPalette *pPalette)
{
    JGLPalette *pSavedPalette;
    int         nResult;

    nResult = 0;
    pSavedPalette = m_palette;
    if (pPalette != 0)
        m_palette = pPalette;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:






            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed())
                nResult = blitRecolored8to16_compressed(pSurf, x, y, pRemap, pTint);
            else
                nResult = draw_dest_8_16(pSurf, x, y, pRemap, pTint);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:

            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPalette;
    release_data(1);
    return nResult;
}


int JGLSprite::Blit24(JGLBuffer *pSurf, int x, int y, int, int, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
        break;
        switch (pSurf->get_pixformat()->bpp) {
        case 8:

            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:



            nResult = (int)pPal;
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:

            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::draw_multi_table_dest(JGLBuffer *pSurf, int x, int y, uchar *pLut, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:
            if (IsCompressed()) {






                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_multi_table_dest_8_8(pSurf, x, y, pLut);
            }
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:

        switch (pSurf->get_pixformat()->bpp) {
        default:





            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::ShadowMaskBlit(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:






            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed()) {
                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_multi_table_dest_8_16(pSurf, x, y, pShadow);
            }
            break;
        default:


            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_multi_table_dest_16_16(pSurf, x, y, pShadow);
            }
            break;
        default:



            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}

#include "jglspriteaccessors.inl"



int JGLSprite::draw_16_shadows(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:










            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_16_shadows_8_16(pSurf, x, y, pShadow);
            }
            break;
        default:




            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 16:
            if (IsCompressed()) {



                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_16_shadows_16_16(pSurf, x, y, pShadow);
            }
            break;
        default:





            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::DrawWithPaletteAndShadowLut(JGLBuffer *dst, JGLBuffer *tgt, int x, int y, ushort *shad, JGLPalette *pal)
{
    JGLPalette *saved = m_palette;

    if (pal != 0)
        m_palette = pal;

    return draw_civ3_units_8_16(dst, x, y, shad, tgt);
}


int JGLSprite::DrawWithPaletteAndShadowLutScaled(JGLBuffer *dst, JGLBuffer *target, int x, int y, int denomX,
                                                 int denomY, int num, ushort *shadow, JGLPalette *pal)
{
    if (denomX != denomY)
        return 1;

    JGLPalette *saved = m_palette;

    if (pal != 0)
        m_palette = pal;

    return ShadowLutBlitScaledToTarget(dst, x, y, denomX, num, shadow, target);
}




int JGLSprite::draw_civ3_units(JGLBuffer *pSurf, int x, int y, unsigned short *pShadow, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:










            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_civ3_units_8_16(pSurf, x, y, pShadow, 0);
            }
            break;
        default:




            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 16:
            if (IsCompressed()) {



                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                nResult = draw_civ3_units_16_16(pSurf, x, y, pShadow);
            }
            break;
        default:





            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}



int JGLSprite::draw_translucent(JGLBuffer *pSurf, int x, int y, float fOpacity, JGLPalette *pPal, char bFlip)
{
    JGLPalette *pSavedPal;
    int         nResult;
    bool        bKeyed;

    bKeyed = true;
    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;
    if (pSurf->get_pixformat()->code == 1)
        bKeyed = false;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:









            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else if (fOpacity < 0.0 || fOpacity > 1.0) {
                nResult = 3;
            } else if (fOpacity > SCALE_FUZZ + 0.5) {
                if (fOpacity > SCALE_FUZZ + 0.75) {
                    if (fOpacity > SCALE_FUZZ + 0.875) {
                        if (fOpacity > 100.0f - SCALE_FUZZ)
                            nResult = draw(pSurf, x, y, pPal);
                        else
                            nResult = bKeyed ? draw_translucent_555_8_16_15_16(pSurf, x, y, bFlip)
                                             : draw_translucent_565_8_16_15_16(pSurf, x, y, bFlip);
                    } else if (fOpacity < 0.875 - SCALE_FUZZ) {
                        nResult = bKeyed ? draw_translucent_555_8_16_13_16(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_13_16(pSurf, x, y, bFlip);
                    } else {
                        nResult = bKeyed ? draw_translucent_555_8_16_7_8(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_7_8(pSurf, x, y, bFlip);
                    }
                } else if (fOpacity < 0.75 - SCALE_FUZZ) {
                    if (fOpacity > SCALE_FUZZ + 0.625) {
                        nResult = bKeyed ? draw_translucent_555_8_16_11_16(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_11_16(pSurf, x, y, bFlip);
                    } else if (fOpacity < 0.625 - SCALE_FUZZ) {
                        nResult = bKeyed ? draw_translucent_555_8_16_9_16(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_9_16(pSurf, x, y, bFlip);
                    } else {
                        nResult = bKeyed ? draw_translucent_555_8_16_5_8(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_5_8(pSurf, x, y, bFlip);
                    }
                } else {
                    nResult = bKeyed ? draw_translucent_555_8_16_3_4(pSurf, x, y, bFlip)
                                     : draw_translucent_565_8_16_3_4(pSurf, x, y, bFlip);
                }
            } else if (fOpacity < 0.5 - SCALE_FUZZ) {
                if (fOpacity > SCALE_FUZZ + 0.25) {
                    if (fOpacity > SCALE_FUZZ + 0.375) {
                        nResult = bKeyed ? draw_translucent_555_8_16_7_16(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_7_16(pSurf, x, y, bFlip);
                    } else if (fOpacity < 0.375 - SCALE_FUZZ) {
                        nResult = bKeyed ? draw_translucent_555_8_16_5_16(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_5_16(pSurf, x, y, bFlip);
                    } else {
                        nResult = bKeyed ? draw_translucent_555_8_16_3_8(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_3_8(pSurf, x, y, bFlip);
                    }
                } else if (fOpacity < 0.25 - SCALE_FUZZ) {
                    if (fOpacity > SCALE_FUZZ + 0.125) {
                        nResult = bKeyed ? draw_translucent_555_8_16_3_16(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_3_16(pSurf, x, y, bFlip);
                    } else if (fOpacity < 0.125 - SCALE_FUZZ) {
                        if (fOpacity > SCALE_FUZZ)
                            nResult = bKeyed ? draw_translucent_555_8_16_1_16(pSurf, x, y, bFlip)
                                             : draw_translucent_565_8_16_1_16(pSurf, x, y, bFlip);
                        else
                            nResult = 0;
                    } else {
                        nResult = bKeyed ? draw_translucent_555_8_16_1_8(pSurf, x, y, bFlip)
                                         : draw_translucent_565_8_16_1_8(pSurf, x, y, bFlip);
                    }
                } else {
                    nResult = bKeyed ? draw_translucent_555_8_16_1_4(pSurf, x, y, bFlip)
                                     : draw_translucent_565_8_16_1_4(pSurf, x, y, bFlip);
                }
            } else {
                nResult = bKeyed ? draw_translucent_555_8_16_1_2(pSurf, x, y, bFlip)
                                 : draw_translucent_565_8_16_1_2(pSurf, x, y, bFlip);
            }
            break;
        default:
















            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:




        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}


int JGLSprite::draw_sudo_trans(JGLBuffer *pSurf, int x, int y, int nPercent, JGLPalette *pPal)
{
    JGLPalette *pSavedPal;
    int         nResult;

    nResult = 0;
    pSavedPal = m_palette;
    if (pPal != 0)
        m_palette = pPal;
    if (pSurf == 0)
        return 16;
    if (!get_data())
        return 7;

    switch (m_bpp) {
    case 8:
        switch (pSurf->get_pixformat()->bpp) {
        case 8:










            JGL_RPTF0(_CRT_WARN, JGL_MSG_NOSURF);
            break;
        case 16:
            if (IsCompressed()) {

                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                switch (nPercent) {
                case 100:
                    nResult = draw(pSurf, x, y, pPal);
                    break;
                case 50:
                    nResult = draw_sudo_trans_8_16_50(pSurf, x, y);
                    break;
                case 25:
                    nResult = draw_sudo_trans_8_16_25(pSurf, x, y);
                    break;
                default:
                    nResult = 3;
                    break;
                }
            }
            break;
        default:





            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    case 16:
        switch (pSurf->get_pixformat()->bpp) {
        case 16:
            if (IsCompressed()) {



                JGL_RPTF0(_CRT_WARN, JGL_MSG_COMPR);
            } else {
                switch (nPercent) {
                case 100:
                    nResult = draw(pSurf, x, y, pPal);
                    break;
                case 50:
                    nResult = draw_sudo_trans_16_16_50(pSurf, x, y);
                    break;
                case 25:
                    nResult = draw_sudo_trans_16_16_25(pSurf, x, y);
                    break;
                default:
                    nResult = 3;
                    break;
                }
            }
            break;
        default:






            JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
            nResult = 23;
            break;
        }
        break;
    default:


        JGL_RPTF0(_CRT_WARN, JGL_MSG_DEPTH);
        nResult = 23;
        break;
    }

    m_palette = pSavedPal;
    release_data(1);
    return nResult;
}



int g_keyBlitDstAdj8 = 0;
int g_blitTailPixels8 = 0;
int g_keyBlitDwords8 = 0;
int g_keyBlitSrcAdj8 = 0;
