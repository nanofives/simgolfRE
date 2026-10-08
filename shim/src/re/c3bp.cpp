// Batch c3bp: jgld.dll render, the Sprite::draw16_* / draw_* family (the hand-off log/c3/c3bm_notes.txt
// asked for, "Candidates not taken", and round 17 of log/c3_brief.md).
//   0x0001cde0  Sprite::draw16_1001cde0  8-bit payload through the Palette table, key 0xff
//   0x00056450  Sprite::draw16_10056450  the same with a two-term stipple test
//   0x000576a0  Sprite::draw16_100576a0  the same with a three-term stipple test
//   0x0001f2d0  Sprite::draw_1001f2d0    one flat colour wherever the payload is below 0xfe
//   0x00026240  Sprite::draw16_10026240  the payload index remapped through a 256-byte table first
//   0x00027430  Sprite::draw16_10027430  the Palette colour remapped through a 16-bit table after
//   0x00028630  Sprite::draw16_10028630  payload 0xf8 remaps the DESTINATION pixel through that table
//   0x000298b0  Sprite::draw16_100298b0  payload 0xf8..0xfe picks a plane of a (source, destination) table
//   0x0002bcf0  Sprite::draw_1002bcf0    payload 0 writes 0, 1..0xf pick a plane, no Palette table at all
//   0x0002d890  Sprite::draw16_1002d890  the Palette colour averaged with the destination, gated by a flag
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below is a
// VA, as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts, so these
// hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// All ten share ONE skeleton, which is why they are one batch: guard, Palette, two scale ratios, the two
// pitches, the format switch, then `abs(0x10122dc0) == abs(0x10122dc8) && abs(0x10122dc4) == abs(0x10122dc8)`
// choosing an unscaled half from a 16.16 stepped half, and inside each half the SIGNS of 0x10122dc0 and
// 0x10122dc4 choosing one of four mirror bodies: eight pixel loops per function. Only the body of the loop
// differs between the ten, so the skeleton is written once (DrawFamily below, with 0x1001cde0's instruction
// addresses) and each function supplies its pixel operation and its preamble kind. Every twin repeats the
// skeleton at its own addresses; the per-function comment gives the addresses of what differs.
//
// Everything the A/B hands these functions is built by re/frida/js/fixtures.d/c3bp.js on jgld's own Surface
// (0x1011d0b0), Palette (0x1011db10) and Sprite (0x1011d380) vtables: never the live Display at 0x10128420,
// never a screen surface, never DirectDraw. log/c3/alt/c3bp_probe.py, one 12 s boot at the menu with counters
// attached straight to the module addresses: 0 entries into all ten of these functions and into each of their
// static callers (0x15180, 0x193d0, 0x15f80, 0x169a0, 0x16cb0, 0x165a0, 0x19830, 0x18120, 0x18920), against
// 1_704 entries into the live twin Sprite::draw16_1001bc40 0x1bc40 and 89_378 into its caller 0x15480, both
// on the process's first thread.
//
// THE SHARED LOOP GLOBALS. Like every sprite blitter these park their loop state in 0x1012857c (source
// pitch), 0x1012855c (destination pitch), 0x1012856c (colour table), 0x10128578 (rows), 0x10128564 (columns),
// 0x1012853c (source row skip) and 0x10128560 (destination row skip), and the live 0x1bc40 writes the same
// block: the probe measured 462, 122, 6 and 462 changes in twelve seconds. Every registry key of this batch
// therefore runs with the game's first thread suspended and restores the block before resuming it (fixture
// c3bp.js, the recipe c3av established for 0x1012847c and c3bm extended). The row counter at 0x10128578 is
// decremented IN MEMORY by the loops below, exactly as `dec dword ptr [0x10128578]` does.
// The scaled half's own globals (0x10128540/44/54/58/70/80/84/88) and the three parameter slots (0x10128530
// the flat colour, 0x10128548 and 0x10128574 the table pointers) measured 0 changes in the same boot, so they
// are state regions of the keys instead.
//
// Surface vtable slots used here (0x1011d0b0, named in re/names/jgld_2.tsv):
//   +0x10  lock()      0x10008730: copies the base at this+0x4c0 to this+0x4cc, bumps the use count at
//          this+0x4c8 when that copy is not NULL (0x10008769), and returns the COPY (0x10008783), so a
//          surface with no pixel base makes the guard return 3.
//   +0x20  bits()      0x1000a7b0: tail-calls slot +0x10.
//   +0x24  unlock(f)   0x100087b0: this+0x4c8 -= f, clearing this+0x4cc and this+0x4c8 at <= 0.
//   +0xcc  clipRect()  0x1000a990: returns this+0x44.
//   +0xe0  pitch()     0x1000aaf0: returns the int at this+0x40.
//   +0xe4  depthPtr()  0x1000ab30: returns this+0x24, so element [1] is the format code at this+0x28.
// Palette vtable slots (0x1011db10): +0x18 -> this+0x40c (RGB565 table), +0x1c -> this+0x60c (RGB555 table).
// Sprite fields: +0x10 Palette, +0x14 payload, +0x2c pitch, +0x30 width, +0x34 height.
#include "hooks.h"

namespace {

// jgld.dll is relocatable, so every module address used below is resolved against the running base.
char* JgldBase() {
    static char* base = 0;
    if (!base) base = reinterpret_cast<char*>(GetModuleHandleA("jgld.dll"));
    return base;
}

// ------------------------------------------------------------------ the Surface, Palette and module helpers

typedef RECT*(__fastcall* RectSlot_t)(void*, void*);
typedef void*(__fastcall* PtrSlot_t)(void*, void*);
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
typedef int*(__fastcall* IntPtrSlot_t)(void*, void*);
typedef int(__fastcall* IntSlot_t)(void*, void*);
typedef unsigned short*(__fastcall* TableSlot_t)(void*, void*);

inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}
inline int vLock(void* s) { return reinterpret_cast<IntSlot_t>(slot(s, 0x10))(s, 0); }
inline void* vBits(void* s) { return reinterpret_cast<PtrSlot_t>(slot(s, 0x20))(s, 0); }
inline void vUnlock(void* s, int f) { reinterpret_cast<UnlockSlot_t>(slot(s, 0x24))(s, 0, f); }
inline RECT* vClip(void* s) { return reinterpret_cast<RectSlot_t>(slot(s, 0xcc))(s, 0); }
inline int vPitch(void* s) { return reinterpret_cast<IntSlot_t>(slot(s, 0xe0))(s, 0); }
inline int* vDepthPtr(void* s) { return reinterpret_cast<IntPtrSlot_t>(slot(s, 0xe4))(s, 0); }
inline unsigned short* vTable565(void* p) { return reinterpret_cast<TableSlot_t>(slot(p, 0x18))(p, 0); }
inline unsigned short* vTable555(void* p) { return reinterpret_cast<TableSlot_t>(slot(p, 0x1c))(p, 0); }

// The module's own helpers, called through their module addresses so the comparison is of this code and not
// of a copy: setRect 0x10008360 (through thunk 0x10001019, 0x1001cfe1), intersect 0x10008590 (thunk
// 0x10001087, 0x1001d44a), the CRT abs 0x1007f370 (0x1001ce80). IntersectRect is called through jgld's OWN
// import slot at 0x1012c5e4, the one the originals call (0x1001d017).
typedef void(__cdecl* SetRect_t)(RECT*, int, int, int, int);
typedef int(__cdecl* Intersect_t)(RECT*, const RECT*, const RECT*);
typedef int(__cdecl* Abs_t)(int);
typedef BOOL(WINAPI* IntersectRect_t)(RECT*, const RECT*, const RECT*);

inline void jSetRect(RECT* r, int x, int y, int w, int h) {
    reinterpret_cast<SetRect_t>(JgldBase() + 0x8360)(r, x, y, w, h);
}
inline int jIntersect(RECT* d, const RECT* a, const RECT* b) {
    return reinterpret_cast<Intersect_t>(JgldBase() + 0x8590)(d, a, b);
}
inline int jAbs(int v) { return reinterpret_cast<Abs_t>(JgldBase() + 0x7f370)(v); }
inline BOOL jIntersectRect(RECT* d, const RECT* a, const RECT* b) {
    return (*reinterpret_cast<IntersectRect_t*>(JgldBase() + 0x12c5e4))(d, a, b);
}

// The module globals, by RVA.
inline int& G(unsigned rva) { return *reinterpret_cast<int*>(JgldBase() + rva); }
inline float& Gf(unsigned rva) { return *reinterpret_cast<float*>(JgldBase() + rva); }
inline unsigned short& Gw(unsigned rva) { return *reinterpret_cast<unsigned short*>(JgldBase() + rva); }
inline int* GpInt(unsigned rva) { return *reinterpret_cast<int**>(JgldBase() + rva); }

inline int SpriteInt(void* s, unsigned off) { return *reinterpret_cast<int*>(reinterpret_cast<char*>(s) + off); }
inline unsigned char* SpriteBits(void* s) {
    return *reinterpret_cast<unsigned char**>(reinterpret_cast<char*>(s) + 0x14);
}
// The Palette of the sprite at +0x10, or the one of the client object the module global 0x1012873c points at
// (0x1001ce37 / 0x1001ce48 and the twins in all ten functions).
inline void* SpritePalette(void* s) {
    void* p = *reinterpret_cast<void**>(reinterpret_cast<char*>(s) + 0x10);
    if (p) return p;
    char* client = *reinterpret_cast<char**>(JgldBase() + 0x12873c);
    return *reinterpret_cast<void**>(client + 4);
}

// VC6 converts a double to an int with __ftol (jgld's own copy is 0x1007f12c, called at 0x1001d3cc,
// 0x1001d3de, 0x1001d3ea and 0x1001d3fb): a 64-bit fistp with the low dword kept, i.e. truncation.
inline int Ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// The colour table parked at 0x1012856c, as the loops read it (`mov ecx, dword ptr [0x1012856c]`, 0x1001d0f8).
inline const unsigned short* Tbl() { return reinterpret_cast<const unsigned short*>(G(0x12856c)); }

// ------------------------------------------------------------------ the skeleton

// What the preamble does between the two pitches and the scale test, which is the only part of the skeleton
// that is not identical across the ten.
enum PreKind {
    PreTableUnlock,   // format switch -> 0x1012856c; the switch default UNLOCKS and returns 1 (0x1001cf59)
    PreTableKeep,     // the same switch whose default returns 1 WITHOUT unlocking (0x100263f1)
    PreFlat,          // no colour table: one resolved colour is parked at 0x10128530 (0x1001f3ea)
    PreNone           // no format switch at all (0x1002bdfc is the last thing before the scale test)
};

// The unscaled half of the four mirror bodies, which begin at 0x1001d05f, 0x1001d134, 0x1001d216 and
// 0x1001d2eb. `edx` is the remaining column count, read by the stipple tests of 0x56450 and 0x576a0 before it
// is decremented, and `dword ptr [0x10128578]` the remaining row count, decremented in memory.
//   colStep  `inc esi` (0x1001d111) when 0x10122dc0 >= 0, `dec esi` (0x1001d2c8) when it is negative
//   rowAdd   `add esi, [0x1012853c]` (0x1001d118) when 0x10122dc4 >= 0, `sub esi` (0x1001d1ed) when negative
template <class Op>
void UnscaledLoop(const unsigned char* s, unsigned short* d, int colStep, bool rowAdd, const Op& op) {
    for (;;) {
        int cols = G(0x128564);                                                   // 0x1001d0fb
        for (;;) {
            op(s, d, static_cast<unsigned>(cols));                                // 0x1001d101..0x1001d10e
            s += colStep;                                                         // 0x1001d111
            ++d;                                                                  // 0x1001d112
            if (--cols == 0) break;                                               // 0x1001d115 / 0x1001d116
        }
        if (rowAdd) s += G(0x12853c); else s -= G(0x12853c);                       // 0x1001d118
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x1001d11e
        if (--G(0x128578) == 0) break;                                            // 0x1001d124 / 0x1001d12a
    }
}

// The 16.16 stepped half of the four mirror bodies (the first is 0x1001d62c..0x1001d6e2). edx carries the
// fractional vertical position in its high half and the fractional horizontal one in its low half
// (0x1001d634..0x1001d63d); ebx carries the remaining column count minus one in its high half and the
// fractional horizontal step in its low half (0x1001d689 / 0x1001d68f), so `sub ebx, 0x10000` + `jns` runs
// the row exactly `cols` times (0x1001d6b9 / 0x1001d6bf). The source cursor takes the INTEGER part of the
// horizontal step plus the carry out of the fractional add (`add dx, bx` + `adc esi`, 0x1001d6ad /
// 0x1001d6b0), and at the end of a row the carry out of the vertical add selects one of the two ints the
// module pointer at 0x10122e20 addresses (`sbb eax, eax` 0x1001d6cd, `add esi, [ebx + eax*4]` 0x1001d6d5).
//   stepX     the integer horizontal step: 0x10128580 where the function parks it, ecx where it does not
//   fwd       `adc esi` (0x1001d6b0) when 0x10122dc0 >= 0, `sbb esi` (0x1001d9e5) when it is negative
template <class Op>
void ScaledLoop(const unsigned char* s, unsigned short* d, int stepX, bool fwd, const Op& op) {
    const int* const rowStep = GpInt(0x122e20);                                   // 0x1001d6cf
    const unsigned fracStepX = static_cast<unsigned short>(G(0x128588));          // 0x1001d68f
    const unsigned fracStepY = static_cast<unsigned>(G(0x128544));                // 0x1001d6c7
    unsigned acc = (static_cast<unsigned>(G(0x128558)) << 16) |
                   static_cast<unsigned short>(G(0x128554));                      // 0x1001d634..0x1001d63d
    for (;;) {
        int ebx = static_cast<int>(static_cast<unsigned>(G(0x128564)) | fracStepX);  // 0x1001d689/0x1001d68f
        acc = (acc & 0xffff0000u) | static_cast<unsigned short>(G(0x128554));     // 0x1001d696
        for (;;) {
            op(s, d, acc);                                                        // 0x1001d69d..0x1001d6aa
            const unsigned lo = (acc & 0xffff) + (static_cast<unsigned>(ebx) & 0xffff);  // 0x1001d6ad
            acc = (acc & 0xffff0000u) | (lo & 0xffff);
            const int carry = static_cast<int>(lo >> 16);
            if (fwd) s += stepX + carry; else s -= stepX + carry;                 // 0x1001d6b0
            ++d;                                                                  // 0x1001d6b6
            ebx -= 0x10000;                                                       // 0x1001d6b9
            if (ebx < 0) break;                                                   // 0x1001d6bf
        }
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x1001d6c1
        const unsigned long long sum =
            static_cast<unsigned long long>(acc) + fracStepY;                     // 0x1001d6c7
        acc = static_cast<unsigned>(sum);
        const int sel = (sum >> 32) ? -1 : 0;                                     // 0x1001d6cd
        s += rowStep[sel];                                                        // 0x1001d6d5
        if (--G(0x128578) == 0) break;                                            // 0x1001d6d8 / 0x1001d6de
    }
}

// The whole function, from the guard to the final unlock. `param` is the fourth argument of the functions
// that take one; `paramRva` the module global it is stored into before the format switch (0 for the ones that
// take no fourth argument, and for 0x1f2d0 whose argument is consumed by PreFlat instead).
// `parkStepX` says whether the stepped half stores the integer horizontal step in 0x10128580 (0x1001d673) or
// keeps it in ecx only (0x1001fb43 in 0x1f2d0 and 0x1002c54e in 0x2bcf0 have no such store).
template <class Op>
int DrawFamily(void* self, void* dst, int x, int y, PreKind pre, int param, unsigned paramRva,
               bool parkStepX, const Op& op) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;  // 0x1001ce0a / 0x1001ce10 / 0x1001ce28, 0x1001ce2a
    void* const pal = SpritePalette(self);                                        // 0x1001ce3b / 0x1001ce48
    if (pal == 0) {                                                               // 0x1001ce57
        vUnlock(dst, 1);                                                          // 0x1001ce65
        return 16;                                                                // 0x1001ce6f
    }
    // Both ratios are `fild` of the CRT abs of a numerator, `fidiv` by the denominator at 0x10122dc8 and a
    // `fstp dword` (0x1001ce8b..0x1001ce94 and 0x1001ceac..0x1001ceb5). The quotient is formed at the x87
    // precision the A/B thread runs under and rounded to float only by the store, so each operand is widened
    // to double and the result assigned to a float: the form batch c3ap established.
    Gf(0x12858c) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc0))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1001ce94
    Gf(0x128590) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc4))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1001ceb5
    G(0x12857c) = SpriteInt(self, 0x2c);                                          // 0x1001cec1
    G(0x12855c) = vPitch(dst);                                                    // 0x1001cede
    if (paramRva != 0) G(paramRva) = param;                                       // 0x1002634f / 0x100299bf
    if (pre == PreTableUnlock || pre == PreTableKeep) {
        const int fmt = vDepthPtr(dst)[1];                                        // 0x1001cf03
        if (fmt == 0) {                                                           // 0x1001cf09 / 0x1001cf0d
            G(0x12856c) = reinterpret_cast<int>(vTable565(pal));                  // 0x1001cf2b
        } else if (fmt == 1) {                                                    // 0x1001cf0f / 0x1001cf13
            G(0x12856c) = reinterpret_cast<int>(vTable555(pal));                  // 0x1001cf46
        } else {
            if (pre == PreTableUnlock) vUnlock(dst, 1);                           // 0x1001cf59
            return 1;                                                             // 0x1001cf63 / 0x100263b5
        }
    } else if (pre == PreFlat) {
        // 0x1f2d0 only: a negative argument IS the colour, otherwise the argument's low byte indexes the
        // Palette table the format code selects (0x1001f3dc..0x1001f46c).
        if (param >> 31) {                                                        // 0x1001f3df / 0x1001f3e4
            Gw(0x128530) = static_cast<unsigned short>(param);                    // 0x1001f3ea
        } else {
            const int fmt = vDepthPtr(dst)[1];                                    // 0x1001f40d
            if (fmt == 0) {                                                       // 0x1001f413 / 0x1001f417
                Gw(0x128530) = vTable565(pal)[param & 0xff];                      // 0x1001f43e / 0x1001f442
            } else if (fmt == 1) {                                                // 0x1001f419 / 0x1001f41d
                Gw(0x128530) = vTable555(pal)[param & 0xff];                      // 0x1001f468 / 0x1001f46c
            } else {
                return 1;                                                         // 0x1001f475, no unlock
            }
        }
    }
    const bool negX = G(0x122dc0) < 0;
    const bool negY = G(0x122dc4) < 0;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1001cf8c / 0x1001cf8e
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1001cfb3 / 0x1001cfb5
        // ---- the unscaled half (0x1001cfbb..0x1001d3bb)
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x1001cfd5
        orig = rc;                                                                // 0x1001cfdd..0x1001cff2
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x1001d017 / 0x1001d026
            vUnlock(dst, 1);                                                      // 0x1001d034
            return 0;                                                             // 0x1001d03e
        }
        // The four mirror bodies, chosen at 0x1001d045/0x1001d04c and 0x1001d052/0x1001d059 (and the second
        // sign test of the mirrored half at 0x1001d209/0x1001d210), compute the same two cursors and the same
        // row and column counts; they differ in the sign of the source row skip and in the column direction.
        const unsigned char* const s = bits + (rc.left - orig.left) +
                                       (rc.top - orig.top) * G(0x12857c);         // 0x1001d05f..0x1001d07c
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x1001d089..0x1001d0a6
        G(0x128578) = rc.bottom - rc.top;                                         // 0x1001d0af
        G(0x128564) = rc.right - rc.left;                                         // 0x1001d0bb
        // pitch - cols where the two numerators have the same sign (0x1001d0c0..0x1001d0cc), pitch + cols
        // where they differ (0x1001d195..0x1001d1a1)
        G(0x12853c) = (negX == negY) ? G(0x12857c) - G(0x128564)
                                     : G(0x12857c) + G(0x128564);
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x1001d0d2..0x1001d0e0
        UnscaledLoop(s, d, negX ? -1 : 1, !negY, op);
    } else {
        // ---- the 16.16 stepped half (0x1001d3c0..0x1001dbcc)
        const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                            static_cast<double>(Gf(0x128590)));                   // 0x1001d3c3..0x1001d3cc
        const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                            static_cast<double>(Gf(0x12858c)));                   // 0x1001d3d5..0x1001d3de
        const int oy = Ftol(static_cast<double>(Gf(0x128590)));                   // 0x1001d3e4 / 0x1001d3ea
        const int ox = Ftol(static_cast<double>(Gf(0x12858c)));                   // 0x1001d3f5 / 0x1001d3fb
        RECT rc;
        RECT orig;
        jSetRect(&rc, x + ox, y + oy, sw, sh);                                    // 0x1001d40a
        orig = rc;                                                                // 0x1001d412..0x1001d427
        if (!jIntersect(&rc, &rc, vClip(dst))) {                                  // 0x1001d44a / 0x1001d454
            vUnlock(dst, 1);                                                      // 0x1001d462
            return 0;                                                             // 0x1001d46c
        }
        // The destination cursor is formed with the pitch the preamble parked, BEFORE the pitch is read again
        // into the same global at 0x1001d4b4.
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x1001d47d..0x1001d4a3
        G(0x12855c) = vPitch(dst);                                                // 0x1001d4b4
        G(0x128564) = rc.right - rc.left;                                         // 0x1001d4bf
        G(0x128578) = rc.bottom - rc.top;                                         // 0x1001d4cb
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x1001d4d1..0x1001d4de
        G(0x128588) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                    // 0x1001d4e3..0x1001d502
        G(0x128540) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                    // 0x1001d507..0x1001d526
        const int offX = rc.left - orig.left;                                     // 0x1001d545..0x1001d54b
        const int offY = rc.top - orig.top;                                       // 0x1001d54e..0x1001d554
        G(0x128554) = G(0x128588) * offX;                                         // 0x1001d557..0x1001d561
        G(0x128558) = G(0x128540) * offY;                                         // 0x1001d567..0x1001d571
        G(0x128570) = ((G(0x128554) & 0xffff) + G(0x128588) * G(0x128564)) >> 16; // 0x1001d577..0x1001d593
        G(0x128584) = ((G(0x128558) & 0xffff) + G(0x128540) * G(0x128578)) >> 16; // 0x1001d598..0x1001d5b5
        const int pitch = G(0x12857c);
        const int w = SpriteInt(self, 0x30);
        const int h = SpriteInt(self, 0x34);
        const unsigned char* s;
        int* const rowStep = GpInt(0x122e20);                                     // 0x1001d60e
        // The four mirror bodies are chosen at 0x1001d52b/0x1001d532, 0x1001d538/0x1001d53f and
        // 0x1001d87c (the second sign test of the mirrored half).
        if (!negX && !negY) {
            G(0x12853c) = pitch - G(0x128570);                                    // 0x1001d5bb..0x1001d5c7
            s = bits + (G(0x128558) >> 16) * pitch + (G(0x128554) >> 16);         // 0x1001d5cd..0x1001d5f0
            rowStep[0] = (G(0x128540) >> 16) * pitch - pitch + G(0x12853c);       // 0x1001d5f3..0x1001d614
        } else if (!negX && negY) {
            // the vertical mirror starts on the LAST payload row (0x1001d761..0x1001d771) and still steps
            // the source cursor FORWARD, both inside the row and from row to row
            s = bits + (h - 1 - (G(0x128558) >> 16)) * pitch + (G(0x128554) >> 16);  // 0x1001d761..0x1001d78b
            rowStep[0] = (G(0x128540) >> 16) * pitch + G(0x128570);               // 0x1001d78e..0x1001d7a2
        } else if (negX && !negY) {
            s = bits + (G(0x128558) >> 16) * pitch + w - 1 - (G(0x128554) >> 16); // 0x1001d900..0x1001d92b
            rowStep[0] = (G(0x128540) >> 16) * pitch + G(0x128570);               // 0x1001d92e..0x1001d93e
        } else {
            s = bits + (h - 1 - (G(0x128558) >> 16)) * pitch + w - 1 -
                (G(0x128554) >> 16);                                              // 0x1001da92..0x1001daca
            G(0x12853c) = -pitch + G(0x128570);                                   // 0x1001dacd..0x1001dada
            rowStep[0] = (G(0x128540) >> 16) * pitch - G(0x128570);               // 0x1001dadf..0x1001dafe
        }
        *GpInt(0x122e1c) = rowStep[0] + pitch;                                    // 0x1001d616..0x1001d62a
        G(0x128564) = (G(0x128564) - 1) << 16;                                    // 0x1001d654..0x1001d65e
        const int stepX = static_cast<unsigned short>(G(0x128588) >> 16);         // 0x1001d666 word [0x1012858a]
        if (parkStepX) G(0x128580) = stepX;                                       // 0x1001d66d
        G(0x128544) = G(0x128540) << 16;                                          // 0x1001d673..0x1001d67b
        ScaledLoop(s, d, stepX, !negX, op);
    }
    vUnlock(dst, 2);                                                              // 0x1001dbd8
    return 0;                                                                     // 0x1001dbe2
}

// ------------------------------------------------------------------ the ten pixel operations

// Each operation below is given with the addresses of the FIRST of its eight copies, the one in the body
// both scale numerators non-negative chooses inside the unscaled half.
// The payload byte is transparent at or above 0xff in seven of the ten; 0x1001f2d0 and 0x1002bcf0 use 0xfe
// (`cmp byte ptr [esi], 0xfe` at 0x1001f612 and 0x1002bf96).
struct OpPalette {        // 0x1001cde0: index the colour table with the payload byte
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xff) *d = Tbl()[*s];                                            // 0x1001d101..0x1001d10e
    }
};

struct OpStipple2 {       // 0x10056450: and only where both counters are even
    void operator()(const unsigned char* s, unsigned short* d, unsigned edx) const {
        if (*s < 0xff &&                                                          // 0x10056771 / 0x10056774
            (edx & 1) == 0 &&                                                     // 0x10056778 / 0x1005677b
            (static_cast<unsigned>(G(0x128578)) & 1) == 0) {                      // 0x10056782 / 0x10056785
            *d = Tbl()[*s];                                                       // 0x10056787..0x1005678f
        }
    }
};

struct OpStipple3 {       // 0x100576a0: the same two tests plus bit 1 of the sum of the two counters
    void operator()(const unsigned char* s, unsigned short* d, unsigned edx) const {
        if (*s < 0xff &&                                                          // 0x100579c1 / 0x100579c4
            (edx & 1) == 0 &&                                                     // 0x100579c8 / 0x100579cb
            (static_cast<unsigned>(G(0x128578)) & 1) == 0 &&                      // 0x100579d2 / 0x100579d5
            ((edx + static_cast<unsigned>(G(0x128578))) & 2) == 0) {              // 0x100579df / 0x100579e2
            *d = Tbl()[*s];                                                       // 0x100579e4..0x100579ec
        }
    }
};

struct OpFlat {           // 0x1001f2d0: one colour; the unscaled loops load it into ax once before the row
                          // loop (0x1001f606) and the stepped ones re-read it for every covered pixel
                          // (0x1001fb85), which is the same value either way
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xfe) *d = Gw(0x128530);                                         // 0x1001f612 / 0x1001f617
    }
};

struct OpRemapIndex {     // 0x10026240: the payload byte indexes a 256-byte table, the result the colour table
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xff) {                                                          // 0x10026559 / 0x1002655c
            const unsigned char* const map =
                reinterpret_cast<const unsigned char*>(G(0x128548));              // 0x10026547 -> ebp
            *d = Tbl()[map[*s]];                                                  // 0x1002655e..0x10026568
        }
    }
};

struct OpRemapColour {    // 0x10027430: the colour table's entry indexes a 16-bit table
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xff) {                                                          // 0x10027749 / 0x1002774c
            const unsigned short* const map =
                reinterpret_cast<const unsigned short*>(G(0x128548));
            *d = map[Tbl()[*s]];                                                  // 0x1002774e..0x10027759
        }
    }
};

struct OpRemapDest {      // 0x10028630: payload 0xf8 remaps the DESTINATION pixel through that table instead
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xff) {                                                          // 0x10028949 / 0x1002894c
            if (*s == 0xf8) {                                                     // 0x1002894e / 0x10028951
                const unsigned short* const map =
                    reinterpret_cast<const unsigned short*>(G(0x128548));
                *d = map[*d];                                                     // 0x10028953..0x1002895b
            } else {
                *d = Tbl()[*s];                                                   // 0x10028960..0x10028966
            }
        }
    }
};

struct OpPlaneHigh {      // 0x100298b0: payload 0xf8..0xfe picks a 0x8000-entry plane of the table at
                          // 0x10128574, indexed by the destination pixel
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xff) {                                                          // 0x10029bca / 0x10029bcd
            if (*s < 0xf8) {                                                      // 0x10029bd1 / 0x10029bd4
                *d = Tbl()[*s];                                                   // 0x10029bd6..0x10029bdc
            } else {
                const unsigned short* const plane =
                    reinterpret_cast<const unsigned short*>(G(0x128574));
                const unsigned idx = (static_cast<unsigned>(*s - 0xf8) << 15) | *d;  // 0x10029be1..0x10029be8
                *d = plane[idx];                                                  // 0x10029beb / 0x10029bf0
            }
        }
    }
};

struct OpPlaneLow {       // 0x1002bcf0: payload 0 writes 0, 1..0xf pick a plane of the table at 0x10128548,
                          // 0x10 up to 0xfd are left alone; no colour table is used at all
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xfe) {                                                          // 0x1002bf96 / 0x1002bf99
            const unsigned char v = *s;                                           // 0x1002bf9d
            if (v == 0) {                                                         // 0x1002bf9f / 0x1002bfa1
                *d = 0;                                                           // 0x1002bfa3
            } else if (static_cast<unsigned char>(v - 1) < 0xf) {                 // 0x1002bfaa..0x1002bfae
                const unsigned short* const plane =
                    reinterpret_cast<const unsigned short*>(G(0x128548));
                const unsigned idx = (static_cast<unsigned>(v - 1) << 15) | *d;   // 0x1002bfb0 / 0x1002bfb3
                *d = plane[idx];                                                  // 0x1002bfb6 / 0x1002bfbb
            }
        }
    }
};

// 0x1002d890: the average of the colour table entry and the destination pixel, taken on the RGB555 field
// layout whatever the format code is. Each colour is shifted up by four so that the red and blue fields
// (mask 0x7c1f0) and the green one (mask 0x3e00) can be added without carrying into each other
// (0x1002dbc9..0x1002dbfe); the two halves are halved, re-masked, added and shifted back down.
inline unsigned short Average555(unsigned short a, unsigned short b) {
    const unsigned x = static_cast<unsigned>(a) << 4;                             // 0x1002dbd1
    const unsigned y = static_cast<unsigned>(b) << 4;                             // 0x1002dbd4
    unsigned rb = (x & 0x7c1f0) + (y & 0x7c1f0);                                  // 0x1002dbdb / 0x1002dbe6 / 0x1002dbf2
    unsigned gr = (x & 0x3e00) + (y & 0x3e00);                                    // 0x1002dbe0 / 0x1002dbec / 0x1002dbf4
    rb = (rb >> 1) & 0x7c1f0;                                                     // 0x1002dbf6 / 0x1002dbfa
    gr = (gr >> 1) & 0x3e00;                                                      // 0x1002dbf8 / 0x1002dbff
    return static_cast<unsigned short>((rb + gr) >> 4);                           // 0x1002dc05 / 0x1002dc07
}

struct OpAverage {        // 0x1002d890: with a non-zero flag the payload bytes from 0xf8 up are skipped too
    unsigned char flag;
    void operator()(const unsigned char* s, unsigned short* d, unsigned) const {
        if (*s < 0xff) {                                                          // 0x1002dbb1 / 0x1002dbb4
            if (flag != 0 && *s >= 0xf8) return;                                  // 0x1002dbb6..0x1002dbbf
            *d = Average555(Tbl()[*s], *d);                                       // 0x1002dbc8..0x1002dc0a
        }
    }
};

// ------------------------------------------------------------------ the hooks

// The MinHook trampolines the A/B's "original" arm is called through (one per SG_HOOK below).
typedef int(__fastcall* Draw3_t)(void*, void*, void*, int, int);
typedef int(__fastcall* Draw4_t)(void*, void*, void*, int, int, int);
typedef int(__fastcall* Draw4c_t)(void*, void*, void*, int, int, unsigned char);
Draw3_t DrawCde0_orig;
Draw3_t Draw56450_orig;
Draw3_t Draw576a0_orig;
Draw4_t DrawF2d0_orig;
Draw4_t Draw26240_orig;
Draw4_t Draw27430_orig;
Draw4_t Draw28630_orig;
Draw4_t Draw298b0_orig;
Draw4_t DrawBcf0_orig;
Draw4c_t DrawD890_orig;

// 0x0001cde0  Sprite::draw16_1001cde0(dst, x, y)
// The cold twin of the live Sprite::draw16_1001bc40 0x1bc40; the two bodies differ only in the transparency
// threshold (0xff at 0x1001d101 against 0xfe at 0x1001bf61).
int __fastcall DrawCde0_re(void* self, void*, void* dst, int x, int y) {
    return DrawFamily(self, dst, x, y, PreTableUnlock, 0, 0, true, OpPalette());
}

// 0x00056450  Sprite::draw16_10056450(dst, x, y)
int __fastcall Draw56450_re(void* self, void*, void* dst, int x, int y) {
    return DrawFamily(self, dst, x, y, PreTableUnlock, 0, 0, true, OpStipple2());
}

// 0x000576a0  Sprite::draw16_100576a0(dst, x, y)
int __fastcall Draw576a0_re(void* self, void*, void* dst, int x, int y) {
    return DrawFamily(self, dst, x, y, PreTableUnlock, 0, 0, true, OpStipple3());
}

// 0x0001f2d0  Sprite::draw_1001f2d0(dst, x, y, colour)
// The one member here that parks no colour table: the fourth argument is resolved ONCE into the 16-bit slot
// 0x10128530 (0x1001f3ea, 0x1001f442, 0x1001f46c) and every covered pixel is written with it. Its stepped
// half keeps the integer horizontal step in ecx (`mov cx, word ptr [0x1012858a]` 0x1001fb50, `adc esi, ecx`
// 0x1001fb93) and writes 0x10128580 nowhere.
int __fastcall DrawF2d0_re(void* self, void*, void* dst, int x, int y, int colour) {
    return DrawFamily(self, dst, x, y, PreFlat, colour, 0, false, OpFlat());
}

// 0x00026240  Sprite::draw16_10026240(dst, x, y, map)
int __fastcall Draw26240_re(void* self, void*, void* dst, int x, int y, int map) {
    return DrawFamily(self, dst, x, y, PreTableKeep, map, 0x128548, true, OpRemapIndex());
}

// 0x00027430  Sprite::draw16_10027430(dst, x, y, map)
int __fastcall Draw27430_re(void* self, void*, void* dst, int x, int y, int map) {
    return DrawFamily(self, dst, x, y, PreTableKeep, map, 0x128548, true, OpRemapColour());
}

// 0x00028630  Sprite::draw16_10028630(dst, x, y, map)
int __fastcall Draw28630_re(void* self, void*, void* dst, int x, int y, int map) {
    return DrawFamily(self, dst, x, y, PreTableKeep, map, 0x128548, true, OpRemapDest());
}

// 0x000298b0  Sprite::draw16_100298b0(dst, x, y, table)
// The argument is stored at 0x10128574 (0x100299bf) and that is also where the loops read their plane table
// (0x10029bbe and its seven twins): this function touches 0x10128548 nowhere.
int __fastcall Draw298b0_re(void* self, void*, void* dst, int x, int y, int table) {
    return DrawFamily(self, dst, x, y, PreTableKeep, table, 0x128574, true, OpPlaneHigh());
}

// 0x0002bcf0  Sprite::draw_1002bcf0(dst, x, y, table)
// No format switch at all: the Palette is fetched and tested and then never used, and the argument stored at
// 0x10128548 (0x1002bdfc) carries every colour. Its stepped half keeps the integer horizontal step in ecx
// (`mov cx, word ptr [0x1012858a]` 0x1002c55b, `adc esi, ecx` 0x1002c5b7) and writes 0x10128580 nowhere.
int __fastcall DrawBcf0_re(void* self, void*, void* dst, int x, int y, int table) {
    return DrawFamily(self, dst, x, y, PreNone, table, 0x128548, false, OpPlaneLow());
}

// 0x0002d890  Sprite::draw16_1002d890(dst, x, y, flag)
// The flag is read from its argument slot inside the loop, one byte at a time (`or byte ptr [ebp + 0x14], 0`
// at 0x1002dbb6), so it is a char and the upper three bytes of the slot never matter.
int __fastcall DrawD890_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpAverage op;
    op.flag = flag;
    return DrawFamily(self, dst, x, y, PreTableUnlock, 0, 0, true, op);
}

}  // namespace

SG_HOOK("jgld.dll", 0x0001cde0, Sprite_draw16_1001cde0, DrawCde0_re, DrawCde0_orig);
SG_HOOK("jgld.dll", 0x00056450, Sprite_draw16_10056450, Draw56450_re, Draw56450_orig);
SG_HOOK("jgld.dll", 0x000576a0, Sprite_draw16_100576a0, Draw576a0_re, Draw576a0_orig);
SG_HOOK("jgld.dll", 0x0001f2d0, Sprite_draw_1001f2d0, DrawF2d0_re, DrawF2d0_orig);
SG_HOOK("jgld.dll", 0x00026240, Sprite_draw16_10026240, Draw26240_re, Draw26240_orig);
SG_HOOK("jgld.dll", 0x00027430, Sprite_draw16_10027430, Draw27430_re, Draw27430_orig);
SG_HOOK("jgld.dll", 0x00028630, Sprite_draw16_10028630, Draw28630_re, Draw28630_orig);
SG_HOOK("jgld.dll", 0x000298b0, Sprite_draw16_100298b0, Draw298b0_re, Draw298b0_orig);
SG_HOOK("jgld.dll", 0x0002bcf0, Sprite_draw_1002bcf0, DrawBcf0_re, DrawBcf0_orig);
SG_HOOK("jgld.dll", 0x0002d890, Sprite_draw16_1002d890, DrawD890_re, DrawD890_orig);
