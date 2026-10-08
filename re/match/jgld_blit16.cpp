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
    int draw16_1001bc40(Surface* dst, int x, int y);
    int draw16_1001cde0(Surface* dst, int x, int y);
    int draw16_10022a10(Surface* dst, int x, int y, int p4, int p5);
    int draw16_10024140(Surface* dst, int x, int y, int p4, int p5);
    int draw16_1002d890(Surface* dst, int x, int y, char flag);
    int draw16_1002ed60(Surface* dst, int x, int y, char flag);
    int draw16_10030290(Surface* dst, int x, int y, char flag);
    int draw16_100317c0(Surface* dst, int x, int y, char flag);
    int draw16_10032d60(Surface* dst, int x, int y, char flag);
    int draw16_10034300(Surface* dst, int x, int y, char flag);
    int draw16_100358b0(Surface* dst, int x, int y, char flag);
    int draw16_10036e60(Surface* dst, int x, int y, char flag);
    int draw16_10038460(Surface* dst, int x, int y, char flag);
    int draw16_10039a60(Surface* dst, int x, int y, char flag);
    int draw16_1003b060(Surface* dst, int x, int y, char flag);
    int draw16_1003c660(Surface* dst, int x, int y, char flag);
    int draw16_1003dca0(Surface* dst, int x, int y, char flag);
    int draw16_1003f2e0(Surface* dst, int x, int y, char flag);
    int draw16_10040930(Surface* dst, int x, int y, char flag);
    int draw16_10041f80(Surface* dst, int x, int y, char flag);
    int draw16_10043430(Surface* dst, int x, int y, char flag);
    int draw16_10044940(Surface* dst, int x, int y, char flag);
    int draw16_10045e50(Surface* dst, int x, int y, char flag);
    int draw16_100473b0(Surface* dst, int x, int y, char flag);
    int draw16_10048910(Surface* dst, int x, int y, char flag);
    int draw16_10049ea0(Surface* dst, int x, int y, char flag);
    int draw16_1004b430(Surface* dst, int x, int y, char flag);
    int draw16_1004ca10(Surface* dst, int x, int y, char flag);
    int draw16_1004dff0(Surface* dst, int x, int y, char flag);
    int draw16_1004f5d0(Surface* dst, int x, int y, char flag);
    int draw16_10050bb0(Surface* dst, int x, int y, char flag);
    int draw16_100521d0(Surface* dst, int x, int y, char flag);
    int draw16_100537f0(Surface* dst, int x, int y, char flag);
    int draw16_10054e20(Surface* dst, int x, int y, char flag);
    int draw16_10056450(Surface* dst, int x, int y);
    int draw16_100576a0(Surface* dst, int x, int y);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x1001bc40 ?draw16_1001bc40@Sprite@@QAEHPAVSurface@@HH@Z
int Sprite::draw16_1001bc40(Surface* dst, int x, int y)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lbf5b:
        mov edx, dword ptr g_10128564
    Lbf61:
        cmp byte ptr [esi], 0xfe
        jae Lbf71
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lbf71:
        inc esi
        add edi, 2
        dec edx
        jne Lbf61
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lbf5b
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
        mov ecx, dword ptr g_1012856c
    Lc030:
        mov edx, dword ptr g_10128564
    Lc036:
        cmp byte ptr [esi], 0xfe
        jae Lc046
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc046:
        inc esi
        add edi, 2
        dec edx
        jne Lc036
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc030
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
        mov ecx, dword ptr g_1012856c
    Lc112:
        mov edx, dword ptr g_10128564
    Lc118:
        cmp byte ptr [esi], 0xfe
        jae Lc128
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc128:
        dec esi
        add edi, 2
        dec edx
        jne Lc118
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc112
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
        mov ecx, dword ptr g_1012856c
    Lc1e7:
        mov edx, dword ptr g_10128564
    Lc1ed:
        cmp byte ptr [esi], 0xfe
        jae Lc1fd
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc1fd:
        dec esi
        add edi, 2
        dec edx
        jne Lc1ed
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc1e7
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lc4e7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc4fd:
        cmp byte ptr [esi], 0xfe
        jae Lc50d
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc50d:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc4fd
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc4e7
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
        mov ecx, dword ptr g_1012856c
    Lc67b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc691:
        cmp byte ptr [esi], 0xfe
        jae Lc6a1
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc6a1:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc691
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc67b
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
        mov ecx, dword ptr g_1012856c
    Lc81c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc832:
        cmp byte ptr [esi], 0xfe
        jae Lc842
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc842:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc832
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc81c
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
        mov ecx, dword ptr g_1012856c
    Lc9d0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc9e6:
        cmp byte ptr [esi], 0xfe
        jae Lc9f6
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Lc9f6:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc9e6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc9d0
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
// MATCH: jgld.dll 0x1001cde0 ?draw16_1001cde0@Sprite@@QAEHPAVSurface@@HH@Z
int Sprite::draw16_1001cde0(Surface* dst, int x, int y)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Ld0fb:
        mov edx, dword ptr g_10128564
    Ld101:
        cmp byte ptr [esi], 0xff
        jae Ld111
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld111:
        inc esi
        add edi, 2
        dec edx
        jne Ld101
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ld0fb
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
        mov ecx, dword ptr g_1012856c
    Ld1d0:
        mov edx, dword ptr g_10128564
    Ld1d6:
        cmp byte ptr [esi], 0xff
        jae Ld1e6
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld1e6:
        inc esi
        add edi, 2
        dec edx
        jne Ld1d6
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ld1d0
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
        mov ecx, dword ptr g_1012856c
    Ld2b2:
        mov edx, dword ptr g_10128564
    Ld2b8:
        cmp byte ptr [esi], 0xff
        jae Ld2c8
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld2c8:
        dec esi
        add edi, 2
        dec edx
        jne Ld2b8
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ld2b2
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
        mov ecx, dword ptr g_1012856c
    Ld387:
        mov edx, dword ptr g_10128564
    Ld38d:
        cmp byte ptr [esi], 0xff
        jae Ld39d
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld39d:
        dec esi
        add edi, 2
        dec edx
        jne Ld38d
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ld387
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Ld687:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld69d:
        cmp byte ptr [esi], 0xff
        jae Ld6ad
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld6ad:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld69d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld687
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
        mov ecx, dword ptr g_1012856c
    Ld81b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld831:
        cmp byte ptr [esi], 0xff
        jae Ld841
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld841:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld831
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld81b
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
        mov ecx, dword ptr g_1012856c
    Ld9bc:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld9d2:
        cmp byte ptr [esi], 0xff
        jae Ld9e2
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ld9e2:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld9d2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld9bc
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
        mov ecx, dword ptr g_1012856c
    Ldb70:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ldb86:
        cmp byte ptr [esi], 0xff
        jae Ldb96
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    Ldb96:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ldb86
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ldb70
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
// MATCH: jgld.dll 0x10022a10 ?draw16_10022a10@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::draw16_10022a10(Surface* dst, int x, int y, int p4, int p5)
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
        push ecx
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L2d33:
        mov edx, dword ptr g_10128564
    L2d39:
        cmp byte ptr [esi], 0xff
        jae L2dc6
        cmp byte ptr [esi], 0xf8
        jae L2dc6
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L2dc6:
        inc esi
        add edi, 2
        dec edx
        jne L2d39
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L2d33
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L2e96:
        mov edx, dword ptr g_10128564
    L2e9c:
        cmp byte ptr [esi], 0xff
        jae L2f29
        cmp byte ptr [esi], 0xf8
        jae L2f29
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L2f29:
        inc esi
        add edi, 2
        dec edx
        jne L2e9c
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L2e96
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L3006:
        mov edx, dword ptr g_10128564
    L300c:
        cmp byte ptr [esi], 0xff
        jae L3099
        cmp byte ptr [esi], 0xf8
        jae L3099
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L3099:
        dec esi
        add edi, 2
        dec edx
        jne L300c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3006
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L3169:
        mov edx, dword ptr g_10128564
    L316f:
        cmp byte ptr [esi], 0xff
        jae L31fc
        cmp byte ptr [esi], 0xf8
        jae L31fc
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L31fc:
        dec esi
        add edi, 2
        dec edx
        jne L316f
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3169
        pop ebp
        pop ecx
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
        push ecx
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L34f7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L350d:
        cmp byte ptr [esi], 0xff
        jae L359a
        cmp byte ptr [esi], 0xf8
        jae L359a
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L359a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L350d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L34f7
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L3719:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L372f:
        cmp byte ptr [esi], 0xff
        jae L37bc
        cmp byte ptr [esi], 0xf8
        jae L37bc
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L37bc:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L372f
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3719
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L3948:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L395e:
        cmp byte ptr [esi], 0xff
        jae L39eb
        cmp byte ptr [esi], 0xf8
        jae L39eb
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L39eb:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L395e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3948
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L3b8a:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L3ba0:
        cmp byte ptr [esi], 0xff
        jae L3c2d
        cmp byte ptr [esi], 0xf8
        jae L3c2d
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        xor eax, eax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x7c00
        and ax, 0x7c00
        shr cx, 0xb
        shr ax, 0xb
        add ax, cx
        shl ax, 0xa
        and bp, 0x83ff
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x3e0
        and ax, 0x3e0
        shr cx, 6
        shr ax, 6
        add ax, cx
        shl ax, 5
        and bp, 0xfc1f
        or bp, ax
        mov ecx, ebp
        shr ecx, 0x10
        mov ax, bp
        and cx, 0x1f
        and ax, 0x1f
        shr cx, 1
        shr ax, 1
        add ax, cx
        and bp, 0xffe0
        or bp, ax
        mov word ptr [edi], bp
        pop ecx
    L3c2d:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L3ba0
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3b8a
        pop ebp
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
// MATCH: jgld.dll 0x10024140 ?draw16_10024140@Sprite@@QAEHPAVSurface@@HHHH@Z
int Sprite::draw16_10024140(Surface* dst, int x, int y, int p4, int p5)
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
        push ecx
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L4463:
        mov edx, dword ptr g_10128564
    L4469:
        cmp byte ptr [esi], 0xff
        jae L4504
        cmp byte ptr [esi], 0xf8
        jae L4504
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L4504:
        inc esi
        add edi, 2
        dec edx
        jne L4469
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4463
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L45d4:
        mov edx, dword ptr g_10128564
    L45da:
        cmp byte ptr [esi], 0xff
        jae L4675
        cmp byte ptr [esi], 0xf8
        jae L4675
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L4675:
        inc esi
        add edi, 2
        dec edx
        jne L45da
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L45d4
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L4752:
        mov edx, dword ptr g_10128564
    L4758:
        cmp byte ptr [esi], 0xff
        jae L47f3
        cmp byte ptr [esi], 0xf8
        jae L47f3
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L47f3:
        dec esi
        add edi, 2
        dec edx
        jne L4758
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4752
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L48c3:
        mov edx, dword ptr g_10128564
    L48c9:
        cmp byte ptr [esi], 0xff
        jae L4964
        cmp byte ptr [esi], 0xf8
        jae L4964
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L4964:
        dec esi
        add edi, 2
        dec edx
        jne L48c9
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L48c3
        pop ebp
        pop ecx
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
        push ecx
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L4c5f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4c75:
        cmp byte ptr [esi], 0xff
        jae L4d10
        cmp byte ptr [esi], 0xf8
        jae L4d10
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L4d10:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4c75
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4c5f
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L4e8f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4ea5:
        cmp byte ptr [esi], 0xff
        jae L4f40
        cmp byte ptr [esi], 0xf8
        jae L4f40
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L4f40:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4ea5
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4e8f
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L50cc:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L50e2:
        cmp byte ptr [esi], 0xff
        jae L517d
        cmp byte ptr [esi], 0xf8
        jae L517d
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L517d:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L50e2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L50cc
        pop ebp
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
        push ebp
        mov bp, word ptr [ebp + 0x14]
        shl ebp, 0x10
        mov ecx, dword ptr g_1012856c
    L531c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5332:
        cmp byte ptr [esi], 0xff
        jae L53cd
        cmp byte ptr [esi], 0xf8
        jae L53cd
        xor eax, eax
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        push ecx
        push edx
        xor edx, edx
        xor ecx, ecx
        mov ax, bp
        and ax, 0x7c00
        shr ax, 7
        add dx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 1
        add dx, ax
        mov ax, bp
        and ax, 0x1f
        shl ax, 3
        add dx, ax
        shr dx, 2
        shr ebp, 0x10
        mov ax, bp
        and ax, 0x1f
        mul dl
        shr ax, 8
        or cx, ax
        mov ax, bp
        and ax, 0x3e0
        shr ax, 5
        mul dl
        shr ax, 8
        shl ax, 5
        or cx, ax
        mov ax, bp
        and ax, 0x7c00
        shr ax, 0xa
        mul dl
        shr ax, 8
        shl ax, 0xa
        or cx, ax
        shl ebp, 0x10
        mov bp, cx
        mov word ptr [edi], bp
        pop edx
        pop ecx
    L53cd:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5332
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L531c
        pop ebp
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
// MATCH: jgld.dll 0x1002d890 ?draw16_1002d890@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1002d890(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Ldbab:
        mov edx, dword ptr g_10128564
    Ldbb1:
        cmp byte ptr [esi], 0xff
        jae Ldc10
        or byte ptr [ebp + 0x14], 0
        je Ldbc1
        cmp byte ptr [esi], 0xf8
        jae Ldc10
    Ldbc1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ldc10:
        inc esi
        add edi, 2
        dec edx
        jne Ldbb1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ldbab
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
        mov ecx, dword ptr g_1012856c
    Ldccf:
        mov edx, dword ptr g_10128564
    Ldcd5:
        cmp byte ptr [esi], 0xff
        jae Ldd34
        or byte ptr [ebp + 0x14], 0
        je Ldce5
        cmp byte ptr [esi], 0xf8
        jae Ldd34
    Ldce5:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ldd34:
        inc esi
        add edi, 2
        dec edx
        jne Ldcd5
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ldccf
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
        mov ecx, dword ptr g_1012856c
    Lde00:
        mov edx, dword ptr g_10128564
    Lde06:
        cmp byte ptr [esi], 0xff
        jae Lde65
        or byte ptr [ebp + 0x14], 0
        je Lde16
        cmp byte ptr [esi], 0xf8
        jae Lde65
    Lde16:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lde65:
        dec esi
        add edi, 2
        dec edx
        jne Lde06
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lde00
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
        mov ecx, dword ptr g_1012856c
    Ldf24:
        mov edx, dword ptr g_10128564
    Ldf2a:
        cmp byte ptr [esi], 0xff
        jae Ldf89
        or byte ptr [ebp + 0x14], 0
        je Ldf3a
        cmp byte ptr [esi], 0xf8
        jae Ldf89
    Ldf3a:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ldf89:
        dec esi
        add edi, 2
        dec edx
        jne Ldf2a
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ldf24
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Le273:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Le289:
        cmp byte ptr [esi], 0xff
        jae Le2e8
        or byte ptr [ebp + 0x14], 0
        je Le299
        cmp byte ptr [esi], 0xf8
        jae Le2e8
    Le299:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le2e8:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Le289
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Le273
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
        mov ecx, dword ptr g_1012856c
    Le45a:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Le470:
        cmp byte ptr [esi], 0xff
        jae Le4cf
        or byte ptr [ebp + 0x14], 0
        je Le480
        cmp byte ptr [esi], 0xf8
        jae Le4cf
    Le480:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le4cf:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Le470
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Le45a
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
        mov ecx, dword ptr g_1012856c
    Le64e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Le664:
        cmp byte ptr [esi], 0xff
        jae Le6c3
        or byte ptr [ebp + 0x14], 0
        je Le674
        cmp byte ptr [esi], 0xf8
        jae Le6c3
    Le674:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le6c3:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Le664
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Le64e
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
        mov ecx, dword ptr g_1012856c
    Le855:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Le86b:
        cmp byte ptr [esi], 0xff
        jae Le8ca
        or byte ptr [ebp + 0x14], 0
        je Le87b
        cmp byte ptr [esi], 0xf8
        jae Le8ca
    Le87b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le8ca:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Le86b
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Le855
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
// MATCH: jgld.dll 0x1002ed60 ?draw16_1002ed60@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1002ed60(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lf07b:
        mov edx, dword ptr g_10128564
    Lf081:
        cmp byte ptr [esi], 0xff
        jae Lf0e8
        or byte ptr [ebp + 0x14], 0
        je Lf091
        cmp byte ptr [esi], 0xf8
        jae Lf0e8
    Lf091:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf0e8:
        inc esi
        add edi, 2
        dec edx
        jne Lf081
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf07b
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
        mov ecx, dword ptr g_1012856c
    Lf1ab:
        mov edx, dword ptr g_10128564
    Lf1b1:
        cmp byte ptr [esi], 0xff
        jae Lf218
        or byte ptr [ebp + 0x14], 0
        je Lf1c1
        cmp byte ptr [esi], 0xf8
        jae Lf218
    Lf1c1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf218:
        inc esi
        add edi, 2
        dec edx
        jne Lf1b1
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf1ab
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
        mov ecx, dword ptr g_1012856c
    Lf2e8:
        mov edx, dword ptr g_10128564
    Lf2ee:
        cmp byte ptr [esi], 0xff
        jae Lf355
        or byte ptr [ebp + 0x14], 0
        je Lf2fe
        cmp byte ptr [esi], 0xf8
        jae Lf355
    Lf2fe:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf355:
        dec esi
        add edi, 2
        dec edx
        jne Lf2ee
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf2e8
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
        mov ecx, dword ptr g_1012856c
    Lf418:
        mov edx, dword ptr g_10128564
    Lf41e:
        cmp byte ptr [esi], 0xff
        jae Lf485
        or byte ptr [ebp + 0x14], 0
        je Lf42e
        cmp byte ptr [esi], 0xf8
        jae Lf485
    Lf42e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf485:
        dec esi
        add edi, 2
        dec edx
        jne Lf41e
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf418
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lf773:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lf789:
        cmp byte ptr [esi], 0xff
        jae Lf7f0
        or byte ptr [ebp + 0x14], 0
        je Lf799
        cmp byte ptr [esi], 0xf8
        jae Lf7f0
    Lf799:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf7f0:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lf789
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lf773
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
        mov ecx, dword ptr g_1012856c
    Lf962:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lf978:
        cmp byte ptr [esi], 0xff
        jae Lf9df
        or byte ptr [ebp + 0x14], 0
        je Lf988
        cmp byte ptr [esi], 0xf8
        jae Lf9df
    Lf988:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf9df:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lf978
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lf962
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
        mov ecx, dword ptr g_1012856c
    Lfb5e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lfb74:
        cmp byte ptr [esi], 0xff
        jae Lfbdb
        or byte ptr [ebp + 0x14], 0
        je Lfb84
        cmp byte ptr [esi], 0xf8
        jae Lfbdb
    Lfb84:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfbdb:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lfb74
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lfb5e
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
        mov ecx, dword ptr g_1012856c
    Lfd6d:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lfd83:
        cmp byte ptr [esi], 0xff
        jae Lfdea
        or byte ptr [ebp + 0x14], 0
        je Lfd93
        cmp byte ptr [esi], 0xf8
        jae Lfdea
    Lfd93:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfdea:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lfd83
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lfd6d
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
// MATCH: jgld.dll 0x10030290 ?draw16_10030290@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10030290(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L05ab:
        mov edx, dword ptr g_10128564
    L05b1:
        cmp byte ptr [esi], 0xff
        jae L0618
        or byte ptr [ebp + 0x14], 0
        je L05c1
        cmp byte ptr [esi], 0xf8
        jae L0618
    L05c1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0618:
        inc esi
        add edi, 2
        dec edx
        jne L05b1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L05ab
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
        mov ecx, dword ptr g_1012856c
    L06db:
        mov edx, dword ptr g_10128564
    L06e1:
        cmp byte ptr [esi], 0xff
        jae L0748
        or byte ptr [ebp + 0x14], 0
        je L06f1
        cmp byte ptr [esi], 0xf8
        jae L0748
    L06f1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0748:
        inc esi
        add edi, 2
        dec edx
        jne L06e1
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L06db
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
        mov ecx, dword ptr g_1012856c
    L0818:
        mov edx, dword ptr g_10128564
    L081e:
        cmp byte ptr [esi], 0xff
        jae L0885
        or byte ptr [ebp + 0x14], 0
        je L082e
        cmp byte ptr [esi], 0xf8
        jae L0885
    L082e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0885:
        dec esi
        add edi, 2
        dec edx
        jne L081e
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0818
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
        mov ecx, dword ptr g_1012856c
    L0948:
        mov edx, dword ptr g_10128564
    L094e:
        cmp byte ptr [esi], 0xff
        jae L09b5
        or byte ptr [ebp + 0x14], 0
        je L095e
        cmp byte ptr [esi], 0xf8
        jae L09b5
    L095e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L09b5:
        dec esi
        add edi, 2
        dec edx
        jne L094e
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0948
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L0ca3:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0cb9:
        cmp byte ptr [esi], 0xff
        jae L0d20
        or byte ptr [ebp + 0x14], 0
        je L0cc9
        cmp byte ptr [esi], 0xf8
        jae L0d20
    L0cc9:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0d20:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0cb9
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0ca3
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
        mov ecx, dword ptr g_1012856c
    L0e92:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0ea8:
        cmp byte ptr [esi], 0xff
        jae L0f0f
        or byte ptr [ebp + 0x14], 0
        je L0eb8
        cmp byte ptr [esi], 0xf8
        jae L0f0f
    L0eb8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0f0f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0ea8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0e92
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
        mov ecx, dword ptr g_1012856c
    L108e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L10a4:
        cmp byte ptr [esi], 0xff
        jae L110b
        or byte ptr [ebp + 0x14], 0
        je L10b4
        cmp byte ptr [esi], 0xf8
        jae L110b
    L10b4:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L110b:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L10a4
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L108e
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
        mov ecx, dword ptr g_1012856c
    L129d:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L12b3:
        cmp byte ptr [esi], 0xff
        jae L131a
        or byte ptr [ebp + 0x14], 0
        je L12c3
        cmp byte ptr [esi], 0xf8
        jae L131a
    L12c3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L131a:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L12b3
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L129d
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
// MATCH: jgld.dll 0x100317c0 ?draw16_100317c0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_100317c0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L1adb:
        mov edx, dword ptr g_10128564
    L1ae1:
        cmp byte ptr [esi], 0xff
        jae L1b50
        or byte ptr [ebp + 0x14], 0
        je L1af1
        cmp byte ptr [esi], 0xf8
        jae L1b50
    L1af1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1b50:
        inc esi
        add edi, 2
        dec edx
        jne L1ae1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1adb
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
        mov ecx, dword ptr g_1012856c
    L1c13:
        mov edx, dword ptr g_10128564
    L1c19:
        cmp byte ptr [esi], 0xff
        jae L1c88
        or byte ptr [ebp + 0x14], 0
        je L1c29
        cmp byte ptr [esi], 0xf8
        jae L1c88
    L1c29:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1c88:
        inc esi
        add edi, 2
        dec edx
        jne L1c19
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1c13
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
        mov ecx, dword ptr g_1012856c
    L1d58:
        mov edx, dword ptr g_10128564
    L1d5e:
        cmp byte ptr [esi], 0xff
        jae L1dcd
        or byte ptr [ebp + 0x14], 0
        je L1d6e
        cmp byte ptr [esi], 0xf8
        jae L1dcd
    L1d6e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1dcd:
        dec esi
        add edi, 2
        dec edx
        jne L1d5e
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1d58
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
        mov ecx, dword ptr g_1012856c
    L1e90:
        mov edx, dword ptr g_10128564
    L1e96:
        cmp byte ptr [esi], 0xff
        jae L1f05
        or byte ptr [ebp + 0x14], 0
        je L1ea6
        cmp byte ptr [esi], 0xf8
        jae L1f05
    L1ea6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1f05:
        dec esi
        add edi, 2
        dec edx
        jne L1e96
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1e90
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L21f3:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2209:
        cmp byte ptr [esi], 0xff
        jae L2278
        or byte ptr [ebp + 0x14], 0
        je L2219
        cmp byte ptr [esi], 0xf8
        jae L2278
    L2219:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2278:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2209
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L21f3
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
        mov ecx, dword ptr g_1012856c
    L23ee:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2404:
        cmp byte ptr [esi], 0xff
        jae L2473
        or byte ptr [ebp + 0x14], 0
        je L2414
        cmp byte ptr [esi], 0xf8
        jae L2473
    L2414:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2473:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2404
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L23ee
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
        mov ecx, dword ptr g_1012856c
    L25f6:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L260c:
        cmp byte ptr [esi], 0xff
        jae L267b
        or byte ptr [ebp + 0x14], 0
        je L261c
        cmp byte ptr [esi], 0xf8
        jae L267b
    L261c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L267b:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L260c
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L25f6
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
        mov ecx, dword ptr g_1012856c
    L2811:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2827:
        cmp byte ptr [esi], 0xff
        jae L2896
        or byte ptr [ebp + 0x14], 0
        je L2837
        cmp byte ptr [esi], 0xf8
        jae L2896
    L2837:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2896:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2827
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2811
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
// MATCH: jgld.dll 0x10032d60 ?draw16_10032d60@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10032d60(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L307b:
        mov edx, dword ptr g_10128564
    L3081:
        cmp byte ptr [esi], 0xff
        jae L30f0
        or byte ptr [ebp + 0x14], 0
        je L3091
        cmp byte ptr [esi], 0xf8
        jae L30f0
    L3091:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L30f0:
        inc esi
        add edi, 2
        dec edx
        jne L3081
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L307b
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
        mov ecx, dword ptr g_1012856c
    L31b3:
        mov edx, dword ptr g_10128564
    L31b9:
        cmp byte ptr [esi], 0xff
        jae L3228
        or byte ptr [ebp + 0x14], 0
        je L31c9
        cmp byte ptr [esi], 0xf8
        jae L3228
    L31c9:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3228:
        inc esi
        add edi, 2
        dec edx
        jne L31b9
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L31b3
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
        mov ecx, dword ptr g_1012856c
    L32f8:
        mov edx, dword ptr g_10128564
    L32fe:
        cmp byte ptr [esi], 0xff
        jae L336d
        or byte ptr [ebp + 0x14], 0
        je L330e
        cmp byte ptr [esi], 0xf8
        jae L336d
    L330e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L336d:
        dec esi
        add edi, 2
        dec edx
        jne L32fe
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L32f8
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
        mov ecx, dword ptr g_1012856c
    L3430:
        mov edx, dword ptr g_10128564
    L3436:
        cmp byte ptr [esi], 0xff
        jae L34a5
        or byte ptr [ebp + 0x14], 0
        je L3446
        cmp byte ptr [esi], 0xf8
        jae L34a5
    L3446:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L34a5:
        dec esi
        add edi, 2
        dec edx
        jne L3436
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3430
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L3793:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L37a9:
        cmp byte ptr [esi], 0xff
        jae L3818
        or byte ptr [ebp + 0x14], 0
        je L37b9
        cmp byte ptr [esi], 0xf8
        jae L3818
    L37b9:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3818:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L37a9
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3793
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
        mov ecx, dword ptr g_1012856c
    L398e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L39a4:
        cmp byte ptr [esi], 0xff
        jae L3a13
        or byte ptr [ebp + 0x14], 0
        je L39b4
        cmp byte ptr [esi], 0xf8
        jae L3a13
    L39b4:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3a13:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L39a4
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L398e
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
        mov ecx, dword ptr g_1012856c
    L3b96:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L3bac:
        cmp byte ptr [esi], 0xff
        jae L3c1b
        or byte ptr [ebp + 0x14], 0
        je L3bbc
        cmp byte ptr [esi], 0xf8
        jae L3c1b
    L3bbc:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3c1b:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L3bac
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3b96
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
        mov ecx, dword ptr g_1012856c
    L3db1:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L3dc7:
        cmp byte ptr [esi], 0xff
        jae L3e36
        or byte ptr [ebp + 0x14], 0
        je L3dd7
        cmp byte ptr [esi], 0xf8
        jae L3e36
    L3dd7:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3e36:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L3dc7
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3db1
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
// MATCH: jgld.dll 0x10034300 ?draw16_10034300@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10034300(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L461b:
        mov edx, dword ptr g_10128564
    L4621:
        cmp byte ptr [esi], 0xff
        jae L4692
        or byte ptr [ebp + 0x14], 0
        je L4631
        cmp byte ptr [esi], 0xf8
        jae L4692
    L4631:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4692:
        inc esi
        add edi, 2
        dec edx
        jne L4621
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L461b
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
        mov ecx, dword ptr g_1012856c
    L4755:
        mov edx, dword ptr g_10128564
    L475b:
        cmp byte ptr [esi], 0xff
        jae L47cc
        or byte ptr [ebp + 0x14], 0
        je L476b
        cmp byte ptr [esi], 0xf8
        jae L47cc
    L476b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L47cc:
        inc esi
        add edi, 2
        dec edx
        jne L475b
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4755
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
        mov ecx, dword ptr g_1012856c
    L489c:
        mov edx, dword ptr g_10128564
    L48a2:
        cmp byte ptr [esi], 0xff
        jae L4913
        or byte ptr [ebp + 0x14], 0
        je L48b2
        cmp byte ptr [esi], 0xf8
        jae L4913
    L48b2:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4913:
        dec esi
        add edi, 2
        dec edx
        jne L48a2
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L489c
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
        mov ecx, dword ptr g_1012856c
    L49d6:
        mov edx, dword ptr g_10128564
    L49dc:
        cmp byte ptr [esi], 0xff
        jae L4a4d
        or byte ptr [ebp + 0x14], 0
        je L49ec
        cmp byte ptr [esi], 0xf8
        jae L4a4d
    L49ec:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4a4d:
        dec esi
        add edi, 2
        dec edx
        jne L49dc
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L49d6
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L4d3b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4d51:
        cmp byte ptr [esi], 0xff
        jae L4dc2
        or byte ptr [ebp + 0x14], 0
        je L4d61
        cmp byte ptr [esi], 0xf8
        jae L4dc2
    L4d61:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4dc2:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4d51
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4d3b
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
        mov ecx, dword ptr g_1012856c
    L4f38:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4f4e:
        cmp byte ptr [esi], 0xff
        jae L4fbf
        or byte ptr [ebp + 0x14], 0
        je L4f5e
        cmp byte ptr [esi], 0xf8
        jae L4fbf
    L4f5e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4fbf:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4f4e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4f38
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
        mov ecx, dword ptr g_1012856c
    L5142:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5158:
        cmp byte ptr [esi], 0xff
        jae L51c9
        or byte ptr [ebp + 0x14], 0
        je L5168
        cmp byte ptr [esi], 0xf8
        jae L51c9
    L5168:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L51c9:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5158
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5142
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
        mov ecx, dword ptr g_1012856c
    L535f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5375:
        cmp byte ptr [esi], 0xff
        jae L53e6
        or byte ptr [ebp + 0x14], 0
        je L5385
        cmp byte ptr [esi], 0xf8
        jae L53e6
    L5385:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L53e6:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5375
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L535f
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
// MATCH: jgld.dll 0x100358b0 ?draw16_100358b0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_100358b0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L5bcb:
        mov edx, dword ptr g_10128564
    L5bd1:
        cmp byte ptr [esi], 0xff
        jae L5c42
        or byte ptr [ebp + 0x14], 0
        je L5be1
        cmp byte ptr [esi], 0xf8
        jae L5c42
    L5be1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5c42:
        inc esi
        add edi, 2
        dec edx
        jne L5bd1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L5bcb
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
        mov ecx, dword ptr g_1012856c
    L5d05:
        mov edx, dword ptr g_10128564
    L5d0b:
        cmp byte ptr [esi], 0xff
        jae L5d7c
        or byte ptr [ebp + 0x14], 0
        je L5d1b
        cmp byte ptr [esi], 0xf8
        jae L5d7c
    L5d1b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5d7c:
        inc esi
        add edi, 2
        dec edx
        jne L5d0b
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L5d05
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
        mov ecx, dword ptr g_1012856c
    L5e4c:
        mov edx, dword ptr g_10128564
    L5e52:
        cmp byte ptr [esi], 0xff
        jae L5ec3
        or byte ptr [ebp + 0x14], 0
        je L5e62
        cmp byte ptr [esi], 0xf8
        jae L5ec3
    L5e62:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5ec3:
        dec esi
        add edi, 2
        dec edx
        jne L5e52
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L5e4c
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
        mov ecx, dword ptr g_1012856c
    L5f86:
        mov edx, dword ptr g_10128564
    L5f8c:
        cmp byte ptr [esi], 0xff
        jae L5ffd
        or byte ptr [ebp + 0x14], 0
        je L5f9c
        cmp byte ptr [esi], 0xf8
        jae L5ffd
    L5f9c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5ffd:
        dec esi
        add edi, 2
        dec edx
        jne L5f8c
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L5f86
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L62eb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6301:
        cmp byte ptr [esi], 0xff
        jae L6372
        or byte ptr [ebp + 0x14], 0
        je L6311
        cmp byte ptr [esi], 0xf8
        jae L6372
    L6311:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6372:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6301
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L62eb
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
        mov ecx, dword ptr g_1012856c
    L64e8:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L64fe:
        cmp byte ptr [esi], 0xff
        jae L656f
        or byte ptr [ebp + 0x14], 0
        je L650e
        cmp byte ptr [esi], 0xf8
        jae L656f
    L650e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L656f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L64fe
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L64e8
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
        mov ecx, dword ptr g_1012856c
    L66f2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6708:
        cmp byte ptr [esi], 0xff
        jae L6779
        or byte ptr [ebp + 0x14], 0
        je L6718
        cmp byte ptr [esi], 0xf8
        jae L6779
    L6718:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6779:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6708
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L66f2
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
        mov ecx, dword ptr g_1012856c
    L690f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6925:
        cmp byte ptr [esi], 0xff
        jae L6996
        or byte ptr [ebp + 0x14], 0
        je L6935
        cmp byte ptr [esi], 0xf8
        jae L6996
    L6935:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6996:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6925
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L690f
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
// MATCH: jgld.dll 0x10036e60 ?draw16_10036e60@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10036e60(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L717b:
        mov edx, dword ptr g_10128564
    L7181:
        cmp byte ptr [esi], 0xff
        jae L71fa
        or byte ptr [ebp + 0x14], 0
        je L7191
        cmp byte ptr [esi], 0xf8
        jae L71fa
    L7191:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L71fa:
        inc esi
        add edi, 2
        dec edx
        jne L7181
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L717b
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
        mov ecx, dword ptr g_1012856c
    L72bd:
        mov edx, dword ptr g_10128564
    L72c3:
        cmp byte ptr [esi], 0xff
        jae L733c
        or byte ptr [ebp + 0x14], 0
        je L72d3
        cmp byte ptr [esi], 0xf8
        jae L733c
    L72d3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L733c:
        inc esi
        add edi, 2
        dec edx
        jne L72c3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L72bd
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
        mov ecx, dword ptr g_1012856c
    L740c:
        mov edx, dword ptr g_10128564
    L7412:
        cmp byte ptr [esi], 0xff
        jae L748b
        or byte ptr [ebp + 0x14], 0
        je L7422
        cmp byte ptr [esi], 0xf8
        jae L748b
    L7422:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L748b:
        dec esi
        add edi, 2
        dec edx
        jne L7412
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L740c
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
        mov ecx, dword ptr g_1012856c
    L754e:
        mov edx, dword ptr g_10128564
    L7554:
        cmp byte ptr [esi], 0xff
        jae L75cd
        or byte ptr [ebp + 0x14], 0
        je L7564
        cmp byte ptr [esi], 0xf8
        jae L75cd
    L7564:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L75cd:
        dec esi
        add edi, 2
        dec edx
        jne L7554
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L754e
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L78bb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L78d1:
        cmp byte ptr [esi], 0xff
        jae L794a
        or byte ptr [ebp + 0x14], 0
        je L78e1
        cmp byte ptr [esi], 0xf8
        jae L794a
    L78e1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L794a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L78d1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L78bb
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
        mov ecx, dword ptr g_1012856c
    L7ac0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7ad6:
        cmp byte ptr [esi], 0xff
        jae L7b4f
        or byte ptr [ebp + 0x14], 0
        je L7ae6
        cmp byte ptr [esi], 0xf8
        jae L7b4f
    L7ae6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L7b4f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7ad6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7ac0
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
        mov ecx, dword ptr g_1012856c
    L7cd2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7ce8:
        cmp byte ptr [esi], 0xff
        jae L7d61
        or byte ptr [ebp + 0x14], 0
        je L7cf8
        cmp byte ptr [esi], 0xf8
        jae L7d61
    L7cf8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L7d61:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7ce8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7cd2
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
        mov ecx, dword ptr g_1012856c
    L7ef7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7f0d:
        cmp byte ptr [esi], 0xff
        jae L7f86
        or byte ptr [ebp + 0x14], 0
        je L7f1d
        cmp byte ptr [esi], 0xf8
        jae L7f86
    L7f1d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L7f86:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7f0d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7ef7
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
// MATCH: jgld.dll 0x10038460 ?draw16_10038460@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10038460(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L877b:
        mov edx, dword ptr g_10128564
    L8781:
        cmp byte ptr [esi], 0xff
        jae L87fa
        or byte ptr [ebp + 0x14], 0
        je L8791
        cmp byte ptr [esi], 0xf8
        jae L87fa
    L8791:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L87fa:
        inc esi
        add edi, 2
        dec edx
        jne L8781
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L877b
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
        mov ecx, dword ptr g_1012856c
    L88bd:
        mov edx, dword ptr g_10128564
    L88c3:
        cmp byte ptr [esi], 0xff
        jae L893c
        or byte ptr [ebp + 0x14], 0
        je L88d3
        cmp byte ptr [esi], 0xf8
        jae L893c
    L88d3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L893c:
        inc esi
        add edi, 2
        dec edx
        jne L88c3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L88bd
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
        mov ecx, dword ptr g_1012856c
    L8a0c:
        mov edx, dword ptr g_10128564
    L8a12:
        cmp byte ptr [esi], 0xff
        jae L8a8b
        or byte ptr [ebp + 0x14], 0
        je L8a22
        cmp byte ptr [esi], 0xf8
        jae L8a8b
    L8a22:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8a8b:
        dec esi
        add edi, 2
        dec edx
        jne L8a12
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8a0c
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
        mov ecx, dword ptr g_1012856c
    L8b4e:
        mov edx, dword ptr g_10128564
    L8b54:
        cmp byte ptr [esi], 0xff
        jae L8bcd
        or byte ptr [ebp + 0x14], 0
        je L8b64
        cmp byte ptr [esi], 0xf8
        jae L8bcd
    L8b64:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8bcd:
        dec esi
        add edi, 2
        dec edx
        jne L8b54
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8b4e
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L8ebb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8ed1:
        cmp byte ptr [esi], 0xff
        jae L8f4a
        or byte ptr [ebp + 0x14], 0
        je L8ee1
        cmp byte ptr [esi], 0xf8
        jae L8f4a
    L8ee1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8f4a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8ed1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L8ebb
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
        mov ecx, dword ptr g_1012856c
    L90c0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L90d6:
        cmp byte ptr [esi], 0xff
        jae L914f
        or byte ptr [ebp + 0x14], 0
        je L90e6
        cmp byte ptr [esi], 0xf8
        jae L914f
    L90e6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L914f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L90d6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L90c0
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
        mov ecx, dword ptr g_1012856c
    L92d2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L92e8:
        cmp byte ptr [esi], 0xff
        jae L9361
        or byte ptr [ebp + 0x14], 0
        je L92f8
        cmp byte ptr [esi], 0xf8
        jae L9361
    L92f8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L9361:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L92e8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L92d2
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
        mov ecx, dword ptr g_1012856c
    L94f7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L950d:
        cmp byte ptr [esi], 0xff
        jae L9586
        or byte ptr [ebp + 0x14], 0
        je L951d
        cmp byte ptr [esi], 0xf8
        jae L9586
    L951d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L9586:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L950d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L94f7
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
// MATCH: jgld.dll 0x10039a60 ?draw16_10039a60@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10039a60(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L9d7b:
        mov edx, dword ptr g_10128564
    L9d81:
        cmp byte ptr [esi], 0xff
        jae L9dfa
        or byte ptr [ebp + 0x14], 0
        je L9d91
        cmp byte ptr [esi], 0xf8
        jae L9dfa
    L9d91:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L9dfa:
        inc esi
        add edi, 2
        dec edx
        jne L9d81
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L9d7b
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
        mov ecx, dword ptr g_1012856c
    L9ebd:
        mov edx, dword ptr g_10128564
    L9ec3:
        cmp byte ptr [esi], 0xff
        jae L9f3c
        or byte ptr [ebp + 0x14], 0
        je L9ed3
        cmp byte ptr [esi], 0xf8
        jae L9f3c
    L9ed3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L9f3c:
        inc esi
        add edi, 2
        dec edx
        jne L9ec3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L9ebd
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
        mov ecx, dword ptr g_1012856c
    La00c:
        mov edx, dword ptr g_10128564
    La012:
        cmp byte ptr [esi], 0xff
        jae La08b
        or byte ptr [ebp + 0x14], 0
        je La022
        cmp byte ptr [esi], 0xf8
        jae La08b
    La022:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La08b:
        dec esi
        add edi, 2
        dec edx
        jne La012
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne La00c
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
        mov ecx, dword ptr g_1012856c
    La14e:
        mov edx, dword ptr g_10128564
    La154:
        cmp byte ptr [esi], 0xff
        jae La1cd
        or byte ptr [ebp + 0x14], 0
        je La164
        cmp byte ptr [esi], 0xf8
        jae La1cd
    La164:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La1cd:
        dec esi
        add edi, 2
        dec edx
        jne La154
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne La14e
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    La4bb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La4d1:
        cmp byte ptr [esi], 0xff
        jae La54a
        or byte ptr [ebp + 0x14], 0
        je La4e1
        cmp byte ptr [esi], 0xf8
        jae La54a
    La4e1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La54a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La4d1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La4bb
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
        mov ecx, dword ptr g_1012856c
    La6c0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La6d6:
        cmp byte ptr [esi], 0xff
        jae La74f
        or byte ptr [ebp + 0x14], 0
        je La6e6
        cmp byte ptr [esi], 0xf8
        jae La74f
    La6e6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La74f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La6d6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La6c0
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
        mov ecx, dword ptr g_1012856c
    La8d2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La8e8:
        cmp byte ptr [esi], 0xff
        jae La961
        or byte ptr [ebp + 0x14], 0
        je La8f8
        cmp byte ptr [esi], 0xf8
        jae La961
    La8f8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La961:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La8e8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La8d2
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
        mov ecx, dword ptr g_1012856c
    Laaf7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lab0d:
        cmp byte ptr [esi], 0xff
        jae Lab86
        or byte ptr [ebp + 0x14], 0
        je Lab1d
        cmp byte ptr [esi], 0xf8
        jae Lab86
    Lab1d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lab86:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lab0d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Laaf7
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
// MATCH: jgld.dll 0x1003b060 ?draw16_1003b060@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1003b060(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lb37b:
        mov edx, dword ptr g_10128564
    Lb381:
        cmp byte ptr [esi], 0xff
        jae Lb3fa
        or byte ptr [ebp + 0x14], 0
        je Lb391
        cmp byte ptr [esi], 0xf8
        jae Lb3fa
    Lb391:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lb3fa:
        inc esi
        add edi, 2
        dec edx
        jne Lb381
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb37b
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
        mov ecx, dword ptr g_1012856c
    Lb4bd:
        mov edx, dword ptr g_10128564
    Lb4c3:
        cmp byte ptr [esi], 0xff
        jae Lb53c
        or byte ptr [ebp + 0x14], 0
        je Lb4d3
        cmp byte ptr [esi], 0xf8
        jae Lb53c
    Lb4d3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lb53c:
        inc esi
        add edi, 2
        dec edx
        jne Lb4c3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb4bd
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
        mov ecx, dword ptr g_1012856c
    Lb60c:
        mov edx, dword ptr g_10128564
    Lb612:
        cmp byte ptr [esi], 0xff
        jae Lb68b
        or byte ptr [ebp + 0x14], 0
        je Lb622
        cmp byte ptr [esi], 0xf8
        jae Lb68b
    Lb622:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lb68b:
        dec esi
        add edi, 2
        dec edx
        jne Lb612
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb60c
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
        mov ecx, dword ptr g_1012856c
    Lb74e:
        mov edx, dword ptr g_10128564
    Lb754:
        cmp byte ptr [esi], 0xff
        jae Lb7cd
        or byte ptr [ebp + 0x14], 0
        je Lb764
        cmp byte ptr [esi], 0xf8
        jae Lb7cd
    Lb764:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lb7cd:
        dec esi
        add edi, 2
        dec edx
        jne Lb754
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb74e
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lbabb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lbad1:
        cmp byte ptr [esi], 0xff
        jae Lbb4a
        or byte ptr [ebp + 0x14], 0
        je Lbae1
        cmp byte ptr [esi], 0xf8
        jae Lbb4a
    Lbae1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lbb4a:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lbad1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lbabb
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
        mov ecx, dword ptr g_1012856c
    Lbcc0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lbcd6:
        cmp byte ptr [esi], 0xff
        jae Lbd4f
        or byte ptr [ebp + 0x14], 0
        je Lbce6
        cmp byte ptr [esi], 0xf8
        jae Lbd4f
    Lbce6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lbd4f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lbcd6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lbcc0
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
        mov ecx, dword ptr g_1012856c
    Lbed2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lbee8:
        cmp byte ptr [esi], 0xff
        jae Lbf61
        or byte ptr [ebp + 0x14], 0
        je Lbef8
        cmp byte ptr [esi], 0xf8
        jae Lbf61
    Lbef8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lbf61:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lbee8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lbed2
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
        mov ecx, dword ptr g_1012856c
    Lc0f7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc10d:
        cmp byte ptr [esi], 0xff
        jae Lc186
        or byte ptr [ebp + 0x14], 0
        je Lc11d
        cmp byte ptr [esi], 0xf8
        jae Lc186
    Lc11d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lc186:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc10d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc0f7
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
// MATCH: jgld.dll 0x1003c660 ?draw16_1003c660@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1003c660(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lc97b:
        mov edx, dword ptr g_10128564
    Lc981:
        cmp byte ptr [esi], 0xff
        jae Lc9fe
        or byte ptr [ebp + 0x14], 0
        je Lc991
        cmp byte ptr [esi], 0xf8
        jae Lc9fe
    Lc991:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lc9fe:
        inc esi
        add edi, 2
        dec edx
        jne Lc981
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lc97b
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
        mov ecx, dword ptr g_1012856c
    Lcac5:
        mov edx, dword ptr g_10128564
    Lcacb:
        cmp byte ptr [esi], 0xff
        jae Lcb48
        or byte ptr [ebp + 0x14], 0
        je Lcadb
        cmp byte ptr [esi], 0xf8
        jae Lcb48
    Lcadb:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lcb48:
        inc esi
        add edi, 2
        dec edx
        jne Lcacb
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lcac5
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
        mov ecx, dword ptr g_1012856c
    Lcc1c:
        mov edx, dword ptr g_10128564
    Lcc22:
        cmp byte ptr [esi], 0xff
        jae Lcc9f
        or byte ptr [ebp + 0x14], 0
        je Lcc32
        cmp byte ptr [esi], 0xf8
        jae Lcc9f
    Lcc32:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lcc9f:
        dec esi
        add edi, 2
        dec edx
        jne Lcc22
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lcc1c
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
        mov ecx, dword ptr g_1012856c
    Lcd66:
        mov edx, dword ptr g_10128564
    Lcd6c:
        cmp byte ptr [esi], 0xff
        jae Lcde9
        or byte ptr [ebp + 0x14], 0
        je Lcd7c
        cmp byte ptr [esi], 0xf8
        jae Lcde9
    Lcd7c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lcde9:
        dec esi
        add edi, 2
        dec edx
        jne Lcd6c
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lcd66
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Ld0db:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld0f1:
        cmp byte ptr [esi], 0xff
        jae Ld16e
        or byte ptr [ebp + 0x14], 0
        je Ld101
        cmp byte ptr [esi], 0xf8
        jae Ld16e
    Ld101:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld16e:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld0f1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld0db
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
        mov ecx, dword ptr g_1012856c
    Ld2e4:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld2fa:
        cmp byte ptr [esi], 0xff
        jae Ld377
        or byte ptr [ebp + 0x14], 0
        je Ld30a
        cmp byte ptr [esi], 0xf8
        jae Ld377
    Ld30a:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld377:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld2fa
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld2e4
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
        mov ecx, dword ptr g_1012856c
    Ld4fa:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld510:
        cmp byte ptr [esi], 0xff
        jae Ld58d
        or byte ptr [ebp + 0x14], 0
        je Ld520
        cmp byte ptr [esi], 0xf8
        jae Ld58d
    Ld520:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld58d:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld510
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld4fa
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
        mov ecx, dword ptr g_1012856c
    Ld723:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld739:
        cmp byte ptr [esi], 0xff
        jae Ld7b6
        or byte ptr [ebp + 0x14], 0
        je Ld749
        cmp byte ptr [esi], 0xf8
        jae Ld7b6
    Ld749:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld7b6:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld739
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld723
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
// MATCH: jgld.dll 0x1003dca0 ?draw16_1003dca0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1003dca0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Ldfbb:
        mov edx, dword ptr g_10128564
    Ldfc1:
        cmp byte ptr [esi], 0xff
        jae Le03e
        or byte ptr [ebp + 0x14], 0
        je Ldfd1
        cmp byte ptr [esi], 0xf8
        jae Le03e
    Ldfd1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le03e:
        inc esi
        add edi, 2
        dec edx
        jne Ldfc1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ldfbb
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
        mov ecx, dword ptr g_1012856c
    Le105:
        mov edx, dword ptr g_10128564
    Le10b:
        cmp byte ptr [esi], 0xff
        jae Le188
        or byte ptr [ebp + 0x14], 0
        je Le11b
        cmp byte ptr [esi], 0xf8
        jae Le188
    Le11b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le188:
        inc esi
        add edi, 2
        dec edx
        jne Le10b
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le105
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
        mov ecx, dword ptr g_1012856c
    Le25c:
        mov edx, dword ptr g_10128564
    Le262:
        cmp byte ptr [esi], 0xff
        jae Le2df
        or byte ptr [ebp + 0x14], 0
        je Le272
        cmp byte ptr [esi], 0xf8
        jae Le2df
    Le272:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le2df:
        dec esi
        add edi, 2
        dec edx
        jne Le262
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le25c
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
        mov ecx, dword ptr g_1012856c
    Le3a6:
        mov edx, dword ptr g_10128564
    Le3ac:
        cmp byte ptr [esi], 0xff
        jae Le429
        or byte ptr [ebp + 0x14], 0
        je Le3bc
        cmp byte ptr [esi], 0xf8
        jae Le429
    Le3bc:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le429:
        dec esi
        add edi, 2
        dec edx
        jne Le3ac
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le3a6
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Le71b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Le731:
        cmp byte ptr [esi], 0xff
        jae Le7ae
        or byte ptr [ebp + 0x14], 0
        je Le741
        cmp byte ptr [esi], 0xf8
        jae Le7ae
    Le741:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le7ae:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Le731
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Le71b
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
        mov ecx, dword ptr g_1012856c
    Le924:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Le93a:
        cmp byte ptr [esi], 0xff
        jae Le9b7
        or byte ptr [ebp + 0x14], 0
        je Le94a
        cmp byte ptr [esi], 0xf8
        jae Le9b7
    Le94a:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le9b7:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Le93a
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Le924
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
        mov ecx, dword ptr g_1012856c
    Leb3a:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Leb50:
        cmp byte ptr [esi], 0xff
        jae Lebcd
        or byte ptr [ebp + 0x14], 0
        je Leb60
        cmp byte ptr [esi], 0xf8
        jae Lebcd
    Leb60:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lebcd:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Leb50
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Leb3a
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
        mov ecx, dword ptr g_1012856c
    Led63:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Led79:
        cmp byte ptr [esi], 0xff
        jae Ledf6
        or byte ptr [ebp + 0x14], 0
        je Led89
        cmp byte ptr [esi], 0xf8
        jae Ledf6
    Led89:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ledf6:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Led79
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Led63
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
// MATCH: jgld.dll 0x1003f2e0 ?draw16_1003f2e0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1003f2e0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lf5fb:
        mov edx, dword ptr g_10128564
    Lf601:
        cmp byte ptr [esi], 0xff
        jae Lf680
        or byte ptr [ebp + 0x14], 0
        je Lf611
        cmp byte ptr [esi], 0xf8
        jae Lf680
    Lf611:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf680:
        inc esi
        add edi, 2
        dec edx
        jne Lf601
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf5fb
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
        mov ecx, dword ptr g_1012856c
    Lf747:
        mov edx, dword ptr g_10128564
    Lf74d:
        cmp byte ptr [esi], 0xff
        jae Lf7cc
        or byte ptr [ebp + 0x14], 0
        je Lf75d
        cmp byte ptr [esi], 0xf8
        jae Lf7cc
    Lf75d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf7cc:
        inc esi
        add edi, 2
        dec edx
        jne Lf74d
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf747
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
        mov ecx, dword ptr g_1012856c
    Lf8a0:
        mov edx, dword ptr g_10128564
    Lf8a6:
        cmp byte ptr [esi], 0xff
        jae Lf925
        or byte ptr [ebp + 0x14], 0
        je Lf8b6
        cmp byte ptr [esi], 0xf8
        jae Lf925
    Lf8b6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf925:
        dec esi
        add edi, 2
        dec edx
        jne Lf8a6
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf8a0
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
        mov ecx, dword ptr g_1012856c
    Lf9ec:
        mov edx, dword ptr g_10128564
    Lf9f2:
        cmp byte ptr [esi], 0xff
        jae Lfa71
        or byte ptr [ebp + 0x14], 0
        je Lfa02
        cmp byte ptr [esi], 0xf8
        jae Lfa71
    Lfa02:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfa71:
        dec esi
        add edi, 2
        dec edx
        jne Lf9f2
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf9ec
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lfd63:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lfd79:
        cmp byte ptr [esi], 0xff
        jae Lfdf8
        or byte ptr [ebp + 0x14], 0
        je Lfd89
        cmp byte ptr [esi], 0xf8
        jae Lfdf8
    Lfd89:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfdf8:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lfd79
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lfd63
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
        mov ecx, dword ptr g_1012856c
    Lff6e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lff84:
        cmp byte ptr [esi], 0xff
        jae L0003
        or byte ptr [ebp + 0x14], 0
        je Lff94
        cmp byte ptr [esi], 0xf8
        jae L0003
    Lff94:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0003:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lff84
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lff6e
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
        mov ecx, dword ptr g_1012856c
    L0186:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L019c:
        cmp byte ptr [esi], 0xff
        jae L021b
        or byte ptr [ebp + 0x14], 0
        je L01ac
        cmp byte ptr [esi], 0xf8
        jae L021b
    L01ac:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L021b:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L019c
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0186
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
        mov ecx, dword ptr g_1012856c
    L03b1:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L03c7:
        cmp byte ptr [esi], 0xff
        jae L0446
        or byte ptr [ebp + 0x14], 0
        je L03d7
        cmp byte ptr [esi], 0xf8
        jae L0446
    L03d7:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0446:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L03c7
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L03b1
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
// MATCH: jgld.dll 0x10040930 ?draw16_10040930@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10040930(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L0c4b:
        mov edx, dword ptr g_10128564
    L0c51:
        cmp byte ptr [esi], 0xff
        jae L0cd0
        or byte ptr [ebp + 0x14], 0
        je L0c61
        cmp byte ptr [esi], 0xf8
        jae L0cd0
    L0c61:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0cd0:
        inc esi
        add edi, 2
        dec edx
        jne L0c51
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0c4b
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
        mov ecx, dword ptr g_1012856c
    L0d97:
        mov edx, dword ptr g_10128564
    L0d9d:
        cmp byte ptr [esi], 0xff
        jae L0e1c
        or byte ptr [ebp + 0x14], 0
        je L0dad
        cmp byte ptr [esi], 0xf8
        jae L0e1c
    L0dad:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0e1c:
        inc esi
        add edi, 2
        dec edx
        jne L0d9d
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0d97
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
        mov ecx, dword ptr g_1012856c
    L0ef0:
        mov edx, dword ptr g_10128564
    L0ef6:
        cmp byte ptr [esi], 0xff
        jae L0f75
        or byte ptr [ebp + 0x14], 0
        je L0f06
        cmp byte ptr [esi], 0xf8
        jae L0f75
    L0f06:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0f75:
        dec esi
        add edi, 2
        dec edx
        jne L0ef6
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0ef0
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
        mov ecx, dword ptr g_1012856c
    L103c:
        mov edx, dword ptr g_10128564
    L1042:
        cmp byte ptr [esi], 0xff
        jae L10c1
        or byte ptr [ebp + 0x14], 0
        je L1052
        cmp byte ptr [esi], 0xf8
        jae L10c1
    L1052:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L10c1:
        dec esi
        add edi, 2
        dec edx
        jne L1042
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L103c
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L13b3:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L13c9:
        cmp byte ptr [esi], 0xff
        jae L1448
        or byte ptr [ebp + 0x14], 0
        je L13d9
        cmp byte ptr [esi], 0xf8
        jae L1448
    L13d9:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1448:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L13c9
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L13b3
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
        mov ecx, dword ptr g_1012856c
    L15be:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L15d4:
        cmp byte ptr [esi], 0xff
        jae L1653
        or byte ptr [ebp + 0x14], 0
        je L15e4
        cmp byte ptr [esi], 0xf8
        jae L1653
    L15e4:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1653:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L15d4
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L15be
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
        mov ecx, dword ptr g_1012856c
    L17d6:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L17ec:
        cmp byte ptr [esi], 0xff
        jae L186b
        or byte ptr [ebp + 0x14], 0
        je L17fc
        cmp byte ptr [esi], 0xf8
        jae L186b
    L17fc:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L186b:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L17ec
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L17d6
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
        mov ecx, dword ptr g_1012856c
    L1a01:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L1a17:
        cmp byte ptr [esi], 0xff
        jae L1a96
        or byte ptr [ebp + 0x14], 0
        je L1a27
        cmp byte ptr [esi], 0xf8
        jae L1a96
    L1a27:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0x7c1f0
        and ebx, 0x3e00
        and ecx, 0x7c1f0
        and edx, 0x3e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0x7c1f0
        and ebx, 0x3e00
        add eax, ebx
        shr eax, 4
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1a96:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L1a17
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L1a01
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
// MATCH: jgld.dll 0x10041f80 ?draw16_10041f80@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10041f80(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L229b:
        mov edx, dword ptr g_10128564
    L22a1:
        cmp byte ptr [esi], 0xff
        jae L22fd
        or byte ptr [ebp + 0x14], 0
        je L22b1
        cmp byte ptr [esi], 0xf8
        jae L22fd
    L22b1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L22fd:
        inc esi
        add edi, 2
        dec edx
        jne L22a1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L229b
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
        mov ecx, dword ptr g_1012856c
    L23bc:
        mov edx, dword ptr g_10128564
    L23c2:
        cmp byte ptr [esi], 0xff
        jae L241e
        or byte ptr [ebp + 0x14], 0
        je L23d2
        cmp byte ptr [esi], 0xf8
        jae L241e
    L23d2:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L241e:
        inc esi
        add edi, 2
        dec edx
        jne L23c2
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L23bc
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
        mov ecx, dword ptr g_1012856c
    L24ea:
        mov edx, dword ptr g_10128564
    L24f0:
        cmp byte ptr [esi], 0xff
        jae L254c
        or byte ptr [ebp + 0x14], 0
        je L2500
        cmp byte ptr [esi], 0xf8
        jae L254c
    L2500:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L254c:
        dec esi
        add edi, 2
        dec edx
        jne L24f0
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L24ea
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
        mov ecx, dword ptr g_1012856c
    L260b:
        mov edx, dword ptr g_10128564
    L2611:
        cmp byte ptr [esi], 0xff
        jae L266d
        or byte ptr [ebp + 0x14], 0
        je L2621
        cmp byte ptr [esi], 0xf8
        jae L266d
    L2621:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L266d:
        dec esi
        add edi, 2
        dec edx
        jne L2611
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L260b
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L2957:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L296d:
        cmp byte ptr [esi], 0xff
        jae L29c9
        or byte ptr [ebp + 0x14], 0
        je L297d
        cmp byte ptr [esi], 0xf8
        jae L29c9
    L297d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L29c9:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L296d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2957
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
        mov ecx, dword ptr g_1012856c
    L2b3b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2b51:
        cmp byte ptr [esi], 0xff
        jae L2bad
        or byte ptr [ebp + 0x14], 0
        je L2b61
        cmp byte ptr [esi], 0xf8
        jae L2bad
    L2b61:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2bad:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2b51
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2b3b
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
        mov ecx, dword ptr g_1012856c
    L2d2c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2d42:
        cmp byte ptr [esi], 0xff
        jae L2d9e
        or byte ptr [ebp + 0x14], 0
        je L2d52
        cmp byte ptr [esi], 0xf8
        jae L2d9e
    L2d52:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2d9e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2d42
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2d2c
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
        mov ecx, dword ptr g_1012856c
    L2f30:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2f46:
        cmp byte ptr [esi], 0xff
        jae L2fa2
        or byte ptr [ebp + 0x14], 0
        je L2f56
        cmp byte ptr [esi], 0xf8
        jae L2fa2
    L2f56:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2fa2:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2f46
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2f30
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
// MATCH: jgld.dll 0x10043430 ?draw16_10043430@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10043430(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L374b:
        mov edx, dword ptr g_10128564
    L3751:
        cmp byte ptr [esi], 0xff
        jae L37b5
        or byte ptr [ebp + 0x14], 0
        je L3761
        cmp byte ptr [esi], 0xf8
        jae L37b5
    L3761:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L37b5:
        inc esi
        add edi, 2
        dec edx
        jne L3751
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L374b
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
        mov ecx, dword ptr g_1012856c
    L3878:
        mov edx, dword ptr g_10128564
    L387e:
        cmp byte ptr [esi], 0xff
        jae L38e2
        or byte ptr [ebp + 0x14], 0
        je L388e
        cmp byte ptr [esi], 0xf8
        jae L38e2
    L388e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L38e2:
        inc esi
        add edi, 2
        dec edx
        jne L387e
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3878
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
        mov ecx, dword ptr g_1012856c
    L39b2:
        mov edx, dword ptr g_10128564
    L39b8:
        cmp byte ptr [esi], 0xff
        jae L3a1c
        or byte ptr [ebp + 0x14], 0
        je L39c8
        cmp byte ptr [esi], 0xf8
        jae L3a1c
    L39c8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3a1c:
        dec esi
        add edi, 2
        dec edx
        jne L39b8
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L39b2
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
        mov ecx, dword ptr g_1012856c
    L3adf:
        mov edx, dword ptr g_10128564
    L3ae5:
        cmp byte ptr [esi], 0xff
        jae L3b49
        or byte ptr [ebp + 0x14], 0
        je L3af5
        cmp byte ptr [esi], 0xf8
        jae L3b49
    L3af5:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3b49:
        dec esi
        add edi, 2
        dec edx
        jne L3ae5
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3adf
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L3e37:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L3e4d:
        cmp byte ptr [esi], 0xff
        jae L3eb1
        or byte ptr [ebp + 0x14], 0
        je L3e5d
        cmp byte ptr [esi], 0xf8
        jae L3eb1
    L3e5d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3eb1:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L3e4d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3e37
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
        mov ecx, dword ptr g_1012856c
    L4023:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4039:
        cmp byte ptr [esi], 0xff
        jae L409d
        or byte ptr [ebp + 0x14], 0
        je L4049
        cmp byte ptr [esi], 0xf8
        jae L409d
    L4049:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L409d:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4039
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4023
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
        mov ecx, dword ptr g_1012856c
    L421c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4232:
        cmp byte ptr [esi], 0xff
        jae L4296
        or byte ptr [ebp + 0x14], 0
        je L4242
        cmp byte ptr [esi], 0xf8
        jae L4296
    L4242:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4296:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4232
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L421c
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
        mov ecx, dword ptr g_1012856c
    L4428:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L443e:
        cmp byte ptr [esi], 0xff
        jae L44a2
        or byte ptr [ebp + 0x14], 0
        je L444e
        cmp byte ptr [esi], 0xf8
        jae L44a2
    L444e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L44a2:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L443e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4428
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
// MATCH: jgld.dll 0x10044940 ?draw16_10044940@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10044940(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L4c5b:
        mov edx, dword ptr g_10128564
    L4c61:
        cmp byte ptr [esi], 0xff
        jae L4cc5
        or byte ptr [ebp + 0x14], 0
        je L4c71
        cmp byte ptr [esi], 0xf8
        jae L4cc5
    L4c71:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4cc5:
        inc esi
        add edi, 2
        dec edx
        jne L4c61
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4c5b
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
        mov ecx, dword ptr g_1012856c
    L4d88:
        mov edx, dword ptr g_10128564
    L4d8e:
        cmp byte ptr [esi], 0xff
        jae L4df2
        or byte ptr [ebp + 0x14], 0
        je L4d9e
        cmp byte ptr [esi], 0xf8
        jae L4df2
    L4d9e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4df2:
        inc esi
        add edi, 2
        dec edx
        jne L4d8e
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4d88
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
        mov ecx, dword ptr g_1012856c
    L4ec2:
        mov edx, dword ptr g_10128564
    L4ec8:
        cmp byte ptr [esi], 0xff
        jae L4f2c
        or byte ptr [ebp + 0x14], 0
        je L4ed8
        cmp byte ptr [esi], 0xf8
        jae L4f2c
    L4ed8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4f2c:
        dec esi
        add edi, 2
        dec edx
        jne L4ec8
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4ec2
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
        mov ecx, dword ptr g_1012856c
    L4fef:
        mov edx, dword ptr g_10128564
    L4ff5:
        cmp byte ptr [esi], 0xff
        jae L5059
        or byte ptr [ebp + 0x14], 0
        je L5005
        cmp byte ptr [esi], 0xf8
        jae L5059
    L5005:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5059:
        dec esi
        add edi, 2
        dec edx
        jne L4ff5
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L4fef
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L5347:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L535d:
        cmp byte ptr [esi], 0xff
        jae L53c1
        or byte ptr [ebp + 0x14], 0
        je L536d
        cmp byte ptr [esi], 0xf8
        jae L53c1
    L536d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L53c1:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L535d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5347
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
        mov ecx, dword ptr g_1012856c
    L5533:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5549:
        cmp byte ptr [esi], 0xff
        jae L55ad
        or byte ptr [ebp + 0x14], 0
        je L5559
        cmp byte ptr [esi], 0xf8
        jae L55ad
    L5559:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L55ad:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5549
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5533
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
        mov ecx, dword ptr g_1012856c
    L572c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5742:
        cmp byte ptr [esi], 0xff
        jae L57a6
        or byte ptr [ebp + 0x14], 0
        je L5752
        cmp byte ptr [esi], 0xf8
        jae L57a6
    L5752:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L57a6:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5742
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L572c
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
        mov ecx, dword ptr g_1012856c
    L5938:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L594e:
        cmp byte ptr [esi], 0xff
        jae L59b2
        or byte ptr [ebp + 0x14], 0
        je L595e
        cmp byte ptr [esi], 0xf8
        jae L59b2
    L595e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L59b2:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L594e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5938
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
// MATCH: jgld.dll 0x10045e50 ?draw16_10045e50@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10045e50(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L616b:
        mov edx, dword ptr g_10128564
    L6171:
        cmp byte ptr [esi], 0xff
        jae L61dd
        or byte ptr [ebp + 0x14], 0
        je L6181
        cmp byte ptr [esi], 0xf8
        jae L61dd
    L6181:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L61dd:
        inc esi
        add edi, 2
        dec edx
        jne L6171
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L616b
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
        mov ecx, dword ptr g_1012856c
    L62a0:
        mov edx, dword ptr g_10128564
    L62a6:
        cmp byte ptr [esi], 0xff
        jae L6312
        or byte ptr [ebp + 0x14], 0
        je L62b6
        cmp byte ptr [esi], 0xf8
        jae L6312
    L62b6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6312:
        inc esi
        add edi, 2
        dec edx
        jne L62a6
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L62a0
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
        mov ecx, dword ptr g_1012856c
    L63e2:
        mov edx, dword ptr g_10128564
    L63e8:
        cmp byte ptr [esi], 0xff
        jae L6454
        or byte ptr [ebp + 0x14], 0
        je L63f8
        cmp byte ptr [esi], 0xf8
        jae L6454
    L63f8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6454:
        dec esi
        add edi, 2
        dec edx
        jne L63e8
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L63e2
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
        mov ecx, dword ptr g_1012856c
    L6517:
        mov edx, dword ptr g_10128564
    L651d:
        cmp byte ptr [esi], 0xff
        jae L6589
        or byte ptr [ebp + 0x14], 0
        je L652d
        cmp byte ptr [esi], 0xf8
        jae L6589
    L652d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6589:
        dec esi
        add edi, 2
        dec edx
        jne L651d
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L6517
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L6877:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L688d:
        cmp byte ptr [esi], 0xff
        jae L68f9
        or byte ptr [ebp + 0x14], 0
        je L689d
        cmp byte ptr [esi], 0xf8
        jae L68f9
    L689d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L68f9:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L688d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6877
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
        mov ecx, dword ptr g_1012856c
    L6a6b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6a81:
        cmp byte ptr [esi], 0xff
        jae L6aed
        or byte ptr [ebp + 0x14], 0
        je L6a91
        cmp byte ptr [esi], 0xf8
        jae L6aed
    L6a91:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6aed:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6a81
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6a6b
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
        mov ecx, dword ptr g_1012856c
    L6c6c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6c82:
        cmp byte ptr [esi], 0xff
        jae L6cee
        or byte ptr [ebp + 0x14], 0
        je L6c92
        cmp byte ptr [esi], 0xf8
        jae L6cee
    L6c92:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6cee:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6c82
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6c6c
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
        mov ecx, dword ptr g_1012856c
    L6e80:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6e96:
        cmp byte ptr [esi], 0xff
        jae L6f02
        or byte ptr [ebp + 0x14], 0
        je L6ea6
        cmp byte ptr [esi], 0xf8
        jae L6f02
    L6ea6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L6f02:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6e96
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6e80
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
// MATCH: jgld.dll 0x100473b0 ?draw16_100473b0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_100473b0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L76cb:
        mov edx, dword ptr g_10128564
    L76d1:
        cmp byte ptr [esi], 0xff
        jae L773d
        or byte ptr [ebp + 0x14], 0
        je L76e1
        cmp byte ptr [esi], 0xf8
        jae L773d
    L76e1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L773d:
        inc esi
        add edi, 2
        dec edx
        jne L76d1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L76cb
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
        mov ecx, dword ptr g_1012856c
    L7800:
        mov edx, dword ptr g_10128564
    L7806:
        cmp byte ptr [esi], 0xff
        jae L7872
        or byte ptr [ebp + 0x14], 0
        je L7816
        cmp byte ptr [esi], 0xf8
        jae L7872
    L7816:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L7872:
        inc esi
        add edi, 2
        dec edx
        jne L7806
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7800
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
        mov ecx, dword ptr g_1012856c
    L7942:
        mov edx, dword ptr g_10128564
    L7948:
        cmp byte ptr [esi], 0xff
        jae L79b4
        or byte ptr [ebp + 0x14], 0
        je L7958
        cmp byte ptr [esi], 0xf8
        jae L79b4
    L7958:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L79b4:
        dec esi
        add edi, 2
        dec edx
        jne L7948
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7942
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
        mov ecx, dword ptr g_1012856c
    L7a77:
        mov edx, dword ptr g_10128564
    L7a7d:
        cmp byte ptr [esi], 0xff
        jae L7ae9
        or byte ptr [ebp + 0x14], 0
        je L7a8d
        cmp byte ptr [esi], 0xf8
        jae L7ae9
    L7a8d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L7ae9:
        dec esi
        add edi, 2
        dec edx
        jne L7a7d
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7a77
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L7dd7:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7ded:
        cmp byte ptr [esi], 0xff
        jae L7e59
        or byte ptr [ebp + 0x14], 0
        je L7dfd
        cmp byte ptr [esi], 0xf8
        jae L7e59
    L7dfd:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L7e59:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7ded
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7dd7
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
        mov ecx, dword ptr g_1012856c
    L7fcb:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7fe1:
        cmp byte ptr [esi], 0xff
        jae L804d
        or byte ptr [ebp + 0x14], 0
        je L7ff1
        cmp byte ptr [esi], 0xf8
        jae L804d
    L7ff1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L804d:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7fe1
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7fcb
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
        mov ecx, dword ptr g_1012856c
    L81cc:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L81e2:
        cmp byte ptr [esi], 0xff
        jae L824e
        or byte ptr [ebp + 0x14], 0
        je L81f2
        cmp byte ptr [esi], 0xf8
        jae L824e
    L81f2:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L824e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L81e2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L81cc
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
        mov ecx, dword ptr g_1012856c
    L83e0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L83f6:
        cmp byte ptr [esi], 0xff
        jae L8462
        or byte ptr [ebp + 0x14], 0
        je L8406
        cmp byte ptr [esi], 0xf8
        jae L8462
    L8406:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8462:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L83f6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L83e0
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
// MATCH: jgld.dll 0x10048910 ?draw16_10048910@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10048910(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L8c2b:
        mov edx, dword ptr g_10128564
    L8c31:
        cmp byte ptr [esi], 0xff
        jae L8c9f
        or byte ptr [ebp + 0x14], 0
        je L8c41
        cmp byte ptr [esi], 0xf8
        jae L8c9f
    L8c41:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8c9f:
        inc esi
        add edi, 2
        dec edx
        jne L8c31
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8c2b
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
        mov ecx, dword ptr g_1012856c
    L8d62:
        mov edx, dword ptr g_10128564
    L8d68:
        cmp byte ptr [esi], 0xff
        jae L8dd6
        or byte ptr [ebp + 0x14], 0
        je L8d78
        cmp byte ptr [esi], 0xf8
        jae L8dd6
    L8d78:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8dd6:
        inc esi
        add edi, 2
        dec edx
        jne L8d68
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8d62
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
        mov ecx, dword ptr g_1012856c
    L8ea6:
        mov edx, dword ptr g_10128564
    L8eac:
        cmp byte ptr [esi], 0xff
        jae L8f1a
        or byte ptr [ebp + 0x14], 0
        je L8ebc
        cmp byte ptr [esi], 0xf8
        jae L8f1a
    L8ebc:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L8f1a:
        dec esi
        add edi, 2
        dec edx
        jne L8eac
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8ea6
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
        mov ecx, dword ptr g_1012856c
    L8fdd:
        mov edx, dword ptr g_10128564
    L8fe3:
        cmp byte ptr [esi], 0xff
        jae L9051
        or byte ptr [ebp + 0x14], 0
        je L8ff3
        cmp byte ptr [esi], 0xf8
        jae L9051
    L8ff3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L9051:
        dec esi
        add edi, 2
        dec edx
        jne L8fe3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8fdd
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L933f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L9355:
        cmp byte ptr [esi], 0xff
        jae L93c3
        or byte ptr [ebp + 0x14], 0
        je L9365
        cmp byte ptr [esi], 0xf8
        jae L93c3
    L9365:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L93c3:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L9355
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L933f
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
        mov ecx, dword ptr g_1012856c
    L9539:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L954f:
        cmp byte ptr [esi], 0xff
        jae L95bd
        or byte ptr [ebp + 0x14], 0
        je L955f
        cmp byte ptr [esi], 0xf8
        jae L95bd
    L955f:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L95bd:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L954f
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L9539
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
        mov ecx, dword ptr g_1012856c
    L9740:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L9756:
        cmp byte ptr [esi], 0xff
        jae L97c4
        or byte ptr [ebp + 0x14], 0
        je L9766
        cmp byte ptr [esi], 0xf8
        jae L97c4
    L9766:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L97c4:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L9756
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L9740
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
        mov ecx, dword ptr g_1012856c
    L995a:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L9970:
        cmp byte ptr [esi], 0xff
        jae L99de
        or byte ptr [ebp + 0x14], 0
        je L9980
        cmp byte ptr [esi], 0xf8
        jae L99de
    L9980:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L99de:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L9970
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L995a
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
// MATCH: jgld.dll 0x10049ea0 ?draw16_10049ea0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10049ea0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    La1bb:
        mov edx, dword ptr g_10128564
    La1c1:
        cmp byte ptr [esi], 0xff
        jae La22f
        or byte ptr [ebp + 0x14], 0
        je La1d1
        cmp byte ptr [esi], 0xf8
        jae La22f
    La1d1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La22f:
        inc esi
        add edi, 2
        dec edx
        jne La1c1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne La1bb
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
        mov ecx, dword ptr g_1012856c
    La2f2:
        mov edx, dword ptr g_10128564
    La2f8:
        cmp byte ptr [esi], 0xff
        jae La366
        or byte ptr [ebp + 0x14], 0
        je La308
        cmp byte ptr [esi], 0xf8
        jae La366
    La308:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La366:
        inc esi
        add edi, 2
        dec edx
        jne La2f8
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne La2f2
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
        mov ecx, dword ptr g_1012856c
    La436:
        mov edx, dword ptr g_10128564
    La43c:
        cmp byte ptr [esi], 0xff
        jae La4aa
        or byte ptr [ebp + 0x14], 0
        je La44c
        cmp byte ptr [esi], 0xf8
        jae La4aa
    La44c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La4aa:
        dec esi
        add edi, 2
        dec edx
        jne La43c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne La436
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
        mov ecx, dword ptr g_1012856c
    La56d:
        mov edx, dword ptr g_10128564
    La573:
        cmp byte ptr [esi], 0xff
        jae La5e1
        or byte ptr [ebp + 0x14], 0
        je La583
        cmp byte ptr [esi], 0xf8
        jae La5e1
    La583:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La5e1:
        dec esi
        add edi, 2
        dec edx
        jne La573
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne La56d
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    La8cf:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La8e5:
        cmp byte ptr [esi], 0xff
        jae La953
        or byte ptr [ebp + 0x14], 0
        je La8f5
        cmp byte ptr [esi], 0xf8
        jae La953
    La8f5:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    La953:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La8e5
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La8cf
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
        mov ecx, dword ptr g_1012856c
    Laac9:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Laadf:
        cmp byte ptr [esi], 0xff
        jae Lab4d
        or byte ptr [ebp + 0x14], 0
        je Laaef
        cmp byte ptr [esi], 0xf8
        jae Lab4d
    Laaef:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lab4d:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Laadf
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Laac9
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
        mov ecx, dword ptr g_1012856c
    Lacd0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lace6:
        cmp byte ptr [esi], 0xff
        jae Lad54
        or byte ptr [ebp + 0x14], 0
        je Lacf6
        cmp byte ptr [esi], 0xf8
        jae Lad54
    Lacf6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lad54:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lace6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lacd0
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
        mov ecx, dword ptr g_1012856c
    Laeea:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Laf00:
        cmp byte ptr [esi], 0xff
        jae Laf6e
        or byte ptr [ebp + 0x14], 0
        je Laf10
        cmp byte ptr [esi], 0xf8
        jae Laf6e
    Laf10:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 4
        sub eax, esi
        mov esi, ebx
        shl ebx, 4
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Laf6e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Laf00
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Laeea
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
// MATCH: jgld.dll 0x1004b430 ?draw16_1004b430@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1004b430(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lb74b:
        mov edx, dword ptr g_10128564
    Lb751:
        cmp byte ptr [esi], 0xff
        jae Lb7c7
        or byte ptr [ebp + 0x14], 0
        je Lb761
        cmp byte ptr [esi], 0xf8
        jae Lb7c7
    Lb761:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lb7c7:
        inc esi
        add edi, 2
        dec edx
        jne Lb751
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb74b
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
        mov ecx, dword ptr g_1012856c
    Lb88a:
        mov edx, dword ptr g_10128564
    Lb890:
        cmp byte ptr [esi], 0xff
        jae Lb906
        or byte ptr [ebp + 0x14], 0
        je Lb8a0
        cmp byte ptr [esi], 0xf8
        jae Lb906
    Lb8a0:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lb906:
        inc esi
        add edi, 2
        dec edx
        jne Lb890
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb88a
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
        mov ecx, dword ptr g_1012856c
    Lb9d6:
        mov edx, dword ptr g_10128564
    Lb9dc:
        cmp byte ptr [esi], 0xff
        jae Lba52
        or byte ptr [ebp + 0x14], 0
        je Lb9ec
        cmp byte ptr [esi], 0xf8
        jae Lba52
    Lb9ec:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lba52:
        dec esi
        add edi, 2
        dec edx
        jne Lb9dc
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lb9d6
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
        mov ecx, dword ptr g_1012856c
    Lbb15:
        mov edx, dword ptr g_10128564
    Lbb1b:
        cmp byte ptr [esi], 0xff
        jae Lbb91
        or byte ptr [ebp + 0x14], 0
        je Lbb2b
        cmp byte ptr [esi], 0xf8
        jae Lbb91
    Lbb2b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lbb91:
        dec esi
        add edi, 2
        dec edx
        jne Lbb1b
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lbb15
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lbe7f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lbe95:
        cmp byte ptr [esi], 0xff
        jae Lbf0b
        or byte ptr [ebp + 0x14], 0
        je Lbea5
        cmp byte ptr [esi], 0xf8
        jae Lbf0b
    Lbea5:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lbf0b:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lbe95
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lbe7f
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
        mov ecx, dword ptr g_1012856c
    Lc081:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc097:
        cmp byte ptr [esi], 0xff
        jae Lc10d
        or byte ptr [ebp + 0x14], 0
        je Lc0a7
        cmp byte ptr [esi], 0xf8
        jae Lc10d
    Lc0a7:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lc10d:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc097
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc081
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
        mov ecx, dword ptr g_1012856c
    Lc290:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc2a6:
        cmp byte ptr [esi], 0xff
        jae Lc31c
        or byte ptr [ebp + 0x14], 0
        je Lc2b6
        cmp byte ptr [esi], 0xf8
        jae Lc31c
    Lc2b6:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lc31c:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc2a6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc290
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
        mov ecx, dword ptr g_1012856c
    Lc4b2:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lc4c8:
        cmp byte ptr [esi], 0xff
        jae Lc53e
        or byte ptr [ebp + 0x14], 0
        je Lc4d8
        cmp byte ptr [esi], 0xf8
        jae Lc53e
    Lc4d8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lc53e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lc4c8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lc4b2
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
// MATCH: jgld.dll 0x1004ca10 ?draw16_1004ca10@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1004ca10(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lcd2b:
        mov edx, dword ptr g_10128564
    Lcd31:
        cmp byte ptr [esi], 0xff
        jae Lcda7
        or byte ptr [ebp + 0x14], 0
        je Lcd41
        cmp byte ptr [esi], 0xf8
        jae Lcda7
    Lcd41:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lcda7:
        inc esi
        add edi, 2
        dec edx
        jne Lcd31
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lcd2b
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
        mov ecx, dword ptr g_1012856c
    Lce6a:
        mov edx, dword ptr g_10128564
    Lce70:
        cmp byte ptr [esi], 0xff
        jae Lcee6
        or byte ptr [ebp + 0x14], 0
        je Lce80
        cmp byte ptr [esi], 0xf8
        jae Lcee6
    Lce80:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lcee6:
        inc esi
        add edi, 2
        dec edx
        jne Lce70
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lce6a
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
        mov ecx, dword ptr g_1012856c
    Lcfb6:
        mov edx, dword ptr g_10128564
    Lcfbc:
        cmp byte ptr [esi], 0xff
        jae Ld032
        or byte ptr [ebp + 0x14], 0
        je Lcfcc
        cmp byte ptr [esi], 0xf8
        jae Ld032
    Lcfcc:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld032:
        dec esi
        add edi, 2
        dec edx
        jne Lcfbc
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lcfb6
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
        mov ecx, dword ptr g_1012856c
    Ld0f5:
        mov edx, dword ptr g_10128564
    Ld0fb:
        cmp byte ptr [esi], 0xff
        jae Ld171
        or byte ptr [ebp + 0x14], 0
        je Ld10b
        cmp byte ptr [esi], 0xf8
        jae Ld171
    Ld10b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld171:
        dec esi
        add edi, 2
        dec edx
        jne Ld0fb
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Ld0f5
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Ld45f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld475:
        cmp byte ptr [esi], 0xff
        jae Ld4eb
        or byte ptr [ebp + 0x14], 0
        je Ld485
        cmp byte ptr [esi], 0xf8
        jae Ld4eb
    Ld485:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld4eb:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld475
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld45f
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
        mov ecx, dword ptr g_1012856c
    Ld661:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld677:
        cmp byte ptr [esi], 0xff
        jae Ld6ed
        or byte ptr [ebp + 0x14], 0
        je Ld687
        cmp byte ptr [esi], 0xf8
        jae Ld6ed
    Ld687:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld6ed:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld677
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld661
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
        mov ecx, dword ptr g_1012856c
    Ld870:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ld886:
        cmp byte ptr [esi], 0xff
        jae Ld8fc
        or byte ptr [ebp + 0x14], 0
        je Ld896
        cmp byte ptr [esi], 0xf8
        jae Ld8fc
    Ld896:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ld8fc:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ld886
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Ld870
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
        mov ecx, dword ptr g_1012856c
    Lda92:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Ldaa8:
        cmp byte ptr [esi], 0xff
        jae Ldb1e
        or byte ptr [ebp + 0x14], 0
        je Ldab8
        cmp byte ptr [esi], 0xf8
        jae Ldb1e
    Ldab8:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 2
        shr ebx, 2
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Ldb1e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Ldaa8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lda92
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
// MATCH: jgld.dll 0x1004dff0 ?draw16_1004dff0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1004dff0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Le30b:
        mov edx, dword ptr g_10128564
    Le311:
        cmp byte ptr [esi], 0xff
        jae Le387
        or byte ptr [ebp + 0x14], 0
        je Le321
        cmp byte ptr [esi], 0xf8
        jae Le387
    Le321:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le387:
        inc esi
        add edi, 2
        dec edx
        jne Le311
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le30b
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
        mov ecx, dword ptr g_1012856c
    Le44a:
        mov edx, dword ptr g_10128564
    Le450:
        cmp byte ptr [esi], 0xff
        jae Le4c6
        or byte ptr [ebp + 0x14], 0
        je Le460
        cmp byte ptr [esi], 0xf8
        jae Le4c6
    Le460:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le4c6:
        inc esi
        add edi, 2
        dec edx
        jne Le450
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le44a
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
        mov ecx, dword ptr g_1012856c
    Le596:
        mov edx, dword ptr g_10128564
    Le59c:
        cmp byte ptr [esi], 0xff
        jae Le612
        or byte ptr [ebp + 0x14], 0
        je Le5ac
        cmp byte ptr [esi], 0xf8
        jae Le612
    Le5ac:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le612:
        dec esi
        add edi, 2
        dec edx
        jne Le59c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le596
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
        mov ecx, dword ptr g_1012856c
    Le6d5:
        mov edx, dword ptr g_10128564
    Le6db:
        cmp byte ptr [esi], 0xff
        jae Le751
        or byte ptr [ebp + 0x14], 0
        je Le6eb
        cmp byte ptr [esi], 0xf8
        jae Le751
    Le6eb:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Le751:
        dec esi
        add edi, 2
        dec edx
        jne Le6db
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Le6d5
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lea3f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lea55:
        cmp byte ptr [esi], 0xff
        jae Leacb
        or byte ptr [ebp + 0x14], 0
        je Lea65
        cmp byte ptr [esi], 0xf8
        jae Leacb
    Lea65:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Leacb:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lea55
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lea3f
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
        mov ecx, dword ptr g_1012856c
    Lec41:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lec57:
        cmp byte ptr [esi], 0xff
        jae Leccd
        or byte ptr [ebp + 0x14], 0
        je Lec67
        cmp byte ptr [esi], 0xf8
        jae Leccd
    Lec67:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Leccd:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lec57
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lec41
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
        mov ecx, dword ptr g_1012856c
    Lee50:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lee66:
        cmp byte ptr [esi], 0xff
        jae Leedc
        or byte ptr [ebp + 0x14], 0
        je Lee76
        cmp byte ptr [esi], 0xf8
        jae Leedc
    Lee76:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Leedc:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lee66
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lee50
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
        mov ecx, dword ptr g_1012856c
    Lf072:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    Lf088:
        cmp byte ptr [esi], 0xff
        jae Lf0fe
        or byte ptr [ebp + 0x14], 0
        je Lf098
        cmp byte ptr [esi], 0xf8
        jae Lf0fe
    Lf098:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf0fe:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns Lf088
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne Lf072
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
// MATCH: jgld.dll 0x1004f5d0 ?draw16_1004f5d0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_1004f5d0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    Lf8eb:
        mov edx, dword ptr g_10128564
    Lf8f1:
        cmp byte ptr [esi], 0xff
        jae Lf967
        or byte ptr [ebp + 0x14], 0
        je Lf901
        cmp byte ptr [esi], 0xf8
        jae Lf967
    Lf901:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lf967:
        inc esi
        add edi, 2
        dec edx
        jne Lf8f1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lf8eb
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
        mov ecx, dword ptr g_1012856c
    Lfa2a:
        mov edx, dword ptr g_10128564
    Lfa30:
        cmp byte ptr [esi], 0xff
        jae Lfaa6
        or byte ptr [ebp + 0x14], 0
        je Lfa40
        cmp byte ptr [esi], 0xf8
        jae Lfaa6
    Lfa40:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfaa6:
        inc esi
        add edi, 2
        dec edx
        jne Lfa30
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lfa2a
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
        mov ecx, dword ptr g_1012856c
    Lfb76:
        mov edx, dword ptr g_10128564
    Lfb7c:
        cmp byte ptr [esi], 0xff
        jae Lfbf2
        or byte ptr [ebp + 0x14], 0
        je Lfb8c
        cmp byte ptr [esi], 0xf8
        jae Lfbf2
    Lfb8c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfbf2:
        dec esi
        add edi, 2
        dec edx
        jne Lfb7c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lfb76
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
        mov ecx, dword ptr g_1012856c
    Lfcb5:
        mov edx, dword ptr g_10128564
    Lfcbb:
        cmp byte ptr [esi], 0xff
        jae Lfd31
        or byte ptr [ebp + 0x14], 0
        je Lfccb
        cmp byte ptr [esi], 0xf8
        jae Lfd31
    Lfccb:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    Lfd31:
        dec esi
        add edi, 2
        dec edx
        jne Lfcbb
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne Lfcb5
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L001f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0035:
        cmp byte ptr [esi], 0xff
        jae L00ab
        or byte ptr [ebp + 0x14], 0
        je L0045
        cmp byte ptr [esi], 0xf8
        jae L00ab
    L0045:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L00ab:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0035
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L001f
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
        mov ecx, dword ptr g_1012856c
    L0221:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0237:
        cmp byte ptr [esi], 0xff
        jae L02ad
        or byte ptr [ebp + 0x14], 0
        je L0247
        cmp byte ptr [esi], 0xf8
        jae L02ad
    L0247:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L02ad:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0237
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0221
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
        mov ecx, dword ptr g_1012856c
    L0430:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0446:
        cmp byte ptr [esi], 0xff
        jae L04bc
        or byte ptr [ebp + 0x14], 0
        je L0456
        cmp byte ptr [esi], 0xf8
        jae L04bc
    L0456:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L04bc:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0446
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0430
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
        mov ecx, dword ptr g_1012856c
    L0652:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L0668:
        cmp byte ptr [esi], 0xff
        jae L06de
        or byte ptr [ebp + 0x14], 0
        je L0678
        cmp byte ptr [esi], 0xf8
        jae L06de
    L0678:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 3
        shr ebx, 3
        pop esi
        add eax, ecx
        add ebx, edx
        shr eax, 1
        shr ebx, 1
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L06de:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L0668
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L0652
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
// MATCH: jgld.dll 0x10050bb0 ?draw16_10050bb0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10050bb0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L0ecb:
        mov edx, dword ptr g_10128564
    L0ed1:
        cmp byte ptr [esi], 0xff
        jae L0f4b
        or byte ptr [ebp + 0x14], 0
        je L0ee1
        cmp byte ptr [esi], 0xf8
        jae L0f4b
    L0ee1:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L0f4b:
        inc esi
        add edi, 2
        dec edx
        jne L0ed1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L0ecb
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
        mov ecx, dword ptr g_1012856c
    L1012:
        mov edx, dword ptr g_10128564
    L1018:
        cmp byte ptr [esi], 0xff
        jae L1092
        or byte ptr [ebp + 0x14], 0
        je L1028
        cmp byte ptr [esi], 0xf8
        jae L1092
    L1028:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1092:
        inc esi
        add edi, 2
        dec edx
        jne L1018
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1012
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
        mov ecx, dword ptr g_1012856c
    L1166:
        mov edx, dword ptr g_10128564
    L116c:
        cmp byte ptr [esi], 0xff
        jae L11e6
        or byte ptr [ebp + 0x14], 0
        je L117c
        cmp byte ptr [esi], 0xf8
        jae L11e6
    L117c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L11e6:
        dec esi
        add edi, 2
        dec edx
        jne L116c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L1166
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
        mov ecx, dword ptr g_1012856c
    L12ad:
        mov edx, dword ptr g_10128564
    L12b3:
        cmp byte ptr [esi], 0xff
        jae L132d
        or byte ptr [ebp + 0x14], 0
        je L12c3
        cmp byte ptr [esi], 0xf8
        jae L132d
    L12c3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L132d:
        dec esi
        add edi, 2
        dec edx
        jne L12b3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L12ad
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L161f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L1635:
        cmp byte ptr [esi], 0xff
        jae L16af
        or byte ptr [ebp + 0x14], 0
        je L1645
        cmp byte ptr [esi], 0xf8
        jae L16af
    L1645:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L16af:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L1635
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L161f
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
        mov ecx, dword ptr g_1012856c
    L1825:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L183b:
        cmp byte ptr [esi], 0xff
        jae L18b5
        or byte ptr [ebp + 0x14], 0
        je L184b
        cmp byte ptr [esi], 0xf8
        jae L18b5
    L184b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L18b5:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L183b
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L1825
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
        mov ecx, dword ptr g_1012856c
    L1a38:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L1a4e:
        cmp byte ptr [esi], 0xff
        jae L1ac8
        or byte ptr [ebp + 0x14], 0
        je L1a5e
        cmp byte ptr [esi], 0xf8
        jae L1ac8
    L1a5e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1ac8:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L1a4e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L1a38
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
        mov ecx, dword ptr g_1012856c
    L1c5e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L1c74:
        cmp byte ptr [esi], 0xff
        jae L1cee
        or byte ptr [ebp + 0x14], 0
        je L1c84
        cmp byte ptr [esi], 0xf8
        jae L1cee
    L1c84:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L1cee:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L1c74
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L1c5e
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
// MATCH: jgld.dll 0x100521d0 ?draw16_100521d0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_100521d0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L24eb:
        mov edx, dword ptr g_10128564
    L24f1:
        cmp byte ptr [esi], 0xff
        jae L256b
        or byte ptr [ebp + 0x14], 0
        je L2501
        cmp byte ptr [esi], 0xf8
        jae L256b
    L2501:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L256b:
        inc esi
        add edi, 2
        dec edx
        jne L24f1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L24eb
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
        mov ecx, dword ptr g_1012856c
    L2632:
        mov edx, dword ptr g_10128564
    L2638:
        cmp byte ptr [esi], 0xff
        jae L26b2
        or byte ptr [ebp + 0x14], 0
        je L2648
        cmp byte ptr [esi], 0xf8
        jae L26b2
    L2648:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L26b2:
        inc esi
        add edi, 2
        dec edx
        jne L2638
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L2632
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
        mov ecx, dword ptr g_1012856c
    L2786:
        mov edx, dword ptr g_10128564
    L278c:
        cmp byte ptr [esi], 0xff
        jae L2806
        or byte ptr [ebp + 0x14], 0
        je L279c
        cmp byte ptr [esi], 0xf8
        jae L2806
    L279c:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2806:
        dec esi
        add edi, 2
        dec edx
        jne L278c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L2786
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
        mov ecx, dword ptr g_1012856c
    L28cd:
        mov edx, dword ptr g_10128564
    L28d3:
        cmp byte ptr [esi], 0xff
        jae L294d
        or byte ptr [ebp + 0x14], 0
        je L28e3
        cmp byte ptr [esi], 0xf8
        jae L294d
    L28e3:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L294d:
        dec esi
        add edi, 2
        dec edx
        jne L28d3
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L28cd
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L2c3f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2c55:
        cmp byte ptr [esi], 0xff
        jae L2ccf
        or byte ptr [ebp + 0x14], 0
        je L2c65
        cmp byte ptr [esi], 0xf8
        jae L2ccf
    L2c65:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2ccf:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2c55
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2c3f
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
        mov ecx, dword ptr g_1012856c
    L2e45:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L2e5b:
        cmp byte ptr [esi], 0xff
        jae L2ed5
        or byte ptr [ebp + 0x14], 0
        je L2e6b
        cmp byte ptr [esi], 0xf8
        jae L2ed5
    L2e6b:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L2ed5:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L2e5b
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L2e45
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
        mov ecx, dword ptr g_1012856c
    L3058:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L306e:
        cmp byte ptr [esi], 0xff
        jae L30e8
        or byte ptr [ebp + 0x14], 0
        je L307e
        cmp byte ptr [esi], 0xf8
        jae L30e8
    L307e:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L30e8:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L306e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L3058
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
        mov ecx, dword ptr g_1012856c
    L327e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L3294:
        cmp byte ptr [esi], 0xff
        jae L330e
        or byte ptr [ebp + 0x14], 0
        je L32a4
        cmp byte ptr [esi], 0xf8
        jae L330e
    L32a4:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 3
        sub eax, esi
        add eax, ecx
        shl eax, 1
        sub eax, esi
        mov esi, ebx
        shl ebx, 3
        sub ebx, esi
        add ebx, edx
        shl ebx, 1
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L330e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L3294
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L327e
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
// MATCH: jgld.dll 0x100537f0 ?draw16_100537f0@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_100537f0(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L3b0b:
        mov edx, dword ptr g_10128564
    L3b11:
        cmp byte ptr [esi], 0xff
        jae L3b8d
        or byte ptr [ebp + 0x14], 0
        je L3b21
        cmp byte ptr [esi], 0xf8
        jae L3b8d
    L3b21:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3b8d:
        inc esi
        add edi, 2
        dec edx
        jne L3b11
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3b0b
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
        mov ecx, dword ptr g_1012856c
    L3c54:
        mov edx, dword ptr g_10128564
    L3c5a:
        cmp byte ptr [esi], 0xff
        jae L3cd6
        or byte ptr [ebp + 0x14], 0
        je L3c6a
        cmp byte ptr [esi], 0xf8
        jae L3cd6
    L3c6a:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3cd6:
        inc esi
        add edi, 2
        dec edx
        jne L3c5a
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3c54
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
        mov ecx, dword ptr g_1012856c
    L3daa:
        mov edx, dword ptr g_10128564
    L3db0:
        cmp byte ptr [esi], 0xff
        jae L3e2c
        or byte ptr [ebp + 0x14], 0
        je L3dc0
        cmp byte ptr [esi], 0xf8
        jae L3e2c
    L3dc0:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3e2c:
        dec esi
        add edi, 2
        dec edx
        jne L3db0
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3daa
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
        mov ecx, dword ptr g_1012856c
    L3ef3:
        mov edx, dword ptr g_10128564
    L3ef9:
        cmp byte ptr [esi], 0xff
        jae L3f75
        or byte ptr [ebp + 0x14], 0
        je L3f09
        cmp byte ptr [esi], 0xf8
        jae L3f75
    L3f09:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L3f75:
        dec esi
        add edi, 2
        dec edx
        jne L3ef9
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L3ef3
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L4267:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L427d:
        cmp byte ptr [esi], 0xff
        jae L42f9
        or byte ptr [ebp + 0x14], 0
        je L428d
        cmp byte ptr [esi], 0xf8
        jae L42f9
    L428d:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L42f9:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L427d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4267
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
        mov ecx, dword ptr g_1012856c
    L446f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L4485:
        cmp byte ptr [esi], 0xff
        jae L4501
        or byte ptr [ebp + 0x14], 0
        je L4495
        cmp byte ptr [esi], 0xf8
        jae L4501
    L4495:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4501:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L4485
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L446f
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
        mov ecx, dword ptr g_1012856c
    L4684:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L469a:
        cmp byte ptr [esi], 0xff
        jae L4716
        or byte ptr [ebp + 0x14], 0
        je L46aa
        cmp byte ptr [esi], 0xf8
        jae L4716
    L46aa:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L4716:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L469a
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L4684
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
        mov ecx, dword ptr g_1012856c
    L48ac:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L48c2:
        cmp byte ptr [esi], 0xff
        jae L493e
        or byte ptr [ebp + 0x14], 0
        je L48d2
        cmp byte ptr [esi], 0xf8
        jae L493e
    L48d2:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov dl, byte ptr [esi]
        mov ax, word ptr [edi]
        mov dx, word ptr [ecx + edx*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L493e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L48c2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L48ac
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
// MATCH: jgld.dll 0x10054e20 ?draw16_10054e20@Sprite@@QAEHPAVSurface@@HHD@Z
int Sprite::draw16_10054e20(Surface* dst, int x, int y, char flag)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L513b:
        mov edx, dword ptr g_10128564
    L5141:
        cmp byte ptr [esi], 0xff
        jae L51bd
        or byte ptr [ebp + 0x14], 0
        je L5151
        cmp byte ptr [esi], 0xf8
        jae L51bd
    L5151:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L51bd:
        inc esi
        add edi, 2
        dec edx
        jne L5141
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L513b
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
        mov ecx, dword ptr g_1012856c
    L5284:
        mov edx, dword ptr g_10128564
    L528a:
        cmp byte ptr [esi], 0xff
        jae L5306
        or byte ptr [ebp + 0x14], 0
        je L529a
        cmp byte ptr [esi], 0xf8
        jae L5306
    L529a:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5306:
        inc esi
        add edi, 2
        dec edx
        jne L528a
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L5284
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
        mov ecx, dword ptr g_1012856c
    L53da:
        mov edx, dword ptr g_10128564
    L53e0:
        cmp byte ptr [esi], 0xff
        jae L545c
        or byte ptr [ebp + 0x14], 0
        je L53f0
        cmp byte ptr [esi], 0xf8
        jae L545c
    L53f0:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L545c:
        dec esi
        add edi, 2
        dec edx
        jne L53e0
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L53da
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
        mov ecx, dword ptr g_1012856c
    L5523:
        mov edx, dword ptr g_10128564
    L5529:
        cmp byte ptr [esi], 0xff
        jae L55a5
        or byte ptr [ebp + 0x14], 0
        je L5539
        cmp byte ptr [esi], 0xf8
        jae L55a5
    L5539:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L55a5:
        dec esi
        add edi, 2
        dec edx
        jne L5529
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L5523
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L5897:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L58ad:
        cmp byte ptr [esi], 0xff
        jae L5929
        or byte ptr [ebp + 0x14], 0
        je L58bd
        cmp byte ptr [esi], 0xf8
        jae L5929
    L58bd:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5929:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L58ad
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5897
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
        mov ecx, dword ptr g_1012856c
    L5a9f:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5ab5:
        cmp byte ptr [esi], 0xff
        jae L5b31
        or byte ptr [ebp + 0x14], 0
        je L5ac5
        cmp byte ptr [esi], 0xf8
        jae L5b31
    L5ac5:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5b31:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5ab5
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5a9f
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
        mov ecx, dword ptr g_1012856c
    L5cb4:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5cca:
        cmp byte ptr [esi], 0xff
        jae L5d46
        or byte ptr [ebp + 0x14], 0
        je L5cda
        cmp byte ptr [esi], 0xf8
        jae L5d46
    L5cda:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5d46:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5cca
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5cb4
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
        mov ecx, dword ptr g_1012856c
    L5edc:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L5ef2:
        cmp byte ptr [esi], 0xff
        jae L5f6e
        or byte ptr [ebp + 0x14], 0
        je L5f02
        cmp byte ptr [esi], 0xf8
        jae L5f6e
    L5f02:
        push ebx
        push ecx
        push edx
        xor eax, eax
        xor edx, edx
        mov al, byte ptr [esi]
        mov dx, word ptr [edi]
        mov ax, word ptr [ecx + eax*2]
        shl eax, 4
        shl edx, 4
        mov ebx, eax
        mov ecx, edx
        and eax, 0xf81f0
        and ebx, 0x7e00
        and ecx, 0xf81f0
        and edx, 0x7e00
        push esi
        mov esi, eax
        shl eax, 2
        sub eax, esi
        add eax, ecx
        shl eax, 2
        sub eax, esi
        mov esi, ebx
        shl ebx, 2
        sub ebx, esi
        add ebx, edx
        shl ebx, 2
        sub ebx, esi
        add eax, ecx
        add ebx, edx
        shr eax, 4
        shr ebx, 4
        pop esi
        and eax, 0xf81f0
        and ebx, 0x7e00
        add eax, ebx
        mov word ptr [edi], ax
        pop edx
        pop ecx
        pop ebx
    L5f6e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L5ef2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L5edc
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
// MATCH: jgld.dll 0x10056450 ?draw16_10056450@Sprite@@QAEHPAVSurface@@HH@Z
int Sprite::draw16_10056450(Surface* dst, int x, int y)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L676b:
        mov edx, dword ptr g_10128564
    L6771:
        cmp byte ptr [esi], 0xff
        jae L6792
        mov eax, edx
        and eax, 1
        jne L6792
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L6792
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6792:
        inc esi
        add edi, 2
        dec edx
        jne L6771
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L676b
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
        mov ecx, dword ptr g_1012856c
    L6851:
        mov edx, dword ptr g_10128564
    L6857:
        cmp byte ptr [esi], 0xff
        jae L6878
        mov eax, edx
        and eax, 1
        jne L6878
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L6878
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6878:
        inc esi
        add edi, 2
        dec edx
        jne L6857
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L6851
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
        mov ecx, dword ptr g_1012856c
    L6944:
        mov edx, dword ptr g_10128564
    L694a:
        cmp byte ptr [esi], 0xff
        jae L696b
        mov eax, edx
        and eax, 1
        jne L696b
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L696b
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L696b:
        dec esi
        add edi, 2
        dec edx
        jne L694a
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L6944
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
        mov ecx, dword ptr g_1012856c
    L6a2a:
        mov edx, dword ptr g_10128564
    L6a30:
        cmp byte ptr [esi], 0xff
        jae L6a51
        mov eax, edx
        and eax, 1
        jne L6a51
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L6a51
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6a51:
        dec esi
        add edi, 2
        dec edx
        jne L6a30
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L6a2a
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L6d3b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6d51:
        cmp byte ptr [esi], 0xff
        jae L6d72
        mov eax, edx
        and eax, 1
        jne L6d72
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L6d72
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6d72:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6d51
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6d3b
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
        mov ecx, dword ptr g_1012856c
    L6ee0:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6ef6:
        cmp byte ptr [esi], 0xff
        jae L6f17
        mov eax, edx
        and eax, 1
        jne L6f17
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L6f17
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6f17:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6ef6
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6ee0
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
        mov ecx, dword ptr g_1012856c
    L7092:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L70a8:
        cmp byte ptr [esi], 0xff
        jae L70c9
        mov eax, edx
        and eax, 1
        jne L70c9
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L70c9
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L70c9:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L70a8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7092
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
        mov ecx, dword ptr g_1012856c
    L7257:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L726d:
        cmp byte ptr [esi], 0xff
        jae L728e
        mov eax, edx
        and eax, 1
        jne L728e
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L728e
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L728e:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L726d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7257
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
// MATCH: jgld.dll 0x100576a0 ?draw16_100576a0@Sprite@@QAEHPAVSurface@@HH@Z
int Sprite::draw16_100576a0(Surface* dst, int x, int y)
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L79bb:
        mov edx, dword ptr g_10128564
    L79c1:
        cmp byte ptr [esi], 0xff
        jae L79ef
        mov eax, edx
        and eax, 1
        jne L79ef
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L79ef
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L79ef
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L79ef:
        inc esi
        add edi, 2
        dec edx
        jne L79c1
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L79bb
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
        mov ecx, dword ptr g_1012856c
    L7aae:
        mov edx, dword ptr g_10128564
    L7ab4:
        cmp byte ptr [esi], 0xff
        jae L7ae2
        mov eax, edx
        and eax, 1
        jne L7ae2
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L7ae2
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L7ae2
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L7ae2:
        inc esi
        add edi, 2
        dec edx
        jne L7ab4
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7aae
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
        mov ecx, dword ptr g_1012856c
    L7bae:
        mov edx, dword ptr g_10128564
    L7bb4:
        cmp byte ptr [esi], 0xff
        jae L7be2
        mov eax, edx
        and eax, 1
        jne L7be2
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L7be2
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L7be2
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L7be2:
        dec esi
        add edi, 2
        dec edx
        jne L7bb4
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7bae
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
        mov ecx, dword ptr g_1012856c
    L7ca1:
        mov edx, dword ptr g_10128564
    L7ca7:
        cmp byte ptr [esi], 0xff
        jae L7cd5
        mov eax, edx
        and eax, 1
        jne L7cd5
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L7cd5
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L7cd5
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L7cd5:
        dec esi
        add edi, 2
        dec edx
        jne L7ca7
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7ca1
        pop ecx
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
        push ecx
        mov ecx, dword ptr g_1012856c
    L7fbf:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7fd5:
        cmp byte ptr [esi], 0xff
        jae L8003
        mov eax, edx
        and eax, 1
        jne L8003
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L8003
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L8003
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L8003:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7fd5
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7fbf
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
        mov ecx, dword ptr g_1012856c
    L8171:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8187:
        cmp byte ptr [esi], 0xff
        jae L81b5
        mov eax, edx
        and eax, 1
        jne L81b5
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L81b5
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L81b5
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L81b5:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8187
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L8171
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
        mov ecx, dword ptr g_1012856c
    L8330:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8346:
        cmp byte ptr [esi], 0xff
        jae L8374
        mov eax, edx
        and eax, 1
        jne L8374
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L8374
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L8374
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L8374:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8346
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L8330
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
        mov ecx, dword ptr g_1012856c
    L8502:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8518:
        cmp byte ptr [esi], 0xff
        jae L8546
        mov eax, edx
        and eax, 1
        jne L8546
        mov eax, dword ptr g_10128578
        and eax, 1
        jne L8546
        mov eax, edx
        add eax, dword ptr g_10128578
        and eax, 2
        jne L8546
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L8546:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8518
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L8502
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

