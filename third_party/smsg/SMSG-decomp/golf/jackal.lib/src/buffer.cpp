#include "jgldefs.h"
#include "AssetSurface.h"
#include "ImageSet.h"
#include "TextRenderer.h"
#include "MiscUtil.h"
#include "crt.h"
#include "jglpixelformat.h"
int g_defaultFormatOn = 1;
int g_markupDefault = 1;
#include "jglpcx.h"
#include "filemap.h"
#include "Sprite.h"
#include "Surface.h"
#include "PolyFill.h"
#include "PaletteList.h"
#include "Display.h"
#include "ComboBox.h"
#include "WidgetBase.h"
#include "ListBoxCtl.h"
#include "HeapNew.h"
#include "FLCPalette.h"
#include "JackalUtil.h"

#pragma intrinsic(strlen)

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define CLIPLEN(s, n) MIN((int)strlen(s), (n))

AssetSurface::AssetSurface()
{
    m_surface = 0;
    m_dirty = 0;
    for (int i = 0; i < 40; i++) {
        m_linkParam[i] = 0;
        m_linkCommand[i] = -2;
    }
    m_palette = 0;
    m_memType = 0;
    m_itemHook = 0;
    m_drawHook = 0;
    m_pendingIndent = 0;
    m_hangingIndent = 0;
    m_lineCount = 0;
    m_depth = 0;
    m_layoutAux = 0;
    m_hotLink = -1;
    m_renderStyle = 0;
    m_wrapStyle = 0;
    m_style = 0;
    m_linkState = 0;
    m_formatOn = g_defaultFormatOn;
    m_dropIndex = -1;
    m_wrapDropState = 0;
    m_dropState = 0;
    m_renderDropState = 0;
    m_dropArrow = 0;
    m_reserved58 = 0;
    m_fonts[0] = g_defaultFont;
    m_textColors[0][0] = 0;
    m_textColors[1][0] = -1;
    m_textColors[2][0] = 2;
    m_textColors[3][0] = 2;
    m_fonts[1] = 0;
    m_textColors[0][1] = -1;
    m_textColors[1][1] = -1;
    m_textColors[2][1] = 2;
    m_textColors[3][1] = 2;
    m_fonts[2] = 0;
    m_textColors[0][2] = -1;
    m_textColors[1][2] = -1;
    m_textColors[2][2] = 2;
    m_textColors[3][2] = 2;
    m_fonts[3] = 0;
    m_textColors[0][3] = -1;
    m_textColors[1][3] = -1;
    m_textColors[2][3] = 2;
    m_textColors[3][3] = 2;
}

AssetSurface::~AssetSurface()
{
    close();
}

void AssetSurface::close()
{
    if (m_surface)
        graphsys->delete_buffer_data(m_surface);
    if (!buffer_resizing && m_palette) {
        delete m_palette;
        m_palette = 0;
    }
    for (int i = 0; i < 40; i++) {
        if (m_linkParam[i]) {
            free(m_linkParam[i]);
            m_linkParam[i] = 0;
        }
        m_linkCommand[i] = -2;
    }
    m_itemHook = 0;
    m_drawHook = 0;
    m_lineCount = 0;
    m_pendingIndent = 0;
    m_hangingIndent = 0;
    m_layoutAux = 0;
    m_hotLink = -1;
    m_renderStyle = 0;
    m_wrapStyle = 0;
    m_style = 0;
    m_linkState = 0;
    m_formatOn = g_defaultFormatOn;
    m_dropIndex = -1;
    m_wrapDropState = 0;
    m_dropState = 0;
    m_renderDropState = 0;
    m_fonts[0] = g_defaultFont;
    m_textColors[0][0] = 0;
    m_textColors[1][0] = -1;
    m_textColors[2][0] = 2;
    m_textColors[3][0] = 2;
    m_fonts[1] = 0;
    m_textColors[0][1] = -1;
    m_textColors[1][1] = -1;
    m_textColors[2][1] = 2;
    m_textColors[3][1] = 2;
    m_fonts[2] = 0;
    m_textColors[0][2] = -1;
    m_textColors[1][2] = -1;
    m_textColors[2][2] = 2;
    m_textColors[3][2] = 2;
    m_fonts[3] = 0;
    m_textColors[0][3] = -1;
    m_textColors[1][3] = -1;
    m_textColors[2][3] = 2;
    m_textColors[3][3] = 2;
    m_dirty = 0;
    m_depth = 0;
}

int AssetSurface::Create(int w, int h, int bpp, int memType, int depth, int f)
{
    int rc;
    close();
    if (!memType) {
        if (!m_memType)
            m_memType = 1;
    } else
        m_memType = memType;
    m_surface = graphsys->get_buffer_data(this, m_memType);
    rc = m_surface->init(w, h, bpp, m_memType);
    if (rc) {
        close();
        return rc;
    }
    clear_links();
    resetDropDownSpots();
    SetFontsForStyles(g_defaultFont, 0, 0, 0);
    m_depth = depth;
    return 0;
}

int AssetSurface::resize(int cx, int cy, int bpp)
{
    int rc;
    buffer_resizing = 1;
    if (is_graphicwin())
        rc = is_graphicwin()->resize(cx, cy, 1, bpp);
    else
        rc = Create(cx, cy, bpp, 0, m_depth, 0);
    buffer_resizing = 0;
    return rc;
}

unsigned char *AssetSurface::LoadBMPFile(char *pszFile, BITMAPINFOHEADER **ppInfo)
{
    BITMAPFILEHEADER bfh;
    FILE *fp;
    unsigned int nSize;
    unsigned char *pData;
    fp = fopen(pszFile, "rb");
    if (fp == 0)
        return 0;
    if (fread(&bfh, sizeof(bfh), 1, fp) < 1) {
        fclose(fp);
        return 0;
    }
    if (bfh.bfType != 0x4d42) {
        fclose(fp);
        return 0;
    }
    nSize = bfh.bfOffBits - sizeof(BITMAPFILEHEADER);
    *ppInfo = (BITMAPINFOHEADER *)malloc(nSize);
    if (*ppInfo == 0) {
        fclose(fp);
        return 0;
    }
    if (fread(*ppInfo, 1, nSize, fp) < nSize) {
        free(*ppInfo);
        fclose(fp);
        return 0;
    }
    nSize = (*ppInfo)->biSizeImage;
    if (nSize == 0)
        nSize = abs((*ppInfo)->biHeight) * (*ppInfo)->biBitCount * (*ppInfo)->biWidth;
    pData = (unsigned char *)malloc(nSize);
    if (pData == 0) {
        free(*ppInfo);
        fclose(fp);
        return 0;
    }
    if (fread(pData, 1, nSize, fp) == 0) {
        free(*ppInfo);
        free(pData);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return pData;
}

int AssetSurface::ConvertBMP8ToSurface16(BITMAPINFOHEADER *info, unsigned char *pBits)
{
    unsigned short *pDest;
    int x, y, idx;
    unsigned char b, g, r;
    int v;
    if (info->biHeight > 0)
        FlipImageVertically(pBits, info);
    if (info->biWidth > GetSurfaceWidth() || abs(info->biHeight) > GetSurfaceHeight())
        Create(info->biWidth, abs(info->biHeight), 0, 1, 0, 0);
    if (m_surface == 0)
        pDest = 0;
    else
        pDest = m_surface->get_data_16();
    for (y = 0; y < abs(info->biHeight); y++) {
        for (x = 0; x < info->biWidth; x++) {
            idx = pBits[info->biWidth * y + x];
            b = ((BITMAPINFO *)info)->bmiColors[idx].rgbBlue;
            g = ((BITMAPINFO *)info)->bmiColors[idx].rgbGreen;
            r = ((BITMAPINFO *)info)->bmiColors[idx].rgbRed;
            if (graphsys->screen_mask() == 1) {
                v = r >> 3;
                v = (v << 6) | (g >> 2);
                v = (v << 5) | (b >> 3);
                v |= 0x80000000;
            } else {
                v = r >> 3;
                v = (v << 5) | (g >> 3);
                v = (v << 5) | (b >> 3);
                v |= 0x80000000;
            }
            pDest[GetSurfaceWidth() * y + x] = v;
        }
    }
    free(pBits);
    free(info);
    return 0;
}

#define IMAGESET_INFO ((BITMAPINFOHEADER *)pszFile)

int AssetSurface::LoadBMPToSurface16(char *pszFile)
{
    unsigned char *pRGB;
    unsigned short *pDest;
    int x, y, i;
    unsigned char b, g, r;
    int v;
    pRGB = LoadBMPFile(pszFile, (BITMAPINFOHEADER **)&pszFile);
    if (pRGB == 0)
        return 6;
    if (IMAGESET_INFO->biBitCount == 8)
        return ConvertBMP8ToSurface16(IMAGESET_INFO, pRGB);
    if (IMAGESET_INFO->biHeight > 0)
        FlipImageVertically(pRGB, IMAGESET_INFO);
    if (IMAGESET_INFO->biWidth > GetSurfaceWidth() || abs(IMAGESET_INFO->biHeight) > GetSurfaceHeight())
        Create(IMAGESET_INFO->biWidth, abs(IMAGESET_INFO->biHeight), 0, 1, 0, 0);
    if (m_surface == 0)
        pDest = 0;
    else
        pDest = m_surface->get_data_16();
    for (y = 0; y < abs(IMAGESET_INFO->biHeight); y++) {
        for (x = 0; x < IMAGESET_INFO->biWidth; x++) {
            i = (IMAGESET_INFO->biWidth * y + x) * 3;
            b = pRGB[i];
            g = pRGB[i + 1];
            r = pRGB[i + 2];
            if (graphsys->screen_mask() == 1) {
                v = r >> 3;
                v = (v << 6) | (g >> 2);
                v = (v << 5) | (b >> 3);
                v |= 0x80000000;
            } else {
                v = r >> 3;
                v = (v << 5) | (g >> 3);
                v = (v << 5) | (b >> 3);
                v |= 0x80000000;
            }
            pDest[GetSurfaceWidth() * y + x] = v;
        }
    }
    free(pRGB);
    free(pszFile);
    return 0;
}

void AssetSurface::FlipImageVertically(unsigned char *pData, BITMAPINFOHEADER *pInfo)
{
    unsigned char *pTmp;
    int bpp;
    int i, j, k;
    int rowOffset, mirrorOffset;
    bpp = 1;
    if (pInfo->biBitCount == 24)
        bpp = 3;
    pTmp = new unsigned char[bpp * pInfo->biWidth];
    if (pInfo->biHeight % 2 == 0) {
        i = abs(pInfo->biHeight) / 2;
        j = i - 1;
    } else {
        i = abs(pInfo->biHeight) / 2 + 1;
        j = i - 2;
    }
    for (; i < abs(pInfo->biHeight); i++, j--) {
        rowOffset = pInfo->biWidth * i * bpp;
        mirrorOffset = pInfo->biWidth * j * bpp;
        for (k = 0; k < bpp * pInfo->biWidth; k += bpp) {
            pTmp[k] = pData[rowOffset + k];
            pData[rowOffset + k] = pData[mirrorOffset + k];
            pData[mirrorOffset + k] = pTmp[k];
            if (bpp == 3) {
                pTmp[k + 1] = pData[rowOffset + k + 1];
                pTmp[k + 2] = pData[rowOffset + k + 2];
                pData[rowOffset + k + 1] = pData[mirrorOffset + k + 1];
                pData[rowOffset + k + 2] = pData[mirrorOffset + k + 2];
                pData[mirrorOffset + k + 1] = pTmp[k + 1];
                pData[mirrorOffset + k + 2] = pTmp[k + 2];
            }
        }
    }
    delete[] pTmp;
}

int AssetSurface::SaveSnapshot(char *file, Rect *area)
{
    BITMAPINFO bmi;
    BITMAPFILEHEADER bfh;
    FILE *fp;
    int nSize;
    unsigned short *pSrc;
    int yEnd, xEnd;
    int x, y, i;
    unsigned short w;
    int nw, nr;
    unsigned char *pBuf;
    DWORD nFileSize;

    if (m_surface == 0)
        pSrc = 0;
    else
        pSrc = m_surface->get_data_16();
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    if (area == 0) {
        bmi.bmiHeader.biWidth = GetSurfaceWidth();
        bmi.bmiHeader.biHeight = GetSurfaceHeight();
    } else {
        bmi.bmiHeader.biWidth = area->right - area->left;
        bmi.bmiHeader.biHeight = area->bottom - area->top;
    }
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = 0;
    fp = fopen(file, "wb");
    if (fp == 0)
        return 2;
    if (bmi.bmiHeader.biSizeImage == 0)
        nSize = ((bmi.bmiHeader.biWidth * bmi.bmiHeader.biBitCount + 7) / 8) * abs(bmi.bmiHeader.biHeight);
    nFileSize = nSize + sizeof(bfh) + sizeof(BITMAPINFOHEADER);
    pBuf = new unsigned char[nSize];
    if (area != 0) {
        x = area->left;
        xEnd = area->left + bmi.bmiHeader.biWidth;
        y = area->top;
        yEnd = y + abs(bmi.bmiHeader.biHeight);
    } else {
        x = 0;
        xEnd = bmi.bmiHeader.biWidth;
        y = 0;
        yEnd = abs(bmi.bmiHeader.biHeight);
    }
    i = 0;
    for (; y < yEnd; y++) {
        for (; x < xEnd; x++) {
            w = pSrc[GetSurfaceWidth() * y + x];
            pBuf[i++] = w << 3;
            nw = w;
            nr = w;
            pBuf[i++] =
                graphsys->screen_mask() == 1 ? (unsigned char)((nw >> 3) & 0xfc) : (unsigned char)((nw >> 2) & 0xf8);
            pBuf[i++] =
                graphsys->screen_mask() == 1 ? (unsigned char)((nr >> 8) & 0xf8) : (unsigned char)((nr >> 7) & 0xf8);
        }
        x = area ? area->left : 0;
    }
    FlipImageVertically(pBuf, &bmi.bmiHeader);
    bfh.bfSize = nFileSize;
    bfh.bfType = 0x4d42;
    bfh.bfReserved1 = 0;
    bfh.bfReserved2 = 0;
    bfh.bfOffBits = sizeof(bfh) + sizeof(BITMAPINFOHEADER);
    if (fwrite(&bfh, 1, sizeof(bfh), fp) < sizeof(bfh)) {
        fclose(fp);
        delete pBuf;
        return 2;
    }
    if (fwrite(&bmi.bmiHeader, 1, sizeof(BITMAPINFOHEADER), fp) < sizeof(BITMAPINFOHEADER)) {
        fclose(fp);
        delete pBuf;
        return 2;
    }
    if (fwrite(pBuf, 1, nSize, fp) < (unsigned)nSize) {
        fclose(fp);
        delete pBuf;
        return 2;
    }
    fclose(fp);
    delete pBuf;
    return 0;
}

static inline FLCPalette *NewPoolPalette()
{
    PaletteList::s_ourPalettes.m_created = 0;
    PaletteList::s_ourPalettes.add(0);
    return PaletteList::s_ourPalettes.m_created;
}

int AssetSurface::load_pcx(const char *file, FLCPalette *pal, int first, int count, int mode)
{
    Filemap map;
    char path[260];
    FLCPalette *use;
    const char *p;
    int rc;

    use = 0;
    if (file == 0)
        return 3;

    if (mode & 1) {
        if (pal != 0)
            return 3;
        if (m_palette == 0) {
            m_palette = new FLCPalette;
            if (m_palette == 0)
                return 4;
        }
        use = m_palette;
        use->init();
    } else if (mode & 2) {
        if (m_palette != 0) {
            delete m_palette;
            m_palette = 0;
        }
        use = NewPoolPalette();
        use->init();
        g_lastPalette = use;
    } else if ((mode & 3) == 0) {
        use = pal;
    }

    ClearString(path);
    AppendString(path, file);
    for (p = path + strlen(file); p != path; p--)
        if (*p == '.')
            break;
    if (p == path)
        AppendString(path, ".pcx");

    if (!map.open_read(path, 1))
        return 6;

    rc = LoadImageFile((char *)map.GetData(), map.GetSize(), use, first, count);
    if (rc == 0) {
        if (mode & 1)
            m_surface->set_current_palette(use->pal);
        else if (mode & 2)
            m_surface->set_current_palette(use->pal);
        else if (mode & 3)
            m_surface->set_current_palette(g_lastPalette->pal);
    }
    return rc;
}

int AssetSurface::set_clip(Rect *r)
{
    JGLBuffer *t = m_surface;
    if (t)
        return t->set_clip(r);
    return 0x18;
}

void AssetSurface::SetClipRect(int x, int y, int w, int h)
{
    Rect r;
    r.SetRect(x, y, w, h);
    set_clip(&r);
}

int AssetSurface::draw(AssetSurface *p, int x, int y)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    if (!p)
        return 0x10;
    return t->draw(p->m_surface, x, y);
}

int AssetSurface::draw_dest(AssetSurface *p, int x, int y, unsigned char *lut)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    if (!p)
        return 0x10;
    return t->draw_dest(p->m_surface, x, y, lut);
}

int AssetSurface::draw_multi_table_dest(AssetSurface *p, int x, int y, unsigned char *lut)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    if (!p)
        return 0x10;
    return t->draw_multi_table_dest(p->m_surface, x, y, lut);
}

int AssetSurface::BlitVtable40(AssetSurface *p, int x, int y, int blitArg)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    if (!p)
        return 0x10;
    return t->SURF_Stub_NotImplemented_21(p->m_surface, x, y, blitArg);
}

void AssetSurface::copy(AssetSurface *dst, int sx, int sy, int dx, int dy, int cx, int cy)
{
    BlitSurfaceRegion(dst, sx, sy, cx, cy, dx, dy, cx, cy);
}

int AssetSurface::BlitRectToRect(AssetSurface *dst, int x, int y, int w, int h)
{
    if (dst == 0)
        return 0x10;
    JGLBuffer *from = m_surface;
    if (from != 0) {
        JGLBuffer *to = dst->m_surface;
        if (to != 0) {
            Rect r;
            r.SetRect(x, y, w, h);
            return from->copy(to, &r, &r);
        }
    }
    return 7;
}

int AssetSurface::BlitRectToRect(AssetSurface *dst, Rect *r)
{
    if (dst == 0)
        return 0x10;
    JGLBuffer *from = m_surface;
    if (from != 0) {
        JGLBuffer *to = dst->m_surface;
        if (to != 0)
            return from->copy(to, r, r);
    }
    return 7;
}

int AssetSurface::BlitSurfaceRegion(AssetSurface *dst, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh)
{
    if (dst == 0)
        return 0x10;
    JGLBuffer *from = m_surface;
    if (from != 0) {
        JGLBuffer *to = dst->m_surface;
        if (to != 0) {
            Rect d, s;
            s.left = sx;
            s.top = sy;
            s.right = sx + sw;
            s.bottom = sy + sh;
            d.left = dx;
            d.top = dy;
            d.right = dx + dw;
            d.bottom = dy + dh;
            return from->copy(to, &s, &d);
        }
    }
    return 7;
}

int AssetSurface::BlitSurfaceRegion(AssetSurface *dst, Rect *src, Rect *dstRect)
{
    if (dst == 0)
        return 0x10;
    JGLBuffer *from = m_surface;
    if (from != 0) {
        JGLBuffer *to = dst->m_surface;
        if (to != 0)
            return from->copy(to, src, dstRect);
    }
    return 7;
}

int AssetSurface::map_colors(int left, int top, int right, int bottom, const unsigned char *table)
{
    JGLBuffer *t = m_surface;
    Rect r;
    if (t == 0)
        return 7;
    r.left = left;
    r.top = top;
    r.right = right;
    r.bottom = top;
    return t->map_colors(&r, table);
}

int AssetSurface::poly(Vert *pts, int count, int color)
{
    return ::FillPolygon(this, pts, count, color);
}

int AssetSurface::tile_mask(AssetSurface *src, int sx, int sy, int x, int y, int w, int h, int mode)
{
    if (m_surface == 0)
        return 7;
    Rect save;
    CopySurfaceClip(&save);
    Rect r;
    r.left = x;
    r.top = y;
    int right = x + w;
    r.right = right;
    int bottom = y + h;
    r.bottom = bottom;
    if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)&save) == 0)
        return 0;
    set_clip(&r);
    int tw = src->GetSurfaceWidth();
    int th = src->GetSurfaceHeight();
    if (tw == 0 || th == 0)
        return 0;
    int nx = w / tw;
    if (nx * tw < w)
        nx++;
    int ny = h / th;
    if (ny * th < h)
        ny++;
    int cx = x - sx % tw;
    int rowX = cx;
    y -= sy % th;
    if (nx * tw + rowX < right)
        nx++;
    if (ny * th + y < bottom)
        ny++;
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            src->m_surface->copy_mask(m_surface, 0, 0, cx, y, tw, th, mode);
            cx += tw;
        }
        cx = rowX;
        y += th;
    }
    set_clip(&save);
    return 0;
}

int AssetSurface::tile(AssetSurface *src, int sx, int sy, int x, int y, int w, int h)
{
    if (m_surface == 0)
        return 7;
    Rect save;
    CopySurfaceClip(&save);
    Rect r;
    r.left = x;
    r.top = y;
    int right = x + w;
    r.right = right;
    int bottom = y + h;
    r.bottom = bottom;
    if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)&save) == 0)
        return 0;
    set_clip(&r);
    int tw = src->GetSurfaceWidth();
    int th = src->GetSurfaceHeight();
    if (tw == 0 || th == 0)
        return 0;
    int nx = w / tw;
    if (nx * tw < w)
        nx++;
    int ny = h / th;
    if (ny * th < h)
        ny++;
    int cx = x - sx % tw;
    int rowX = cx;
    y -= sy % th;
    if (nx * tw + rowX < right)
        nx++;
    if (ny * th + y < bottom)
        ny++;
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            src->copy(this, 0, 0, cx, y, tw, th);
            cx += tw;
        }
        cx = rowX;
        y += th;
    }
    set_clip(&save);
    return 0;
}

int AssetSurface::TileSprite(SpriteObject *src, int sx, int sy, int x, int y, int w, int h)
{
    if (m_surface == 0)
        return 7;
    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);
    Rect save;
    CopySurfaceClip(&save);
    Rect r;
    r.left = x;
    r.top = y;
    int right = x + w;
    r.right = right;
    int bottom = y + h;
    r.bottom = bottom;
    if (IntersectRect((LPRECT)&r, (const RECT *)&r, (const RECT *)&save) == 0)
        return 0;
    set_clip(&r);
    int tw = src->m_width;
    int th = src->m_height;
    if (tw == 0 || th == 0)
        return 0;
    int nx = w / tw;
    if (nx * tw < w)
        nx++;
    int ny = h / th;
    if (ny * th < h)
        ny++;
    int cx = x - sx % tw;
    int rowX = cx;
    int cy = y - sy % th;
    if (nx * tw + rowX < right)
        nx++;
    if (ny * th + cy < bottom)
        ny++;
    for (int j = 0; j < ny; j++) {
        for (int i = 0; i < nx; i++) {
            src->BlitScaled(this, cx, cy, 0);
            cx += tw;
        }
        cx = rowX;
        cy += th;
    }
    set_clip(&save);
    return 0;
}

int AssetSurface::SetFontsForStyles(Font *font, Font *braceFont, Font *bracketFont, Font *linkFont)
{
    if (font == 0)
        return 3;
    if (font->m_owner)
        m_fonts[0] = font;
    m_fonts[1] = braceFont;
    m_fonts[2] = bracketFont;
    m_fonts[3] = linkFont;
    return 0;
}

void AssetSurface::set_text_color(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColors[0][0] = color;
    m_textColors[1][0] = shadowColor;
    m_textColors[2][0] = shadowDx;
    m_textColors[3][0] = shadowDy;
}

void AssetSurface::set_text_color2(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColors[0][1] = color;
    m_textColors[1][1] = shadowColor;
    m_textColors[2][1] = shadowDx;
    m_textColors[3][1] = shadowDy;
}

void AssetSurface::set_text_color3(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColors[0][2] = color;
    m_textColors[1][2] = shadowColor;
    m_textColors[2][2] = shadowDx;
    m_textColors[3][2] = shadowDy;
}

void AssetSurface::set_text_color_hyper(int color, int shadowColor, int shadowDx, int shadowDy)
{
    m_textColors[0][3] = color;
    m_textColors[1][3] = shadowColor;
    m_textColors[2][3] = shadowDx;
    m_textColors[3][3] = shadowDy;
}

static inline void SkipLineAndBeginLayout(AssetSurface *s, int x, int &y, int maxWidth)
{
    y += s->text_line_height();
    s->wrap_flying(0, x, y, maxWidth);
}

static inline void NextItem(AssetSurface *s, StringList *lines)
{
    lines->next_entry();
}

int AssetSurface::write_strings(StringList *lines, int x, int y, int maxWidth, int flags)
{
    if (lines == 0)
        return y;
    if ((flags & 2) == 0)
        lines->SetPos(0);
    int style = lines->AttrAtCursor();
    if (m_itemHook)
        m_itemHook(lines->KeyAtCursor(), style);
    if (style == 3) {
        SkipLineAndBeginLayout(this, x, y, maxWidth);
    } else if (style == 2) {
        y = wrap_cent(lines->TextAtCursor(), x, y, maxWidth);
        wrap_flying(0, x, y, maxWidth);
    } else {
        wrap_flying(lines->TextAtCursor(), x, y, maxWidth);
    }
    while (lines->m_curIndex < lines->GetCount() - 1) {
        if (y > GetSurfaceHeight())
            break;
        NextItem(this, lines);
        style = lines->AttrAtCursor();
        if (lines->AttrAtCursor()) {
            int bottom = wrap_flying_flush();
            if (m_itemHook)
                m_itemHook(lines->KeyAtCursor(), style);
            if (bottom >= y)
                y = bottom;
            if (style == 3) {
                SkipLineAndBeginLayout(this, x, y, maxWidth);
            } else if (style == 2) {
                y = wrap_cent(lines->TextAtCursor(), x, y, maxWidth);
                wrap_flying(0, x, y, maxWidth);
            } else {
                wrap_flying(lines->TextAtCursor(), x, y, maxWidth);
            }
        } else {
            if (m_itemHook)
                m_itemHook(lines->KeyAtCursor(), style);
            TextLayout_AppendAndWrap(lines->TextAtCursor());
        }
    }
    int bottom = wrap_flying_flush();
    if (bottom >= y)
        y = bottom;
    return y;
}

void AssetSurface::DrawTextLeftAlignedWithFont(Font *font, const char *s, int x, int y, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    write_l(s, x, y, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextVerticallyCenteredWithFont(Font *font, const char *s, Rect *rc, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderTextVerticallyCentered(s, rc, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextCenteredInWidthWithFont(Font *font, const char *s, int x, int y, int w, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderTextCenteredInWidth(s, x, y, w, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextCenteredInRectWithFont(Font *font, const char *s, Rect *rc, unsigned int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderTextCenteredInRect(s, rc, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextCenteredInBoundsWithFont(Font *font, const char *s, int x, int y, int w, int h, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    write_cent_l(s, x, y, w, h, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextCenteredWithFont(Font *font, const char *s, int x, int y, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderTextHorizontallyCentered(s, x, y, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextRightAlignedInWidthWithFont(Font *font, const char *s, int x, int y, int w, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderTextRightAlignedInWidth(s, x, y, w, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextRightAlignedWithFont(Font *font, const char *s, int x, int y, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    write_right_l(s, x, y, n);
    m_fontSet = saved;
}

void AssetSurface::DrawTextRightAlignedInRectVCenteredWithFont(Font *font, const char *s, Rect *rc, int n)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderTextRightAlignedInRectVCentered(s, rc, n);
    m_fontSet = saved;
}

void AssetSurface::DrawMultilineTextInRectWithFont(Font *font, char *text, Rect *rc)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    RenderMultilineTextInRect(text, rc, 0, 0);
    m_fontSet = saved;
}

void AssetSurface::DrawMultilineTextCenteredWithFont(Font *font, char *text, int x, int y, int w)
{
    FontSet saved = m_fontSet;

    if (font != 0 && font->m_owner != 0) {
        m_fonts[0] = font;
        m_fonts[1] = 0;
        m_fonts[2] = 0;
    }
    wrap_cent(text, x, y, w);
    m_fontSet = saved;
}

int AssetSurface::write_raw_l(const char *s, int x, int y, int n)
{
    int fromTextPalette = 0;
    if (m_drawHook) {
        int (*hook)(const char *, int, int, int) = m_drawHook;
        int r = hook(s, x, y, n);
        if (r)
            return r;
    }
    if (n == 0 || m_surface == 0)
        return x;

    const char *orig = s;
    char *copy = (char *)AllocateBytes(n);
    if (copy) {
        memcpy(copy, s, n);
        for (int i = 0; i < n; i++)
            if (copy[i] == '_')
                copy[i] = ' ';
        s = copy;
    }

    JGLPalette *pal = 0;
    if (GetPixelFormat()->bpp == 16) {
        if (g_textPalette) {
            fromTextPalette = 1;
            pal = g_textPalette->pal;
        } else if (m_surface->get_current_palette())
            pal = m_surface->get_current_palette();
        else
            pal = g_activePalettePtr->pal;
    }

    int font, col;
    if (m_formatOn == 0) {
        font = 0;
        col = 0;
        y += m_fonts[0]->m_capHeight;
    } else {
        int style = m_renderStyle;
        font = m_fonts[style] ? style : 0;
        col = m_textColors[0][style] != -1 ? style : 0;
        int h = 0;
        if (m_fonts[0] && m_fonts[0]->m_capHeight > h)
            h = m_fonts[0]->m_capHeight;
        if (m_fonts[1] && m_fonts[1]->m_capHeight > h)
            h = m_fonts[1]->m_capHeight;
        if (m_fonts[2] && m_fonts[2]->m_capHeight > h)
            h = m_fonts[2]->m_capHeight;
        y += h;
    }
    m_surface->select_font(m_fonts[font]);

    int c = m_textColors[1][col];
    if (c != -1 && !(fromTextPalette && c == 0xff)) {
        if (c & 0x80000000) {
            unsigned char g, r;
            if (graphsys->screen_mask() == 1)
                g = (c >> 3) & 0xfc;
            else
                g = (c >> 2) & 0xf8;
            if (graphsys->screen_mask() == 1)
                r = (c >> 8) & 0xf8;
            else
                r = (c >> 7) & 0xf8;
            m_surface->set_text_color(r, g, (unsigned char)(c << 3));
        } else if (pal)
            m_surface->set_text_color(pal->get_entry(c)->peRed, pal->get_entry(c)->peGreen, pal->get_entry(c)->peBlue);
        else if (m_depth & 4)
            m_surface->set_text_color(pal->get_entry(c)->peRed, pal->get_entry(c)->peGreen, pal->get_entry(c)->peBlue);
        else
            m_surface->SURF_SetTextColor16(c);
        m_surface->write_text(x + m_textColors[2][col], y + m_textColors[3][col], s, n);
    }

    c = m_textColors[0][col];
    if (c & 0x80000000) {
        unsigned char g, r;
        if (graphsys->screen_mask() == 1)
            g = (c >> 3) & 0xfc;
        else
            g = (c >> 2) & 0xf8;
        if (graphsys->screen_mask() == 1)
            r = (c >> 8) & 0xf8;
        else
            r = (c >> 7) & 0xf8;
        m_surface->set_text_color(r, g, (unsigned char)(c << 3));
    } else if (pal)
        m_surface->set_text_color(pal->get_entry(c)->peRed, pal->get_entry(c)->peGreen, pal->get_entry(c)->peBlue);
    else if (!(m_depth & 4))
        m_surface->SURF_SetTextColor16(c);

    char segment[260];
    int penX = x;
    int start = 0;
    int len;
    for (int i = 0; i <= n; i++) {
        if (i == n) {
            len = i - start;
            for (int k = start; k < i; k++)
                segment[k - start] = s[k];
            start = i + 1;
            m_surface->write_text(penX, y, segment, len);
            penX += m_fonts[font]->MeasureTextWidth(segment, len);
        } else if (s[i] == '\t') {
            len = i - start;
            for (int k = start; k < i; k++)
                segment[k - start] = s[k];
            start = i + 1;
            m_surface->write_text(penX, y, segment, len);
            penX += m_fonts[font]->MeasureTextWidth(segment, len);
            penX += 30 - penX % 30;
        }
    }
    m_surface->deselect_font();
    if (copy)
        FreeIfNotNull(copy);
    return x + m_fonts[font]->MeasureTextWidth(orig, n);
}

char *get_special(char *s, int *pLen)
{
    __asm {
        mov     eax, s
        mov     ebx, pLen
        mov     edx, [ebx]
        and     edx, edx
        jz      done
    top:
        mov     cl, byte ptr [eax]
        cmp     cl, 0x7b
        je      hit
        cmp     cl, 0x7d
        je      hit
        cmp     cl, 0x5b
        je      hit
        cmp     cl, 0x5d
        je      hit
        cmp     cl, 0x24
        je      hit
        inc     eax
        dec     edx
        jnz     top
    hit:
        mov     [ebx], edx
        mov     s, eax
        jmp     done
    done:
    }
    return s;
}

char *get_special_link(char *s, int *pLen)
{
    __asm {
        mov     eax, s
        mov     ebx, pLen
        mov     edx, [ebx]
        and     edx, edx
        jz      done
    top:
        mov     cl, byte ptr [eax]
        cmp     cl, 0x7b
        je      hit
        cmp     cl, 0x7d
        je      hit
        cmp     cl, 0x5b
        je      hit
        cmp     cl, 0x5d
        je      hit
        cmp     cl, 0x24
        je      hit
        cmp     cl, 0x3d
        je      hit
        inc     eax
        dec     edx
        jnz     top
    hit:
        mov     [ebx], edx
        mov     s, eax
        jmp     done
    done:
    }
    return s;
}

char *get_special_dropdown(char *s, int *pLen)
{
    __asm {
        mov     eax, s
        mov     ebx, pLen
        mov     edx, [ebx]
        and     edx, edx
        jz      done
    top:
        mov     cl, byte ptr [eax]
        cmp     cl, 0x7b
        je      hit
        cmp     cl, 0x7d
        je      hit
        cmp     cl, 0x5b
        je      hit
        cmp     cl, 0x5d
        je      hit
        cmp     cl, 0x24
        je      hit
        cmp     cl, 0x3d
        je      hit
        cmp     cl, 0x5e
        je      hit
        inc     eax
        dec     edx
        jnz     top
    hit:
        mov     [ebx], edx
        mov     s, eax
        jmp     done
    done:
    }
    return s;
}

int AssetSurface::wrap_height(char *text, int avail, char **pLines, int *pCount)
{
    int total;
    char **pOut;
    int count;
    int lineWidth;
    char *brk;
    int n;

    if (text == 0)
        return 0;
    m_lineCount = 0;
    g_lineWidthUsed = 0;
    m_pendingIndent = m_hangingIndent;
    total = 0;
    count = 0;
    pOut = pLines;
    do {
        if (pLines != 0) {
            count++;
            *pOut = text;
            pOut++;
        }
        n = strlen(text);
        lineWidth = avail;
        brk = find_line_break_l(text, &lineWidth, n);
        g_lineWidthUsed = 0;
        total += text_line_height();
        text = brk;
        m_lineCount++;
        if (m_pendingIndent != 0) {
            avail -= m_pendingIndent;
            m_pendingIndent = 0;
        }
    } while (brk != 0);
    if (pCount != 0)
        *pCount = count;
    return total;
}

char *AssetSurface::find_line_break_l(char *text, int *pWidth, int n)
{
    char *p;
    char *q;
    char *brk;
    int newStyle;

    if (pWidth != 0) {
        if (m_fonts[0] == 0)
            m_fonts[0] = g_defaultFont;
        if (m_wrapDropState == 2)
            m_wrapDropState = 1;
        p = text;
        if (m_formatOn == 0)
            return m_fonts[0]->find_line_break_l(text, pWidth, n);
        for (;;) {
            if (m_wrapDropState == 1)
                q = get_special_dropdown(p, &n);
            else if (m_wrapStyle == 3)
                q = get_special_link(p, &n);
            else
                q = get_special(p, &n);
            if (n != 0) {
                switch (*q) {
                case '=':
                    m_linkState = 2;
                    newStyle = m_wrapStyle;
                    break;
                case '$':
                    if (strncmp(q, "$LINK<", 6) == 0) {
                        m_linkState = 1;
                        newStyle = 3;
                    } else if (strncmp(q, "$DROPDOWN", 9) == 0) {
                        m_wrapDropState = 1;
                        newStyle = m_wrapStyle;
                    } else if (strncmp(q, "$DROPLINK", 9) == 0) {
                        m_wrapDropState = 1;
                        newStyle = 3;
                    } else {
                        newStyle = m_wrapStyle;
                    }
                    break;
                case '^':
                    m_wrapDropState = 2;
                    newStyle = 0;
                    break;
                case '{':
                    if (q[1] == '{') {
                        q++;
                        n--;
                        newStyle = m_wrapStyle;
                    } else {
                        newStyle = 1;
                    }
                    break;
                case '}':
                    if (q[1] == '}') {
                        q++;
                        n--;
                        newStyle = m_wrapStyle;
                    } else {
                        newStyle = 0;
                    }
                    break;
                case '[':
                    if (q[1] == '[') {
                        q++;
                        n--;
                        newStyle = m_wrapStyle;
                    } else {
                        newStyle = 2;
                    }
                    break;
                case ']':
                    if (q[1] == ']') {
                        q++;
                        n--;
                        newStyle = m_wrapStyle;
                    } else {
                        newStyle = 0;
                    }
                    break;
                default: newStyle = m_wrapStyle; break;
                }
                if (m_fonts[m_wrapStyle] != 0) {
                    brk = m_fonts[m_wrapStyle]->find_line_break_l(p, pWidth, q - p);
                    if (brk != 0) {
                        m_linkState = 0;
                        if (brk[-1] == ' ')
                            return brk;
                        if (brk == p)
                            return brk;
                        return memrchr(text, brk, ' ');
                    }
                } else {
                    brk = m_fonts[0]->find_line_break_l(p, pWidth, q - p);
                    if (brk != 0) {
                        m_linkState = 0;
                        if (brk[-1] == ' ')
                            return brk;
                        if (brk == p)
                            return brk;
                        return memrchr(text, brk, ' ');
                    }
                }
                if (m_linkState == 1) {
                    q += 5;
                    n -= 5;
                } else if (m_wrapDropState == 1) {
                    q += 9;
                    n -= 9;
                } else if (m_wrapDropState == 2) {
                    m_wrapDropState = 0;
                }
                p = q + 1;
                if (m_linkState == 2) {
                    char *gt = strchr(p, '>');
                    if (gt != 0) {
                        n -= (gt - p) + 1;
                        p = gt + 1;
                    }
                    m_linkState = 0;
                    newStyle = 0;
                }
                m_wrapStyle = newStyle;
                n--;
            } else {
                if (m_fonts[m_wrapStyle] != 0) {
                    brk = m_fonts[m_wrapStyle]->find_line_break_l(p, pWidth, q - p);
                    if (brk == 0)
                        break;
                    m_linkState = 0;
                    if (brk[-1] == ' ')
                        return brk;
                    if (brk == p)
                        return brk;
                    return memrchr(text, brk, ' ');
                } else {
                    brk = m_fonts[0]->find_line_break_l(p, pWidth, q - p);
                    if (brk != 0) {
                        m_linkState = 0;
                        if (brk[-1] == ' ')
                            return brk;
                        if (brk == p)
                            return brk;
                        return memrchr(text, brk, ' ');
                    }
                    break;
                }
            }
        }
        m_linkState = 0;
        m_wrapDropState = 0;
    }
    return 0;
}

int AssetSurface::text_width(const char *s)
{
    if (!s)
        return 0;
    return CalculateFormattedTextWidth(s, strlen(s));
}

int AssetSurface::CalculateFormattedTextWidth(const char *s, unsigned int len)
{
    char *p;
    char *q;
    int newStyle;
    int width;

    if (m_fonts[0] == 0)
        m_fonts[0] = g_defaultFont;
    p = (char *)s;
    width = 0;
    if (m_formatOn == 0)
        return m_fonts[0]->MeasureTextWidth(s, len);
    for (;;) {
        if (m_dropState == 1)
            q = get_special_dropdown(p, (int *)&len);
        else if (m_style == 3)
            q = get_special_link(p, (int *)&len);
        else
            q = get_special(p, (int *)&len);
        if (len == 0)
            break;
        switch (*q) {
        case '=':
            m_linkState = 2;
            newStyle = m_style;
            break;
        case '$':
            if (strncmp(q, "$LINK<", 6) == 0) {
                m_linkState = 1;
                newStyle = 3;
            } else if (strncmp(q, "$DROPDOWN", 9) == 0) {
                m_dropState = 1;
                newStyle = m_style;
            } else if (strncmp(q, "$DROPLINK", 9) == 0) {
                m_dropState = 1;
                newStyle = 3;
            } else {
                newStyle = m_style;
            }
            break;
        case '^':
            m_dropState = 2;
            newStyle = 0;
            break;
        case '{':
            if (q[1] == '{') {
                q++;
                len--;
                newStyle = m_style;
            } else {
                newStyle = 1;
            }
            break;
        case '}':
            if (q[1] == '}') {
                q++;
                len--;
                newStyle = m_style;
            } else {
                newStyle = 0;
            }
            break;
        case '[':
            if (q[1] == '[') {
                q++;
                len--;
                newStyle = m_style;
            } else {
                newStyle = 2;
            }
            break;
        case ']':
            if (q[1] == ']') {
                q++;
                len--;
                newStyle = m_style;
            } else {
                newStyle = 0;
            }
            break;
        default: newStyle = m_style; break;
        }
        if (m_fonts[m_style] != 0)
            width += m_fonts[m_style]->MeasureTextWidth(p, q - p);
        else
            width += m_fonts[0]->MeasureTextWidth(p, q - p);
        if (m_linkState == 1) {
            q += 5;
            len -= 5;
        } else if (m_dropState == 1) {
            q += 9;
            len -= 9;
        } else if (m_dropState == 2) {
            m_dropState = 0;
            if (m_dropArrow)
                width += 25;
            else
                width += 30;
        }
        p = q + 1;
        if (m_linkState == 2) {
            char *gt = strchr(p, '>');
            if (gt != 0) {
                len -= (gt - p) + 1;
                p = gt + 1;
            }
            m_linkState = 0;
            newStyle = 0;
        }
        m_style = newStyle;
        len--;
    }
    if (m_fonts[m_style] != 0)
        width += m_fonts[m_style]->MeasureTextWidth(p, q - p);
    else
        width += m_fonts[0]->MeasureTextWidth(p, q - p);
    if (m_dropState == 2) {
        m_dropState = 0;
        width += 30;
    }
    m_linkState = 0;
    m_dropState = 0;
    return width;
}

int AssetSurface::text_height()
{
    if (m_fonts[0] == 0)
        m_fonts[0] = g_defaultFont;
    return m_fonts[0]->m_capHeight;
}

int AssetSurface::text_line_height()
{
    if (m_fonts[0] == 0)
        m_fonts[0] = g_defaultFont;
    if (m_fonts[0]->m_extraLeading >= 0)
        return m_fonts[0]->m_capHeight + m_fonts[0]->m_extraLeading;
    return m_fonts[0]->m_lineSpacing;
}

int AssetSurface::write_multi_font_raw_l(const char *s, int x, int y, int n)
{
    char *p;
    char *q;
    int newStyle;
    int runX;
    int cx;
    Font *f;

    p = (char *)s;
    cx = x;
    if (m_formatOn == 0)
        return write_raw_l(s, x, y, n);
    for (;;) {
        if (m_renderDropState == 1)
            q = get_special_dropdown(p, &n);
        else if (m_renderStyle == 3)
            q = get_special_link(p, &n);
        else
            q = get_special(p, &n);
        if (q == 0 || n == 0)
            break;
        switch (*q) {
        case '=':
            m_linkState = 2;
            newStyle = m_renderStyle;
            break;
        case '$':
            if (strncmp(q, "$LINK<", 6) == 0) {
                m_linkState = 1;
                newStyle = 3;
                if (++m_linkCount > 40)
                    MessageBoxA(0, "Maximum hypertext links exceeded!", "WARNING!", 0);
            } else if (strncmp(q, "$DROPDOWN", 9) == 0) {
                m_renderDropState = 1;
                newStyle = m_renderStyle;
            } else if (strncmp(q, "$DROPLINK", 9) == 0) {
                m_renderDropState = 1;
                newStyle = 3;
            } else {
                newStyle = m_renderStyle;
            }
            break;
        case '^':
            m_renderDropState = 2;
            newStyle = 0;
            break;
        case '{':
            if (q[1] == '{') {
                q++;
                n--;
                newStyle = m_renderStyle;
            } else {
                newStyle = 1;
            }
            break;
        case '}':
            if (q[1] == '}') {
                q++;
                n--;
                newStyle = m_renderStyle;
            } else {
                newStyle = 0;
            }
            break;
        case '[':
            if (q[1] == '[') {
                q++;
                n--;
                newStyle = m_renderStyle;
            } else {
                newStyle = 2;
            }
            break;
        case ']':
            if (q[1] == ']') {
                q++;
                n--;
                newStyle = m_renderStyle;
            } else {
                newStyle = 0;
            }
            break;
        default: newStyle = m_renderStyle; break;
        }
        runX = cx;
        if (m_renderDropState == 2) {
            if (g_parse_cb_dropdowns[m_dropIndex].list != 0 && is_graphicwin() != 0) {
                GraphicWin *owner = is_graphicwin();
                ComboBox *box = g_parse_cb_dropdowns[m_dropIndex].list;
                int w = text_width(box->getCurrentString()) + 30;
                if (w < 200)
                    w = 200;
                box->init(cx, y, w, 0x138, is_graphicwin(), 7);
                box->m_frame.SetFontsForStyles(m_fonts[m_renderStyle], 0, 0, 0);
                g_parse_cb_dropdowns[m_dropIndex].list->show();
                cx += box->width();
            }
        } else {
            cx = write_raw_l(p, cx, y, q - p);
        }
        if (m_linkState == 1) {
            q += 5;
            n -= 5;
        } else if (m_renderDropState == 1) {
            q += 9;
            n -= 9;
            m_dropIndex = *q - '0';
        }
        p = q + 1;
        if (m_linkState == 2) {
            char *gt = strchr(p, '>');
            if (gt != 0) {
                if (m_linkCount < 40) {
                    if (*p >= '0' && *p <= '9') {
                        int cmd;
                        *gt = 0;
                        cmd = atoi(p);
                        *gt = '>';
                        m_linkCommand[m_linkCount - 1] = cmd;
                    } else {
                        int len = gt - p;
                        m_linkCommand[m_linkCount - 1] = -1;
                        m_linkParam[m_linkCount - 1] = (char *)AllocateBytes(len + 1);
                        if (m_linkParam[m_linkCount - 1] != 0) {
                            memcpy(m_linkParam[m_linkCount - 1], p, len);
                            m_linkParam[m_linkCount - 1][len] = 0;
                        }
                    }
                }
                n -= (gt - p) + 1;
                p = gt + 1;
            }
            newStyle = 0;
            m_linkState = 0;
        }
        if (m_renderDropState != 0 && cx - runX > 0) {
            f = m_fonts[m_renderStyle];
            if (f == 0)
                f = m_fonts[0];
            if (f != 0)
                addDropDownSpot(m_dropIndex, 1, runX, y, cx - runX,
                    f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing);
            if (m_renderDropState == 2) {
                m_renderDropState = 0;
                if (f != 0 && m_dropArrow != 0 && g_parse_cb_dropdowns[m_dropIndex].list == 0) {
                    m_dropArrow->BlitScaled(this, cx, y, 0);
                    runX = cx;
                    cx += m_dropArrow->m_width;
                    addDropDownSpot(m_dropIndex, 0, runX, y, cx - runX,
                        f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing);
                }
            }
        } else if (m_renderStyle == 3 && m_linkCount < 40) {
            f = 0;
            if (m_fonts[3] != 0)
                f = m_fonts[3];
            else if (m_fonts[0] != 0)
                f = m_fonts[0];
            if (f != 0)
                m_links.add(m_linkCount - 1, 0, runX, y, cx - runX,
                    f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing, 0);
        }
        m_renderStyle = newStyle;
        n--;
    }
    runX = cx;
    cx = write_raw_l(p, cx, y, q - p);
    if (m_renderDropState != 0 && cx - runX > 0) {
        f = m_fonts[m_renderStyle];
        if (f == 0)
            f = m_fonts[0];
        if (f != 0)
            addDropDownSpot(m_dropIndex, 1, runX, y, cx - runX,
                f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing);
        if (m_renderDropState == 2) {
            m_renderDropState = 0;
            if (f != 0 && m_dropArrow != 0 && g_parse_cb_dropdowns[m_dropIndex].list == 0) {
                m_dropArrow->BlitScaled(this, cx, y, 0);
                runX = cx;
                cx += m_dropArrow->m_width;
                addDropDownSpot(m_dropIndex, 0, runX, y, cx - runX,
                    f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing);
            }
        }
    } else if (m_renderStyle == 3 && m_linkCount < 40) {
        f = 0;
        if (m_fonts[3] != 0)
            f = m_fonts[3];
        else if (m_fonts[0] != 0)
            f = m_fonts[0];
        if (f != 0)
            m_links.add(m_linkCount - 1, 0, runX, y, cx - runX,
                f->m_extraLeading >= 0 ? f->m_capHeight + f->m_extraLeading : f->m_lineSpacing, 0);
    }
    return cx;
}

int AssetSurface::write_l(const char *s, int x, int y, int n)
{
    if (s == 0)
        return x;
    if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
        return 3;
    if (CLIPLEN(s, n) >= 0) {
        n = CLIPLEN(s, n);
        if (n == 0)
            return x;
        return write_multi_font_raw_l(s, x, y, n);
    }
    return x;
}

int AssetSurface::RenderTextVerticallyCentered(const char *s, Rect *rc, int n)
{
    Rect r;
    int y;

    if (s == 0 || rc == 0 || m_fonts[0] == 0 || m_fonts[0]->m_owner == 0 || CLIPLEN(s, n) < 0
        || (n = CLIPLEN(s, n)) == 0)
        return 0;
    r = *rc;
    y = r.top + (r.bottom - r.top - text_height()) / 2;
    return write_multi_font_raw_l(s, r.left, y, n);
}

int AssetSurface::RenderTextCenteredInWidth(const char *s, int x, int y, int w, int n)
{
    if (s == 0)
        return x;
    if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
        return 3;
    if (((int)strlen(s) < n ? (int)strlen(s) : n) >= 0) {
        n = ((int)strlen(s) < n ? (int)strlen(s) : n);
        if (n == 0)
            return x;
        x += (w - CalculateFormattedTextWidth(s, n)) / 2;
        return write_multi_font_raw_l(s, x, y, n);
    }
    return x;
}

int AssetSurface::write_cent_l(const char *s, int x, int y, int w, int h, int n)
{
    Rect r;
    r.left = x;
    r.top = y;
    r.right = x + w;
    r.bottom = y + h;
    return RenderTextCenteredInRect(s, &r, n);
}

int AssetSurface::RenderTextCenteredInRect(const char *s, Rect *rc, unsigned int n)
{
    Rect r;
    int x, y;
    if (s != 0 && rc != 0) {
        r = *rc;
        if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
            return 3;
        if (((int)strlen(s) < (int)n ? (int)strlen(s) : (int)n) >= 0) {
            n = ((int)strlen(s) < (int)n ? (int)strlen(s) : (int)n);
            if (n != 0) {
                x = r.left + (RectWidth(&r) - text_width(s)) / 2;
                y = r.top + (RectHeight(&r) - text_height()) / 2;
                return write_multi_font_raw_l(s, x, y, n);
            }
        }
    }
    return 0;
}

int AssetSurface::RenderTextHorizontallyCentered(const char *s, int x, int y, int n)
{
    if (s == 0)
        return x;
    if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
        return 3;
    if (((int)strlen(s) < n ? (int)strlen(s) : n) >= 0) {
        n = ((int)strlen(s) < n ? (int)strlen(s) : n);
        if (n == 0)
            return x;
        x -= CalculateFormattedTextWidth(s, n) >> 1;
        return write_multi_font_raw_l(s, x, y, n);
    }
    return x;
}

int AssetSurface::RenderTextRightAlignedInWidth(const char *s, int x, int y, int w, int n)
{
    if (s == 0)
        return x;
    if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
        return 3;
    if (((int)strlen(s) < n ? (int)strlen(s) : n) >= 0) {
        n = ((int)strlen(s) < n ? (int)strlen(s) : n);
        if (n == 0)
            return x;
        x += w - CalculateFormattedTextWidth(s, n);
        return write_multi_font_raw_l(s, x, y, n);
    }
    return x;
}

int AssetSurface::write_right_l(const char *s, int x, int y, int n)
{
    if (s == 0)
        return x;
    if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
        return 3;
    if (((int)strlen(s) < n ? (int)strlen(s) : n) >= 0) {
        n = ((int)strlen(s) < n ? (int)strlen(s) : n);
        if (n == 0)
            return x;
        x -= CalculateFormattedTextWidth(s, n);
        return write_multi_font_raw_l(s, x, y, n);
    }
    return x;
}

int AssetSurface::RenderTextRightAlignedInRectVCentered(const char *s, Rect *rc, int n)
{
    int x, y;
    if (s != 0 && rc != 0) {
        if (m_fonts[0] == 0 || m_fonts[0]->m_owner == 0)
            return 3;
        if (((int)strlen(s) < n ? (int)strlen(s) : n) >= 0) {
            n = ((int)strlen(s) < n ? (int)strlen(s) : n);
            if (n != 0) {
                x = rc->right - CalculateFormattedTextWidth(s, n);
                y = rc->top + (rc->bottom - rc->top - text_height()) / 2;
                return write_multi_font_raw_l(s, x, y, n);
            }
        }
    }
    return 0;
}

int AssetSurface::RenderMultilineTextInRect(char *text, Rect *rc, char **pLines, int *pCount)
{
    if (text != 0 && rc != 0) {
        int avail;
        int count;
        char **pOut;
        int lineWidth;
        int x;
        int y;
        char *brk;
        int lastX;

        g_lineWidthUsed = 0;
        m_lineCount = 0;
        m_pendingIndent = m_hangingIndent;
        avail = RectWidth(rc);
        x = rc->left;
        y = rc->top;
        count = 0;
        pOut = pLines;

        do {
            if (pLines != 0) {
                count++;
                *pOut = text;
                pOut++;
            }
            lineWidth = avail;
            if (text == 0) {
                lastX = x;
                brk = 0;
            } else {
                g_lineWidthUsed = 0;
                brk = find_line_break_l(text, &lineWidth, strlen(text));
                if (brk != 0) {
                    g_layoutLineEnd = write_l(text, x, y, brk - text);
                    goto advanced;
                }
                lastX = write_l(text, x, y, strlen(text));
            }
            g_layoutLineEnd = lastX;
advanced:
            y += text_line_height();
            text = brk;
            m_lineCount++;
            if (m_pendingIndent != 0) {
                avail -= m_pendingIndent;
                x += m_pendingIndent;
                m_pendingIndent = 0;
            }
        } while (brk != 0);

        if (pCount != 0)
            *pCount = count;
        return y;
    }
    return rc->top;
}

int AssetSurface::wrap(char *text, int x, int y, int avail)
{
    if (text != 0 && avail != 0) {
        int lineWidth;
        char *brk;
        int lastX;

        g_lineWidthUsed = 0;
        m_lineCount = 0;
        m_pendingIndent = m_hangingIndent;
        do {
            lineWidth = avail;
            if (text == 0) {
                lastX = x;
                brk = 0;
            } else {
                g_lineWidthUsed = 0;
                brk = find_line_break_l(text, &lineWidth, strlen(text));
                if (brk != 0) {
                    g_layoutLineEnd = write_l(text, x, y, brk - text);
                    goto advanced;
                }
                lastX = write_l(text, x, y, strlen(text));
            }
            g_layoutLineEnd = lastX;
advanced:
            y += text_line_height();
            text = brk;
            m_lineCount++;
            if (m_pendingIndent != 0) {
                avail -= m_pendingIndent;
                x += m_pendingIndent;
                m_pendingIndent = 0;
            }
        } while (brk != 0);
    }
    return y;
}

int AssetSurface::wrap_cent(char *text, int x, int y, int w)
{
    int lineWidth;
    char *brk;

    if (text == 0)
        return y;
    g_lineWidthUsed = 0;
    m_lineCount = 0;
    do {
        lineWidth = w;
        if (text == 0) {
            brk = 0;
        } else {
            g_lineWidthUsed = 0;
            brk = find_line_break_l(text, &lineWidth, strlen(text));
            if (brk != 0)
                RenderTextCenteredInWidth(text, x, y, w, brk - text);
            else
                RenderTextCenteredInWidth(text, x, y, w, strlen(text));
        }
        m_lineCount++;
        y += text_line_height();
        text = brk;
    } while (brk != 0);
    return y;
}

int AssetSurface::RenderMultilineTextCenteredInRect(char *text, Rect *rc)
{
    if (text != 0 && rc != 0) {
        int lineWidth;
        char *brk;
        int x;
        int y;
        int w;

        g_lineWidthUsed = 0;
        m_lineCount = 0;
        x = rc->left;
        w = rc->right - rc->left;
        y = rc->top;
        do {
            lineWidth = w;
            if (text == 0) {
                brk = 0;
            } else {
                g_lineWidthUsed = 0;
                brk = find_line_break_l(text, &lineWidth, strlen(text));
                if (brk != 0)
                    RenderTextCenteredInWidth(text, x, y, w, brk - text);
                else
                    RenderTextCenteredInWidth(text, x, y, w, strlen(text));
            }
            m_lineCount++;
            y += text_line_height();
            text = brk;
        } while (brk != 0);
        return y;
    }
    return rc->top;
}

int AssetSurface::wrap_right(char *text, int x, int y, int avail)
{
    int lineWidth;
    char *brk;

    if (text == 0)
        return y;
    g_lineWidthUsed = 0;
    m_lineCount = 0;
    m_pendingIndent = m_hangingIndent;
    do {
        lineWidth = avail;
        if (text == 0) {
            brk = 0;
        } else {
            g_lineWidthUsed = 0;
            brk = find_line_break_l(text, &lineWidth, strlen(text));
            if (brk != 0)
                write_right_l(text, x, y, brk - text);
            else
                write_right_l(text, x, y, strlen(text));
        }
        m_lineCount++;
        y += text_line_height();
        text = brk;
        if (m_pendingIndent != 0) {
            avail -= m_pendingIndent;
            m_pendingIndent = 0;
        }
    } while (brk != 0);
    return y;
}

FLCPalette *g_lastPalette = 0;

int g_layoutX = 0;

int g_layoutY = 0;

int g_layoutWidth = 0;

int g_lineWidthUsed = 0;

static int s_layoutSpare = 0;

int buffer_resizing = 0;

int AssetSurface::TextLayout_BeginWithWidth(char *text, int width)
{
    m_lineCount = 0;
    m_pendingIndent = m_hangingIndent;
    m_linkState = 0;
    g_layoutWidth = width;
    g_layoutY = 0;
    g_lineWidthUsed = 0;
    return wrap_height_flying(text);
}

int AssetSurface::wrap_height_flying(char *text)
{
    int avail;
    int n;
    int width = g_layoutWidth;
    if (width == 0)
        return 0;
    if (text == 0)
        return g_layoutY;
    if (m_fonts[0] == 0) {
        m_fonts[0] = g_defaultFont;
        width = g_layoutWidth;
    }
    if (*text == 0)
        return g_layoutY;
    do {
        n = strlen(text);
        avail = width - g_lineWidthUsed;
        text = find_line_break_l(text, &avail, n);
        if (text != 0) {
            m_lineCount++;
            g_layoutY += text_line_height();
            g_lineWidthUsed = 0;
            if (m_pendingIndent != 0) {
                g_layoutWidth -= m_pendingIndent;
                m_pendingIndent = 0;
            }
            width = g_layoutWidth;
        }
    } while (text != 0);
    g_lineWidthUsed += text_width(" ");
    return g_layoutY;
}

int AssetSurface::wrap_height_flying_flush()
{
    if (g_lineWidthUsed != 0) {
        g_layoutY += text_line_height();
        m_lineCount++;
    }
    g_layoutWidth = 0;
    m_renderStyle = 0;
    m_style = 0;
    m_wrapStyle = 0;
    m_linkState = 0;
    return g_layoutY;
}

int AssetSurface::wrap_flying(char *text, int x, int y, int width)
{
    m_lineCount = 0;
    m_pendingIndent = m_hangingIndent;
    g_layoutX = x;
    g_lineWidthUsed = 0;
    g_layoutY = y;
    g_layoutWidth = width;
    m_linkState = 0;
    ClearString(g_layoutLine);
    return TextLayout_AppendAndWrap(text);
}

int AssetSurface::TextLayout_AppendAndWrap(char *text)
{
    int avail;
    char *brk;
    char *end;
    int width = g_layoutWidth;

    if (width == 0)
        return 0;
    if (text == 0)
        return g_layoutY;
    if (*text == 0)
        return g_layoutY;
    for (;;) {
        if (g_layoutLine[0] != 0) {
            AppendSpace(g_layoutLine);
            g_lineWidthUsed += text_width(" ");
            width = g_layoutWidth;
        }
        if (g_lineWidthUsed < width) {
            int len = strlen(text);
            avail = width - g_lineWidthUsed;
            brk = find_line_break_l(text, &avail, len);
        } else
            brk = text;
        if (brk == 0)
            break;
        end = g_layoutLine + strlen(g_layoutLine);
        memcpy(end, text, brk - text);
        end[brk - text] = 0;
        if (g_layoutLine != 0)
            write_l(g_layoutLine, g_layoutX, g_layoutY, strlen(g_layoutLine));
        ClearString(g_layoutLine);
        g_lineWidthUsed = 0;
        m_lineCount++;
        g_layoutY += text_line_height();
        if (m_pendingIndent != 0) {
            g_layoutWidth -= m_pendingIndent;
            g_layoutX += m_pendingIndent;
            m_pendingIndent = 0;
        }
        width = g_layoutWidth;
        text = brk;
    }
    AppendString(g_layoutLine, text);
    return g_layoutY;
}

int AssetSurface::wrap_flying_flush()
{
    if (g_layoutLine[0] != 0) {
        if (g_layoutLine != 0)
            write_l(g_layoutLine, g_layoutX, g_layoutY, strlen(g_layoutLine));
        g_layoutY += text_line_height();
        m_lineCount++;
    }
    ClearString(g_layoutLine);
    g_layoutWidth = 0;
    m_renderStyle = 0;
    m_style = 0;
    m_wrapStyle = 0;
    m_linkState = 0;
    return g_layoutY;
}

int AssetSurface::ReplaceColorInRect(int x, int y, int w, int h, unsigned long from, unsigned long to)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    Rect r;
    r.SetRect(x, y, w, h);
    return t->change_color(&r, from, to);
}

int AssetSurface::change_color(unsigned long from, unsigned long to)
{
    if (!m_surface)
        return 7;
    Rect *r = m_surface->get_rect();
    return m_surface->change_color(r, from, to);
}

int AssetSurface::sync_to_palette(PaletteBase *pal)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    if (!pal)
        return 3;
    t->set_current_palette(pal->pal);
    return 0;
}

void AssetSurface::clear_links()
{
    m_links.init(80);
    m_linkCount = 0;
    for (int i = 0; i < 40; i++) {
        if (m_linkParam[i])
            m_linkParam[i] = 0;
        m_linkCommand[i] = -2;
    }
}

void AssetSurface::resetDropDownSpots()
{
    m_dropSpots.init(10);
    m_dropSpotCount = 0;
}

void AssetSurface::addDropDownSpot(int id, int param, int x, int y, int w, int h)
{
    if (++m_dropSpotCount > 20)
        MessageBoxA(0, "Maximum drop down spots exceeded!", "WARNING!", 0);
    else
        m_dropSpots.add(id, param, x, y, w, h, 0);
}

void AssetSurface::fill(int x, int y, int w, int h, int color)
{
    Rect r;
    r.SetRect(x, y, w, h);
    Surface_FillRect(&r, color);
}

void AssetSurface::ClearToColor(unsigned color)
{
    JGLBuffer *t = m_surface;
    if (t)
        t->fill(0, color);
}

int AssetSurface::Surface_FillRect(Rect *r, int color)
{
    JGLBuffer *t = m_surface;
    if (!t)
        return 7;
    return t->fill(r, color);
}

int AssetSurface::fill_shadow_over_underlay(Rect *r, AssetSurface *src, int pct, unsigned short *lut)
{
    JGLBuffer *t = m_surface;
    if (t == 0 || src->m_surface == 0)
        return 7;
    return t->fill_shadow_over_underlay(r, src->m_surface, pct, lut);
}

void AssetSurface::line(int fromX, int fromY, int toX, int toY, int color)
{
    JGLBuffer *t = m_surface;
    if (t)
        t->line(fromX, fromY, toX, toY, color, 1);
}

void AssetSurface::hline(int fromX, int toX, int y, int color)
{
    JGLBuffer *t = m_surface;
    if (t)
        t->line(fromX, y, toX, y, color, 1);
}

void AssetSurface::vline(int x, int fromY, int toY, int color)
{
    JGLBuffer *t = m_surface;
    if (t)
        t->line(x, fromY, x, toY, color, 1);
}

void AssetSurface::dotted_hline(int fromX, int toX, int y, int color)
{
    switch (GetPixelFormat()->bpp) {
    case 8: dotted_hline_8(fromX, toX, y, (unsigned char)color); break;
    case 16: dotted_hline_16(fromX, toX, y, color); break;
    }
}

void AssetSurface::dotted_vline(int x, int fromY, int toY, int color)
{
    switch (GetPixelFormat()->bpp) {
    case 8: dotted_vline_8(x, fromY, toY, (unsigned char)color); break;
    case 16: dotted_vline_16(x, fromY, toY, color); break;
    }
}

int AssetSurface::LoadImageFile(char *pData, int nSize, FLCPalette *pPal, int first, int count)
{
    PcxHeader hdr;

    if (!pData)
        return 3;

    if (strncmp(pData, Civ3encrypt, strlen(Civ3encrypt)) == 0) {
        pData += strlen(Civ3encrypt) + 1;
        nSize -= strlen(Civ3encrypt) + 1;
    }

    memcpy(&hdr, pData, sizeof(hdr));

    if (hdr.bitsPerPixel != 8)
        return 0x17;

    if (is_graphicwin() && display_depth() != 8)
        return load_pcx_8_16(pData, nSize, pPal, first, count);

    return load_pcx_8_8(pData, nSize, pPal, first, count);
}

int AssetSurface::get_pixel(int x, int y)
{
    if (m_surface != 0 && x >= 0 && y >= 0) {
        int w = (m_surface ? m_surface->get_width() : 0);
        if (x < w) {
            int h = (m_surface ? m_surface->get_height() : 0);
            if (y < h) {
                unsigned char *p = get_data(x, y);
                if (m_surface)
                    m_surface->release_data(1);
                if (p != 0) {
                    switch ((m_surface ? m_surface->get_pixformat() : 0)->bpp) {
                    case 8: return *p;
                    case 16: return *(unsigned short *)p;
                    default: return *(int *)p;
                    }
                }
            }
        }
    }
    return 0;
}

int AssetSurface::put_pixel(int x, int y, int color)
{
    if (m_surface == 0)
        return 7;
    if (x < 0 || y < 0)
        return 0;
    int w = (m_surface ? m_surface->get_width() : 0);
    if (x >= w)
        return 0;
    int h = (m_surface ? m_surface->get_height() : 0);
    if (y >= h)
        return 0;
    unsigned char *p = get_data(x, y);
    if (p != 0) {
        switch ((m_surface ? m_surface->get_pixformat() : 0)->bpp) {
        case 8: *p = (unsigned char)color; break;
        case 16: *(unsigned short *)p = (unsigned short)color; break;
        default: return 0x17;
        }
    }
    if (m_surface)
        m_surface->release_data(1);
    return 0;
}

int AssetSurface::get_pcx_dimensions(const char *szFile, int *pWidth, int *pHeight)
{
    Filemap file;
    PcxHeader hdr;
    char path[260];
    const char *p;
    const unsigned char *data;

    if (!szFile || !pWidth || !pHeight)
        return 3;

    ClearString(path);
    AppendString(path, szFile);

    for (p = path + strlen(szFile); p != path; p--)
        if (*p == '.')
            break;
    if (p == path) {
        switch (language) {
        case 1: AppendString(path, ".pcf"); break;
        case 2: AppendString(path, ".pcg"); break;
        default: AppendString(path, ".pcx"); break;
        }
    }

    if (!file.open_read(path, 1))
        return 6;

    data = (const unsigned char *)file.GetData();
    file.GetSize();

    if (strncmp((const char *)data, Civ3encrypt, strlen(Civ3encrypt)) == 0)
        data += strlen(Civ3encrypt) + 1;

    memcpy(&hdr, data, sizeof(hdr));

    if (hdr.manufacturer == 0x0a && hdr.encoding == 1) {
        *pWidth = hdr.xmax - hdr.xmin + 1;
        *pHeight = hdr.ymax - hdr.ymin + 1;

        file.close();
        return 0;
    }
    return 3;
}

void AssetSurface::box_sprite(Rect *r, BoxSpriteParams *frame)
{
    SpriteObject *s;
    int x, y, w, h;

    if (r == 0)
        return;
    if (frame == 0)
        return;

    if (graphsys)
        graphsys->set_sprite_scale(1, 1, 1);

    s = frame->topLeft;
    if (s) {
        x = r->left;
        y = r->top;
        s->BlitScaled(this, x, y, 0);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, s->m_width, s->m_height, frame->fillMode);
    }

    s = frame->topRight;
    if (s) {
        x = r->right - s->m_width;
        y = r->top;
        s->BlitScaled(this, x, y, 0);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, s->m_width, s->m_height, frame->fillMode);
    }

    s = frame->bottomLeft;
    if (s) {
        x = r->left;
        y = r->bottom - s->m_height;
        s->BlitScaled(this, x, y, 0);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, s->m_width, s->m_height, frame->fillMode);
    }

    s = frame->bottomRight;
    if (s) {
        x = r->right - s->m_width;
        y = r->bottom - s->m_height;
        s->BlitScaled(this, x, y, 0);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, s->m_width, s->m_height, frame->fillMode);
    }

    s = frame->top;
    if (s) {
        x = r->left;
        y = r->top;
        w = RectWidth(r);
        h = s->m_height;
        if (frame->topLeft) {
            x += frame->topLeft->m_width;
            w -= frame->topLeft->m_width;
        }
        if (frame->topRight)
            w -= frame->topRight->m_width;
        TileSprite(s, 0, 0, x, y, w, h);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, w, h, frame->fillMode);
    }

    s = frame->bottom;
    if (s) {
        x = r->left;
        y = r->bottom - s->m_height;
        w = RectWidth(r);
        h = s->m_height;
        if (frame->bottomLeft) {
            w -= frame->bottomLeft->m_width;
            x += frame->bottomLeft->m_width;
        }
        if (frame->bottomRight)
            w -= frame->bottomRight->m_width;
        TileSprite(s, 0, 0, x, y, w, h);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, w, h, frame->fillMode);
    }

    s = frame->left;
    if (s) {
        x = r->left;
        w = s->m_width;
        y = r->top;
        h = RectHeight(r);
        if (frame->topLeft) {
            y += frame->topLeft->m_height;
            h -= frame->topLeft->m_height;
        }
        if (frame->bottomLeft)
            h -= frame->bottomLeft->m_height;
        TileSprite(s, 0, 0, x, y, w, h);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, w, h, frame->fillMode);
    }

    s = frame->right;
    if (s) {
        w = s->m_width;
        x = r->right - w;
        y = r->top;
        h = RectHeight(r);
        if (frame->topRight) {
            y += frame->topRight->m_height;
            h -= frame->topRight->m_height;
        }
        if (frame->bottomRight)
            h -= frame->bottomRight->m_height;
        TileSprite(s, 0, 0, x, y, w, h);
        if (frame->fill)
            tile_mask(frame->fill, x, y, x, y, w, h, frame->fillMode);
    }

    if (frame->fill) {
        x = r->left;
        y = r->top;
        w = RectWidth(r);
        h = RectHeight(r);
        if (frame->left) {
            x += frame->left->m_width;
            w -= frame->left->m_width;
        }
        if (frame->top) {
            y += frame->top->m_height;
            h -= frame->top->m_height;
        }
        if (frame->right)
            w -= frame->right->m_width;
        if (frame->bottom)
            h -= frame->bottom->m_height;
        tile(frame->fill, x, y, x, y, w, h);
    }
}

void AssetSurface::box(int left, int top, int right, int bottom, int cHi, int cLo)
{
    hline(left + 1, right, top, cHi);
    hline(left, right - 1, bottom, cLo);
    vline(left, top, bottom - 1, cHi);
    vline(right, top + 1, bottom, cLo);
}

int AssetSurface::DrawBevelFrameInRect(Rect *rc, int cHi, int cLo)
{
    if (rc == 0)
        return 3;
    box(rc->left, rc->top, rc->right - 1, rc->bottom - 1, cHi, cLo);
    return 0;
}

void AssetSurface::DrawDottedRect(int left, int top, int right, int bottom, int color)
{
    dotted_hline(left + 1, right, top, color);
    dotted_hline(left, right - 1, bottom, color);
    dotted_vline(left, top, bottom, color);
    dotted_vline(right, top + 1, bottom, color);
}

int AssetSurface::dotted_box(Rect *r, int color)
{
    if (r == 0)
        return 0x10;
    DrawDottedRect(r->left, r->top, r->right - 1, r->bottom - 1, color);
    return 0;
}

unsigned char *AssetSurface::get_data(int x, int y)
{
    if (x >= (m_surface ? m_surface->get_width() : 0))
        return 0;
    if (y >= (m_surface ? m_surface->get_height() : 0))
        return 0;
    unsigned char *base = m_surface->get_data();
    if (base == 0)
        return 0;
    switch ((m_surface ? m_surface->get_pixformat() : 0)->bpp) {
    case 8: return base + y * (m_surface ? m_surface->get_pitch() : 0) + x;
    case 16: return base + (y * (m_surface ? m_surface->get_pitch() : 0) + x) * 2;
    case 24: return base + (y * (m_surface ? m_surface->get_pitch() : 0) + x) * 3;
    case 32: return base + (y * (m_surface ? m_surface->get_pitch() : 0) + x) * 4;
    }
    return 0;
}

inline GraphicWin *AssetSurface::is_graphicwin()
{
    return 0;
}

unsigned char AssetSurface::GetPixelRed(int x, int y, unsigned char def)
{
    unsigned char *p;
    int c;
    p = get_data(x, y);
    if (p == 0)
        return def;
    switch ((m_surface ? m_surface->get_pixformat() : 0)->bpp) {
    case 16:
        c = *(unsigned short *)p;
        if (graphsys->screen_mask() == 1)
            def = (unsigned char)((c >> 8) & 0xf8);
        else
            def = (unsigned char)((c >> 7) & 0xf8);
        break;
    case 24:
    case 32: def = p[2]; break;
    }
    if (m_surface)
        m_surface->release_data(1);
    return def;
}

unsigned char AssetSurface::GetPixelGreen(int x, int y, unsigned char def)
{
    unsigned char *p;
    int c;
    p = get_data(x, y);
    if (p == 0)
        return def;
    switch ((m_surface ? m_surface->get_pixformat() : 0)->bpp) {
    case 16:
        c = *(unsigned short *)p;
        if (graphsys->screen_mask() == 1)
            def = (unsigned char)((c >> 3) & 0xfc);
        else
            def = (unsigned char)((c >> 2) & 0xf8);
        break;
    case 24:
    case 32: def = p[1]; break;
    }
    if (m_surface)
        m_surface->release_data(1);
    return def;
}

unsigned char AssetSurface::GetPixelBlue(int x, int y, unsigned char def)
{
    unsigned char *p;
    p = get_data(x, y);
    if (p == 0)
        return def;
    switch ((m_surface ? m_surface->get_pixformat() : 0)->bpp) {
    case 16: def = (unsigned char)(*p << 3); break;
    case 24: def = p[0]; break;
    case 32: def = p[0]; break;
    }
    if (m_surface)
        m_surface->release_data(1);
    return def;
}

char AssetSurface::GetDirty()
{
    return m_dirty;
}

void AssetSurface::SetDirty(char dirty)
{
    m_dirty = dirty;
}
