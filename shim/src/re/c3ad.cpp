// C3 batch c3ad (2026-10-08) of golf_clean.exe: a large (2 KB) pure-math render function over the candidate set
// of functions >= 2 KB. Hand-written from the disassembly (re/tools/asm2inline.py golf_clean.exe 0x00461830
// --list); each body cites the address of every global, offset and callee it uses. __thiscall is emulated with
// __fastcall (ecx = this, edx unused). The virtual display-mode call and brightenComponent are invoked through
// their original addresses, so both A/B arms see the same callees.
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }

// The 16-bit display object is DAT_0083ad50 (loaded at 0x00461880, 0x004618fa, ...); its vtable slot +0xb4
// returns the pixel format and the builder branches on `cmp eax,1` (RGB565 when 1, RGB555 otherwise), e.g. at
// 0x0046188e / 0x00461891. Called thiscall (ecx = object), no stack arguments.
int displayMode565() {
    unsigned obj = at<unsigned>(0x0083ad50);                 // mov ecx,[0x83ad50]
    unsigned vtbl = at<unsigned>(obj);                       // mov edx,[ecx]
    typedef int(__fastcall* ModeFn)(unsigned ecx, unsigned edx);
    ModeFn fn = reinterpret_cast<ModeFn>(at<unsigned>(vtbl + 0xb4));   // call [edx+0xb4]
    return fn(obj, 0) == 1 ? 1 : 0;
}

// brightenComponent 0x00461810 (v + 0x20 saturating at 0xff), called three times in the last block at
// 0x00461f1d / 0x00461f2b / 0x00461f39. Invoked through its original address.
typedef int(__cdecl* Int1_t)(int);
const Int1_t kBrighten = reinterpret_cast<Int1_t>(0x00461810);

// Pack an (x, y, z) byte triple into the 16-bit pixel the builder forms. RGB565 (0x00461893..0x004618bf) keeps
// 5/6/5 bits (the middle channel via >>2); RGB555 (0x004618c3..0x004618ef) keeps 5/5/5 bits (middle via >>3).
// Only the low 16 bits survive (`and esi,0xffff` 0x00461965; the word store 0x0046197b), so the high-bit
// 0xfff00000 / 0xffe00000 ors the code inserts before the shifts are discarded and are not reproduced here.
unsigned pack16(int mode565, unsigned x, unsigned y, unsigned z) {
    if (mode565)
        return (((((x & 0xff) >> 3) << 6) | ((y & 0xff) >> 2)) << 5 | ((z & 0xff) >> 3)) & 0xffff;
    return (((((x & 0xff) >> 3) << 5) | ((y & 0xff) >> 3)) << 5 | ((z & 0xff) >> 3)) & 0xffff;
}

// Blocks 1-4 (word stores at 0x0046197b, 0x00461b08, 0x00461c99, 0x00461e2a; base offsets 0, 0x10000, 0x20000,
// 0x30000 bytes). Coarse R, G, B each run 0, 8, .., 0xf8 (`cmp di,0x100` / `jb`, e.g. 0x00461976 / 0x0046197f).
// The table index is pack16 of the coarse triple. The stored value is pack16 of the "fine" triple, where each
// fine byte is ((coarse/8) * step >> 8) & 0xff: the fine accumulators start at 0 and add `step` per coarse+8
// (add ebp,step at 0x00461970 / 0x00461afd / 0x00461c8e / 0x00461e1f) and are read as `>>8` (sar .,8 then low
// byte, e.g. 0x00461851 / 0x00461857). step is 0x400, 0x500, 0x600, 0x700 for blocks 1-4.
void fillBlock(int mode565, unsigned short* base, unsigned step) {
    for (unsigned r = 0; r < 0x100; r += 8) {
        unsigned rf = ((r / 8) * step >> 8) & 0xff;
        for (unsigned g = 0; g < 0x100; g += 8) {
            unsigned gf = ((g / 8) * step >> 8) & 0xff;
            for (unsigned b = 0; b < 0x100; b += 8) {
                unsigned bf = ((b / 8) * step >> 8) & 0xff;
                base[pack16(mode565, r, g, b)] = static_cast<unsigned short>(pack16(mode565, rf, gf, bf));
            }
        }
    }
}

// Block 5 (word store 0x00461fc3; base offset 0x40000 bytes). Same coarse index, but the stored value is pack16
// of the brightened coarse triple brightenComponent(r/g/b). The three pushes at 0x00461f1c / 0x00461f2a /
// 0x00461f38 brighten B, G, R and store them into the s1c / s20 / s24 slots (0x00461f26 / 0x00461f34 /
// 0x00461f44), which the value packing reads as (R, G, B).
void fillBlock5(int mode565, unsigned short* base) {
    for (unsigned r = 0; r < 0x100; r += 8) {
        unsigned rb = static_cast<unsigned>(kBrighten(static_cast<int>(r)));
        for (unsigned g = 0; g < 0x100; g += 8) {
            unsigned gb = static_cast<unsigned>(kBrighten(static_cast<int>(g)));
            for (unsigned b = 0; b < 0x100; b += 8) {
                unsigned bb = static_cast<unsigned>(kBrighten(static_cast<int>(b)));
                base[pack16(mode565, r, g, b)] = static_cast<unsigned short>(pack16(mode565, rb, gb, bb));
            }
        }
    }
}

// 0x00461830  build16BitColorTable(): builds the five 16-bit RGB blend tables through the pointer DAT_00824148
// (loaded at 0x00461953, 0x00461ae0, 0x00461c71, 0x00461e02, 0x00461f9e) and returns 0 (xor eax,eax /
// 0x0046200d). Each pixel's layout (565 vs 555) comes from the display cap. The five blocks sit at byte offsets
// 0, 0x10000, 0x20000, 0x30000, 0x40000 from the base and use brightness steps 0x400/0x500/0x600/0x700 and a
// brightenComponent pass.
typedef int(__cdecl* Build16_t)(void);
Build16_t Build16_orig;
int __cdecl Build16_re(void) {
    unsigned char* tbl = at<unsigned char*>(0x00824148);     // mov ecx,[0x824148]
    int mode = displayMode565();
    fillBlock(mode, reinterpret_cast<unsigned short*>(tbl + 0x00000), 0x400);
    fillBlock(mode, reinterpret_cast<unsigned short*>(tbl + 0x10000), 0x500);
    fillBlock(mode, reinterpret_cast<unsigned short*>(tbl + 0x20000), 0x600);
    fillBlock(mode, reinterpret_cast<unsigned short*>(tbl + 0x30000), 0x700);
    fillBlock5(mode, reinterpret_cast<unsigned short*>(tbl + 0x40000));
    return 0;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00461830, build16BitColorTable, Build16_re, Build16_orig);
