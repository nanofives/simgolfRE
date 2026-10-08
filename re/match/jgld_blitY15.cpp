// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitter from a 16-bit source onto 16-bit surfaces: one __asm loop for the unmirrored unscaled case, an int
// parameter kept in 0x101284fc, Sprite+0x28 set to 0xff; the scaled branch only computes steps. Names are chosen here.
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
    virtual RECT* clipRect();              // slot 51 (+0xcc)
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
extern float g_10128518;                     // horizontal scale
extern float g_1012851c;                     // vertical scale
extern int g_1012850c;                       // source pitch (pixels)
extern int g_101284ec;                       // destination pitch (pixels)
extern int g_101284fc;                       // 4th parameter, read by the __asm loop
extern int g_10128508;                       // rows
extern int g_101284f4;                       // columns
extern int g_101284d8;                       // source row skip (bytes)
extern int g_101284f0;                       // destination row skip
extern int g_10128514;                       // 16.16 horizontal step
extern int g_101284dc;                       // 16.16 vertical step
class Sprite {
public:
    virtual ~Sprite();
    int draw16t_1001b060(Surface* dst, int x, int y, int p4);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1001b060 ?draw16t_1001b060@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw16t_1001b060(Surface* dst, int x, int y, int p4)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned short* s;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    g_10128518 = (float)abs(g_10122dc0) / g_10122dc8;
    g_1012851c = (float)abs(g_10122dc4) / g_10122dc8;
    m_28 = 0xff;
    g_1012850c = m_pitch;
    g_101284ec = dst->pitch();
    s = (unsigned short*)m_bits;
    g_101284fc = p4;
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->clipRect())) {
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0 && g_10122dc4 >= 0) {
            s = (unsigned short*)m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012850c;
            d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101284ec;
            g_10128508 = rc.bottom - rc.top;
            g_101284f4 = rc.right - rc.left;
            g_101284d8 = (g_1012850c - g_101284f4) * 2;
            g_101284f0 = (g_101284ec - g_101284f4) * 2;
    __asm {
        push esi
        push edi
        xor ebx, ebx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ecx, dword ptr g_101284d8
        push ebp
        mov ebp, dword ptr g_101284fc
    Lb2c7:
        mov edx, dword ptr g_101284f4
    Lb2cd:
        cmp byte ptr [esi], 0xff
        je Lb2e4
        xor eax, eax
        mov ax, word ptr [esi]
        shl eax, 0x10
        mov ax, word ptr [edi]
        mov ax, word ptr [eax + ebp]
        mov word ptr [edi], ax
    Lb2e4:
        add esi, 2
        add edi, 2
        dec edx
        jne Lb2cd
        add esi, ecx
        add edi, dword ptr g_101284f0
        dec dword ptr g_10128508
        jne Lb2c7
        pop ebp
        pop edi
        pop esi
    }
        }
    } else {
        setRect(&rc, x, y, (int)(m_width * g_10128518), (int)(m_height * g_1012851c));
        orig = rc;
        if (!intersect(&rc, &rc, dst->clipRect())) {
            dst->unlock(1);
            return 0;
        }
        d = (unsigned short*)dst->bits() + rc.left + rc.top * g_101284ec;
        g_101284ec = dst->pitch();
        g_101284f4 = rc.right - rc.left;
        g_10128508 = rc.bottom - rc.top;
        g_101284f0 = g_101284ec - g_101284f4;
        g_10128514 = (g_10122dc8 << 16) / abs(g_10122dc0);
        g_101284dc = (g_10122dc8 << 16) / abs(g_10122dc4);
    }
    dst->unlock(1);
    return 0;
}

