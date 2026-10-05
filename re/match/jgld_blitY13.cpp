// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitter onto 16-bit surfaces with an 8-bit mask Surface and a byte level kept in 0x10128568 for the __asm
// loops (lock both, palette table, four mirror cases, unscaled only). Names are chosen here.
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
extern int g_10128538;                       // mask pitch
extern int g_10128534;                       // mask row skip
extern unsigned char g_10128568;             // mask threshold, read by the __asm loops
class Sprite {
public:
    virtual ~Sprite();
    int draw16m_1001e970(Surface* dst, int x, int y, Surface* mask, unsigned char level);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1001e970 ?draw16m_1001e970@Sprite@@QAEHPAVSurface@@HH0E@Z
int Sprite::draw16m_1001e970(Surface* dst, int x, int y, Surface* mask, unsigned char level)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned char* s;
    unsigned char* m;
    Pal16* pal;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    if (mask == 0 || !mask->lock()) {
        dst->unlock(1);
        return 3;
    }
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0) {
        mask->unlock(2);
        dst->unlock(1);
        return 16;
    }
    g_1012858c = (float)abs(g_10122dc0) / g_10122dc8;
    g_10128590 = (float)abs(g_10122dc4) / g_10122dc8;
    g_1012857c = m_pitch;
    g_1012855c = dst->pitch();
    g_10128538 = mask->pitch();
    s = m_bits;
    g_10128568 = level;
    switch (dst->format()[1]) {
    case 0:
        g_1012856c = pal->table565();
        break;
    case 1:
        g_1012856c = pal->table555();
        break;
    default:
        return 1;
    }
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
            mask->unlock(2);
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                m = mask->bits8() + rc.left + rc.top * g_10128538;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c - g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_10128538 - g_10128564;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ecx, dword ptr [ebp - 0x30]
        mov bl, byte ptr g_10128568
        push ebp
        mov ebp, dword ptr g_1012856c
    Led3c:
        mov edx, dword ptr g_10128564
    Led42:
        cmp byte ptr [esi], 0xfe
        jae Led57
        cmp byte ptr [ecx], bl
        jb Led57
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Led57:
        inc esi
        add edi, 2
        inc ecx
        dec edx
        jne Led42
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        add ecx, dword ptr g_10128534
        dec dword ptr g_10128578
        jne Led3c
        pop ebp
        pop edi
        pop esi
    }
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c + g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_10128538 - g_10128564;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ecx, dword ptr [ebp - 0x30]
        mov bl, byte ptr g_10128568
        push ebp
        mov ebp, dword ptr g_1012856c
    Lee34:
        mov edx, dword ptr g_10128564
    Lee3a:
        cmp byte ptr [esi], 0xfe
        jae Lee4f
        cmp byte ptr [ecx], bl
        jb Lee4f
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lee4f:
        inc esi
        add edi, 2
        inc ecx
        dec edx
        jne Lee3a
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        add ecx, dword ptr g_10128534
        dec dword ptr g_10128578
        jne Lee34
        pop ebp
        pop edi
        pop esi
    }
            }
        } else {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c + g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_10128538 - g_10128564;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ecx, dword ptr [ebp - 0x30]
        mov bl, byte ptr g_10128568
        push ebp
        mov ebp, dword ptr g_1012856c
    Lef39:
        mov edx, dword ptr g_10128564
    Lef3f:
        cmp byte ptr [esi], 0xfe
        jae Lef54
        cmp byte ptr [ecx], bl
        jb Lef54
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lef54:
        dec esi
        add edi, 2
        add ecx, 2
        dec edx
        jne Lef3f
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        add ecx, dword ptr g_10128534
        dec dword ptr g_10128578
        jne Lef39
        pop ebp
        pop edi
        pop esi
    }
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c - g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
                g_10128534 = g_1012855c - g_10128564;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ecx, dword ptr [ebp - 0x30]
        mov bl, byte ptr g_10128568
        push ebp
        mov ebp, dword ptr g_1012856c
    Lf033:
        mov edx, dword ptr g_10128564
    Lf039:
        cmp byte ptr [esi], 0xfe
        jae Lf04e
        cmp byte ptr [ecx], bl
        jb Lf04e
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lf04e:
        dec esi
        add edi, 2
        add ecx, 2
        dec edx
        jne Lf039
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        add ecx, dword ptr g_10128534
        dec dword ptr g_10128578
        jne Lf033
        pop ebp
        pop edi
        pop esi
    }
            }
        }
    } else {
        mask->unlock(2);
        dst->unlock(2);
    }
    dst->unlock(2);
    mask->unlock(2);
    return 0;
}

