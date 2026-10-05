// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll run-length sprite blitter onto 16-bit surfaces (source jglsprite_8_16c.cpp): per-row {skip, length, offset} table at
// Sprite+0x1c, all C (no __asm), _RPTF0 'not written for compression' for mirrored cases. Names are chosen here.
#include <windows.h>
#include <stdlib.h>
class Pal16 {
public:
    virtual void p0(); virtual void p1(); virtual void p2(); virtual void p3(); virtual void p4(); virtual void p5();
    virtual unsigned short* table565();      // slot 6 (+0x18)
    virtual unsigned short* table555();      // slot 7 (+0x1c)
};
struct PalClient { int m_0; Pal16* m_palette; };
extern PalClient* g_palClient1;              // 0x1012873c
int intersect(RECT* out, const RECT* a, const RECT* b);   // 0x10008590
void setRect(RECT* r, int x, int y, int w, int h);        // 0x10008360
class Surface {
public:
    virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3();
    virtual int lock();                      // slot 4 (+0x10)
    virtual void s5();
    virtual unsigned char* bits8();          // slot 6 (+0x18)
    virtual void s7();
    virtual void* bits();                    // slot 8 (+0x20)
    virtual void unlock(int flag);           // slot 9 (+0x24)
    virtual void t10(); virtual void t11(); virtual void t12(); virtual void t13(); virtual void t14(); virtual void t15(); virtual void t16(); virtual void t17(); virtual void t18(); virtual void t19(); virtual void t20(); virtual void t21(); virtual void t22(); virtual void t23(); virtual void t24(); virtual void t25(); virtual void t26(); virtual void t27(); virtual void t28(); virtual void t29(); virtual void t30(); virtual void t31(); virtual void t32(); virtual void t33(); virtual void t34(); virtual void t35(); virtual void t36(); virtual void t37(); virtual void t38(); virtual void t39(); virtual void t40(); virtual void t41(); virtual void t42(); virtual void t43(); virtual void t44(); virtual void t45(); virtual void t46(); virtual void t47(); virtual void t48(); virtual void t49(); virtual void t50();
    virtual RECT* boundsRect();              // slot 51 (+0xcc)
    virtual void t52(); virtual void t53(); virtual void t54(); virtual void t55();
    virtual int pitch();                     // slot 56 (+0xe0)
    virtual int* format();                   // slot 57 (+0xe4)
};
extern int g_10122dc0;                       // horizontal scale numerator (sign = mirror)
extern int g_10122dc4;                       // vertical scale numerator (sign = mirror)
extern int g_10122dc8;                       // scale denominator
extern int* g_10122e20;
extern int* g_10122e1c;
extern float g_1012858c;
extern float g_10128590;
extern int g_1012857c;                       // source pitch
extern int g_1012855c;                       // destination pitch
extern unsigned short* g_1012856c;           // palette table
extern int g_10128578;                       // rows
extern int g_10128564;                       // columns
extern int g_1012853c;                       // source row skip
extern int g_10128560;                       // destination row skip (bytes)
extern unsigned g_10128588;                  // 16.16 horizontal step
extern unsigned g_10128540;                  // 16.16 vertical step
extern unsigned g_10128554;
extern unsigned g_10128558;
extern unsigned g_10128570;
extern unsigned g_10128584;
extern int g_10128580;
extern int g_10128544;
extern unsigned g_10128664;                  // 8-bit fill colour
extern unsigned char* g_1012862c;               // 8-bit blend table
extern int* g_10122e58;
extern int* g_10122e54;
extern float g_1012868c;
extern float g_10128690;
extern int g_10128678;
extern int g_10128644;
extern int g_10128670;
extern int g_10128650;
extern int g_1012861c;
extern int g_10128648;
extern unsigned g_10128688;
extern unsigned g_10128624;
extern unsigned g_1012863c;
extern unsigned g_10128640;
extern unsigned g_10128660;
extern unsigned g_10128680;
extern int g_10128548;
extern int g_10128574;
extern int g_1012866c;
extern unsigned short g_10128530;            // 16-bit fill colour
#define _DEBUG
#include <crtdbg.h>
struct Row { unsigned char start; unsigned char len; unsigned short off; };   // per-row run: skip, length, byte offset
extern float g_101285b8;                     // horizontal scale
extern float g_101285b4;                     // vertical scale
extern int g_101285f4;                       // source pitch
extern int g_101285dc;                       // destination pitch (pixels)
extern unsigned short* g_101285ec;           // 5th parameter: table indexed by the destination pixel (codes >= 0xf8)
extern unsigned short* g_101285c8;           // 4th parameter: table indexed by the palette entry
extern unsigned short* g_10128600;           // palette table
extern int g_101285f0;                       // rows
extern int g_101285e4;                       // columns
extern int g_101285bc;
extern int g_101285e0;
extern int g_101285fc;                       // 16.16 horizontal step
extern int g_101285c0;                       // 16.16 vertical step
extern unsigned g_101285d8;                  // 16.16 source row
extern unsigned g_101285d4;                  // 16.16 source column
class Sprite {
public:
    virtual ~Sprite();
    int drawRle16(Surface* dst, int x, int y, unsigned short* p4, unsigned short* p5);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1005d1f0 ?drawRle16@Sprite@@QAEHPAVSurface@@HHPAG1@Z
int Sprite::drawRle16(Surface* dst, int x, int y, unsigned short* p4, unsigned short* p5)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
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
    unsigned ry;
    int sx;
    // padding: with /ZI each _RPTF0 line is __LINE__ relative to the function's opening brace, and the
    // original reports sit at base+0x5c/0x60/0x62 (unscaled) and base+0xa9/0xad/0xaf (scaled)












    if (m_bits == 0 || dst == 0)
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
    g_101285ec = p5;
    g_101285c8 = p4;
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
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101285dc;
                g_101285f0 = rc.bottom - rc.top;
                g_101285e4 = rc.right - rc.left;
                g_101285bc = g_101285f4 - g_101285e4;
                g_101285e0 = g_101285dc - g_101285e4;
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
                            if (c >= 0xf8)
                                d[dx] = g_101285ec[d[j]];
                            else
                                d[dx] = g_101285c8[g_10128600[c]];
                        }
                        dx++;
                        s++;
                    }
                    d += g_101285dc;
                }
            } else {
                _RPTF0(_CRT_WARN, "report text (placeholder)");
            }
        } else {
            if (g_10122dc4 >= 0) {
                _RPTF0(_CRT_WARN, "report text (placeholder)");
            } else {
                _RPTF0(_CRT_WARN, "report text (placeholder)");
            }
        }
    } else {
        setRect(&rc, x, y, (int)(m_width * g_101285b8), (int)(m_height * g_101285b4));
        orig = rc;
        if (!intersect(&rc, &rc, dst->boundsRect())) {
            dst->unlock(1);
            return 0;
        }
        d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101285dc;
        g_101285dc = dst->pitch();
        g_101285e4 = rc.right - rc.left;
        g_101285f0 = rc.bottom - rc.top;
        g_101285e0 = (g_101285dc - g_101285e4) * 2;
        g_101285fc = (g_10122dc8 << 16) / abs(g_10122dc0);
        g_101285c0 = (g_10122dc8 << 16) / abs(g_10122dc4);



















        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101285dc;
                g_101285f0 = rc.bottom - rc.top;
                g_101285e4 = rc.right - rc.left;
                offX = rc.left - orig.left;
                g_101285d8 = g_101285c0 * (rc.top - orig.top);
                tbl = (Row*)m_1c;
                for (i = 0; i < g_101285f0; i++) {
                    ry = g_101285d8 >> 16;
                    if (tbl[ry].len == 0)
                        continue;
                    g_101285d4 = offX * g_101285fc;
                    s = m_bits + tbl[ry].off;
                    for (j = 0; j < g_101285e4; j++) {
                        sx = (g_101285d4 >> 16) - tbl[ry].start;
                        if (sx < 0) {
                            g_101285d4 += g_101285fc;
                            continue;
                        }
                        if (sx > tbl[ry].len)
                            break;
                        c = s[sx];
                        if (c != 0xff) {
                            if (c >= 0xf8)
                                d[j] = g_101285ec[d[j]];
                            else
                                d[j] = g_101285c8[g_10128600[c]];
                        }
                        g_101285d4 += g_101285fc;
                    }
                    g_101285d8 += g_101285c0;
                    d += g_101285dc;
                }
            } else {
                _RPTF0(_CRT_WARN, "report text (placeholder)");
            }
        } else {
            if (g_10122dc4 >= 0) {
                _RPTF0(_CRT_WARN, "report text (placeholder)");
            } else {
                _RPTF0(_CRT_WARN, "report text (placeholder)");
            }
        }
    }
    dst->unlock(1);
    return 0;
}

