// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll sprite blitter onto 16-bit surfaces combining two sources (this and a second Sprite's bits) into one destination
// Surface, unscaled only, no lock or source checks (one-destination form of jgld_blitY3.cpp). Names are chosen here.
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
    int blitPair1(Sprite* other, Surface* dst, int x, int y);
    int m_4, m_8, m_c;
    Pal16* m_palette;                        // +0x10
    unsigned char* m_bits;                   // +0x14
    int m_18, m_1c, m_20, m_24, m_28;
    int m_pitch;                             // +0x2c
    int m_width;                             // +0x30
    int m_height;                            // +0x34
};

// MATCH: jgld.dll 0x10025dd0 ?blitPair1@Sprite@@QAEHPAV1@PAVSurface@@HH@Z
int Sprite::blitPair1(Sprite* other, Surface* dst, int x, int y)
{
    RECT rc;
    RECT orig;
    unsigned short* d;
    unsigned char* s;
    unsigned char* s2;
    Pal16* pal;
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
    setRect(&rc, x, y, m_width, m_height);
    orig = rc;
    if (!IntersectRect(&rc, &rc, dst->boundsRect())) {
        dst->unlock(1);
        return 0;
    }
    s = m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
    s2 = other->m_bits + (rc.left - orig.left) + (rc.top - orig.top) * g_1012857c;
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
        mov ebx, dword ptr [ebp - 0x30]
        mov edi, dword ptr [ebp - 0x28]
        push ecx
        push ebp
        mov ecx, dword ptr g_1012856c
    L602e:
        mov edx, dword ptr g_10128564
    L6034:
        cmp byte ptr [esi], 0xff
        jae L60fa
        cmp byte ptr [ebx], 0xff
        jae L60fa
        xor eax, eax
        xor ebp, ebp
        mov al, byte ptr [ebx]
        cmp al, 0
        je L60f1
        mov al, byte ptr [esi]
        mov bp, word ptr [ecx + eax*2]
        mov ax, word ptr [edi]
        push ecx
        push edx
        mov dx, bp
        shl edx, 0x10
        mov cx, ax
        shl ecx, 0x10
        mov cl, byte ptr [ebx]
        shr bp, 7
        and bp, 0xf8
        shr ax, 7
        and al, 0xf8
        mul cl
        shr ax, 8
        add ax, bp
        shr ax, 3
        shl ax, 0xa
        or dx, ax
        mov eax, ecx
        shr eax, 0x10
        mov ebp, edx
        shr ebp, 0x10
        shr bp, 2
        and bp, 0xf8
        shr ax, 2
        and ax, 0xf8
        mul cl
        shr ax, 8
        add ax, bp
        shr ax, 3
        shl ax, 5
        or dx, ax
        mov eax, ecx
        shr eax, 0x10
        mov ebp, edx
        shr ebp, 0x10
        shl bp, 3
        and bp, 0xf8
        shl ax, 3
        and ax, 0xf8
        mul cl
        shr ax, 8
        add ax, bp
        shr ax, 3
        or dx, ax
        mov word ptr [edi], dx
        pop edx
        pop ecx
        jmp L60fa
    L60f1:
        mov al, byte ptr [esi]
        mov ax, word ptr [ecx + eax*2]
        mov word ptr [edi], ax
    L60fa:
        inc esi
        inc ebx
        add edi, 2
        dec edx
        jne L6034
        add esi, dword ptr g_1012853c
        add ebx, dword ptr g_1012853c
        add edi, dword ptr g_10128560
        dec dword ptr g_10128578
        jne L602e
        pop ebp
        pop ecx
        pop edi
        pop esi
    }
    dst->unlock(2);
    return 0;
}

