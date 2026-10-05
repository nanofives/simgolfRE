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
extern int g_1012858a;
class Sprite {
public:
    virtual ~Sprite();
    int draw_1002bcf0(Surface* dst, int x, int y, int p4);
    int draw_1002ab90(Surface* dst, int x, int y, int p4);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1002bcf0 ?draw_1002bcf0@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw_1002bcf0(Surface* dst, int x, int y, int p4)
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
    g_10128548 = p4;
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
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
        push ebp
        mov ebp, dword ptr g_10128548
    Lbf90:
        mov edx, dword ptr g_10128564
    Lbf96:
        cmp byte ptr [esi], 0xfe
        jae Lbfbe
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lbfaa
        mov word ptr [edi], 0
        jmp Lbfbe
    Lbfaa:
        dec al
        cmp al, 0xf
        jae Lbfbe
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lbfbe:
        inc esi
        add edi, 2
        dec edx
        jne Lbf96
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lbf90
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
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        push ebp
        mov ebp, dword ptr g_10128548
    Lc07d:
        mov edx, dword ptr g_10128564
    Lc083:
        cmp byte ptr [esi], 0xfe
        jae Lc0ab
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lc097
        mov word ptr [edi], 0
        jmp Lc0ab
    Lc097:
        dec al
        cmp al, 0xf
        jae Lc0ab
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lc0ab:
        inc esi
        add edi, 2
        dec edx
        jne Lc083
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc07d
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
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        push ebp
        mov ebp, dword ptr g_10128548
    Lc177:
        mov edx, dword ptr g_10128564
    Lc17d:
        cmp byte ptr [esi], 0xfe
        jae Lc1a5
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lc191
        mov word ptr [edi], 0
        jmp Lc1a5
    Lc191:
        dec al
        cmp al, 0xf
        jae Lc1a5
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lc1a5:
        dec esi
        add edi, 2
        dec edx
        jne Lc17d
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc177
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
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        push ebp
        mov ebp, dword ptr g_10128548
    Lc264:
        mov edx, dword ptr g_10128564
    Lc26a:
        cmp byte ptr [esi], 0xfe
        jae Lc292
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lc27e
        mov word ptr [edi], 0
        jmp Lc292
    Lc27e:
        dec al
        cmp al, 0xf
        jae Lc292
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lc292:
        dec esi
        add edi, 2
        dec edx
        jne Lc26a
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc264
        pop ebp
        pop edi
        pop esi
    }
            }
        }
    } else {
        setRect(&rc, x + (int)g_1012858c, y + (int)g_10128590, (int)(m_width * g_1012858c), (int)(m_height * g_10128590));
        orig = rc;
        if (!intersect(&rc, &rc, dst->boundsRect())) {
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
        push ebp
        mov ebp, dword ptr g_10128548
    Lc576:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc58c:
        cmp byte ptr [esi], 0xfe
        jae Lc5b4
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lc5a0
        mov word ptr [edi], 0
        jmp Lc5b4
    Lc5a0:
        dec al
        cmp al, 0xf
        jae Lc5b4
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lc5b4:
        add dx, bx
        adc esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lc58c
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc576
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_10128548
    Lc718:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc72e:
        cmp byte ptr [esi], 0xfe
        jae Lc756
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lc742
        mov word ptr [edi], 0
        jmp Lc756
    Lc742:
        dec al
        cmp al, 0xf
        jae Lc756
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lc756:
        add dx, bx
        adc esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lc72e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc718
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_10128548
    Lc8c7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc8dd:
        cmp byte ptr [esi], 0xfe
        jae Lc905
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lc8f1
        mov word ptr [edi], 0
        jmp Lc905
    Lc8f1:
        dec al
        cmp al, 0xf
        jae Lc905
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lc905:
        add dx, bx
        sbb esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lc8dd
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc8c7
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_10128548
    Lca89:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lca9f:
        cmp byte ptr [esi], 0xfe
        jae Lcac7
        xor eax, eax
        mov al, byte ptr [esi]
        or al, al
        jne Lcab3
        mov word ptr [edi], 0
        jmp Lcac7
    Lcab3:
        dec al
        cmp al, 0xf
        jae Lcac7
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    Lcac7:
        add dx, bx
        sbb esi, ecx
        add edi, 2
        sub ebx, 0x10000
        jns Lca9f
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lca89
        pop ebp
        pop edi
        pop esi
    }
            }
        }
    }
    dst->unlock(2);
    return 0;
}
// MATCH: jgld.dll 0x1002ab90 ?draw_1002ab90@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw_1002ab90(Surface* dst, int x, int y, int p4)
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
    g_10128548 = p4;
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
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
        push ecx
        mov ecx, dword ptr g_10128548
    Lae30:
        mov edx, dword ptr g_10128564
    Lae36:
        cmp byte ptr [esi], 0xfe
        jae Lae4e
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lae4e:
        inc esi
        add edi, 2
        dec edx
        jne Lae36
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lae30
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_10128548
    Laf0d:
        mov edx, dword ptr g_10128564
    Laf13:
        cmp byte ptr [esi], 0xfe
        jae Laf2b
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Laf2b:
        inc esi
        add edi, 2
        dec edx
        jne Laf13
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Laf0d
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_10128548
    Laff7:
        mov edx, dword ptr g_10128564
    Laffd:
        cmp byte ptr [esi], 0xfe
        jae Lb015
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lb015:
        dec esi
        add edi, 2
        dec edx
        jne Laffd
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Laff7
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_10128548
    Lb0d4:
        mov edx, dword ptr g_10128564
    Lb0da:
        cmp byte ptr [esi], 0xfe
        jae Lb0f2
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lb0f2:
        dec esi
        add edi, 2
        dec edx
        jne Lb0da
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb0d4
        pop ecx
        pop edi
        pop esi
    }
            }
        }
    } else {
        setRect(&rc, x + (int)g_1012858c, y + (int)g_10128590, (int)(m_width * g_1012858c), (int)(m_height * g_10128590));
        orig = rc;
        if (!intersect(&rc, &rc, dst->boundsRect())) {
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ecx
        mov ecx, dword ptr g_10128548
    Lb3dc:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lb3f2:
        cmp byte ptr [esi], 0xfe
        jae Lb40a
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lb40a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lb3f2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lb3dc
        pop ecx
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ecx
        mov ecx, dword ptr g_10128548
    Lb578:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lb58e:
        cmp byte ptr [esi], 0xfe
        jae Lb5a6
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lb5a6:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lb58e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lb578
        pop ecx
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ecx
        mov ecx, dword ptr g_10128548
    Lb721:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lb737:
        cmp byte ptr [esi], 0xfe
        jae Lb74f
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lb74f:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lb737
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lb721
        pop ecx
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ecx
        mov ecx, dword ptr g_10128548
    Lb8dd:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lb8f3:
        cmp byte ptr [esi], 0xfe
        jae Lb90b
        xor eax, eax
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lb90b:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lb8f3
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lb8dd
        pop ecx
        pop edi
        pop esi
    }
            }
        }
    }
    dst->unlock(2);
    return 0;
}

