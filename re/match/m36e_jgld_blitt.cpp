// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll Surface::blitT8from8 0x10013c50 / blitT8from16 0x10012db0 / blitT16 0x1000ed70 (hooks.csv blitT_8to8 /
// blitT_8to16 / blitT_16to16; dispatched by Surface::blitT 0x100091a0 in jgld_surface.cpp), round 36 batch m36e.
// Clip as blit8from8 does, then copy rows between the two surfaces with no transparency test (jgld_0.tsv). The lock
// of the ARGUMENT surface goes to edi and this surface's to esi, so the copy runs from this into the argument (as
// blit8from8, CLAUDE.md). When both are the same surface the copy direction follows the overlap: rows bottom-up when
// sy >= y, and pixels backward (std, from the last dword) when sx >= x. blitT8from16 widens this surface's 8-bit
// pixels into the argument's 16-bit ones through a palette table (slot +0x18 or +0x1c of the palette, picked by the
// argument's +0x28 word; any other value returns 1, no palette returns 0x10). The pixel loops are inline __asm in the
// original (generated from the binary by re/tools/asm2inline.py); their state sits in module globals (g_101284b4,
// g_10128464, and blitT8from16's g_1012849c table, g_10128498 row skip, g_1012848c width, g_10128494 row count).
// Names are chosen here.
#include <windows.h>
extern int g_101284b4;
extern int g_10128464;
extern int g_1012849c;
extern int g_10128498;
extern int g_1012848c;
extern int g_10128494;
void setRect(RECT* r, int x, int y, int w, int h);      // 0x10008360 (thunk 0x10001019)
BOOL intersect(RECT* dst, const RECT* a, const RECT* b); // 0x10008590 (thunk 0x10001087)
class Palette {
public:
    virtual void p0(); virtual void p1(); virtual void p2(); virtual void p3(); virtual void p4(); virtual void p5();
    virtual int table1();                    // slot 6 (+0x18)
    virtual int table2();                    // slot 7 (+0x1c)
};
struct PalClient { int m_0; Palette* m_palette; };
extern PalClient* g_palClient1;              // 0x1012873c
class Surface {
public:
    virtual void v0(); virtual void v1(); virtual void v2(); virtual void v3(); virtual void v4();
    virtual void* lockAt8(int x, int y);     // slot 5 (+0x14)
    virtual void v6();
    virtual void* lockAt16(int x, int y);    // slot 7 (+0x1c)
    virtual void v8();
    virtual void unlock(int flag);           // slot 9 (+0x24)
    virtual void v10(); virtual void v11(); virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15(); virtual void v16(); virtual void v17(); virtual void v18(); virtual void v19();
    virtual void v20(); virtual void v21(); virtual void v22(); virtual void v23(); virtual void v24(); virtual void v25(); virtual void v26(); virtual void v27(); virtual void v28(); virtual void v29();
    virtual void v30(); virtual void v31(); virtual void v32(); virtual void v33(); virtual void v34(); virtual void v35(); virtual void v36(); virtual void v37(); virtual void v38(); virtual void v39();
    virtual void v40(); virtual void v41(); virtual void v42(); virtual void v43(); virtual void v44(); virtual void v45(); virtual void v46(); virtual void v47(); virtual void v48(); virtual void v49();
    virtual void v50();
    virtual RECT* clipRect();                // slot 51 (+0xcc)
    virtual void v52(); virtual void v53();
    virtual int width();                     // slot 54 (+0xd8)
    virtual int height();                    // slot 55 (+0xdc)
    int blitT8from8(Surface* src, int x, int y, int sx, int sy, int w, int h);
    int blitT8from16(Surface* src, int x, int y, int sx, int sy, int w, int h);
    int blitT16(Surface* src, int x, int y, int sx, int sy, int w, int h);
    char pad4[0x28 - 4];
    int m_28;                                // +0x28: palette table selector (blitT8from16)
    char pad2c[0x40 - 0x2c];
    int m_40;                                // +0x40: pitch in pixels
    char pad44[0x7c - 0x44];
    Palette* m_palette;                      // +0x7c
    char pad80[0x4c0 - 0x80];
    int m_4c0;                               // +0x4c0
};

// MATCH: jgld.dll 0x10013c50 ?blitT8from8@Surface@@QAEHPAV1@HHHHHH@Z
int Surface::blitT8from8(Surface* src, int x, int y, int sx, int sy, int w, int h)
{
    char* a;
    char* b;
    int apitch;
    int bpitch;
    RECT rc;
    Surface* s = src;
    if (m_4c0 == 0)
        return 7;
    if (s == 0)
        return 0x10;
    if (x < 0) {
        w += x;
        sx -= x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        sy -= y;
        y = 0;
    }
    if (x > width())
        return 0;
    if (y > height())
        return 0;
    if (x + w > width())
        w = width() - x;
    if (y + h > height())
        h = height() - y;
    setRect(&rc, sx, sy, w, h);
    if (!intersect(&rc, &rc, s->clipRect()))
        return 0;
    x += rc.left - sx;
    y += rc.top - sy;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (s != this) {
        a = (char*)s->lockAt8(rc.left, rc.top);
        if (a == 0)
            return 3;
        b = (char*)lockAt8(x, y);
        if (b == 0) {
            s->unlock(1);
            return 3;
        }
        apitch = s->m_40;
        bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        mov edx, dword ptr [ebp - 0x14]
        sub edx, dword ptr [ebp + 0x1c]
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 2
        and ecx, 3
        mov dword ptr g_101284b4, ecx
    L3ed4:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_101284b4
        rep movsb
        add esi, edx
        add edi, ebx
        dec eax
        jne L3ed4
        pop ebp
        pop esi
        pop edi
        popf
    }
    } else if (sx < x) {
        if (sy < y) {
            a = (char*)s->lockAt8(rc.left, rc.top);
            if (a == 0)
                return 3;
            b = (char*)lockAt8(x, y);
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        mov edx, dword ptr [ebp - 0x14]
        sub edx, dword ptr [ebp + 0x1c]
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 2
        and ecx, 3
        mov dword ptr g_101284b4, ecx
    L3fbb:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_101284b4
        rep movsb
        add esi, edx
        add edi, ebx
        dec eax
        jne L3fbb
        pop ebp
        pop esi
        pop edi
        popf
    }
        } else {
            a = (char*)s->lockAt8(rc.left, rc.top + h - 1);
            if (a == 0)
                return 3;
            b = (char*)lockAt8(x, y + h - 1);
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        add ebx, dword ptr [ebp + 0x1c]
        mov edx, dword ptr [ebp - 0x14]
        add edx, dword ptr [ebp + 0x1c]
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 2
        and ecx, 3
        mov dword ptr g_101284b4, ecx
    L4098:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_101284b4
        rep movsb
        sub esi, edx
        sub edi, ebx
        dec eax
        jne L4098
        pop ebp
        pop esi
        pop edi
        popf
    }
        }
    } else {
        if (sy < y) {
            if (w < 4) {
                a = (char*)s->lockAt8(rc.left + w - 1, rc.top);
                b = (char*)lockAt8(x + w - 1, y);
            } else {
                a = (char*)s->lockAt8(rc.left + w - 4, rc.top);
                b = (char*)lockAt8(x + w - 4, y);
            }
            if (a == 0)
                return 3;
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        std
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        add ebx, dword ptr [ebp + 0x1c]
        mov edx, dword ptr [ebp - 0x14]
        add edx, dword ptr [ebp + 0x1c]
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 2
        and ecx, 3
        mov dword ptr g_101284b4, ecx
    L41d5:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_101284b4
        rep movsb
        add esi, edx
        add edi, ebx
        dec eax
        jne L41d5
        pop ebp
        pop esi
        pop edi
        popf
        cld
    }
        } else {
            if (w < 4) {
                a = (char*)s->lockAt8(rc.left + w - 1, rc.top + h - 1);
                b = (char*)lockAt8(x + w - 1, y + h - 1);
            } else {
                a = (char*)s->lockAt8(rc.left + w - 4, rc.top + h - 1);
                b = (char*)lockAt8(x + w - 4, y + h - 1);
            }
            if (a == 0)
                return 3;
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        std
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        mov edx, dword ptr [ebp - 0x14]
        sub edx, dword ptr [ebp + 0x1c]
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 2
        and ecx, 3
        mov dword ptr g_101284b4, ecx
    L4323:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_101284b4
        rep movsb
        sub esi, edx
        sub edi, ebx
        dec eax
        jne L4323
        pop ebp
        pop esi
        pop edi
        popf
        cld
    }
        }
    }
    unlock(1);
    s->unlock(1);
    return 0;
}

// MATCH: jgld.dll 0x1000ed70 ?blitT16@Surface@@QAEHPAV1@HHHHHH@Z
int Surface::blitT16(Surface* src, int x, int y, int sx, int sy, int w, int h)
{
    short* a;
    short* b;
    int apitch;
    int bpitch;
    RECT rc;
    Surface* s = src;
    if (s == 0)
        return 3;
    if (x < 0) {
        w += x;
        sx -= x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        sy -= y;
        y = 0;
    }
    if (x > width())
        return 0;
    if (y > height())
        return 0;
    if (x + w > width())
        w = width() - x;
    if (y + h > height())
        h = height() - y;
    setRect(&rc, sx, sy, w, h);
    if (!intersect(&rc, &rc, s->clipRect()))
        return 0;
    x += rc.left - sx;
    y += rc.top - sy;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (s != this) {
        a = (short*)s->lockAt16(rc.left, rc.top);
        if (a == 0)
            return 3;
        b = (short*)lockAt16(x, y);
        if (b == 0) {
            s->unlock(1);
            return 3;
        }
        apitch = s->m_40;
        bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr [ebp - 0x14]
        sub edx, dword ptr [ebp + 0x1c]
        shl edx, 1
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 1
        and ecx, 1
        mov dword ptr g_10128464, ecx
    Lefe1:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_10128464
        rep movsw
        add esi, edx
        add edi, ebx
        dec eax
        jne Lefe1
        pop ebp
        pop esi
        pop edi
        popf
    }
    } else if (sx < x) {
        if (sy < y) {
            a = (short*)s->lockAt16(rc.left, rc.top);
            if (a == 0)
                return 3;
            b = (short*)lockAt16(x, y);
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr [ebp - 0x14]
        sub edx, dword ptr [ebp + 0x1c]
        shl edx, 1
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 1
        and ecx, 1
        mov dword ptr g_10128464, ecx
    Lf0cc:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_10128464
        rep movsw
        add esi, edx
        add edi, ebx
        dec eax
        jne Lf0cc
        pop ebp
        pop esi
        pop edi
        popf
    }
        } else {
            a = (short*)s->lockAt16(rc.left, rc.top + h - 1);
            if (a == 0)
                return 3;
            b = (short*)lockAt16(x, y + h - 1);
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        add ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr [ebp - 0x14]
        add edx, dword ptr [ebp + 0x1c]
        shl edx, 1
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 1
        and ecx, 1
        mov dword ptr g_10128464, ecx
    Lf1ad:
        mov ecx, ebp
        rep movsd
        mov ecx, dword ptr g_10128464
        rep movsw
        sub esi, edx
        sub edi, ebx
        dec eax
        jne Lf1ad
        pop ebp
        pop esi
        pop edi
        popf
    }
        }
    } else {
        if (sy < y) {
            if (w < 4) {
                a = (short*)s->lockAt16(rc.left + w - 1, rc.top);
                b = (short*)lockAt16(x + w - 1, y);
            } else {
                a = (short*)s->lockAt16(rc.left + w - 2, rc.top);
                b = (short*)lockAt16(x + w - 2, y);
            }
            if (a == 0)
                return 3;
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        std
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        add ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr [ebp - 0x14]
        add edx, dword ptr [ebp + 0x1c]
        shl edx, 1
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 1
        and ecx, 1
        mov dword ptr g_10128464, ecx
        cmp dword ptr g_10128464, 0
        jne Lf304
    Lf2f7:
        mov ecx, ebp
        rep movsd
        add esi, edx
        add edi, ebx
        dec eax
        jne Lf2f7
        jmp Lf317
    Lf304:
        mov ecx, ebp
        rep movsd
        mov cx, word ptr [esi + 2]
        mov word ptr [edi + 2], cx
        add esi, edx
        add edi, ebx
        dec eax
        jne Lf304
    Lf317:
        pop ebp
        pop esi
        pop edi
        popf
        cld
    }
        } else {
            if (w < 4) {
                a = (short*)s->lockAt16(rc.left + w - 1, rc.top + h - 1);
                b = (short*)lockAt16(x + w - 1, y + h - 1);
            } else {
                a = (short*)s->lockAt16(rc.left + w - 2, rc.top + h - 1);
                b = (short*)lockAt16(x + w - 2, y + h - 1);
            }
            if (a == 0)
                return 3;
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            bpitch = m_40;
    __asm {
        pushf
        push edi
        push esi
        std
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr [ebp - 0x14]
        sub edx, dword ptr [ebp + 0x1c]
        shl edx, 1
        mov ecx, dword ptr [ebp + 0x1c]
        push ebp
        mov ebp, ecx
        shr ebp, 1
        and ecx, 1
        mov dword ptr g_10128464, ecx
        cmp dword ptr g_10128464, 0
        jne Lf46b
    Lf45e:
        mov ecx, ebp
        rep movsd
        sub esi, edx
        sub edi, ebx
        dec eax
        jne Lf45e
        jmp Lf47e
    Lf46b:
        mov ecx, ebp
        rep movsd
        mov cx, word ptr [esi + 2]
        mov word ptr [edi + 2], cx
        sub esi, edx
        sub edi, ebx
        dec eax
        jne Lf46b
    Lf47e:
        pop ebp
        pop esi
        pop edi
        popf
        cld
    }
        }
    }
    unlock(1);
    s->unlock(1);
    return 0;
}

// MATCH: jgld.dll 0x10012db0 ?blitT8from16@Surface@@QAEHPAV1@HHHHHH@Z
int Surface::blitT8from16(Surface* src, int x, int y, int sx, int sy, int w, int h)
{
    short* a;
    unsigned char* b;
    int apitch;
    RECT rc;
    Palette* pal;
    Surface* s = src;
    if (m_4c0 == 0)
        return 7;
    if (s == 0)
        return 0x10;
    if (x < 0) {
        w += x;
        sx -= x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        sy -= y;
        y = 0;
    }
    if (x > width())
        return 0;
    if (y > height())
        return 0;
    if (x + w > width())
        w = width() - x;
    if (y + h > height())
        h = height() - y;
    setRect(&rc, sx, sy, w, h);
    if (!intersect(&rc, &rc, s->clipRect()))
        return 0;
    if (m_palette)
        pal = m_palette;
    else
        pal = g_palClient1->m_palette;
    if (pal == 0)
        return 0x10;
    switch (s->m_28) {
    case 0:
        g_1012849c = pal->table1();
        break;
    case 1:
        g_1012849c = pal->table2();
        break;
    default:
        return 1;
    }
    x += rc.left - sx;
    y += rc.top - sy;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    apitch = s->m_40;
    g_10128498 = m_40;
    if (s != this) {
        a = (short*)s->lockAt16(rc.left, rc.top);
        if (a == 0)
            return 3;
        b = (unsigned char*)lockAt8(x, y);
        if (b == 0) {
            s->unlock(1);
            return 3;
        }
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr g_10128498
        sub edx, dword ptr [ebp + 0x1c]
        mov dword ptr g_10128498, edx
        mov ecx, dword ptr [ebp + 0x1c]
        mov eax, dword ptr [ebp + 0x20]
        mov dword ptr g_1012848c, ecx
        mov dword ptr g_10128494, eax
        xor eax, eax
        push ebp
        mov ebp, dword ptr g_1012849c
    L30cc:
        mov edx, dword ptr g_1012848c
    L30d2:
        mov al, byte ptr [esi]
        mov cx, word ptr [ebp + eax*2]
        inc esi
        mov word ptr [edi], cx
        add edi, 2
        dec edx
        jne L30d2
        add esi, dword ptr g_10128498
        add edi, ebx
        dec dword ptr g_10128494
        jne L30cc
        pop ebp
        pop esi
        pop edi
        popf
    }
    } else if (sx < x) {
        if (sy < y) {
            a = (short*)s->lockAt16(rc.left, rc.top);
            if (a == 0)
                return 3;
            b = (unsigned char*)lockAt8(x, y);
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr g_10128498
        sub edx, dword ptr [ebp + 0x1c]
        mov dword ptr g_10128498, edx
        mov ecx, dword ptr [ebp + 0x1c]
        mov eax, dword ptr [ebp + 0x20]
        mov dword ptr g_1012848c, ecx
        mov dword ptr g_10128494, eax
        xor eax, eax
        push ebp
        mov ebp, dword ptr g_1012849c
    L31c8:
        mov edx, dword ptr g_1012848c
    L31ce:
        mov al, byte ptr [esi]
        mov cx, word ptr [ebp + eax*2]
        inc esi
        mov word ptr [edi], cx
        add edi, 2
        dec edx
        jne L31ce
        add esi, dword ptr g_10128498
        add edi, ebx
        dec dword ptr g_10128494
        jne L31c8
        pop ebp
        pop esi
        pop edi
        popf
    }
        } else {
            a = (short*)s->lockAt16(rc.left, rc.top + h - 1);
            if (a == 0)
                return 3;
            b = (unsigned char*)lockAt8(x, y + h - 1);
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
    __asm {
        pushf
        push edi
        push esi
        cld
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        add ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr g_10128498
        add edx, dword ptr [ebp + 0x1c]
        mov dword ptr g_10128498, edx
        mov ecx, dword ptr [ebp + 0x1c]
        mov eax, dword ptr [ebp + 0x20]
        mov dword ptr g_1012848c, ecx
        mov dword ptr g_10128494, eax
        xor eax, eax
        push ebp
        mov ebp, dword ptr g_1012849c
    L32ba:
        mov edx, dword ptr g_1012848c
    L32c0:
        mov al, byte ptr [esi]
        mov cx, word ptr [ebp + eax*2]
        inc esi
        mov word ptr [edi], cx
        add edi, 2
        dec edx
        jne L32c0
        sub esi, dword ptr g_10128498
        sub edi, ebx
        dec dword ptr g_10128494
        jne L32ba
        pop ebp
        pop esi
        pop edi
        popf
    }
        }
    } else {
        if (sy < y) {
            if (w < 4) {
                a = (short*)s->lockAt16(rc.left + w - 1, rc.top);
                b = (unsigned char*)lockAt8(x + w - 1, y);
            } else {
                a = (short*)s->lockAt16(rc.left + w - 4, rc.top);
                b = (unsigned char*)lockAt8(x + w - 4, y);
            }
            if (a == 0)
                return 3;
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
    __asm {
        pushf
        push edi
        push esi
        std
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        add ebx, dword ptr [ebp + 0x1c]
        shl ebx, 1
        mov edx, dword ptr g_10128498
        add edx, dword ptr [ebp + 0x1c]
        mov dword ptr g_10128498, edx
        mov ecx, dword ptr [ebp + 0x1c]
        mov eax, dword ptr [ebp + 0x20]
        mov dword ptr g_1012848c, ecx
        mov dword ptr g_10128494, eax
        xor eax, eax
        push ebp
        mov ebp, dword ptr g_1012849c
    L340c:
        mov edx, dword ptr g_1012848c
    L3412:
        mov al, byte ptr [esi]
        mov cx, word ptr [ebp + eax*2]
        inc esi
        mov word ptr [edi], cx
        add edi, 2
        dec edx
        jne L3412
        add esi, dword ptr g_10128498
        add edi, ebx
        dec dword ptr g_10128494
        jne L340c
        pop ebp
        pop esi
        pop edi
        popf
        cld
    }
        } else {
            if (w < 4) {
                a = (short*)s->lockAt16(rc.left + w - 1, rc.top + h - 1);
                b = (unsigned char*)lockAt8(x + w - 1, y + h - 1);
            } else {
                a = (short*)s->lockAt16(rc.left + w - 4, rc.top + h - 1);
                b = (unsigned char*)lockAt8(x + w - 4, y + h - 1);
            }
            if (a == 0)
                return 3;
            if (b == 0) {
                s->unlock(1);
                return 3;
            }
            apitch = s->m_40;
            g_10128498 = m_40;
    __asm {
        pushf
        push edi
        push esi
        std
        mov edi, dword ptr [ebp - 8]
        mov esi, dword ptr [ebp - 0xc]
        mov eax, dword ptr [ebp + 0x20]
        mov ebx, dword ptr [ebp - 0x10]
        sub ebx, dword ptr [ebp + 0x1c]
        mov edx, dword ptr g_10128498
        sub edx, dword ptr [ebp + 0x1c]
        mov dword ptr g_10128498, edx
        mov ecx, dword ptr [ebp + 0x1c]
        mov eax, dword ptr [ebp + 0x20]
        mov dword ptr g_1012848c, ecx
        mov dword ptr g_10128494, eax
        xor eax, eax
        push ebp
        mov ebp, dword ptr g_1012849c
    L3581:
        mov edx, dword ptr g_1012848c
    L3587:
        mov al, byte ptr [esi]
        mov cx, word ptr [ebp + eax*2]
        inc esi
        mov word ptr [edi], cx
        add edi, 2
        dec edx
        jne L3587
        sub esi, dword ptr g_10128498
        sub edi, ebx
        dec eax
        jne L3581
        pop ebp
        pop esi
        pop edi
        popf
        cld
    }
        }
    }
    unlock(1);
    s->unlock(1);
    return 0;
}
