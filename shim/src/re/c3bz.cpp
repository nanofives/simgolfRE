// Batch c3bz: jgld.dll render, the Sprite draw rows batch c3bu handed over (round 19 of log/c3_brief.md;
// the hand-off is log/c3/c3bu_notes.txt, "What the next batch could take from here").
//   0x00022a10  Sprite::draw16_10022a10  the Palette colour averaged field by field with a caller colour
//   0x00024140  Sprite::draw16_10024140  a caller colour scaled by the Palette colour's weighted sum
//   0x00021720  Sprite::draw16_10021720  two 16-bit tables, applied in OPPOSITE orders in the two halves
//   0x00063f60  Sprite::draw8_10063f60   one 64 KB table indexed by (payload << 8) | destination
//   0x00061db0  Sprite::draw8_10061db0   the payload byte remapped through a byte table
//   0x00060cf0  Sprite::draw8_10060cf0   payload 0xf8 remaps the destination, every other byte is copied
//   0x0005fd20  Sprite::draw8_1005fd20   one flat colour; the stepped half writes zero instead (finding)
//   0x0005dde0  Sprite::draw_1005dde0    the payload byte copied, threshold 0xff
//   0x0005ed80  Sprite::draw_1005ed80    the payload byte copied, threshold 0xfe
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below is a
// VA, as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts, so these
// hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// TWO SKELETONS, the pair batch c3bu reimplemented (shim/src/re/c3bu.cpp), re-derived here against this
// batch's own disassembly:
//   * the 16-bit one (guard, Palette, the two scale ratios, the two pitches, the format switch, then
//     `abs(0x10122dc0) == abs(0x10122dc8) && abs(0x10122dc4) == abs(0x10122dc8)` choosing an unscaled half
//     from a 16.16 stepped half, and inside each half the SIGNS of 0x10122dc0 and 0x10122dc4 choosing one of
//     four mirror bodies) for 0x22a10, 0x24140 and 0x21720, written here with 0x10022a10's addresses;
//   * the 8-bit one (no Palette, no `return 16`, no format switch, `bits8()` instead of `bits()`, and a
//     parallel block of module globals) for the other six, written here with 0x10063f60's addresses.
// A normalized whole-function diff of each of the nine against the member of its skeleton batch c3bu already
// reimplemented (0x20430 for the 16-bit three, 0x62dd0 for the 8-bit six; the diff folds addresses and jump
// targets into label indices) leaves every changed line inside the eight pixel loops, except for the
// preamble stores and the integer-step store listed next, each read one by one in the binary:
//   0x22a10 / 0x24140  park NO argument in a global (0x20430 stores both at 0x1002053f and 0x10020547,
//                      these two store nothing) and their format-switch default UNLOCKS before returning 1
//                      (0x10022b89, where 0x20430's and 0x21720's default returns 1 holding the lock).
//                      Neither reads its fifth argument slot [ebp + 0x18] anywhere in its 1323 and 1371
//                      instructions.
//   0x21720            parks both arguments exactly where 0x20430 does (0x1002182f, 0x10021837).
//   0x63f60 / 0x61db0  park their one argument at 0x1012862c (0x10064022, 0x10061e72) and nothing at
//                      0x1012866c, and keep the stepped half's integer horizontal step in ecx
//                      (`adc esi, ecx`, 0x10064753) instead of parking it at 0x1012867c.
//   0x60cf0            parks its argument at 0x1012862c (0x10060db2) and DOES park the integer step at
//                      0x1012867c (0x100614b2), because its pixel operation needs ecx.
//   0x5fd20            parks its argument masked to a byte at 0x10128664 (`and edx, 0xff` 0x1005fde2,
//                      `mov dword ptr [0x10128664], edx` 0x1005fde8) and keeps the integer step in ecx.
//   0x5dde0 / 0x5ed80  park nothing at all and keep the integer step in ecx.
//
// Everything the A/B hands these functions is built by re/frida/js/fixtures.d/c3bz.js on jgld's own Surface
// (0x1011d0b0), Palette (0x1011db10) and Sprite (0x1011d380) vtables: never the live Display at 0x10128420,
// never a screen surface, never DirectDraw.
//
// ONE OF THE NINE IS LIVE. log/c3/alt/c3bz_probe.py, one 12 s boot at the menu with counters attached
// straight to the module addresses: 0 entries into eight of the nine and into seven of their eight static
// callers, but 87_674 entries into Sprite::draw_1005ed80 0x5ed80 and 90_022 into its caller 0x15480, both on
// the process's first thread (the thread the live Sprite::draw16_1001bc40 0x1bc40 also runs on, 2_348
// entries). Every registry key of this batch therefore runs with that thread suspended and both global
// blocks restored before it is resumed (fixture c3bz.js, the recipe c3av established for 0x1012847c and
// c3bm, c3bp and c3bu extended). The row counters at 0x10128578 and 0x10128670 are decremented IN MEMORY by
// the loops below, exactly as `dec dword ptr [0x10128670]` (0x100641db) does.
//
// Surface vtable slots used here (0x1011d0b0, named in re/names/jgld_2.tsv):
//   +0x10  lock()      0x10008730: copies the base at this+0x4c0 to this+0x4cc, bumps the use count at
//          this+0x4c8 when that copy is not NULL, and returns the COPY, so a surface with no pixel base
//          makes the guard return 3.
//   +0x18  bits8()     0x1000a750: tail-calls slot +0x10 (0x1000a777); the six 8-bit members call it
//          (0x10064144).
//   +0x20  bits()      0x1000a7b0: tail-calls slot +0x10 (0x1000a7d7); the three 16-bit members call it
//          (0x10022cb9).
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
// of a copy: setRect 0x10008360 (through thunk 0x10001019, 0x10022c05 and 0x10064090), intersect 0x10008590
// (thunk 0x10001087, 0x100232b2 and 0x100644f5), the CRT abs 0x1007f370 (0x10022ab0 and 0x10063fb4).
// IntersectRect is called through jgld's OWN import slot at 0x1012c5e4, the one the originals call
// (0x10022c47 and 0x100640d2).
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
// (0x10022a6d / 0x10022a78 in 0x22a10 and the twins in the other two 16-bit members).
inline void* SpritePalette(void* s) {
    void* p = *reinterpret_cast<void**>(reinterpret_cast<char*>(s) + 0x10);
    if (p) return p;
    char* client = *reinterpret_cast<char**>(JgldBase() + 0x12873c);
    return *reinterpret_cast<void**>(client + 4);
}

// VC6 converts a double to an int with __ftol (jgld's own copy is 0x1007f12c, called at 0x10023234,
// 0x10023246, 0x10023252 and 0x10023263): a 64-bit fistp with the low dword kept, i.e. truncation.
inline int Ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// The colour table parked at 0x1012856c, as the 16-bit loops read it (`mov ecx, dword ptr [0x1012856c]`,
// 0x10022d2d). The six 8-bit members read NO colour table: none of them references 0x10128658, the slot
// 0x62dd0 reads.
inline const unsigned short* Tbl() { return reinterpret_cast<const unsigned short*>(G(0x12856c)); }
// The byte table the four table-driven 8-bit members park at 0x1012862c and read inside the loop
// (`mov ebp, dword ptr [0x1012862c]`, 0x100641aa).
inline const unsigned char* Tbl8() { return reinterpret_cast<const unsigned char*>(G(0x12862c)); }

// ------------------------------------------------------------------ the 16-bit skeleton

// What the preamble does between the two pitches and the scale test, which is the only part of the 16-bit
// skeleton that is not identical across the three members.
enum PreKind {
    PreTableUnlock,   // format switch -> 0x1012856c; the switch default UNLOCKS and returns 1 (0x10022b89)
    PreTableKeep      // the same switch whose default returns 1 WITHOUT unlocking (0x1002189e)
};

// Which half of the function a pixel operation is running in. 0x21720 is the only member of the family found
// so far whose two halves do DIFFERENT work (see OpTwoTables16 below), so the flag is handed to every one.
enum Half { Unscaled, Stepped };

// The unscaled half of the four mirror bodies of 0x10022a10 (the first begins at 0x10022c8f).
// `dword ptr [0x10128578]` is the remaining row count, decremented in memory.
//   colStep  `inc esi` (0x10022dc6) when 0x10122dc0 >= 0, `dec esi` when it is negative
//   rowAdd   `add esi, [0x1012853c]` (0x10022dd1) when 0x10122dc4 >= 0, `sub esi` when negative
template <class Op>
void UnscaledLoop16(const unsigned char* s, unsigned short* d, int colStep, bool rowAdd, const Op& op) {
    for (;;) {
        int cols = G(0x128564);                                                   // 0x10022d33
        for (;;) {
            op(s, d, Unscaled);                                                   // 0x10022d39..0x10022dc5
            s += colStep;                                                         // 0x10022dc6
            ++d;                                                                  // 0x10022dc7
            if (--cols == 0) break;                                               // 0x10022dca / 0x10022dcb
        }
        if (rowAdd) s += G(0x12853c); else s -= G(0x12853c);                       // 0x10022dd1
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x10022dd7
        if (--G(0x128578) == 0) break;                                            // 0x10022ddd / 0x10022de3
    }
}

// The 16.16 stepped half of the four mirror bodies (0x10024c5f is the first one's row head in 0x10024140,
// whose addresses this loop is written with because its pixel operation is the longest of the three and its
// disassembly shows every step). edx carries the fractional vertical position in its high half and the
// fractional horizontal one in its low half (0x100234a2 / 0x100234a5); ebx carries the remaining column
// count minus one in its high half and the fractional horizontal step in its low half, so `sub ebx, 0x10000`
// + `jns` runs the row exactly `cols` times. The source cursor takes the INTEGER part of the horizontal step
// plus the carry out of the fractional add (`add dx, bx` + `adc esi`), and at the end of a row the carry out
// of the vertical add selects one of the two ints the module pointer at 0x10122e20 addresses (`sbb eax,
// eax`, `add esi, [ebx + eax*4]`, 0x10024d34 / 0x10024d3c).
//   stepX     the integer horizontal step the preamble parked at 0x10128580 (0x100234d5)
//   fwd       `adc esi` when 0x10122dc0 >= 0, `sbb esi` when it is negative
template <class Op>
void ScaledLoop16(const unsigned char* s, unsigned short* d, int stepX, bool fwd, const Op& op) {
    const int* const rowStep = GpInt(0x122e20);                                   // 0x10024d36
    const unsigned fracStepX = static_cast<unsigned short>(G(0x128588));          // 0x10024c67
    const unsigned fracStepY = static_cast<unsigned>(G(0x128544));                // 0x10024d2e
    unsigned acc = (static_cast<unsigned>(G(0x128558)) << 16) |
                   static_cast<unsigned short>(G(0x128554));                      // 0x1002349c..0x100234a5
    for (;;) {
        int ebx = static_cast<int>(static_cast<unsigned>(G(0x128564)) | fracStepX);  // 0x10024c61/0x10024c67
        acc = (acc & 0xffff0000u) | static_cast<unsigned short>(G(0x128554));     // 0x10024c6e
        for (;;) {
            op(s, d, Stepped);
            const unsigned lo = (acc & 0xffff) + (static_cast<unsigned>(ebx) & 0xffff);  // 0x10024d10
            acc = (acc & 0xffff0000u) | (lo & 0xffff);
            const int carry = static_cast<int>(lo >> 16);
            if (fwd) s += stepX + carry; else s -= stepX + carry;                 // 0x10024d13
            ++d;                                                                  // 0x10024d19
            ebx -= 0x10000;                                                       // 0x10024d1c
            if (ebx < 0) break;                                                   // 0x10024d22
        }
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x10024d28
        const unsigned long long sum =
            static_cast<unsigned long long>(acc) + fracStepY;                     // 0x10024d2e
        acc = static_cast<unsigned>(sum);
        const int sel = (sum >> 32) ? -1 : 0;                                     // 0x10024d34
        s += rowStep[sel];                                                        // 0x10024d3c
        if (--G(0x128578) == 0) break;                                            // 0x10024d3f
    }
}

// The whole 16-bit function, from the guard to the final unlock. `param4` / `param5` are the fourth and
// fifth arguments of the member that parks them, stored into the module globals `rva4` / `rva5` before the
// format switch (0 where the member parks none: 0x22a10 and 0x24140 park nothing, 0x21720 parks 0x128548
// and 0x128574).
template <class Op>
int DrawFamily16(void* self, void* dst, int x, int y, PreKind pre,
                 int param4, unsigned rva4, int param5, unsigned rva5, const Op& op) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;  // 0x10022a36 / 0x10022a3c / 0x10022a4c, 0x10022a5a
    void* const pal = SpritePalette(self);                                        // 0x10022a67 / 0x10022a7d
    if (pal == 0) {                                                               // 0x10022a83 / 0x10022a87
        vUnlock(dst, 1);                                                          // 0x10022a95
        return 16;                                                                // 0x10022a9f
    }
    // Both ratios are `fild` of the CRT abs of a numerator, `fidiv` by the denominator at 0x10122dc8 and a
    // `fstp dword` (0x10022abb..0x10022ac4 and 0x10022adc..0x10022ae5). The quotient is formed at the x87
    // precision the A/B thread runs under and rounded to float only by the store, so each operand is widened
    // to double and the result assigned to a float: the form batch c3ap established.
    Gf(0x12858c) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc0))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10022ac4
    Gf(0x128590) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc4))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10022ae5
    G(0x12857c) = SpriteInt(self, 0x2c);                                          // 0x10022af1
    G(0x12855c) = vPitch(dst);                                                    // 0x10022b0e
    if (rva4 != 0) G(rva4) = param4;                                              // 0x1002182f (0x21720 only)
    if (rva5 != 0) G(rva5) = param5;                                              // 0x10021837 (0x21720 only)
    {
        const int fmt = vDepthPtr(dst)[1];                                        // 0x10022b26 / 0x10022b33
        if (fmt == 0) {                                                           // 0x10022b39 / 0x10022b3d
            G(0x12856c) = reinterpret_cast<int>(vTable565(pal));                  // 0x10022b5b
        } else if (fmt == 1) {                                                    // 0x10022b3f / 0x10022b43
            G(0x12856c) = reinterpret_cast<int>(vTable555(pal));                  // 0x10022b76
        } else {
            if (pre == PreTableUnlock) vUnlock(dst, 1);                           // 0x10022b89
            return 1;                                                             // 0x10022b93 / 0x1002189e
        }
    }
    const bool negX = G(0x122dc0) < 0;
    const bool negY = G(0x122dc4) < 0;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x10022bbc / 0x10022bbe
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x10022be3 / 0x10022be5
        // ---- the unscaled half (begins at 0x10022beb)
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x10022c05
        orig = rc;                                                                // 0x10022c0d..0x10022c22
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x10022c47 / 0x10022c56
            vUnlock(dst, 1);                                                      // 0x10022c64
            return 0;                                                             // 0x10022c6e
        }
        // The four mirror bodies, chosen at 0x10022c75/0x10022c7c, 0x10022c82/0x10022c89 and the second sign
        // test of the mirrored half, compute the same two cursors and the same row and column counts; they
        // differ in the sign of the source row skip and in the column direction.
        const unsigned char* const s = bits + (rc.left - orig.left) +
                                       (rc.top - orig.top) * G(0x12857c);         // 0x10022c8f..0x10022cac
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x10022cb9..0x10022cd6
        G(0x128578) = rc.bottom - rc.top;                                         // 0x10022cdf
        G(0x128564) = rc.right - rc.left;                                         // 0x10022ceb
        // pitch - cols where the two numerators have the same sign (0x10022cf0..0x10022cfc), pitch + cols
        // where they differ
        G(0x12853c) = (negX == negY) ? G(0x12857c) - G(0x128564)
                                     : G(0x12857c) + G(0x128564);
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x10022d02..0x10022d10
        UnscaledLoop16(s, d, negX ? -1 : 1, !negY, op);
    } else {
        // ---- the 16.16 stepped half (begins at 0x10023228)
        const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                            static_cast<double>(Gf(0x128590)));                   // 0x1002322b..0x10023234
        const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                            static_cast<double>(Gf(0x12858c)));                   // 0x1002323d..0x10023246
        const int oy = Ftol(static_cast<double>(Gf(0x128590)));                   // 0x1002324c / 0x10023252
        const int ox = Ftol(static_cast<double>(Gf(0x12858c)));                   // 0x1002325d / 0x10023263
        RECT rc;
        RECT orig;
        jSetRect(&rc, x + ox, y + oy, sw, sh);                                    // 0x10023272
        orig = rc;                                                                // 0x1002327a..0x1002328f
        if (!jIntersect(&rc, &rc, vClip(dst))) {                                  // 0x100232b2 / 0x100232bc
            vUnlock(dst, 1);                                                      // 0x100232ca
            return 0;                                                             // 0x100232d4
        }
        // The destination cursor is formed with the pitch the preamble parked, BEFORE the pitch is read again
        // into the same global at 0x1002331c.
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x100232e5..0x10023302
        G(0x12855c) = vPitch(dst);                                                // 0x1002331c
        G(0x128564) = rc.right - rc.left;                                         // 0x10023327
        G(0x128578) = rc.bottom - rc.top;                                         // 0x10023333
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x10023339..0x10023346
        G(0x128588) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                    // 0x1002334b..0x1002336a
        G(0x128540) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                    // 0x1002336f..0x1002338e
        const int offX = rc.left - orig.left;                                     // 0x100233ad..0x100233b3
        const int offY = rc.top - orig.top;                                       // 0x100233b6..0x100233bc
        G(0x128554) = G(0x128588) * offX;                                         // 0x100233bf..0x100233c9
        G(0x128558) = G(0x128540) * offY;                                         // 0x100233cf..0x100233d9
        G(0x128570) = ((G(0x128554) & 0xffff) + G(0x128588) * G(0x128564)) >> 16; // 0x100233df..0x100233fb
        G(0x128584) = ((G(0x128558) & 0xffff) + G(0x128540) * G(0x128578)) >> 16; // 0x10023400..0x1002341d
        const int pitch = G(0x12857c);
        const int w = SpriteInt(self, 0x30);
        const int h = SpriteInt(self, 0x34);
        const unsigned char* s;
        int* const rowStep = GpInt(0x122e20);                                     // 0x10023476
        // The four mirror bodies are chosen at 0x10023393/0x1002339a, 0x100233a0/0x100233a7 and the second
        // sign test of the mirrored half.
        if (!negX && !negY) {
            G(0x12853c) = pitch - G(0x128570);                                    // 0x10023423..0x1002342f
            s = bits + (G(0x128558) >> 16) * pitch + (G(0x128554) >> 16);         // 0x10023435..0x10023458
            rowStep[0] = (G(0x128540) >> 16) * pitch - pitch + G(0x12853c);       // 0x1002345b..0x1002347c
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
        *GpInt(0x122e1c) = rowStep[0] + pitch;                                    // 0x1002347e..0x10023492
        G(0x128564) = (G(0x128564) - 1) << 16;                                    // 0x100234bc..0x100234c6
        const int stepX = static_cast<unsigned short>(G(0x128588) >> 16);         // 0x100234ce word [0x1012858a]
        G(0x128580) = stepX;                                                      // 0x100234d5
        G(0x128544) = G(0x128540) << 16;                                          // 0x100234db..0x100234e3
        ScaledLoop16(s, d, stepX, !negX, op);
    }
    vUnlock(dst, 2);                                                              // 0x10023c78
    return 0;                                                                     // 0x10023c82
}

// ------------------------------------------------------------------ the three 16-bit pixel operations

// 0x10022a10: the gate is UNCONDITIONAL, unlike the `char flag` members batch c3bu reimplemented: payload
// bytes from 0xff up and from 0xf8 up are both skipped, with no argument to switch the second test off
// (0x10022d39 and 0x10022d42). The fourth argument's LOW WORD is loaded once per pixel loop into the high
// half of ebp (`mov bp, word ptr [ebp + 0x14]`, `shl ebp, 0x10`, 0x10022d26 / 0x10022d2a), so only sixteen
// bits of it are ever read and the upper half of the argument slot cannot matter; the low half of ebp then
// carries the pixel being built. Each of the three RGB555 fields of the result is the sum of the two
// operands' fields, each shifted down one bit more than the field's position (`shr cx, 0xb` / `shr ax, 0xb`
// for red at 0x10022d63 / 0x10022d67, `shr cx, 6` for green, `shr cx, 1` for blue), i.e. the truncated
// average, written back into its own field (`and bp, 0x83ff` / `or bp, ax`, 0x10022d72 / 0x10022d77 and the
// two twins). All three masks are fixed RGB555 ones whatever the destination's format code.
struct OpAvg555 {
    unsigned short c;                                 // the fourth argument's low word
    void operator()(const unsigned char* s, unsigned short* d, Half) const {
        if (*s >= 0xff) return;                                                   // 0x10022d39 / 0x10022d3c
        if (*s >= 0xf8) return;                                                   // 0x10022d42 / 0x10022d45
        unsigned bp = Tbl()[*s];                                                  // 0x10022d49 / 0x10022d4b
        const unsigned hi = c;                                                    // ebp's high half
        unsigned ax, cx;
        cx = (hi & 0x7c00) >> 11;                                                 // 0x10022d5a / 0x10022d63
        ax = (bp & 0x7c00) >> 11;                                                 // 0x10022d5f / 0x10022d67
        ax = (ax + cx) & 0xffff;                                                  // 0x10022d6b `add ax, cx`
        bp = (bp & 0x83ff) | ((ax << 10) & 0xffff);                               // 0x10022d6e..0x10022d77
        cx = (hi & 0x3e0) >> 6;                                                   // 0x10022d82 / 0x10022d8b
        ax = (bp & 0x3e0) >> 6;                                                   // 0x10022d87 / 0x10022d8f
        ax = (ax + cx) & 0xffff;                                                  // 0x10022d93
        bp = (bp & 0xfc1f) | ((ax << 5) & 0xffff);                                // 0x10022d96..0x10022d9f
        cx = (hi & 0x1f) >> 1;                                                    // 0x10022daa / 0x10022db2
        ax = (bp & 0x1f) >> 1;                                                    // 0x10022dae / 0x10022db5
        ax = (ax + cx) & 0xffff;                                                  // 0x10022db8
        bp = (bp & 0xffe0) | ax;                                                  // 0x10022dbb / 0x10022dbf
        *d = static_cast<unsigned short>(bp);                                     // 0x10022dc2
    }
};

// 0x10024140: the same gate (0x10024c75, 0x10024c7e) and the same fourth-argument handling (0x10024456 in
// the first pixel loop, 0x10024c52 in the first stepped one), but
// the Palette colour is reduced to ONE byte and the caller's colour is scaled by it. The byte is
// `((pal & 0x7c00) >> 7) + ((pal & 0x3e0) >> 1) + ((pal & 0x1f) << 3)` summed in dx and then `shr dx, 2`
// (0x10024c95..0x10024cbe), so the three 5-bit fields contribute 8, 16 and 8 times their value and the sum
// is at most 992, i.e. at most 248 after the shift and always a byte. Each field of the CALLER's colour is
// then multiplied by that byte with the 8-bit `mul dl` (0x10024ccc, 0x10024ce0, 0x10024cf8), shifted down by
// eight and put back in its place; nothing of the Palette colour and nothing of the destination survives.
struct OpScale555 {
    unsigned short c;
    void operator()(const unsigned char* s, unsigned short* d, Half) const {
        if (*s >= 0xff) return;                                                   // 0x10024c75 / 0x10024c78
        if (*s >= 0xf8) return;                                                   // 0x10024c7e / 0x10024c81
        const unsigned pal = Tbl()[*s];                                           // 0x10024c89 / 0x10024c8b
        unsigned dx = 0;                                                          // 0x10024c91
        dx = (dx + ((pal & 0x7c00) >> 7)) & 0xffff;                               // 0x10024c98..0x10024ca0
        dx = (dx + ((pal & 0x3e0) >> 1)) & 0xffff;                                // 0x10024ca6..0x10024cad
        dx = (dx + ((pal & 0x1f) << 3)) & 0xffff;                                 // 0x10024cb3..0x10024cbb
        dx = (dx >> 2) & 0xffff;                                                  // 0x10024cbe
        const unsigned dl = dx & 0xff;                                            // `mul dl` reads dl only
        const unsigned hi = c;                                                    // ebp's high half
        unsigned cx = 0;                                                          // 0x10024c93
        cx |= (((hi & 0x1f) * dl) & 0xffff) >> 8;                                 // 0x10024cc8..0x10024cd2
        cx |= (((((hi & 0x3e0) >> 5) * dl) & 0xffff) >> 8) << 5;                  // 0x10024cd8..0x10024cea
        cx |= (((((hi & 0x7c00) >> 10) * dl) & 0xffff) >> 8) << 10;               // 0x10024cf0..0x10024d02
        *d = static_cast<unsigned short>(cx);                                     // 0x10024d08 / 0x10024d0b
    }
};

// 0x10021720: the two tables of 0x10020430's shape, both 16-bit here, and the two halves apply them in
// OPPOSITE ORDERS. Payload bytes from 0xfe up are skipped (0x10021a3c); payload 0xf8 remaps the DESTINATION
// pixel through the table the fifth argument parked at 0x10128574 (0x10021a46..0x10021a54, identical in both
// halves). Every other byte:
//   unscaled half (0x10021a59..0x10021a6a)  `mov bx, [ecx + eax*2]` then `mov bx, [ebp + ebx*2]`: the
//       payload byte indexes the Palette table and the COLOUR indexes the fourth argument's table;
//   stepped half (0x1002207b..0x1002208c)   `mov ax, [ebp + eax*2]` then `mov bp, [ecx + eax*2]`: the
//       payload byte indexes the fourth argument's table and ITS 16-bit result indexes the Palette table.
// The second order makes a Palette table of 256 entries read far out of bounds for any table entry above
// 255, which is why the fixture's table at 0x10128548 holds only values below 256.
struct OpTwoTables16 {
    void operator()(const unsigned char* s, unsigned short* d, Half half) const {
        if (*s >= 0xfe) return;                                                   // 0x10021a3c / 0x10021a3f
        const unsigned short* const pal = Tbl();                                  // 0x10021a30
        if (*s == 0xf8) {                                                         // 0x10021a41 / 0x10021a44
            const unsigned short* const dmap =
                reinterpret_cast<const unsigned short*>(G(0x128574));             // 0x10021a46
            *d = dmap[*d];                                                        // 0x10021a4c / 0x10021a54
            return;
        }
        const unsigned short* const imap =
            reinterpret_cast<const unsigned short*>(G(0x128548));                 // 0x10021a59
        if (half == Unscaled) {
            *d = imap[pal[*s]];                                                   // 0x10021a61 / 0x10021a65
        } else {
            *d = pal[imap[*s]];                                                   // 0x10022083 / 0x10022088
        }
    }
};

// ------------------------------------------------------------------ the 8-bit skeleton

// What the 8-bit preamble parks, which is all that separates the six 8-bit members outside their pixel loops.
struct Pre8 {
    unsigned rva4;      // the global the fourth argument goes to, 0 where the member takes none
    bool maskByte;      // 0x5fd20 stores the argument masked to a byte (`and edx, 0xff`, 0x1005fde2)
    bool writeStep;     // 0x60cf0 parks the stepped half's integer step at 0x1012867c (0x100614b2)
};

// The unscaled half of 0x10063f60's four mirror bodies (the first begins at 0x1006411a). The destination
// advances ONE byte per pixel (`inc edi`, 0x100641cb) and by 0x10128648 per row (0x100641d5).
template <class Op>
void UnscaledLoop8(const unsigned char* s, unsigned char* d, int colStep, bool rowAdd, const Op& op) {
    for (;;) {
        int cols = G(0x128650);                                                   // 0x100641b0
        for (;;) {
            op(s, d, Unscaled, 0);                                                // 0x100641b6..0x100641c8
            s += colStep;                                                         // 0x100641ca
            ++d;                                                                  // 0x100641cb
            if (--cols == 0) break;                                               // 0x100641cc / 0x100641cd
        }
        if (rowAdd) s += G(0x12861c); else s -= G(0x12861c);                       // 0x100641cf
        d += G(0x128648);                                                         // 0x100641d5
        if (--G(0x128670) == 0) break;                                            // 0x100641db / 0x100641e1
    }
}

// The 16.16 stepped half of the 8-bit members (0x10064726 is 0x10063f60's first row head): the same
// arithmetic as ScaledLoop16 on the 8-bit block, with the integer step either in ecx (`adc esi, ecx`,
// 0x10064753) or reloaded from 0x1012867c (`adc esi, dword ptr [0x1012867c]`, 0x100614fd in 0x10060cf0).
template <class Op>
void ScaledLoop8(const unsigned char* s, unsigned char* d, int stepX, bool fwd, const Op& op) {
    const int* const rowStep = GpInt(0x122e58);                                   // 0x1006476c
    const unsigned fracStepX = static_cast<unsigned short>(G(0x128688));          // 0x1006472e
    const unsigned fracStepY = static_cast<unsigned>(G(0x128628));                // 0x10064764
    unsigned acc = (static_cast<unsigned>(G(0x128640)) << 16) |
                   static_cast<unsigned short>(G(0x12863c));                      // 0x100646d9..0x100646e2
    for (;;) {
        int ebx = static_cast<int>(static_cast<unsigned>(G(0x128650)) | fracStepX);  // 0x10064728/0x1006472e
        acc = (acc & 0xffff0000u) | static_cast<unsigned short>(G(0x12863c));     // 0x10064735
        for (;;) {
            op(s, d, Stepped, stepX);
            const unsigned lo = (acc & 0xffff) + (static_cast<unsigned>(ebx) & 0xffff);  // 0x10064750
            acc = (acc & 0xffff0000u) | (lo & 0xffff);
            const int carry = static_cast<int>(lo >> 16);
            if (fwd) s += stepX + carry; else s -= stepX + carry;                 // 0x10064753
            ++d;                                                                  // 0x10064755
            ebx -= 0x10000;                                                       // 0x10064756
            if (ebx < 0) break;                                                   // 0x1006475c
        }
        d += G(0x128648);                                                         // 0x1006475e
        const unsigned long long sum =
            static_cast<unsigned long long>(acc) + fracStepY;                     // 0x10064764
        acc = static_cast<unsigned>(sum);
        const int sel = (sum >> 32) ? -1 : 0;                                     // 0x1006476a
        s += rowStep[sel];                                                        // 0x10064772
        if (--G(0x128670) == 0) break;                                            // 0x10064775 / 0x1006477b
    }
}

// The whole 8-bit function, from the guard to the final unlock. There is no Palette fetch, no `return 16`
// and no format switch: the guard at 0x10063f84 is followed straight by the two scale ratios
// (0x10063fc8 / 0x10063fe9).
template <class Op>
int DrawFamily8(void* self, void* dst, int x, int y, int param4, const Pre8& pre, const Op& op) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;  // 0x10063f80 / 0x10063f86 / 0x10063f96, 0x10063fa4
    Gf(0x12868c) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc0))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10063fc8
    Gf(0x128690) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc4))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10063fe9
    G(0x128678) = SpriteInt(self, 0x2c);                                          // 0x10063ff5
    G(0x128644) = vPitch(dst);                                                    // 0x10064011
    if (pre.rva4 != 0) {
        G(pre.rva4) = pre.maskByte ? (param4 & 0xff) : param4;                    // 0x10064022 / 0x1005fde8
    }
    const bool negX = G(0x122dc0) < 0;
    const bool negY = G(0x122dc4) < 0;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x10064047 / 0x10064049
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1006406e / 0x10064070
        // ---- the unscaled half (begins at 0x10064076)
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x10064090
        orig = rc;                                                                // 0x10064098..0x100640ad
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x100640d2 / 0x100640e1
            vUnlock(dst, 1);                                                      // 0x100640ef
            return 0;                                                             // 0x100640f9
        }
        // The four mirror bodies are chosen at 0x10064100/0x10064107, 0x1006410d/0x10064114 and the second
        // sign test of the mirrored half.
        const unsigned char* const s = bits + (rc.left - orig.left) +
                                       (rc.top - orig.top) * G(0x128678);         // 0x1006411a..0x10064137
        unsigned char* const d = reinterpret_cast<unsigned char*>(vBits8(dst)) +
                                 rc.left + rc.top * G(0x128644);                  // 0x10064144..0x1006415d
        G(0x128670) = rc.bottom - rc.top;                                         // 0x10064166
        G(0x128650) = rc.right - rc.left;                                         // 0x10064172
        G(0x12861c) = (negX == negY) ? G(0x128678) - G(0x128650)                  // 0x10064177..0x10064183
                                     : G(0x128678) + G(0x128650);
        G(0x128648) = G(0x128644) - G(0x128650);                                  // 0x10064189..0x10064195
        UnscaledLoop8(s, d, negX ? -1 : 1, !negY, op);
    } else {
        // ---- the 16.16 stepped half (begins at 0x1006446b)
        const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                            static_cast<double>(Gf(0x128690)));                   // 0x1006446e..0x10064477
        const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                            static_cast<double>(Gf(0x12868c)));                   // 0x10064480..0x10064489
        const int oy = Ftol(static_cast<double>(Gf(0x128690)));                   // 0x1006448f / 0x10064495
        const int ox = Ftol(static_cast<double>(Gf(0x12868c)));                   // 0x100644a0 / 0x100644a6
        RECT rc;
        RECT orig;
        jSetRect(&rc, x + ox, y + oy, sw, sh);                                    // 0x100644b5
        orig = rc;                                                                // 0x100644bd..0x100644d2
        if (!jIntersect(&rc, &rc, vClip(dst))) {                                  // 0x100644f5 / 0x100644ff
            vUnlock(dst, 1);                                                      // 0x1006450d
            return 0;                                                             // 0x10064517
        }
        unsigned char* const d = reinterpret_cast<unsigned char*>(vBits8(dst)) +
                                 rc.left + rc.top * G(0x128644);                  // 0x10064528..0x10064541
        G(0x128644) = vPitch(dst);                                                // 0x1006455b
        G(0x128650) = rc.right - rc.left;                                         // 0x10064566
        G(0x128670) = rc.bottom - rc.top;                                         // 0x10064572
        G(0x128648) = G(0x128644) - G(0x128650);                                  // 0x10064578..0x10064583
        G(0x128688) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                    // 0x10064588..0x100645a7
        G(0x128624) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                    // 0x100645ac..0x100645cb
        const int offX = rc.left - orig.left;                                     // 0x100645ea..0x100645f0
        const int offY = rc.top - orig.top;
        G(0x12863c) = G(0x128688) * offX;
        G(0x128640) = G(0x128624) * offY;
        G(0x128660) = ((G(0x12863c) & 0xffff) + G(0x128688) * G(0x128650)) >> 16;
        G(0x128680) = ((G(0x128640) & 0xffff) + G(0x128624) * G(0x128670)) >> 16;
        const int pitch = G(0x128678);
        const int w = SpriteInt(self, 0x30);
        const int h = SpriteInt(self, 0x34);
        const unsigned char* s;
        int* const rowStep = GpInt(0x122e58);                                     // 0x100646b3
        // The four mirror bodies are chosen at 0x100645d0/0x100645d7, 0x100645dd/0x100645e4 and the second
        // sign test of the mirrored half.
        if (!negX && !negY) {
            G(0x12861c) = pitch - G(0x128660);
            s = bits + (G(0x128640) >> 16) * pitch + (G(0x12863c) >> 16);
            rowStep[0] = (G(0x128624) >> 16) * pitch - pitch + G(0x12861c);       // 0x10064698..0x100646b9
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
        *GpInt(0x122e54) = rowStep[0] + pitch;                                    // 0x100646bb..0x100646cf
        G(0x128650) = (G(0x128650) - 1) << 16;                                    // 0x100646f9..0x10064703
        const int stepX = static_cast<unsigned short>(G(0x128688) >> 16);         // 0x1006470b word [0x1012868a]
        if (pre.writeStep) G(0x12867c) = stepX;                                   // 0x100614b2 (0x60cf0 only)
        G(0x128628) = G(0x128624) << 16;                                          // 0x10064712..0x1006471a
        ScaledLoop8(s, d, stepX, !negX, op);
    }
    vUnlock(dst, 2);                                                              // 0x10064c5d
    return 0;                                                                     // 0x10064c67
}

// ------------------------------------------------------------------ the six 8-bit pixel operations

// 0x10063f60: `xor eax, eax; mov al, [esi]; shl eax, 8; mov al, [edi]; mov al, [ebp + eax]`
// (0x100641bb..0x100641c4, the same eight times): the payload byte and the destination byte form a 16-bit
// index into the caller's table, which is therefore 64 KB.
struct Op8Pair {
    void operator()(const unsigned char* s, unsigned char* d, Half, int) const {
        if (*s >= 0xfe) return;                                                   // 0x100641b6 / 0x100641b9
        *d = Tbl8()[(static_cast<unsigned>(*s) << 8) | *d];                       // 0x100641bb..0x100641c8
    }
};

// 0x10061db0: `mov bl, [esi]; mov al, [ebp + ebx]` in the unscaled half (0x1006200b / 0x1006200d) and
// `mov al, [esi]; mov al, [ebp + eax]` in the stepped one (0x10062575 / 0x10062577): the same 256-byte
// remap of the payload byte, with the index in a register the body zeroes before the loop in both cases
// (`xor ebx, ebx` 0x10061fef, `xor eax, eax` 0x1006255a).
struct Op8Remap {
    void operator()(const unsigned char* s, unsigned char* d, Half, int) const {
        if (*s >= 0xfe) return;                                                   // 0x10062006 / 0x10062009
        *d = Tbl8()[*s];                                                          // 0x1006200b / 0x1006200d
    }
};

// 0x10060cf0: payload 0xf8 remaps the DESTINATION byte through the caller's table, every other byte below
// 0xfe is copied unchanged (0x10060f46..0x10060f5e). The index register differs between the halves, and in
// the stepped half it is NOT clean: ecx is loaded with the integer horizontal step
// (`xor ecx, ecx; mov cx, word ptr [0x1012868a]`, 0x100614a9 / 0x100614ab) and only its LOW BYTE is then
// replaced by the destination byte (`mov cl, byte ptr [edi]`, 0x100614ec), so the index carries the step's
// high byte in bits 8..15. With every scale this batch registers the step is below 0x100 and those bits are
// zero; the table the fixture passes is 64 KB so that a larger step would still read inside it.
struct Op8CopyOrRemap {
    void operator()(const unsigned char* s, unsigned char* d, Half half, int stepX) const {
        if (*s >= 0xfe) return;                                                   // 0x10060f46 / 0x10060f49
        if (*s == 0xf8) {                                                         // 0x10060f4b / 0x10060f4e
            const unsigned idx = (half == Unscaled)
                                     ? *d                                         // ebx, zeroed at 0x10060f2f
                                     : ((static_cast<unsigned>(stepX) & 0xff00) | *d);
            *d = Tbl8()[idx];                                                     // 0x10060f52 / 0x10060f56
        } else {
            *d = *s;                                                              // 0x10060f5a / 0x10060f5c
        }
    }
};

// 0x1005fd20: one flat colour for every covered pixel. The four unscaled bodies load it into al once
// (`mov al, byte ptr [0x10128664]`, 0x1005ff6f, 0x10060030, 0x100600fe and 0x100601bf) and keep it for every
// row. THE FOUR STEPPED BODIES LOAD IT AND THEN ZERO THE REGISTER: `mov al, byte ptr [0x10128664]` at
// 0x100604a5 is followed immediately by `xor eax, eax` at 0x100604aa, which is also the row-loop head the
// row end jumps back to (`jne 0x100604aa`, 0x100604f2), so every pixel the stepped half writes is 0 and the
// argument has no effect there. The same pair stands at 0x10060621/0x10060626, 0x100607aa/0x100607af and
// 0x10060946/0x1006094b. This is a defect of the original; both arms of the A/B do it.
struct Op8Fill {
    void operator()(const unsigned char* s, unsigned char* d, Half half, int) const {
        if (*s >= 0xfe) return;                                                   // 0x1005ff7a / 0x1005ff7d
        *d = (half == Unscaled) ? static_cast<unsigned char>(G(0x128664)) : 0;    // 0x1005ff7f / 0x100604c5
    }
};

// 0x1005dde0 and 0x1005ed80: the payload byte copied straight (`mov ah, byte ptr [esi]; mov byte ptr [edi],
// ah`, 0x1005e02c / 0x1005e02e and 0x1005efcc / 0x1005efce). The only difference between the two functions
// anywhere in their 877 instructions each is this threshold (0x1005e027 `cmp byte ptr [esi], 0xff` against
// 0x1005efc7 `cmp byte ptr [esi], 0xfe`), so 0x5dde0 draws payload byte 0xfe and 0x5ed80 skips it.
struct Op8Copy {
    unsigned char limit;
    void operator()(const unsigned char* s, unsigned char* d, Half, int) const {
        if (*s >= limit) return;                                                  // 0x1005e027 / 0x1005efc7
        *d = *s;                                                                  // 0x1005e02c / 0x1005e02e
    }
};

// ------------------------------------------------------------------ the hooks

// The MinHook trampolines the A/B's "original" arm is called through (one per SG_HOOK below).
typedef int(__fastcall* Draw5_t)(void*, void*, void*, int, int, int, int);
typedef int(__fastcall* Draw4_t)(void*, void*, void*, int, int, int);
typedef int(__fastcall* Draw3_t)(void*, void*, void*, int, int);
Draw5_t Draw22a10_orig;
Draw5_t Draw24140_orig;
Draw5_t Draw21720_orig;
Draw4_t Draw63f60_orig;
Draw4_t Draw61db0_orig;
Draw4_t Draw60cf0_orig;
Draw4_t Draw5fd20_orig;
Draw3_t Draw5dde0_orig;
Draw3_t Draw5ed80_orig;

// 0x00022a10  Sprite::draw16_10022a10(dst, x, y, colour, unused)
int __fastcall Draw22a10_re(void* self, void*, void* dst, int x, int y, int p4, int) {
    OpAvg555 op;
    op.c = static_cast<unsigned short>(p4);
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00024140  Sprite::draw16_10024140(dst, x, y, colour, unused)
int __fastcall Draw24140_re(void* self, void*, void* dst, int x, int y, int p4, int) {
    OpScale555 op;
    op.c = static_cast<unsigned short>(p4);
    return DrawFamily16(self, dst, x, y, PreTableUnlock, 0, 0, 0, 0, op);
}

// 0x00021720  Sprite::draw16_10021720(dst, x, y, colourMap, pixelMap)
// 0x10020430's twin: the same two globals and the same format-switch default that returns 1 WITHOUT
// unlocking (0x1002189e).
int __fastcall Draw21720_re(void* self, void*, void* dst, int x, int y, int p4, int p5) {
    return DrawFamily16(self, dst, x, y, PreTableKeep, p4, 0x128548, p5, 0x128574, OpTwoTables16());
}

// 0x00063f60  Sprite::draw8_10063f60(dst, x, y, table)
int __fastcall Draw63f60_re(void* self, void*, void* dst, int x, int y, int p4) {
    Pre8 pre = {0x12862c, false, false};
    return DrawFamily8(self, dst, x, y, p4, pre, Op8Pair());
}

// 0x00061db0  Sprite::draw8_10061db0(dst, x, y, table)
int __fastcall Draw61db0_re(void* self, void*, void* dst, int x, int y, int p4) {
    Pre8 pre = {0x12862c, false, false};
    return DrawFamily8(self, dst, x, y, p4, pre, Op8Remap());
}

// 0x00060cf0  Sprite::draw8_10060cf0(dst, x, y, table)
int __fastcall Draw60cf0_re(void* self, void*, void* dst, int x, int y, int p4) {
    Pre8 pre = {0x12862c, false, true};
    return DrawFamily8(self, dst, x, y, p4, pre, Op8CopyOrRemap());
}

// 0x0005fd20  Sprite::draw8_1005fd20(dst, x, y, colour)
int __fastcall Draw5fd20_re(void* self, void*, void* dst, int x, int y, int p4) {
    Pre8 pre = {0x128664, true, false};
    return DrawFamily8(self, dst, x, y, p4, pre, Op8Fill());
}

// 0x0005dde0  Sprite::draw_1005dde0(dst, x, y)
int __fastcall Draw5dde0_re(void* self, void*, void* dst, int x, int y) {
    Pre8 pre = {0, false, false};
    Op8Copy op;
    op.limit = 0xff;
    return DrawFamily8(self, dst, x, y, 0, pre, op);
}

// 0x0005ed80  Sprite::draw_1005ed80(dst, x, y)
int __fastcall Draw5ed80_re(void* self, void*, void* dst, int x, int y) {
    Pre8 pre = {0, false, false};
    Op8Copy op;
    op.limit = 0xfe;
    return DrawFamily8(self, dst, x, y, 0, pre, op);
}

}  // namespace

SG_HOOK("jgld.dll", 0x00022a10, Sprite_draw16_10022a10, Draw22a10_re, Draw22a10_orig);
SG_HOOK("jgld.dll", 0x00024140, Sprite_draw16_10024140, Draw24140_re, Draw24140_orig);
SG_HOOK("jgld.dll", 0x00021720, Sprite_draw16_10021720, Draw21720_re, Draw21720_orig);
SG_HOOK("jgld.dll", 0x00063f60, Sprite_draw8_10063f60, Draw63f60_re, Draw63f60_orig);
SG_HOOK("jgld.dll", 0x00061db0, Sprite_draw8_10061db0, Draw61db0_re, Draw61db0_orig);
SG_HOOK("jgld.dll", 0x00060cf0, Sprite_draw8_10060cf0, Draw60cf0_re, Draw60cf0_orig);
SG_HOOK("jgld.dll", 0x0005fd20, Sprite_draw8_1005fd20, Draw5fd20_re, Draw5fd20_orig);
SG_HOOK("jgld.dll", 0x0005dde0, Sprite_draw_1005dde0, Draw5dde0_re, Draw5dde0_orig);
SG_HOOK("jgld.dll", 0x0005ed80, Sprite_draw_1005ed80, Draw5ed80_re, Draw5ed80_orig);
