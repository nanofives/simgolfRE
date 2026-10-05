// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitter onto 16-bit surfaces with a mask Surface parameter, 16.16 stepping from a scale and denominator
// parameter pair, no source-bits or lock check; the 6th int parameter is kept in 0x10128548. Names are chosen here.
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
class Sprite {
public:
    virtual ~Sprite();
    int blitMaskScaled(Surface* dst, int x, int y, int scale, int den, int p6, Surface* mask);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1002d390 ?blitMaskScaled@Sprite@@QAEHPAVSurface@@HHHHH0@Z
int Sprite::blitMaskScaled(Surface* dst, int x, int y, int scale, int den, int p6, Surface* mask)
{
    RECT rc;
    unsigned short* d;
    unsigned char* s;
    Pal16* pal;
    unsigned short* m;
    m = 0;
    if (mask != 0)
        m = (unsigned short*)mask->bits();
    else
        return 3;
    g_1012858c = (float)abs(g_10122dc0) / den;
    g_10128590 = (float)abs(g_10122dc4) / den;
    setRect(&rc, x + (int)g_1012858c, y + (int)g_10128590, (int)(m_width * g_1012858c), (int)(m_height * g_10128590));
    if (!intersect(&rc, &rc, dst->boundsRect())) {
        dst->unlock(1);
        return 0;
    }
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0) {
        dst->unlock(1);
        return 16;
    }
    g_1012857c = m_pitch;
    g_1012855c = dst->pitch();
    s = m_bits;
    d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
    m += rc.left + rc.top * g_1012855c;
    g_10128564 = rc.right - rc.left;
    g_10128578 = rc.bottom - rc.top;
    g_10128560 = (g_1012855c - g_10128564) * 2;
    g_10128588 = (den << 16) / abs(scale);
    g_10128540 = (den << 16) / abs(scale);
    g_10128548 = p6;
    switch (dst->format()[1]) {
    case 0:
        g_1012856c = pal->table565();
        break;
    case 1:
        g_1012856c = pal->table555();
        break;
    default:
        dst->unlock(1);
        return 1;
    }
    if (scale >= 0) {
        g_10128570 = g_10128588 * g_10128564 >> 16;
        g_10128584 = g_10128540 * g_10128578 >> 16;
        g_1012853c = g_1012857c - g_10128570;
        *g_10122e20 = (g_10128540 >> 16) * g_1012857c - g_1012857c + g_1012853c;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x1c]
        mov edi, dword ptr [ebp - 0x18]
        mov ecx, dword ptr g_10128564
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128564, ecx
        push ecx
        mov ecx, dword ptr g_1012856c
    Ld6e5:
        mov ebx, dword ptr g_10128564
    Ld6eb:
        cmp byte ptr [esi], 0xff
        jae Ld73e
        xor eax, eax
        push ebx
        push ebp
        mov ebx, dword ptr [ebp - 0x24]
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        cmp al, 0xe0
        jae Ld70e
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [ebx], ax
        pop ebp
        pop ebx
        jmp Ld73e
    Ld70e:
        cmp al, 0xf0
        jae Ld716
        sub al, 0xd1
        jmp Ld718
    Ld716:
        sub al, 0xf0
    Ld718:
        shl eax, 0xf
        cmp word ptr [ebx], 0x7c1f
        jne Ld731
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [ebx], ax
        pop ebp
        pop ebx
        jmp Ld73e
    Ld731:
        or ax, word ptr [ebx]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [ebx], ax
        pop ebp
        pop ebx
    Ld73e:
        add esi, 2
        add edi, 2
        add dword ptr [ebp - 0x24], 2
        sub ebx, 0x10000
        jns Ld6eb
        add edi, dword ptr g_10128560
        push edi
        mov edi, dword ptr [ebp - 0x24]
        add edi, dword ptr g_10128560
        mov dword ptr [ebp - 0x24], edi
        pop edi
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx]
        dec dword ptr g_10128578
        jne Ld6e5
        pop ecx
        pop edi
        pop esi
    }
    }
    return 0;
}

