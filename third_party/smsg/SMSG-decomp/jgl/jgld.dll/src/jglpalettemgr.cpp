#include "jgldefs.h"
#include "jglwin.h"
#include "jglpalettetypes.h"
#include "jglglobals.h"
#include "jgltypes.h"
struct Quad {
    uchar rgbBlue, rgbGreen, rgbRed, rgbReserved;
};
const Quad system_static_colors[20] = {
    {0x00, 0x00, 0x00, 0}, {0x00, 0x00, 0x80, 0}, {0x00, 0x80, 0x00, 0}, {0x00, 0x80, 0x80, 0}, {0x80, 0x00, 0x00, 0},
    {0x80, 0x00, 0x80, 0}, {0x80, 0x80, 0x00, 0}, {0xc0, 0xc0, 0xc0, 0}, {0xc0, 0xdc, 0xc0, 0}, {0xf0, 0xca, 0xa6, 0},
    {0xf0, 0xfb, 0xff, 0}, {0xa4, 0xa0, 0xa0, 0}, {0x80, 0x80, 0x80, 0}, {0x00, 0x00, 0xff, 0}, {0x00, 0xff, 0x00, 0},
    {0x00, 0xff, 0xff, 0}, {0xff, 0x00, 0x00, 0}, {0xff, 0x00, 0xff, 0}, {0xff, 0xff, 0x00, 0}, {0xff, 0xff, 0xff, 0},
};
#include "jglpalette.h"
#include "jglpalettemgr.h"
extern "C" {
void *g_hPalette = 0;
}
PaletteBase *def_palette = 0;
PaletteBase *def_text_palette = 0;
#include "jglalloc.h"
#include "random.h"
#include "gsobject.h"
#include "gspalette.h"
JGLPalette::JGLPalette(PaletteBase *client)
{
    m_client = client;
    m_palHeader = 0;
    m_palVersion = 0;
    m_lutVersion = 0;
}
GSPalette::GSPalette() {}
JGLPalette::~JGLPalette()
{
    if (m_client != 0)
        m_client->pal = 0;
    close();
}
void JGLPalette::close()
{
    m_palVersion = 0;
    m_lutVersion = 0;
}
void JGLPalette::reseed()
{
    PRNG rng;
    m_palVersion = 0;
    QueryPerformanceCounter((LARGE_INTEGER *)&m_seedTime);
    rng.Seed(m_seedTime.LowPart);
    while (m_palVersion == 0)
        m_palVersion = rng.get(0xffff);
    compute_tables();
}
void JGLPalette::compute_tables()
{
    int i;
    if (m_lutVersion == m_palVersion)
        return;
    m_lutVersion = m_palVersion;
    for (i = 0; i < 256; i++) {
        m_lut555[i] = ((m_palEntries[i].peBlue >> 3) & 0x1f) | ((m_palEntries[i].peGreen << 2) & 0x3e0)
                      | ((m_palEntries[i].peRed << 7) & 0xfc00);
        m_lut565[i] = ((m_palEntries[i].peBlue >> 3) & 0x1f) | ((m_palEntries[i].peGreen << 3) & 0x7e0)
                      | ((m_palEntries[i].peRed << 8) & 0xf800);
    }
}
int JGLPalette::init(void)
{
    int i;
    close();
    for (i = 0; i < 10; i += 1) {
        m_palEntries[i].peRed = system_static_colors[i].rgbRed;
        m_palEntries[i].peGreen = system_static_colors[i].rgbGreen;
        m_palEntries[i].peBlue = system_static_colors[i].rgbBlue;
        m_palEntries[i].peFlags = 4;
        m_palEntries[i + 246].peRed = system_static_colors[i + 10].rgbRed;
        m_palEntries[i + 246].peGreen = system_static_colors[i + 10].rgbGreen;
        m_palEntries[i + 246].peBlue = system_static_colors[i + 10].rgbBlue;
        m_palEntries[i + 246].peFlags = 4;
    }
    for (i = 10; i < 246; i += 1) {
        m_palEntries[i].peRed = (uchar)i;
        m_palEntries[i].peGreen = (uchar)i;
        m_palEntries[i].peBlue = (uchar)i;
        m_palEntries[i].peFlags = 5;
    }
    for (i = 0; i < 2; i += 1) {
        m_palEntries[i + 8].peRed = (uchar)(i + 8);
        m_palEntries[i + 8].peGreen = 0;
        m_palEntries[i + 8].peBlue = 0;
        m_palEntries[i + 8].peFlags = 2;
        m_palEntries[i + 246].peRed = (uchar)(i + 245);
        m_palEntries[i + 246].peGreen = 0;
        m_palEntries[i + 246].peBlue = 0;
        m_palEntries[i + 246].peFlags = 2;
    }
    reseed();
    return 0;
}
void JGLPalette::set()
{
    if (g_hPalette == 0)
        return;
    AnimatePalette((HPALETTE)g_hPalette, 10, 236, &m_palEntries[10]);
}
int JGLPalette::get_entries(unsigned char *dst, int start, int count)
{
    for (int i = 0; i < count; i += 1) {
        dst[i * 3] = m_palEntries[i + start].peRed;
        dst[i * 3 + 1] = m_palEntries[i + start].peGreen;
        dst[i * 3 + 2] = m_palEntries[i + start].peBlue;
    }
    return 0;
}
int JGLPalette::set_entries(const unsigned char *rgb, int first, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        m_palEntries[i + first].peRed = rgb[i * 3];
        m_palEntries[i + first].peGreen = rgb[i * 3 + 1];
        m_palEntries[i + first].peBlue = rgb[i * 3 + 2];
    }
    reseed();
    return 0;
}
int JGLPalette::init_class(PaletteBase *tr, PaletteBase *tr2)
{
    int            i;
    LogPalette256 *lp;
    close_class();
    lp = (LogPalette256 *)jgl_malloc(sizeof(LogPalette256));
    if (lp == 0) {
        return 4;
    }
    lp->palVersion = 0x300;
    lp->palNumEntries = 0x100;
    for (i = 0; i < 10; i += 1) {
        lp->palPalEntry[i].peRed = system_static_colors[i].rgbRed;
        lp->palPalEntry[i].peGreen = system_static_colors[i].rgbGreen;
        lp->palPalEntry[i].peBlue = system_static_colors[i].rgbBlue;
        lp->palPalEntry[i].peFlags = 0;
        lp->palPalEntry[i + 246].peRed = system_static_colors[i + 10].rgbRed;
        lp->palPalEntry[i + 246].peGreen = system_static_colors[i + 10].rgbGreen;
        lp->palPalEntry[i + 246].peBlue = system_static_colors[i + 10].rgbBlue;
        lp->palPalEntry[i + 246].peFlags = 0;
    }
    for (i = 10; i < 246; i += 1) {
        lp->palPalEntry[i].peFlags = 5;
    }
    for (i = 0; i < 2; i += 1) {
        lp->palPalEntry[i + 8].peRed = (uchar)(i + 8);
        lp->palPalEntry[i + 8].peGreen = 0;
        lp->palPalEntry[i + 8].peBlue = 0;
        lp->palPalEntry[i + 8].peFlags = 2;
        lp->palPalEntry[i + 246].peRed = (uchar)(i + 245);
        lp->palPalEntry[i + 246].peGreen = 0;
        lp->palPalEntry[i + 246].peBlue = 0;
        lp->palPalEntry[i + 246].peFlags = 2;
    }
    g_hPalette = CreatePalette((const LOGPALETTE *)lp);
    if (g_hPalette == 0) {
        jgl_free(lp);
        lp = 0;
        return 1;
    }
    jgl_free(lp);
    lp = 0;
    tr->init();
    tr2->init();
    def_palette = tr;
    def_text_palette = tr2;
    return 0;
}
void JGLPalette::close_class(void)
{
    if (g_hPalette != 0) {
        DeleteObject(g_hPalette);
        g_hPalette = 0;
    }
}
PALETTEENTRY *JGLPalette::get_entry(int index)
{
    static PALETTEENTRY entry;
    entry.peRed = m_palEntries[index].peRed;
    entry.peGreen = m_palEntries[index].peGreen;
    entry.peBlue = m_palEntries[index].peBlue;
    return &entry;
}
void JGLPalette::fill_rgb(RgbQuad *dst)
{
    for (int i = 0; i < 256; i += 1) {
        dst[i].rgbRed = m_palEntries[i].peRed;
        dst[i].rgbGreen = m_palEntries[i].peGreen;
        dst[i].rgbBlue = m_palEntries[i].peBlue;
        dst[i].rgbReserved = 0;
    }
}
GSPalette::~GSPalette() {}
unsigned short *JGLPalette::get_555_table()
{
    return m_lut555;
}
unsigned short *JGLPalette::get_565_table()
{
    return m_lut565;
}
