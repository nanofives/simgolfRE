#include "jgldefs.h"
#include "jglsprite.h"
#include "filemap.h"
#include "jgltypes.h"
#include "jgl.h"
#include <iostream.h>
#include "jglbuffertyped.h"
#include "jglpalettemgr.h"
#include "jglfont.h"
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>


JGLBuffer::JGLBuffer()
{
    m_field08 = 0;
    m_owner = 0;
    m_width = 0;
    m_height = 0;
    m_pitch = 0;
    m_flag74 = 0;
    m_palette = 0;
    m_hRgn = 0;
    m_hOldBitmap = 0;
    m_hBitmap = 0;
    m_hdcCached = 0;
    m_hdc = 0;
    m_bits = 0;
    m_dcRefCount = 0;
    m_lockCount = 0;
    m_bmi.bmiHeader.biSize = 0x28;
    m_bmi.bmiHeader.biWidth = 0;
    m_bmi.bmiHeader.biHeight = 0;
    m_bmi.bmiHeader.biPlanes = 1;
    m_bmi.bmiHeader.biBitCount = 8;
    m_bmi.bmiHeader.biCompression = 0;
    m_bmi.bmiHeader.biSizeImage = 0;
    m_bmi.bmiHeader.biXPelsPerMeter = 0;
    m_bmi.bmiHeader.biYPelsPerMeter = 0;
    m_bmi.bmiHeader.biClrUsed = 0x100;
    m_bmi.bmiHeader.biClrImportant = 0;
    memset(&m_bounds, 0, sizeof(Rect));
    InitializeCriticalSection(&m_lock);
    if (graphsys)
        m_colorKey = PackRGB16(0xff, 0, 0xff);
    else
        m_colorKey = 0x80007c1f;
}


JGLBuffer::JGLBuffer(BufferBase *pOwner)
{
    m_owner = pOwner;
    m_field08 = 0;
    m_width = 0;
    m_height = 0;
    m_pitch = 0;
    m_flag74 = 0;
    m_palette = 0;
    m_hRgn = 0;
    m_hOldBitmap = 0;
    m_hBitmap = 0;
    m_hdcCached = 0;
    m_hdc = 0;
    m_bits = 0;
    m_dcRefCount = 0;
    m_lockCount = 0;
    m_bmi.bmiHeader.biSize = 0x28;
    m_bmi.bmiHeader.biWidth = 0;
    m_bmi.bmiHeader.biHeight = 0;
    m_bmi.bmiHeader.biPlanes = 1;
    m_bmi.bmiHeader.biBitCount = 8;
    m_bmi.bmiHeader.biCompression = 0;
    m_bmi.bmiHeader.biSizeImage = 0;
    m_bmi.bmiHeader.biXPelsPerMeter = 0;
    m_bmi.bmiHeader.biYPelsPerMeter = 0;
    m_bmi.bmiHeader.biClrUsed = 0x100;
    m_bmi.bmiHeader.biClrImportant = 0;
    memset(&m_bounds, 0, sizeof(Rect));
    InitializeCriticalSection(&m_lock);
    m_colorKey = PackRGB16(0xff, 0, 0xff);
}


JGLBuffer::~JGLBuffer()
{
    if (m_owner)
        m_owner->m_surface = 0;
    DeleteCriticalSection(&m_lock);
    close();
}


void JGLBuffer::close()
{
    if (m_hdc) {
        m_dcRefCount = 0;
        m_lockCount = 0;
        if (m_hOldBitmap) {
            SelectObject(m_hdc, m_hOldBitmap);
            m_hOldBitmap = 0;
        }
        DeleteDC(m_hdc);
        m_hdcCached = 0;
        m_hdc = 0;
    }
    if (m_hBitmap) {
        DeleteObject(m_hBitmap);
        m_hBitmap = 0;
    }
    if (m_hRgn) {
        DeleteObject(m_hRgn);
        m_hRgn = 0;
    }
}


int JGLBuffer::init(int width, int height, int bpp, int memType)
{
    char msg[0x400];
    if (width < 0 || height < 0)
        return 3;
    if (width == m_width && height == m_height && (this->bpp == bpp || bpp == 0))
        return 0;
    this->close();
    if (bpp == 0) {
        this->bpp = graphsys->screen_bpp();
        if (this->bpp == 0)
            this->bpp = 8;
    } else {
        this->bpp = bpp;
    }
    m_hdc = CreateCompatibleDC(0);
    if (m_hdc == 0)
        return 1;
    m_bmi.bmiHeader.biWidth = width;
    m_bmi.bmiHeader.biHeight = -height;
    m_bmi.bmiHeader.biBitCount = (ushort)this->bpp;
    m_bmi.bmiHeader.biClrUsed = 0;
    m_hBitmap = (HBITMAP)CreateDIBSection(m_hdc, (const BITMAPINFO *)&m_bmi, 0, &m_bits, 0, 0);
    if (m_hBitmap == 0) {
        FormatMessageA(0x1200, 0, GetLastError(), 0x400, msg, 0x400, 0);
        MessageBoxA(0, "Unable to allocate draw-buffer; terminating program.", "FATAL ERROR",
                    MB_OK | MB_ICONEXCLAMATION);
        exit(4);
    }
    m_hOldBitmap = (HBITMAP)SelectObject(m_hdc, m_hBitmap);
    switch (this->bpp) {
    case 8:
        m_pitch = ((width + 3) >> 2) << 2;
        break;
    case 16:
        m_pitch = ((width + 1) >> 1) << 1;
        break;
    case 24:
        m_pitch = width + (width & 1);
        break;
    case 32:
        m_pitch = width;
        break;
    default:
        return 0x17;
    }
    m_width = width;
    m_height = height;
    this->code = 0;
    if (this->code == 0) {
        this->rShift = 0xa;
        this->gShift = 0x5;
        this->bShift = 0x0;
        this->bMask = 0x1f;
        this->gMask = (ushort)(this->bMask ^ 0x3ff);
        this->rMask = (ushort)((this->bMask | this->gMask) ^ 0x7fff);
    } else {
        this->rShift = 0xb;
        this->gShift = 0x6;
        this->bShift = 0x0;
        this->bMask = 0x1f;
        this->gMask = (ushort)(this->bMask ^ 0x7ff);
        this->rMask = (ushort)((this->bMask | this->gMask) ^ 0xffff);
    }
    SetBkMode(m_hdc, 1);
    SetTextAlign(m_hdc, 0x18);
    jgl::SetRect(&m_bounds, 0, 0, width, height);
    this->set_clip(&m_bounds);
    return 0;
}

namespace jgl {

extern "C" void SetRect(Rect *r, int x, int y, int w, int h)
{
    r->left = x;
    r->top = y;
    r->right = x + w;
    r->bottom = y + h;
}
}


int JGLBuffer::set_clip(Rect *pRect)
{
    Rect r;
    HDC  hdc;
    if (pRect == 0)
        return 3;
    r = *pRect;
    if (!jgl_IntersectRect(&m_clip, &m_bounds, &r))
        return 1;
    hdc = this->get_hdc();
    if (hdc != 0) {
        if (m_hRgn != 0) {
            DeleteObject(m_hRgn);
            m_hRgn = 0;
        }
        if (RectEqual(&m_clip, &m_bounds)) {
            SelectClipRgn(hdc, 0);
        } else {
            m_hRgn = CreateRectRgnIndirect((const RECT *)&m_clip);
            if (m_hRgn == 0)
                return 1;
            SelectClipRgn(hdc, m_hRgn);
        }
    }
    this->release_hdc(1);
    return 0;
}


extern "C" int jgl_IntersectRect(Rect *dst, const Rect *src1, const Rect *src2)
{
    return IntersectRect((LPRECT)dst, (const RECT *)src1, (const RECT *)src2);
}

extern "C" int RectEqual(const Rect *rect1, const Rect *rect2)
{
    return EqualRect((const RECT *)rect1, (const RECT *)rect2);
}


HDC JGLBuffer::get_hdc()
{
    this->m_hdcCached = this->m_hdc;
    this->m_dcRefCount++;
    return this->m_hdcCached;
}


void JGLBuffer::release_hdc(int n)
{
    this->m_dcRefCount -= n;
    if (this->m_dcRefCount <= 0) {
        this->m_hdcCached = 0;
        this->m_dcRefCount = 0;
    }
}


uchar *JGLBuffer::get_data()
{
    this->m_lockedBits = (uchar *)this->m_bits;
    if (this->m_lockedBits != 0)
        this->m_lockCount++;
    return this->m_lockedBits;
}


void JGLBuffer::release_data(int n)
{
    this->m_lockCount -= n;
    if (this->m_lockCount <= 0) {
        this->m_lockedBits = 0;
        this->m_lockCount = 0;
    }
}


uchar *JGLBuffer::SURF_GetPixelPtr(int x, int y)
{
    uchar *p;
    if (x >= this->get_width() || y >= this->get_height())
        return 0;
    p = this->get_data();
    if (p == 0)
        return 0;
    switch (this->bpp) {
    case 8:
        return p + x + y * this->m_pitch;
    case 16:
        return p + x * 2 + y * this->m_pitch * 2;
    case 24:
        return p + x * 3 + y * this->m_pitch * 3;
    case 32:
        return p + x * 4 + y * this->m_pitch * 4;
    }
    return 0;
}


void JGLBuffer::set_current_palette(JGLPalette *pal)
{
    JGLPalette *p = pal;
    RgbQuad     entries[256];
    if (p == 0)
        return;
    this->m_palette = p;
    if (this->m_palVersion != p->GetPaletteVersion()) {
        this->m_palVersion = p->GetPaletteVersion();
        p->fill_rgb(entries);
        if (this->get_hdc()) {
            SetDIBColorTable(this->m_hdcCached, 0, 256, (const RGBQUAD *)entries);
            this->release_hdc(1);
        }
    }
}


int JGLBuffer::select_font(FontBase *metrics)
{
    JGLFont *owner;
    HDC      hdc;
    hdc = get_hdc();
    owner = metrics->m_owner;
    SelectObject(hdc, owner->m_hFont);
    release_hdc(1);
    return 0;
}


int JGLBuffer::deselect_font()
{
    HDC hdc = get_hdc();
    SelectObject(hdc, GetStockObject(SYSTEM_FONT));
    release_hdc(1);
    return 0;
}


int JGLBuffer::set_text_color(int r, int g, int b)
{
    HDC hdc = get_hdc();
    SetTextColor(hdc, RGB(r, g, b));
    release_hdc(1);
    return 0;
}


int JGLBuffer::SURF_SetTextColor16(int index)
{
    HDC hdc = get_hdc();
    SetTextColor(hdc, DIBINDEX(index));
    release_hdc(1);
    return 0;
}


int JGLBuffer::write_text(int x, int y, const char *text, int count)
{
    HDC hdc = get_hdc();
    TextOutA(hdc, x, y, text, count);
    release_hdc(1);
    return 0;
}


int JGLBuffer::copy_mask(JGLBuffer *src, int sx, int sy, int w, int h, int dx, int dy, int mode)
{
    switch (this->get_pixformat()->bpp) {
    case 8:
        switch (src->get_pixformat()->bpp) {
        case 8:
            return this->copy_mask_8_8(src, sx, sy, w, h, dx, dy, mode);
        case 16:
            return this->copy_mask_8_16(src, sx, sy, w, h, dx, dy, mode);
        }
        return 0x17;
    }
    return 0x17;
}


int JGLBuffer::copy(JGLBuffer *dst, Rect *srcRect, Rect *dstRect)
{
    HDC        hdcDst;
    HDC        hdcSrc;
    JGLBuffer *pDst = dst;
    if (pDst == 0 || srcRect == 0 || dstRect == 0)
        return 0x10;
    hdcDst = pDst->get_hdc();
    if (hdcDst == 0)
        return 7;
    hdcSrc = this->get_hdc();
    if (hdcSrc == 0) {
        pDst->release_hdc(1);
        return 7;
    }
    StretchBlt(hdcDst, dstRect->left, dstRect->top,

               GetRectWidth(dstRect), GetRectHeight(dstRect), hdcSrc, srcRect->left, srcRect->top,
               GetRectWidth(srcRect), GetRectHeight(srcRect), 0xcc0020);
    pDst->release_hdc(1);
    this->release_hdc(1);
    return 0;
}


int JGLBuffer::SURF_Blit(JGLBuffer *src, int sx, int sy, int w, int h, int dx, int dy)
{
    switch (this->bpp) {
    case 8:
        switch (src->get_pixformat()->bpp) {
        case 8:
            return this->copy_8_8(src, sx, sy, w, h, dx, dy);
        case 16:
            return this->copy_8_16(src, sx, sy, w, h, dx, dy);
        }
        return 0x17;
    case 16:
        return this->copy_16_16(src, sx, sy, w, h, dx, dy);
    }
    return 0x17;
}


PixelFormat::PixelFormat()
{
    code = 0;
}

void SetRect(Rect *r, int x0, int y0, int x1, int y1);
int  IntersectRect(Rect *dst, Rect *src1, Rect *src2);

int JGLBuffer::change_color(Rect *r, int from, int to)
{
    if (r == 0)
        return 0x10;
    switch (this->get_pixformat()->bpp) {
    case 8:
        return this->change_color_8(r->left, r->top, GetRectWidth(r), GetRectHeight(r), from, to);
    case 16:
        return this->change_color_16(r->left, r->top, GetRectWidth(r), GetRectHeight(r), from, to);
    }
    return 0x17;
}


int JGLBuffer::map_colors(Rect *pRect, const uchar *lut)
{
    switch (this->get_pixformat()->bpp) {
    case 8:
        return this->map_colors_8(pRect->left, pRect->top, pRect->right, pRect->bottom, lut);
    }
    return 0;
}


int JGLBuffer::line(int x0, int y0, int x1, int y1, int color, int mode)
{
    switch (get_pixformat()->bpp) {
    case 8:
        line_8(x0, y0, x1, y1, color);
        break;
    case 16:
        line_16(x0, y0, x1, y1, color);
        break;
    }
    return 0;
}


int JGLBuffer::SURF_DrawDashedLine(int x0, int y0, int x1, int y1, int onColor, int offColor, int on, int off, int ph)
{
    switch (this->get_pixformat()->bpp) {
    case 8:
        this->DrawDashedLine8(x0, y0, x1, y1, onColor, offColor, on, off, ph);
        break;
    case 16:
        this->DrawDashedLine(x0, y0, x1, y1, onColor, offColor, on, off, ph);
        break;
    }
    return 0;
}


GSBuffer::GSBuffer() {}


GSBuffer::~GSBuffer() {}


HDC GSBuffer::get_hdc()
{
    return 0;
}


int JGLBuffer::fill(Rect *pRect, int color)
{
    switch (this->get_pixformat()->bpp) {
    case 8:
        this->fill8(pRect, color);
        break;
    case 16:
        this->fill16(pRect, color);
        break;
    }
    return 0;
}


void GSBuffer::release_hdc(int n) {}


void GSBuffer::sync_to_palette(GSPalette *) {}


int GSBuffer::SURF_Blit(JGLBuffer *src, int sx, int sy, int w, int h, int dx, int dy)
{
    return 0x18;
}


int GSBuffer::change_color(Rect *r, int from, int to)
{
    return 0x18;
}


int JGLBuffer::fill_shadow_over_underlay(Rect *pRect, JGLBuffer *src, int pct, ushort *lut)
{
    switch (this->get_pixformat()->bpp) {
    case 16:
        switch (src->get_pixformat()->bpp) {
        case 16:
            return this->fill_shadow_over_underlay_16_16(pRect, src, pct, lut);
        }
        return 0x17;
    }
    return 0x17;
}


int GSBuffer::map_colors(Rect *pRect, const uchar *lut)
{
    return 0x18;
}


int GSBuffer::line(int x0, int y0, int x1, int y1, int color, int mode)
{
    return 0x18;
}


int GSBuffer::SURF_DrawDashedLine(int, int, int, int, int, int, int, int, int)
{
    return 0x18;
}


int GSBuffer::load_pcx(char *, int, int, int)
{
    return 0x18;
}


int JGLBuffer::fill_translucent(tagRECT *rc, int color, int percent)
{
    switch (this->get_pixformat()->bpp) {
    case 16:
        return this->fill_translucent_16((Rect *)rc, color, percent);
    }
    return 0x17;
}


int GSBuffer::load_bmp(char *, int, int, int)
{
    return 0x18;
}


int GSBuffer::load_targa(char *)
{
    return 0x18;
}


int GSBuffer::load_png(const char *name, JGLPalette *pPal, int first, int count)
{
    return 0x18;
}


int GSBuffer::write_pcx(const char *path)
{
    return 0x18;
}


int JGLBuffer::dither(Rect *pRect, int color)
{
    switch (this->get_pixformat()->bpp) {
    case 8:
        return this->dither_8(pRect, color);
    case 16:
        return this->dither_16(pRect, color);
    }
    return 0x17;
}


int GSBuffer::write_bmp(char *)
{
    return 0x18;
}


int GSBuffer::draw(JGLBuffer *dst, int x, int y)
{
    return 0x18;
}


int GSBuffer::draw_mono(GSBuffer *, int, int, int)
{
    return 0x18;
}


int GSBuffer::draw_dest(JGLBuffer *dst, int x, int y, uchar *lut)
{
    return 0x18;
}


int GSBuffer::SURF_ShadowBlit(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    return 0x18;
}


int JGLBuffer::dither_translucent(tagRECT *rc, int color, int pct)
{
    switch (this->get_pixformat()->bpp) {
    case 16:
        return this->dither_translucent_16((Rect *)rc, color, pct);
    }
    return 0x17;
}


int GSBuffer::draw_dest_no_source(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int GSBuffer::draw_dest_source(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int GSBuffer::SURF_Stub_NotImplemented_18(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int GSBuffer::draw_table(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int JGLBuffer::draw(JGLBuffer *dst, int x, int y)
{
    JGLSprite self(0);
    int       rc;
    self.m_bpp = this->bpp;
    self.m_pixels = this->get_data();
    self.m_flags = 0;
    self.m_srcStride = this->m_pitch;
    self.m_width = this->get_width();
    self.m_height = this->get_height();
    self.m_palette = this->m_palette;
    self.set_seethru(this->m_colorKey);
    rc = self.draw(dst, x, y, this->get_current_palette());
    this->release_data(1);
    self.m_pixels = 0;
    return rc;
}


int GSBuffer::draw_multi_table_dest(JGLBuffer *dst, int x, int y, uchar *lut)
{
    return 0x18;
}


int GSBuffer::SURF_Stub_NotImplemented_21(JGLBuffer *dst, int x, int y, int)
{
    return 0x18;
}


int GSBuffer::select_font(FontBase *metrics)
{
    return 0x18;
}


int GSBuffer::deselect_font()
{
    return 0x18;
}


int GSBuffer::set_text_color(int r, int g, int b)
{
    return 0x18;
}


int GSBuffer::SURF_SetTextColor16(int index)
{
    return 0x18;
}


int GSBuffer::write_text(int x, int y, const char *text, int count)
{
    return 0x18;
}


int JGLBuffer::SURF_ShadowBlit(JGLBuffer *dst, int x, int y, ushort *shadow)
{
    JGLSprite self(0);
    int       rc;
    self.m_bpp = this->bpp;
    self.m_pixels = this->get_data();
    self.m_flags = 0;
    self.m_srcStride = this->m_pitch;
    self.m_width = this->get_width();
    self.m_height = this->get_height();
    self.m_palette = this->m_palette;
    self.set_seethru(this->m_colorKey);
    rc = self.ShadowBlit(dst, x, y, shadow, this->get_current_palette());
    this->release_data(1);
    self.m_pixels = 0;
    return rc;
}
inline int SurfacePad(int a, int b)
{
    int c = a * b;
    if (c > 3)
        c -= a;
    if (c < b)
        c += a;
    if (c == a)
        c = b;
    if (c != b)
        c ^= a;
    return c + b;
}

int JGLBuffer::write_pcx(const char *path)
{
    switch (this->get_pixformat()->bpp) {
    case 8:
        return this->write_pcx_8(path);
    }
    return 0x17;
}


int JGLBuffer::set_seethru(int key)
{
    this->m_colorKey = key;
    return 0;
}


ulong JGLBuffer::get_seethru()
{
    return m_colorKey;
}


extern "C" int PackRGB16(uchar r, uchar g, uchar b)
{
    if (graphsys->screen_mask() == 1)
        return (b >> 3) | ((unsigned short)(g >> 2) << 5) | ((r >> 3) << 11) | 0x80000000;
    return (b >> 3) | ((unsigned short)(g >> 3) << 5) | ((r >> 3) << 10) | 0x80000000;
}


uchar *JGLBuffer::get_data_8()
{
    return this->get_data();
}


ushort *JGLBuffer::get_data_16()
{
    return (ushort *)this->get_data();
}


uchar *JGLBuffer::SURF_GetPixelPtr8(int x, int y)
{
    return this->SURF_GetPixelPtr(x, y);
}


ushort *JGLBuffer::SURF_GetPixelPtr16(int x, int y)
{
    return (ushort *)this->SURF_GetPixelPtr(x, y);
}


int JGLBuffer::reset_clip()
{
    return this->set_clip(&this->m_bounds);
}


void JGLBuffer::SURF_CopyClipRect(Rect *r)
{
    *r = this->m_clip;
}


Rect *JGLBuffer::get_clip()
{
    return &m_clip;
}

void JGLBuffer::SURF_CopyBoundsRect(Rect *r)
{
    *r = this->m_bounds;
}


Rect *JGLBuffer::get_rect()
{
    return &m_bounds;
}

int JGLBuffer::get_width()
{
    return m_bounds.right - m_bounds.left;
}

int JGLBuffer::get_height()
{
    return m_bounds.bottom - m_bounds.top;
}

int JGLBuffer::get_pitch()
{
    return m_pitch;
}

PixelFormat *JGLBuffer::get_pixformat()
{
    return (PixelFormat *)&this->bpp;
}

JGLPalette *JGLBuffer::get_current_palette()
{
    return m_palette;
}

int JGLBuffer::load_pcx(char *, int, int, int)
{
    return 0x18;
}


int JGLBuffer::load_bmp(char *, int, int, int)
{
    return 0x18;
}


int JGLBuffer::load_targa(char *)
{
    return 0x18;
}


int JGLBuffer::write_bmp(char *)
{
    return 0x18;
}


int JGLBuffer::draw_mono(GSBuffer *, int, int, int)
{
    return 0x18;
}


int JGLBuffer::draw_dest(JGLBuffer *dst, int x, int y, uchar *lut)
{
    return 0x18;
}


int JGLBuffer::draw_dest_no_source(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int JGLBuffer::draw_dest_source(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int JGLBuffer::SURF_Stub_NotImplemented_18(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int JGLBuffer::draw_table(GSBuffer *, int, int, uchar *)
{
    return 0x18;
}


int JGLBuffer::draw_multi_table_dest(JGLBuffer *dst, int x, int y, uchar *lut)
{
    return 0x18;
}


int JGLBuffer::SURF_Stub_NotImplemented_21(JGLBuffer *dst, int x, int y, int)
{
    return 0x18;
}


#include "jglspriteaccessors.inl"


inline int JGLPalette::GetPaletteVersion()
{
    return m_palVersion;
}


extern "C" void memsetword(ushort *dst, ushort val, int count)
{
    int            n;
    unsigned long *p;
    unsigned long  dw;
    dw = ((unsigned long)val << 16) | val;
    if (count & 1) {
        *dst = (unsigned short)val;
        p = (unsigned long *)(dst + 1);
    } else {
        p = (unsigned long *)dst;
    }
    for (n = count / 2; n > 0; n--) {
        *p = dw;
        p++;
    }
}


int JGLBuffer::change_color_16(int x, int y, int w, int h, int from, int to)
{
    Rect    r;
    ushort *dst;
    int     advance;
    jgl::SetRect(&r, x, y, w, h);
    if (!jgl_IntersectRect(&r, this->get_clip(), &r))
        return 0;
    dst = this->SURF_GetPixelPtr16(r.left, r.top);
    if (dst == 0)
        return 7;
    w = GetRectWidth(&r);
    h = GetRectHeight(&r);
    advance = (this->m_pitch - w) * 2;
    __asm {
        push edi
        push esi
        mov  ax, word ptr from
        mov  bx, word ptr to
        mov  edi, dst
        mov  ecx, h
        mov  edx, advance
    L_row:
        mov  esi, w
    L_col:
        cmp  word ptr [edi], ax
        jnz  L_skip
        mov  word ptr [edi], bx
    L_skip:
        add  edi, 2
        dec  esi
        jnz  L_col
        add  edi, edx
        dec  ecx
        jnz  L_row
        pop  esi
        pop  edi
    }
    this->release_data(1);
    return 0;
}


void JGLBuffer::line_16(int x0, int y0, int x1, int y1, int color)
{
    Rect        clip;
    int         d;
    int         dx;
    int         dy;
    int         incE;
    int         incNE;
    int         incNE2;
    ushort     *dstp;
    JGLPalette *pal;
    if (x0 == x1) {
        this->vline_16(x0, y0, y1, color);
        return;
    }
    if (y0 == y1) {
        this->hline_16(x0, x1, y0, color);
        return;
    }
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                return;
            }
        }
    }
    color &= 0xffff;
    if (x1 < x0) {
        __asm { mov  eax, x0
                xchg x1, eax
                mov  x0, eax
                mov  eax, y0
                xchg y1, eax
                mov  y0, eax }
    }
    if (this->get_data() == 0)
        return;
    this->SURF_CopyClipRect(&clip);
    dx = x1 - x0;
    dy = y1 - y0;
    if (y0 < y1) {
        if (x0 > clip.right || y0 > clip.bottom || x1 < clip.left || y1 < clip.top)
            goto done;
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE = 2 * (dy - dx);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE;
                    y0++;
                }
                x0++;
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            *dstp = (ushort)color;
            if (x1 >= clip.right)
                x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y0++;
                    if (y0 >= clip.bottom)
                        goto done;
                    d += incNE;
                    dstp += this->get_pitch();
                }
                dstp++;
                x0++;
                *dstp = (ushort)color;
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE = 2 * (dx - dy);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE;
                    x0++;
                }
                y0++;
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            *dstp = (ushort)color;
            if (y1 >= clip.bottom)
                y1 = clip.bottom - 1;
            while (y0 < y1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE;
                    dstp++;
                }
                y0++;
                dstp += this->get_pitch();
                *dstp = (ushort)color;
            }
            goto done;
        }
    } else {
        if (x0 > clip.right || y0 < clip.top || x1 < clip.left || y1 > clip.bottom)
            goto done;
        dy = abs(dy);
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE2 = 2 * (dy - dx);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE2;
                    y0--;
                }
                x0++;
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            *dstp = (ushort)color;
            if (x1 >= clip.right)
                x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y0--;
                    if (y0 < clip.top)
                        goto done;
                    d += incNE2;
                    dstp -= this->get_pitch();
                }
                x0++;
                dstp++;
                *dstp = (ushort)color;
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE2 = 2 * (dx - dy);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                } else {
                    d += incNE2;
                    x0++;
                }
                y0--;
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            *dstp = (ushort)color;
            if (y1 < clip.top)
                y1 = clip.top;
            while (y0 > y1) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE2;
                    dstp++;
                }
                y0--;
                dstp -= this->get_pitch();
                *dstp = (ushort)color;
            }
            goto done;
        }
    }
done:
    this->release_data(1);
}


void JGLBuffer::hline_16(int x0, int x1, int y, int color)
{
    ushort     *p;
    JGLPalette *pal;
    int         n;
    if (y < this->get_clip()->top || y >= this->get_clip()->bottom)
        return;
    if (x0 == x1)
        return;
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                return;
            }
        }
    }
    color &= 0xffff;
    if (x0 > x1) {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
    }
    if (x0 >= this->get_clip()->right || x1 < this->get_clip()->left)
        return;
    if (x0 < this->get_clip()->left)
        x0 = this->get_clip()->left;
    if (x1 >= this->get_clip()->right)
        x1 = this->get_clip()->right - 1;
    p = this->SURF_GetPixelPtr16(x0, y);
    if (p == 0)
        return;
    for (n = x1 - x0 + 1; n > 0; n--) {
        *p = (ushort)color;
        p++;
    }
    this->release_data(1);
}


void JGLBuffer::vline_16(int x, int y0, int y1, int color)
{
    ushort     *p;
    int         stride;
    JGLPalette *pal;
    if (x < this->get_clip()->left || x >= this->get_clip()->right)
        return;
    if (y0 == y1)
        return;
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                return;
            }
        }
    }
    color &= 0xffff;
    if (y0 > y1) {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
    }
    if (y0 >= this->get_clip()->bottom || y1 < this->get_clip()->top)
        return;
    if (y0 < this->get_clip()->top)
        y0 = this->get_clip()->top;
    if (y1 >= this->get_clip()->bottom)
        y1 = this->get_clip()->bottom - 1;
    p = this->SURF_GetPixelPtr16(x, y0);
    if (p == 0)
        return;
    stride = this->get_pitch();
    __asm { push edi
            mov  ecx, y1
            sub  ecx, y0
            inc  ecx
            mov  ax,  word ptr color
            mov  ebx, stride
            shl  ebx, 1
            mov  edi, p
        vrun:
            mov  word ptr [edi], ax
            add  edi, ebx
            loop vrun
            pop  edi
    }
    this->release_data(1);
}

int        g_lineRunCount = 0;
static int s_lineSpare = 0;
#define PLOT                                            \
    if (phase < 0) {                                    \
        if (onColor != -1) *dstp = (ushort)onColor;               \
        phase++;                                        \
        if (phase == 0) phase = off;                    \
    } else {                                            \
        if (offColor != -1) *dstp = (ushort)offColor;               \
        phase--;                                        \
        if (phase == 0) phase = on;                     \
    }

void JGLBuffer::DrawDashedLine(int x0, int y0, int x1, int y1, int onColor, int offColor, int on, int off, int phase)
{
    Rect        clip;
    int         d;
    int         dx;
    int         dy;
    int         incE;
    int         incNE;
    int         incNE2;
    ushort     *dstp;
    JGLPalette *pal;
    if (x0 == x1) {
        this->DrawDashedVLine(x0, y0, y1, onColor, offColor, on, off, phase);
        return;
    }
    if (y0 == y1) {
        this->DrawDashedHLine(x0, x1, y0, onColor, offColor, on, off, phase);
        return;
    }
    pal = 0;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else if (def_palette != 0)
        pal = def_palette->pal;
    if (pal != 0) {
        switch (this->get_pixformat()->code) {
        case 0:
            if ((onColor & 0x80000000) == 0)
                onColor = pal->get_555_table()[onColor & 0xff];
            if ((offColor & 0x80000000) == 0)
                offColor = pal->get_565_table()[offColor & 0xff];
            break;
        case 1:
            if ((onColor & 0x80000000) == 0)
                onColor = pal->get_555_table()[onColor & 0xff];
            if ((offColor & 0x80000000) == 0)
                offColor = pal->get_565_table()[offColor & 0xff];
            break;
        default:
            return;
        }
    }
    onColor = onColor & 0xffff;
    offColor = offColor & 0xffff;
    if (x1 < x0) {
        __asm { mov  eax, x0
                xchg x1, eax
                mov  x0, eax
                mov  eax, y0
                xchg y1, eax
                mov  y0, eax }
        on ^= off;
        off ^= on;
        on ^= off;
        onColor ^= offColor;
        offColor ^= onColor;
        onColor ^= offColor;
        phase = on + off - phase;
    }
    if (this->get_data() == 0)
        return;
    this->SURF_CopyClipRect(&clip);
    phase = phase % (on + off);
    if (phase >= on) {
        phase = phase - on;
        phase = off - phase;
    } else {
        phase = on - phase;
        phase = -phase;
    }
    on = -on;
    dx = x1 - x0;
    dy = y1 - y0;
    if (y0 < y1) {
        if (x0 > clip.right || y0 > clip.bottom || x1 < clip.left || y1 < clip.top)
            goto done;
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE = 2 * (dy - dx);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                } else {
                    d += incNE;
                    x0++;
                    y0++;
                }
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            PLOT if (x1 >= clip.right) x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                    dstp++;
                } else {
                    y0++;
                    if (y0 >= clip.bottom)
                        goto done;
                    d += incNE;
                    x0++;
                    dstp += this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE = 2 * (dx - dy);
            while (x0 < clip.left || y0 < clip.top) {
                if (d <= 0) {
                    d += incE;
                    y0++;
                } else {
                    d += incNE;
                    y0++;
                    x0++;
                }
            }
            if (x0 >= clip.right || y0 >= clip.bottom)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            PLOT if (y1 >= clip.bottom) y1 = clip.bottom - 1;
            while (y0 < y1) {
                if (d <= 0) {
                    d += incE;
                    y0++;
                    dstp += this->get_pitch();
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE;
                    y0++;
                    dstp += this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        }
    } else {
        if (x0 > clip.right || y0 < clip.top || x1 < clip.left || y1 > clip.bottom)
            goto done;
        dy = abs(dy);
        if (dx >= dy) {
            d = 2 * dy - dx;
            incE = 2 * dy;
            incNE2 = 2 * (dy - dx);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                } else {
                    d += incNE2;
                    x0++;
                    y0--;
                }
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            PLOT if (x1 >= clip.right) x1 = clip.right - 1;
            while (x0 < x1) {
                if (d <= 0) {
                    d += incE;
                    x0++;
                    dstp++;
                } else {
                    y0--;
                    if (y0 < clip.top)
                        goto done;
                    d += incNE2;
                    x0++;
                    dstp = dstp - this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        } else {
            d = 2 * dx - dy;
            incE = 2 * dx;
            incNE2 = 2 * (dx - dy);
            while (x0 < clip.left || y0 >= clip.bottom) {
                if (d <= 0) {
                    d += incE;
                    y0--;
                } else {
                    d += incNE2;
                    y0--;
                    x0++;
                }
            }
            if (x0 >= clip.right || y0 < clip.top)
                goto done;
            dstp = this->get_data_16() + x0 + y0 * this->get_pitch();
            PLOT if (y1 < clip.top) y1 = clip.top;
            while (y0 > y1) {
                if (d <= 0) {
                    d += incE;
                    y0--;
                    dstp = dstp - this->get_pitch();
                } else {
                    x0++;
                    if (x0 >= clip.right)
                        goto done;
                    d += incNE2;
                    y0--;
                    dstp = dstp - this->get_pitch() + 1;
                }
                PLOT
            }
            goto done;
        }
    }
done:
    this->release_data(1);
}


void JGLBuffer::DrawDashedHLine(int x0, int x1, int y, int onColor, int offColor, int on, int off, int phase)
{
    ushort     *p;
    JGLPalette *pal;
    if (y < this->get_clip()->top || y >= this->get_clip()->bottom)
        return;
    if (x0 == x1)
        return;
    if (onColor == -1 && offColor == -1)
        return;
    pal = 0;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else if (def_palette != 0)
        pal = def_palette->pal;
    if (pal != 0) {
        switch (this->get_pixformat()->code) {
        case 0:
            if ((onColor & 0x80000000) == 0)
                onColor = pal->get_555_table()[onColor & 0xff];
            if ((offColor & 0x80000000) == 0)
                offColor = pal->get_565_table()[offColor & 0xff];
            break;
        case 1:
            if ((onColor & 0x80000000) == 0)
                onColor = pal->get_555_table()[onColor & 0xff];
            if ((offColor & 0x80000000) == 0)
                offColor = pal->get_565_table()[offColor & 0xff];
            break;
        default:
            return;
        }
    }
    onColor &= 0xffff;
    offColor &= 0xffff;
    if (x0 > x1) {
        x0 ^= x1;
        x1 ^= x0;
        x0 ^= x1;
        on ^= off;
        off ^= on;
        on ^= off;
        onColor ^= offColor;
        offColor ^= onColor;
        onColor ^= offColor;
        phase = on + off - phase;
    }
    if (x0 >= this->get_clip()->right || x1 < this->get_clip()->left)
        return;
    if (x0 < this->get_clip()->left)
        x0 = this->get_clip()->left;
    if (x1 >= this->get_clip()->right)
        x1 = this->get_clip()->right - 1;
    p = this->SURF_GetPixelPtr16(x0, y);
    if (p == 0)
        return;
    phase = phase % (on + off);
    if (phase >= on) {
        phase = phase - on;
        phase = off - phase;
    } else {
        phase = on - phase;
        phase = -phase;
    }
    on = -on;
    if (onColor == -1) {
        __asm { push edi
                push esi
                mov  ecx, x1
                sub  ecx, x0
                inc  ecx
                mov  ax,  word ptr offColor
                mov  ebx, on
                mov  esi, off
                mov  edi, p
                mov  edx, phase
            a_top:
                and  edx, edx
                js   a_neg
                mov  word ptr [edi], ax
                dec  edx
                jnz  a_next
                mov  edx, ebx
                jmp  a_next
            a_neg:
                inc  edx
                jnz  a_next
                mov  edx, esi
            a_next:
                add  edi, 2
                dec  ecx
                jnz  a_top
                pop  esi
                pop  edi
        }
    } else if (offColor == -1) {
        __asm { push esi
                push edi
                mov  ecx, x1
                sub  ecx, x0
                inc  ecx
                mov  ax,  word ptr onColor
                mov  ebx, on
                mov  esi, off
                mov  edi, p
                mov  edx, phase
            b_top:
                and  edx, edx
                js   b_neg
                dec  edx
                jnz  b_next
                mov  edx, ebx
                jmp  b_next
            b_neg:
                mov  word ptr [edi], ax
                inc  edx
                jnz  b_next
                mov  edx, esi
            b_next:
                add  edi, 2
                dec  ecx
                jnz  b_top
                pop  edi
                pop  esi
        }
    } else {
        __asm { push esi
                push edi
                mov  ecx, x1
                sub  ecx, x0
                inc  ecx
                mov  ax,  word ptr onColor
                mov  ebx, on
                mov  esi, off
                mov  edi, p
                mov  edx, phase
                mov  g_lineRunCount, ecx
                mov  cx,  word ptr offColor
                push ebp
                mov  ebp, g_lineRunCount
            c_top:
                and  edx, edx
                js   c_neg
                mov  word ptr [edi], cx
                dec  edx
                jnz  c_next
                mov  edx, ebx
                jmp  c_next
            c_neg:
                mov  word ptr [edi], ax
                inc  edx
                jnz  c_next
                mov  edx, esi
            c_next:
                add  edi, 2
                dec  ebp
                jnz  c_top
                pop  ebp
                pop  edi
                pop  esi
        }
    }
    this->release_data(1);
}


void JGLBuffer::DrawDashedVLine(int x, int y0, int y1, int onColor, int offColor, int on, int off, int phase)
{
    ushort     *p;
    int         stride;
    JGLPalette *pal;
    if (x < this->get_clip()->left || x >= this->get_clip()->right)
        return;
    if (y0 == y1)
        return;
    if (onColor == -1 && offColor == -1)
        return;
    pal = 0;
    if (this->m_palette != 0)
        pal = this->m_palette;
    else if (def_palette != 0)
        pal = def_palette->pal;
    if (pal != 0) {
        switch (this->get_pixformat()->code) {
        case 0:
            if ((onColor & 0x80000000) == 0)
                onColor = pal->get_555_table()[onColor & 0xff];
            if ((offColor & 0x80000000) == 0)
                offColor = pal->get_565_table()[offColor & 0xff];
            break;
        case 1:
            if ((onColor & 0x80000000) == 0)
                onColor = pal->get_555_table()[onColor & 0xff];
            if ((offColor & 0x80000000) == 0)
                offColor = pal->get_565_table()[offColor & 0xff];
            break;
        default:
            return;
        }
    }
    onColor &= 0xffff;
    offColor &= 0xffff;
    if (y0 > y1) {
        y0 ^= y1;
        y1 ^= y0;
        y0 ^= y1;
        on ^= off;
        off ^= on;
        on ^= off;
        onColor ^= offColor;
        offColor ^= onColor;
        onColor ^= offColor;
        phase = on + off - phase;
    }
    if (y0 >= this->get_clip()->bottom || y1 < this->get_clip()->top)
        return;
    if (y0 < this->get_clip()->top)
        y0 = this->get_clip()->top;
    if (y1 >= this->get_clip()->bottom)
        y1 = this->get_clip()->bottom - 1;
    p = (ushort *)this->SURF_GetPixelPtr8(x, y0);
    if (p == 0)
        return;
    stride = this->get_pitch();
    phase = phase % (on + off);
    if (phase >= on) {
        phase = phase - on;
        phase = off - phase;
    } else {
        phase = on - phase;
        phase = -phase;
    }
    on = -on;
    if (onColor == -1) {
        __asm { push esi
                push edi
                mov  ecx, y1
                sub  ecx, y0
                inc  ecx
                mov  ax,  word ptr offColor
                mov  ebx, stride
                shl  ebx, 1
                mov  edi, p
                mov  edx, phase
            a_top:
                and  edx, edx
                js   a_neg
                mov  word ptr [edi], ax
                dec  edx
                jnz  a_next
                mov  edx, on
                jmp  a_next
            a_neg:
                inc  edx
                jnz  a_next
                mov  edx, off
            a_next:
                add  edi, ebx
                dec  ecx
                jnz  a_top
                pop  edi
                pop  esi
        }
    } else if (offColor == -1) {
        __asm { push edi
                push esi
                mov  ecx, y1
                sub  ecx, y0
                inc  ecx
                mov  ax,  word ptr onColor
                mov  ebx, stride
                mov  edi, p
                mov  edx, phase
            b_top:
                and  edx, edx
                js   b_neg
                dec  edx
                jnz  b_next
                mov  edx, on
                jmp  b_next
            b_neg:
                mov  word ptr [edi], ax
                inc  edx
                jnz  b_next
                mov  edx, off
            b_next:
                add  edi, ebx
                dec  ecx
                jnz  b_top
                pop  esi
                pop  edi
        }
    } else {
        __asm { push edi
                push esi
                mov  ecx, y1
                sub  ecx, y0
                inc  ecx
                mov  ax,  word ptr onColor
                mov  ebx, stride
                mov  edi, p
                mov  edx, phase
                mov  g_lineRunCount, ecx
                mov  cx,  word ptr offColor
                push ebp
                mov  ebp, g_lineRunCount
            c_top:
                and  edx, edx
                js   c_neg
                mov  word ptr [edi], cx
                dec  edx
                jnz  c_next
                mov  edx, on
                jmp  c_next
            c_neg:
                mov  word ptr [edi], ax
                inc  edx
                jnz  c_next
                mov  edx, off
            c_next:
                add  edi, ebx
                dec  ebp
                jnz  c_top
                pop  ebp
                pop  esi
                pop  edi
        }
    }
    this->release_data(1);
}


void JGLBuffer::fill_16(int color)
{
    int         height;
    int         width;
    ushort     *p;
    JGLPalette *pal;
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                return;
            }
        }
    }
    if (RectEqual(this->get_clip(), this->get_rect()) == 0) {
        this->fill(this->get_clip(), color);
        return;
    }
    color &= 0xffff;
    p = this->get_data_16();
    height = this->get_rect()->bottom;
    width = this->get_rect()->right;
    memsetword(p, color, width * height);
    this->release_data(1);
}

int        g_fillDstAdvanceBytes = 0;
static int s_fillSpare[2] = {0};

int JGLBuffer::fill16(Rect *pr, int color)
{
    int         rows;
    int         width;
    Rect        r;
    ushort     *dstp = 0;
    JGLPalette *pal;
    if (pr == 0) {
        this->fill_16(color);
        return 0;
    }
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                return 0x18;
            }
        }
    }
    color &= 0xffff;
    r = *pr;
    if (jgl_IntersectRect(&r, &r, this->get_clip()) == 0)
        return 0;
    dstp = this->SURF_GetPixelPtr16(r.left, r.top);
    if (dstp == 0)
        return 0;
    rows = r.bottom - r.top;
    width = r.right - r.left;
    g_fillDstAdvanceBytes = (this->get_pitch() - width) * 2;
    while (rows-- != 0) {
        memsetword(dstp, color, width);
        dstp += this->get_pitch();
    }
    this->release_data(1);
    return 0;
}


int JGLBuffer::fill_translucent_16(Rect *pr, int color, int pct)
{
    JGLPalette *pal;
    int         outR;
    int         outG;
    int         outB;
    int         sr;
    int         sg;
    int         sb;
    int         rows;
    int         inv;
    int         cols;
    int         dr;
    int         dg;
    int         db;
    int         width;
    int         rc = 0;
    Rect        r;
    ushort     *dstp;
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                rc = 0x18;
            }
        }
    }
    if (rc == 0) {
        color &= 0xffff;
        if (pr == 0)
            r = *this->get_rect();
        else
            r = *pr;
        if (jgl_IntersectRect(&r, &r, this->get_clip()) != 0) {
            dstp = this->SURF_GetPixelPtr16(r.left, r.top);
            if (dstp != 0) {
                rows = r.bottom - r.top;
                width = r.right - r.left;
                pct = (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
                pct = (pct << 8) / 100;
                inv = 256 - pct;
                dr = ExtractRed16(color) & 0xff;
                dg = ExtractGreen16(color) & 0xff;
                db = ExtractBlue16(color) & 0xff;
                while (rows-- != 0) {
                    cols = width;
                    for (; cols > 0; cols--) {
                        sr = ExtractRed16(*dstp) & 0xff;
                        sg = ExtractGreen16(*dstp) & 0xff;
                        sb = ExtractBlue16(*dstp) & 0xff;
                        outR = (dr * inv >> 8) + (sr * pct >> 8);
                        outG = (dg * inv >> 8) + (sg * pct >> 8);
                        outB = (db * inv >> 8) + (sb * pct >> 8);
                        *dstp = PackRGB16(outR, outG, outB);
                        dstp++;
                    }
                    dstp += this->get_pitch() - width;
                }
                this->release_data(1);
            }
        }
    }
    return rc;
}


int JGLBuffer::dither_16(Rect *pr, int color)
{
    JGLPalette *pal;
    int         rows;
    int         odd;
    int         cols;
    int         width;
    int         rc = 0;
    Rect        r;
    ushort     *dstp;
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                rc = 0x18;
            }
        }
    }
    if (rc == 0) {
        color &= 0xffff;
        if (pr == 0)
            r = *this->get_rect();
        else
            r = *pr;
        if (jgl_IntersectRect(&r, &r, this->get_clip()) != 0) {
            dstp = this->SURF_GetPixelPtr16(r.left, r.top);
            if (dstp != 0) {
                rows = r.bottom - r.top;
                width = r.right - r.left;
                while (rows-- != 0) {
                    cols = width;
                    if (rows % 2) {
                        dstp++;
                        odd = 1;
                        cols--;
                    } else {
                        odd = 0;
                    }
                    for (; cols > 0; cols -= 2) {
                        *dstp = (ushort)color;
                        dstp += 2;
                    }
                    dstp += this->get_pitch() - width - odd;
                }
                this->release_data(1);
            }
        }
    }
    return rc;
}


int JGLBuffer::dither_translucent_16(Rect *pr, int color, int pct)
{
    JGLPalette *pal;
    int         outR;
    int         outG;
    int         outB;
    int         sr;
    int         sg;
    int         sb;
    int         rows;
    int         odd;
    int         inv;
    int         cols;
    int         dr;
    int         dg;
    int         db;
    int         width;
    int         rc = 0;
    Rect        r;
    ushort     *dstp;
    if ((color & 0x80000000) == 0) {
        pal = 0;
        if (this->m_palette != 0)
            pal = this->m_palette;
        else if (def_palette != 0)
            pal = def_palette->pal;
        if (pal != 0) {
            switch (this->get_pixformat()->code) {
            case 0:
                color = pal->get_555_table()[color & 0xff];
                break;
            case 1:
                color = pal->get_565_table()[color & 0xff];
                break;
            default:
                rc = 0x18;
            }
        }
    }
    if (rc == 0) {
        color &= 0xffff;
        if (pr == 0)
            r = *this->get_rect();
        else
            r = *pr;
        if (jgl_IntersectRect(&r, &r, this->get_clip()) != 0) {
            dstp = this->SURF_GetPixelPtr16(r.left, r.top);
            if (dstp != 0) {
                rows = r.bottom - r.top;
                width = r.right - r.left;
                pct = (pct < 0) ? 0 : ((pct > 100) ? 100 : pct);
                pct = (pct << 8) / 100;
                inv = 256 - pct;
                dr = ExtractRed16(color) & 0xff;
                dg = ExtractGreen16(color) & 0xff;
                db = ExtractBlue16(color) & 0xff;
                while (rows-- != 0) {
                    cols = width;
                    if (rows % 2) {
                        dstp++;
                        odd = 1;
                        cols--;
                    } else {
                        odd = 0;
                    }
                    for (; cols > 0; cols -= 2) {
                        sr = ExtractRed16(*dstp) & 0xff;
                        sg = ExtractGreen16(*dstp) & 0xff;
                        sb = ExtractBlue16(*dstp) & 0xff;
                        outR = (dr * inv >> 8) + (sr * pct >> 8);
                        outG = (dg * inv >> 8) + (sg * pct >> 8);
                        outB = (db * inv >> 8) + (sb * pct >> 8);
                        *dstp = PackRGB16(outR, outG, outB);
                        dstp += 2;
                    }
                    dstp += this->get_pitch() - width - odd;
                }
                this->release_data(1);
            }
        }
    }
    return rc;
}

int        g_blitTailPixels16 = 0;
static int s_blitSpare[5] = {0};

int JGLBuffer::copy_16_16(JGLBuffer *pDst, int sx, int sy, int dx, int dy, int w, int h)
{
    ushort    *dstp;
    ushort    *srcp;
    int        dstPitch;
    int        srcPitch;
    Rect       r;
    JGLBuffer *dst = pDst;
    if (dst == 0)
        return 3;
    if (sx < 0) {
        w += sx;
        dx -= sx;
        sx = 0;
    }
    if (sy < 0) {
        h += sy;
        dy -= sy;
        sy = 0;
    }
    if (sx > this->get_width())
        return 0;
    if (sy > this->get_height())
        return 0;
    if (sx + w > this->get_width())
        w = this->get_width() - sx;
    if (sy + h > this->get_height())
        h = this->get_height() - sy;
    jgl::SetRect(&r, dx, dy, w, h);
    if (jgl_IntersectRect(&r, &r, dst->get_clip()) == 0)
        return 0;
    sx += r.left - dx;
    sy += r.top - dy;
    w = r.right - r.left;
    h = r.bottom - r.top;
    if (dst != this) {
        dstp = dst->SURF_GetPixelPtr16(r.left, r.top);
        if (dstp == 0)
            return 3;
        srcp = this->SURF_GetPixelPtr16(sx, sy);
        if (srcp == 0) {
            dst->release_data(1);
            return 3;
        }
        dstPitch = dst->m_pitch;
        srcPitch = this->m_pitch;
        __asm {
            pushf
            push edi
            push esi
            cld
            mov  edi, dstp
            mov  esi, srcp
            mov  eax, h
            mov  ebx, dstPitch
            sub  ebx, w
            shl  ebx, 1
            mov  edx, srcPitch
            sub  edx, w
            shl  edx, 1
            mov  ecx, w
            push ebp
            mov  ebp, ecx
            shr  ebp, 1
            and  ecx, 1
            mov  g_blitTailPixels16, ecx
        row0:
            mov  ecx, ebp
            rep  movsd
            mov  ecx, g_blitTailPixels16
            rep  movsw
            add  esi, edx
            add  edi, ebx
            dec  eax
            jnz  row0
            pop  ebp
            pop  esi
            pop  edi
            popf
        }
    } else if (dx < sx) {
        if (dy < sy) {
            dstp = dst->SURF_GetPixelPtr16(r.left, r.top);
            if (dstp == 0)
                return 3;
            srcp = this->SURF_GetPixelPtr16(sx, sy);
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                cld
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                sub  ebx, w
                shl  ebx, 1
                mov  edx, srcPitch
                sub  edx, w
                shl  edx, 1
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 1
                and  ecx, 1
                mov  g_blitTailPixels16, ecx
            row1:
                mov  ecx, ebp
                rep  movsd
                mov  ecx, g_blitTailPixels16
                rep  movsw
                add  esi, edx
                add  edi, ebx
                dec  eax
                jnz  row1
                pop  ebp
                pop  esi
                pop  edi
                popf
            }
        } else {
            dstp = dst->SURF_GetPixelPtr16(r.left, r.top + h - 1);
            if (dstp == 0)
                return 3;
            srcp = this->SURF_GetPixelPtr16(sx, sy + h - 1);
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                cld
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                add  ebx, w
                shl  ebx, 1
                mov  edx, srcPitch
                add  edx, w
                shl  edx, 1
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 1
                and  ecx, 1
                mov  g_blitTailPixels16, ecx
            row2:
                mov  ecx, ebp
                rep  movsd
                mov  ecx, g_blitTailPixels16
                rep  movsw
                sub  esi, edx
                sub  edi, ebx
                dec  eax
                jnz  row2
                pop  ebp
                pop  esi
                pop  edi
                popf
            }
        }
    } else {
        if (dy < sy) {
            if (w < 4) {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 1, r.top);
                srcp = this->SURF_GetPixelPtr16(sx + w - 1, sy);
            } else {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 2, r.top);
                srcp = this->SURF_GetPixelPtr16(sx + w - 2, sy);
            }
            if (dstp == 0)
                return 3;
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                std
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                add  ebx, w
                shl  ebx, 1
                mov  edx, srcPitch
                add  edx, w
                shl  edx, 1
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 1
                and  ecx, 1
                mov  g_blitTailPixels16, ecx
                cmp  g_blitTailPixels16, 0
                jnz  odd3
            even3:
                mov  ecx, ebp
                rep  movsd
                add  esi, edx
                add  edi, ebx
                dec  eax
                jnz  even3
                jmp  done3
            odd3:
                mov  ecx, ebp
                rep  movsd
                mov  cx, word ptr [esi+2]
                mov  word ptr [edi+2], cx
                add  esi, edx
                add  edi, ebx
                dec  eax
                jnz  odd3
            done3:
                pop  ebp
                pop  esi
                pop  edi
                popf
                cld
            }
        } else {
            if (w < 4) {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 1, r.top + h - 1);
                srcp = this->SURF_GetPixelPtr16(sx + w - 1, sy + h - 1);
            } else {
                dstp = dst->SURF_GetPixelPtr16(r.left + w - 2, r.top + h - 1);
                srcp = this->SURF_GetPixelPtr16(sx + w - 2, sy + h - 1);
            }
            if (dstp == 0)
                return 3;
            if (srcp == 0) {
                dst->release_data(1);
                return 3;
            }
            dstPitch = dst->m_pitch;
            srcPitch = this->m_pitch;
            __asm {
                pushf
                push edi
                push esi
                std
                mov  edi, dstp
                mov  esi, srcp
                mov  eax, h
                mov  ebx, dstPitch
                sub  ebx, w
                shl  ebx, 1
                mov  edx, srcPitch
                sub  edx, w
                shl  edx, 1
                mov  ecx, w
                push ebp
                mov  ebp, ecx
                shr  ebp, 1
                and  ecx, 1
                mov  g_blitTailPixels16, ecx
                cmp  g_blitTailPixels16, 0
                jnz  odd4
            even4:
                mov  ecx, ebp
                rep  movsd
                sub  esi, edx
                sub  edi, ebx
                dec  eax
                jnz  even4
                jmp  done4
            odd4:
                mov  ecx, ebp
                rep  movsd
                mov  cx, word ptr [esi+2]
                mov  word ptr [edi+2], cx
                sub  esi, edx
                sub  edi, ebx
                dec  eax
                jnz  odd4
            done4:
                pop  ebp
                pop  esi
                pop  edi
                popf
                cld
            }
        }
    }
    this->release_data(1);
    dst->release_data(1);
    return 0;
}

int g_fill8DstSkip = 0;
