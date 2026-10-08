// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitters onto 16-bit surfaces, family 3: the family-2 skeleton (jgld_blit16b.cpp) with a 5th int
// parameter kept in 0x10128574 as well. Names are chosen here.
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
    virtual void s5(); virtual void s6(); virtual void s7();
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
extern int g_10128548;
extern int g_10128574;
extern int g_1012858a;
class Sprite {
public:
    virtual ~Sprite();
    int draw16_10021720(Surface* dst, int x, int y, int p4, int p5);
    int draw16_10020430(Surface* dst, int x, int y, int p4, int p5);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x10021720 ?draw16_10021720@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::draw16_10021720(Surface* dst, int x, int y, int p4, int p5)
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
    g_10128574 = p5;
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
        push ebp
        mov ecx, dword ptr g_1012856c
    L1a36:
        mov edx, dword ptr g_10128564
    L1a3c:
        cmp byte ptr [esi], 0xfe
        jae L1a6d
        cmp byte ptr [esi], 0xf8
        jne L1a59
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L1a6d
    L1a59:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L1a6d:
        inc esi
        add edi, 2
        dec edx
        jne L1a3c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1a36
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
        mov ecx, dword ptr g_1012856c
    L1b2c:
        mov edx, dword ptr g_10128564
    L1b32:
        cmp byte ptr [esi], 0xfe
        jae L1b63
        cmp byte ptr [esi], 0xf8
        jne L1b4f
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L1b63
    L1b4f:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L1b63:
        inc esi
        add edi, 2
        dec edx
        jne L1b32
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1b2c
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
        mov ecx, dword ptr g_1012856c
    L1c2f:
        mov edx, dword ptr g_10128564
    L1c35:
        cmp byte ptr [esi], 0xfe
        jae L1c66
        cmp byte ptr [esi], 0xf8
        jne L1c52
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L1c66
    L1c52:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L1c66:
        dec esi
        add edi, 2
        dec edx
        jne L1c35
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1c2f
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
        mov ecx, dword ptr g_1012856c
    L1d25:
        mov edx, dword ptr g_10128564
    L1d2b:
        cmp byte ptr [esi], 0xfe
        jae L1d5c
        cmp byte ptr [esi], 0xf8
        jne L1d48
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L1d5c
    L1d48:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L1d5c:
        dec esi
        add edi, 2
        dec edx
        jne L1d2b
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1d25
        pop ebp
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L2046:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L205c:
        cmp byte ptr [esi], 0xfe
        jae L208f
        cmp byte ptr [esi], 0xf8
        jne L2079
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L208f
    L2079:
        xor eax, eax
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L208f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L205c
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2046
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L21fd:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2213:
        cmp byte ptr [esi], 0xfe
        jae L2246
        cmp byte ptr [esi], 0xf8
        jne L2230
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L2246
    L2230:
        xor eax, eax
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L2246:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2213
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L21fd
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L23c1:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L23d7:
        cmp byte ptr [esi], 0xfe
        jae L240a
        cmp byte ptr [esi], 0xf8
        jne L23f4
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L240a
    L23f4:
        xor eax, eax
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L240a:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L23d7
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L23c1
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L2598:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L25ae:
        cmp byte ptr [esi], 0xfe
        jae L25e1
        cmp byte ptr [esi], 0xf8
        jne L25cb
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L25e1
    L25cb:
        xor eax, eax
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov ax, word ptr [ebp + eax*2]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L25e1:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L25ae
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2598
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
// MATCH: jgld.dll 0x10020430 ?draw16_10020430@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::draw16_10020430(Surface* dst, int x, int y, int p4, int p5)
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
    g_10128574 = p5;
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
        push ebp
        mov ecx, dword ptr g_1012856c
    L0746:
        mov edx, dword ptr g_10128564
    L074c:
        cmp byte ptr [esi], 0xfe
        jae L077c
        cmp byte ptr [esi], 0xf8
        jne L0769
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L077c
    L0769:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L077c:
        inc esi
        add edi, 2
        dec edx
        jne L074c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0746
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
        mov ecx, dword ptr g_1012856c
    L083b:
        mov edx, dword ptr g_10128564
    L0841:
        cmp byte ptr [esi], 0xfe
        jae L0871
        cmp byte ptr [esi], 0xf8
        jne L085e
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L0871
    L085e:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L0871:
        inc esi
        add edi, 2
        dec edx
        jne L0841
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L083b
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
        mov ecx, dword ptr g_1012856c
    L093d:
        mov edx, dword ptr g_10128564
    L0943:
        cmp byte ptr [esi], 0xfe
        jae L0973
        cmp byte ptr [esi], 0xf8
        jne L0960
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L0973
    L0960:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L0973:
        dec esi
        add edi, 2
        dec edx
        jne L0943
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L093d
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
        mov ecx, dword ptr g_1012856c
    L0a32:
        mov edx, dword ptr g_10128564
    L0a38:
        cmp byte ptr [esi], 0xfe
        jae L0a68
        cmp byte ptr [esi], 0xf8
        jne L0a55
        mov ebp, dword ptr g_10128574
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L0a68
    L0a55:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L0a68:
        dec esi
        add edi, 2
        dec edx
        jne L0a38
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0a32
        pop ebp
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L0d52:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0d68:
        cmp byte ptr [esi], 0xfe
        jae L0d9a
        cmp byte ptr [esi], 0xf8
        jne L0d87
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        xor eax, eax
        jmp L0d9a
    L0d87:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L0d9a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0d68
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0d52
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L0f08:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0f1e:
        cmp byte ptr [esi], 0xfe
        jae L0f50
        cmp byte ptr [esi], 0xf8
        jne L0f3d
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        xor eax, eax
        jmp L0f50
    L0f3d:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L0f50:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0f1e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0f08
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L10cb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L10e1:
        cmp byte ptr [esi], 0xfe
        jae L1113
        cmp byte ptr [esi], 0xf8
        jne L1100
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        xor eax, eax
        jmp L1113
    L1100:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L1113:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L10e1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L10cb
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ecx, dword ptr g_1012856c
    L12a1:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L12b7:
        cmp byte ptr [esi], 0xfe
        jae L12e9
        cmp byte ptr [esi], 0xf8
        jne L12d6
        mov ebp, dword ptr g_10128574
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        xor eax, eax
        jmp L12e9
    L12d6:
        mov ebp, dword ptr g_10128548
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bp, word ptr [ecx + eax*2]
        mov word ptr [edi], bp
    L12e9:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L12b7
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L12a1
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

