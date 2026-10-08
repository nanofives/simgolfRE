// Batch c3bm: jgld.dll render, the Sprite family (the hand-off log/c3/c3bh_notes.txt asked for).
//   0x00025900  Sprite::blitPair           two 8-bit sprite payloads blended onto a 16-bit surface pair
//   0x00025dd0  Sprite::blitPair1          the same blend with one destination
//   0x0001ab40  Sprite::draw16t_1001ab40   a 16-bit sprite payload with a key colour and a destination table
//   0x0001b060  Sprite::draw16t_1001b060   the same shape with a (source, destination) pair table
//   0x0001df80  Sprite::draw16m_1001df80   an 8-bit payload through a Palette table, gated by a mask surface
//   0x0001e970  Sprite::draw16m_1001e970   its twin without the flat-colour arm
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below is a VA,
// as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// Everything the A/B hands these functions is built by re/frida/js/fixtures.d/c3bm.js on jgld's own Surface
// (0x1011d0b0), Palette (0x1011db10) and Sprite (0x1011d380) vtables: never the live Display at 0x10128420,
// never a screen surface, never DirectDraw, never the module's own Palette. log/c3/alt/c3bm_probe.py, one 12 s
// boot at the menu with counters attached straight to the module addresses: 0 entries into all six of these
// functions and into each of their static callers (0x19b90, 0x19aa0, 0x165a0, 0x17b40, 0x157e0, 0x15bb0),
// against 2_047 entries into the live Sprite::draw16_1001bc40 0x1bc40 on the same boot.
//
// THE SHARED LOOP GLOBALS. blitPair, blitPair1 and both draw16m park their loop state in the block
// 0x1012857c (source pitch), 0x1012855c (destination pitch), 0x1012856c (colour table), 0x10128578 (rows),
// 0x10128564 (columns), 0x1012853c (source row skip), 0x10128560 (destination row skip), and the live
// 0x1bc40 writes the same block: the probe measured 523, 524, 524, 130 and 25 changes in twelve seconds.
// Every registry key of this batch therefore runs with the game's render thread suspended and restores the
// block before resuming it (fixture c3bm.js, the recipe c3av established for 0x1012847c). The row counter at
// 0x10128578 is decremented IN MEMORY by the loops below, exactly as `dec dword ptr [0x10128578]` does.
//
// Surface vtable slots used here (0x1011d0b0, named in re/names/jgld_2.tsv):
//   +0x10  lock()      0x10008730: copies the base at this+0x4c0 to this+0x4cc, bumps the use count at
//          this+0x4c8 when it is not NULL (0x10008769), and returns this+0x4cc.
//   +0x18  bits8()     0x1000a750: tail-calls slot +0x10.
//   +0x20  bits()      0x1000a7b0: tail-calls slot +0x10.
//   +0x24  unlock(f)   0x100087b0: this+0x4c8 -= f, clearing this+0x4cc and this+0x4c8 at <= 0.
//   +0xcc  clipRect()  0x1000a990: returns this+0x44.
//   +0xe0  pitch()     0x1000aaf0: returns the int at this+0x40.
//   +0xe4  depthPtr()  0x1000ab30: returns this+0x24, so element [1] is the format code at this+0x28.
// Palette vtable slots (0x1011db10): +0x18 -> this+0x40c (RGB565 table), +0x1c -> this+0x60c (RGB555 table).
// Sprite fields: +0x10 Palette, +0x14 payload, +0x28 key colour, +0x2c pitch, +0x30 width, +0x34 height.
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
// of a copy: setRect 0x10008360 (through thunk 0x10001019), intersect 0x10008590 (thunk 0x10001087), the CRT
// abs 0x1007f370. IntersectRect is called through jgld's OWN import slot at 0x1012c5e4, the one the originals
// call (0x10025a70, 0x1001acaa, 0x1001e21f).
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
inline unsigned char& Gb(unsigned rva) { return *reinterpret_cast<unsigned char*>(JgldBase() + rva); }

inline int SpriteInt(void* s, unsigned off) { return *reinterpret_cast<int*>(reinterpret_cast<char*>(s) + off); }
inline unsigned char* SpriteBits(void* s) {
    return *reinterpret_cast<unsigned char**>(reinterpret_cast<char*>(s) + 0x14);
}
// The Palette of the sprite at +0x10, or the one of the client object the module global 0x1012873c points at
// (0x10025920 / 0x10025931 and the twins in all six functions).
inline void* SpritePalette(void* s) {
    void* p = *reinterpret_cast<void**>(reinterpret_cast<char*>(s) + 0x10);
    if (p) return p;
    char* client = *reinterpret_cast<char**>(JgldBase() + 0x12873c);
    return *reinterpret_cast<void**>(client + 4);
}

// VC6 converts a double to an int with __ftol (jgld's own copy is 0x1007f12c): a 64-bit fistp with the low
// dword kept, i.e. truncation toward zero.
inline int Ftol(double v) { return static_cast<int>(static_cast<long long>(v)); }

// ------------------------------------------------------------------ 0x00025900 / 0x00025dd0, the pair blend

// One pixel of the blend at 0x10025bae..0x10025c44 (blitPair) and 0x10026054..0x100260ea (blitPair1). The two
// bodies are byte for byte the same arithmetic; only the store target differs.
//
//   edx holds the accumulator with the source colour parked in its high half (`mov dx, bp` 0x10025bb9,
//   `shl edx, 0x10` 0x10025bbc), ecx the destination colour in ITS high half with the weight in cl
//   (0x10025bbf..0x10025bc5). Each of the three channel steps takes the five-bit field of each colour as the
//   mask 0xf8 of a shifted copy, multiplies the DESTINATION field by the weight with an 8-bit `mul cl`
//   (so only al takes part: 0x10025bd6, 0x10025c05, 0x10025c34), keeps the high byte of that product
//   (`shr ax, 8`), ADDS the source field unweighted, and divides by 8. The sum reaches 0x1ee, so the result of
//   a channel reaches 0x3d and overflows its five bits into the next field: that is what the original does.
inline unsigned short PairBlend(unsigned short src, unsigned short dst, unsigned char w) {
    unsigned short acc = 0;                                                        // dx after 0x10025bbc
    unsigned short t;
    // bits 15..10: the 0xf8 mask of colour >> 7
    t = static_cast<unsigned short>(static_cast<unsigned char>((dst >> 7) & 0xf8) * w);   // 0x10025bd0/0x10025bd6
    t = static_cast<unsigned short>(t >> 8);                                              // 0x10025bd8
    t = static_cast<unsigned short>(t + static_cast<unsigned short>((src >> 7) & 0xf8));  // 0x10025bdc
    t = static_cast<unsigned short>(t >> 3);                                              // 0x10025bdf
    acc = static_cast<unsigned short>(acc | static_cast<unsigned short>(t << 10));        // 0x10025be3/0x10025be7
    // bits 9..5: the 0xf8 mask of colour >> 2
    t = static_cast<unsigned short>(static_cast<unsigned char>((dst >> 2) & 0xf8) * w);   // 0x10025bfd/0x10025c05
    t = static_cast<unsigned short>(t >> 8);                                              // 0x10025c07
    t = static_cast<unsigned short>(t + static_cast<unsigned short>((src >> 2) & 0xf8));  // 0x10025c0b
    t = static_cast<unsigned short>(t >> 3);                                              // 0x10025c0e
    acc = static_cast<unsigned short>(acc | static_cast<unsigned short>(t << 5));         // 0x10025c12/0x10025c16
    // bits 4..0: the 0xf8 mask of colour << 3
    t = static_cast<unsigned short>(
            static_cast<unsigned char>(static_cast<unsigned short>(dst << 3) & 0xf8) * w);  // 0x10025c2c/0x10025c34
    t = static_cast<unsigned short>(t >> 8);                                                // 0x10025c36
    t = static_cast<unsigned short>(
            t + static_cast<unsigned short>(static_cast<unsigned short>(src << 3) & 0xf8)); // 0x10025c3a
    t = static_cast<unsigned short>(t >> 3);                                                // 0x10025c3d
    acc = static_cast<unsigned short>(acc | t);                                             // 0x10025c41
    return acc;
}

// The preamble both pair blitters share: the Palette, the two pitches and the colour table. Returns 0 to go on,
// or the status the function must return (0x10025958 -> 16, 0x10025a0a -> 1).
inline int PairPreamble(void* self, void* dst, int* status) {
    void* pal = SpritePalette(self);                                              // 0x10025920 / 0x10025931
    if (pal == 0) {                                                               // 0x1002593c
        vUnlock(dst, 1);                                                          // 0x1002594e
        *status = 16;                                                             // 0x10025958
        return 1;
    }
    G(0x12857c) = SpriteInt(self, 0x2c);                                          // 0x10025968
    G(0x12855c) = vPitch(dst);                                                    // 0x10025985
    const int fmt = vDepthPtr(dst)[1];                                            // 0x1002599d / 0x100259aa
    if (fmt == 0) {                                                               // 0x100259b0
        G(0x12856c) = reinterpret_cast<int>(vTable565(pal));                      // 0x100259d2
    } else if (fmt == 1) {                                                        // 0x100259b6
        G(0x12856c) = reinterpret_cast<int>(vTable555(pal));                      // 0x100259ed
    } else {
        vUnlock(dst, 1);                                                          // 0x10025a00
        *status = 1;                                                              // 0x10025a0a
        return 1;
    }
    return 0;
}

// The MinHook trampolines the A/B's "original" arm is called through (one per SG_HOOK below).
typedef int(__fastcall* BlitPair_t)(void*, void*, void*, void*, void*, int, int);
BlitPair_t BlitPair_orig;
typedef int(__fastcall* BlitPair1_t)(void*, void*, void*, void*, int, int);
BlitPair1_t BlitPair1_orig;
typedef int(__fastcall* Draw16t_t)(void*, void*, void*, int, int, void*);
Draw16t_t Draw16tAb40_orig;
Draw16t_t Draw16tB060_orig;
typedef int(__fastcall* Draw16mDf80_t)(void*, void*, void*, int, int, void*, unsigned char, unsigned short);
Draw16mDf80_t Draw16mDf80_orig;
typedef int(__fastcall* Draw16mE970_t)(void*, void*, void*, int, int, void*, unsigned char);
Draw16mE970_t Draw16mE970_orig;

// 0x00025900  Sprite::blitPair(other, dst, dst2, x, y)
// `this` supplies the colour byte, `other` the per-pixel weight, `dst` the pixel read into the blend and
// `dst2` the pixel written: BOTH arms of the inner test store through the cursor reloaded from [ebp-0x2c]
// (0x10025c4b and 0x10025c5d), never through edi.
int __fastcall BlitPair_re(void* self, void*, void* other, void* dst, void* dst2, int x, int y) {
    int status = 0;
    if (PairPreamble(self, dst, &status)) return status;
    RECT rc;
    RECT orig;
    jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));            // 0x10025a2e
    orig = rc;                                                                    // 0x10025a36
    if (!jIntersectRect(&rc, &rc, vClip(dst))) {                                  // 0x10025a70 / 0x10025a7f
        vUnlock(dst, 1);                                                          // 0x10025a8d
        return 0;                                                                 // 0x10025a97
    }
    const int dl = rc.left - orig.left;
    const int dt = rc.top - orig.top;
    const unsigned char* s = SpriteBits(self) + dl + dt * G(0x12857c);            // 0x10025a9e
    const unsigned char* s2 = SpriteBits(other) + dl + dt * G(0x12857c);          // 0x10025abe
    const unsigned short* d =
        reinterpret_cast<unsigned short*>(vBits(dst)) + rc.left + rc.top * G(0x12855c);    // 0x10025ae8
    unsigned short* d2 =
        reinterpret_cast<unsigned short*>(vBits(dst2)) + rc.left + rc.top * G(0x12855c);   // 0x10025b12
    G(0x128578) = rc.bottom - rc.top;                                             // 0x10025b38
    G(0x128564) = rc.right - rc.left;                                             // 0x10025b44
    G(0x12853c) = G(0x12857c) - G(0x128564);                                      // 0x10025b55
    G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                                // 0x10025b69
    const unsigned short* tbl = reinterpret_cast<const unsigned short*>(G(0x12856c));      // 0x10025b82
    for (;;) {
        int cols = G(0x128564);                                                   // 0x10025b88
        for (;;) {
            const unsigned char sv = *s;
            const unsigned char w = *s2;
            if (sv < 0xff && w < 0xff) {                                          // 0x10025b91 / 0x10025b9a
                if (w == 0) {                                                     // 0x10025ba8
                    *d2 = tbl[sv];                                                // 0x10025c57 / 0x10025c60
                } else {
                    *d2 = PairBlend(tbl[sv], *d, w);                              // 0x10025bb0..0x10025c4e
                }
            }
            ++s;                                                                  // 0x10025c65
            ++s2;                                                                 // 0x10025c66
            ++d;                                                                  // 0x10025c67
            ++d2;                                                                 // 0x10025c6b
            if (--cols == 0) break;                                               // 0x10025c75
        }
        s += G(0x12853c);                                                         // 0x10025c7c
        s2 += G(0x12853c);                                                        // 0x10025c82
        d = reinterpret_cast<const unsigned short*>(
                reinterpret_cast<const char*>(d) + G(0x128560));                  // 0x10025c88
        d2 = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d2) + G(0x128560));                       // 0x10025c92
        if (--G(0x128578) == 0) break;                                            // 0x10025c9c
    }
    vUnlock(dst, 2);                                                              // 0x10025cb8
    return 0;                                                                     // 0x10025cc2
}

// 0x00025dd0  Sprite::blitPair1(other, dst, x, y)
// The same body with one destination: the blended word and the plain table lookup both go to the cursor the
// loop walks (0x100260ea and 0x100260f7).
int __fastcall BlitPair1_re(void* self, void*, void* other, void* dst, int x, int y) {
    int status = 0;
    if (PairPreamble(self, dst, &status)) return status;                          // 0x10025df0..0x10025f04
    RECT rc;
    RECT orig;
    jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));            // 0x10025efe
    orig = rc;                                                                    // 0x10025f06
    if (!jIntersectRect(&rc, &rc, vClip(dst))) {                                  // 0x10025f40 / 0x10025f4f
        vUnlock(dst, 1);                                                          // 0x10025f5d
        return 0;                                                                 // 0x10025f67
    }
    const int dl = rc.left - orig.left;
    const int dt = rc.top - orig.top;
    const unsigned char* s = SpriteBits(self) + dl + dt * G(0x12857c);            // 0x10025f6e
    const unsigned char* s2 = SpriteBits(other) + dl + dt * G(0x12857c);          // 0x10025f8e
    unsigned short* d =
        reinterpret_cast<unsigned short*>(vBits(dst)) + rc.left + rc.top * G(0x12855c);    // 0x10025fb8
    G(0x128578) = rc.bottom - rc.top;                                             // 0x10025fde
    G(0x128564) = rc.right - rc.left;                                             // 0x10025fea
    G(0x12853c) = G(0x12857c) - G(0x128564);                                      // 0x10025ffb
    G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                                // 0x1002600f
    const unsigned short* tbl = reinterpret_cast<const unsigned short*>(G(0x12856c));      // 0x10026028
    for (;;) {
        int cols = G(0x128564);                                                   // 0x1002602e
        for (;;) {
            const unsigned char sv = *s;
            const unsigned char w = *s2;
            if (sv < 0xff && w < 0xff) {                                          // 0x10026037 / 0x10026040
                if (w == 0) {                                                     // 0x1002604e
                    *d = tbl[sv];                                                 // 0x100260f7
                } else {
                    *d = PairBlend(tbl[sv], *d, w);                               // 0x10026056..0x100260ea
                }
            }
            ++s;                                                                  // 0x100260fa
            ++s2;                                                                 // 0x100260fb
            ++d;                                                                  // 0x100260fc
            if (--cols == 0) break;                                               // 0x100260ff
        }
        s += G(0x12853c);                                                         // 0x10026106
        s2 += G(0x12853c);                                                        // 0x1002610c
        d = reinterpret_cast<unsigned short*>(
                reinterpret_cast<char*>(d) + G(0x128560));                        // 0x10026112
        if (--G(0x128578) == 0) break;                                            // 0x10026118
    }
    vUnlock(dst, 2);                                                              // 0x10026134
    return 0;                                                                     // 0x1002613e
}

// ------------------------------------------------------------------ 0x0001ab40 / 0x0001b060, draw16t

// The two scale ratios both draw16t functions compute into 0x10128518 and 0x1012851c: `fild` of the CRT abs of
// the numerator, `fidiv` by the denominator at 0x10122dc8, `fstp dword` (0x1001ab88..0x1001ab91 and
// 0x1001aba9..0x1001abb2). The quotient is formed at the x87 precision the A/B thread runs under and rounded
// to float only by the store, so each operand is widened to double and the result assigned to a float, the
// form batch c3ap established.
inline void Draw16tRatios() {
    const int a0 = jAbs(G(0x122dc0));                                             // 0x1001ab7d
    Gf(0x128518) = static_cast<float>(static_cast<double>(a0) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1001ab88 / 0x1001ab91
    const int a4 = jAbs(G(0x122dc4));                                             // 0x1001ab9e
    Gf(0x12851c) = static_cast<float>(static_cast<double>(a4) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1001aba9 / 0x1001abb2
}

// The scaled half both draw16t functions end in (0x1001addc..0x1001af26 and 0x1001b305..0x1001b44f): it lays
// out the stepped rectangle and parks the two 16.16 steps, and contains no pixel loop at all. The destination
// address it computes at 0x1001ae7f is stored into a local that nothing reads; the slot-+0x20 call is kept
// because it bumps the surface's use count. Returns 1 when the rectangle is empty.
inline int Draw16tScaled(void* self, void* dst, int x, int y) {
    const int sh = Ftol(static_cast<double>(SpriteInt(self, 0x34)) *
                        static_cast<double>(Gf(0x12851c)));                       // 0x1001addf..0x1001ade8
    const int sw = Ftol(static_cast<double>(SpriteInt(self, 0x30)) *
                        static_cast<double>(Gf(0x128518)));                       // 0x1001adf1..0x1001adfa
    RECT rc;
    RECT orig;
    jSetRect(&rc, x, y, sw, sh);                                                  // 0x1001ae0c
    orig = rc;                                                                    // 0x1001ae14
    (void)orig;                                                                   // the copy nothing reads back
    if (!jIntersect(&rc, &rc, vClip(dst))) {                                      // 0x1001ae4c / 0x1001ae56
        vUnlock(dst, 1);                                                          // 0x1001ae64
        return 1;                                                                 // 0x1001ae6e
    }
    (void)(reinterpret_cast<unsigned short*>(vBits(dst)) + rc.left + rc.top * G(0x1284ec));  // 0x1001ae7f
    G(0x1284ec) = vPitch(dst);                                                    // 0x1001aeb6
    G(0x1284f4) = rc.right - rc.left;                                             // 0x1001aec1
    G(0x128508) = rc.bottom - rc.top;                                             // 0x1001aecd
    G(0x1284f0) = G(0x1284ec) - G(0x1284f4);                                      // 0x1001aede
    G(0x128514) = (G(0x122dc8) << 16) / jAbs(G(0x122dc0));                        // 0x1001aee3..0x1001af02
    G(0x1284dc) = (G(0x122dc8) << 16) / jAbs(G(0x122dc4));                        // 0x1001af07..0x1001af26
    return 0;
}

// 0x0001ab40  Sprite::draw16t_1001ab40(dst, x, y, table)
// A 16-bit payload copied onto a 16-bit surface: the LOW BYTE of the source word selects the arm. Equal to the
// sprite's key colour at +0x28 the pixel is left alone, zero replaces the destination pixel by the `table`
// entry that pixel indexes, anything else copies the source word.
int __fastcall Draw16tAb40_re(void* self, void*, void* dst, int x, int y, void* table) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0) return 3;                                          // 0x1001ab60 / 0x1001ab6a
    Draw16tRatios();
    Gw(0x1284f8) = *reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(self) + 0x28);  // 0x1001abbb movzx
    G(0x12850c) = SpriteInt(self, 0x2c);                                          // 0x1001abcd
    G(0x1284ec) = vPitch(dst);                                                    // 0x1001abe9
    G(0x1284fc) = reinterpret_cast<int>(table);                                   // 0x1001abfa
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1001ac21
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1001ac48
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x1001ac68
        orig = rc;                                                                // 0x1001ac70
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x1001acaa / 0x1001acb9
            vUnlock(dst, 1);                                                      // 0x1001acc7
            return 0;                                                             // 0x1001acd1
        }
        if (G(0x122dc0) >= 0 && G(0x122dc4) >= 0) {                               // 0x1001acdf / 0x1001acec
            const unsigned short* s = reinterpret_cast<unsigned short*>(bits) +
                                      (rc.left - orig.left) +
                                      (rc.top - orig.top) * G(0x12850c);          // 0x1001acf2
            unsigned short* d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                rc.left + rc.top * G(0x1284ec);                   // 0x1001ad1e
            G(0x128508) = rc.bottom - rc.top;                                     // 0x1001ad44
            G(0x1284f4) = rc.right - rc.left;                                     // 0x1001ad50
            G(0x1284d8) = (G(0x12850c) - G(0x1284f4)) * 2;                        // 0x1001ad63
            G(0x1284f0) = (G(0x1284ec) - G(0x1284f4)) * 2;                        // 0x1001ad77
            const unsigned char key = Gb(0x1284f8);                               // 0x1001ad87 mov al
            const int srcSkip = G(0x1284d8);                                      // 0x1001ad8c
            const unsigned short* tbl = reinterpret_cast<const unsigned short*>(G(0x1284fc));  // 0x1001ad93
            for (;;) {
                int cols = G(0x1284f4);                                           // 0x1001ad99
                for (;;) {
                    const unsigned char lo = *reinterpret_cast<const unsigned char*>(s);
                    if (lo != key) {                                              // 0x1001ad9f
                        if (lo == 0) {                                            // 0x1001ada3
                            *d = tbl[*d];                                         // 0x1001ada8..0x1001adb0
                        } else {
                            *d = *s;                                              // 0x1001adb5 / 0x1001adb8
                        }
                    }
                    ++s;                                                          // 0x1001adbb
                    ++d;                                                          // 0x1001adbe
                    if (--cols == 0) break;                                       // 0x1001adc1
                }
                s = reinterpret_cast<const unsigned short*>(
                        reinterpret_cast<const char*>(s) + srcSkip);              // 0x1001adc4
                d = reinterpret_cast<unsigned short*>(
                        reinterpret_cast<char*>(d) + G(0x1284f0));                // 0x1001adc6
                if (--G(0x128508) == 0) break;                                    // 0x1001adcc
            }
        }
    } else {
        if (Draw16tScaled(self, dst, x, y)) return 0;                             // 0x1001addc..0x1001af26
    }
    vUnlock(dst, 1);                                                              // 0x1001af37
    return 0;                                                                     // 0x1001af41
}

// 0x0001b060  Sprite::draw16t_1001b060(dst, x, y, table)
// The same shape with three differences: the destination is locked in the guard (so a surface with no pixel
// base returns 3), the sprite's key colour at +0x28 is OVERWRITTEN with 0xff, and the table is indexed by the
// 32-bit value (source << 16) | destination taken as a BYTE offset (0x1001b2d2..0x1001b2dd).
int __fastcall Draw16tB060_re(void* self, void*, void* dst, int x, int y, void* table) {
    unsigned char* const bits = SpriteBits(self);
    if (bits == 0 || dst == 0 || !vLock(dst)) return 3;                           // 0x1001b084/0x1001b08a/0x1001b0a2
    Draw16tRatios();                                                              // 0x1001b0b4..0x1001b0e9
    *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x28) = 0xff;         // 0x1001b0f2
    G(0x12850c) = SpriteInt(self, 0x2c);                                          // 0x1001b0ff
    G(0x1284ec) = vPitch(dst);                                                    // 0x1001b11c
    G(0x1284fc) = reinterpret_cast<int>(table);                                   // 0x1001b12d
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1001b154
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1001b17b
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x1001b19b
        orig = rc;
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x1001b1dd / 0x1001b1ec
            vUnlock(dst, 1);                                                      // 0x1001b1fa
            return 0;                                                             // 0x1001b204
        }
        if (G(0x122dc0) >= 0 && G(0x122dc4) >= 0) {                               // 0x1001b212 / 0x1001b21f
            const unsigned short* s = reinterpret_cast<unsigned short*>(bits) +
                                      (rc.left - orig.left) +
                                      (rc.top - orig.top) * G(0x12850c);
            unsigned short* d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                rc.left + rc.top * G(0x1284ec);
            G(0x128508) = rc.bottom - rc.top;
            G(0x1284f4) = rc.right - rc.left;
            G(0x1284d8) = (G(0x12850c) - G(0x1284f4)) * 2;
            G(0x1284f0) = (G(0x1284ec) - G(0x1284f4)) * 2;                        // 0x1001b2aa
            const int srcSkip = G(0x1284d8);                                      // 0x1001b2ba
            const char* tbl = reinterpret_cast<const char*>(G(0x1284fc));         // 0x1001b2c1
            for (;;) {
                int cols = G(0x1284f4);                                           // 0x1001b2c7
                for (;;) {
                    if (*reinterpret_cast<const unsigned char*>(s) != 0xff) {     // 0x1001b2cd
                        const unsigned idx =
                            (static_cast<unsigned>(*s) << 16) | *d;               // 0x1001b2d4..0x1001b2da
                        *d = *reinterpret_cast<const unsigned short*>(tbl + idx); // 0x1001b2dd / 0x1001b2e1
                    }
                    ++s;                                                          // 0x1001b2e4
                    ++d;                                                          // 0x1001b2e7
                    if (--cols == 0) break;                                       // 0x1001b2ea
                }
                s = reinterpret_cast<const unsigned short*>(
                        reinterpret_cast<const char*>(s) + srcSkip);              // 0x1001b2ed
                d = reinterpret_cast<unsigned short*>(
                        reinterpret_cast<char*>(d) + G(0x1284f0));                // 0x1001b2ef
                if (--G(0x128508) == 0) break;                                    // 0x1001b2f5
            }
        }
    } else {
        if (Draw16tScaled(self, dst, x, y)) return 0;                             // 0x1001b305..0x1001b44f
    }
    vUnlock(dst, 1);                                                              // 0x1001b460
    return 0;                                                                     // 0x1001b46a
}

// ------------------------------------------------------------------ 0x0001df80 / 0x0001e970, draw16m

// The preamble both draw16m functions share, up to and including the colour-table switch. `*status` carries
// the value to return: 3 for a missing payload, destination or mask (0x1001dfc7 and 0x1001e005), 16 for a
// missing Palette (0x1001e060) and 1 for a destination format the switch does not know (0x1001e16b, which is
// the one exit of either function that unlocks NOTHING).
inline int Draw16mPreamble(void* self, void* dst, void* mask, void** palOut, int* status) {
    if (SpriteBits(self) == 0 || dst == 0 || !vLock(dst)) {                       // 0x1001dfa7/0x1001dfad/0x1001dfc5
        *status = 3;                                                              // 0x1001dfc7
        return 1;
    }
    if (mask == 0 || !vLock(mask)) {                                              // 0x1001dfd5 / 0x1001dfed
        vUnlock(dst, 1);                                                          // 0x1001dffb
        *status = 3;                                                              // 0x1001e005
        return 1;
    }
    void* pal = SpritePalette(self);                                              // 0x1001e012 / 0x1001e023
    if (pal == 0) {                                                               // 0x1001e032
        vUnlock(mask, 2);                                                         // 0x1001e040
        vUnlock(dst, 1);                                                          // 0x1001e056
        *status = 16;                                                             // 0x1001e060
        return 1;
    }
    *palOut = pal;                                                                // kept in [ebp-0x34]
    const int a0 = jAbs(G(0x122dc0));                                             // 0x1001e070
    Gf(0x12858c) = static_cast<float>(static_cast<double>(a0) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1001e07b / 0x1001e084
    const int a4 = jAbs(G(0x122dc4));                                             // 0x1001e091
    Gf(0x128590) = static_cast<float>(static_cast<double>(a4) /
                                      static_cast<double>(G(0x122dc8)));          // 0x1001e09c / 0x1001e0a5
    G(0x12857c) = SpriteInt(self, 0x2c);                                          // 0x1001e0b1
    G(0x12855c) = vPitch(dst);                                                    // 0x1001e0cd
    G(0x128538) = vPitch(mask);                                                   // 0x1001e0e9
    return 0;
}

// The colour-table switch (0x1001e114..0x1001e170 and 0x1001eafa..0x1001eb56). 1 means "return 1".
inline int Draw16mTable(void* pal, void* dst, int* status) {
    const int fmt = vDepthPtr(dst)[1];                                            // 0x1001e114 / 0x1001e121
    if (fmt == 0) {                                                               // 0x1001e127
        G(0x12856c) = reinterpret_cast<int>(vTable565(pal));                      // 0x1001e149
    } else if (fmt == 1) {                                                        // 0x1001e12d
        G(0x12856c) = reinterpret_cast<int>(vTable555(pal));                      // 0x1001e164
    } else {
        *status = 1;                                                              // 0x1001e16b: no unlock at all
        return 1;
    }
    return 0;
}

// 0x0001df80  Sprite::draw16m_1001df80(dst, x, y, mask, level, color)
// An 8-bit payload drawn through the Palette table onto a 16-bit surface, with a second 8-bit surface as a
// per-pixel gate: where the mask byte is below `level` only the payload value 0xfe is drawn, and then as the
// flat `color`; where it is at or above `level` every value but 0xfe is drawn through the table. 0xff is
// transparent in both cases.
//
// Only the quadrant with both scale numerators non-negative can be driven: the three mirror bodies at
// 0x1001e3b6, 0x1001e4d6 and 0x1001e5f1 read their mask cursor from the local at [ebp-0x30], which is written
// ONLY by this quadrant (0x1001e2ea, the sole store in the function), so in the other three it still holds the
// debug build's frame fill 0xcccccccc and the first `cmp byte ptr [ecx], bl` faults. Reimplemented here as the
// original is written, not as it would have to be written to work.
int __fastcall Draw16mDf80_re(void* self, void*, void* dst, int x, int y, void* mask,
                              unsigned char level, unsigned short color) {
    int status = 0;
    void* pal = 0;
    if (Draw16mPreamble(self, dst, mask, &pal, &status)) return status;
    Gb(0x128568) = level;                                                         // 0x1001e0fa
    Gw(0x128530) = color;                                                         // 0x1001e104
    if (Draw16mTable(pal, dst, &status)) return status;
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1001e196
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1001e1bd
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x1001e1dd
        orig = rc;                                                                // 0x1001e1e5
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x1001e21f / 0x1001e22e
            vUnlock(mask, 2);                                                     // 0x1001e23c
            vUnlock(dst, 1);                                                      // 0x1001e252
            return 0;                                                             // 0x1001e25c
        }
        if (G(0x122dc0) >= 0 && G(0x122dc4) >= 0) {                               // 0x1001e26a / 0x1001e277
            const unsigned char* s = SpriteBits(self) + (rc.left - orig.left) +
                                     (rc.top - orig.top) * G(0x12857c);           // 0x1001e27d
            unsigned short* d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                rc.left + rc.top * G(0x12855c);                   // 0x1001e2b1
            const unsigned char* m = reinterpret_cast<unsigned char*>(vBits8(mask)) +
                                     rc.left + rc.top * G(0x128538);              // 0x1001e2d9 / 0x1001e2ea
            G(0x128578) = rc.bottom - rc.top;                                     // 0x1001e2f3
            G(0x128564) = rc.right - rc.left;                                     // 0x1001e2ff
            G(0x12853c) = G(0x12857c) - G(0x128564);                              // 0x1001e310
            G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                        // 0x1001e324
            G(0x128534) = G(0x128538) - G(0x128564);                              // 0x1001e335
            const unsigned short flat = Gw(0x128530);                             // 0x1001e34c
            const unsigned char lv = Gb(0x128568);                                // 0x1001e34f
            const unsigned short* tbl = reinterpret_cast<const unsigned short*>(G(0x12856c));  // 0x1001e356
            for (;;) {
                int cols = G(0x128564);                                           // 0x1001e35c
                for (;;) {
                    const unsigned char sv = *s;
                    if (sv != 0xff) {                                             // 0x1001e362
                        if (*m < lv) {                                            // 0x1001e367
                            if (sv == 0xfe) *d = flat;                            // 0x1001e36b / 0x1001e373
                        } else if (sv != 0xfe) {                                  // 0x1001e37b
                            *d = tbl[sv];                                         // 0x1001e383 / 0x1001e389
                        }
                    }
                    ++s;                                                          // 0x1001e38c
                    ++d;                                                          // 0x1001e38d
                    ++m;                                                          // 0x1001e390
                    if (--cols == 0) break;                                       // 0x1001e391
                }
                s += G(0x12853c);                                                 // 0x1001e398
                d = reinterpret_cast<unsigned short*>(
                        reinterpret_cast<char*>(d) + G(0x128560));                // 0x1001e39e
                m += G(0x128534);                                                 // 0x1001e3a4
                if (--G(0x128578) == 0) break;                                    // 0x1001e3aa
            }
        }
    } else {
        vUnlock(mask, 2);                                                         // 0x1001e709
        vUnlock(dst, 2);                                                          // 0x1001e71f
    }
    vUnlock(dst, 2);                                                              // 0x1001e735
    vUnlock(mask, 2);                                                             // 0x1001e74b
    return 0;                                                                     // 0x1001e755
}

// 0x0001e970  Sprite::draw16m_1001e970(dst, x, y, mask, level)
// The same drawing with no flat-colour arm: a payload byte below 0xfe is drawn through the Palette table
// wherever the mask byte is at or above `level`, and nothing else is written. The three mirror bodies at
// 0x1001ed81, 0x1001ee86 and 0x1001ef80 fault for the same reason as the twin's: the mask cursor at
// [ebp-0x30] is written only by the quadrant below (0x1001ecd0).
int __fastcall Draw16mE970_re(void* self, void*, void* dst, int x, int y, void* mask, unsigned char level) {
    int status = 0;
    void* pal = 0;
    if (Draw16mPreamble(self, dst, mask, &pal, &status)) return status;           // 0x1001e993..0x1001ead9
    Gb(0x128568) = level;                                                         // 0x1001eaea
    if (Draw16mTable(pal, dst, &status)) return status;                           // 0x1001eafa..0x1001eb51
    if (jAbs(G(0x122dc0)) == jAbs(G(0x122dc8)) &&                                 // 0x1001eb7c
        jAbs(G(0x122dc4)) == jAbs(G(0x122dc8))) {                                 // 0x1001eba3
        RECT rc;
        RECT orig;
        jSetRect(&rc, x, y, SpriteInt(self, 0x30), SpriteInt(self, 0x34));        // 0x1001ebc3
        orig = rc;
        if (!jIntersectRect(&rc, &rc, vClip(dst))) {                              // 0x1001ec05 / 0x1001ec14
            vUnlock(mask, 2);                                                     // 0x1001ec22
            vUnlock(dst, 1);                                                      // 0x1001ec38
            return 0;                                                             // 0x1001ec42
        }
        if (G(0x122dc0) >= 0 && G(0x122dc4) >= 0) {                               // 0x1001ec50 / 0x1001ec5d
            const unsigned char* s = SpriteBits(self) + (rc.left - orig.left) +
                                     (rc.top - orig.top) * G(0x12857c);           // 0x1001ec63
            unsigned short* d = reinterpret_cast<unsigned short*>(vBits(dst)) +
                                rc.left + rc.top * G(0x12855c);                   // 0x1001ec8d
            const unsigned char* m = reinterpret_cast<unsigned char*>(vBits8(mask)) +
                                     rc.left + rc.top * G(0x128538);              // 0x1001ecb7 / 0x1001ecd0
            G(0x128578) = rc.bottom - rc.top;                                     // 0x1001ecd9
            G(0x128564) = rc.right - rc.left;                                     // 0x1001ece5
            G(0x12853c) = G(0x12857c) - G(0x128564);                              // 0x1001ecf6
            G(0x128560) = (G(0x12855c) - G(0x128564)) * 2;                        // 0x1001ed0a
            G(0x128534) = G(0x128538) - G(0x128564);                              // 0x1001ed1b
            const unsigned char lv = Gb(0x128568);                                // 0x1001ed2f
            const unsigned short* tbl = reinterpret_cast<const unsigned short*>(G(0x12856c));  // 0x1001ed36
            for (;;) {
                int cols = G(0x128564);                                           // 0x1001ed3c
                for (;;) {
                    const unsigned char sv = *s;
                    if (sv < 0xfe && *m >= lv) {                                  // 0x1001ed42 / 0x1001ed47
                        *d = tbl[sv];                                             // 0x1001ed4f / 0x1001ed55
                    }
                    ++s;                                                          // 0x1001ed57
                    ++d;                                                          // 0x1001ed58
                    ++m;                                                          // 0x1001ed5b
                    if (--cols == 0) break;                                       // 0x1001ed5c
                }
                s += G(0x12853c);                                                 // 0x1001ed5f
                d = reinterpret_cast<unsigned short*>(
                        reinterpret_cast<char*>(d) + G(0x128560));                // 0x1001ed65
                m += G(0x128534);                                                 // 0x1001ed6b
                if (--G(0x128578) == 0) break;                                    // 0x1001ed71
            }
        }
    } else {
        vUnlock(mask, 2);                                                         // 0x1001f083
        vUnlock(dst, 2);                                                          // 0x1001f099
    }
    vUnlock(dst, 2);                                                              // 0x1001f0af
    vUnlock(mask, 2);                                                             // 0x1001f0c5
    return 0;                                                                     // 0x1001f0cf
}

}  // namespace

SG_HOOK("jgld.dll", 0x00025900, Sprite_blitPair, BlitPair_re, BlitPair_orig);
SG_HOOK("jgld.dll", 0x00025dd0, Sprite_blitPair1, BlitPair1_re, BlitPair1_orig);
SG_HOOK("jgld.dll", 0x0001ab40, Sprite_draw16t_1001ab40, Draw16tAb40_re, Draw16tAb40_orig);
SG_HOOK("jgld.dll", 0x0001b060, Sprite_draw16t_1001b060, Draw16tB060_re, Draw16tB060_orig);
SG_HOOK("jgld.dll", 0x0001df80, Sprite_draw16m_1001df80, Draw16mDf80_re, Draw16mDf80_orig);
SG_HOOK("jgld.dll", 0x0001e970, Sprite_draw16m_1001e970, Draw16mE970_re, Draw16mE970_orig);
