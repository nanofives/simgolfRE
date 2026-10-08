// Batch c3bu: jgld.dll render, the ten Sprite draw rows left over from batch c3bp (round 18 of
// log/c3_brief.md; the hand-off is log/c3/c3bp_notes.txt, "Candidates not taken").
//   0x0002ed60  Sprite::draw16_1002ed60  Palette colour at 1/4 against the destination, RGB555 fields
//   0x00030290  Sprite::draw16_10030290  destination at 1/4 against the Palette colour, RGB555 fields
//   0x00041f80  Sprite::draw16_10041f80  Palette colour at 1/2, RGB565 fields, result NOT shifted back
//   0x00043430  Sprite::draw16_10043430  Palette colour at 1/4, RGB565 fields, result NOT shifted back
//   0x00044940  Sprite::draw16_10044940  destination at 1/4, RGB565 fields, result NOT shifted back
//   0x00045e50  Sprite::draw16_10045e50  Palette colour at 1/8, RGB565 fields, result NOT shifted back
//   0x000473b0  Sprite::draw16_100473b0  destination at 1/8, RGB565 fields, result NOT shifted back
//   0x00048910  Sprite::draw16_10048910  destination at 15/16, RGB565 fields, result NOT shifted back
//   0x00020430  Sprite::draw16_10020430  two tables: an index remap, and payload 0xf8 remapping the pixel
//   0x00062dd0  Sprite::draw_10062dd0    the same pair of tables onto an 8-BIT destination
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below is a
// VA, as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts, so these
// hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// The first nine share ONE skeleton with the ten functions of batch c3bp: guard, Palette, the two scale
// ratios, the two pitches, the format switch, then `abs(0x10122dc0) == abs(0x10122dc8) &&
// abs(0x10122dc4) == abs(0x10122dc8)` choosing an unscaled half from a 16.16 stepped half, and inside each
// half the SIGNS of 0x10122dc0 and 0x10122dc4 choosing one of four mirror bodies: eight pixel loops per
// function. A normalized whole-function diff of the eight blend members against 0x1002d890 (the member
// batch c3bp took, shim/src/re/c3bp.cpp) leaves no changed line outside the pixel operation, and the eight
// pixel loops of each one carry the SAME operation, so the skeleton is written once here (DrawFamily16,
// with 0x1002ed60's instruction addresses) and each function supplies its pixel operation.
// 0x00062dd0 does NOT share it: it writes an 8-bit destination through a second, parallel block of module
// globals and has no Palette and no format switch at all, so it gets its own skeleton (DrawFamily8, with
// 0x10062dd0's instruction addresses).
//
// Everything the A/B hands these functions is built by re/frida/js/fixtures.d/c3bu.js on jgld's own Surface
// (0x1011d0b0), Palette (0x1011db10) and Sprite (0x1011d380) vtables: never the live Display at 0x10128420,
// never a screen surface, never DirectDraw. log/c3/alt/c3bu_probe.py, one 12 s boot at the menu with counters
// attached straight to the module addresses: 0 entries into all ten of these functions and into each of their
// static callers (0x18920, 0x172b0, 0x16fc0), against the live twin Sprite::draw16_1001bc40 0x1bc40 and its
// caller 0x15480, both on the process's first thread.
//
// THE SHARED LOOP GLOBALS. The nine 16-bit members park their loop state in 0x1012857c (source pitch),
// 0x1012855c (destination pitch), 0x1012856c (colour table), 0x10128578 (rows), 0x10128564 (columns),
// 0x1012853c (source row skip) and 0x10128560 (destination row skip), the same block the live 0x1bc40 writes;
// 0x62dd0 uses the parallel 8-bit block 0x10128678, 0x10128644, 0x10128658, 0x10128670, 0x10128650,
// 0x1012861c and 0x10128648. Every registry key of this batch therefore runs with the game's first thread
// suspended and restores both blocks before resuming it (fixture c3bu.js, the recipe c3av established for
// 0x1012847c and c3bm and c3bp extended). The row counters at 0x10128578 and 0x10128670 are decremented IN
// MEMORY by the loops below, exactly as `dec dword ptr [0x10128578]` (0x1002f0fb) does.
//
// Surface vtable slots used here (0x1011d0b0, named in re/names/jgld_2.tsv):
//   +0x10  lock()      0x10008730: copies the base at this+0x4c0 to this+0x4cc, bumps the use count at
//          this+0x4c8 when that copy is not NULL, and returns the COPY, so a surface with no pixel base
//          makes the guard return 3.
//   +0x18  bits8()     0x1000a750: tail-calls slot +0x10 (0x1000a777).
//   +0x20  bits()      0x1000a7b0: tail-calls slot +0x10 (0x1000a7d7).
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
inline void* vBits8(void* s) { return reinterpret_cast<PtrSlot_t>(slot(s, 0x18))(s, 0); }
inline void* vBits(void* s) { return reinterpret_cast<PtrSlot_t>(slot(s, 0x20))(s, 0); }
inline void vUnlock(void* s, int f) { reinterpret_cast<UnlockSlot_t>(slot(s, 0x24))(s, 0, f); }
inline RECT* vClip(void* s) { return reinterpret_cast<RectSlot_t>(slot(s, 0xcc))(s, 0); }
inline int vPitch(void* s) { return reinterpret_cast<IntSlot_t>(slot(s, 0xe0))(s, 0); }
inline int* vDepthPtr(void* s) { return reinterpret_cast<IntPtrSlot_t>(slot(s, 0xe4))(s, 0); }
inline unsigned short* vTable565(void* p) { return reinterpret_cast<TableSlot_t>(slot(p, 0x18))(p, 0); }
inline unsigned short* vTable555(void* p) { return reinterpret_cast<TableSlot_t>(slot(p, 0x1c))(p, 0); }

// The module's own helpers, called through their module addresses so the comparison is of this code and not
// of a copy: setRect 0x10008360 (through thunk 0x10001019, 0x1002ef55 and 0x10062f09), intersect 0x10008590
// (thunk 0x10001087, 0x1002f536 and 0x100633ca), the CRT abs 0x1007f370 (0x1002ee00 and 0x10062e24).
// IntersectRect is called through jgld's OWN import slot at 0x1012c5e4, the one the originals call
// (0x1002ef97 and 0x10062f4b).
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
inline int* GpInt(unsigned rva) { return *reinterpret_cast<int**>(JgldBase() + rva); }

inline int SpriteInt(void* s, unsigned off) { return *reinterpret_cast<int*>(reinterpret_cast<char*>(s) + off); }
inline unsigned char* SpriteBits(void* s) {
    return *reinterpret_cast<unsigned char**>(reinterpret_cast<char*>(s) + 0x14);
}
// The Palette of the sprite at +0x10, or the one of the client object the module global 0x1012873c points at
// (0x1002edb7 / 0x1002edc8 and the twins in the nine 16-bit members).
inline void* SpritePalette(void* s) {
    void* p = *reinterpret_cast<void**>(reinterpret_cast<char*>(s) + 0x10);
    if (p) return p;
    char* client = *reinterpret_cast<char**>(JgldBase() + 0x12873c);
    return *reinterpret_cast<void**>(client + 4);
}

// VC6 converts a double to an int with __ftol (jgld's own copy is 0x1007f12c, called at 0x1002f4b8,
// 0x1002f4ca, 0x1002f4d6 and 0x1002f4e1): a 64-bit fistp with the low dword kept, i.e. truncation.
inline int Ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// The colour table parked at 0x1012856c, as the 16-bit loops read it (`mov ecx, dword ptr [0x1012856c]`,
// 0x1002f075). The 8-bit member reads ITS table from 0x10128658 instead and never writes it (0x10063023).
inline const unsigned short* Tbl() { return reinterpret_cast<const unsigned short*>(G(0x12856c)); }
inline const unsigned char* Tbl8() { return reinterpret_cast<const unsigned char*>(G(0x128658)); }

// ------------------------------------------------------------------ the 16-bit skeleton

// What the preamble does between the two pitches and the scale test, which is the only part of the skeleton
// that is not identical across the nine 16-bit members.
enum PreKind {
    PreTableUnlock,   // format switch -> 0x1012856c; the switch default UNLOCKS and returns 1 (0x1002eecd)
    PreTableKeep      // the same switch whose default returns 1 WITHOUT unlocking (0x100205ae)
};

// The unscaled half of the four mirror bodies, which begin at 0x1002efdf, 0x1002f10f, 0x1002f24c and
// 0x1002f37c in 0x1002ed60. `dword ptr [0x10128578]` is the remaining row count, decremented in memory.
//   colStep  `inc esi` (0x1002f0e8) when 0x10122dc0 >= 0, `dec esi` when it is negative
//   rowAdd   `add esi, [0x1012853c]` (0x1002f0ef) when 0x10122dc4 >= 0, `sub esi` when negative
template <class Op>
void UnscaledLoop16(const unsigned char* s, unsigned short* d, int colStep, bool rowAdd, const Op& op) {
    for (;;) {
        int cols = G(0x128564);                                                   // 0x1002f07b
        for (;;) {
            op(s, d);                                                             // 0x1002f081..0x1002f0e2
            s += colStep;                                                         // 0x1002f0e8
            ++d;                                                                  // 0x1002f0e9
            if (--cols == 0) break;                                               // 0x1002f0ec / 0x1002f0ed
        }
        if (rowAdd) s += G(0x12853c); else s -= G(0x12853c);                       // 0x1002f0ef
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x1002f0f5
        if (--G(0x128578) == 0) break;                                            // 0x1002f0fb / 0x1002f101
    }
}

// The 16.16 stepped half of the four mirror bodies (the first begins at 0x1002f773). edx carries the
// fractional vertical position in its high half and the fractional horizontal one in its low half
// (0x1002f720..0x1002f729); ebx carries the remaining column count minus one in its high half and the
// fractional horizontal step in its low half, so `sub ebx, 0x10000` + `jns` runs the row exactly `cols`
// times. The source cursor takes the INTEGER part of the horizontal step plus the carry out of the
// fractional add (`add dx, bx` + `adc esi`), and at the end of a row the carry out of the vertical add
// selects one of the two ints the module pointer at 0x10122e20 addresses (`sbb eax, eax`,
// `add esi, [ebx + eax*4]`).
//   stepX     the integer horizontal step the preamble parked at 0x10128580 (0x1002f759)
//   fwd       `adc esi` when 0x10122dc0 >= 0, `sbb esi` when it is negative
template <class Op>
void ScaledLoop16(const unsigned char* s, unsigned short* d, int stepX, bool fwd, const Op& op) {
    const int* const rowStep = GpInt(0x122e20);
    const unsigned fracStepX = static_cast<unsigned short>(G(0x128588));
    const unsigned fracStepY = static_cast<unsigned>(G(0x128544));
    unsigned acc = (static_cast<unsigned>(G(0x128558)) << 16) |
                   static_cast<unsigned short>(G(0x128554));                      // 0x1002f720..0x1002f729
    for (;;) {
        int ebx = static_cast<int>(static_cast<unsigned>(G(0x128564)) | fracStepX);
        acc = (acc & 0xffff0000u) | static_cast<unsigned short>(G(0x128554));
        for (;;) {
            op(s, d);
            const unsigned lo = (acc & 0xffff) + (static_cast<unsigned>(ebx) & 0xffff);
            acc = (acc & 0xffff0000u) | (lo & 0xffff);
            const int carry = static_cast<int>(lo >> 16);
            if (fwd) s += stepX + carry; else s -= stepX + carry;
            ++d;
            ebx -= 0x10000;
            if (ebx < 0) break;
        }
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));
        const unsigned long long sum =
            static_cast<unsigned long long>(acc) + fracStepY;
        acc = static_cast<unsigned>(sum);
        const int sel = (sum >> 32) ? -1 : 0;
        s += rowStep[sel];
        if (--G(0x128578) == 0) break;
    }
}

// The whole 16-bit function, from the guard to the final unlock. `param4` / `param5` are the fourth and
// fifth arguments of the member that takes them, stored into the module globals `rva4` / `rva5` before the
// format switch (0 where the member takes none; the eight blend members pass 0 and 0, 0x20430 passes
// 0x128548 and 0x128574).
template <class Op>
int DrawFamily16(void* self, void* dst, int x, int y, PreKind pre,
                 int param4, unsigned rva4, int param5, unsigned rva5, const Op& op) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;  // 0x1002ed86 / 0x1002ed8c / 0x1002eda6, 0x1002edaa
    void* const pal = SpritePalette(self);                                        // 0x1002edbb / 0x1002edc8
    if (pal == 0) {                                                               // 0x1002edd3
        vUnlock(dst, 1);                                                          // 0x1002ede5
        return 16;                                                                // 0x1002edef
    }
    // Both ratios are `fild` of the CRT abs of a numerator, `fidiv` by the denominator at 0x10122dc8 and a
    // `fstp dword` (0x1002ee0b..0x1002ee14 and 0x1002ee2c..0x1002ee35). The quotient is formed at the x87
    // precision the A/B thread runs under and rounded to float only by the store, so each operand is widened
    // to double and the result assigned to a float: the form batch c3ap established.
    Gf(0x12858c) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc0))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1002ee14
    Gf(0x128590) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc4))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1002ee35
    G(0x12857c) = SpriteInt(self, 0x2c);                                          // 0x1002ee41
    G(0x12855c) = vPitch(dst);                                                    // 0x1002ee5e
    if (rva4 != 0) G(rva4) = param4;                                              // 0x1002053f (0x20430 only)
    if (rva5 != 0) G(rva5) = param5;                                              // 0x10020547 (0x20430 only)
    {
        const int fmt = vDepthPtr(dst)[1];                                        // 0x1002ee83
        if (fmt == 0) {                                                           // 0x1002ee89 / 0x1002ee8d
            G(0x12856c) = reinterpret_cast<int>(vTable565(pal));                  // 0x1002eeab
        } else if (fmt == 1) {                                                    // 0x1002ee8f / 0x1002ee93
            G(0x12856c) = reinterpret_cast<int>(vTable555(pal));                  // 0x1002eec6
        } else {
            if (pre == PreTableUnlock) vUnlock(dst, 1);                           // 0x1002eed9
            return 1;                                                             // 0x1002eee3 / 0x100205ae
        }
    }
    const bool negX = G(0x122dc0) < 0;
    const bool negY = G(0x122dc4) < 0;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1002ef0c / 0x1002ef0e
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1002ef33 / 0x1002ef35
        // ---- the unscaled half (begins at 0x1002ef3b)
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x1002ef55
        orig = rc;                                                                // 0x1002ef5d..0x1002ef72
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x1002ef97 / 0x1002efa6
            vUnlock(dst, 1);                                                      // 0x1002efb4
            return 0;                                                             // 0x1002efbe
        }
        // The four mirror bodies, chosen at 0x1002efc5/0x1002efcc, 0x1002efd2/0x1002efd9 and the second sign
        // test of the mirrored half (0x1002f23f/0x1002f246), compute the same two cursors and the same row
        // and column counts; they differ in the sign of the source row skip and in the column direction.
        const unsigned char* const s = bits + (rc.left - orig.left) +
                                       (rc.top - orig.top) * G(0x12857c);         // 0x1002efdf..0x1002effc
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x1002f009..0x1002f026
        G(0x128578) = rc.bottom - rc.top;                                         // 0x1002f02f
        G(0x128564) = rc.right - rc.left;                                         // 0x1002f03b
        // pitch - cols where the two numerators have the same sign (0x1002f040..0x1002f04c), pitch + cols
        // where they differ
        G(0x12853c) = (negX == negY) ? G(0x12857c) - G(0x128564)
                                     : G(0x12857c) + G(0x128564);
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x1002f052..0x1002f060
        UnscaledLoop16(s, d, negX ? -1 : 1, !negY, op);
    } else {
        // ---- the 16.16 stepped half (begins at 0x1002f4ac)
        const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                            static_cast<double>(Gf(0x128590)));                   // 0x1002f4af..0x1002f4b8
        const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                            static_cast<double>(Gf(0x12858c)));                   // 0x1002f4c1..0x1002f4ca
        const int oy = Ftol(static_cast<double>(Gf(0x128590)));                   // 0x1002f4d0 / 0x1002f4d6
        const int ox = Ftol(static_cast<double>(Gf(0x12858c)));                   // 0x1002f4e1 / 0x1002f4e7
        RECT rc;
        RECT orig;
        jSetRect(&rc, x + ox, y + oy, sw, sh);                                    // 0x1002f4f6
        orig = rc;                                                                // 0x1002f4fe..0x1002f513
        if (!jIntersect(&rc, &rc, vClip(dst))) {                                  // 0x1002f536 / 0x1002f53e
            vUnlock(dst, 1);                                                      // 0x1002f54e
            return 0;                                                             // 0x1002f558
        }
        // The destination cursor is formed with the pitch the preamble parked, BEFORE the pitch is read again
        // into the same global at 0x1002f5a0.
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x1002f569..0x1002f586
        G(0x12855c) = vPitch(dst);                                                // 0x1002f5a0
        G(0x128564) = rc.right - rc.left;                                         // 0x1002f5ab
        G(0x128578) = rc.bottom - rc.top;                                         // 0x1002f5b7
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x1002f5bd..0x1002f5ca
        G(0x128588) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                    // 0x1002f5cf..0x1002f5ee
        G(0x128540) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                    // 0x1002f5f3..0x1002f612
        const int offX = rc.left - orig.left;                                     // 0x1002f631..0x1002f637
        const int offY = rc.top - orig.top;                                       // 0x1002f63a..0x1002f640
        G(0x128554) = G(0x128588) * offX;                                         // 0x1002f643..0x1002f64d
        G(0x128558) = G(0x128540) * offY;                                         // 0x1002f653..0x1002f65d
        G(0x128570) = ((G(0x128554) & 0xffff) + G(0x128588) * G(0x128564)) >> 16; // 0x1002f663..0x1002f67f
        G(0x128584) = ((G(0x128558) & 0xffff) + G(0x128540) * G(0x128578)) >> 16; // 0x1002f684..0x1002f6a1
        const int pitch = G(0x12857c);
        const int w = SpriteInt(self, 0x30);
        const int h = SpriteInt(self, 0x34);
        const unsigned char* s;
        int* const rowStep = GpInt(0x122e20);                                     // 0x1002f6fa
        // The four mirror bodies (0x1002f631, 0x1002f82f, 0x1002fa2b and 0x1002fc1a) are chosen at
        // 0x1002f617/0x1002f61e, 0x1002f624/0x1002f62b and 0x1002fa1e/0x1002fa25.
        if (!negX && !negY) {
            G(0x12853c) = pitch - G(0x128570);                                    // 0x1002f6a7..0x1002f6b3
            s = bits + (G(0x128558) >> 16) * pitch + (G(0x128554) >> 16);         // 0x1002f6b9..0x1002f6dc
            rowStep[0] = (G(0x128540) >> 16) * pitch - pitch + G(0x12853c);       // 0x1002f6df..0x1002f700
        } else if (!negX && negY) {
            // the vertical mirror starts on the LAST payload row and still steps the source cursor FORWARD,
            // both inside the row and from row to row (c3bp's finding 2)
            s = bits + (h - 1 - (G(0x128558) >> 16)) * pitch + (G(0x128554) >> 16);
            rowStep[0] = (G(0x128540) >> 16) * pitch + G(0x128570);
        } else if (negX && !negY) {
            s = bits + (G(0x128558) >> 16) * pitch + w - 1 - (G(0x128554) >> 16);
            rowStep[0] = (G(0x128540) >> 16) * pitch + G(0x128570);
        } else {
            s = bits + (h - 1 - (G(0x128558) >> 16)) * pitch + w - 1 -
                (G(0x128554) >> 16);
            G(0x12853c) = -pitch + G(0x128570);
            rowStep[0] = (G(0x128540) >> 16) * pitch - G(0x128570);
        }
        *GpInt(0x122e1c) = rowStep[0] + pitch;                                    // 0x1002f702..0x1002f716
        G(0x128564) = (G(0x128564) - 1) << 16;                                    // 0x1002f740..0x1002f74a
        const int stepX = static_cast<unsigned short>(G(0x128588) >> 16);         // 0x1002f752 word [0x1012858a]
        G(0x128580) = stepX;                                                      // 0x1002f759
        G(0x128544) = G(0x128540) << 16;                                          // 0x1002f75f..0x1002f767
        ScaledLoop16(s, d, stepX, !negX, op);
    }
    vUnlock(dst, 2);                                                              // 0x1002fe30
    return 0;                                                                     // 0x1002fe3a
}

// ------------------------------------------------------------------ the blend sub-family's pixel operation

// The averaging step the eight blend members run, in the register roles their disassembly uses: `a` is the
// 16-bit value loaded into eax and `b` the one loaded into edx; each is shifted up by four so that the two
// field groups (`rbMask`, `gMask`) can be added without carrying into each other, the pair
// `add eax, ecx / add ebx, edx / shr eax, 1 / shr ebx, 1` is run `rounds` times against the UNCHANGED edx
// fields, and only then are the two groups re-masked and recombined. `a` therefore ends with weight
// 1 / 2^rounds and `b` with the rest.
// Addresses are 0x1002ed60's, the first of the eight (0x1002f0a1..0x1002f0e2).
inline unsigned short BlendRounds(unsigned short a, unsigned short b, int rounds,
                                  unsigned rbMask, unsigned gMask, bool shiftBack) {
    const unsigned x = static_cast<unsigned>(a) << 4;                             // 0x1002f0a1
    const unsigned y = static_cast<unsigned>(b) << 4;                             // 0x1002f0a4
    unsigned arb = x & rbMask;                                                    // 0x1002f0ab
    unsigned ag = x & gMask;                                                      // 0x1002f0b0
    const unsigned brb = y & rbMask;                                              // 0x1002f0b6
    const unsigned bg = y & gMask;                                                // 0x1002f0bc
    for (int i = 0; i < rounds; ++i) {
        arb = (arb + brb) >> 1;                                                   // 0x1002f0c2 / 0x1002f0c6
        ag = (ag + bg) >> 1;                                                      // 0x1002f0c4 / 0x1002f0c8
    }
    arb &= rbMask;                                                                // 0x1002f0d2
    ag &= gMask;                                                                  // 0x1002f0d7
    const unsigned out = arb + ag;                                                // 0x1002f0dd
    // The four RGB555 members shift the result back down (`shr eax, 4`, 0x1002f0df); the six RGB565 ones do
    // NOT (0x100422f5 stores `ax` straight after the add), so what they write is the blend still shifted up
    // by four and truncated to sixteen bits.
    return static_cast<unsigned short>(shiftBack ? (out >> 4) : out);             // 0x1002f0e2
}

// The 15:1 weighting of 0x10048910 only: eax is multiplied by fifteen (`mov esi, eax; shl eax, 4;
// sub eax, esi`, 0x10048c73..0x10048c78), edx added once (0x10048c81), and the sum shifted down by four
// (0x10048c85). `a` therefore ends with weight 15/16 and `b` with 1/16.
inline unsigned short Blend15to1(unsigned short a, unsigned short b) {
    const unsigned x = static_cast<unsigned>(a) << 4;
    const unsigned y = static_cast<unsigned>(b) << 4;
    unsigned arb = x & 0xf81f0;
    unsigned ag = x & 0x7e00;
    const unsigned brb = y & 0xf81f0;
    const unsigned bg = y & 0x7e00;
    arb = (((arb << 4) - arb) + brb) >> 4;
    ag = (((ag << 4) - ag) + bg) >> 4;
    arb &= 0xf81f0;
    ag &= 0x7e00;
    return static_cast<unsigned short>(arb + ag);   // no `shr eax, 4`: the 565 members store `ax` as it is
}

// The gate the eight blend members share, with 0x1002ed60's addresses: payload bytes from 0xff up are always
// skipped (0x1002f081), and a non-zero fourth argument (read one byte at a time out of its argument slot,
// `or byte ptr [ebp + 0x14], 0` at 0x1002f086) skips 0xf8..0xfe as well (0x1002f08c).
// `rounds`, `rb`, `g`, `shiftBack` and `swap` are what distinguishes the eight; `swap` says whether the
// PALETTE colour is the one loaded into eax (`mov al, byte ptr [esi]`) or into edx (`mov dl, ...`).
struct OpBlend {
    unsigned char flag;
    int rounds;
    unsigned rb;
    unsigned g;
    bool shiftBack;
    bool swap;
    void operator()(const unsigned char* s, unsigned short* d) const {
        if (*s < 0xff) {                                                          // 0x1002f081 / 0x1002f084
            if (flag != 0 && *s >= 0xf8) return;                                  // 0x1002f086..0x1002f08f
            const unsigned short colour = Tbl()[*s];                              // 0x1002f098 / 0x1002f09d
            *d = swap ? BlendRounds(*d, colour, rounds, rb, g, shiftBack)
                      : BlendRounds(colour, *d, rounds, rb, g, shiftBack);
        }
    }
};

// 0x10048910's gate is the same; only the blend differs.
struct OpBlend15 {
    unsigned char flag;
    void operator()(const unsigned char* s, unsigned short* d) const {
        if (*s < 0xff) {                                                          // 0x10048c31 / 0x10048c34
            if (flag != 0 && *s >= 0xf8) return;                                  // 0x10048c36..0x10048c3f
            // `mov dl, byte ptr [esi]` / `mov ax, word ptr [edi]` (0x10048c48 / 0x10048c4a) put the
            // DESTINATION pixel in eax, so the destination is the one weighted 15/16.
            *d = Blend15to1(*d, Tbl()[*s]);                                       // 0x10048c4d..0x10048c99
        }
    }
};

// 0x10020430: payload bytes from 0xfe up are skipped (0x1002074c), 0xf8 remaps the DESTINATION pixel through
// the 16-bit table the fifth argument parked at 0x10128574 (0x10020756..0x10020764), and every other byte
// indexes the 256-byte table the fourth argument parked at 0x10128548, whose result indexes the Palette
// table (0x10020769..0x10020779).
struct OpTwoTables16 {
    void operator()(const unsigned char* s, unsigned short* d) const {
        if (*s < 0xfe) {                                                          // 0x1002074c / 0x1002074f
            if (*s == 0xf8) {                                                     // 0x10020751 / 0x10020754
                const unsigned short* const dmap =
                    reinterpret_cast<const unsigned short*>(G(0x128574));         // 0x10020756
                *d = dmap[*d];                                                    // 0x1002075c / 0x1002075f
            } else {
                const unsigned char* const imap =
                    reinterpret_cast<const unsigned char*>(G(0x128548));          // 0x10020769
                *d = Tbl()[imap[*s]];                                             // 0x1002076f..0x10020779
            }
        }
    }
};

// ------------------------------------------------------------------ the 8-bit skeleton (0x10062dd0 only)

// 0x10062dd0's pixel operation, the 8-bit twin of OpTwoTables16: the destination remap at 0x1012866c and the
// index remap at 0x1012862c are both 256-byte tables, and the colour table at 0x10128658 is a 256-byte table
// the function READS and never writes (0x10063023).
inline void Op8(const unsigned char* s, unsigned char* d) {
    if (*s < 0xfe) {                                                              // 0x1006302f / 0x10063032
        // The eight bodies' own addresses differ; the stepped half's first copy is 0x10063617.
        if (*s == 0xf8) {                                                         // 0x10063034 / 0x10063037
            const unsigned char* const dmap =
                reinterpret_cast<const unsigned char*>(G(0x12866c));              // 0x10063039
            *d = dmap[*d];                                                        // 0x1006303f / 0x10063041
        } else {
            const unsigned char* const imap =
                reinterpret_cast<const unsigned char*>(G(0x12862c));              // 0x10063049
            *d = Tbl8()[imap[*s]];                                                // 0x1006304f..0x10063058
        }
    }
}

// The unscaled half of 0x10062dd0's four mirror bodies (which begin at 0x10062f93, 0x1006307b, 0x10063170
// and 0x10063258). The destination advances ONE byte per pixel (`inc edi`, 0x1006305b) and by 0x10128648
// per row (0x10063065).
inline void UnscaledLoop8(const unsigned char* s, unsigned char* d, int colStep, bool rowAdd) {
    for (;;) {
        int cols = G(0x128650);                                                   // 0x10063029
        for (;;) {
            Op8(s, d);                                                            // 0x1006302f..0x10063058
            s += colStep;                                                         // 0x1006305a
            ++d;                                                                  // 0x1006305b
            if (--cols == 0) break;                                               // 0x1006305c / 0x1006305d
        }
        if (rowAdd) s += G(0x12861c); else s -= G(0x12861c);                       // 0x1006305f
        d += G(0x128648);                                                         // 0x10063065
        if (--G(0x128670) == 0) break;                                            // 0x1006306b / 0x10063071
    }
}

// The 16.16 stepped half of 0x10062dd0 (the first body is 0x10063601..0x10063673); the same arithmetic as
// ScaledLoop16 on the 8-bit block: integer step at 0x1012867c (0x10063647), destination row skip at
// 0x10128648 (0x10063656), vertical step at 0x10128628 (0x1006365c), the row-step pair the module pointer
// at 0x10122e58 addresses (0x10063664 / 0x1006366a) and the row counter at 0x10128670 (0x1006366d).
inline void ScaledLoop8(const unsigned char* s, unsigned char* d, int stepX, bool fwd) {
    const int* const rowStep = GpInt(0x122e58);                                   // 0x10063664
    const unsigned fracStepX = static_cast<unsigned short>(G(0x128688));          // 0x10063609
    const unsigned fracStepY = static_cast<unsigned>(G(0x128628));                // 0x1006365c
    unsigned acc = (static_cast<unsigned>(G(0x128640)) << 16) |
                   static_cast<unsigned short>(G(0x12863c));                      // 0x100635ae..0x100635b7
    for (;;) {
        int ebx = static_cast<int>(static_cast<unsigned>(G(0x128650)) | fracStepX);  // 0x10063603/0x10063609
        acc = (acc & 0xffff0000u) | static_cast<unsigned short>(G(0x12863c));     // 0x10063610
        for (;;) {
            Op8(s, d);
            const unsigned lo = (acc & 0xffff) + (static_cast<unsigned>(ebx) & 0xffff);  // 0x10063644
            acc = (acc & 0xffff0000u) | (lo & 0xffff);
            const int carry = static_cast<int>(lo >> 16);
            if (fwd) s += stepX + carry; else s -= stepX + carry;                 // 0x10063647
            ++d;                                                                  // 0x1006364d
            ebx -= 0x10000;                                                       // 0x1006364e
            if (ebx < 0) break;                                                   // 0x10063654
        }
        d += G(0x128648);                                                         // 0x10063656
        const unsigned long long sum =
            static_cast<unsigned long long>(acc) + fracStepY;                     // 0x1006365c
        acc = static_cast<unsigned>(sum);
        const int sel = (sum >> 32) ? -1 : 0;                                     // 0x10063662
        s += rowStep[sel];                                                        // 0x1006366a
        if (--G(0x128670) == 0) break;                                            // 0x1006366d / 0x10063673
    }
}

// ------------------------------------------------------------------ the hooks

// The MinHook trampolines the A/B's "original" arm is called through (one per SG_HOOK below).
typedef int(__fastcall* Draw4c_t)(void*, void*, void*, int, int, unsigned char);
typedef int(__fastcall* Draw5_t)(void*, void*, void*, int, int, int, int);
Draw4c_t DrawEd60_orig;
Draw4c_t Draw30290_orig;
Draw4c_t Draw41f80_orig;
Draw4c_t Draw43430_orig;
Draw4c_t Draw44940_orig;
Draw4c_t Draw45e50_orig;
Draw4c_t Draw473b0_orig;
Draw4c_t Draw48910_orig;
Draw5_t Draw20430_orig;
Draw5_t Draw62dd0_orig;

// The fourth argument of the eight blend members is a char read from its argument slot one byte at a time
// (`or byte ptr [ebp + 0x14], 0`), so the upper three bytes of the slot never matter.

// 0x0002ed60  Sprite::draw16_1002ed60(dst, x, y, flag)
// Two averaging rounds on the RGB555 field layout with the result shifted back down: the Palette colour
// carries a quarter and the destination three quarters (0x1002f0c2..0x1002f0e2).
int __fastcall DrawEd60_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 2;
    op.rb = 0x7c1f0;
    op.g = 0x3e00;
    op.shiftBack = true;
    op.swap = false;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00030290  Sprite::draw16_10030290(dst, x, y, flag)
// 0x1002ed60 with the two colours exchanged: `mov dl, byte ptr [esi]` / `mov ax, word ptr [edi]` /
// `mov dx, word ptr [ecx + edx*2]` at 0x100305c8..0x100305cd put the DESTINATION pixel in eax, so it is the
// destination that carries a quarter and the Palette colour three quarters.
int __fastcall Draw30290_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 2;
    op.rb = 0x7c1f0;
    op.g = 0x3e00;
    op.shiftBack = true;
    op.swap = true;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00041f80  Sprite::draw16_10041f80(dst, x, y, flag)
// One averaging round on the RGB565 field layout (masks 0xf81f0 and 0x7e00 at 0x100422cb and 0x100422d0),
// and the result is stored WITHOUT the `shr eax, 4` the RGB555 members end with (0x100422f5 / 0x100422f7).
int __fastcall Draw41f80_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 1;
    op.rb = 0xf81f0;
    op.g = 0x7e00;
    op.shiftBack = false;
    op.swap = false;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00043430  Sprite::draw16_10043430(dst, x, y, flag)
int __fastcall Draw43430_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 2;
    op.rb = 0xf81f0;
    op.g = 0x7e00;
    op.shiftBack = false;
    op.swap = false;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00044940  Sprite::draw16_10044940(dst, x, y, flag)
int __fastcall Draw44940_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 2;
    op.rb = 0xf81f0;
    op.g = 0x7e00;
    op.shiftBack = false;
    op.swap = true;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00045e50  Sprite::draw16_10045e50(dst, x, y, flag)
int __fastcall Draw45e50_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 3;
    op.rb = 0xf81f0;
    op.g = 0x7e00;
    op.shiftBack = false;
    op.swap = false;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x000473b0  Sprite::draw16_100473b0(dst, x, y, flag)
int __fastcall Draw473b0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend op;
    op.flag = flag;
    op.rounds = 3;
    op.rb = 0xf81f0;
    op.g = 0x7e00;
    op.shiftBack = false;
    op.swap = true;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00048910  Sprite::draw16_10048910(dst, x, y, flag)
int __fastcall Draw48910_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    OpBlend15 op;
    op.flag = flag;
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00020430  Sprite::draw16_10020430(dst, x, y, indexMap, pixelMap)
// The only member of the nine with two extra arguments: they are stored at 0x10128548 (0x1002053f) and
// 0x10128574 (0x10020547) before the format switch, whose default returns 1 WITHOUT unlocking (0x100205ae).
int __fastcall Draw20430_re(void* self, void*, void* dst, int x, int y, int indexMap, int pixelMap) {
    return DrawFamily16(self, dst, x, y, PreTableKeep, indexMap, 0x128548, pixelMap, 0x128574,
                        OpTwoTables16());
}

// 0x00062dd0  Sprite::draw_10062dd0(dst, x, y, indexMap, pixelMap)
// The 8-bit-destination branch of the family: no Palette and no format switch at all (the guard at
// 0x10062df0 is followed straight by the two ratios at 0x10062e38 / 0x10062e59), the destination comes from
// slot +0x18 instead of +0x20 (0x10062fbd) and every loop global is the parallel 8-bit one.
int __fastcall Draw62dd0_re(void* self, void*, void* dst, int x, int y, int indexMap, int pixelMap) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;  // 0x10062df0 / 0x10062df6 / 0x10062e10, 0x10062e14
    Gf(0x12868c) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc0))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10062e38
    Gf(0x128690) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc4))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10062e59
    G(0x128678) = SpriteInt(self, 0x2c);                                          // 0x10062e65
    G(0x128644) = vPitch(dst);                                                    // 0x10062e81
    G(0x12862c) = indexMap;                                                       // 0x10062e92
    G(0x12866c) = pixelMap;                                                       // 0x10062e9b
    const bool negX = G(0x122dc0) < 0;
    const bool negY = G(0x122dc4) < 0;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x10062ec0 / 0x10062ec2
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x10062ee7 / 0x10062ee9
        // ---- the unscaled half (begins at 0x10062eef)
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x10062f09
        orig = rc;                                                                // 0x10062f11..0x10062f26
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x10062f4b / 0x10062f58
            vUnlock(dst, 1);                                                      // 0x10062f68
            return 0;                                                             // 0x10062f72
        }
        const unsigned char* const s = bits + (rc.left - orig.left) +
                                       (rc.top - orig.top) * G(0x128678);         // 0x10062f93..0x10062fb0
        unsigned char* const d = reinterpret_cast<unsigned char*>(vBits8(dst)) +
                                 rc.left + rc.top * G(0x128644);                  // 0x10062fbd..0x10062fd6
        G(0x128670) = rc.bottom - rc.top;                                         // 0x10062fdf
        G(0x128650) = rc.right - rc.left;                                         // 0x10062feb
        G(0x12861c) = (negX == negY) ? G(0x128678) - G(0x128650)                  // 0x10062ff0..0x10062ffc
                                     : G(0x128678) + G(0x128650);
        G(0x128648) = G(0x128644) - G(0x128650);                                  // 0x10063002..0x1006300e
        UnscaledLoop8(s, d, negX ? -1 : 1, !negY);
    } else {
        // ---- the 16.16 stepped half (begins at 0x10063340)
        const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                            static_cast<double>(Gf(0x128690)));                   // 0x10063343..0x1006334c
        const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                            static_cast<double>(Gf(0x12868c)));                   // 0x10063355..0x1006335e
        const int oy = Ftol(static_cast<double>(Gf(0x128690)));                   // 0x10063364 / 0x1006336a
        const int ox = Ftol(static_cast<double>(Gf(0x12868c)));                   // 0x10063375 / 0x1006337b
        RECT rc;
        RECT orig;
        jSetRect(&rc, x + ox, y + oy, sw, sh);                                    // 0x1006338a
        orig = rc;                                                                // 0x10063392..0x100633a7
        if (!jIntersect(&rc, &rc, vClip(dst))) {                                  // 0x100633ca / 0x100633d2
            vUnlock(dst, 1);                                                      // 0x100633e2
            return 0;                                                             // 0x100633ec
        }
        unsigned char* const d = reinterpret_cast<unsigned char*>(vBits8(dst)) +
                                 rc.left + rc.top * G(0x128644);                  // 0x100633fd..0x10063416
        G(0x128644) = vPitch(dst);                                                // 0x10063430
        G(0x128650) = rc.right - rc.left;                                         // 0x1006343b
        G(0x128670) = rc.bottom - rc.top;                                         // 0x10063447
        G(0x128648) = G(0x128644) - G(0x128650);                                  // 0x1006344d..0x10063458
        G(0x128688) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                    // 0x1006345d..0x1006347c
        G(0x128624) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                    // 0x10063481..0x100634a0
        const int offX = rc.left - orig.left;                                     // 0x100634bf..0x100634c5
        const int offY = rc.top - orig.top;                                       // 0x100634c8..0x100634ce
        G(0x12863c) = G(0x128688) * offX;                                         // 0x100634d1..0x100634db
        G(0x128640) = G(0x128624) * offY;                                         // 0x100634e1..0x100634eb
        G(0x128660) = ((G(0x12863c) & 0xffff) + G(0x128688) * G(0x128650)) >> 16; // 0x100634f1..0x1006350d
        G(0x128680) = ((G(0x128640) & 0xffff) + G(0x128624) * G(0x128670)) >> 16; // 0x10063512..0x1006352f
        const int pitch = G(0x128678);
        const int w = SpriteInt(self, 0x30);
        const int h = SpriteInt(self, 0x34);
        const unsigned char* s;
        int* const rowStep = GpInt(0x122e58);                                     // 0x10063588
        // The four mirror bodies (0x100634bf, 0x1006367d, 0x10063839 and 0x100639e8) are chosen at
        // 0x100634a5/0x100634ac, 0x100634b2/0x100634b9 and 0x1006382c/0x10063833.
        if (!negX && !negY) {
            G(0x12861c) = pitch - G(0x128660);                                    // 0x10063535..0x10063541
            s = bits + (G(0x128640) >> 16) * pitch + (G(0x12863c) >> 16);         // 0x10063547..0x1006356a
            rowStep[0] = (G(0x128624) >> 16) * pitch - pitch + G(0x12861c);       // 0x1006356d..0x1006358e
        } else if (!negX && negY) {
            s = bits + (h - 1 - (G(0x128640) >> 16)) * pitch + (G(0x12863c) >> 16);
            rowStep[0] = (G(0x128624) >> 16) * pitch + G(0x128660);
        } else if (negX && !negY) {
            s = bits + (G(0x128640) >> 16) * pitch + w - 1 - (G(0x12863c) >> 16);
            rowStep[0] = (G(0x128624) >> 16) * pitch + G(0x128660);
        } else {
            s = bits + (h - 1 - (G(0x128640) >> 16)) * pitch + w - 1 -
                (G(0x12863c) >> 16);
            G(0x12861c) = -pitch + G(0x128660);
            rowStep[0] = (G(0x128624) >> 16) * pitch - G(0x128660);
        }
        *GpInt(0x122e54) = rowStep[0] + pitch;                                    // 0x10063590..0x100635a4
        G(0x128650) = (G(0x128650) - 1) << 16;                                    // 0x100635ce..0x100635d8
        const int stepX = static_cast<unsigned short>(G(0x128688) >> 16);         // 0x100635e0 word [0x1012868a]
        G(0x12867c) = stepX;                                                      // 0x100635e7
        G(0x128628) = G(0x128624) << 16;                                          // 0x100635ed..0x100635f5
        ScaledLoop8(s, d, stepX, !negX);
    }
    vUnlock(dst, 2);                                                              // 0x10063bbe
    return 0;                                                                     // 0x10063bc8
}

}  // namespace

SG_HOOK("jgld.dll", 0x0002ed60, Sprite_draw16_1002ed60, DrawEd60_re, DrawEd60_orig);
SG_HOOK("jgld.dll", 0x00030290, Sprite_draw16_10030290, Draw30290_re, Draw30290_orig);
SG_HOOK("jgld.dll", 0x00041f80, Sprite_draw16_10041f80, Draw41f80_re, Draw41f80_orig);
SG_HOOK("jgld.dll", 0x00043430, Sprite_draw16_10043430, Draw43430_re, Draw43430_orig);
SG_HOOK("jgld.dll", 0x00044940, Sprite_draw16_10044940, Draw44940_re, Draw44940_orig);
SG_HOOK("jgld.dll", 0x00045e50, Sprite_draw16_10045e50, Draw45e50_re, Draw45e50_orig);
SG_HOOK("jgld.dll", 0x000473b0, Sprite_draw16_100473b0, Draw473b0_re, Draw473b0_orig);
SG_HOOK("jgld.dll", 0x00048910, Sprite_draw16_10048910, Draw48910_re, Draw48910_orig);
SG_HOOK("jgld.dll", 0x00020430, Sprite_draw16_10020430, Draw20430_re, Draw20430_orig);
SG_HOOK("jgld.dll", 0x00062dd0, Sprite_draw_10062dd0, Draw62dd0_re, Draw62dd0_orig);
