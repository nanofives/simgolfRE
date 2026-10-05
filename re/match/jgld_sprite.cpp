// FLAGS jgld.dll: /Od /ZI /GZ /GX
// jgld.dll sprites (c:/jackdev/jglsprite.cpp, debug build). The _RPTF0 reports of <crtdbg.h> carry __LINE__,
// which /ZI compiles as a per-function base plus the line's distance from the function's `{`: the blank lines
// below keep each report at the original distance (written with log/padlines.py `@N` markers). Report texts are
// placeholders. Class, member and slot names are chosen here. 8-bit sprites can be stored as per-row spans of
// non-key pixels (RowSpan, m_1c; flag bit 0 of m_18) when g_101284c4 enables it.
#define _DEBUG
#include <windows.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <string.h>
#include <malloc.h>
struct RowSpan { unsigned char start; unsigned char len; short off; };
extern int g_101284c4;
extern int g_101284c8;
extern int g_101284cc;
void jfreeDbg(void* p);          // 0x100806d0
class Surface { public: virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4(); virtual void s5(); virtual void s6(); virtual void s7(); virtual void s8(); virtual void s9(); virtual void s10(); virtual void s11(); virtual void s12(); virtual void s13(); virtual void s14(); virtual void s15(); virtual void s16(); virtual void s17(); virtual void s18(); virtual void s19(); virtual void s20(); virtual void s21(); virtual void s22(); virtual void s23(); virtual void s24(); virtual void s25(); virtual void s26(); virtual void s27(); virtual void s28(); virtual void s29(); virtual void s30(); virtual void s31(); virtual void s32(); virtual void s33(); virtual void s34(); virtual void s35(); virtual void s36(); virtual void s37(); virtual void s38(); virtual void s39(); virtual void s40(); virtual void s41(); virtual void s42(); virtual void s43(); virtual void s44(); virtual void s45(); virtual void s46(); virtual void s47(); virtual void s48(); virtual void s49(); virtual void s50(); virtual void s51(); virtual void s52(); virtual void s53(); virtual void s54(); virtual void s55(); virtual void s56(); virtual int* format(); };
class Sprite {
public:
    virtual void v0(); virtual void v1(); virtual void clear(); virtual void v3();
    virtual void setKey(int k);  // slot 4 (+0x10)
    virtual void v5(); virtual void v6(); virtual void v7();
    virtual int isValid();       // slot 8 (+0x20)
    virtual void unlock(int f);  // slot 9 (+0x24)
    virtual int bpp();           // slot 10 (+0x28)
    virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16();
    virtual int drawV(Surface* dst, int x, int y, int key);   // slot 17 (+0x44)
    void release();
    Sprite& copyFrom(Sprite* o);
    int isBusy();                                   // 0x10001118 (thunk)
    int draw(Surface* dst, int x, int y, int key);
    int draw8to8(Surface* dst, int x, int y);       // 0x100016fe (thunk)
    int draw8to16(Surface* dst, int x, int y);      // 0x10001168 (thunk)
    int drawB(Surface* dst, int x, int y, int key);
    int drawB8to8(Surface* dst, int x, int y);      // 0x100016cc (thunk)
    int drawB8to16busy(Surface* dst, int x, int y); // 0x100015b4 (thunk)
    int drawB8to16(Surface* dst, int x, int y);     // 0x10001a50 (thunk)
    int drawB16(Surface* dst, int x, int y);        // 0x1000155a (thunk)
    int drawE(Surface* dst, int x, int y, int z, int key);
    int drawE8(Surface* dst, int x, int y, int z);      // 0x100015eb
    int drawE16busy(Surface* dst, int x, int y, int z); // 0x10001a0a
    int drawE16(Surface* dst, int x, int y, int z);     // 0x10001221
    int drawF(Surface* dst, int x, int y, int z, int key);
    int drawF8(Surface* dst, int x, int y, int z);      // 0x10001401
    int drawG(Surface* dst, int x, int y, int z, int key);
    int drawG16busy(Surface* dst, int x, int y, int z); // 0x10001514
    int drawG16(Surface* dst, int x, int y, int z);     // 0x100017fd
    int drawG2(Surface* dst, int x, int y, int z);      // 0x10001a82
    int drawH(Surface* dst, int x, int y, int z, int key);
    int drawH8(Surface* dst, int x, int y, int z);      // 0x100010af
    int drawH16(Surface* dst, int x, int y, int z);     // 0x100014c4
    int drawI(Surface* dst, int x, int y, int z, int key);
    int drawI16busy(Surface* dst, int x, int y, int z); // 0x1000146a
    int drawI16(Surface* dst, int x, int y, int z);     // 0x100014bf
    int drawJ(Surface* dst, int x, int y, int z, int w, int key);
    int drawJ8(Surface* dst, int x, int y, int z, int w);      // 0x1000159b
    int drawK(Surface* dst, int x, int y, int z, int w, int key);
    int drawK16busy(Surface* dst, int x, int y, int z, int w); // 0x1000142e
    int drawK16(Surface* dst, int x, int y, int z, int w);     // 0x10001b0e
    int lineTo(Surface* dst, int* p2, int a, int b, int c, int key);
    int lineTo8(Surface* dst, int a, int b, int c, int* p2);   // 0x1002cec0
    int dashTo(Surface* dst, int* p2, int a, int b, int c, int d, int e, int f, int key);
    int dashTo8(Surface* dst, int a, int b, int c, int e, int f, int* p2);   // 0x1002d390
    int blt16a(Surface* dst, int a, int b, int c, int key);
    int blt16a8(Surface* dst, int a, int b, int c);        // 0x100298b0
    int blt16b(Surface* dst, int a, int b, unsigned short c, int key, char d);
    int blt16b8(Surface* dst, int a, int b, unsigned short c, char d);   // 0x10022a10
    int blt16c(Surface* dst, int a, int b, unsigned short c, int key, char d);
    int blt16c8(Surface* dst, int a, int b, unsigned short c, char d);   // 0x10024140
    int bltS(Sprite* s, Surface* dst, int a, int b, int key);
    int bltS8(Sprite* s, Surface* dst, int a, int b);              // 0x10025dd0
    int bltS2(Sprite* s, Surface* dst, int a, int b, int c, int key);
    int bltS28(Sprite* s, Surface* dst, int a, int b, int c);      // 0x10025900
    int drawL(Surface* dst, int x, int y, int z, int key);
    int drawL8(Surface* dst, int x, int y, int z);      // 0x100012f8
    int drawM(Surface* dst, int x, int y, int z, int key);
    int drawM8(Surface* dst, int x, int y, int z);      // 0x10001744
    int drawM16(Surface* dst, int x, int y, int z);     // 0x10001915
    int drawP(Surface* dst, int x, int y, int pct, int key);
    int drawP8_50(Surface* dst, int x, int y);   // 0x10001a69
    int drawP8_25(Surface* dst, int x, int y);   // 0x1000139d
    int drawP16_50(Surface* dst, int x, int y);  // 0x10001807
    int drawP16_25(Surface* dst, int x, int y);  // 0x10001613
    int drawN(Surface* dst, int x, int y, int z, int key);
    int drawNA(Surface* dst, int x, int y, int z);   // 0x1000197e
    int drawNB(Surface* dst, int x, int y, int z);   // 0x1000141f
    int drawO(Surface* dst, int x, int y, int z, int key);
    int drawOA(Surface* dst, int x, int y, int z, int e);   // 0x100010a0
    int drawOB(Surface* dst, int x, int y, int z);   // 0x10001663
    int drawC(Surface* dst, int p1, int p2, int p3, char p4, int p5, int key);
    int drawCA(Surface* dst, int p1, int p2, int p3, char p4, int p5);   // 0x1000182f
    int drawCB(Surface* dst, int p1, int p2, int p3, char p4, int p5);   // 0x1000140b
    int drawD(Surface* dst, int p1, int p2, int p3, char p4, int key);
    int drawDA(Surface* dst, int p1, int p2, int p3, char p4);   // 0x100019b5
    int drawDB(Surface* dst, int p1, int p2, int p3, char p4);   // 0x10001a78
    int create(void* bits, unsigned a, int b, int bpp, int e, int f);
    int create8(void* bits, unsigned w, int h, int bpp, int e);
    int create16(void* bits, unsigned w, int h, int bpp, int e);
    int remap(int a, int b);
    int remap8(int a, int b);                       // 0x1001bae0
    int m_4, m_8, m_c;
    int m_10;
    void* m_14;
    unsigned m_18;
    RowSpan* m_1c;
    int m_20;
    unsigned m_24;
    int m_28, m_2c, m_30, m_34;
};
// MATCH: jgld.dll 0x10014550 ?trace@@YAXHPBD@Z
void trace(int level, const char* s)
// FN
{
    OutputDebugStringA(s);
}
// MATCH: jgld.dll 0x10014a40 ?jalloc@@YAPAXI@Z
void* jalloc(size_t n)
// FN
{
    return malloc(n);
}
// MATCH: jgld.dll 0x10014a90 ?jfree@@YAHPAX@Z
int jfree(void* p)
// FN
{
    if (p == 0)
        return 0;
    jfreeDbg(p);
    return 0;
}
// MATCH: jgld.dll 0x10014eb0 ?release@Sprite@@QAEXXZ
void Sprite::release()
// FN
{
    if (m_14) {
        if (m_18 & 1)
            g_101284cc -= m_30 * m_34 - m_24 - m_34 * 4;
        g_101284c8 -= m_30 * m_34;
        jfree(m_14);
        m_14 = 0;
    }
    if (m_1c) {
        jfree(m_1c);
        m_1c = 0;
    }
    m_2c = 0;
    m_30 = 0;
    m_34 = 0;
    m_18 = 0;
    m_24 = 0;
}
// MATCH: jgld.dll 0x10014ff0 ?create@Sprite@@QAEHPAXIHHHH@Z
int Sprite::create(void* bits, unsigned a, int b, int bpp, int e, int f)
// FN
{
    int r;
    switch (bpp) {
    case 8:
        r = create8(bits, a, b, bpp, e);
        break;
    case 0x10:
        r = create16(bits, a, b, bpp, e);
        break;
    }
    if (r != 0)
        return r;
    setKey(f);
    return 0;
}
// MATCH: jgld.dll 0x100150d0 ?remap@Sprite@@QAEHHH@Z
int Sprite::remap(int a, int b)
// FN
{
    switch (m_20) {
    case 8:
        return remap8(a, b);
    default:




     _RPTF0(_CRT_WARN, "");
        return 0x17;
    }
}
// MATCH: jgld.dll 0x10015180 ?draw@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw(Surface* dst, int x, int y, int key)
// FN
{
    int saved;
    int r;
    if (dst == 0 || !isValid())
        return 7;
    saved = m_10;
    if (key)
        m_10 = key;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else
                r = draw8to8(dst, x, y);
            break;
        case 0x10:
            if (isBusy())

          _RPTF0(_CRT_WARN, "");
            else
                r = draw8to16(dst, x, y);
            break;
        default:




      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        default:






      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10015480 ?drawB@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::drawB(Surface* dst, int x, int y, int key)
// FN
{
    int saved;
    int r;
    if (dst == 0 || !isValid())
        return 7;
    saved = m_10;
    if (key)
        m_10 = key;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else
                r = drawB8to8(dst, x, y);
            break;
        case 0x10:
            if (isBusy())
                r = drawB8to16busy(dst, x, y);
            else
                r = drawB8to16(dst, x, y);
            break;
        default:




      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:
        case 0x10:
            if (isBusy())


          _RPTF0(_CRT_WARN, "");
            else
                r = drawB16(dst, x, y);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x100157e0 ?drawC@Sprite@@QAEHPAVSurface@@HHHDHH@Z
int Sprite::drawC(Surface* dst, int p1, int p2, int p3, char p4, int p5, int key)
// FN
{
    int saved;
    int r;
    if (dst == 0 || !isValid())
        return 7;
    saved = m_10;
    if (key)
        m_10 = key;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else {
          _RPTF0(_CRT_WARN, "");
                r = 0x17;
            }
            break;
        case 0x10:
            if (isBusy())
                r = drawCA(dst, p1, p2, p3, p4, p5);
            else
                r = drawCB(dst, p1, p2, p3, p4, p5);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:
        case 0x10:
            if (isBusy())


          _RPTF0(_CRT_WARN, "");
            else {
          _RPTF0(_CRT_WARN, "");
                r = 0x17;
            }
            break;
        default:


      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10015bb0 ?drawD@Sprite@@QAEHPAVSurface@@HHHDH@Z
int Sprite::drawD(Surface* dst, int p1, int p2, int p3, char p4, int key)
// FN
{
    int saved;
    int r;
    if (dst == 0 || !isValid())
        return 7;
    saved = m_10;
    if (key)
        m_10 = key;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else {
          _RPTF0(_CRT_WARN, "");
                r = 0x17;
            }
            break;
        case 0x10:
            if (isBusy())
                r = drawDA(dst, p1, p2, p3, p4);
            else
                r = drawDB(dst, p1, p2, p3, p4);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:
        case 0x10:
            if (isBusy())


          _RPTF0(_CRT_WARN, "");
            else {
          _RPTF0(_CRT_WARN, "");
                r = 0x17;
            }
            break;
        default:


      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10015f80 ?drawE@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawE(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0 || !isValid())
        return 0x10;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else
                r = drawE8(dst, x, y, z);
            break;
        case 0x10:
            if (isBusy())
                r = drawE16busy(dst, x, y, z);
            else
                r = drawE16(dst, x, y, z);
            break;
        default:




      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        default:







      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10016270 ?drawF@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawF(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())


          _RPTF0(_CRT_WARN, "");
            else
                r = drawF8(dst, x, y, z);
            break;
        case 0x10:

      _RPTF0(_CRT_WARN, "");
            break;
        default:








      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:
        case 0x10:



      _RPTF0(_CRT_WARN, "");
            break;
        default:








      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x100165a0 ?drawG@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawG(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:



      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())
                r = drawG16busy(dst, x, y, z);
            else
                r = drawG16(dst, x, y, z);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:
        case 0x10:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else
                r = drawG2(dst, x, y, z);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x100169a0 ?drawH@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawH(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())


          _RPTF0(_CRT_WARN, "");
            else
                r = drawH8(dst, x, y, z);
            break;
        case 0x10:
            if (isBusy())

          _RPTF0(_CRT_WARN, "");
            else
                r = drawH16(dst, x, y, z);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        default:







      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10016cb0 ?drawI@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawI(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:


      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())
                r = drawI16busy(dst, x, y, z);
            else
                r = drawI16(dst, x, y, z);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:


      _RPTF0(_CRT_WARN, "");
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10016fc0 ?drawJ@Sprite@@QAEHPAVSurface@@HHHHH@Z
int Sprite::drawJ(Surface* dst, int x, int y, int z, int w, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())



          _RPTF0(_CRT_WARN, "");
            else
                r = drawJ8(dst, x, y, z, w);
            break;
        case 0x10:

      _RPTF0(_CRT_WARN, "");
            break;
        default:








      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        default:






      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x100172b0 ?drawK@Sprite@@QAEHPAVSurface@@HHHHH@Z
int Sprite::drawK(Surface* dst, int x, int y, int z, int w, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:








      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())
                r = drawK16busy(dst, x, y, z, w);
            else
                r = drawK16(dst, x, y, z, w);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 8:


      _RPTF0(_CRT_WARN, "");
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10018490 ?lineTo@Sprite@@QAEHPAVSurface@@PAHHHHH@Z
int Sprite::lineTo(Surface* dst, int* p2, int a, int b, int c, int key)
// FN
{
    int saved;
    saved = m_10;
    if (key)
        m_10 = key;
    return lineTo8(dst, a, b, c, p2);
}
// MATCH: jgld.dll 0x10018510 ?dashTo@Sprite@@QAEHPAVSurface@@PAHHHHHHHH@Z
int Sprite::dashTo(Surface* dst, int* p2, int a, int b, int c, int d, int e, int f, int key)
// FN
{
    int saved;
    if (c != d)
        return 1;
    saved = m_10;
    if (key)
        m_10 = key;
    return dashTo8(dst, a, b, c, e, f, p2);
}
// MATCH: jgld.dll 0x10019830 ?blt16a@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::blt16a(Surface* dst, int a, int b, int c, int key)
// FN
{
    int saved;
    if (*dst->format() != 0x10 || bpp() != 8)
        return 0x17;
    saved = m_10;
    if (key)
        m_10 = key;
    return blt16a8(dst, a, b, c);
}
// MATCH: jgld.dll 0x10019900 ?blt16b@Sprite@@QAEHPAVSurface@@HHGHD@Z
int Sprite::blt16b(Surface* dst, int a, int b, unsigned short c, int key, char d)
// FN
{
    int saved;
    if (*dst->format() != 0x10 || bpp() != 8)
        return 0x17;
    saved = m_10;
    if (key)
        m_10 = key;
    return blt16b8(dst, a, b, c, d);
}
// MATCH: jgld.dll 0x100199d0 ?blt16c@Sprite@@QAEHPAVSurface@@HHGHD@Z
int Sprite::blt16c(Surface* dst, int a, int b, unsigned short c, int key, char d)
// FN
{
    int saved;
    if (*dst->format() != 0x10 || bpp() != 8)
        return 0x17;
    saved = m_10;
    if (key)
        m_10 = key;
    return blt16c8(dst, a, b, c, d);
}
// MATCH: jgld.dll 0x10019aa0 ?bltS@Sprite@@QAEHPAV1@PAVSurface@@HHH@Z
int Sprite::bltS(Sprite* s, Surface* dst, int a, int b, int key)
// FN
{
    int saved;
    if (*dst->format() != 0x10 || bpp() != 8 || s->bpp() != 8)
        return 0x17;
    saved = m_10;
    if (key)
        m_10 = key;
    return bltS8(s, dst, a, b);
}
// MATCH: jgld.dll 0x10019b90 ?bltS2@Sprite@@QAEHPAV1@PAVSurface@@HHHH@Z
int Sprite::bltS2(Sprite* s, Surface* dst, int a, int b, int c, int key)
// FN
{
    int saved;
    if (*dst->format() != 0x10 || bpp() != 8 || s->bpp() != 8)
        return 0x17;
    saved = m_10;
    if (key)
        m_10 = key;
    return bltS28(s, dst, a, b, c);
}
// MATCH: jgld.dll 0x100178a0 ?drawL@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawL(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:
            if (isBusy())








          _RPTF0(_CRT_WARN, "");
            else
                r = drawL8(dst, x, y, z);
            break;
        default:




      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        default:







      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10017b40 ?drawM@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawM(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:








      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())
          _RPTF0(_CRT_WARN, "");
            else
                r = drawM8(dst, x, y, z);
            break;
        default:



      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 0x10:
            if (isBusy())


          _RPTF0(_CRT_WARN, "");
            else
                r = drawM16(dst, x, y, z);
            break;
        default:




      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x10018120 ?drawN@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawN(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:












      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())

          _RPTF0(_CRT_WARN, "");
            else
                r = drawNA(dst, x, y, z);
            break;
        default:





      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 0x10:
            if (isBusy())




          _RPTF0(_CRT_WARN, "");
            else
                r = drawNB(dst, x, y, z);
            break;
        default:






      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x100185b0 ?drawO@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawO(Surface* dst, int x, int y, int z, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:












      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())

          _RPTF0(_CRT_WARN, "");
            else
                r = drawOA(dst, x, y, z, 0);
            break;
        default:





      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 0x10:
            if (isBusy())




          _RPTF0(_CRT_WARN, "");
            else
                r = drawOB(dst, x, y, z);
            break;
        default:






      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
// MATCH: jgld.dll 0x100193d0 ?drawP@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::drawP(Surface* dst, int x, int y, int pct, int key)
// FN
{
    int saved;
    int r;
    r = 0;
    saved = m_10;
    if (key)
        m_10 = key;
    if (dst == 0)
        return 0x10;
    if (!isValid())
        return 7;
    switch (m_20) {
    case 8:
        switch (*dst->format()) {
        case 8:












      _RPTF0(_CRT_WARN, "");
            break;
        case 0x10:
            if (isBusy())

          _RPTF0(_CRT_WARN, "");
            else
                switch (pct) {
                case 100:
                    r = drawV(dst, x, y, key);
                    break;
                case 50:
                    r = drawP8_50(dst, x, y);
                    break;
                case 25:
                    r = drawP8_25(dst, x, y);
                    break;
                default:
                    r = 3;
                }
            break;
        default:







      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    case 0x10:
        switch (*dst->format()) {
        case 0x10:
            if (isBusy())




          _RPTF0(_CRT_WARN, "");
            else
                switch (pct) {
                case 100:
                    r = drawV(dst, x, y, key);
                    break;
                case 50:
                    r = drawP16_50(dst, x, y);
                    break;
                case 25:
                    r = drawP16_25(dst, x, y);
                    break;
                default:
                    r = 3;
                }
            break;
        default:








      _RPTF0(_CRT_WARN, " ");
            r = 0x17;
        }
        break;
    default:



  _RPTF0(_CRT_WARN, " ");
        r = 0x17;
    }
    m_10 = saved;
    unlock(1);
    return r;
}
void* jalloc(size_t n);
// MATCH: jgld.dll 0x1001a0e0 ?copyFrom@Sprite@@QAEAAV1@PAV1@@Z
Sprite& Sprite::copyFrom(Sprite* o)
// FN
{
    Sprite* src;
    clear();
    src = o;
    m_10 = src->m_10;
    m_18 = src->m_18;
    m_34 = src->m_34;
    m_30 = src->m_30;
    m_2c = src->m_2c;
    m_28 = src->m_28;
    m_20 = src->m_20;
    m_24 = src->m_24;
    m_14 = jalloc(m_24);
    memcpy(m_14, src->m_14, m_24);
    if (src->m_1c) {
        m_1c = (RowSpan*)jalloc(m_34 << 2);
        memcpy(m_1c, src->m_1c, m_34 << 2);
    }
    return *this;
}
// MATCH: jgld.dll 0x1001bae0 ?remap8@Sprite@@QAEHHH@Z
int Sprite::remap8(int a, int b)
// FN
{
    char* p;
    int n4;
    int rem;
    if (m_14 == 0)
        return 7;
    a &= 0xff;
    b &= 0xff;
    p = (char*)m_14;
    if (m_18 & 1) {
        n4 = m_24 >> 2;
        rem = m_24 - (n4 << 2);
    } else {
        n4 = (m_2c * m_34) >> 2;
        rem = m_2c * m_34 - (n4 << 2);
    }
    __asm {
        push esi
        mov esi, dword ptr [ebp - 8]
        mov ecx, dword ptr [ebp - 0xc]
        mov al, byte ptr [ebp + 8]
        mov ah, byte ptr [ebp + 0xc]
        and ecx, ecx
        je Lbbe1
    Lbb99:
        cmp byte ptr [esi], al
        je Lbbb6
    Lbb9d:
        inc esi
        cmp byte ptr [esi], al
        je Lbbbd
    Lbba2:
        inc esi
        cmp byte ptr [esi], al
        je Lbbc4
    Lbba7:
        inc esi
        cmp byte ptr [esi], al
        je Lbbcb
    Lbbac:
        inc esi
        loop Lbb99
        dec esi
        jmp Lbbd1
    Lbbb2:
        cmp byte ptr [esi], al
        jne Lbb9d
    Lbbb6:
        mov byte ptr [esi], ah
        inc esi
        cmp byte ptr [esi], al
        jne Lbba2
    Lbbbd:
        mov byte ptr [esi], ah
        inc esi
        cmp byte ptr [esi], al
        jne Lbba7
    Lbbc4:
        mov byte ptr [esi], ah
        inc esi
        cmp byte ptr [esi], al
        jne Lbbac
    Lbbcb:
        mov byte ptr [esi], ah
        inc esi
        loop Lbbb2
        dec esi
    Lbbd1:
        mov ecx, dword ptr [ebp - 0x10]
        and ecx, ecx
        je Lbbe1
    Lbbd8:
        inc esi
        cmp byte ptr [esi], al
        jne Lbbdf
        mov byte ptr [esi], ah
    Lbbdf:
        loop Lbbd8
    Lbbe1:
        pop esi
    }
    return 0;
}
// MATCH: jgld.dll 0x1001b690 ?create8@Sprite@@QAEHPAXIHHH@Z
int Sprite::create8(void* bits, unsigned w, int h, int bpp, int e)
// FN
{
    unsigned budget;
    unsigned char* row;
    char* dst;
    unsigned char* src;
    int y;
    int x;
    clear();
    m_30 = w;
    m_34 = h;
    m_20 = 8;
    m_24 = w * h;
    m_14 = jalloc(m_24);
    if (m_14 == 0)
        return 4;
    g_101284c8 += w * h;
    if (bits) {
        if (g_101284c4 && w > 4 && (int)w < 0x100 && (int)(w * h) < 0x10000) {
            budget = w * h - h * 4;
            m_1c = (RowSpan*)jalloc(h * 4);
            if (m_1c == 0) {
                clear();
                return 4;
            }
            dst = (char*)m_14;
            src = (unsigned char*)bits;
            for (y = 0; y < h; y++) {
                row = src;
                for (x = 0; x < (int)w; x++) {
                    if (row[x] != m_28)
                        break;
                }
                m_1c[y].start = x;
                for (x = w - 1; x > m_1c[y].start; x--) {
                    if (row[x] != m_28)
                        break;
                }
                m_1c[y].len = x + 1 - m_1c[y].start;
                if (m_1c[y].len) {
                    if (budget < m_1c[y].len) {
                        jfree(m_1c);
                        m_1c = 0;
                        goto plain;
                    }
                    m_1c[y].off = dst - (char*)m_14;
                    memcpy(dst, row + m_1c[y].start, m_1c[y].len);
                    budget -= m_1c[y].len;
                    dst += m_1c[y].len;
                }
                src += w;
            }
            m_18 |= 1;
            m_24 = w * h - budget;
            m_14 = _expand(m_14, m_24);
            g_101284cc += w * h - m_24 - h * 4;
            goto done;
        } else {
plain:
            m_18 &= ~1;
            memcpy(m_14, bits, w * h);
        }
    }
done:
    m_2c = w;
    return 0;
}
// MATCH: jgld.dll 0x1001a250 ?create16@Sprite@@QAEHPAXIHHH@Z
int Sprite::create16(void* bits, unsigned w, int h, int bpp, int e)
// FN
{
    unsigned budget;
    unsigned short* row;
    unsigned short* dst;
    unsigned short* src;
    int y;
    int x;
    clear();
    m_30 = w;
    m_34 = h;
    m_20 = 0x10;
    m_24 = w * h * 2;
    m_14 = jalloc(m_24);
    if (m_14 == 0)
        return 4;
    g_101284c8 += w * h;
    if (bits) {
        if (g_101284c4 && w > 4 && (int)w < 0x100 && (int)(w * h) < 0x10000) {
            budget = w * h - h * 4;
            m_1c = (RowSpan*)jalloc(h * 4);
            if (m_1c == 0) {
                clear();
                return 4;
            }
            dst = (unsigned short*)m_14;
            src = (unsigned short*)bits;
            for (y = 0; y < h; y++) {
                row = src;
                for (x = 0; x < (int)w; x++) {
                    if (row[x] != m_28)
                        break;
                }
                m_1c[y].start = x;
                for (x = w - 1; x > m_1c[y].start; x--) {
                    if (row[x] != m_28)
                        break;
                }
                m_1c[y].len = x + 1 - m_1c[y].start;
                if (m_1c[y].len) {
                    if (budget < m_1c[y].len) {
                        jfree(m_1c);
                        m_1c = 0;
                        goto plain;
                    }
                    m_1c[y].off = dst - (unsigned short*)m_14;
                    memcpy(dst, row + m_1c[y].start, m_1c[y].len);
                    budget -= m_1c[y].len;
                    dst += m_1c[y].len;
                }
                src += w;
            }
            m_18 |= 1;
            m_24 = w * h - budget;
            m_14 = _expand(m_14, m_24);
            g_101284cc += w * h - m_24 - h * 4;
            goto done;
        } else {
plain:
            m_18 &= ~1;
            memcpy(m_14, bits, w * h);
        }
    }
done:
    m_2c = w;
    return 0;
}
