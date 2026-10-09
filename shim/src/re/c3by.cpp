// Batch c3by: jgld.dll render, the twenty-one `char flag` Sprite blend members that batches c3bp and
// c3bu left over (round 19 of log/c3_brief.md; the hand-off is log/c3/c3bu_notes.txt, "What the next
// batch could take from here").
//
//   RGB555 field layout (masks 0x7c1f0 / 0x3e00, the result shifted back down by four before the store)
//     0x000317c0  Sprite::draw16_100317c0  three halvings, the Palette colour is the halved side
//     0x00032d60  Sprite::draw16_10032d60  three halvings, the destination pixel is the halved side
//     0x00034300  Sprite::draw16_10034300  (15 * destination + Palette) >> 4
//     0x000358b0  Sprite::draw16_100358b0  (15 * Palette + destination) >> 4
//     0x00036e60  Sprite::draw16_10036e60  ((3 * Palette + destination) >> 2 + destination) >> 1
//     0x00038460  Sprite::draw16_10038460  ((3 * destination + Palette) >> 2 + Palette) >> 1
//     0x00039a60  Sprite::draw16_10039a60  ((7 * Palette + destination) >> 3 + destination) >> 1
//     0x0003b060  Sprite::draw16_1003b060  ((7 * destination + Palette) >> 3 + Palette) >> 1
//     0x0003c660  Sprite::draw16_1003c660  (13 * destination + 3 * Palette) >> 4
//     0x0003dca0  Sprite::draw16_1003dca0  (13 * Palette + 3 * destination) >> 4
//     0x0003f2e0  Sprite::draw16_1003f2e0  (11 * destination + 5 * Palette) >> 4
//     0x00040930  Sprite::draw16_10040930  (11 * Palette + 5 * destination) >> 4
//   RGB565 field layout (masks 0xf81f0 / 0x7e00, the result NOT shifted back: c3bu's finding 1)
//     0x00049ea0  Sprite::draw16_10049ea0  (15 * Palette + destination) >> 4
//     0x0004b430  Sprite::draw16_1004b430  ((3 * Palette + destination) >> 2 + destination) >> 1
//     0x0004ca10  Sprite::draw16_1004ca10  ((3 * destination + Palette) >> 2 + Palette) >> 1
//     0x0004dff0  Sprite::draw16_1004dff0  ((7 * Palette + destination) >> 3 + destination) >> 1
//     0x0004f5d0  Sprite::draw16_1004f5d0  ((7 * destination + Palette) >> 3 + Palette) >> 1
//     0x00050bb0  Sprite::draw16_10050bb0  (13 * destination + 3 * Palette) >> 4
//     0x000521d0  Sprite::draw16_100521d0  (13 * Palette + 3 * destination) >> 4
//     0x000537f0  Sprite::draw16_100537f0  (11 * destination + 5 * Palette) >> 4
//     0x00054e20  Sprite::draw16_10054e20  (11 * Palette + 5 * destination) >> 4
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below
// is a VA, as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts,
// so these hooks install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook
// finds them by (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall:
// ecx = this, edx unused.
//
// ONE SKELETON, ONE PIXEL OPERATION EACH, CHECKED AGAINST THE BINARY. log/c3/alt/c3by_asm.py reads all
// twenty-one bodies and 0x1002ed60's (the member batch c3bu reimplemented and verified) out of
// original\jgld.dll with capstone: each holds exactly EIGHT pixel blocks (from `push ebx; push ecx;
// push edx` to the matching `pop ebx`), the eight blocks of one function carry byte-identical
// instructions, and with those blocks folded away the remaining 1,216 instructions of every one of the
// twenty-one are instruction-for-instruction identical to 0x1002ed60's once jump and call targets are
// folded. The same script cross-checks each block against the 100% matched source
// re/match/jgld_blit16.cpp (log/c3/alt/c3by_ops.py), which agrees for all twenty-one, and finds a
// single transparency gate shared by all thirty `char flag` members of the family. So the skeleton is
// written once here (DrawFamily16, with 0x100317c0's instruction addresses) and each function supplies
// its own pixel operation, cited at its own addresses below.
//
// Everything the A/B hands these functions is built by re/frida/js/fixtures.d/c3by.js on jgld's own
// Surface (0x1011d0b0), Palette (0x1011db10) and Sprite (0x1011d380) vtables: never the live Display at
// 0x10128420, never a screen surface, never DirectDraw.
//
// THE SHARED LOOP GLOBALS. These twenty-one park their loop state in 0x1012857c (source pitch),
// 0x1012855c (destination pitch), 0x1012856c (colour table), 0x10128578 (rows), 0x10128564 (columns),
// 0x1012853c (source row skip) and 0x10128560 (destination row skip), the same block the live
// Sprite::draw16_1001bc40 0x1bc40 writes. Every registry key of this batch therefore runs with the
// game's first thread suspended and restores the block before resuming it (fixture c3by.js, the recipe
// c3av established for 0x1012847c and c3bm, c3bp and c3bu extended). The row counter at 0x10128578 is
// decremented IN MEMORY by the loop below, exactly as `dec dword ptr [0x10128578]` (0x10031b63) does.
//
// Surface vtable slots used here (0x1011d0b0, named in re/names/jgld_2.tsv):
//   +0x10  lock()      0x10008730: copies the base at this+0x4c0 to this+0x4cc, bumps the use count at
//          this+0x4c8 when that copy is not NULL, and returns the COPY, so a surface with no pixel base
//          makes the guard return 3.
//   +0x20  bits()      0x1000a7b0: tail-calls slot +0x10 (0x1000a7d7).
//   +0x24  unlock(f)   0x100087b0: this+0x4c8 -= f, clearing this+0x4cc and this+0x4c8 at <= 0.
//   +0xcc  clipRect()  0x1000a990: returns this+0x44.
//   +0xe0  pitch()     0x1000aaf0: returns the int at this+0x40.
//   +0xe4  depthPtr()  0x1000ab30: returns this+0x24, so element [1] is the format code at this+0x28.
// Palette vtable slots (0x1011db10): +0x18 -> this+0x40c (RGB565 table), +0x1c -> this+0x60c (RGB555).
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

// The module's own helpers, called through their module addresses so the comparison is of this code and
// not of a copy: setRect 0x10008360 (through thunk 0x10001019, 0x100319b5), intersect 0x10008590
// (thunk 0x10001087, 0x10031fb6), the CRT abs 0x1007f370 (0x10031860). IntersectRect is called through
// jgld's OWN import slot at 0x1012c5e4, the one the originals call (0x100319f7).
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
// The Palette of the sprite at +0x10, or the one of the client object the module global 0x1012873c
// points at (0x10031817 / 0x10031828 and the twins).
inline void* SpritePalette(void* s) {
    void* p = *reinterpret_cast<void**>(reinterpret_cast<char*>(s) + 0x10);
    if (p) return p;
    char* client = *reinterpret_cast<char**>(JgldBase() + 0x12873c);
    return *reinterpret_cast<void**>(client + 4);
}

// VC6 converts a double to an int with __ftol (jgld's own copy is 0x1007f12c, called at 0x10031f38,
// 0x10031f4a, 0x10031f56 and 0x10031f61): a 64-bit fistp with the low dword kept, i.e. truncation.
inline int Ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// The colour table parked at 0x1012856c, as the loops read it (`mov ecx, dword ptr [0x1012856c]`,
// 0x10031ad5).
inline const unsigned short* Tbl() { return reinterpret_cast<const unsigned short*>(G(0x12856c)); }

// ------------------------------------------------------------------ the shared skeleton

// The unscaled half of the four mirror bodies, which begin at 0x10031a3f, 0x10031b77, 0x10031cbc and
// 0x10031df4 in 0x100317c0. `dword ptr [0x10128578]` is the remaining row count, decremented in memory.
//   colStep  `inc esi` (0x10031b50) when 0x10122dc0 >= 0, `dec esi` when it is negative
//   rowAdd   `add esi, [0x1012853c]` (0x10031b57) when 0x10122dc4 >= 0, `sub esi` when negative
template <class Op>
void UnscaledLoop16(const unsigned char* s, unsigned short* d, int colStep, bool rowAdd, const Op& op) {
    for (;;) {
        int cols = G(0x128564);                                                   // 0x10031adb
        for (;;) {
            op(s, d);                                                             // 0x10031ae1..0x10031b4f
            s += colStep;                                                         // 0x10031b50
            ++d;                                                                  // 0x10031b51
            if (--cols == 0) break;                                               // 0x10031b54 / 0x10031b55
        }
        if (rowAdd) s += G(0x12853c); else s -= G(0x12853c);                       // 0x10031b57
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x10031b5d
        if (--G(0x128578) == 0) break;                                            // 0x10031b63 / 0x10031b69
    }
}

// The 16.16 stepped half of the four mirror bodies (the first begins at 0x100321f3 in 0x100317c0). edx
// carries the fractional vertical position in its high half and the fractional horizontal one in its low
// half; ebx carries the remaining column count minus one in its high half and the fractional horizontal
// step in its low half, so `sub ebx, 0x10000` + `jns` runs the row exactly `cols` times. The source
// cursor takes the INTEGER part of the horizontal step plus the carry out of the fractional add
// (`add dx, bx` + `adc esi`), and at the end of a row the carry out of the vertical add selects one of
// the two ints the module pointer at 0x10122e20 addresses (`sbb eax, eax`, `add esi, [ebx + eax*4]`).
//   stepX     the integer horizontal step the preamble parked at 0x10128580 (0x100321d9)
//   fwd       `adc esi` when 0x10122dc0 >= 0, `sbb esi` when it is negative
template <class Op>
void ScaledLoop16(const unsigned char* s, unsigned short* d, int stepX, bool fwd, const Op& op) {
    const int* const rowStep = GpInt(0x122e20);
    const unsigned fracStepX = static_cast<unsigned short>(G(0x128588));
    const unsigned fracStepY = static_cast<unsigned>(G(0x128544));
    unsigned acc = (static_cast<unsigned>(G(0x128558)) << 16) |
                   static_cast<unsigned short>(G(0x128554));                      // 0x100321a0..0x100321a9
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

// The whole function, from the guard to the final unlock, at 0x100317c0's addresses. Every one of the
// twenty-one shares it instruction for instruction (log/c3/alt/c3by_asm.py).
template <class Op>
int DrawFamily16(void* self, void* dst, int x, int y, const Op& op) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;  // 0x100317e6 / 0x100317ec / 0x10031806, 0x1003180a
    void* const pal = SpritePalette(self);                                        // 0x1003181b / 0x10031828
    if (pal == 0) {                                                               // 0x10031833
        vUnlock(dst, 1);                                                          // 0x10031845
        return 16;                                                                // 0x1003184f
    }
    // Both ratios are `fild` of the CRT abs of a numerator, `fidiv` by the denominator at 0x10122dc8 and
    // a `fstp dword` (0x1003186b..0x10031874 and 0x1003188c..0x10031895). The quotient is formed at the
    // x87 precision the A/B thread runs under and rounded to float only by the store, so each operand is
    // widened to double and the result assigned to a float: the form batch c3ap established.
    Gf(0x12858c) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc0))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10031874
    Gf(0x128590) = static_cast<float>(static_cast<double>(jAbs(G(0x122dc4))) /
                                      static_cast<double>(G(0x122dc8)));          // 0x10031895
    G(0x12857c) = SpriteInt(self, 0x2c);                                          // 0x100318a1
    G(0x12855c) = vPitch(dst);                                                    // 0x100318be
    {
        const int fmt = vDepthPtr(dst)[1];                                        // 0x100318e3
        if (fmt == 0) {                                                           // 0x100318e9 / 0x100318ed
            G(0x12856c) = reinterpret_cast<int>(vTable565(pal));                  // 0x1003190b
        } else if (fmt == 1) {                                                    // 0x100318ef / 0x100318f3
            G(0x12856c) = reinterpret_cast<int>(vTable555(pal));                  // 0x10031926
        } else {
            vUnlock(dst, 1);                                                      // 0x10031939
            return 1;                                                             // 0x10031943
        }
    }
    const bool negX = G(0x122dc0) < 0;
    const bool negY = G(0x122dc4) < 0;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1003196c / 0x1003196e
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x10031993 / 0x10031995
        // ---- the unscaled half (begins at 0x1003199b)
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x100319b5
        orig = rc;                                                                // 0x100319bd..0x100319d2
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x100319f7 / 0x10031a06
            vUnlock(dst, 1);                                                      // 0x10031a14
            return 0;                                                             // 0x10031a1e
        }
        // The four mirror bodies, chosen at 0x10031a25/0x10031a2c, 0x10031a32/0x10031a39 and the second
        // sign test of the mirrored half (0x10031caf/0x10031cb6), compute the same two cursors and the
        // same row and column counts; they differ in the sign of the source row skip and in the column
        // direction.
        const unsigned char* const s = bits + (rc.left - orig.left) +
                                       (rc.top - orig.top) * G(0x12857c);         // 0x10031a3f..0x10031a5c
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x10031a69..0x10031a86
        G(0x128578) = rc.bottom - rc.top;                                         // 0x10031a8f
        G(0x128564) = rc.right - rc.left;                                         // 0x10031a9b
        // pitch - cols where the two numerators have the same sign (0x10031aa0..0x10031aac), pitch + cols
        // where they differ
        G(0x12853c) = (negX == negY) ? G(0x12857c) - G(0x128564)
                                     : G(0x12857c) + G(0x128564);
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x10031ab2..0x10031ac0
        UnscaledLoop16(s, d, negX ? -1 : 1, !negY, op);
    } else {
        // ---- the 16.16 stepped half (begins at 0x10031f2c)
        const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                            static_cast<double>(Gf(0x128590)));                   // 0x10031f2f..0x10031f38
        const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                            static_cast<double>(Gf(0x12858c)));                   // 0x10031f41..0x10031f4a
        const int oy = Ftol(static_cast<double>(Gf(0x128590)));                   // 0x10031f50 / 0x10031f56
        const int ox = Ftol(static_cast<double>(Gf(0x12858c)));                   // 0x10031f61 / 0x10031f67
        RECT rc;
        RECT orig;
        jSetRect(&rc, x + ox, y + oy, sw, sh);                                    // 0x10031f76
        orig = rc;                                                                // 0x10031f7e..0x10031f93
        if (!jIntersect(&rc, &rc, vClip(dst))) {                                  // 0x10031fb6 / 0x10031fbe
            vUnlock(dst, 1);                                                      // 0x10031fce
            return 0;                                                             // 0x10031fd8
        }
        // The destination cursor is formed with the pitch the preamble parked, BEFORE the pitch is read
        // again into the same global at 0x10032020.
        unsigned short* const d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                  rc.left + rc.top * G(0x12855c);                 // 0x10031fe9..0x10032006
        G(0x12855c) = vPitch(dst);                                                // 0x10032020
        G(0x128564) = rc.right - rc.left;                                         // 0x1003202b
        G(0x128578) = rc.bottom - rc.top;                                         // 0x10032037
        G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                            // 0x1003203d..0x1003204a
        G(0x128588) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                    // 0x1003204f..0x1003206e
        G(0x128540) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                    // 0x10032073..0x10032092
        const int offX = rc.left - orig.left;                                     // 0x100320b1..0x100320b7
        const int offY = rc.top - orig.top;                                       // 0x100320ba..0x100320c0
        G(0x128554) = G(0x128588) * offX;                                         // 0x100320c3..0x100320cd
        G(0x128558) = G(0x128540) * offY;                                         // 0x100320d3..0x100320dd
        G(0x128570) = ((G(0x128554) & 0xffff) + G(0x128588) * G(0x128564)) >> 16; // 0x100320e3..0x100320ff
        G(0x128584) = ((G(0x128558) & 0xffff) + G(0x128540) * G(0x128578)) >> 16; // 0x10032104..0x10032121
        const int pitch = G(0x12857c);
        const int w = SpriteInt(self, 0x30);
        const int h = SpriteInt(self, 0x34);
        const unsigned char* s;
        int* const rowStep = GpInt(0x122e20);                                     // 0x1003217a
        // The four mirror bodies (0x100320b1, 0x100322bb, 0x100324c3 and 0x100326be) are chosen at
        // 0x10032097/0x1003209e, 0x100320a4/0x100320ab and 0x100324b6/0x100324bd.
        if (!negX && !negY) {
            G(0x12853c) = pitch - G(0x128570);                                    // 0x10032127..0x10032133
            s = bits + (G(0x128558) >> 16) * pitch + (G(0x128554) >> 16);         // 0x10032139..0x1003215c
            rowStep[0] = (G(0x128540) >> 16) * pitch - pitch + G(0x12853c);       // 0x1003215f..0x10032180
        } else if (!negX && negY) {
            // the vertical mirror starts on the LAST payload row and still steps the source cursor
            // FORWARD, both inside the row and from row to row (c3bp's finding 2)
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
        *GpInt(0x122e1c) = rowStep[0] + pitch;                                    // 0x10032182..0x10032196
        G(0x128564) = (G(0x128564) - 1) << 16;                                    // 0x100321c0..0x100321ca
        const int stepX = static_cast<unsigned short>(G(0x128588) >> 16);         // 0x100321d2 word [0x1012858a]
        G(0x128580) = stepX;                                                      // 0x100321d9
        G(0x128544) = G(0x128540) << 16;                                          // 0x100321df..0x100321e7
        ScaledLoop16(s, d, stepX, !negX, op);
    }
    vUnlock(dst, 2);                                                              // 0x100328e0
    return 0;                                                                     // 0x100328ea
}

// ------------------------------------------------------------------ the pixel operations

// Every one of the twenty-one starts its pixel block the same way (0x100317c0's addresses, and the same
// instructions at every twin): the two 16-bit values are shifted up by four and split into a
// red-plus-blue group and a green group, so that the arithmetic below cannot carry from one field group
// into the other.
//   `a` is the value the body loads into eax and `b` the one it loads into edx. `swap` says which is
//   which: `mov al, byte ptr [esi]` / `mov ax, word ptr [ecx + eax*2]` (0x10031af8 / 0x10031afd) puts
//   the PALETTE colour in eax; `mov dl, byte ptr [esi]` / `mov ax, word ptr [edi]` / `mov dx, word ptr
//   [ecx + edx*2]` (0x10033098 / 0x1003309a / 0x1003309d in 0x10032d60) puts the DESTINATION pixel
//   there and the Palette colour in edx.
struct Split {
    unsigned arb, ag, brb, bg;
};
inline Split SplitFields(unsigned short a, unsigned short b, unsigned rbMask, unsigned gMask) {
    Split s;
    const unsigned x = static_cast<unsigned>(a) << 4;                             // 0x10031b01
    const unsigned y = static_cast<unsigned>(b) << 4;                             // 0x10031b04
    s.arb = x & rbMask;                                                           // 0x10031b0b
    s.ag = x & gMask;                                                             // 0x10031b10
    s.brb = y & rbMask;                                                           // 0x10031b16
    s.bg = y & gMask;                                                             // 0x10031b1c
    return s;
}

// The tail every one of the twenty-one shares: re-mask the two groups, recombine them, and store the low
// sixteen bits. The twelve RGB555 members shift the result back down by four first (`shr eax, 4`,
// 0x10031b47); the nine RGB565 ones do NOT (c3bu's finding 1, re-read here for each of the nine), so
// what they write is the blend still shifted up by four and truncated to sixteen bits.
inline unsigned short Recombine(unsigned arb, unsigned ag, unsigned rbMask, unsigned gMask,
                                bool shiftBack) {
    arb &= rbMask;                                                                // 0x10031b3a
    ag &= gMask;                                                                  // 0x10031b3f
    const unsigned out = arb + ag;                                                // 0x10031b45
    return static_cast<unsigned short>(shiftBack ? (out >> 4) : out);             // 0x10031b47 / 0x10031b4a
}

// SHAPE 1, the repeated halving: `add eax, ecx / add ebx, edx / shr eax, 1 / shr ebx, 1` run `rounds`
// times against the UNCHANGED b fields (0x10031b22..0x10031b3a in 0x100317c0: three rounds). `a` ends
// with weight 1 / 2^rounds and `b` with the rest.
inline unsigned short BlendHalves(unsigned short a, unsigned short b, int rounds,
                                  unsigned rbMask, unsigned gMask, bool shiftBack) {
    Split s = SplitFields(a, b, rbMask, gMask);
    for (int i = 0; i < rounds; ++i) {
        s.arb = (s.arb + s.brb) >> 1;                                             // 0x10031b22 / 0x10031b26
        s.ag = (s.ag + s.bg) >> 1;                                                // 0x10031b24 / 0x10031b28
    }
    return Recombine(s.arb, s.ag, rbMask, gMask, shiftBack);
}

// SHAPE 2, the weighted sum: `mov esi, eax / shl eax, k / sub eax, esi` multiplies the a group by
// 2^k - 1 (0x10034663 / 0x10034665 / 0x10034668 in 0x10034300, k = 4), `add eax, ecx` adds the b group
// once (0x10034671) and `shr eax, k` divides by 2^k (0x10034675). `extraHalf` adds the further
// `add eax, ecx / shr eax, 1` that eight of the members run afterwards (0x100371dc / 0x100371e0 in
// 0x10036e60), which halves the result against the b group once more.
inline unsigned short BlendWeighted(unsigned short a, unsigned short b, int k, bool extraHalf,
                                    unsigned rbMask, unsigned gMask, bool shiftBack) {
    Split s = SplitFields(a, b, rbMask, gMask);
    s.arb = (((s.arb << k) - s.arb) + s.brb) >> k;                                // 0x10034663..0x10034675
    s.ag = (((s.ag << k) - s.ag) + s.bg) >> k;                                    // 0x1003466a..0x10034678
    if (extraHalf) {
        s.arb = (s.arb + s.brb) >> 1;                                             // 0x100371dc / 0x100371e0
        s.ag = (s.ag + s.bg) >> 1;                                                // 0x100371de / 0x100371e2
    }
    return Recombine(s.arb, s.ag, rbMask, gMask, shiftBack);
}

// SHAPE 3, the nested weighted sum of eight members: the a group is multiplied by 2^k - 1, the b group
// added, the sum multiplied by 2^j, the ORIGINAL a group subtracted once more, the b group added again
// and the whole divided by sixteen (0x1003c9c3..0x1003c9e1 in 0x1003c660, k = 3 and j = 1; k = 2 and
// j = 2 at 0x1003f643..0x1003f663 in 0x1003f2e0). With k = 3, j = 1 that is (13a + 3b) >> 4 and with
// k = 2, j = 2 it is (11a + 5b) >> 4.
inline unsigned short BlendNested(unsigned short a, unsigned short b, int k, int j,
                                  unsigned rbMask, unsigned gMask, bool shiftBack) {
    Split s = SplitFields(a, b, rbMask, gMask);
    const unsigned arb0 = s.arb;                                                  // `mov esi, eax` 0x1003c9c3
    const unsigned ag0 = s.ag;                                                    // `mov esi, ebx` 0x1003c9d0
    s.arb = ((((s.arb << k) - arb0) + s.brb) << j) - arb0;                        // 0x1003c9c5..0x1003c9ce
    s.ag = ((((s.ag << k) - ag0) + s.bg) << j) - ag0;                             // 0x1003c9d2..0x1003c9db
    s.arb = (s.arb + s.brb) >> 4;                                                 // 0x1003c9dd / 0x1003c9e1
    s.ag = (s.ag + s.bg) >> 4;                                                    // 0x1003c9df / 0x1003c9e4
    return Recombine(s.arb, s.ag, rbMask, gMask, shiftBack);
}

// The gate the twenty-one share, byte-identical in all thirty `char flag` members of the family
// (log/c3/alt/c3by_ops.py, one distinct gate): payload bytes from 0xff up are always skipped
// (0x10031ae1), and a non-zero fourth argument, read one byte at a time out of its argument slot
// (`or byte ptr [ebp + 0x14], 0` at 0x10031ae6), skips 0xf8..0xfe as well (0x10031aec).
//
// `Shape` is which of the three blends above runs; `rb`, `g`, `shiftBack` and `swap` are the rest of
// what distinguishes the twenty-one. The colour table index is the payload byte (0x10031af8 /
// 0x10031afd).
enum Shape { ShapeHalves, ShapeWeighted, ShapeNested };

struct OpBlend {
    unsigned char flag;
    Shape shape;
    int p1;              // ShapeHalves: the number of halvings; the other two: k
    int p2;              // ShapeWeighted: 1 to run the extra halving; ShapeNested: j
    unsigned rb;
    unsigned g;
    bool shiftBack;
    bool swap;           // true = the DESTINATION pixel is the one loaded into eax
    void operator()(const unsigned char* s, unsigned short* d) const {
        if (*s < 0xff) {                                                          // 0x10031ae1 / 0x10031ae4
            if (flag != 0 && *s >= 0xf8) return;                                  // 0x10031ae6..0x10031aef
            const unsigned short colour = Tbl()[*s];                              // 0x10031af8 / 0x10031afd
            const unsigned short a = swap ? *d : colour;
            const unsigned short b = swap ? colour : *d;
            if (shape == ShapeHalves) {
                *d = BlendHalves(a, b, p1, rb, g, shiftBack);
            } else if (shape == ShapeWeighted) {
                *d = BlendWeighted(a, b, p1, p2 != 0, rb, g, shiftBack);
            } else {
                *d = BlendNested(a, b, p1, p2, rb, g, shiftBack);
            }
        }
    }
};

// The two field layouts, read off the four `and` instructions of every pixel block (the table in
// log/c3/alt/c3by_cite.py prints the address of the first `and` of each of the twenty-one).
const unsigned RB555 = 0x7c1f0, G555 = 0x3e00;
const unsigned RB565 = 0xf81f0, G565 = 0x7e00;

inline int Run(void* self, void* dst, int x, int y, unsigned char flag, Shape shape, int p1, int p2,
               unsigned rb, unsigned g, bool shiftBack, bool swap) {
    OpBlend op;
    op.flag = flag;
    op.shape = shape;
    op.p1 = p1;
    op.p2 = p2;
    op.rb = rb;
    op.g = g;
    op.shiftBack = shiftBack;
    op.swap = swap;
    return DrawFamily16(self, dst, x, y, op);
}

// ------------------------------------------------------------------ the hooks

// The MinHook trampolines the A/B's "original" arm is called through (one per SG_HOOK below). The fourth
// argument is a char read from its argument slot one byte at a time (`or byte ptr [ebp + 0x14], 0`), so
// the upper three bytes of the slot never matter.
typedef int(__fastcall* Draw4c_t)(void*, void*, void*, int, int, unsigned char);
Draw4c_t Draw317c0_orig;
Draw4c_t Draw32d60_orig;
Draw4c_t Draw34300_orig;
Draw4c_t Draw358b0_orig;
Draw4c_t Draw36e60_orig;
Draw4c_t Draw38460_orig;
Draw4c_t Draw39a60_orig;
Draw4c_t Draw3b060_orig;
Draw4c_t Draw3c660_orig;
Draw4c_t Draw3dca0_orig;
Draw4c_t Draw3f2e0_orig;
Draw4c_t Draw40930_orig;
Draw4c_t Draw49ea0_orig;
Draw4c_t Draw4b430_orig;
Draw4c_t Draw4ca10_orig;
Draw4c_t Draw4dff0_orig;
Draw4c_t Draw4f5d0_orig;
Draw4c_t Draw50bb0_orig;
Draw4c_t Draw521d0_orig;
Draw4c_t Draw537f0_orig;
Draw4c_t Draw54e20_orig;

// 0x000317c0  Sprite::draw16_100317c0(dst, x, y, flag)
// Three halvings on the RGB555 field layout (0x10031b22..0x10031b38) with the result shifted back down
// (0x10031b47) and stored at 0x10031b4a. The Palette colour is the halved side (`mov al, byte ptr
// [esi]`, 0x10031af8), so it is weighted one eighth and the destination seven eighths.
int __fastcall Draw317c0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeHalves, 3, 0, RB555, G555, true, false);
}

// 0x00032d60  Sprite::draw16_10032d60(dst, x, y, flag)
// 0x100317c0 with the two colours exchanged: `mov dl, byte ptr [esi]` / `mov ax, word ptr [edi]` /
// `mov dx, word ptr [ecx + edx*2]` at 0x10033098 / 0x1003309a / 0x1003309d put the DESTINATION pixel in
// eax, so the destination is weighted one eighth and the Palette colour seven eighths
// (0x100330c2..0x100330d8, shifted back at 0x100330e7, stored at 0x100330ea).
int __fastcall Draw32d60_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeHalves, 3, 0, RB555, G555, true, true);
}

// 0x00034300  Sprite::draw16_10034300(dst, x, y, flag)
// (15 * a + b) >> 4 on RGB555 (0x10034662..0x1003467b), with the destination pixel as `a`
// (`mov dl, byte ptr [esi]`, 0x10034638): the destination is weighted fifteen sixteenths. Shifted back
// at 0x10034689, stored at 0x1003468c.
int __fastcall Draw34300_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 4, 0, RB555, G555, true, true);
}

// 0x000358b0  Sprite::draw16_100358b0(dst, x, y, flag)
// 0x10034300's arithmetic (0x10035c12..0x10035c2b) with the Palette colour as `a`
// (`mov al, byte ptr [esi]`, 0x10035be8), so the Palette colour is weighted fifteen sixteenths.
int __fastcall Draw358b0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 4, 0, RB555, G555, true, false);
}

// 0x00036e60  Sprite::draw16_10036e60(dst, x, y, flag)
// ((3 * a + b) >> 2 + b) >> 1 on RGB555 (0x100371c2..0x100371e2), `a` the Palette colour (0x10037198):
// the Palette colour is weighted three eighths.
int __fastcall Draw36e60_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 2, 1, RB555, G555, true, false);
}

// 0x00038460  Sprite::draw16_10038460(dst, x, y, flag)
// 0x10036e60's arithmetic (0x100387c2..0x100387e2) with the destination pixel as `a` (`mov dl, byte ptr
// [esi]`, 0x10038798), so the destination is weighted three eighths.
int __fastcall Draw38460_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 2, 1, RB555, G555, true, true);
}

// 0x00039a60  Sprite::draw16_10039a60(dst, x, y, flag)
// ((7 * a + b) >> 3 + b) >> 1 on RGB555 (0x10039dc2..0x10039de2), `a` the Palette colour (0x10039d98):
// the Palette colour is weighted seven sixteenths.
int __fastcall Draw39a60_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 3, 1, RB555, G555, true, false);
}

// 0x0003b060  Sprite::draw16_1003b060(dst, x, y, flag)
// 0x10039a60's arithmetic (0x1003b3c2..0x1003b3e2) with the destination pixel as `a` (0x1003b398), so
// the destination is weighted seven sixteenths.
int __fastcall Draw3b060_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 3, 1, RB555, G555, true, true);
}

// 0x0003c660  Sprite::draw16_1003c660(dst, x, y, flag)
// The nested form with k = 3 and j = 1 on RGB555 (0x1003c9c2..0x1003c9e7), which is
// (13 * a + 3 * b) >> 4, `a` the destination pixel (0x1003c998): the destination is weighted thirteen
// sixteenths.
int __fastcall Draw3c660_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 3, 1, RB555, G555, true, true);
}

// 0x0003dca0  Sprite::draw16_1003dca0(dst, x, y, flag)
// 0x1003c660's arithmetic (0x1003e002..0x1003e027) with the Palette colour as `a` (0x1003dfd8), so the
// Palette colour is weighted thirteen sixteenths.
int __fastcall Draw3dca0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 3, 1, RB555, G555, true, false);
}

// 0x0003f2e0  Sprite::draw16_1003f2e0(dst, x, y, flag)
// The nested form with k = 2 and j = 2 on RGB555 (0x1003f642..0x1003f669), which is
// (11 * a + 5 * b) >> 4, `a` the destination pixel (0x1003f618): the destination is weighted eleven
// sixteenths.
int __fastcall Draw3f2e0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 2, 2, RB555, G555, true, true);
}

// 0x00040930  Sprite::draw16_10040930(dst, x, y, flag)
// 0x1003f2e0's arithmetic (0x10040c92..0x10040cb9) with the Palette colour as `a` (0x10040c68), so the
// Palette colour is weighted eleven sixteenths.
int __fastcall Draw40930_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 2, 2, RB555, G555, true, false);
}

// 0x00049ea0  Sprite::draw16_10049ea0(dst, x, y, flag)
// (15 * a + b) >> 4 on the RGB565 field layout (masks 0xf81f0 and 0x7e00 at 0x1004a1eb and 0x1004a1f0,
// arithmetic 0x1004a202..0x1004a21b), `a` the Palette colour (`mov al, byte ptr [esi]`, 0x1004a1d8).
// The two groups are recombined at 0x1004a227 and stored at 0x1004a229 WITHOUT the `shr eax, 4` the
// RGB555 members end with. It is the mirror of 0x10048910, which batch c3bu took.
int __fastcall Draw49ea0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 4, 0, RB565, G565, false, false);
}

// 0x0004b430  Sprite::draw16_1004b430(dst, x, y, flag)
// ((3 * a + b) >> 2 + b) >> 1 on RGB565 (0x1004b792..0x1004b7b2), `a` the Palette colour (0x1004b768),
// stored without a final shift at 0x1004b7c1.
int __fastcall Draw4b430_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 2, 1, RB565, G565, false, false);
}

// 0x0004ca10  Sprite::draw16_1004ca10(dst, x, y, flag)
// 0x1004b430's arithmetic (0x1004cd72..0x1004cd92) with the destination pixel as `a` (`mov dl, byte ptr
// [esi]`, 0x1004cd48), stored at 0x1004cda1.
int __fastcall Draw4ca10_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 2, 1, RB565, G565, false, true);
}

// 0x0004dff0  Sprite::draw16_1004dff0(dst, x, y, flag)
// ((7 * a + b) >> 3 + b) >> 1 on RGB565 (0x1004e352..0x1004e372), `a` the Palette colour (0x1004e328),
// stored at 0x1004e381.
int __fastcall Draw4dff0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 3, 1, RB565, G565, false, false);
}

// 0x0004f5d0  Sprite::draw16_1004f5d0(dst, x, y, flag)
// 0x1004dff0's arithmetic (0x1004f932..0x1004f952) with the destination pixel as `a` (0x1004f908),
// stored at 0x1004f961.
int __fastcall Draw4f5d0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeWeighted, 3, 1, RB565, G565, false, true);
}

// 0x00050bb0  Sprite::draw16_10050bb0(dst, x, y, flag)
// The nested form with k = 3 and j = 1 on RGB565 (0x10050f12..0x10050f37), which is
// (13 * a + 3 * b) >> 4, `a` the destination pixel (0x10050ee8), stored without a final shift at
// 0x10050f45.
int __fastcall Draw50bb0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 3, 1, RB565, G565, false, true);
}

// 0x000521d0  Sprite::draw16_100521d0(dst, x, y, flag)
// 0x10050bb0's arithmetic (0x10052532..0x10052557) with the Palette colour as `a` (0x10052508), stored
// at 0x10052565.
int __fastcall Draw521d0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 3, 1, RB565, G565, false, false);
}

// 0x000537f0  Sprite::draw16_100537f0(dst, x, y, flag)
// The nested form with k = 2 and j = 2 on RGB565 (0x10053b52..0x10053b79), which is
// (11 * a + 5 * b) >> 4, `a` the destination pixel (0x10053b28), stored at 0x10053b87.
int __fastcall Draw537f0_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 2, 2, RB565, G565, false, true);
}

// 0x00054e20  Sprite::draw16_10054e20(dst, x, y, flag)
// 0x100537f0's arithmetic (0x10055182..0x100551a9) with the Palette colour as `a` (0x10055158), stored
// at 0x100551b7.
int __fastcall Draw54e20_re(void* self, void*, void* dst, int x, int y, unsigned char flag) {
    return Run(self, dst, x, y, flag, ShapeNested, 2, 2, RB565, G565, false, false);
}

}  // namespace

SG_HOOK("jgld.dll", 0x000317c0, Sprite_draw16_100317c0, Draw317c0_re, Draw317c0_orig);
SG_HOOK("jgld.dll", 0x00032d60, Sprite_draw16_10032d60, Draw32d60_re, Draw32d60_orig);
SG_HOOK("jgld.dll", 0x00034300, Sprite_draw16_10034300, Draw34300_re, Draw34300_orig);
SG_HOOK("jgld.dll", 0x000358b0, Sprite_draw16_100358b0, Draw358b0_re, Draw358b0_orig);
SG_HOOK("jgld.dll", 0x00036e60, Sprite_draw16_10036e60, Draw36e60_re, Draw36e60_orig);
SG_HOOK("jgld.dll", 0x00038460, Sprite_draw16_10038460, Draw38460_re, Draw38460_orig);
SG_HOOK("jgld.dll", 0x00039a60, Sprite_draw16_10039a60, Draw39a60_re, Draw39a60_orig);
SG_HOOK("jgld.dll", 0x0003b060, Sprite_draw16_1003b060, Draw3b060_re, Draw3b060_orig);
SG_HOOK("jgld.dll", 0x0003c660, Sprite_draw16_1003c660, Draw3c660_re, Draw3c660_orig);
SG_HOOK("jgld.dll", 0x0003dca0, Sprite_draw16_1003dca0, Draw3dca0_re, Draw3dca0_orig);
SG_HOOK("jgld.dll", 0x0003f2e0, Sprite_draw16_1003f2e0, Draw3f2e0_re, Draw3f2e0_orig);
SG_HOOK("jgld.dll", 0x00040930, Sprite_draw16_10040930, Draw40930_re, Draw40930_orig);
SG_HOOK("jgld.dll", 0x00049ea0, Sprite_draw16_10049ea0, Draw49ea0_re, Draw49ea0_orig);
SG_HOOK("jgld.dll", 0x0004b430, Sprite_draw16_1004b430, Draw4b430_re, Draw4b430_orig);
SG_HOOK("jgld.dll", 0x0004ca10, Sprite_draw16_1004ca10, Draw4ca10_re, Draw4ca10_orig);
SG_HOOK("jgld.dll", 0x0004dff0, Sprite_draw16_1004dff0, Draw4dff0_re, Draw4dff0_orig);
SG_HOOK("jgld.dll", 0x0004f5d0, Sprite_draw16_1004f5d0, Draw4f5d0_re, Draw4f5d0_orig);
SG_HOOK("jgld.dll", 0x00050bb0, Sprite_draw16_10050bb0, Draw50bb0_re, Draw50bb0_orig);
SG_HOOK("jgld.dll", 0x000521d0, Sprite_draw16_100521d0, Draw521d0_re, Draw521d0_orig);
SG_HOOK("jgld.dll", 0x000537f0, Sprite_draw16_100537f0, Draw537f0_re, Draw537f0_orig);
SG_HOOK("jgld.dll", 0x00054e20, Sprite_draw16_10054e20, Draw54e20_re, Draw54e20_orig);
