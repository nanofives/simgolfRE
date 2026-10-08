// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitters onto 16-bit surfaces (debug build): one C skeleton per family (clip, palette table, mirror
// direction from the signs of the global scale, unscaled or 16.16 fixed-point scaled stepping) and an inline __asm
// inner loop per case, as in the original (register-only loops in a /Od build). Variants of a family differ only in
// the __asm blocks. Names are chosen here.
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
extern int g_1012858a;
class Sprite {
public:
    virtual ~Sprite();
    int draw_1001f2d0(Surface* dst, int x, int y, int c);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1001f2d0 ?draw_1001f2d0@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw_1001f2d0(Surface* dst, int x, int y, int c)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned char* s;
    int offX;
    int offY;
    Pal16* pal;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0) {
        dst->unlock(1);
        return 16;
    }
    g_1012858c = (float)abs(g_10122dc0) / g_10122dc8;
    g_10128590 = (float)abs(g_10122dc4) / g_10122dc8;
    g_1012857c = m_pitch;
    g_1012855c = dst->pitch();
    s = m_bits;
    if (c >> 31)
        g_10128530 = (unsigned short)c;
    else
        switch (dst->format()[1]) {
        case 0:
            g_10128530 = pal->table565()[c & 0xff];
            break;
        case 1:
            g_10128530 = pal->table555()[c & 0xff];
            break;
        default:
            return 1;
        }
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->clipRect())) {
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
                d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
                g_10128578 = rc.bottom - rc.top;
                g_10128564 = rc.right - rc.left;
                g_1012853c = g_1012857c - g_10128564;
                g_10128560 = (g_1012855c - g_10128564) * 2;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ax, word ptr g_10128530
    Lf60c:
        mov edx, dword ptr g_10128564
    Lf612:
        cmp byte ptr [esi], 0xfe
        jae Lf61a
        mov word ptr [edi], ax
    Lf61a:
        inc esi
        add edi, 2
        dec edx
        jne Lf612
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf60c
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
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ax, word ptr g_10128530
    Lf6d7:
        mov edx, dword ptr g_10128564
    Lf6dd:
        cmp byte ptr [esi], 0xfe
        jae Lf6e5
        mov word ptr [edi], ax
    Lf6e5:
        inc esi
        add edi, 2
        dec edx
        jne Lf6dd
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf6d7
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
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ax, word ptr g_10128530
    Lf7af:
        mov edx, dword ptr g_10128564
    Lf7b5:
        cmp byte ptr [esi], 0xfe
        jae Lf7bd
        mov word ptr [edi], ax
    Lf7bd:
        dec esi
        add edi, 2
        dec edx
        jne Lf7b5
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf7af
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
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov ax, word ptr g_10128530
    Lf87a:
        mov edx, dword ptr g_10128564
    Lf880:
        cmp byte ptr [esi], 0xfe
        jae Lf888
        mov word ptr [edi], ax
    Lf888:
        dec esi
        add edi, 2
        dec edx
        jne Lf880
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf87a
        pop edi
        pop esi
    }
            }
        }
    } else {
        setRect(&rc, x + (int)g_1012858c, y + (int)g_10128590, (int)(m_width * g_1012858c), (int)(m_height * g_10128590));
        orig = rc;
        if (!intersect(&rc, &rc, dst->clipRect())) {
            dst->unlock(1);
            return 0;
        }
        d = (unsigned short*)dst->bits() + rc.left + rc.top * g_1012855c;
        g_1012855c = dst->pitch();
        g_10128564 = rc.right - rc.left;
        g_10128578 = rc.bottom - rc.top;
        g_10128560 = (g_1012855c - g_10128564) * 2;
        g_10128588 = (g_10122dc8 << 16) / abs(g_10122dc0);
        g_10128540 = (g_10122dc8 << 16) / abs(g_10122dc4);
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_10128554 = g_10128588 * offX;
                g_10128558 = g_10128540 * offY;
                g_10128570 = (g_10128554 & 0xffff) + g_10128588 * g_10128564 >> 16;
                g_10128584 = (g_10128558 & 0xffff) + g_10128540 * g_10128578 >> 16;
                g_1012853c = g_1012857c - g_10128570;
                s = m_bits + (g_10128558 >> 16) * g_1012857c + (g_10128554 >> 16);
                *g_10122e20 = (g_10128540 >> 16) * g_1012857c - g_1012857c + g_1012853c;
                *g_10122e1c = *g_10122e20 + g_1012857c;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128558
        shl edx, 0x10
        mov dx, word ptr g_10128554
        mov ebx, dword ptr g_10128578
        shl ebx, 0x10
        mov bx, word ptr g_10128588
        mov ecx, dword ptr g_10128564
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128564, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012858a
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        mov ax, word ptr g_10128530
    Lfb6a:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lfb80:
        cmp byte ptr [esi], 0xfe
        jae Lfb90
        mov ax, word ptr g_10128530
        mov word ptr [edi], ax
        xor eax, eax
    Lfb90:
        add dx, bx
        adc esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lfb80
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lfb6a
        pop edi
        pop esi
    }
            } else {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_10128554 = g_10128588 * offX;
                g_10128558 = g_10128540 * offY;
                g_10128570 = (g_10128554 & 0xffff) + g_10128588 * g_10128564 >> 16;
                g_10128584 = (g_10128558 & 0xffff) + g_10128540 * g_10128578 >> 16;
                s = m_bits + (m_height - 1 - (g_10128558 >> 16)) * g_1012857c + (g_10128554 >> 16);
                *g_10122e20 = (g_10128540 >> 16) * g_1012857c + g_10128570;
                *g_10122e1c = *g_10122e20 + g_1012857c;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128558
        shl edx, 0x10
        mov dx, word ptr g_10128554
        mov ebx, dword ptr g_10128578
        shl ebx, 0x10
        mov bx, word ptr g_10128588
        mov ecx, dword ptr g_10128564
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128564, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012858a
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        mov ax, word ptr g_10128530
    Lfcf2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lfd08:
        cmp byte ptr [esi], 0xfe
        jae Lfd18
        mov ax, word ptr g_10128530
        mov word ptr [edi], ax
        xor eax, eax
    Lfd18:
        add dx, bx
        adc esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lfd08
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lfcf2
        pop edi
        pop esi
    }
            }
        } else {
            if (g_10122dc4 >= 0) {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_10128554 = g_10128588 * offX;
                g_10128558 = g_10128540 * offY;
                g_10128570 = (g_10128554 & 0xffff) + g_10128588 * g_10128564 >> 16;
                g_10128584 = (g_10128558 & 0xffff) + g_10128540 * g_10128578 >> 16;
                s = m_bits + (g_10128558 >> 16) * g_1012857c + m_width - 1 - (g_10128554 >> 16);
                *g_10122e20 = (g_10128540 >> 16) * g_1012857c + g_10128570;
                *g_10122e1c = *g_10122e20 + g_1012857c;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128558
        shl edx, 0x10
        mov dx, word ptr g_10128554
        mov ebx, dword ptr g_10128578
        shl ebx, 0x10
        mov bx, word ptr g_10128588
        mov ecx, dword ptr g_10128564
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128564, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012858a
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        mov ax, word ptr g_10128530
    Lfe87:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lfe9d:
        cmp byte ptr [esi], 0xfe
        jae Lfead
        mov ax, word ptr g_10128530
        mov word ptr [edi], ax
        xor eax, eax
    Lfead:
        add dx, bx
        sbb esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lfe9d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lfe87
        pop edi
        pop esi
    }
            } else {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_10128554 = g_10128588 * offX;
                g_10128558 = g_10128540 * offY;
                g_10128570 = (g_10128554 & 0xffff) + g_10128588 * g_10128564 >> 16;
                g_10128584 = (g_10128558 & 0xffff) + g_10128540 * g_10128578 >> 16;
                s = m_bits + (m_height - 1 - (g_10128558 >> 16)) * g_1012857c + m_width - 1 - (g_10128554 >> 16);
                g_1012853c = -g_1012857c + g_10128570;
                *g_10122e20 = (g_10128540 >> 16) * g_1012857c + -g_10128570;
                *g_10122e1c = *g_10122e20 + g_1012857c;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128558
        shl edx, 0x10
        mov dx, word ptr g_10128554
        mov ebx, dword ptr g_10128578
        shl ebx, 0x10
        mov bx, word ptr g_10128588
        mov ecx, dword ptr g_10128564
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128564, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012858a
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        mov ax, word ptr g_10128530
    L002f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0045:
        cmp byte ptr [esi], 0xfe
        jae L0055
        mov ax, word ptr g_10128530
        mov word ptr [edi], ax
        xor eax, eax
    L0055:
        add dx, bx
        sbb esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns L0045
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L002f
        pop edi
        pop esi
    }
            }
        }
    }
    dst->unlock(2);
    return 0;
}

