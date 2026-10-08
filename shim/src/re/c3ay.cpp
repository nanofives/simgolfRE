// Batch c3ay: six 16-bit drawing primitives of jgld.dll's Surface class plus the 8-bit keyed copy
// blit8from8 (SG_HOOK addresses are RVAs; the module's
// VAs are 0x10000000 + RVA, and every instruction cited below is a VA, as the debug build's disassembly prints it).
// jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// These are the 16-bit twins of the 8-bit primitives of batch c3av (shim/src/re/c3av.cpp). What is new here, and
// what c3av's notes recorded as the reason it left them alone, is the colour block every one of them starts with:
// a colour whose bit 31 is clear is an 8-bit palette index that has to be looked up in a Palette object before the
// 16-bit store. The fixture (re/frida/js/fixtures.d/c3ay.js) therefore builds a second object on jgld's own
// Palette vtable (0x1011db10) in addition to the Surface objects, and nothing live is handed to either arm.
//
// Surface vtable slots used here (0x1011d0b0; the 8-bit ones are documented in c3av.cpp):
//   +0x1c  0x1000a870: the pixel address, through a forwarder that calls slot +0x0c (0x1000a89f) exactly as the
//          +0x14 forwarder does. slot +0x0c (0x10008830) returns NULL when x >= width() (slot +0xd8, 0x10008857)
//          or y >= height() (slot +0xdc, 0x10008873) or when the base of slot +0x10 is NULL (0x100088a3), and for
//          depth 16 (field +0x24) returns base + x * 2 + y * [this+0x40] * 2 (0x100088ee..0x10008901).
//   +0x20  0x1000a7b0: the surface base, through a forwarder that calls slot +0x10 (0x1000a7d7).
//   +0xe4  0x1000ab30: returns this+0x24 (0x1000ab50), so element [1] of it is the format code at this+0x28.
//   +0xcc / +0xd4 / +0xe0 / +0x24 / +0x44: clip rect, bounds rect, pitch, release, rectangle-fill dispatch, all
//          as in c3av.cpp.
//
// Palette vtable slots (0x1011db10, written by Palette::ctor 0x1006a150):
//   +0x18  thunk 0x100010c8 -> 0x1006b000: returns this+0x40c (0x1006b020), the RGB565 lookup table.
//   +0x1c  thunk 0x10001af0 -> 0x1006b040: returns this+0x60c (0x1006b060), the RGB555 lookup table.
//   Both tables are the ones Palette::apply 0x1006a4a0 rebuilds from the 256 BGRA entries at +0xc.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address used below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// ------------------------------------------------------------------ the Surface and Palette virtuals

typedef RECT*(__fastcall* RectSlot_t)(void*, void*);
typedef void*(__fastcall* AddrSlot_t)(void*, void*, int, int);
typedef void*(__fastcall* BitsSlot_t)(void*, void*);
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
typedef int(__fastcall* PitchSlot_t)(void*, void*);
typedef int(__fastcall* DimSlot_t)(void*, void*);
typedef int*(__fastcall* FormatSlot_t)(void*, void*);
typedef void(__fastcall* FillRectSlot_t)(void*, void*, const RECT*, unsigned);
typedef unsigned short*(__fastcall* TableSlot_t)(void*, void*);

inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}
// +0xcc, the clip rectangle (this+0x44)
inline RECT* vClip(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xcc))(self, 0); }
// +0xd4, the bounds rectangle (this+0x54)
inline RECT* vBounds(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xd4))(self, 0); }
// +0x1c, the pixel address of (x, y)
inline void* vPixel1c(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x1c))(self, 0, x, y); }
// +0x14, the other forwarder onto the same slot +0x0c (0x1000a810, call at 0x1000a83f); blit8from8 uses this one
// on both of its surfaces.
inline void* vPixel14(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x14))(self, 0, x, y); }
// +0xd8 / +0xdc: bounds.right - bounds.left (0x1000aa93) and bounds.bottom - bounds.top (0x1000aad3)
inline int vWidth(void* self) { return reinterpret_cast<DimSlot_t>(slot(self, 0xd8))(self, 0); }
inline int vHeight(void* self) { return reinterpret_cast<DimSlot_t>(slot(self, 0xdc))(self, 0); }
// +0x20, the surface base address
inline void* vBits20(void* self) { return reinterpret_cast<BitsSlot_t>(slot(self, 0x20))(self, 0); }
// +0x24
inline void vUnlock(void* self, int flag) { reinterpret_cast<UnlockSlot_t>(slot(self, 0x24))(self, 0, flag); }
// +0xe0
inline int vPitch(void* self) { return reinterpret_cast<PitchSlot_t>(slot(self, 0xe0))(self, 0); }
// +0xe4, a pointer to this+0x24: [0] is the depth, [1] the format code
inline int* vFormat(void* self) { return reinterpret_cast<FormatSlot_t>(slot(self, 0xe4))(self, 0); }
// +0x44, the depth dispatcher of the rectangle fill
inline void vFillRect(void* self, const RECT* r, unsigned c) {
    reinterpret_cast<FillRectSlot_t>(slot(self, 0x44))(self, 0, r, c);
}
// Palette +0x18 / +0x1c
inline unsigned short* vTable565(void* pal) { return reinterpret_cast<TableSlot_t>(slot(pal, 0x18))(pal, 0); }
inline unsigned short* vTable555(void* pal) { return reinterpret_cast<TableSlot_t>(slot(pal, 0x1c))(pal, 0); }

// ------------------------------------------------------------------ the module's own helpers, called where the
// originals call them (through their module addresses, so the comparison is of this code and not of a copy).

typedef int(__cdecl* Intersect_t)(RECT*, const RECT*, const RECT*);   // 0x10008590 (IntersectRect)
typedef int(__cdecl* Equal_t)(const RECT*, const RECT*);              // 0x100085f0 (EqualRect)
typedef void(__cdecl* FillWords_t)(void*, unsigned short, int);       // 0x1000af30
typedef void(__fastcall* Clear16_t)(void*, void*, unsigned);          // 0x1000da70
typedef void(__cdecl* SetRect_t)(RECT*, int, int, int, int);          // 0x10008360 (x, y, w, h)

inline int jIntersect(RECT* d, const RECT* a, const RECT* b) {
    return reinterpret_cast<Intersect_t>(JgldBase() + 0x8590)(d, a, b);
}
inline void jSetRect(RECT* r, int x, int y, int w, int h) {
    reinterpret_cast<SetRect_t>(JgldBase() + 0x8360)(r, x, y, w, h);
}
inline int jEqual(const RECT* a, const RECT* b) {
    return reinterpret_cast<Equal_t>(JgldBase() + 0x85f0)(a, b);
}
inline void jFillWords(void* dst, unsigned short v, int n) {
    reinterpret_cast<FillWords_t>(JgldBase() + 0xaf30)(dst, v, n);
}
inline void jClear16(void* self, unsigned c) {
    reinterpret_cast<Clear16_t>(JgldBase() + 0xda70)(self, 0, c);
}

// The field at Surface+0x7c, read directly (not through a slot) as the Palette of this surface: clear16 tests it
// at 0x1000daa7, fillRect16 at 0x1000dd48, ditherRect16 at 0x1000e5be, hline16 at 0x1000bb17, vline16 at
// 0x1000be66 and dashedHLine16 at 0x1000d018.
inline void* surfacePalette(void* self) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x7c);
}

// The module global at VA 0x1012873c: when the surface carries no Palette of its own, the colour is looked up in
// the Palette of the object this global points at, taken from its field +4 (clear16 0x1000dabf / 0x1000dac7).
inline void* paletteClientFallback() {
    void* client = *reinterpret_cast<void**>(JgldBase() + 0x12873c);
    if (client == 0) return 0;
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(client) + 4);
}

// The row skip fillRect16 parks in the module global at VA 0x10128458 (written at 0x1000deaf). Nothing in
// fillRect16 reads it back: the row cursor is advanced from the slot-+0xe0 pitch instead (0x1000def3).
inline int& gSkip16() { return *reinterpret_cast<int*>(JgldBase() + 0x128458); }

// The pixel count the three-colour loop of dashedHLine16 parks in the module global at VA 0x10128450 so that it
// can use ebp as its counter (written at 0x1000d380, read back at 0x1000d38b).
inline int& gCount() { return *reinterpret_cast<int*>(JgldBase() + 0x128450); }

// blit8from8's three loop globals: the group count at 0x101284b8 (written at 0x10013a54 and again by each of
// the three `inc ecx` arms at 0x10013aa5 / 0x10013ab4 / 0x10013ac3, re-read once per row at 0x10013b15), the
// `this` row skip at 0x101284bc (0x10013a60, added to esi at 0x10013b1b) and the argument surface's row skip at
// 0x101284b0 (0x10013a6b, added to edi at 0x10013b21).
inline int& gGroups() { return *reinterpret_cast<int*>(JgldBase() + 0x1284b8); }
inline int& gSkipThis() { return *reinterpret_cast<int*>(JgldBase() + 0x1284bc); }
inline int& gSkipOther() { return *reinterpret_cast<int*>(JgldBase() + 0x1284b0); }

// ------------------------------------------------------------------ the colour block shared by five of the six
// Five of these functions open with the same block, byte for byte: a colour whose bit 31 is clear is an index into
// a Palette's 16-bit table. The Palette is the surface's own (+0x7c) or, when that is null, the one hanging off
// the module global 0x1012873c; when neither exists the colour is left alone. Which of the two tables is used is
// decided by the format code (slot +0xe4, element [1]): 0 selects the RGB565 table of Palette slot +0x18 and 1
// the RGB555 table of slot +0x1c. Any other format code aborts the whole call. The looked-up entry is a word,
// zero-extended into the colour (`xor edx, edx; mov dx, [eax + ecx*2]`, clear16 0x1000db1f).
// Returns 1 when the format code is neither 0 nor 1, which is each caller's abort path.
int mapColor16(void* self, unsigned& c) {
    if (c & 0x80000000) return 0;
    void* pal = 0;
    if (surfacePalette(self) != 0) {
        pal = surfacePalette(self);
    } else {
        pal = paletteClientFallback();
    }
    if (pal == 0) return 0;
    const int fmt = vFormat(self)[1];
    if (fmt == 0) {
        c = vTable565(pal)[c & 0xff];
    } else if (fmt == 1) {
        c = vTable555(pal)[c & 0xff];
    } else {
        return 1;
    }
    return 0;
}

// ----------------------------------------------------------------------------------------- 0x0000da70 clear16

// 0x0000da70  Surface::clear16(c) (VA 0x1000da70): fills a 16-bit surface with the colour c. The colour block
// above runs first (bit-31 test 0x1000da97, surface palette 0x1000daab, global fallback 0x1000dabf, format switch
// 0x1000daf8 / 0x1000dafe) and an unsupported format code returns with nothing drawn (0x1000db00 -> 0x1000db52).
// When the clip rectangle (slot +0xcc) and the bounds rectangle (slot +0xd4) are not equal, the two being handed
// to EqualRect 0x100085f0 in that order (0x1000db87) and the test being at 0x1000db91, the work is handed to the
// depth dispatcher of slot +0x44 with the clip rectangle and the colour (0x1000dbb9) and nothing else runs.
// Otherwise the colour is masked to 16 bits (0x1000dbcb), the base comes from slot +0x20 with no null test
// (0x1000dbdd), h is the bounds rectangle's bottom (0x1000dc01) and w its right (0x1000dc1e), and fillWords
// 0x1000af30 writes w * h words of the colour (`imul` 0x1000dc27, call 0x1000dc35). The surface is then released
// with slot +0x24 and the argument 1 (0x1000dc49).
typedef void(__fastcall* Clear16Fn_t)(void*, void*, unsigned);
Clear16Fn_t Clear16_orig;
void __fastcall Clear16_re(void* self, void*, unsigned c) {
    if (mapColor16(self, c) != 0) return;
    if (jEqual(vClip(self), vBounds(self)) == 0) {
        vFillRect(self, vClip(self), c);
        return;
    }
    c &= 0xffff;
    void* bits = vBits20(self);
    const int h = vBounds(self)->bottom;
    const int w = vBounds(self)->right;
    jFillWords(bits, static_cast<unsigned short>(c), w * h);
    vUnlock(self, 1);
}

// -------------------------------------------------------------------------------------- 0x0000dcf0 fillRect16

// 0x0000dcf0  Surface::fillRect16(rect, c) (VA 0x1000dcf0): fills a rectangle of a 16-bit surface. A null
// rectangle means the whole surface: 0x1000dd18 tests it and clear16 0x1000da70 is called through the thunk
// 0x10001a23 (0x1000dd21) before returning 0. The colour block then runs (bit-31 test 0x1000dd38, surface
// palette 0x1000dd4c, global fallback 0x1000dd60, pal-null 0x1000dd72, format switch 0x1000dd99 / 0x1000dd9f)
// and an unsupported format code leaves at 0x1000dda1 for the `mov eax, 0x18` at 0x1000ddf3, so it returns 24.
// The colour is masked to 16 bits (0x1000de00), the rectangle copied to a local (0x1000de0b..0x1000de1f) and
// intersected with the clip rectangle of slot +0xcc by IntersectRect 0x10008590 (0x1000de42); an empty
// intersection returns 0 (0x1000de4c). The top-left pixel address comes from slot +0x1c (0x1000de67) and a NULL
// one returns 0 (0x1000de78). h is the intersection's height (0x1000de84) and w its width (0x1000de90); the
// module global at 0x10128458 receives (pitch - w) * 2 with the pitch from slot +0xe0 (0x1000de9d, shifted and
// stored at 0x1000dead / 0x1000deaf). Each of the h rows is written by fillWords 0x1000af30 with w words
// (0x1000ded1) and the cursor is advanced by the slot-+0xe0 pitch doubled (0x1000dee3..0x1000def6), the row
// counter being the post-decrement `h--` test at 0x1000deb7..0x1000dec2. Slot +0x24 (1) closes (0x1000df07) and
// the function returns 0 (0x1000df11).
typedef int(__fastcall* FillRect16_t)(void*, void*, const RECT*, unsigned);
FillRect16_t FillRect16_orig;
int __fastcall FillRect16_re(void* self, void*, const RECT* rp, unsigned c) {
    if (rp == 0) {
        jClear16(self, c);
        return 0;
    }
    if (mapColor16(self, c) != 0) return 24;
    c &= 0xffff;
    RECT rc = *rp;
    if (!jIntersect(&rc, &rc, vClip(self))) return 0;
    char* dst = reinterpret_cast<char*>(vPixel1c(self, rc.left, rc.top));
    if (dst == 0) return 0;
    int h = rc.bottom - rc.top;
    const int w = rc.right - rc.left;
    gSkip16() = (vPitch(self) - w) * 2;
    while (h--) {
        jFillWords(dst, static_cast<unsigned short>(c), w);
        dst = dst + vPitch(self) * 2;
    }
    vUnlock(self, 1);
    return 0;
}

// ------------------------------------------------------------------------------------ 0x0000e580 ditherRect16

// 0x0000e580  Surface::ditherRect16(rect, c) (VA 0x1000e580): writes the colour c over every second pixel of a
// rectangle of a 16-bit surface, the parity alternating per row. The colour block runs first (bit-31 test
// 0x1000e5ae, surface palette 0x1000e5c2, global fallback 0x1000e5d6, pal-null 0x1000e5e8, format switch
// 0x1000e60f / 0x1000e615) and an unsupported format code leaves at 0x1000e617 for the `err = 0x18` at
// 0x1000e669, which makes the function skip everything else (tested at 0x1000e674). The colour is masked
// to 16 bits
// (0x1000e67d); a null rectangle is replaced by the bounds rectangle of slot +0xd4 (0x1000e689 jne, call at
// 0x1000e695) and otherwise the argument is copied to a local (0x1000e6c6..0x1000e6d6). The local is intersected
// with the clip rectangle of slot +0xcc by IntersectRect 0x10008590 (0x1000e6f5, tested at 0x1000e6ff) and the
// top-left pixel address comes from slot +0x1c (0x1000e717), a NULL one ending the call (0x1000e728). h is the
// intersection's height (0x1000e734) and w its width (0x1000e73d). The row loop is the post-decrement `h--` at
// 0x1000e740..0x1000e74e: a row whose remaining count is odd (the signed `h % 2` idiom at 0x1000e75d..0x1000e769,
// tested at 0x1000e76c) starts one pixel in (0x1000e771), remembers that in a flag (0x1000e777) and counts one
// pixel less (0x1000e781); the inner loop then writes words two pixels apart while the counter is above zero
// (0x1000e79b jle, store 0x1000e7a8, `add edx, 4` 0x1000e7ae, `sub edx, 2` 0x1000e795). The cursor moves to the
// next row by the slot-+0xe0 pitch less w less that flag, doubled (0x1000e7c0..0x1000e7d9). Slot +0x24 (1) closes
// (0x1000e7ed) and the function returns 0 on every path that reached the loop.
typedef int(__fastcall* DitherRect16_t)(void*, void*, const RECT*, unsigned);
DitherRect16_t DitherRect16_orig;
int __fastcall DitherRect16_re(void* self, void*, const RECT* rp, unsigned c) {
    int err = 0;
    if (mapColor16(self, c) != 0) err = 24;
    if (err == 0) {
        c &= 0xffff;
        RECT rc;
        if (rp == 0) {
            rc = *vBounds(self);
        } else {
            rc = *rp;
        }
        if (jIntersect(&rc, &rc, vClip(self))) {
            unsigned short* p = reinterpret_cast<unsigned short*>(vPixel1c(self, rc.left, rc.top));
            if (p != 0) {
                int h = rc.bottom - rc.top;
                const int w = rc.right - rc.left;
                while (h--) {
                    int n = w;
                    int odd;
                    if (h % 2) {
                        p++;
                        odd = 1;
                        n--;
                    } else {
                        odd = 0;
                    }
                    for (; n > 0; n -= 2) {
                        *p = static_cast<unsigned short>(c);
                        p += 2;
                    }
                    p += vPitch(self) - w - odd;
                }
                vUnlock(self, 1);
            }
        }
    }
    return err;
}

// ----------------------------------------------------------------------------------------- 0x0000ba90 hline16

// 0x0000ba90  Surface::hline16(x1, x2, y, c) (VA 0x1000ba90): one horizontal run of the colour c across row y of
// a 16-bit surface, clipped to the clip rectangle (slot +0xcc, re-fetched before every test as the original does).
// It returns without drawing when y is outside [top, bottom) (0x1000baca jl, 0x1000bae9 jl) or when x1 == x2
// (0x1000baf6 jne), both of which are tested BEFORE the colour block, which is the one ordering difference from
// clear16. The colour block then runs (bit-31 test 0x1000bb07, surface palette 0x1000bb1b, global fallback
// 0x1000bb2f, format switch 0x1000bb68 / 0x1000bb6e) and an unsupported format code returns (0x1000bbc2). The
// colour is masked to 16 bits (0x1000bbca) and x1 > x2 is normalised by the three-xor swap at 0x1000bbda
// (guarded by the `jle` at 0x1000bbd8). The run is dropped when it lies entirely right of the rectangle
// (0x1000bc12 jge) or entirely left of it (0x1000bc30 jge); x1 is raised to left (0x1000bc53 jge) and x2 lowered
// to right - 1 (0x1000bc8e jl). The pixel address comes from slot +0x1c (0x1000bcc2) and a NULL one returns
// (0x1000bcd3). The loop counts x2 - x1 + 1 down to zero (0x1000bcf2 jle), storing a word and stepping two bytes
// each time (0x1000bcfb, 0x1000bd01), and slot +0x24 (1) closes (0x1000bd15).
typedef void(__fastcall* Hline16_t)(void*, void*, int, int, int, unsigned);
Hline16_t Hline16_orig;
void __fastcall Hline16_re(void* self, void*, int x1, int x2, int y, unsigned c) {
    if (y < vClip(self)->top) return;
    if (y >= vClip(self)->bottom) return;
    if (x1 == x2) return;
    if (mapColor16(self, c) != 0) return;
    c &= 0xffff;
    if (x1 > x2) {
        x1 ^= x2;
        x2 ^= x1;
        x1 ^= x2;
    }
    if (x1 >= vClip(self)->right) return;
    if (x2 < vClip(self)->left) return;
    if (x1 < vClip(self)->left) x1 = vClip(self)->left;
    if (x2 >= vClip(self)->right) x2 = vClip(self)->right - 1;
    unsigned short* p = reinterpret_cast<unsigned short*>(vPixel1c(self, x1, y));
    if (p == 0) return;
    for (int n = x2 - x1 + 1; n > 0; n--) {
        *p = static_cast<unsigned short>(c);
        p++;
    }
    vUnlock(self, 1);
}

// ----------------------------------------------------------------------------------------- 0x0000bde0 vline16

// 0x0000bde0  Surface::vline16(x, y1, y2, c) (VA 0x1000bde0): the vertical twin of hline16. It returns when x is
// outside [left, right) (0x1000be19 jl, 0x1000be38 jl) or when y1 == y2 (0x1000be45 jne), then runs the colour
// block (bit-31 test 0x1000be56, surface palette 0x1000be6a, global fallback 0x1000be7e, pal-null 0x1000be90,
// format switch 0x1000beb7 / 0x1000bebd) whose unsupported arm returns (0x1000bf11). The colour is masked to 16
// bits (0x1000bf19), y1 > y2 swapped by three xors at 0x1000bf29 (guard `jle` 0x1000bf27), the run dropped when
// it is below the rectangle (0x1000bf61 jge) or above it (0x1000bf80 jge), y1 raised to top (0x1000bfa4 jge) and
// y2 lowered to bottom - 1 (0x1000bfe0 jl). The pixel address comes from slot +0x1c (0x1000c014) and a NULL one
// returns (0x1000c025). The pitch of slot +0xe0 is read once (0x1000c033) and the run is an inline `loop` over
// y2 - y1 + 1 words (0x1000c057..0x1000c05c) stepping that pitch doubled (`shl ebx, 1` at 0x1000c052). Slot
// +0x24 (1) closes (0x1000c06b).
typedef void(__fastcall* Vline16_t)(void*, void*, int, int, int, unsigned);
Vline16_t Vline16_orig;
void __fastcall Vline16_re(void* self, void*, int x, int y1, int y2, unsigned c) {
    if (x < vClip(self)->left) return;
    if (x >= vClip(self)->right) return;
    if (y1 == y2) return;
    if (mapColor16(self, c) != 0) return;
    c &= 0xffff;
    if (y1 > y2) {
        y1 ^= y2;
        y2 ^= y1;
        y1 ^= y2;
    }
    if (y1 >= vClip(self)->bottom) return;
    if (y2 < vClip(self)->top) return;
    if (y1 < vClip(self)->top) y1 = vClip(self)->top;
    if (y2 >= vClip(self)->bottom) y2 = vClip(self)->bottom - 1;
    char* p = reinterpret_cast<char*>(vPixel1c(self, x, y1));
    if (p == 0) return;
    const int stride = vPitch(self);
    const int step = stride * 2;
    const unsigned short v = static_cast<unsigned short>(c);
    for (int n = y2 - y1 + 1; n != 0; n--) {
        *reinterpret_cast<unsigned short*>(p) = v;
        p += step;
    }
    vUnlock(self, 1);
}

// ----------------------------------------------------------------------------------- 0x0000cf90 dashedHLine16

// 0x0000cf90  Surface::dashedHLine16(x1, x2, y, c1, c2, len1, len2, phase) (VA 0x1000cf90): a horizontal dashed
// run on a 16-bit surface, alternating len1 pixels of c1 with len2 pixels of c2 from the given phase.
//
// It returns without drawing when y is outside the clip rectangle's [top, bottom) (0x1000cfca jl, 0x1000cfe9 jl),
// when x1 == x2 (0x1000cff6 jne) or when BOTH colours are -1 (0x1000d001 jne over the first `cmp ..., -1` and
// 0x1000d007 jne over the second). The colour block then runs TWICE, once per colour: the Palette is picked once
// (surface +0x7c at 0x1000d01c, global 0x1012873c at 0x1000d030, the null test at 0x1000d041) and the format code
// of slot +0xe4 selects the whole block (0x1000d068 for 0, 0x1000d06e for 1, anything else leaving at 0x1000d070
// -> 0x1000d142), and inside each arm c1 is looked up only when its bit 31 is clear (0x1000d080) and c2 only when
// its bit 31 is clear (0x1000d0b2); in the second arm the same two tests are at 0x1000d0e6 and 0x1000d118. The
// two arms are the same code: both look c1 up through Palette slot +0x18 (0x1000d08c, 0x1000d0f2) and c2 through
// slot +0x1c (0x1000d0be, 0x1000d124), so c1 always comes from the RGB565 table and c2 always from the RGB555
// one, whichever of the two format codes the surface carries.
//
// Both colours are then masked to 16 bits (0x1000d14a, 0x1000d155). x1 > x2 swaps the two ends, the two run
// lengths and the two colours and replaces the phase with len1 + len2 - phase (0x1000d164 jle guards
// 0x1000d166..0x1000d1c0). The run is dropped when it lies entirely right of the clip rectangle (0x1000d1e0 jge)
// or entirely left of it (0x1000d1fe jge); x1 is raised to left (0x1000d221 jge) and x2 lowered to right - 1
// (0x1000d25c jl). The pixel address comes from slot +0x1c (0x1000d290) and a NULL one returns (0x1000d2a1).
//
// The phase is reduced modulo len1 + len2 by the signed `idiv` at 0x1000d2b2 and turned into a signed cursor: a
// phase at or past len1 becomes len2 - (phase - len1), a phase below len1 becomes -(len1 - phase) (0x1000d2bd jl
// and the two arms at 0x1000d2bf and 0x1000d2d3); len1 is then negated (0x1000d2e7). The cursor is non-negative
// inside a c2 run and negative inside a c1 run; each pixel moves it one step toward zero and, on reaching zero,
// reloads it with the negated len1 or with len2.
//
// Three loop bodies follow, selected by `cmp [c1], -1` at 0x1000d2ec and `cmp [c2], -1` at 0x1000d32b. Both of
// those compare the colours AFTER the unconditional 16-bit masking at 0x1000d14a and 0x1000d155, so each of them
// holds a value in [0, 0xffff] and neither compare can ever be equal: the two single-colour loops at 0x1000d2f2
// and 0x1000d331 are unreachable, and every call that gets this far runs the third loop at 0x1000d367. That loop
// parks its pixel count in the module global at 0x10128450 (0x1000d380) so it can use ebp as the counter
// (0x1000d38b) and writes c2 on the non-negative side (0x1000d395) and c1 on the negative side (0x1000d39f),
// stepping two bytes each pixel (0x1000d3a7). Slot +0x24 (1) closes (0x1000d3bc).
typedef void(__fastcall* DashedH16_t)(void*, void*, int, int, int, int, int, int, int, int);
DashedH16_t DashedH16_orig;
void __fastcall DashedH16_re(void* self, void*, int x1, int x2, int y, int c1, int c2, int len1, int len2,
                             int phase) {
    if (y < vClip(self)->top) return;
    if (y >= vClip(self)->bottom) return;
    if (x1 == x2) return;
    if (c1 == -1 && c2 == -1) return;

    void* pal = 0;
    if (surfacePalette(self) != 0) {
        pal = surfacePalette(self);
    } else {
        pal = paletteClientFallback();
    }
    if (pal != 0) {
        const int fmt = vFormat(self)[1];
        if (fmt != 0 && fmt != 1) return;
        // Both arms of the switch hold the SAME two calls: c1 through Palette slot +0x18 and c2 through slot
        // +0x1c, so the format code decides only whether the lookup runs at all, never which table is used.
        // Proven by the bytes: 0x1000d075..0x1000d0d9 (the format-0 arm) and 0x1000d0dc..0x1000d13f (the
        // format-1 arm) differ only in the encoding of their first `mov` and in the relative displacements of
        // their `__chkesp` calls; both contain `ff 52 18` for c1 (0x1000d08c, 0x1000d0f2) and `ff 52 1c` for c2
        // (0x1000d0be, 0x1000d124). A surface with format code 1 therefore takes c1 from the RGB565 table and
        // c2 from the RGB555 one.
        if (!(c1 & 0x80000000)) c1 = vTable565(pal)[c1 & 0xff];
        if (!(c2 & 0x80000000)) c2 = vTable555(pal)[c2 & 0xff];
    }
    c1 &= 0xffff;
    c2 &= 0xffff;

    if (x1 > x2) {
        x1 ^= x2; x2 ^= x1; x1 ^= x2;
        len1 ^= len2; len2 ^= len1; len1 ^= len2;
        c1 ^= c2; c2 ^= c1; c1 ^= c2;
        phase = len1 + len2 - phase;
    }
    if (x1 >= vClip(self)->right) return;
    if (x2 < vClip(self)->left) return;
    if (x1 < vClip(self)->left) x1 = vClip(self)->left;
    if (x2 >= vClip(self)->right) x2 = vClip(self)->right - 1;
    unsigned short* p = reinterpret_cast<unsigned short*>(vPixel1c(self, x1, y));
    if (p == 0) return;

    phase = phase % (len1 + len2);
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;

    // The two single-colour loops are unreachable (see the comment above), so only the third one is written here.
    gCount() = x2 - x1 + 1;
    int n = gCount();
    int d = phase;
    const unsigned short a = static_cast<unsigned short>(c1);
    const unsigned short b = static_cast<unsigned short>(c2);
    do {
        if (d >= 0) {
            *p = b;
            if (--d == 0) d = len1;
        } else {
            *p = a;
            if (++d == 0) d = len2;
        }
        p++;
    } while (--n != 0);
    vUnlock(self, 1);
}

// ------------------------------------------------------------------------------------- 0x00013800 blit8from8

// 0x00013800  Surface::blit8from8(other, x, y, sx, sy, w, h, key) (VA 0x10013800): an 8-bit keyed copy between
// two surfaces. A null first argument returns 3 at once (0x10013821). The destination point is pulled into the
// first quadrant: a negative x adds itself to w, is subtracted from sx and becomes 0 (0x10013831 guards
// 0x10013833..0x10013845), and a negative y does the same to h and sy (0x10013850 guards
// 0x10013852..0x10013864). An x past `this`'s width (slot +0xd8) or a y past its height (slot +0xdc) returns 0
// (0x10013885 jle and 0x100138a8 jle, so x == width is still accepted); w is then cut to width - x when x + w
// overflows the width (0x100138d0 jle skips the cut at 0x100138e9) and h to height - y likewise (0x1001390e,
// 0x10013927). The source rectangle (sx, sy, w, h) is built by rectFromXYWH 0x10008360 (0x10013941) and
// intersected with the CLIP RECTANGLE OF THE ARGUMENT surface, slot +0xcc on it (0x10013953), by IntersectRect
// 0x10008590 (0x10013969); an empty intersection returns 0 (0x10013973). Whatever the intersection trimmed off
// the left and the top is added to x and y (0x1001397c..0x10013995) and w and h are replaced by its width and
// height (0x10013998..0x100139a7).
//
// The two pixel addresses both come from slot +0x14: the argument's at the intersection's top-left corner
// (0x100139bc) and `this`'s at (x, y) (0x100139eb); either one NULL returns 3 (0x100139cd, 0x100139fc). The
// pitches come from slot +0xe0, the argument's at 0x10013a12 and `this`'s at 0x10013a2c. rows and cols are the
// intersection's height and width (0x10013a3c, 0x10013a45) and three module globals are loaded: 0x101284b8 with
// cols >> 2 (0x10013a54), 0x101284bc with thisPitch - cols (0x10013a60) and 0x101284b0 with otherPitch - cols
// (0x10013a6b).
//
// The copy itself is an inline four-way unrolled loop entered through a stored label (`jmp edx` at 0x10013acb),
// the label and a possible extra group being chosen once from cols & 3 (0x10013a88..0x10013ac9): with cols & 3
// of 1, 2 or 3 the group count is incremented and the first group of each row is that short. Inside, esi holds
// `this`'s pixels (0x10013a73) and edi the argument's (0x10013a76), and each step compares the byte at EDI with
// the key (0x10013acd, 0x10013afa, 0x10013b00, 0x10013b06, 0x10013b0c) and, when they are equal, copies the byte
// at ESI over it (0x10013ad1, 0x10013adb, 0x10013ae5, 0x10013aef). So the byte that is read is `this`'s and the
// byte that is written is the ARGUMENT surface's, at the positions where the argument surface already holds the
// key: the direction is the opposite of the two pointers' names in the signature, and it is what both arms of
// the A/B are compared on. Each row ends by adding the two globals to the two cursors (0x10013b1b, 0x10013b21)
// and the row counter is the `dec ebx` at 0x10013b27. Both surfaces are released with slot +0x24 and 1,
// `this` first (0x10013b38) and the argument second (0x10013b4e), and the function returns 0 (0x10013b58).
typedef int(__fastcall* Blit8from8_t)(void*, void*, void*, int, int, int, int, int, int, int);
Blit8from8_t Blit8from8_orig;
int __fastcall Blit8from8_re(void* self, void*, void* other, int x, int y, int sx, int sy, int w, int h,
                             int key) {
    if (other == 0) return 3;
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
    if (x > vWidth(self)) return 0;
    if (y > vHeight(self)) return 0;
    if (x + w > vWidth(self)) w = vWidth(self) - x;
    if (y + h > vHeight(self)) h = vHeight(self) - y;
    RECT rc;
    jSetRect(&rc, sx, sy, w, h);
    if (!jIntersect(&rc, &rc, vClip(other))) return 0;
    x += rc.left - sx;
    y += rc.top - sy;
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    unsigned char* edi = reinterpret_cast<unsigned char*>(vPixel14(other, rc.left, rc.top));
    if (edi == 0) return 3;
    unsigned char* esi = reinterpret_cast<unsigned char*>(vPixel14(self, x, y));
    if (esi == 0) return 3;
    const int otherPitch = vPitch(other);
    const int thisPitch = vPitch(self);
    const int rows = rc.bottom - rc.top;
    const int cols = rc.right - rc.left;
    gGroups() = cols >> 2;
    gSkipThis() = thisPitch - cols;
    gSkipOther() = otherPitch - cols;
    const int first = cols & 3;                  // 0 means a full first group of four
    if (first != 0) gGroups() = gGroups() + 1;   // the three `inc ecx` arms
    const unsigned char k = static_cast<unsigned char>(key);
    int r = rows;
    do {
        int groups = gGroups();                  // re-read per row at 0x10013b15
        int n = (first != 0) ? first : 4;
        do {
            for (int i = 0; i < n; i++) {
                if (*edi == k) *edi = *esi;
                esi++;
                edi++;
            }
            n = 4;
        } while (--groups != 0);
        esi += gSkipThis();
        edi += gSkipOther();
    } while (--r != 0);
    vUnlock(self, 1);
    vUnlock(other, 1);
    return 0;
}

}  // namespace

SG_HOOK("jgld.dll", 0x00013800, Surface_blit8from8, Blit8from8_re, Blit8from8_orig);
SG_HOOK("jgld.dll", 0x0000da70, Surface_clear16, Clear16_re, Clear16_orig);
SG_HOOK("jgld.dll", 0x0000dcf0, Surface_fillRect16, FillRect16_re, FillRect16_orig);
SG_HOOK("jgld.dll", 0x0000e580, Surface_ditherRect16, DitherRect16_re, DitherRect16_orig);
SG_HOOK("jgld.dll", 0x0000ba90, Surface_hline16, Hline16_re, Hline16_orig);
SG_HOOK("jgld.dll", 0x0000bde0, Surface_vline16, Vline16_re, Vline16_orig);
SG_HOOK("jgld.dll", 0x0000cf90, Surface_dashedHLine16, DashedH16_re, DashedH16_orig);
