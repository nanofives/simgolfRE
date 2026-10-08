// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitter onto 8-bit surfaces (jgld_blit8.cpp skeleton) whose 4th parameter is a colour byte kept in
// 0x10128664 for the __asm loops. Names are chosen here.
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
extern int g_10128628;
extern int g_1012868a;
class Sprite {
public:
    virtual ~Sprite();
    int draw8_1005fd20(Surface* dst, int x, int y, unsigned char color);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1005fd20 ?draw8_1005fd20@Sprite@@QAEHPAVSurface@@HHE@Z
int Sprite::draw8_1005fd20(Surface* dst, int x, int y, unsigned char color)
{
    RECT rc;
    RECT orig;
    unsigned char* d;
    unsigned char* s;
    int offX;
    int offY;
    if (m_bits == 0 || dst == 0 || !dst->lock())
        return 3;
    g_1012868c = (float)abs(g_10122dc0) / g_10122dc8;
    g_10128690 = (float)abs(g_10122dc4) / g_10122dc8;
    g_10128678 = m_pitch;
    g_10128644 = dst->pitch();
    s = m_bits;
    g_10128664 = color;
    if (abs(g_10122dc0) == abs(g_10122dc8) && abs(g_10122dc4) == abs(g_10122dc8)) {
        setRect(&rc, x, y, m_width, m_height);
        orig = rc;
        if (!IntersectRect(&rc, &rc, dst->clipRect())) {
            dst->unlock(1);
            return 0;
        }
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 - g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov al, byte ptr g_10128664
    Lff74:
        mov edx, dword ptr g_10128650
    Lff7a:
        cmp byte ptr [esi], 0xfe
        jae Lff81
        mov byte ptr [edi], al
    Lff81:
        inc esi
        inc edi
        dec edx
        jne Lff7a
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne Lff74
        pop edi
        pop esi
    }
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 + g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov al, byte ptr g_10128664
    L0035:
        mov edx, dword ptr g_10128650
    L003b:
        cmp byte ptr [esi], 0xfe
        jae L0042
        mov byte ptr [edi], al
    L0042:
        inc esi
        inc edi
        dec edx
        jne L003b
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L0035
        pop edi
        pop esi
    }
            }
        } else {
            if (g_10122dc4 >= 0) {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 + g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov al, byte ptr g_10128664
    L0103:
        mov edx, dword ptr g_10128650
    L0109:
        cmp byte ptr [esi], 0xfe
        jae L0110
        mov byte ptr [edi], al
    L0110:
        dec esi
        inc edi
        dec edx
        jne L0109
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L0103
        pop edi
        pop esi
    }
            } else {
                s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_10128678;
                d = dst->bits8() + rc.left + rc.top * g_10128644;
                g_10128670 = rc.bottom - rc.top;
                g_10128650 = rc.right - rc.left;
                g_1012861c = g_10128678 - g_10128650;
                g_10128648 = g_10128644 - g_10128650;
    __asm {
        push esi
        push edi
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov al, byte ptr g_10128664
    L01c4:
        mov edx, dword ptr g_10128650
    L01ca:
        cmp byte ptr [esi], 0xfe
        jae L01d1
        mov byte ptr [edi], al
    L01d1:
        dec esi
        inc edi
        dec edx
        jne L01ca
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L01c4
        pop edi
        pop esi
    }
            }
        }
    } else {
        setRect(&rc, x + (int)g_1012868c, y + (int)g_10128690, (int)(m_width * g_1012868c), (int)(m_height * g_10128690));
        orig = rc;
        if (!intersect(&rc, &rc, dst->clipRect())) {
            dst->unlock(1);
            return 0;
        }
        d = dst->bits8() + rc.left + rc.top * g_10128644;
        g_10128644 = dst->pitch();
        g_10128650 = rc.right - rc.left;
        g_10128670 = rc.bottom - rc.top;
        g_10128648 = g_10128644 - g_10128650;
        g_10128688 = (g_10122dc8 << 16) / abs(g_10122dc0);
        g_10128624 = (g_10122dc8 << 16) / abs(g_10122dc4);
        if (g_10122dc0 >= 0) {
            if (g_10122dc4 >= 0) {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                g_1012861c = g_10128678 - g_10128660;
                s = m_bits + (g_10128640 >> 16) * g_10128678 + (g_1012863c >> 16);
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 - g_10128678 + g_1012861c;
                *g_10122e54 = *g_10122e58 + g_10128678;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128640
        shl edx, 0x10
        mov dx, word ptr g_1012863c
        mov ebx, dword ptr g_10128670
        shl ebx, 0x10
        mov bx, word ptr g_10128688
        mov ecx, dword ptr g_10128650
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128650, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012868a
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        mov al, byte ptr g_10128664
    L04aa:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L04c0:
        cmp byte ptr [esi], 0xfe
        jae L04c7
        mov byte ptr [edi], al
    L04c7:
        add dx, bx
        adc esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L04c0
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L04aa
        pop edi
        pop esi
    }
            } else {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                s = m_bits + (m_height - 1 - (g_10128640 >> 16)) * g_10128678 + (g_1012863c >> 16);
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 + g_10128660;
                *g_10122e54 = *g_10122e58 + g_10128678;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128640
        shl edx, 0x10
        mov dx, word ptr g_1012863c
        mov ebx, dword ptr g_10128670
        shl ebx, 0x10
        mov bx, word ptr g_10128688
        mov ecx, dword ptr g_10128650
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128650, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012868a
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        mov al, byte ptr g_10128664
    L0626:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L063c:
        cmp byte ptr [esi], 0xfe
        jae L0643
        mov byte ptr [edi], al
    L0643:
        add dx, bx
        adc esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L063c
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L0626
        pop edi
        pop esi
    }
            }
        } else {
            if (g_10122dc4 >= 0) {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                s = m_bits + (g_10128640 >> 16) * g_10128678 + m_width - 1 - (g_1012863c >> 16);
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 + g_10128660;
                *g_10122e54 = *g_10122e58 + g_10128678;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128640
        shl edx, 0x10
        mov dx, word ptr g_1012863c
        mov ebx, dword ptr g_10128670
        shl ebx, 0x10
        mov bx, word ptr g_10128688
        mov ecx, dword ptr g_10128650
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128650, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012868a
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        mov al, byte ptr g_10128664
    L07af:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L07c5:
        cmp byte ptr [esi], 0xfe
        jae L07cc
        mov byte ptr [edi], al
    L07cc:
        add dx, bx
        sbb esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L07c5
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L07af
        pop edi
        pop esi
    }
            } else {
                offX = rc.left - orig.left;
                offY = rc.top - orig.top;
                g_1012863c = g_10128688 * offX;
                g_10128640 = g_10128624 * offY;
                g_10128660 = (g_1012863c & 0xffff) + g_10128688 * g_10128650 >> 16;
                g_10128680 = (g_10128640 & 0xffff) + g_10128624 * g_10128670 >> 16;
                s = m_bits + (m_height - 1 - (g_10128640 >> 16)) * g_10128678 + m_width - 1 - (g_1012863c >> 16);
                g_1012861c = -g_10128678 + g_10128660;
                *g_10122e58 = (g_10128624 >> 16) * g_10128678 + -g_10128660;
                *g_10122e54 = *g_10122e58 + g_10128678;
    __asm {
        push esi
        push edi
        mov esi, dword ptr [ebp - 0x2c]
        mov edi, dword ptr [ebp - 0x28]
        mov edx, dword ptr g_10128640
        shl edx, 0x10
        mov dx, word ptr g_1012863c
        mov ebx, dword ptr g_10128670
        shl ebx, 0x10
        mov bx, word ptr g_10128688
        mov ecx, dword ptr g_10128650
        dec ecx
        shl ecx, 0x10
        mov dword ptr g_10128650, ecx
        xor ecx, ecx
        mov cx, word ptr g_1012868a
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        mov al, byte ptr g_10128664
    L094b:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L0961:
        cmp byte ptr [esi], 0xfe
        jae L0968
        mov byte ptr [edi], al
    L0968:
        add dx, bx
        sbb esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L0961
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L094b
        pop edi
        pop esi
    }
            }
        }
    }
    dst->unlock(2);
    return 0;
}

