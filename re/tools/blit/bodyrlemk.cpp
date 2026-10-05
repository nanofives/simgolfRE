#define _DEBUG
#include <crtdbg.h>
struct Row { unsigned char start; unsigned char len; unsigned short off; };   // per-row run: skip, length, byte offset
extern float g_101285b8;                     // horizontal scale
extern float g_101285b4;                     // vertical scale
extern int g_101285f4;                       // source pitch
extern int g_101285dc;                       // destination pitch (pixels)
extern int g_101285b0;                       // mask pitch
extern unsigned short* g_10128600;           // palette table
extern int g_101285f0;                       // rows
extern int g_101285e4;                       // columns
extern int g_101285bc;
extern int g_101285e0;
extern int g_101285ac;
// ----
// MATCH: jgld.dll ADDR ?NAME@Sprite@@QAEHFDECOR@Z
int Sprite::NAME(FPARAMS)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned char* m;
    unsigned char* s;
    Pal16* pal;
    int i;
    int j;
    int dx;
    unsigned char c;
    Row* tbl;
    int offX;
    int offR;
    int w;
    int n;
    // padding: with /ZI each _RPTF0 line is __LINE__ relative to the function's opening brace, and the
    // original reports sit at base+0x63/0x67/0x69
















    if (m_bits == 0 || dst == 0 || mask == 0)
        return 3;
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0)
        return 16;
    g_101285b8 = (float)abs(g_10122dc0) / g_10122dc8;
    g_101285b4 = (float)abs(g_10122dc4) / g_10122dc8;
    m_28 = (unsigned char)m_28;
    g_101285f4 = m_pitch;
    g_101285dc = dst->pitch();
    g_101285b0 = mask->pitch();
    switch (dst->format()[1]) {
    case 0:
        g_10128600 = pal->table565();
        break;
    case 1:
        g_10128600 = pal->table555();
        break;
    default:
        return 1;
    }
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!intersect(&rc, &rc, dst->boundsRect())) {
            mask->unlock(1);
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101285dc;
                m = mask->bits8() + rc.left + rc.top * g_101285b0;
                g_101285f0 = rc.bottom - rc.top;
                g_101285e4 = rc.right - rc.left;
                g_101285bc = g_101285f4 - g_101285e4;
                g_101285e0 = g_101285dc - g_101285e4;
                g_101285ac = g_101285b0 - g_101285e4;
                offX = rc.left - orig.left;
                offR = orig.right - rc.right;
                tbl = (Row*)m_1c + (rc.top - orig.top);
                for (i = 0; i < g_101285f0; i++) {
                    w = m_width - tbl[i].start - tbl[i].len;
                    dx = (tbl[i].start - offX < 0) ? 0 : tbl[i].start - offX;
                    s = m_bits + tbl[i].off + ((offX - tbl[i].start < 0) ? 0 : offX - tbl[i].start);
                    n = tbl[i].len - ((offX - tbl[i].start < 0) ? 0 : offX - tbl[i].start) - ((offR - w < 0) ? 0 : offR - w);
                    for (j = 0; j < n; j++) {
                        c = *s;
                        if (c != 0xff) {
                            if (m[dx] > level) {
                                if (c != 0xfe)
                                    d[dx] = g_10128600[c];
                            } else if (c == 0xfe)
                                d[dx] = color;
                        }
                        dx++;
                        s++;
                    }
                    d += g_101285dc;
                    m += g_101285dc;
                }
            } else {
                _RPTF0(_CRT_WARN, "NOT WRITTEN FOR COMPRESSION");
            }
        } else {
            if (g_10122dc4 >= 0) {
                _RPTF0(_CRT_WARN, "NOT WRITTEN FOR COMPRESSION");
            } else {
                _RPTF0(_CRT_WARN, "NOT WRITTEN FOR COMPRESSION");
            }
        }
    }
    dst->unlock(1);
    mask->unlock(1);
    return 0;
}
