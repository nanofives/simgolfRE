// Batch c3bh: jgld.dll render.
//   0x00012db0  Surface::blitT_8to16   the 8-bit -> 16-bit block copy batch c3bc deferred for size
//                                      (log/c3/c3bc_notes.txt, "First thing to offer the next jgld render batch")
//   0x0000ed70  Surface::blitT_16to16  its 16-bit twin, the third arm of Surface::blitT 0x91a0's dispatch
//   0x0001bae0  Sprite::remap8         the colour substitution over a Sprite's 8-bit bitmap
//   0x0000afe0  Surface::fill16        the 16-bit twin of Surface::fill8 0xf880 (batch c3av)
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below is a VA,
// as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// The A/B drives the function over synthetic Surface and Palette objects built on jgld's own vtables
// (re/frida/js/fixtures.d/c3bh.js), as batches c3av, c3ay and c3bc did: never the live Display at 0x10128420,
// never a screen surface, never DirectDraw, never the module's own Palette. log/c3/alt/c3bh_probe.py, one 12 s
// boot at the menu with counters attached straight to the module addresses: 0 entries into 0x12db0 and 0 into
// its only caller Surface::blitT 0x91a0, and 0 changes in each of the four module globals the body parks loop
// state in (0x10128498, 0x1012848c, 0x10128494, 0x1012849c).
//
// Surface vtable slots used here (0x1011d0b0; the rest are documented in c3av.cpp, c3ay.cpp and c3bc.cpp):
//   +0x0c  pixelAddr(x, y)  0x10008830: NULL when x >= width() or y >= height() or the slot-+0x10 base is NULL,
//          else base + x * bytesPerPixel + y * [this+0x40] * bytesPerPixel, with the depth at [this+0x24]
//          selecting 1/2/3/4 bytes and any other depth giving NULL (0x100088c2 ja).
//   +0x14  lockAt8(x, y)    forwarder 0x1000a810 -> slot +0x0c (0x1000a83f).
//   +0x1c  lockAt16(x, y)   forwarder 0x1000a870 -> slot +0x0c (0x1000a89f).
//   +0x24  unlock(flag)     0x100087b0: subtracts flag from [this+0x4c8], clearing [this+0x4cc] at <= 0.
//   +0xcc  clipRect()       0x1000a990: returns this+0x44.
//   +0xd8  width()          0x1000aa70: bounds.right - bounds.left (0x1000aa93).
//   +0xdc  height()         0x1000aab0: bounds.bottom - bounds.top (0x1000aad3).
// Palette vtable slots (0x1011db10): +0x18 -> this+0x40c (RGB565 table), +0x1c -> this+0x60c (RGB555 table).
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
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
typedef int(__fastcall* IntSlot_t)(void*, void*);
typedef unsigned short*(__fastcall* TableSlot_t)(void*, void*);

inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}
// +0xcc, the clip rectangle (this+0x44)
inline RECT* vClip(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xcc))(self, 0); }
// +0x14 / +0x1c, the two forwarders onto slot +0x0c
inline void* vLockAt8(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x14))(self, 0, x, y); }
inline void* vLockAt16(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x1c))(self, 0, x, y); }
// +0x24
inline void vUnlock(void* self, int flag) { reinterpret_cast<UnlockSlot_t>(slot(self, 0x24))(self, 0, flag); }
// +0xd8 / +0xdc
inline int vWidth(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0xd8))(self, 0); }
inline int vHeight(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0xdc))(self, 0); }
// Palette +0x18 / +0x1c
inline unsigned short* vTable565(void* pal) { return reinterpret_cast<TableSlot_t>(slot(pal, 0x18))(pal, 0); }
inline unsigned short* vTable555(void* pal) { return reinterpret_cast<TableSlot_t>(slot(pal, 0x1c))(pal, 0); }

// ------------------------------------------------------------------ the module's own helpers, called where the
// originals call them (through their module addresses, so the comparison is of this code and not of a copy).

typedef int(__cdecl* Intersect_t)(RECT*, const RECT*, const RECT*);   // 0x10008590, through thunk 0x10001087
typedef void(__cdecl* SetRect_t)(RECT*, int, int, int, int);          // 0x10008360, through thunk 0x10001019
typedef int(__cdecl* RectDim_t)(const RECT*);          // 0x10009120 / 0x10009160, thunks 0x10001794/0x1000173a

inline int jIntersect(RECT* d, const RECT* a, const RECT* b) {
    return reinterpret_cast<Intersect_t>(JgldBase() + 0x8590)(d, a, b);
}
inline void jSetRect(RECT* r, int x, int y, int w, int h) {
    reinterpret_cast<SetRect_t>(JgldBase() + 0x8360)(r, x, y, w, h);
}
inline int jRectWidth(const RECT* r) { return reinterpret_cast<RectDim_t>(JgldBase() + 0x9120)(r); }
inline int jRectHeight(const RECT* r) { return reinterpret_cast<RectDim_t>(JgldBase() + 0x9160)(r); }

// Surface+0x7c read directly (not through a slot) as the Palette of this surface (0x10012f4b).
inline void* surfacePalette(void* self) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x7c);
}
// Surface+0x28 read directly as the format code (0x10012f7a); blitKey8to16 0x12900 reads the same value through
// slot +0xe4 element [1] instead, this function does not.
inline int surfaceFormatField(void* self) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x28);
}
// Surface+0x40 read directly as the row stride in pixels (0x10012fff for dst, 0x10013008 for `this`).
inline int surfacePitchField(void* self) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x40);
}
// The module global at VA 0x1012873c: when a surface carries no Palette of its own, the Palette of the object
// this global points at (its field +4) is used instead (0x10012f5c).
inline void* paletteClient() { return *reinterpret_cast<void**>(JgldBase() + 0x12873c); }

// The loop-state globals this body parks values in. They belong to the module, not to a vector, so they are not
// state regions (the rule c3av recorded for 0x1012847c and c3bc for the blitKey8to16 six); what each one is set
// to is still verified through the pixels the loops write. 0x1012848c / 0x10128494 / 0x1012849c are the same
// three blitKey8to16 0x12900 uses.
inline int& gSrcSkip() { return *reinterpret_cast<int*>(JgldBase() + 0x128498); }   // src pitch, then pitch -+ cols
inline int& gCols() { return *reinterpret_cast<int*>(JgldBase() + 0x12848c); }      // pixels per row
inline int& gRows() { return *reinterpret_cast<int*>(JgldBase() + 0x128494); }      // row counter
inline unsigned short*& gTable() {                                                  // the colour table
    return *reinterpret_cast<unsigned short**>(JgldBase() + 0x12849c);
}

// =================================================================================== 0x00012db0 blitT_8to16

// 0x00012db0  Surface::blitT_8to16(dst, sx, sy, dx, dy, w, h) (VA 0x10012db0): copies a w x h block of 8-bit
// pixels from (sx, sy) of `this` into the surface dst at (dx, dy), expanding each source byte through a
// Palette colour table into a 16-bit word. `ret 0x1c`, returns int.
//
// Entry tests and the clipping preamble are blitT_8to8 0x13c50's, instruction for instruction: [this+0x4c0]
// (the pixel base field, read directly) zero returns 7 (0x10012ddd jne over 0x10012ddf), a null dst returns 16
// (0x10012ded); a negative sx adds itself to w, is subtracted from dx and becomes 0 (0x10012dfd jge over
// 0x10012dff..0x10012e11) and a negative sy does the same to h and dy (0x10012e1c); an sx past `this`'s width
// (slot +0xd8) or an sy past its height (slot +0xdc) returns 0 (0x10012e51 jle, 0x10012e74 jle, so sx == width
// is still accepted); w is cut to width - sx when sx + w overflows (0x10012e9c jle skips 0x10012e9e..0x10012eb8)
// and h to height - sy likewise (0x10012eda jle). The destination rectangle (dx, dy, w, h) is built by
// rectFromXYWH 0x10008360 (call 0x10012f0d through the thunk 0x10001019) and intersected with the CLIP
// RECTANGLE OF dst, slot +0xcc on it (0x10012f1f), by intersect 0x10008590 (call 0x10012f35 through the thunk
// 0x10001087); an empty intersection returns 0 (0x10012f3f jne over 0x10012f41).
//
// The colour table then comes from the PALETTE OF `this` ([this+0x7c] at 0x10012f4b, the client global
// 0x1012873c's field +4 when it is null, 0x10012f5c) and is SELECTED by the FORMAT CODE OF dst, read as the
// FIELD [dst+0x28] (0x10012f7a), not through slot +0xe4: code 0 takes Palette slot +0x18 (0x10012f98) and code 1
// slot +0x1c (0x10012fb3), each parked in the module global 0x1012849c (0x10012fa2 / 0x10012fbd); any other code
// returns 1 with nothing released (0x10012fc4) and a null Palette returns 16 (0x10012f6d).
//
// What the intersection trimmed off the left and the top is added to sx and sy (0x10012fce..0x10012fe7), w and h
// become the intersection's (0x10012fea..0x10012ff9), dst's row stride is read as the FIELD [dst+0x40]
// (0x10012fff) and `this`'s as [this+0x40] into the module global 0x10128498 (0x1001300b).
//
// `cmp dst, this` at 0x10013013 (je 0x100130fd) then picks one of five copy bodies, the same five-way split as
// blitT_8to8: dst != this, and for dst == this the four quadrants dx < sx / dx >= sx (0x10013103 jge) crossed
// with dy < sy / dy >= sy (0x1001310f jge and 0x100132f1 jge). All five lock the destination through dst's slot
// +0x1c and the source through `this`'s slot +0x14, park the per-row pixel count in 0x1012848c and the row count
// in 0x10128494, and walk each row as: read the source byte, look the 16-bit colour up in the table, advance the
// source one byte and the destination two (0x100130d2..0x100130e1 and the four matching loops).
//
//   dst != this                           (0x1001301c): lock at (rc.left, rc.top) and (sx, sy); the destination
//       row step is (dstPitch - cols) * 2 bytes (0x1001309b..0x100130a1) and the source row step is subtracted
//       INTO the global, 0x10128498 = srcPitch - cols (0x100130a3..0x100130ac); both are ADDED after each row.
//   dst == this, dx <  sx, dy <  sy       (0x10013115): identical to the above (0x1001319a / 0x100131a5).
//   dst == this, dx <  sx, dy >= sy       (0x100131f9): locks at (rc.left, rc.top + h - 1) and (sx, sy + h - 1),
//       the two steps become (dstPitch + cols) * 2 and srcPitch + cols (0x1001328c, 0x10013297) and are
//       SUBTRACTED after each row (0x100132d1, 0x100132d7), so the rows run bottom to top.
//   dst == this, dx >= sx, dy <  sy       (0x100132f7): the cursors start cols - 1 columns right of the
//       rectangle's left edge when cols < 4 (0x100132fb jge, 0x10013309 / 0x1001332f) and cols - 4 otherwise
//       (0x10013357 / 0x1001337d); the two steps are (dstPitch + cols) * 2 and srcPitch + cols (0x100133de,
//       0x100133e9) and are ADDED after each row (0x10013423, 0x10013429).
//   dst == this, dx >= sx, dy >= sy       (0x1001343e): the same cols - 1 / cols - 4 start (0x10013442 jge),
//       taken on the LAST row (rc.top + h - 1, sy + h - 1); both pitches are re-read from the two +0x40 fields
//       (0x10013533, 0x1001353f) and the steps become dstPitch - cols and srcPitch - cols (0x10013555,
//       0x1001355e), SUBTRACTED after each row (0x10013598, 0x1001359e).
//
// Three properties of the last two bodies are defects of the original, and the A/B drives all three:
//   (a) Both set the direction flag (`std` 0x100133d1 and 0x10013548) before their loop, as blitT_8to8's two
//       descending bodies do, but this function's loops contain no string instruction at all (`mov al, [esi]` /
//       `inc esi` / `mov [edi], cx` / `add edi, 2`), so the flag changes nothing and both loops still walk each
//       row LEFT TO RIGHT from a cursor placed cols - 4 (or cols - 1) columns to the right of the rectangle's
//       left edge. Every row therefore writes cols pixels ending cols - 4 columns past the rectangle's right
//       edge, and the row step, written for a right-to-left walk, moves the cursor forward by another
//       dstPitch + cols pixels in body 4.
//   (b) The destination row step of the LAST body is `mov ebx, [dst+0x40]; sub ebx, cols` with no `shl ebx, 1`
//       (0x10013552..0x10013555), unlike the other four (0x100130a1, 0x1001319d, 0x1001328f, 0x100133e1), so
//       that body steps the destination by a number of BYTES equal to a number of pixels.
//   (c) The LAST body counts its rows with `dec eax` / `jne` (0x100135a0) instead of the row global every other
//       body uses, and eax at that point holds the LAST SOURCE BYTE of the row just copied, zero-extended by
//       the `xor eax, eax` at 0x10013578 and the byte-wide `mov al, [esi]` at 0x10013587. The loop therefore
//       ends on the first row whose last source byte is 1, runs on for any other non-zero value, and a row
//       whose last source byte is 0 leaves eax = 0xffffffff, after which `mov cx, [ebp + eax*2]` (0x10013589)
//       reads about 8 GB past the colour table. The A/B drives it with source bytes chosen so the loop ends
//       after exactly one row and after exactly two rows.
//
// Null returns: in the first three bodies the destination address is tested first and returns 3 with nothing
// released (0x1001303f, 0x10013138, 0x10013223), and the source address returns 3 after releasing dst with slot
// +0x24 and 1 (0x1001306e -> 0x1001307c, 0x10013167, 0x10013259). In the last two bodies BOTH addresses are
// taken before either is tested (0x10013320 / 0x10013346 and 0x100134ca / 0x100134f7), with the same two
// outcomes (0x1001339b / 0x100133ab and 0x100134fe / 0x1001350e). On success `this` is released first
// (0x100135b5) and dst second (0x100135cb) and the function returns 0 (0x100135d5).
typedef int(__fastcall* BlitT8to16_t)(void*, void*, void*, int, int, int, int, int, int);
BlitT8to16_t BlitT8to16_orig;

// One row: source byte -> table -> 16-bit word, source +1 and destination +2 per pixel, the count re-read from
// the module global 0x1012848c at the top of every row (0x100130cc and the four matching loads).
inline void rowExpand(char*& d, const unsigned char*& s, int n, const unsigned short* tbl) {
    while (n != 0) {
        const unsigned short v = tbl[*s];                                  // 0x100130d2, 0x100130d4
        ++s;                                                               // 0x100130d9
        *reinterpret_cast<unsigned short*>(d) = v;                         // 0x100130da
        d += 2;                                                            // 0x100130dd
        --n;                                                               // 0x100130e0
    }
}

int __fastcall BlitT8to16_re(void* self, void*, void* dst, int sx, int sy, int dx, int dy, int w, int h) {
    if (*reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x4c0) == 0) return 7;   // 0x10012dd6
    if (dst == 0) return 16;                                                               // 0x10012de9
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }                                             // 0x10012df9
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }                                             // 0x10012e18
    if (sx > vWidth(self)) return 0;                                                       // 0x10012e4e
    if (sy > vHeight(self)) return 0;                                                      // 0x10012e71
    if (sx + w > vWidth(self)) w = vWidth(self) - sx;                                      // 0x10012e9a
    if (sy + h > vHeight(self)) h = vHeight(self) - sy;                                    // 0x10012ed8
    RECT rc;
    jSetRect(&rc, dx, dy, w, h);                                                           // 0x10012f0d
    if (!jIntersect(&rc, &rc, vClip(dst))) return 0;                                       // 0x10012f35

    void* pal;
    if (surfacePalette(self) != 0) {
        pal = surfacePalette(self);                                                        // 0x10012f54
    } else {
        pal = *reinterpret_cast<void**>(reinterpret_cast<char*>(paletteClient()) + 4);     // 0x10012f61
    }
    if (pal == 0) return 16;                                                               // 0x10012f67
    const int fmt = surfaceFormatField(dst);                                               // 0x10012f7a
    if (fmt == 0) {
        gTable() = vTable565(pal);                                                         // 0x10012f98
    } else if (fmt == 1) {
        gTable() = vTable555(pal);                                                         // 0x10012fb3
    } else {
        return 1;                                                                          // 0x10012fc4
    }

    sx += rc.left - dx;                                                                    // 0x10012fce
    sy += rc.top - dy;                                                                     // 0x10012fdc
    w = rc.right - rc.left;                                                                // 0x10012fea
    h = rc.bottom - rc.top;                                                                // 0x10012ff3
    int dstPitch = surfacePitchField(dst);                                                 // 0x10012fff
    gSrcSkip() = surfacePitchField(self);                                                  // 0x1001300b

    char* d;
    const unsigned char* s;
    int dstStep;                     // what the destination cursor moves by after each row, in bytes
    const unsigned short* tbl;

    if (dst != self) {                                                                     // 0x10013013
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left, rc.top));                      // 0x1001302e
        if (d == 0) return 3;                                                              // 0x1001303b
        s = reinterpret_cast<const unsigned char*>(vLockAt8(self, sx, sy));                // 0x1001305d
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1001306a
        dstStep = (dstPitch - w) * 2;                                                      // 0x1001309b
        gSrcSkip() = gSrcSkip() - w;                                                       // 0x100130a3
        gCols() = w;                                                                       // 0x100130b8
        gRows() = h;                                                                       // 0x100130be
        tbl = gTable();                                                                    // 0x100130c6
        do {
            rowExpand(d, s, gCols(), tbl);                                                 // 0x100130cc
            s += gSrcSkip();                                                               // 0x100130e3
            d += dstStep;                                                                  // 0x100130e9
        } while (--gRows() != 0);                                                          // 0x100130eb
    } else if (dx < sx && dy < sy) {                                     // 0x10013103 jge, 0x1001310f jge
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left, rc.top));                      // 0x10013127
        if (d == 0) return 3;                                                              // 0x10013134
        s = reinterpret_cast<const unsigned char*>(vLockAt8(self, sx, sy));                // 0x10013156
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x10013163
        dstStep = (dstPitch - w) * 2;                                                      // 0x1001319a
        gSrcSkip() = gSrcSkip() - w;                                                       // 0x1001319f
        gCols() = w;                                                                       // 0x100131b4
        gRows() = h;                                                                       // 0x100131ba
        tbl = gTable();                                                                    // 0x100131c2
        do {
            rowExpand(d, s, gCols(), tbl);                                                 // 0x100131c8
            s += gSrcSkip();                                                               // 0x100131df
            d += dstStep;                                                                  // 0x100131e5
        } while (--gRows() != 0);                                                          // 0x100131e7
    } else if (dx < sx) {                                                // dy >= sy: 0x100131f9
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left, rc.top + h - 1));              // 0x10013212
        if (d == 0) return 3;                                                              // 0x1001321f
        s = reinterpret_cast<const unsigned char*>(vLockAt8(self, sx, sy + h - 1));        // 0x10013248
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x10013255
        dstStep = (dstPitch + w) * 2;                                                      // 0x1001328c
        gSrcSkip() = gSrcSkip() + w;                                                       // 0x10013291
        gCols() = w;                                                                       // 0x100132a6
        gRows() = h;                                                                       // 0x100132ac
        tbl = gTable();                                                                    // 0x100132b4
        do {
            rowExpand(d, s, gCols(), tbl);                                                 // 0x100132ba
            s -= gSrcSkip();                                                               // 0x100132d1
            d -= dstStep;                                                                  // 0x100132d7
        } while (--gRows() != 0);                                                          // 0x100132d9
    } else if (dy < sy) {                                                // dx >= sx: 0x100132f1 jge
        const int k = (w < 4) ? (w - 1) : (w - 4);                                         // 0x100132fb
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left + k, rc.top));                  // 0x10013316 / 0x10013364
        s = reinterpret_cast<const unsigned char*>(vLockAt8(self, sx + k, sy));            // 0x1001333c / 0x1001338a
        if (d == 0) return 3;                                                              // 0x10013397
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x100133a7
        dstStep = (dstPitch + w) * 2;                                                      // 0x100133de
        gSrcSkip() = gSrcSkip() + w;                                                       // 0x100133e3
        gCols() = w;                                                                       // 0x100133f8
        gRows() = h;                                                                       // 0x100133fe
        tbl = gTable();                                                                    // 0x10013406
        do {
            rowExpand(d, s, gCols(), tbl);                                                 // 0x1001340c
            s += gSrcSkip();                                                               // 0x10013423
            d += dstStep;                                                                  // 0x10013429
        } while (--gRows() != 0);                                                          // 0x1001342b
    } else {                                                             // dx >= sx, dy >= sy: 0x1001343e
        const int k = (w < 4) ? (w - 1) : (w - 4);                                         // 0x10013442
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left + k, rc.top + h - 1));          // 0x10013464 / 0x100134c0
        s = reinterpret_cast<const unsigned char*>(vLockAt8(self, sx + k, sy + h - 1));    // 0x10013491 / 0x100134ed
        if (d == 0) return 3;                                                              // 0x100134fa
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1001350a
        dstPitch = surfacePitchField(dst);                                                 // 0x10013533
        gSrcSkip() = surfacePitchField(self);                                              // 0x1001353f
        dstStep = dstPitch - w;                                     // no `shl ebx, 1` here: 0x10013552/0x10013555
        gSrcSkip() = gSrcSkip() - w;                                                       // 0x10013558
        gCols() = w;                                                                       // 0x1001356d
        gRows() = h;                                       // written (0x10013573) but never read by this body
        tbl = gTable();                                                                    // 0x1001357b
        unsigned rowCtr = 0;                                                               // 0x10013578
        do {
            int n = gCols();                                                               // 0x10013581
            while (n != 0) {
                // The row counter and the source byte share eax: `mov al` leaves the upper 24 bits alone.
                rowCtr = (rowCtr & 0xffffff00u) | *s;                                      // 0x10013587
                const unsigned short v = tbl[rowCtr];                                      // 0x10013589
                ++s;                                                                       // 0x1001358e
                *reinterpret_cast<unsigned short*>(d) = v;                                 // 0x1001358f
                d += 2;                                                                    // 0x10013592
                --n;                                                                       // 0x10013595
            }
            s -= gSrcSkip();                                                               // 0x10013598
            d -= dstStep;                                                                  // 0x1001359e
            --rowCtr;                                                                      // 0x100135a0
        } while (rowCtr != 0);                                                             // 0x100135a1
    }
    vUnlock(self, 1);                                                                      // 0x100135b5
    vUnlock(dst, 1);                                                                       // 0x100135cb
    return 0;                                                                              // 0x100135d5
}

// ================================================================================== 0x0000ed70 blitT_16to16

// The only module global blitT_16to16 parks loop state in: the leftover pixel of an odd-width row. No other
// function of jgld.dll contains its address (a byte scan of the mapped image found the ten references at
// 0x1000efdd..0x1000f457, all inside this body), so nothing else in the module can move it.
inline int& gOddPixel() { return *reinterpret_cast<int*>(JgldBase() + 0x128464); }

// 0x0000ed70  Surface::blitT_16to16(dst, sx, sy, dx, dy, w, h) (VA 0x1000ed70): copies a w x h block of 16-bit
// pixels from (sx, sy) of `this` to (dx, dy) of dst, with four separate loops for the case dst == this so that
// an overlapping copy still reads each pixel before it is overwritten. `ret 0x1c`, returns int.
//
// The preamble is blitT_8to16 0x12db0's with two differences: there is no test of [this+0x4c0] (a null dst is
// the first test and returns 3, 0x1000ed97, not 16) and there is no Palette block at all, because source and
// destination hold the same 16-bit pixels. A negative sx folds into w and dx (0x1000eda7 jge over
// 0x1000eda9..0x1000edbb) and a negative sy into h and dy (0x1000edc6); an sx past `this`'s width (slot +0xd8)
// or an sy past its height (slot +0xdc) returns 0 (0x1000edfb jle, 0x1000ee1e jle); w and h are cut to what is
// left (0x1000ee46 jle, 0x1000ee84 jle); rectFromXYWH 0x10008360 builds the destination rectangle (call
// 0x1000eeb7) and intersect 0x10008590 clips it to dst's clip rectangle, slot +0xcc (calls 0x1000eec9 and
// 0x1000eedf), an empty intersection returning 0 (0x1000eee9); what was trimmed is added back to sx and sy and
// w and h become the intersection's (0x1000eef2..0x1000ef1d).
//
// `cmp dst, this` at 0x1000ef23 (je 0x1000efff) then picks one of five copy bodies, the same five-way split as
// blitT_8to16, on dx vs sx (0x1000f005 jge) and dy vs sy (0x1000f011 jge, 0x1000f1d1 jge). All five lock BOTH
// surfaces through slot +0x1c (dst at 0x1000ef3e and the four matching sites, `this` at 0x1000ef6d), read both
// row strides from the FIELD [surface+0x40] after the locks (0x1000efa3 / 0x1000efac and the matching pairs),
// park cols & 1 in the module global 0x10128464 (0x1000efdb and four more sites) and copy each row as
// `rep movsd` of cols >> 1 dwords, two pixels at a time:
//
//   dst != this                           (0x1000ef2c): lock at (rc.left, rc.top) and (sx, sy), cld, row steps
//       +(dstPitch - cols) * 2 and +(srcPitch - cols) * 2 bytes (0x1000efc6, 0x1000efce); the odd pixel is one
//       `rep movsw` right after the dwords, its count re-read from 0x10128464 each row (0x1000efe5).
//   dst == this, dx <  sx, dy <  sy       (0x1000f017): identical (0x1000f0b1, 0x1000f0b9, 0x1000f0d0).
//   dst == this, dx <  sx, dy >= sy       (0x1000f0ea): locks on the last row (rc.top + h - 1, sy + h - 1), the
//       two steps become +(dstPitch + cols) * 2 and +(srcPitch + cols) * 2 and are SUBTRACTED after each row
//       (0x1000f1ba, 0x1000f1bc), so the rows run bottom to top; the row body is unchanged.
//   dst == this, dx >= sx, dy <  sy       (0x1000f1d7): the cursors start cols - 1 pixels right of the
//       rectangle's left edge when cols < 4 (0x1000f1db jge, 0x1000f1e9 / 0x1000f20f) and cols - 2 otherwise
//       (0x1000f237 / 0x1000f25d), `std` is set (0x1000f2c3) so the `rep movsd` runs right to left, and the
//       row steps +(dstPitch + cols) * 2 / +(srcPitch + cols) * 2 are ADDED (0x1000f2fb, 0x1000f2fd).
//   dst == this, dx >= sx, dy >= sy       (0x1000f322): the same cols - 1 / cols - 2 start (0x1000f326 jge) on
//       the last row, `std` (0x1000f42a), row steps -(dstPitch - cols) * 2 and -(srcPitch - cols) * 2
//       (0x1000f462, 0x1000f464).
//
// The two descending bodies do NOT use `rep movsw` for the odd pixel: each tests the global once before its
// loop (0x1000f2ee and 0x1000f455) and runs one of two whole loops, the second of which copies the leftover
// pixel with `mov cx, [esi+2]` / `mov [edi+2], cx` (0x1000f308 / 0x1000f30c and 0x1000f46f / 0x1000f473) AFTER
// the descending `rep movsd` has already stepped both cursors four bytes below their last dword, so that pixel
// is the one two bytes above the cursor's final position, not the rectangle's leftmost pixel. With cols 1 the
// dword count is 0 and that copy is the only one, and it writes the pixel to the RIGHT of the one-pixel
// rectangle.
//
// Null returns: in the first three bodies the destination address is tested first and returns 3 with nothing
// released (0x1000ef4f, 0x1000f03a, 0x1000f114) and the source address returns 3 after releasing dst with slot
// +0x24 and 1 (0x1000ef7e, 0x1000f069, 0x1000f14a); in the two descending bodies both addresses are taken
// before either is tested (0x1000f27b / 0x1000f28b and 0x1000f3e2 / 0x1000f3f2). On success `this` is released
// first (0x1000f490) and dst second (0x1000f4a6) and the function returns 0 (0x1000f4b0).
typedef int(__fastcall* BlitT16to16_t)(void*, void*, void*, int, int, int, int, int, int);
BlitT16to16_t BlitT16to16_orig;

// `rep movsd` of n dwords with DF clear, then `rep movsw` of the module global's count, also forward.
inline void rowCopy16Up(char*& d, const char*& s, int dwords, int words) {
    for (int n = dwords; n > 0; n--) {
        *reinterpret_cast<unsigned*>(d) = *reinterpret_cast<const unsigned*>(s);
        d += 4;
        s += 4;
    }
    for (int m = words; m > 0; m--) {
        *reinterpret_cast<unsigned short*>(d) = *reinterpret_cast<const unsigned short*>(s);
        d += 2;
        s += 2;
    }
}
// `rep movsd` of n dwords with DF set: each dword is written at the current address and both cursors then step
// back four bytes.
inline void rowCopy16Down(char*& d, const char*& s, int dwords) {
    for (int n = dwords; n > 0; n--) {
        *reinterpret_cast<unsigned*>(d) = *reinterpret_cast<const unsigned*>(s);
        d -= 4;
        s -= 4;
    }
}

int __fastcall BlitT16to16_re(void* self, void*, void* dst, int sx, int sy, int dx, int dy, int w, int h) {
    if (dst == 0) return 3;                                                                // 0x1000ed93
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }                                             // 0x1000eda3
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }                                             // 0x1000edc2
    if (sx > vWidth(self)) return 0;                                                       // 0x1000edf8
    if (sy > vHeight(self)) return 0;                                                      // 0x1000ee1b
    if (sx + w > vWidth(self)) w = vWidth(self) - sx;                                      // 0x1000ee44
    if (sy + h > vHeight(self)) h = vHeight(self) - sy;                                    // 0x1000ee82
    RECT rc;
    jSetRect(&rc, dx, dy, w, h);                                                           // 0x1000eeb7
    if (!jIntersect(&rc, &rc, vClip(dst))) return 0;                                       // 0x1000eedf
    sx += rc.left - dx;                                                                    // 0x1000eef2
    sy += rc.top - dy;                                                                     // 0x1000ef00
    w = rc.right - rc.left;                                                                // 0x1000ef0e
    h = rc.bottom - rc.top;                                                                // 0x1000ef17

    char* d;
    const char* s;
    int dstStep;                     // bytes added to (or subtracted from) the destination cursor after a row
    int srcStep;

    if (dst != self) {                                                                     // 0x1000ef23
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left, rc.top));                      // 0x1000ef3e
        if (d == 0) return 3;                                                              // 0x1000ef4b
        s = reinterpret_cast<const char*>(vLockAt16(self, sx, sy));                        // 0x1000ef6d
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1000ef7a
        dstStep = (surfacePitchField(dst) - w) * 2;                                        // 0x1000efc3
        srcStep = (surfacePitchField(self) - w) * 2;                                       // 0x1000efcb
        gOddPixel() = w & 1;                                                               // 0x1000efdb
        for (int r = h; r > 0; r--) {                                                      // 0x1000eff2
            rowCopy16Up(d, s, w >> 1, gOddPixel());                                        // 0x1000efe1
            s += srcStep;                                                                  // 0x1000efee
            d += dstStep;                                                                  // 0x1000eff0
        }
    } else if (dx < sx && dy < sy) {                                     // 0x1000f005 jge, 0x1000f011 jge
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left, rc.top));                      // 0x1000f029
        if (d == 0) return 3;                                                              // 0x1000f036
        s = reinterpret_cast<const char*>(vLockAt16(self, sx, sy));                        // 0x1000f058
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1000f065
        dstStep = (surfacePitchField(dst) - w) * 2;                                        // 0x1000f0ae
        srcStep = (surfacePitchField(self) - w) * 2;                                       // 0x1000f0b6
        gOddPixel() = w & 1;                                                               // 0x1000f0c6
        for (int r = h; r > 0; r--) {                                                      // 0x1000f0dd
            rowCopy16Up(d, s, w >> 1, gOddPixel());                                        // 0x1000f0cc
            s += srcStep;                                                                  // 0x1000f0d9
            d += dstStep;                                                                  // 0x1000f0db
        }
    } else if (dx < sx) {                                                // dy >= sy: 0x1000f0ea
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left, rc.top + h - 1));              // 0x1000f103
        if (d == 0) return 3;                                                              // 0x1000f110
        s = reinterpret_cast<const char*>(vLockAt16(self, sx, sy + h - 1));                // 0x1000f139
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1000f146
        dstStep = (surfacePitchField(dst) + w) * 2;                                        // 0x1000f18f
        srcStep = (surfacePitchField(self) + w) * 2;                                       // 0x1000f197
        gOddPixel() = w & 1;                                                               // 0x1000f1a7
        for (int r = h; r > 0; r--) {                                                      // 0x1000f1be
            rowCopy16Up(d, s, w >> 1, gOddPixel());                                        // 0x1000f1ad
            s -= srcStep;                                                                  // 0x1000f1ba
            d -= dstStep;                                                                  // 0x1000f1bc
        }
    } else if (dy < sy) {                                                // dx >= sx: 0x1000f1d1 jge
        const int k = (w < 4) ? (w - 1) : (w - 2);                                         // 0x1000f1db
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left + k, rc.top));       // 0x1000f1f6 / 0x1000f244
        s = reinterpret_cast<const char*>(vLockAt16(self, sx + k, sy));         // 0x1000f21c / 0x1000f26a
        if (d == 0) return 3;                                                              // 0x1000f277
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1000f287
        dstStep = (surfacePitchField(dst) + w) * 2;                                        // 0x1000f2d0
        srcStep = (surfacePitchField(self) + w) * 2;                                       // 0x1000f2d8
        gOddPixel() = w & 1;                                                               // 0x1000f2e8
        if (gOddPixel() == 0) {                                                            // 0x1000f2ee
            for (int r = h; r > 0; r--) {                                                  // 0x1000f2ff
                rowCopy16Down(d, s, w >> 1);                                               // 0x1000f2f7
                s += srcStep;                                                              // 0x1000f2fb
                d += dstStep;                                                              // 0x1000f2fd
            }
        } else {
            for (int r = h; r > 0; r--) {                                                  // 0x1000f314
                rowCopy16Down(d, s, w >> 1);                                               // 0x1000f304
                // the leftover pixel, taken two bytes above where the descending dwords stopped
                *reinterpret_cast<unsigned short*>(d + 2) =
                    *reinterpret_cast<const unsigned short*>(s + 2);                       // 0x1000f308
                s += srcStep;                                                              // 0x1000f310
                d += dstStep;                                                              // 0x1000f312
            }
        }
    } else {                                                             // dx >= sx, dy >= sy: 0x1000f322
        const int k = (w < 4) ? (w - 1) : (w - 2);                                         // 0x1000f326
        d = reinterpret_cast<char*>(vLockAt16(dst, rc.left + k, rc.top + h - 1));          // 0x1000f348 / 0x1000f3a4
        s = reinterpret_cast<const char*>(vLockAt16(self, sx + k, sy + h - 1));            // 0x1000f375 / 0x1000f3d1
        if (d == 0) return 3;                                                              // 0x1000f3de
        if (s == 0) { vUnlock(dst, 1); return 3; }                                         // 0x1000f3ee
        dstStep = (surfacePitchField(dst) - w) * 2;                                        // 0x1000f437
        srcStep = (surfacePitchField(self) - w) * 2;                                       // 0x1000f43f
        gOddPixel() = w & 1;                                                               // 0x1000f44f
        if (gOddPixel() == 0) {                                                            // 0x1000f455
            for (int r = h; r > 0; r--) {                                                  // 0x1000f466
                rowCopy16Down(d, s, w >> 1);                                               // 0x1000f45e
                s -= srcStep;                                                              // 0x1000f462
                d -= dstStep;                                                              // 0x1000f464
            }
        } else {
            for (int r = h; r > 0; r--) {                                                  // 0x1000f47b
                rowCopy16Down(d, s, w >> 1);                                               // 0x1000f46b
                *reinterpret_cast<unsigned short*>(d + 2) =
                    *reinterpret_cast<const unsigned short*>(s + 2);                       // 0x1000f46f
                s -= srcStep;                                                              // 0x1000f477
                d -= dstStep;                                                              // 0x1000f479
            }
        }
    }
    vUnlock(self, 1);                                                                      // 0x1000f490
    vUnlock(dst, 1);                                                                       // 0x1000f4a6
    return 0;                                                                              // 0x1000f4b0
}

// ===================================================================================== 0x0001bae0 remap8

// 0x0001bae0  Sprite::remap8(from, to) (VA 0x1001bae0): replaces every byte equal to `from` with `to` over the
// Sprite's 8-bit bitmap at [this+0x14]. `ret 8`, returns int: 7 when the bitmap pointer is null (0x1001bb04
// jne over 0x1001bb06), 0 on every other path (0x1001bbe2). It calls nothing but the frame check 0x1007e780
// and touches no global.
//
// Both arguments are masked to a byte first (0x1001bb13, 0x1001bb1f) and then used as the two halves of one
// register, `from` in al (0x1001bb8f) and `to` in ah (0x1001bb92), so only the low byte of each matters.
//
// The byte count comes from one of two places, chosen by bit 0 of [this+0x18] (0x1001bb37 and the `test`
// 0x1001bb3a): with the bit set it is [this+0x24] shifted right UNSIGNED by 2 (`shr edx, 2` at 0x1001bb44)
// with the remainder [this+0x24] - (quotient << 2) (0x1001bb56), and with the bit clear it is
// [this+0x2c] * [this+0x34] shifted right SIGNED (`sar edx, 2` at 0x1001bb6a, 0x1001bb83). The two shifts
// differ for a negative product: `sar` keeps the sign and `shr` does not.
//
// The quotient is the `loop` count of a block unrolled four times (0x1001bb99..0x1001bbad) and the remainder
// the `loop` count of a one-byte tail (0x1001bbd8..0x1001bbdf). Two things follow from the shape:
//   * a zero quotient jumps straight to the exit (0x1001bb97 je 0x1001bbe1), SKIPPING the tail, so a bitmap of
//     one, two or three bytes is left untouched;
//   * the unrolled block exists twice, once for a run with no match so far (0x1001bb99) and once entered after
//     a match (0x1001bbb2), and the two differ only in that the second writes `to` before moving on; the
//     cursor is left one past the block's last byte and `dec esi` (0x1001bbaf / 0x1001bbd0) pulls it back so
//     the tail's leading `inc esi` (0x1001bbd8) lands on the first byte after the block.
typedef int(__fastcall* Remap8_t)(void*, void*, int, int);
Remap8_t Remap8_orig;
int __fastcall Remap8_re(void* self, void*, int from, int to) {
    unsigned char* const bits = *reinterpret_cast<unsigned char**>(reinterpret_cast<char*>(self) + 0x14);
    if (bits == 0) return 7;                                                               // 0x1001bb00
    const unsigned char a = static_cast<unsigned char>(from & 0xff);                       // 0x1001bb13
    const unsigned char b = static_cast<unsigned char>(to & 0xff);                         // 0x1001bb1f
    const int flags = *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x18);       // 0x1001bb34
    int groups;
    int rest;
    if (flags & 1) {                                                                       // 0x1001bb3c
        const unsigned total = *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 0x24);
        groups = static_cast<int>(total >> 2);                                             // 0x1001bb44 shr
        rest = static_cast<int>(total) - (groups << 2);                                    // 0x1001bb56
    } else {
        const int total = *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x2c) *
                          *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x34);
        groups = total >> 2;                                                               // 0x1001bb6a sar
        rest = total - (groups << 2);                                                      // 0x1001bb83
    }
    if (groups == 0) return 0;                                      // 0x1001bb97: the tail is skipped too
    unsigned char* p = bits;
    for (int g = groups; g > 0; g--) {                              // 0x1001bbad / 0x1001bbce `loop`
        for (int k = 0; k < 4; k++) {                               // 0x1001bb99..0x1001bbac, unrolled
            if (*p == a) *p = b;                                    // 0x1001bb9b / 0x1001bbb6 and the twins
            ++p;
        }
    }
    for (int t = rest; t > 0; t--) {                                                       // 0x1001bbdf
        if (*p == a) *p = b;                                                               // 0x1001bbd9
        ++p;
    }
    return 0;                                                                              // 0x1001bbe2
}

// ====================================================================================== 0x0000afe0 fill16

// 0x0000afe0  Surface::fill16(x, y, w, h, c1, c2) (VA 0x1000afe0): over the clipped rectangle (x, y, w, h) of
// this 16-bit surface, replaces every pixel that holds c1 with c2 and leaves the rest alone. `ret 0x18`,
// returns int. It is the 16-bit twin of Surface::fill8 0xf880 (batch c3av): the same four calls in the same
// order and the same two-colour loop, two bytes per pixel instead of one.
//
// rectFromXYWH 0x10008360 builds the rectangle (call 0x1000b011 through the thunk 0x10001019) and intersect
// 0x10008590 clips it (call 0x1000b039 through 0x10001087) with the arguments (&r, clipRect(), &r) — the clip
// rectangle of slot +0xcc is the SECOND argument here and the rectangle is both the first and the third, where
// blitT_8to16 0x12db0 passes (&rc, &rc, clip). An empty intersection returns 0 (0x1000b043 jne over
// 0x1000b045). The pixel address comes from slot +0x1c at the intersection's top-left corner (0x1000b05e) and
// a NULL one returns 7 with nothing released (0x1000b06f). The width and height of the loop are then taken
// from the intersection by rectWidth 0x10009120 and rectHeight 0x10009160 (calls 0x1000b07c and 0x1000b08b
// through the thunks 0x10001794 and 0x1000173a) and written back over the w and h ARGUMENT slots (0x1000b084,
// 0x1000b093), and the row skip is (pitch - w) * 2 bytes from the FIELD [this+0x40] (0x1000b099..0x1000b09f),
// not through slot +0xe0.
//
// Both colours are read as 16-bit words from their argument slots (`mov ax, word ptr [ebp+0x18]` 0x1000b0a6
// and `mov bx, word ptr [ebp+0x1c]` 0x1000b0aa), so the upper half of either argument is ignored. The loop
// compares each destination word with c1 (0x1000b0ba) and stores c2 only where they are equal (0x1000b0bd jne
// over 0x1000b0bf), steps two bytes per pixel (0x1000b0c2), re-reads the pixel count from the w slot at the
// top of every row (0x1000b0b7) and adds the skip after each (0x1000b0c8). The surface is released once
// through slot +0x24 with 1 (0x1000b0db) and the function returns 0 (0x1000b0e5).
typedef int(__fastcall* Fill16_t)(void*, void*, int, int, int, int, int, int);
Fill16_t Fill16_orig;
int __fastcall Fill16_re(void* self, void*, int x, int y, int w, int h, int c1, int c2) {
    RECT r;
    jSetRect(&r, x, y, w, h);                                                              // 0x1000b011
    if (!jIntersect(&r, vClip(self), &r)) return 0;                                        // 0x1000b039
    char* d = reinterpret_cast<char*>(vLockAt16(self, r.left, r.top));                     // 0x1000b05e
    if (d == 0) return 7;                                                                  // 0x1000b06b
    w = jRectWidth(&r);                                                                    // 0x1000b07c
    h = jRectHeight(&r);                                                                   // 0x1000b08b
    const int skip = (surfacePitchField(self) - w) * 2;                                    // 0x1000b099
    const unsigned short a = static_cast<unsigned short>(c1);                              // 0x1000b0a6
    const unsigned short b = static_cast<unsigned short>(c2);                              // 0x1000b0aa
    for (int row = h; row > 0; row--) {                                                    // 0x1000b0ca
        for (int n = w; n > 0; n--) {                                                      // 0x1000b0c5
            if (*reinterpret_cast<unsigned short*>(d) == a) {                              // 0x1000b0ba
                *reinterpret_cast<unsigned short*>(d) = b;                                 // 0x1000b0bf
            }
            d += 2;                                                                        // 0x1000b0c2
        }
        d += skip;                                                                         // 0x1000b0c8
    }
    vUnlock(self, 1);                                                                      // 0x1000b0db
    return 0;                                                                              // 0x1000b0e5
}

}  // namespace

SG_HOOK("jgld.dll", 0x00012db0, Surface_blitT_8to16, BlitT8to16_re, BlitT8to16_orig);
SG_HOOK("jgld.dll", 0x0000afe0, Surface_fill16, Fill16_re, Fill16_orig);
SG_HOOK("jgld.dll", 0x0000ed70, Surface_blitT_16to16, BlitT16to16_re, BlitT16to16_orig);
SG_HOOK("jgld.dll", 0x0001bae0, Sprite_remap8, Remap8_re, Remap8_orig);
