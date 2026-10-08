// Batch c3av: nine 8-bit drawing primitives of jgld.dll's Surface class (SG_HOOK addresses are RVAs; the module's
// VAs are 0x10000000 + RVA, and every instruction cited below is a VA, as the debug build's disassembly prints it).
// jgld.dll is LoadLibrary'd after the shim starts, so these hooks install from the LoadLibraryA hook
// (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by (module, rva) through SimGolfShim_FindHookM.
// __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// All nine are integer-only: not one body contains an x87 instruction. Each works through the Surface vtable
// (0x1011d0b0, mapped in re/analysis/systems/jgld_2.md), so the reimplementations call the same virtual slots at the
// same points, through the object's own vtable pointer; the A/B drives both arms over synthetic surfaces built on
// that real vtable (re/frida/js/fixtures.d/c3av.js), never the live display or a screen surface.
//
// Vtable slots used here (offset -> what the slot does on a Surface; verified from the bodies at the listed VAs):
//   +0x0c  pixelAddr(x, y)     0x10008830: NULL when x >= width() or y >= height(), else the slot-+0x10 base scaled
//                              by the depth at this+0x24 (8 -> 1 byte, 16 -> 2, 24 -> 3, 32 -> 4) with row stride
//                              [this+0x40]; any other depth returns NULL (0x100088c2 ja).
//   +0x14  the same address through a one-line forwarder 0x1000a810 (it calls slot +0x0c, 0x1000a83f).
//   +0x18  forwarder 0x1000a750 -> slot +0x10 (0x1000a777).
//   +0x10  0x10008730: copies [this+0x4c0] to [this+0x4cc], bumps [this+0x4c8] when it is non-zero, returns it.
//   +0x24  0x100087b0: subtracts its argument from [this+0x4c8] and, at <= 0, clears [this+0x4cc] and [this+0x4c8].
//   +0xcc  0x1000a990: returns this+0x44 (0x1000a9b0, the clip rectangle).
//   +0xd4  0x1000aa30: returns this+0x54 (0x1000aa50, the bounds rectangle).
//   +0xe0  0x1000aaf0: returns [this+0x40] (0x1000ab10, the row stride in pixels).
//   +0x44  0x10009770: switches on the depth at [this+0x24] and forwards to the 8-bit rectangle fill 0x10011dd0
//          (0x100097c2) or the 16-bit one 0x1000dcf0 (0x100097d4).
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address used below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// ------------------------------------------------------------------ the Surface virtuals these bodies call

typedef RECT*(__fastcall* RectSlot_t)(void*, void*);
typedef void*(__fastcall* AddrSlot_t)(void*, void*, int, int);
typedef void*(__fastcall* BitsSlot_t)(void*, void*);
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
typedef int(__fastcall* PitchSlot_t)(void*, void*);
typedef void(__fastcall* FillRectSlot_t)(void*, void*, const RECT*, int);

inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}
// +0xcc, the clip rectangle (this+0x44)
inline RECT* vClip(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xcc))(self, 0); }
// +0xd4, the bounds rectangle (this+0x54)
inline RECT* vBounds(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xd4))(self, 0); }
// +0x14, the pixel address through the forwarder
inline void* vPixel14(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x14))(self, 0, x, y); }
// +0x0c, the pixel address directly
inline void* vPixel0c(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x0c))(self, 0, x, y); }
// +0x18, the surface base address
inline void* vBits(void* self) { return reinterpret_cast<BitsSlot_t>(slot(self, 0x18))(self, 0); }
// +0x24
inline void vUnlock(void* self, int flag) { reinterpret_cast<UnlockSlot_t>(slot(self, 0x24))(self, 0, flag); }
// +0xe0
inline int vPitch(void* self) { return reinterpret_cast<PitchSlot_t>(slot(self, 0xe0))(self, 0); }
// +0x44
inline void vFillRect(void* self, const RECT* r, int c) {
    reinterpret_cast<FillRectSlot_t>(slot(self, 0x44))(self, 0, r, c);
}

// ------------------------------------------------------------------ the module's own helpers, called where the
// originals call them (through their module addresses, so the comparison is of this code and not of a copy).

typedef int(__cdecl* Intersect_t)(RECT*, const RECT*, const RECT*);        // 0x10008590 (IntersectRect)
typedef int(__cdecl* Equal_t)(const RECT*, const RECT*);                   // 0x100085f0 (EqualRect)
typedef void(__cdecl* SetRect_t)(RECT*, int, int, int, int);               // 0x10008360 (x, y, w, h)
typedef void(__cdecl* SetRectLTRB_t)(RECT*, int, int, int, int);           // 0x10010390 (l, t, r, b)
typedef int(__cdecl* RectDim_t)(const RECT*);                              // 0x10009120 / 0x10009160
typedef void*(__cdecl* Memset_t)(void*, int, unsigned);                    // 0x1007e7c0 (CRT memset)
typedef void(__fastcall* Clear8_t)(void*, void*, int);                     // 0x10011c40

inline int jIntersect(RECT* d, const RECT* a, const RECT* b) {
    return reinterpret_cast<Intersect_t>(JgldBase() + 0x8590)(d, a, b);
}
inline int jEqual(const RECT* a, const RECT* b) {
    return reinterpret_cast<Equal_t>(JgldBase() + 0x85f0)(a, b);
}
inline void jSetRect(RECT* r, int x, int y, int w, int h) {
    reinterpret_cast<SetRect_t>(JgldBase() + 0x8360)(r, x, y, w, h);
}
inline void jSetRectLTRB(RECT* r, int l, int t, int rr, int b) {
    reinterpret_cast<SetRectLTRB_t>(JgldBase() + 0x10390)(r, l, t, rr, b);
}
inline int jRectWidth(const RECT* r) { return reinterpret_cast<RectDim_t>(JgldBase() + 0x9120)(r); }
inline int jRectHeight(const RECT* r) { return reinterpret_cast<RectDim_t>(JgldBase() + 0x9160)(r); }
inline void jMemset(void* p, int c, unsigned n) { reinterpret_cast<Memset_t>(JgldBase() + 0x7e7c0)(p, c, n); }
inline void jClear8(void* self, int c) { reinterpret_cast<Clear8_t>(JgldBase() + 0x11c40)(self, 0, c); }

// The row skip the 8-bit rectangle fill parks in a module global between setting it up and the fill loop.
inline int& gSkip() { return *reinterpret_cast<int*>(JgldBase() + 0x12847c); }

// Swap written the way the original writes it (three xors, 0x10010465..0x1001047d).
inline void xorSwap(int& a, int& b) { a ^= b; b ^= a; a ^= b; }

// ------------------------------------------------------------------------------------------- 0x000103f0 hline

// 0x000103f0  Surface::hline(x1, x2, y, c) (VA 0x100103f0): one horizontal run of the byte c across row y of an
// 8-bit surface, clipped to the clip rectangle (slot +0xcc, re-fetched before every test as the original does).
// Returns without drawing when y is outside [top, bottom) (0x1001042a jl, 0x10010449 jl), when x1 == x2
// (0x10010456 jne), when the span lies entirely right of the rectangle (0x1001049d jge) or entirely left of it
// (0x100104bb jge), or when the pixel address is NULL (0x1001055e jne). x1 > x2 is normalised by the three-xor
// swap at 0x10010463. x1 is raised to left (0x100104de jge) and x2 lowered to right - 1 (0x10010519 jl). The run is
// written by the CRT memset 0x1007e7c0 (0x10010574) over x2 - x1 + 1 bytes, and the surface is released with
// slot +0x24 and the argument 1 (0x1001057e).
typedef void(__fastcall* Hline_t)(void*, void*, int, int, int, int);
Hline_t Hline_orig;
void __fastcall Hline_re(void* self, void*, int x1, int x2, int y, int c) {
    if (y < vClip(self)->top) return;
    if (y >= vClip(self)->bottom) return;
    if (x1 == x2) return;
    if (x1 > x2) xorSwap(x1, x2);
    if (x1 >= vClip(self)->right) return;
    if (x2 < vClip(self)->left) return;
    if (x1 < vClip(self)->left) x1 = vClip(self)->left;
    if (x2 >= vClip(self)->right) x2 = vClip(self)->right - 1;
    void* p = vPixel14(self, x1, y);
    if (p == 0) return;
    jMemset(p, c, static_cast<unsigned>(x2 - x1 + 1));
    vUnlock(self, 1);
}

// ------------------------------------------------------------------------------------------- 0x00010620 vline

// 0x00010620  Surface::vline(x, y1, y2, c) (VA 0x10010620): the vertical twin of hline. Returns when x is outside
// [left, right) (0x10010659 jl, 0x10010678 jl), when y1 == y2 (0x10010685 jne), when the span is below the
// rectangle (0x100106cc jge) or above it (0x100106eb jge), or when the pixel address is NULL (0x10010790 jne).
// y1 > y2 is swapped at 0x10010692, y1 raised to top (0x1001070f jge) and y2 lowered to bottom - 1 (0x1001074b jl).
// The run is an inline loop (0x100107bf..0x100107c3) that writes the low byte of c y2 - y1 + 1 times, stepping by
// the stride from slot +0xe0 (0x1001079e) each time, and ends with slot +0x24 (1) at 0x100107c8.
typedef void(__fastcall* Vline_t)(void*, void*, int, int, int, int);
Vline_t Vline_orig;
void __fastcall Vline_re(void* self, void*, int x, int y1, int y2, int c) {
    if (x < vClip(self)->left) return;
    if (x >= vClip(self)->right) return;
    if (y1 == y2) return;
    if (y1 > y2) xorSwap(y1, y2);
    if (y1 >= vClip(self)->bottom) return;
    if (y2 < vClip(self)->top) return;
    if (y1 < vClip(self)->top) y1 = vClip(self)->top;
    if (y2 >= vClip(self)->bottom) y2 = vClip(self)->bottom - 1;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel14(self, x, y1));
    if (p == 0) return;
    const int stride = vPitch(self);
    const unsigned char v = static_cast<unsigned char>(c);
    for (int n = y2 - y1 + 1; n != 0; n--) {
        *p = v;
        p += stride;
    }
    vUnlock(self, 1);
}

// ------------------------------------------------------------------------------------------ 0x00011c40 clear8

// 0x00011c40  Surface::clear8(c) (VA 0x10011c40): fills an 8-bit surface with the byte c. When the clip rectangle
// (slot +0xcc) and the bounds rectangle (slot +0xd4) are not equal (0x10011c8d calls 0x100085f0, test / jne at
// 0x10011c95), the work is handed to slot +0x44 with the clip rectangle and c (0x10011cbf) and nothing else runs.
// Otherwise the base address comes from slot +0x18 (0x10011cd8) with no null test, h is the bounds rectangle's
// bottom (0x10011cfc) and w its right (0x10011d19), and a `rep stosd` (0x10011d4e) writes
// ((w >> 2) + (w & 3 ? 1 : 0)) * h dwords of the byte replicated four times, the multiplication being the unsigned
// `mul` at 0x10011d3c. The surface is then released with slot +0x24 (1) at 0x10011d56.
typedef void(__fastcall* Clear8Fn_t)(void*, void*, int);
Clear8Fn_t Clear8_orig;
void __fastcall Clear8_re(void* self, void*, int c) {
    RECT* const b = vBounds(self);
    RECT* const k = vClip(self);
    if (jEqual(k, b) == 0) {
        vFillRect(self, vClip(self), c);
        return;
    }
    unsigned* q = reinterpret_cast<unsigned*>(vBits(self));
    const int h = vBounds(self)->bottom;
    const int w = vBounds(self)->right;
    unsigned dwords = (static_cast<unsigned>(w) >> 2) + ((w & 3) != 0 ? 1u : 0u);
    dwords = dwords * static_cast<unsigned>(h);          // `mul ecx` keeps the low dword
    const unsigned char v = static_cast<unsigned char>(c);
    const unsigned fill = v * 0x01010101u;
    for (unsigned i = 0; i < dwords; i++) *q++ = fill;
    vUnlock(self, 1);
}

// -------------------------------------------------------------------------------------- 0x00011dd0 fillRectC8

// 0x00011dd0  Surface::fillRectC8(rect, c) (VA 0x10011dd0): fills a rectangle of an 8-bit surface with the byte c
// and always returns 0. A null rectangle means the whole surface: 0x10011df4 / 0x10011df8 test it and call
// clear8 0x10011c40 directly (0x10011e01) before returning 0. Otherwise the rectangle is copied to a local
// (0x10011e10..0x10011e24), intersected with the clip rectangle of slot +0xcc (0x10011e47 calls 0x10008590) and the
// function returns 0 when the intersection is empty (0x10011e51 jne) or when the pixel address of its top-left
// corner, from slot +0x14, is NULL (0x10011e7d jne). h and w are the intersection's height and width
// (0x10011e89, 0x10011e92); the module global at 0x1012847c receives the row skip stride - w, the stride coming
// from slot +0xe0 (0x10011ea2, stored at 0x10011eb2). Each of the h rows writes w >> 2 dwords of the byte
// replicated four times (`rep stosd` 0x10011ee0) followed by w & 3 single bytes (`rep stosb` 0x10011ee4) and then
// skips the global (0x10011ee6); the row counter is the `dec ebp / jne` at 0x10011eec. Slot +0x24 (1) closes
// (0x10011ef6).
typedef int(__fastcall* FillRectC8_t)(void*, void*, const RECT*, int);
FillRectC8_t FillRectC8_orig;
int __fastcall FillRectC8_re(void* self, void*, const RECT* rp, int c) {
    if (rp == 0) {
        jClear8(self, c);
        return 0;
    }
    RECT rc = *rp;
    if (!jIntersect(&rc, &rc, vClip(self))) return 0;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel14(self, rc.left, rc.top));
    if (p == 0) return 0;
    const int h = rc.bottom - rc.top;
    const int w = rc.right - rc.left;
    gSkip() = vPitch(self) - w;
    const unsigned char v = static_cast<unsigned char>(c);
    const unsigned fill = v * 0x01010101u;
    const unsigned nd = static_cast<unsigned>(w) >> 2;
    const unsigned nb = static_cast<unsigned>(w) & 3;
    int rows = h;
    do {
        for (unsigned i = 0; i < nd; i++) {
            *reinterpret_cast<unsigned*>(p) = fill;
            p += 4;
        }
        for (unsigned i = 0; i < nb; i++) *p++ = v;
        p += gSkip();
    } while (--rows != 0);
    vUnlock(self, 1);
    return 0;
}

// ------------------------------------------------------------------------------------ 0x00011f80 ditherRectC8

// 0x00011f80  Surface::ditherRectC8(rect, c) (VA 0x10011f80): writes the byte c over every second pixel of a
// rectangle of an 8-bit surface, the parity alternating per row, and always returns 0. A null rectangle is replaced
// by the bounds rectangle of slot +0xd4 (0x10011fa8 jne, 0x10011fb4); the rectangle is then copied to a local
// (0x10011fc7..0x10011fdb) and intersected with the clip rectangle of slot +0xcc (0x10011ffe calls 0x10008590),
// and the function returns 0 when the intersection is empty (0x10012008 je) or when slot +0x14 gives a NULL pixel
// address (0x10012031 je). h = bottom - top (0x1001203a), w = right - left (0x10012043), skip = stride - w with
// the stride from slot +0xe0 (0x10012053). The inline loop (0x1001206c..0x100120b6) keeps h in edx, counts it
// down, and per row: when the remaining row count is odd the cursor is advanced one byte (0x1001207a test / je,
// 0x10012082), w >> 1 bytes are written two apart (0x1001208d..0x10012092, skipped entirely when w >> 1 is 0 at
// 0x1001208b je), and the row is closed by stepping back one byte when the row count is odd and w is even
// (0x10012094 test, 0x1001209c test, 0x100120a4 dec) or by writing one more byte when the row count is even and w
// is odd (0x100120ad je, 0x100120af). skip is then added (0x100120b2). Slot +0x24 (1) closes (0x100120bd).
typedef int(__fastcall* DitherRectC8_t)(void*, void*, const RECT*, int);
DitherRectC8_t DitherRectC8_orig;
int __fastcall DitherRectC8_re(void* self, void*, const RECT* rp, int c) {
    if (rp == 0) rp = vBounds(self);
    RECT rc = *rp;
    if (!jIntersect(&rc, &rc, vClip(self))) return 0;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel14(self, rc.left, rc.top));
    if (p == 0) return 0;
    const int h = rc.bottom - rc.top;
    const int w = rc.right - rc.left;
    const int skip = vPitch(self) - w;
    const unsigned char v = static_cast<unsigned char>(c);
    const int wOdd = w & 1;                     // ebx, computed once at 0x10012077
    int rows = h;                               // edx
    do {
        if (rows & 1) p++;
        unsigned n = static_cast<unsigned>(w) >> 1;
        if (n != 0) {
            do {
                *p = v;
                p += 2;
            } while (--n != 0);
        }
        if (rows & 1) {
            if (!(wOdd & 1)) p--;
        } else if (wOdd & 1) {
            *p = v;
            p++;
        }
        p += skip;
    } while (--rows != 0);
    vUnlock(self, 1);
    return 0;
}

// ------------------------------------------------------------------------------------------- 0x0000f880 fill8

// 0x0000f880  Surface::fill8(x, y, w, h, from, to) (VA 0x1000f880): inside the rectangle (x, y, w, h) of an 8-bit
// surface, replaces every pixel equal to the byte `from` with the byte `to`. The rectangle is built by 0x10008360
// (0x1000f8b1, arguments x, y, w, h) and intersected with the clip rectangle of slot +0xcc, the clip rectangle
// being the SECOND argument and the local the first and third (0x1000f8b9..0x1000f8d9); an empty intersection
// returns 0 (0x1000f8e3 jne). The top-left pixel address comes from slot +0x14 (0x1000f8fe) and a NULL one returns
// 7 (0x1000f90f jne). w and h are then re-read from the intersection with 0x10009120 and 0x10009160
// (0x1000f91c, 0x1000f92b) and the row skip is [this+0x40] - w taken as a direct field read, not through slot
// +0xe0 (0x1000f939). The inline loop (0x1000f958..0x1000f965) walks h rows of w bytes, comparing each with
// `from` (0x1000f958 cmp / 0x1000f95a jne) and storing `to` on a match (0x1000f95c). Slot +0x24 (1) closes
// (0x1000f96b) and the function returns 0 (0x1000f97f).
typedef int(__fastcall* Fill8_t)(void*, void*, int, int, int, int, int, int);
Fill8_t Fill8_orig;
int __fastcall Fill8_re(void* self, void*, int x, int y, int w, int h, int from, int to) {
    RECT r;
    jSetRect(&r, x, y, w, h);
    if (!jIntersect(&r, vClip(self), &r)) return 0;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel14(self, r.left, r.top));
    if (p == 0) return 7;
    w = jRectWidth(&r);
    h = jRectHeight(&r);
    const int skip = *reinterpret_cast<const int*>(reinterpret_cast<char*>(self) + 0x40) - w;
    const unsigned char a = static_cast<unsigned char>(from);
    const unsigned char b = static_cast<unsigned char>(to);
    int rows = h;
    do {
        int cols = w;
        do {
            if (*p == a) *p = b;
            p++;
        } while (--cols != 0);
        p += skip;
    } while (--rows != 0);
    vUnlock(self, 1);
    return 0;
}

// ----------------------------------------------------------------------------------------- 0x0000f9e0 fill2_8

// 0x0000f9e0  Surface::fill2_8(left, top, right, bottom, lut) (VA 0x1000f9e0): maps every pixel of a rectangle of
// an 8-bit surface through the 256-entry byte table `lut`. A null table returns 0x10 at once (0x1000f9fd cmp /
// 0x1000fa01 jne, 0x1000fa03). The rectangle is built from the four edges by 0x10010390 (0x1000fa21) and
// intersected with the clip rectangle of slot +0xcc (0x1000fa49 calls 0x10008590); an empty intersection returns 0
// (0x1000fa53 jne). w and h come from 0x10009120 / 0x10009160 (0x1000fa60, 0x1000fa6f) and the row skip is the
// slot-+0xe0 stride minus w (0x1000fa84, 0x1000fa91). Unlike fill8 and the two rectangle fills, the pixel address
// is taken from slot +0x0c directly rather than through the +0x14 forwarder (0x1000faa9); a NULL one returns 7
// (0x1000faba jne). The inline loop (0x1000fadb..0x1000fae9) reads each byte into the low byte of a register whose
// upper bytes were zeroed once (0x1000fad7), indexes the table with it and stores the result back
// (0x1000fadd, 0x1000fae0). Slot +0x24 (1) closes (0x1000faf0) and the function returns 0 (0x1000fb04).
typedef int(__fastcall* Fill2_8_t)(void*, void*, int, int, int, int, const unsigned char*);
Fill2_8_t Fill2_8_orig;
int __fastcall Fill2_8_re(void* self, void*, int left, int top, int right, int bottom, const unsigned char* lut) {
    if (lut == 0) return 0x10;
    RECT rc;
    jSetRectLTRB(&rc, left, top, right, bottom);
    if (!jIntersect(&rc, &rc, vClip(self))) return 0;
    const int w = jRectWidth(&rc);
    const int h = jRectHeight(&rc);
    const int skip = vPitch(self) - w;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel0c(self, rc.left, rc.top));
    if (p == 0) return 7;
    int rows = h;
    do {
        int cols = w;
        do {
            *p = lut[*p];
            p++;
        } while (--cols != 0);
        p += skip;
    } while (--rows != 0);
    vUnlock(self, 1);
    return 0;
}

// ------------------------------------------------------------------------------------ the dashed-line pattern
// Both dashed primitives carry the same state machine. After clipping, the phase is reduced modulo len1 + len2 by
// the signed `idiv` (dashedHLine8 0x100116b2, dashedVLine8 0x10011a6e) and turned into a signed cursor: a phase at
// or past len1 becomes len2 - (phase - len1), a phase below len1 becomes -(len1 - phase) (0x100116bd jl and the two
// arms at 0x100116bf / 0x100116d3; 0x10011a79 and 0x10011a7b / 0x10011a8f). len1 is then negated
// (0x100116e7, 0x10011aa3). The cursor is non-negative inside a `c2` run and negative inside a `c1` run: each step
// moves it one toward zero and, on reaching zero, reloads it with the negated len1 or with len2
// (0x10011713..0x10011718 and 0x1001171a..0x1001171d). Which colours are written depends on the two colours: with
// c1 == -1 only the non-negative half writes (0x100116f0 jne selects that block at 0x100116f2), with c2 == -1 only
// the negative half writes (0x1001172b jne selects the block at 0x1001172d), otherwise both do (0x10011762).

// ------------------------------------------------------------------------------------- 0x000114e0 dashedHLine8

// 0x000114e0  Surface::dashedHLine8(x1, x2, y, c1, c2, len1, len2, phase) (VA 0x100114e0): a horizontal dashed run
// on an 8-bit surface, alternating len1 pixels of c1 with len2 pixels of c2, starting at the given phase. It
// returns without drawing when y is outside the clip rectangle's [top, bottom) (0x1001151a jl, 0x10011539 jl),
// when x1 == x2 (0x10011546 jne), when both colours are -1 (0x10011551 jne and 0x10011557 jne over the two
// `cmp ..., -1`), when the span is entirely right of the rectangle (0x100115e0 jge) or entirely left of it
// (0x100115fe jge), or when slot +0x14 gives a NULL pixel address (0x100116a1 jne). x1 > x2 swaps x1 with x2, len1
// with len2 and c1 with c2 and replaces the phase with len1 + len2 - phase (0x10011564 jle guards the block at
// 0x10011566..0x100115c0). x1 is raised to left (0x10011621 jge) and x2 lowered to right - 1 (0x1001165c jl). The
// loop writes x2 - x1 + 1 bytes one after another (0x1001171f / 0x1001175a / 0x10011791 `inc edi`), and slot +0x24
// (1) closes (0x10011799).
typedef void(__fastcall* DashedH_t)(void*, void*, int, int, int, int, int, int, int, int);
DashedH_t DashedH_orig;
void __fastcall DashedH_re(void* self, void*, int x1, int x2, int y, int c1, int c2, int len1, int len2, int phase) {
    if (y < vClip(self)->top) return;
    if (y >= vClip(self)->bottom) return;
    if (x1 == x2) return;
    if (c1 == -1 && c2 == -1) return;
    if (x1 > x2) {
        xorSwap(x1, x2);
        xorSwap(len1, len2);
        xorSwap(c1, c2);
        phase = len1 + len2 - phase;
    }
    if (x1 >= vClip(self)->right) return;
    if (x2 < vClip(self)->left) return;
    if (x1 < vClip(self)->left) x1 = vClip(self)->left;
    if (x2 >= vClip(self)->right) x2 = vClip(self)->right - 1;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel14(self, x1, y));
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
    int n = x2 - x1 + 1;
    int d = phase;
    const unsigned char a = static_cast<unsigned char>(c1);
    const unsigned char b = static_cast<unsigned char>(c2);
    if (c1 == -1) {
        do {
            if (d >= 0) { *p = b; if (--d == 0) d = len1; }
            else        { if (++d == 0) d = len2; }
            p++;
        } while (--n != 0);
    } else if (c2 == -1) {
        do {
            if (d >= 0) { if (--d == 0) d = len1; }
            else        { *p = a; if (++d == 0) d = len2; }
            p++;
        } while (--n != 0);
    } else {
        do {
            if (d >= 0) { *p = b; if (--d == 0) d = len1; }
            else        { *p = a; if (++d == 0) d = len2; }
            p++;
        } while (--n != 0);
    }
    vUnlock(self, 1);
}

// ------------------------------------------------------------------------------------- 0x00011880 dashedVLine8

// 0x00011880  Surface::dashedVLine8(x, y1, y2, c1, c2, len1, len2, phase) (VA 0x10011880): the vertical twin of
// dashedHLine8. It returns without drawing when x is outside the clip rectangle's [left, right) (0x100118b9 jl,
// 0x100118d8 jl), when y1 == y2 (0x100118e5 jne), when both colours are -1 (0x100118f0 jne, 0x100118f6 jne), when
// the span is below the rectangle (0x1001197f jge) or above it (0x1001199e jge), or when slot +0x14 gives a NULL
// pixel address (0x10011a43 jne). y1 > y2 swaps the ends, the two run lengths and the two colours and replaces the
// phase with len1 + len2 - phase (0x10011903 jle guards 0x10011905..0x1001195f). y1 is raised to top (0x100119c2
// jge) and y2 lowered to bottom - 1 (0x100119fe jl). The row stride comes from slot +0xe0 before the phase
// arithmetic (0x10011a54) and the loop steps the cursor by it after each of the y2 - y1 + 1 pixels
// (0x10011ada / 0x10011b15 / 0x10011b4c `add edi, ebx`); the run-length reloads read the negated len1 and len2 back
// from the arguments (0x10011acf, 0x10011ad7) because ebx is carrying the stride. Slot +0x24 (1) closes
// (0x10011b55).
typedef void(__fastcall* DashedV_t)(void*, void*, int, int, int, int, int, int, int, int);
DashedV_t DashedV_orig;
void __fastcall DashedV_re(void* self, void*, int x, int y1, int y2, int c1, int c2, int len1, int len2, int phase) {
    if (x < vClip(self)->left) return;
    if (x >= vClip(self)->right) return;
    if (y1 == y2) return;
    if (c1 == -1 && c2 == -1) return;
    if (y1 > y2) {
        xorSwap(y1, y2);
        xorSwap(len1, len2);
        xorSwap(c1, c2);
        phase = len1 + len2 - phase;
    }
    if (y1 >= vClip(self)->bottom) return;
    if (y2 < vClip(self)->top) return;
    if (y1 < vClip(self)->top) y1 = vClip(self)->top;
    if (y2 >= vClip(self)->bottom) y2 = vClip(self)->bottom - 1;
    unsigned char* p = reinterpret_cast<unsigned char*>(vPixel14(self, x, y1));
    if (p == 0) return;
    const int stride = vPitch(self);
    phase = phase % (len1 + len2);
    if (phase >= len1) {
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;
    }
    len1 = -len1;
    int n = y2 - y1 + 1;
    int d = phase;
    const unsigned char a = static_cast<unsigned char>(c1);
    const unsigned char b = static_cast<unsigned char>(c2);
    if (c1 == -1) {
        do {
            if (d >= 0) { *p = b; if (--d == 0) d = len1; }
            else        { if (++d == 0) d = len2; }
            p += stride;
        } while (--n != 0);
    } else if (c2 == -1) {
        do {
            if (d >= 0) { if (--d == 0) d = len1; }
            else        { *p = a; if (++d == 0) d = len2; }
            p += stride;
        } while (--n != 0);
    } else {
        do {
            if (d >= 0) { *p = b; if (--d == 0) d = len1; }
            else        { *p = a; if (++d == 0) d = len2; }
            p += stride;
        } while (--n != 0);
    }
    vUnlock(self, 1);
}

}  // namespace

SG_HOOK("jgld.dll", 0x000103f0, Surface_hline, Hline_re, Hline_orig);
SG_HOOK("jgld.dll", 0x00010620, Surface_vline, Vline_re, Vline_orig);
SG_HOOK("jgld.dll", 0x00011c40, Surface_clear8, Clear8_re, Clear8_orig);
SG_HOOK("jgld.dll", 0x00011dd0, Surface_fillRectC8, FillRectC8_re, FillRectC8_orig);
SG_HOOK("jgld.dll", 0x00011f80, Surface_ditherRectC8, DitherRectC8_re, DitherRectC8_orig);
SG_HOOK("jgld.dll", 0x0000f880, Surface_fill8, Fill8_re, Fill8_orig);
SG_HOOK("jgld.dll", 0x0000f9e0, Surface_fill2_8, Fill2_8_re, Fill2_8_orig);
SG_HOOK("jgld.dll", 0x000114e0, Surface_dashedHLine8, DashedH_re, DashedH_orig);
SG_HOOK("jgld.dll", 0x00011880, Surface_dashedVLine8, DashedV_re, DashedV_orig);
