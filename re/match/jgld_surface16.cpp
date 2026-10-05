// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll 16-bit surface clear (debug build). Surface and its palette are re-declared with only the slots used
// here (jgld_surface.cpp names: boundsRect +0xcc, clipRect +0xd4, format +0xe4). Names are chosen here.
#include <windows.h>
class Pal16 {
public:
    virtual void p0(); virtual void p1(); virtual void p2(); virtual void p3(); virtual void p4(); virtual void p5();
    virtual unsigned short* table565();      // slot 6 (+0x18)
    virtual unsigned short* table555();      // slot 7 (+0x1c)
};
struct PalClient { int m_0; Pal16* m_palette; };
extern PalClient* g_palClient1;              // 0x1012873c
int equal(const RECT* a, const RECT* b);     // 0x100085f0
int intersect(RECT* out, const RECT* a, const RECT* b);   // 0x10008590
extern int g_skip16;                         // 0x10128458
void setRect(RECT* r, int x, int y, int w, int h);   // 0x10008360
extern unsigned short* g_1012849c;           // palette table of the 8-to-16 blits
extern int g_101284a0; extern int g_101284a4; extern int g_10128490; extern int g_1012848c; extern int g_10128494;
int redOf(int c);                            // 0x1000e440
int greenOf(int c);                          // 0x1000e4c0
int blueOf(int c);                           // 0x1000e540
unsigned short pack16(unsigned char r, unsigned char g, unsigned char b);   // 0x10001582 (thunk)
void fillWords(void* dst, unsigned short v, int n);   // 0x1000af30
class Surface {
public:
    virtual void s0(); virtual void s1(); virtual void s2(); virtual void s3(); virtual void s4();
    virtual void* lockAt8(int x, int y);     // slot 5 (+0x14)
    virtual void s6();
    virtual void* lockAt16(int x, int y);    // slot 7 (+0x1c)
    virtual void* lock();                    // slot 8 (+0x20)
    virtual void unlock(int flag);           // slot 9 (+0x24)
    virtual void t10(); virtual void t11(); virtual void t12(); virtual void t13(); virtual void t14(); virtual void t15(); virtual void t16();
    virtual void fillRectC(RECT* r, int c);  // slot 17 (+0x44)
    virtual void u18(); virtual void u19(); virtual void u20(); virtual void u21(); virtual void u22(); virtual void u23(); virtual void u24(); virtual void u25(); virtual void u26(); virtual void u27(); virtual void u28(); virtual void u29(); virtual void u30(); virtual void u31(); virtual void u32(); virtual void u33(); virtual void u34(); virtual void u35(); virtual void u36(); virtual void u37(); virtual void u38(); virtual void u39(); virtual void u40(); virtual void u41(); virtual void u42(); virtual void u43(); virtual void u44(); virtual void u45(); virtual void u46(); virtual void u47(); virtual void u48(); virtual void u49(); virtual void u50();
    virtual RECT* boundsRect();              // slot 51 (+0xcc)
    virtual void w52();
    virtual RECT* clipRect();                // slot 53 (+0xd4)
    virtual int width();                     // slot 54 (+0xd8)
    virtual int height();                    // slot 55 (+0xdc)
    virtual int pitch();                     // slot 56 (+0xe0)
    virtual int* format();                   // slot 57 (+0xe4)
    void clear16(unsigned c);
    int fillRect16(const RECT* r, unsigned c);
    int ditherRect16(const RECT* r, unsigned c);
    int blendRect16(const RECT* r, unsigned c, int alpha);
    int ditherBlendRect16(const RECT* r, unsigned c, int alpha);
    void hline16(int x1, int x2, int y, unsigned c);
    void vline16(int x, int y1, int y2, unsigned c);
    int blitKey8to16(Surface* dst, int sx, int sy, int dx, int dy, int w, int h, unsigned char key);
    char m_4[0x7c - 4];
    Pal16* m_palette;                        // +0x7c
};
// MATCH: jgld.dll 0x1000da70 ?clear16@Surface@@QAEXI@Z
void Surface::clear16(unsigned c)
{
    int h;
    int w;
    void* bits;
    Pal16* pal;
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                return;
            }
        }
    }
    if (!equal(boundsRect(), clipRect())) {
        fillRectC(boundsRect(), c);
        return;
    }
    c &= 0xffff;
    bits = lock();
    h = clipRect()->bottom;
    w = clipRect()->right;
    fillWords(bits, (unsigned short)c, w * h);
    unlock(1);
}
// MATCH: jgld.dll 0x1000dcf0 ?fillRect16@Surface@@QAEHPBUtagRECT@@I@Z
int Surface::fillRect16(const RECT* r, unsigned c)
{
    int h;
    int w;
    RECT rc;
    void* dst = 0;
    Pal16* pal;
    if (r == 0) {
        clear16(c);
        return 0;
    }
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                return 24;
            }
        }
    }
    c &= 0xffff;
    rc = *r;
    if (!intersect(&rc, &rc, boundsRect()))
        return 0;
    dst = lockAt16(rc.left, rc.top);
    if (dst == 0)
        return 0;
    h = rc.bottom - rc.top;
    w = rc.right - rc.left;
    g_skip16 = (pitch() - w) * 2;
    while (h--) {
        fillWords(dst, (unsigned short)c, w);
        dst = (char*)dst + pitch() * 2;
    }
    unlock(1);
    return 0;
}
// MATCH: jgld.dll 0x1000e580 ?ditherRect16@Surface@@QAEHPBUtagRECT@@I@Z
int Surface::ditherRect16(const RECT* r, unsigned c)
{
    Pal16* pal;
    int h;
    int odd;
    int n;
    int w;
    int err = 0;
    RECT rc;
    unsigned short* p;
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                err = 24;
            }
        }
    }
    if (err == 0) {
        c &= 0xffff;
        if (r == 0)
            rc = *clipRect();
        else
            rc = *r;
        if (intersect(&rc, &rc, boundsRect())) {
            p = (unsigned short*)lockAt16(rc.left, rc.top);
            if (p) {
                h = rc.bottom - rc.top;
                w = rc.right - rc.left;
                while (h--) {
                    n = w;
                    if (h % 2) {
                        p++;
                        odd = 1;
                        n--;
                    } else {
                        odd = 0;
                    }
                    for (; n > 0; n -= 2) {
                        *p = (unsigned short)c;
                        p += 2;
                    }
                    p += pitch() - w - odd;
                }
                unlock(1);
            }
        }
    }
    return err;
}
// MATCH: jgld.dll 0x1000dfc0 ?blendRect16@Surface@@QAEHPBUtagRECT@@IH@Z
int Surface::blendRect16(const RECT* r, unsigned c, int alpha)
{
    Pal16* pal;
    int rr;
    int gg;
    int bb;
    int dr;
    int dg;
    int db;
    int h;
    int inv;
    int n;
    int sr;
    int sg;
    int sb;
    int w;
    int err = 0;
    RECT rc;
    unsigned short* p;
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                err = 24;
            }
        }
    }
    if (err == 0) {
        c &= 0xffff;
        if (r == 0)
            rc = *clipRect();
        else
            rc = *r;
        if (intersect(&rc, &rc, boundsRect())) {
            p = (unsigned short*)lockAt16(rc.left, rc.top);
            if (p) {
                h = rc.bottom - rc.top;
                w = rc.right - rc.left;
                alpha = alpha < 0 ? 0 : (alpha > 100 ? 100 : alpha);
                alpha = (alpha << 8) / 100;
                inv = 256 - alpha;
                sr = redOf(c) & 0xff;
                sg = greenOf(c) & 0xff;
                sb = blueOf(c) & 0xff;
                while (h--) {
                    for (n = w; n > 0; n--) {
                        dr = redOf(*p) & 0xff;
                        dg = greenOf(*p) & 0xff;
                        db = blueOf(*p) & 0xff;
                        rr = (sr * inv >> 8) + (dr * alpha >> 8);
                        gg = (sg * inv >> 8) + (dg * alpha >> 8);
                        bb = (sb * inv >> 8) + (db * alpha >> 8);
                        *p = pack16((unsigned char)rr, (unsigned char)gg, (unsigned char)bb);
                        p++;
                    }
                    p += pitch() - w;
                }
                unlock(1);
            }
        }
    }
    return err;
}
// MATCH: jgld.dll 0x1000e8b0 ?ditherBlendRect16@Surface@@QAEHPBUtagRECT@@IH@Z
int Surface::ditherBlendRect16(const RECT* r, unsigned c, int alpha)
{
    Pal16* pal;
    int rr;
    int gg;
    int bb;
    int dr;
    int dg;
    int db;
    int h;
    int odd;
    int inv;
    int n;
    int sr;
    int sg;
    int sb;
    int w;
    int err = 0;
    RECT rc;
    unsigned short* p;
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                err = 24;
            }
        }
    }
    if (err == 0) {
        c &= 0xffff;
        if (r == 0)
            rc = *clipRect();
        else
            rc = *r;
        if (intersect(&rc, &rc, boundsRect())) {
            p = (unsigned short*)lockAt16(rc.left, rc.top);
            if (p) {
                h = rc.bottom - rc.top;
                w = rc.right - rc.left;
                alpha = alpha < 0 ? 0 : (alpha > 100 ? 100 : alpha);
                alpha = (alpha << 8) / 100;
                inv = 256 - alpha;
                sr = redOf(c) & 0xff;
                sg = greenOf(c) & 0xff;
                sb = blueOf(c) & 0xff;
                while (h--) {
                    n = w;
                    if (h % 2) {
                        p++;
                        odd = 1;
                        n--;
                    } else {
                        odd = 0;
                    }
                    for (; n > 0; n -= 2) {
                        dr = redOf(*p) & 0xff;
                        dg = greenOf(*p) & 0xff;
                        db = blueOf(*p) & 0xff;
                        rr = (sr * inv >> 8) + (dr * alpha >> 8);
                        gg = (sg * inv >> 8) + (dg * alpha >> 8);
                        bb = (sb * inv >> 8) + (db * alpha >> 8);
                        *p = pack16((unsigned char)rr, (unsigned char)gg, (unsigned char)bb);
                        p += 2;
                    }
                    p += pitch() - w - odd;
                }
                unlock(1);
            }
        }
    }
    return err;
}
// MATCH: jgld.dll 0x1000ba90 ?hline16@Surface@@QAEXHHHI@Z
void Surface::hline16(int x1, int x2, int y, unsigned c)
{
    unsigned short* p;
    Pal16* pal;
    int n;
    if (y < boundsRect()->top || y >= boundsRect()->bottom)
        return;
    if (x1 == x2)
        return;
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                return;
            }
        }
    }
    c &= 0xffff;
    if (x1 > x2) {
        x1 ^= x2;
        x2 ^= x1;
        x1 ^= x2;
    }
    if (x1 >= boundsRect()->right || x2 < boundsRect()->left)
        return;
    if (x1 < boundsRect()->left)
        x1 = boundsRect()->left;
    if (x2 >= boundsRect()->right)
        x2 = boundsRect()->right - 1;
    p = (unsigned short*)lockAt16(x1, y);
    if (p == 0)
        return;
    for (n = x2 - x1 + 1; n > 0; n--) {
        *p = (unsigned short)c;
        p++;
    }
    unlock(1);
}
// MATCH: jgld.dll 0x1000bde0 ?vline16@Surface@@QAEXHHHI@Z
void Surface::vline16(int x, int y1, int y2, unsigned c)
{
    unsigned short* p;
    int stride;
    Pal16* pal;
    if (x < boundsRect()->left || x >= boundsRect()->right)
        return;
    if (y1 == y2)
        return;
    if (!(c & 0x80000000)) {
        pal = 0;
        if (m_palette)
            pal = m_palette;
        else if (g_palClient1)
            pal = g_palClient1->m_palette;
        if (pal) {
            switch (format()[1]) {
            case 0:
                c = pal->table565()[c & 0xff];
                break;
            case 1:
                c = pal->table555()[c & 0xff];
                break;
            default:
                return;
            }
        }
    }
    c &= 0xffff;
    if (y1 > y2) {
        y1 ^= y2;
        y2 ^= y1;
        y1 ^= y2;
    }
    if (y1 >= boundsRect()->bottom || y2 < boundsRect()->top)
        return;
    if (y1 < boundsRect()->top)
        y1 = boundsRect()->top;
    if (y2 >= boundsRect()->bottom)
        y2 = boundsRect()->bottom - 1;
    p = (unsigned short*)lockAt16(x, y1);
    if (p == 0)
        return;
    stride = pitch();
    __asm {
        push edi
        mov ecx, dword ptr [ebp + 0x10]
        sub ecx, dword ptr [ebp + 0xc]
        inc ecx
        mov ax, word ptr [ebp + 0x14]
        mov ebx, dword ptr [ebp - 0xc]
        shl ebx, 1
        mov edi, dword ptr [ebp - 8]
    Lc057:
        mov word ptr [edi], ax
        add edi, ebx
        loop Lc057
        pop edi
    }
    unlock(1);
}
// MATCH: jgld.dll 0x10012900 ?blitKey8to16@Surface@@QAEHPAV1@HHHHHHE@Z
int Surface::blitKey8to16(Surface* dst, int sx, int sy, int dx, int dy, int w, int h, unsigned char key)
{
    unsigned short* d;
    unsigned char* s;
    int dpitch;
    int spitch;
    RECT rc;
    int hh;
    int ww;
    Pal16* pal;
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
    if (sx > width())
        return 0;
    if (sy > height())
        return 0;
    if (sx + w > width())
        w = width() - sx;
    if (sy + h > height())
        h = height() - sy;
    setRect(&rc, dx, dy, w, h);
    if (!intersect(&rc, &rc, dst->boundsRect()))
        return 0;
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0)
        return 16;
    switch (dst->format()[1]) {
    case 0:
        g_1012849c = pal->table565();
        break;
    case 1:
        g_1012849c = pal->table555();
        break;
    default:
        return 1;
    }
    sx += rc.left - dx;
    sy += rc.top - dy;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    d = (unsigned short*)dst->lockAt16(rc.left, rc.top);
    if (d == 0)
        return 3;
    s = (unsigned char*)lockAt8(sx, sy);
    if (s == 0)
        return 3;
    dpitch = dst->pitch();
    spitch = pitch();
    hh = rc.bottom - rc.top;
    ww = rc.right - rc.left;
    g_101284a0 = ww >> 2;
    g_101284a4 = spitch - ww;
    g_10128490 = (dpitch - ww) * 2;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0xc]
        mov edi, dword ptr [ebp - 8]
        xor eax, eax
        mov al, byte ptr [ebp + 0x24]
        shl eax, 1
        add eax, dword ptr g_1012849c
        mov ax, word ptr [eax]
        mov ecx, dword ptr g_101284a4
        mov edx, dword ptr [ebp - 0x2c]
        mov dword ptr g_1012848c, edx
        mov edx, dword ptr [ebp - 0x28]
        mov dword ptr g_10128494, edx
        push ebp
        mov ebp, dword ptr g_10128490
    L2c44:
        mov edx, dword ptr g_1012848c
    L2c4a:
        cmp word ptr [edi], ax
        jne L2c61
        xor ebx, ebx
        mov bl, byte ptr [esi]
        shl ebx, 1
        add ebx, dword ptr g_1012849c
        mov bx, word ptr [ebx]
        mov word ptr [edi], bx
    L2c61:
        inc esi
        add edi, 2
        dec edx
        jne L2c4a
        add esi, ecx
        add edi, ebp
        dec dword ptr g_10128494
        jne L2c44
        pop ebp
        pop edi
        pop esi
    }
    unlock(1);
    dst->unlock(1);
    return 0;
}
