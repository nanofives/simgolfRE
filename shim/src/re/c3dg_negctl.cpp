// NEGATIVE CONTROL for batch c3dg. NEVER list this file in shim/re_batches.txt: every body below is wrong on
// purpose, in ONE detail each, to prove the c3dg A/B keys see that detail. Built only through
// log/c3/c3dg_negctl_batches.txt (log/c3/alt/c3dg_negctl.py wrong). The correct bodies are in c3dg.cpp.
//   0x00002ef0 MappedFile::closeTruncate  closes the file handle when it is not -1 (the original tests != 0)
//   0x000699c0 realizePalette             never calls RealizePalette (state: $implog)
//   0x0006aa70 createPalette              PC_EXPLICIT entries 246/247 get index 0xf6 + i, not 0xf5 + i ($arena)
//   0x00069c70 onKey                      PeekMessageA with PM_NOREMOVE 0 instead of PM_REMOVE 1 ($implog)
//   0x00065a90 Display::enumModes         the third dword of the 5-dword record is the height, not the width
//   0x0001b690 Sprite::create8            hands _expand one byte more than +0x24 ($exlog)
//   0x00014cd0 Sprite::ctorN              writes 0xfe at +0x28 instead of 0xff ($blk)
#include "hooks.h"

namespace {

char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}
void* JgldVA(unsigned va) { return JgldBase() + (va - 0x10000000); }
template <class T> T& G(unsigned va) { return *static_cast<T*>(JgldVA(va)); }
template <class T> T& F(void* obj, unsigned off) { return *reinterpret_cast<T*>(static_cast<char*>(obj) + off); }
template <class T> T Imp(unsigned slotVa) { return *static_cast<T*>(JgldVA(slotVa)); }
template <class T> T Virt(void* obj, unsigned off) { return F<T>(F<void*>(obj, 0), off); }

typedef int(__stdcall* Api1_t)(void*);
typedef unsigned(__stdcall* SetFilePointer_t)(void*, int, int*, unsigned);
typedef void*(__stdcall* GetDC_t)(void*);
typedef int(__stdcall* ReleaseDC_t)(void*, void*);
typedef void*(__stdcall* SelectPalette_t)(void*, void*, int);
typedef void*(__stdcall* CreatePalette_t)(void*);
typedef int(__stdcall* PeekMessageA_t)(void*, void*, unsigned, unsigned, unsigned);
typedef int(__stdcall* EnumDisplaySettingsA_t)(const char*, unsigned, void*);
typedef void(__stdcall* CritSec_t)(void*);
typedef void(__fastcall* Method_t)(void*, void*);
typedef int(__fastcall* IntMethod_t)(void*, void*);
typedef void*(__cdecl* Alloc_t)(unsigned);
typedef void(__cdecl* Free_t)(void*);
typedef int(__cdecl* Cdecl0_t)();
typedef void*(__cdecl* Memcpy_t)(void*, const void*, unsigned);
typedef void*(__cdecl* Expand_t)(void*, unsigned);

void CloseMf(void* self) { reinterpret_cast<Method_t>(JgldVA(0x100018bb))(self, 0); }

typedef void(__fastcall* MfCloseTruncate_t)(void*, void*, char*);
MfCloseTruncate_t MfCloseTruncate_orig;
void __fastcall MfCloseTruncate_re(void* self, void*, char* end) {
    if (reinterpret_cast<unsigned>(end) < F<unsigned>(self, 4)) { CloseMf(self); return; }
    const unsigned length = reinterpret_cast<unsigned>(end) - F<unsigned>(self, 4);
    if (F<void*>(self, 4) != 0) { Imp<Api1_t>(0x1012c430)(F<void*>(self, 4)); F<void*>(self, 4) = 0; }
    if (F<void*>(self, 0xc) != 0) { Imp<Api1_t>(0x1012c428)(F<void*>(self, 0xc)); F<void*>(self, 0xc) = 0; }
    if (Imp<SetFilePointer_t>(0x1012c438)(F<void*>(self, 8), static_cast<int>(length), 0, 0) == 0xffffffffu) {
        CloseMf(self);
        return;
    }
    Imp<Api1_t>(0x1012c43c)(F<void*>(self, 8));
    if (F<int>(self, 8) != -1) {                                  // DEFECT: the original tests != 0
        Imp<Api1_t>(0x1012c428)(F<void*>(self, 8));
        F<void*>(self, 8) = 0;
    }
}

typedef int(__cdecl* RealizePal_t)(void*);
RealizePal_t RealizePal_orig;
int __cdecl RealizePal_re(void* hwnd) {
    void* dc = Imp<GetDC_t>(0x1012c5ec)(hwnd);
    Imp<SelectPalette_t>(0x1012c3b8)(dc, G<void*>(0x10128738), 0);
    // DEFECT: RealizePalette (0x10069a10) is not called
    Imp<ReleaseDC_t>(0x1012c630)(hwnd, dc);
    return 1;
}

typedef int(__cdecl* CreatePal_t)(void*, void*);
CreatePal_t CreatePal_orig;
int __cdecl CreatePal_re(void* a, void* b) {
    reinterpret_cast<Cdecl0_t>(JgldVA(0x1000129e))();
    unsigned char* lp = static_cast<unsigned char*>(reinterpret_cast<Alloc_t>(JgldVA(0x100017c6))(0x404));
    if (lp == 0) return 4;
    F<unsigned short>(lp, 0) = 0x300;
    F<unsigned short>(lp, 2) = 0x100;
    const unsigned char* low = static_cast<const unsigned char*>(JgldVA(0x1011dab0));
    const unsigned char* high = static_cast<const unsigned char*>(JgldVA(0x1011dad8));
    for (int i = 0; i < 0xa; ++i) {
        lp[4 + i * 4 + 0] = low[i * 4 + 2]; lp[4 + i * 4 + 1] = low[i * 4 + 1];
        lp[4 + i * 4 + 2] = low[i * 4 + 0]; lp[4 + i * 4 + 3] = 0;
        lp[0x3dc + i * 4 + 0] = high[i * 4 + 2]; lp[0x3dc + i * 4 + 1] = high[i * 4 + 1];
        lp[0x3dc + i * 4 + 2] = high[i * 4 + 0]; lp[0x3dc + i * 4 + 3] = 0;
    }
    for (int i = 0xa; i < 0xf6; ++i) lp[4 + i * 4 + 3] = 5;
    for (int i = 0; i < 2; ++i) {
        lp[0x24 + i * 4 + 0] = static_cast<unsigned char>(i + 8);
        lp[0x24 + i * 4 + 1] = 0; lp[0x24 + i * 4 + 2] = 0; lp[0x24 + i * 4 + 3] = 2;
        lp[0x3dc + i * 4 + 0] = static_cast<unsigned char>(i + 0xf6);   // DEFECT: 0xf5 + i in the original
        lp[0x3dc + i * 4 + 1] = 0; lp[0x3dc + i * 4 + 2] = 0; lp[0x3dc + i * 4 + 3] = 2;
    }
    G<void*>(0x10128738) = Imp<CreatePalette_t>(0x1012c3b0)(lp);
    Free_t jfree = reinterpret_cast<Free_t>(JgldVA(0x100017b7));
    if (G<void*>(0x10128738) == 0) { jfree(lp); return 1; }
    jfree(lp);
    Virt<Method_t>(a, 0)(a, 0);
    Virt<Method_t>(b, 0)(b, 0);
    G<void*>(0x1012873c) = a;
    G<void*>(0x10128740) = b;
    return 0;
}

typedef void(__cdecl* OnKey_t)(void*, unsigned, int);
OnKey_t OnKey_orig;
void __cdecl OnKey_re(void* hwnd, unsigned key, int down) {
    void* sink = G<void*>(0x1012872c);
    reinterpret_cast<void(__fastcall*)(void*, void*, unsigned, int)>(F<void*>(F<void*>(sink, 0), 0xc))(sink, 0, key, down);
    if (down == 0) return;
    if (key < 0x60 || key > 0x6f) {
        for (;;) {
            void* display = G<void*>(0x10128420);
            if (Virt<IntMethod_t>(display, 0x138)(display, 0) == 0) break;
        }
        return;
    }
    unsigned char msg[0x1c];
    while (Imp<PeekMessageA_t>(0x1012c63c)(msg, hwnd, 0x102, 0x102, 0) != 0)   // DEFECT: PM_NOREMOVE
        ;
}

struct Rec14 { unsigned v[5]; };
struct Rec94 { unsigned char b[0x94]; };
typedef void(__fastcall* Rec14Add_t)(void*, void*, Rec14);
typedef void(__fastcall* Rec94Add_t)(void*, void*, Rec94);
typedef void*(__fastcall* EnumModes_t)(void*, void*);
EnumModes_t EnumModes_orig;
void* __fastcall EnumModes_re(void*, void*) {
    Rec94 dm;
    F<unsigned short>(&dm, 0x24) = 0x94;
    F<unsigned>(&dm, 0x28) = 0;
    F<unsigned short>(&dm, 0x26) = 0;
    reinterpret_cast<Method_t>(JgldVA(0x1000116d))(JgldVA(0x101286c8), 0);
    reinterpret_cast<Method_t>(JgldVA(0x1000160e))(JgldVA(0x101286f8), 0);
    for (unsigned i = 0;; ++i) {
        if (Imp<EnumDisplaySettingsA_t>(0x1012c60c)(0, i, &dm) == 0) break;
        Rec14 r;
        r.v[3] = F<unsigned>(&dm, 0x68);
        r.v[0] = F<unsigned>(&dm, 0x6c);
        r.v[1] = F<unsigned>(&dm, 0x70);
        r.v[2] = F<unsigned>(&dm, 0x70);                         // DEFECT: the original repeats the width +0x6c
        r.v[4] = F<unsigned>(&dm, 0x78);
        reinterpret_cast<Rec14Add_t>(JgldVA(0x1000119f))(JgldVA(0x101286c8), 0, r);
        reinterpret_cast<Rec94Add_t>(JgldVA(0x10001375))(JgldVA(0x101286f8), 0, dm);
    }
    return JgldVA(0x101286c8);
}

struct RowSpan { unsigned char start; unsigned char len; unsigned short off; };
typedef int(__fastcall* Create8_t)(void*, void*, const unsigned char*, unsigned, int, int, int);
Create8_t Create8_orig;
int __fastcall Create8_re(void* self, void*, const unsigned char* bits, unsigned w, int h, int, int) {
    Virt<Method_t>(self, 8)(self, 0);
    F<unsigned>(self, 0x30) = w; F<int>(self, 0x34) = h; F<int>(self, 0x20) = 8; F<unsigned>(self, 0x24) = w * h;
    Alloc_t jalloc = reinterpret_cast<Alloc_t>(JgldVA(0x100017c6));
    F<unsigned char*>(self, 0x14) = static_cast<unsigned char*>(jalloc(F<unsigned>(self, 0x24)));
    if (F<unsigned char*>(self, 0x14) == 0) return 4;
    G<unsigned>(0x101284c8) += w * h;
    if (bits != 0) {
        bool flat = true;
        if (G<int>(0x101284c4) != 0 && w > 4 && static_cast<int>(w) < 0x100 && static_cast<int>(w * h) < 0x10000) {
            flat = false;
            unsigned budget = w * h - h * 4;
            F<RowSpan*>(self, 0x1c) = static_cast<RowSpan*>(jalloc(h * 4));
            if (F<RowSpan*>(self, 0x1c) == 0) { Virt<Method_t>(self, 8)(self, 0); return 4; }
            unsigned char* dst = F<unsigned char*>(self, 0x14);
            const unsigned char* row = bits;
            for (int y = 0; y < h; ++y) {
                RowSpan* sp = F<RowSpan*>(self, 0x1c) + y;
                int x = 0;
                while (x < static_cast<int>(w) && row[x] == F<unsigned>(self, 0x28)) ++x;
                sp->start = static_cast<unsigned char>(x);
                x = static_cast<int>(w) - 1;
                while (x > sp->start && row[x] == F<unsigned>(self, 0x28)) --x;
                sp->len = static_cast<unsigned char>(x + 1 - sp->start);
                if (sp->len != 0) {
                    if (budget < sp->len) {
                        reinterpret_cast<Free_t>(JgldVA(0x100017b7))(F<void*>(self, 0x1c));
                        F<void*>(self, 0x1c) = 0;
                        flat = true;
                        break;
                    }
                    sp->off = static_cast<unsigned short>(dst - F<unsigned char*>(self, 0x14));
                    reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0))(dst, row + sp->start, sp->len);
                    budget -= sp->len;
                    dst += sp->len;
                }
                row += w;
            }
            if (!flat) {
                F<unsigned>(self, 0x18) |= 1;
                F<unsigned>(self, 0x24) = w * h - budget;
                F<void*>(self, 0x14) = reinterpret_cast<Expand_t>(JgldVA(0x10080610))(
                    F<void*>(self, 0x14), F<unsigned>(self, 0x24) + 1);   // DEFECT: one byte more
                G<unsigned>(0x101284cc) += w * h - F<unsigned>(self, 0x24) - h * 4;
            }
        }
        if (flat) {
            F<unsigned>(self, 0x18) &= ~1u;
            reinterpret_cast<Memcpy_t>(JgldVA(0x1007f3a0))(F<void*>(self, 0x14), bits, w * h);
        }
    }
    F<unsigned>(self, 0x2c) = w;
    return 0;
}

typedef void*(__fastcall* SpriteCtorN_t)(void*, void*, void*);
SpriteCtorN_t SpriteCtorN_orig;
void* __fastcall SpriteCtorN_re(void* self, void*, void* owner) {
    reinterpret_cast<Method_t>(JgldVA(0x100017c1))(self, 0);
    F<void*>(self, 0) = JgldVA(0x1011d380);
    F<void*>(self, 0xc) = owner;
    for (unsigned off = 8; off <= 0x34; off += 4)
        if (off != 0xc) F<unsigned>(self, off) = 0;
    F<unsigned>(self, 0x28) = 0xfe;                               // DEFECT: 0xff in the original (0x10014d50)
    Imp<CritSec_t>(0x1012c41c)(static_cast<char*>(self) + 0x38);
    return self;
}

}  // namespace

// 0x00002ef0  MappedFile::closeTruncate (negative control)
SG_HOOK("jgld.dll", 0x00002ef0, jgld_MappedFile_closeTruncate, MfCloseTruncate_re, MfCloseTruncate_orig);
// 0x000699c0  realizePalette (negative control)
SG_HOOK("jgld.dll", 0x000699c0, jgld_realizePalette, RealizePal_re, RealizePal_orig);
// 0x0006aa70  createPalette (negative control)
SG_HOOK("jgld.dll", 0x0006aa70, jgld_createPalette, CreatePal_re, CreatePal_orig);
// 0x00069c70  onKey (negative control)
SG_HOOK("jgld.dll", 0x00069c70, jgld_onKey, OnKey_re, OnKey_orig);
// 0x00065a90  Display::enumModes (negative control)
SG_HOOK("jgld.dll", 0x00065a90, jgld_Display_enumModes, EnumModes_re, EnumModes_orig);
// 0x0001b690  Sprite::create8 (negative control)
SG_HOOK("jgld.dll", 0x0001b690, jgld_Sprite_create8, Create8_re, Create8_orig);
// 0x00014cd0  Sprite::ctorN (negative control)
SG_HOOK("jgld.dll", 0x00014cd0, jgld_Sprite_ctorN, SpriteCtorN_re, SpriteCtorN_orig);
