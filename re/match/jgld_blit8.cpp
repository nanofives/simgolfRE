// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitters onto 8-bit surfaces: the jgld_blit16.cpp skeleton without the palette, with a byte
// destination (slot 6) and a 256x256 blend table parameter kept in 0x1012862c for the __asm loops. Names are chosen here.
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
extern int g_1012867c;
extern int g_1012868a;
class Sprite {
public:
    virtual ~Sprite();
    int draw8_10063f60(Surface* dst, int x, int y, unsigned char* table);
    int draw8_10061db0(Surface* dst, int x, int y, unsigned char* table);
    int draw8_10060cf0(Surface* dst, int x, int y, unsigned char* table);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x10063f60 ?draw8_10063f60@Sprite@@QAEHPAVSurface@@HHPAE@Z
int Sprite::draw8_10063f60(Surface* dst, int x, int y, unsigned char* table)
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
    g_1012862c = table;
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L41b0:
        mov edx, dword ptr g_10128650
    L41b6:
        cmp byte ptr [esi], 0xfe
        jae L41ca
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L41ca:
        inc esi
        inc edi
        dec edx
        jne L41b6
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L41b0
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L4281:
        mov edx, dword ptr g_10128650
    L4287:
        cmp byte ptr [esi], 0xfe
        jae L429b
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L429b:
        inc esi
        inc edi
        dec edx
        jne L4287
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L4281
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L435f:
        mov edx, dword ptr g_10128650
    L4365:
        cmp byte ptr [esi], 0xfe
        jae L4379
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L4379:
        dec esi
        inc edi
        dec edx
        jne L4365
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L435f
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L4430:
        mov edx, dword ptr g_10128650
    L4436:
        cmp byte ptr [esi], 0xfe
        jae L444a
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L444a:
        dec esi
        inc edi
        dec edx
        jne L4436
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L4430
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L4726:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L473c:
        cmp byte ptr [esi], 0xfe
        jae L4750
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L4750:
        add dx, bx
        adc esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L473c
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L4726
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L48b2:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L48c8:
        cmp byte ptr [esi], 0xfe
        jae L48dc
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L48dc:
        add dx, bx
        adc esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L48c8
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L48b2
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L4a4b:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L4a61:
        cmp byte ptr [esi], 0xfe
        jae L4a75
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L4a75:
        add dx, bx
        sbb esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L4a61
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L4a4b
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L4bf7:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L4c0d:
        cmp byte ptr [esi], 0xfe
        jae L4c21
        xor eax, eax
        mov al, byte ptr [esi]
        shl eax, 8
        mov al, byte ptr [edi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L4c21:
        add dx, bx
        sbb esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L4c0d
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L4bf7
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
// MATCH: jgld.dll 0x10061db0 ?draw8_10061db0@Sprite@@QAEHPAVSurface@@HHPAE@Z
int Sprite::draw8_10061db0(Surface* dst, int x, int y, unsigned char* table)
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
    g_1012862c = table;
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L2000:
        mov edx, dword ptr g_10128650
    L2006:
        cmp byte ptr [esi], 0xfe
        jae L2013
        mov bl, byte ptr [esi]
        mov al, byte ptr [ebp + ebx]
        mov byte ptr [edi], al
    L2013:
        inc esi
        inc edi
        dec edx
        jne L2006
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L2000
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L20ca:
        mov edx, dword ptr g_10128650
    L20d0:
        cmp byte ptr [esi], 0xfe
        jae L20dd
        mov bl, byte ptr [esi]
        mov al, byte ptr [ebp + ebx]
        mov byte ptr [edi], al
    L20dd:
        inc esi
        inc edi
        dec edx
        jne L20d0
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L20ca
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L21a1:
        mov edx, dword ptr g_10128650
    L21a7:
        cmp byte ptr [esi], 0xfe
        jae L21b4
        mov bl, byte ptr [esi]
        mov al, byte ptr [ebp + ebx]
        mov byte ptr [edi], al
    L21b4:
        dec esi
        inc edi
        dec edx
        jne L21a7
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L21a1
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L226b:
        mov edx, dword ptr g_10128650
    L2271:
        cmp byte ptr [esi], 0xfe
        jae L227e
        mov bl, byte ptr [esi]
        mov al, byte ptr [ebp + ebx]
        mov byte ptr [edi], al
    L227e:
        dec esi
        inc edi
        dec edx
        jne L2271
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L226b
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L255a:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L2570:
        cmp byte ptr [esi], 0xfe
        jae L257d
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L257d:
        add dx, bx
        adc esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L2570
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L255a
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L26df:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L26f5:
        cmp byte ptr [esi], 0xfe
        jae L2702
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L2702:
        add dx, bx
        adc esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L26f5
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L26df
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L2871:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L2887:
        cmp byte ptr [esi], 0xfe
        jae L2894
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L2894:
        add dx, bx
        sbb esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L2887
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L2871
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L2a16:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L2a2c:
        cmp byte ptr [esi], 0xfe
        jae L2a39
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov byte ptr [edi], al
    L2a39:
        add dx, bx
        sbb esi, ecx
        inc edi
        sub ebx, 0x10000
        jns L2a2c
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L2a16
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
// MATCH: jgld.dll 0x10060cf0 ?draw8_10060cf0@Sprite@@QAEHPAVSurface@@HHPAE@Z
int Sprite::draw8_10060cf0(Surface* dst, int x, int y, unsigned char* table)
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
    g_1012862c = table;
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L0f40:
        mov edx, dword ptr g_10128650
    L0f46:
        cmp byte ptr [esi], 0xfe
        jae L0f5e
        cmp byte ptr [esi], 0xf8
        jne L0f5a
        mov bl, byte ptr [edi]
        mov ah, byte ptr [ebp + ebx]
        mov byte ptr [edi], ah
        jmp L0f5e
    L0f5a:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L0f5e:
        inc esi
        inc edi
        dec edx
        jne L0f46
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L0f40
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L1015:
        mov edx, dword ptr g_10128650
    L101b:
        cmp byte ptr [esi], 0xfe
        jae L1033
        cmp byte ptr [esi], 0xf8
        jne L102f
        mov bl, byte ptr [edi]
        mov ah, byte ptr [ebp + ebx]
        mov byte ptr [edi], ah
        jmp L1033
    L102f:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L1033:
        inc esi
        inc edi
        dec edx
        jne L101b
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L1015
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L10f7:
        mov edx, dword ptr g_10128650
    L10fd:
        cmp byte ptr [esi], 0xfe
        jae L1115
        cmp byte ptr [esi], 0xf8
        jne L1111
        mov bl, byte ptr [edi]
        mov ah, byte ptr [ebp + ebx]
        mov byte ptr [edi], ah
        jmp L1115
    L1111:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L1115:
        dec esi
        inc edi
        dec edx
        jne L10fd
        add esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L10f7
        pop ebp
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
        push ebp
        mov ebp, dword ptr g_1012862c
    L11cc:
        mov edx, dword ptr g_10128650
    L11d2:
        cmp byte ptr [esi], 0xfe
        jae L11ea
        cmp byte ptr [esi], 0xf8
        jne L11e6
        mov bl, byte ptr [edi]
        mov ah, byte ptr [ebp + ebx]
        mov byte ptr [edi], ah
        jmp L11ea
    L11e6:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L11ea:
        dec esi
        inc edi
        dec edx
        jne L11d2
        sub esi, dword ptr g_1012861c
        add edi, dword ptr g_10128648
        dec dword ptr g_10128670
        jne L11cc
        pop ebp
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
        mov dword ptr g_1012867c, ecx
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        push ebp
        mov ebp, dword ptr g_1012862c
    L14cc:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L14e2:
        cmp byte ptr [esi], 0xfe
        jae L14fa
        cmp byte ptr [esi], 0xf8
        jne L14f6
        mov cl, byte ptr [edi]
        mov ah, byte ptr [ebp + ecx]
        mov byte ptr [edi], ah
        jmp L14fa
    L14f6:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L14fa:
        add dx, bx
        adc esi, dword ptr g_1012867c
        inc edi
        sub ebx, 0x10000
        jns L14e2
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L14cc
        pop ebp
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
        mov dword ptr g_1012867c, ecx
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        push ebp
        mov ebp, dword ptr g_1012862c
    L1666:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L167c:
        cmp byte ptr [esi], 0xfe
        jae L1694
        cmp byte ptr [esi], 0xf8
        jne L1690
        mov cl, byte ptr [edi]
        mov ah, byte ptr [ebp + ecx]
        mov byte ptr [edi], ah
        jmp L1694
    L1690:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L1694:
        add dx, bx
        adc esi, dword ptr g_1012867c
        inc edi
        sub ebx, 0x10000
        jns L167c
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L1666
        pop ebp
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
        mov dword ptr g_1012867c, ecx
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        push ebp
        mov ebp, dword ptr g_1012862c
    L180d:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L1823:
        cmp byte ptr [esi], 0xfe
        jae L183b
        cmp byte ptr [esi], 0xf8
        jne L1837
        mov cl, byte ptr [edi]
        mov ah, byte ptr [ebp + ecx]
        mov byte ptr [edi], ah
        jmp L183b
    L1837:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L183b:
        add dx, bx
        sbb esi, dword ptr g_1012867c
        inc edi
        sub ebx, 0x10000
        jns L1823
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L180d
        pop ebp
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
        mov dword ptr g_1012867c, ecx
        mov eax, dword ptr g_10128624
        shl eax, 0x10
        mov dword ptr g_10128628, eax
        push ebp
        mov ebp, dword ptr g_1012862c
    L19c7:
        xor eax, eax
        mov ebx, dword ptr g_10128650
        mov bx, word ptr g_10128688
        mov dx, word ptr g_1012863c
    L19dd:
        cmp byte ptr [esi], 0xfe
        jae L19f5
        cmp byte ptr [esi], 0xf8
        jne L19f1
        mov cl, byte ptr [edi]
        mov ah, byte ptr [ebp + ecx]
        mov byte ptr [edi], ah
        jmp L19f5
    L19f1:
        mov ah, byte ptr [esi]
        mov byte ptr [edi], ah
    L19f5:
        add dx, bx
        sbb esi, dword ptr g_1012867c
        inc edi
        sub ebx, 0x10000
        jns L19dd
        add edi, dword ptr g_10128648
        add edx, dword ptr g_10128628
        sbb eax, eax
        mov ebx, dword ptr g_10122e58
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128670
        jne L19c7
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

