// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitters onto 16-bit surfaces, family 2: the skeleton of jgld_blit16.cpp with a 4th int parameter
// kept in 0x10128548 for the __asm loop, and no unlock when the surface format is unknown. Names are chosen here.
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
extern int g_10128548;
extern int g_10128574;
extern int g_1012858a;
class Sprite {
public:
    virtual ~Sprite();
    int draw16_100298b0(Surface* dst, int x, int y, int p4);
    int draw16_10028630(Surface* dst, int x, int y, int p4);
    int draw16_10027430(Surface* dst, int x, int y, int p4);
    int draw16_10026240(Surface* dst, int x, int y, int p4);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x100298b0 ?draw16_100298b0@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw16_100298b0(Surface* dst, int x, int y, int p4)
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
        push ebp
        mov ebp, dword ptr g_10128574
    L9bc4:
        mov edx, dword ptr g_10128564
    L9bca:
        cmp byte ptr [esi], 0xff
        jae L9bf3
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae L9be1
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp L9bf3
    L9be1:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L9bf3:
        inc esi
        add edi, 2
        dec edx
        jne L9bca
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L9bc4
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    L9cba:
        mov edx, dword ptr g_10128564
    L9cc0:
        cmp byte ptr [esi], 0xff
        jae L9ce9
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae L9cd7
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp L9ce9
    L9cd7:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L9ce9:
        inc esi
        add edi, 2
        dec edx
        jne L9cc0
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L9cba
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    L9dbd:
        mov edx, dword ptr g_10128564
    L9dc3:
        cmp byte ptr [esi], 0xff
        jae L9dec
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae L9dda
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp L9dec
    L9dda:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L9dec:
        dec esi
        add edi, 2
        dec edx
        jne L9dc3
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L9dbd
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    L9eb3:
        mov edx, dword ptr g_10128564
    L9eb9:
        cmp byte ptr [esi], 0xff
        jae L9ee2
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae L9ed0
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp L9ee2
    L9ed0:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L9ee2:
        dec esi
        add edi, 2
        dec edx
        jne L9eb9
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L9eb3
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    La1d4:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La1ea:
        cmp byte ptr [esi], 0xff
        jae La213
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae La201
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp La213
    La201:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    La213:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La1ea
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La1d4
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    La389:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La39f:
        cmp byte ptr [esi], 0xff
        jae La3c8
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae La3b6
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp La3c8
    La3b6:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    La3c8:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La39f
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La389
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    La54b:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La561:
        cmp byte ptr [esi], 0xff
        jae La58a
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae La578
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp La58a
    La578:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    La58a:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La561
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La54b
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
        mov ecx, dword ptr g_1012856c
        push ebp
        mov ebp, dword ptr g_10128574
    La720:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    La736:
        cmp byte ptr [esi], 0xff
        jae La75f
        xor eax, eax
        cmp byte ptr [esi], 0xf8
        jae La74d
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
        jmp La75f
    La74d:
        mov al, byte ptr [esi]
        sub al, 0xf8
        shl eax, 0xf
        or ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    La75f:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns La736
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne La720
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
// MATCH: jgld.dll 0x10028630 ?draw16_10028630@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw16_10028630(Surface* dst, int x, int y, int p4)
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
        mov ecx, dword ptr g_1012856c
    L8943:
        mov edx, dword ptr g_10128564
    L8949:
        cmp byte ptr [esi], 0xff
        jae L8969
        cmp byte ptr [esi], 0xf8
        jne L8960
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L8969
    L8960:
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L8969:
        inc esi
        add edi, 2
        dec edx
        jne L8949
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8943
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
        mov ecx, dword ptr g_1012856c
    L8a2e:
        mov edx, dword ptr g_10128564
    L8a34:
        cmp byte ptr [esi], 0xff
        jae L8a54
        cmp byte ptr [esi], 0xf8
        jne L8a4b
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L8a54
    L8a4b:
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L8a54:
        inc esi
        add edi, 2
        dec edx
        jne L8a34
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8a2e
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
        mov ecx, dword ptr g_1012856c
    L8b26:
        mov edx, dword ptr g_10128564
    L8b2c:
        cmp byte ptr [esi], 0xff
        jae L8b4c
        cmp byte ptr [esi], 0xf8
        jne L8b43
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L8b4c
    L8b43:
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L8b4c:
        dec esi
        add edi, 2
        dec edx
        jne L8b2c
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8b26
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
        mov ecx, dword ptr g_1012856c
    L8c11:
        mov edx, dword ptr g_10128564
    L8c17:
        cmp byte ptr [esi], 0xff
        jae L8c37
        cmp byte ptr [esi], 0xf8
        jne L8c2e
        mov bx, word ptr [edi]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
        jmp L8c37
    L8c2e:
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L8c37:
        dec esi
        add edi, 2
        dec edx
        jne L8c17
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L8c11
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L8f27:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8f3d:
        cmp byte ptr [esi], 0xff
        jae L8f5f
        cmp byte ptr [esi], 0xf8
        jne L8f54
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L8f5f
    L8f54:
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L8f5f:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8f3d
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L8f27
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L90d3:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L90e9:
        cmp byte ptr [esi], 0xff
        jae L910b
        cmp byte ptr [esi], 0xf8
        jne L9100
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L910b
    L9100:
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L910b:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L90e9
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L90d3
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L928c:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L92a2:
        cmp byte ptr [esi], 0xff
        jae L92c4
        cmp byte ptr [esi], 0xf8
        jne L92b9
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L92c4
    L92b9:
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L92c4:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L92a2
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L928c
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L9458:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L946e:
        cmp byte ptr [esi], 0xff
        jae L9490
        cmp byte ptr [esi], 0xf8
        jne L9485
        mov ax, word ptr [edi]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
        jmp L9490
    L9485:
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L9490:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L946e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L9458
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
// MATCH: jgld.dll 0x10027430 ?draw16_10027430@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw16_10027430(Surface* dst, int x, int y, int p4)
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
        mov ecx, dword ptr g_1012856c
    L7743:
        mov edx, dword ptr g_10128564
    L7749:
        cmp byte ptr [esi], 0xff
        jae L775c
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L775c:
        inc esi
        add edi, 2
        dec edx
        jne L7749
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7743
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
        mov ecx, dword ptr g_1012856c
    L7821:
        mov edx, dword ptr g_10128564
    L7827:
        cmp byte ptr [esi], 0xff
        jae L783a
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L783a:
        inc esi
        add edi, 2
        dec edx
        jne L7827
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L7821
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
        mov ecx, dword ptr g_1012856c
    L790c:
        mov edx, dword ptr g_10128564
    L7912:
        cmp byte ptr [esi], 0xff
        jae L7925
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L7925:
        dec esi
        add edi, 2
        dec edx
        jne L7912
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L790c
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
        mov ecx, dword ptr g_1012856c
    L79ea:
        mov edx, dword ptr g_10128564
    L79f0:
        cmp byte ptr [esi], 0xff
        jae L7a03
        mov al, byte ptr [esi]
        mov bx, word ptr [ecx + eax*2]
        mov bx, word ptr [ebp + ebx*2]
        mov word ptr [edi], bx
    L7a03:
        dec esi
        add edi, 2
        dec edx
        jne L79f0
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L79ea
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L7cf3:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7d09:
        cmp byte ptr [esi], 0xff
        jae L7d1e
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L7d1e:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7d09
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7cf3
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L7e92:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L7ea8:
        cmp byte ptr [esi], 0xff
        jae L7ebd
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L7ebd:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L7ea8
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7e92
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L803e:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8054:
        cmp byte ptr [esi], 0xff
        jae L8069
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L8069:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8054
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L803e
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L81fd:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L8213:
        cmp byte ptr [esi], 0xff
        jae L8228
        xor eax, eax
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov ax, word ptr [ebp + eax*2]
        mov word ptr [edi], ax
    L8228:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L8213
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L81fd
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
// MATCH: jgld.dll 0x10026240 ?draw16_10026240@Sprite@@QAEHPAVSurface@@HHH@Z
int Sprite::draw16_10026240(Surface* dst, int x, int y, int p4)
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
        mov ecx, dword ptr g_1012856c
    L6553:
        mov edx, dword ptr g_10128564
    L6559:
        cmp byte ptr [esi], 0xff
        jae L656b
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L656b:
        inc esi
        add edi, 2
        dec edx
        jne L6559
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L6553
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
        mov ecx, dword ptr g_1012856c
    L6630:
        mov edx, dword ptr g_10128564
    L6636:
        cmp byte ptr [esi], 0xff
        jae L6648
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L6648:
        inc esi
        add edi, 2
        dec edx
        jne L6636
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L6630
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
        mov ecx, dword ptr g_1012856c
    L671a:
        mov edx, dword ptr g_10128564
    L6720:
        cmp byte ptr [esi], 0xff
        jae L6732
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L6732:
        dec esi
        add edi, 2
        dec edx
        jne L6720
        add esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L671a
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
        mov ecx, dword ptr g_1012856c
    L67f7:
        mov edx, dword ptr g_10128564
    L67fd:
        cmp byte ptr [esi], 0xff
        jae L680f
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov bx, word ptr [ecx + eax*2]
        mov word ptr [edi], bx
    L680f:
        dec esi
        add edi, 2
        dec edx
        jne L67fd
        sub esi, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L67f7
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
        mov dword ptr g_10128580, ecx
        mov eax, dword ptr g_10128540
        shl eax, 0x10
        mov dword ptr g_10128544, eax
        push ebp
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L6aff:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6b15:
        cmp byte ptr [esi], 0xff
        jae L6b29
        xor eax, eax
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6b29:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6b15
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6aff
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L6c9d:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6cb3:
        cmp byte ptr [esi], 0xff
        jae L6cc7
        xor eax, eax
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6cc7:
        add dx, bx
        adc esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6cb3
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6c9d
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L6e48:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L6e5e:
        cmp byte ptr [esi], 0xff
        jae L6e72
        xor eax, eax
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L6e72:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L6e5e
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L6e48
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
        mov ebp, dword ptr g_10128548
        mov ecx, dword ptr g_1012856c
    L7006:
        xor eax, eax
        mov ebx, dword ptr g_10128564
        mov bx, word ptr g_10128588
        mov dx, word ptr g_10128554
    L701c:
        cmp byte ptr [esi], 0xff
        jae L7030
        xor eax, eax
        mov al, byte ptr [esi]
        mov al, byte ptr [ebp + eax]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L7030:
        add dx, bx
        sbb esi, dword ptr g_10128580
        add edi, 2
        sub ebx, 0x10000
        jns L701c
        add edi, dword ptr g_10128560
        add edx, dword ptr g_10128544
        sbb eax, eax
        mov ebx, dword ptr g_10122e20
        add esi, dword ptr [ebx + eax*4]
        dec dword ptr g_10128578
        jne L7006
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

