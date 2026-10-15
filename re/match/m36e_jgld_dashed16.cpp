// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll Surface dashed lines on 16-bit surfaces, round 36 batch m36e: the 8-bit dashedHLine8 / dashedLine8 of
// jgld_surface.cpp with a palette step in front. A colour without bit 31 is an 8-bit index: it is translated through the
// surface's palette (+0x7c, else g_palClient1 0x1012873c's +4), colour 1 through the palette's slot +0x18 table and
// colour 2 through slot +0x1c whatever the format word format()[1] is (0 or 1; any other value returns without
// drawing); both are then cut to 16 bits. The pixel loops are inline __asm in the original (register-only loops in a
// /Od build, re/tools/asm2inline.py), the third keeping its count in g_10128450. Names are chosen here.
#include <windows.h>
#include <stdlib.h>
class Palette {
public:
    virtual void p0(); virtual void p1(); virtual void p2(); virtual void p3(); virtual void p4(); virtual void p5();
    virtual unsigned short* table1();        // slot 6 (+0x18)
    virtual unsigned short* table2();        // slot 7 (+0x1c)
};
struct PalClient { int m_0; Palette* m_palette; };
extern PalClient* g_palClient1;              // 0x1012873c
extern int g_10128450;                       // dashedHLine16's two-colour loop count
class Surface {
public:
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3();
    virtual void* lockBits();                // slot 4 (+0x10)
    virtual void v5(); virtual void v6();
    virtual unsigned short* lockAt16(int x, int y);   // slot 7 (+0x1c)
    virtual unsigned short* bits16();        // slot 8 (+0x20)
    virtual void unlock(int flag);           // slot 9 (+0x24)
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19();
    virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29();
    virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39();
    virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49();
    virtual void getClipRect(RECT* r);       // slot 50 (+0xc8)
    virtual RECT* clipRect();                // slot 51 (+0xcc)
    virtual void v52(); virtual void v53(); virtual void v54(); virtual void v55();
    virtual int pitch();                     // slot 56 (+0xe0)
    virtual int* format();                   // slot 57 (+0xe4)
    void dashedLine16(int x1, int y1, int x2, int y2, unsigned c1, unsigned c2, int len1, int len2, int phase);
    void dashedVLine16(int x, int y1, int y2, unsigned c1, unsigned c2, int len1, int len2, int phase);   // 0x1000d4f0
    void dashedHLine16(int x1, int x2, int y, unsigned c1, unsigned c2, int len1, int len2, int phase);
    char pad4[0x7c - 4];
    Palette* m_palette;                      // +0x7c
};

// MATCH: jgld.dll 0x1000cf90 ?dashedHLine16@Surface@@QAEXHHHIIHHH@Z
void Surface::dashedHLine16(int x1, int x2, int y, unsigned c1, unsigned c2, int len1, int len2, int phase)
{
    unsigned short* p;
    Palette* pal;
    if (y < clipRect()->top || y >= clipRect()->bottom)
        return;
    if (x1 == x2)
        return;
    if (c1 == -1 && c2 == -1)
        return;
    pal = 0;
    if (m_palette)
        pal = m_palette;
    else if (g_palClient1)
        pal = g_palClient1->m_palette;
    if (pal) {
        switch (format()[1]) {
        case 0:
            if (!(c1 & 0x80000000))
                c1 = pal->table1()[c1 & 0xff];
            if (!(c2 & 0x80000000))
                c2 = pal->table2()[c2 & 0xff];
            break;
        case 1:
            if (!(c1 & 0x80000000))
                c1 = pal->table1()[c1 & 0xff];
            if (!(c2 & 0x80000000))
                c2 = pal->table2()[c2 & 0xff];
            break;
        default:
            return;
        }
    }
    c1 &= 0xffff;
    c2 &= 0xffff;
    if (x1 > x2) {
        x1 ^= x2;
        x2 ^= x1;
        x1 ^= x2;
        len1 ^= len2;
        len2 ^= len1;
        len1 ^= len2;
        c1 ^= c2;
        c2 ^= c1;
        c1 ^= c2;
        phase = len1 + len2 - phase;
    }
    if (x1 >= clipRect()->right || x2 < clipRect()->left)
        return;
    if (x1 < clipRect()->left)
        x1 = clipRect()->left;
    if (x2 >= clipRect()->right)
        x2 = clipRect()->right - 1;
    p = lockAt16(x1, y);
    if (p == 0)
        return;
    phase %= len1 + len2;
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;
    if (c1 == -1) {
    __asm {
        push edi
        push esi
        mov ecx, dword ptr [ebp + 0xc]
        sub ecx, dword ptr [ebp + 8]
        inc ecx
        mov ax, word ptr [ebp + 0x18]
        mov ebx, dword ptr [ebp + 0x1c]
        mov esi, dword ptr [ebp + 0x20]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    Ld30b:
        and edx, edx
        js Ld319
        mov word ptr [edi], ax
        dec edx
        jne Ld31e
        mov edx, ebx
        jmp Ld31e
    Ld319:
        inc edx
        jne Ld31e
        mov edx, esi
    Ld31e:
        add edi, 2
        dec ecx
        jne Ld30b
        pop esi
        pop edi
    }
    } else if (c2 == -1) {
    __asm {
        push esi
        push edi
        mov ecx, dword ptr [ebp + 0xc]
        sub ecx, dword ptr [ebp + 8]
        inc ecx
        mov ax, word ptr [ebp + 0x14]
        mov ebx, dword ptr [ebp + 0x1c]
        mov esi, dword ptr [ebp + 0x20]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
    Ld34a:
        and edx, edx
        js Ld355
        dec edx
        jne Ld35d
        mov edx, ebx
        jmp Ld35d
    Ld355:
        mov word ptr [edi], ax
        inc edx
        jne Ld35d
        mov edx, esi
    Ld35d:
        add edi, 2
        dec ecx
        jne Ld34a
        pop edi
        pop esi
    }
    } else {
    __asm {
        push esi
        push edi
        mov ecx, dword ptr [ebp + 0xc]
        sub ecx, dword ptr [ebp + 8]
        inc ecx
        mov ax, word ptr [ebp + 0x14]
        mov ebx, dword ptr [ebp + 0x1c]
        mov esi, dword ptr [ebp + 0x20]
        mov edi, dword ptr [ebp - 8]
        mov edx, dword ptr [ebp + 0x24]
        mov dword ptr g_10128450, ecx
        mov cx, word ptr [ebp + 0x18]
        push ebp
        mov ebp, dword ptr g_10128450
    Ld391:
        and edx, edx
        js Ld39f
        mov word ptr [edi], cx
        dec edx
        jne Ld3a7
        mov edx, ebx
        jmp Ld3a7
    Ld39f:
        mov word ptr [edi], ax
        inc edx
        jne Ld3a7
        mov edx, esi
    Ld3a7:
        add edi, 2
        dec ebp
        jne Ld391
        pop ebp
        pop edi
        pop esi
    }
    }
    unlock(1);
}

#define PLOT16                                  \
    if (phase < 0) {                            \
        if (c1 != -1)                           \
            *p = (unsigned short)c1;            \
        phase++;                                \
        if (phase == 0)                         \
            phase = len2;                       \
    } else {                                    \
        if (c2 != -1)                           \
            *p = (unsigned short)c2;            \
        phase--;                                \
        if (phase == 0)                         \
            phase = len1;                       \
    }
// MATCH: jgld.dll 0x1000c140 ?dashedLine16@Surface@@QAEXHHHHIIHHH@Z
void Surface::dashedLine16(int x1, int y1, int x2, int y2, unsigned c1, unsigned c2, int len1, int len2, int phase)
{
    RECT rc;
    int d;
    int dx;
    int dy;
    int incE;
    int incNE;
    int incNE2;
    unsigned short* p;
    Palette* pal;
    if (x1 == x2) {
        dashedVLine16(x1, y1, y2, c1, c2, len1, len2, phase);
        return;
    }
    if (y1 == y2) {
        dashedHLine16(x1, x2, y1, c1, c2, len1, len2, phase);
        return;
    }
    pal = 0;
    if (m_palette)
        pal = m_palette;
    else if (g_palClient1)
        pal = g_palClient1->m_palette;
    if (pal) {
        switch (format()[1]) {
        case 0:
            if (!(c1 & 0x80000000))
                c1 = pal->table1()[c1 & 0xff];
            if (!(c2 & 0x80000000))
                c2 = pal->table2()[c2 & 0xff];
            break;
        case 1:
            if (!(c1 & 0x80000000))
                c1 = pal->table1()[c1 & 0xff];
            if (!(c2 & 0x80000000))
                c2 = pal->table2()[c2 & 0xff];
            break;
        default:
            return;
        }
    }
    c1 &= 0xffff;
    c2 &= 0xffff;
    if (x2 < x1) {
    __asm {
        mov eax, dword ptr [ebp + 8]
        xchg dword ptr [ebp + 0x10], eax
        mov dword ptr [ebp + 8], eax
        mov eax, dword ptr [ebp + 0xc]
        xchg dword ptr [ebp + 0x14], eax
        mov dword ptr [ebp + 0xc], eax
    }
        len1 ^= len2;
        len2 ^= len1;
        len1 ^= len2;
        c1 ^= c2;
        c2 ^= c1;
        c1 ^= c2;
        phase = len1 + len2 - phase;
    }
    if (lockBits() == 0)
        return;
    getClipRect(&rc);
    phase %= len1 + len2;
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;
    dx = x2 - x1;
    dy = y2 - y1;
    if (y1 < y2) {
        if (x1 > rc.right || y1 > rc.bottom || x2 < rc.left || y2 < rc.top)
            goto done;
        if (dx >= dy) {
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                } else {
                    d += incNE;
                    x1++;
                    y1++;
                }
            }
            if (x1 >= rc.right || y1 >= rc.bottom)
                goto done;
            p = bits16() + x1 + y1 * pitch();
            PLOT16
            if (x2 >= rc.right)
                x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                    p++;
                } else {
                    y1++;
                    if (y1 >= rc.bottom)
                        goto done;
                    d += incNE;
                    x1++;
                    p = p + pitch() + 1;
                }
                PLOT16
            }
            goto done;
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) {
                    d += incE;
                    y1++;
                } else {
                    d += incNE;
                    y1++;
                    x1++;
                }
            }
            if (x1 >= rc.right || y1 >= rc.bottom)
                goto done;
            p = bits16() + x1 + y1 * pitch();
            PLOT16
            if (y2 >= rc.bottom)
                y2 = rc.bottom - 1;
            while (y1 < y2) {
                if (d <= 0) {
                    d += incE;
                    y1++;
                    p += pitch();
                } else {
                    x1++;
                    if (x1 >= rc.right)
                        goto done;
                    d += incNE;
                    y1++;
                    p = p + pitch() + 1;
                }
                PLOT16
            }
            goto done;
        }
    } else {
        if (x1 > rc.right || y1 < rc.top || x2 < rc.left || y2 > rc.bottom)
            goto done;
        dy = abs(dy);
        if (dx >= dy) {
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE2 = (dy - dx) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                } else {
                    d += incNE2;
                    x1++;
                    y1--;
                }
            }
            if (x1 >= rc.right || y1 < rc.top)
                goto done;
            p = bits16() + x1 + y1 * pitch();
            PLOT16
            if (x2 >= rc.right)
                x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                    x1++;
                    p++;
                } else {
                    y1--;
                    if (y1 < rc.top)
                        goto done;
                    d += incNE2;
                    x1++;
                    p = p - pitch() + 1;
                }
                PLOT16
            }
            goto done;
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE2 = (dx - dy) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) {
                    d += incE;
                    y1--;
                } else {
                    d += incNE2;
                    y1--;
                    x1++;
                }
            }
            if (x1 >= rc.right || y1 < rc.top)
                goto done;
            p = bits16() + x1 + y1 * pitch();
            PLOT16
            if (y2 < rc.top)
                y2 = rc.top;
            while (y1 > y2) {
                if (d <= 0) {
                    d += incE;
                    y1--;
                    p -= pitch();
                } else {
                    x1++;
                    if (x1 >= rc.right)
                        goto done;
                    d += incNE2;
                    y1--;
                    p = p - pitch() + 1;
                }
                PLOT16
            }
            goto done;
        }
    }
done:
    unlock(1);
}
#undef PLOT16
