// Batch c3bc: six jgld.dll render functions — the two surface-to-surface copies the earlier render batches
// left (blitT_8to8 0x13c50, blitKey8to16 0x12900), the alpha/shade blend blit16from16 0xf6a0 (x87), and the
// three Bresenham line drawers line8 0xfb70, line16 0xb140 and dashedLine8 0x10870.
//
// SG_HOOK addresses are RVAs; the module's VAs are 0x10000000 + RVA, and every instruction cited below is a VA,
// as the debug build's disassembly prints it. jgld.dll is LoadLibrary'd after the shim starts, so these hooks
// install from the LoadLibraryA hook (SgHooksInstallPending, re/hooks.cpp) and diff_hook finds them by
// (module, rva) through SimGolfShim_FindHookM. __thiscall is emulated with __fastcall: ecx = this, edx unused.
//
// The A/B drives all six over synthetic Surface and Palette objects built on jgld's own vtables
// (re/frida/js/fixtures.d/c3bc.js), exactly as batches c3av and c3ay did: never the live Display at 0x10128420,
// never a screen surface, never DirectDraw, never the module's own Palette. log/c3/alt/c3bc_probe.py shows the
// running game enters none of these six (nor their callers Surface::blit 0x8e50, Surface::blitT 0x91a0,
// Surface::blit16 0x98f0, Surface::drawLine 0x94e0, Surface::drawDashedLine 0x95a0) at the menu, and that the
// seven module globals they park loop state in never move on their own.
//
// Surface vtable slots used here (0x1011d0b0; the rest are documented in c3av.cpp and c3ay.cpp):
//   +0x0c  pixelAddr(x, y)  0x10008830: NULL when x >= width() or y >= height() or the slot-+0x10 base is NULL,
//          else base + x * bytesPerPixel + y * [this+0x40] * bytesPerPixel, with the depth at [this+0x24]
//          selecting 1/2/3/4 bytes and any other depth giving NULL (0x100088c2 ja).
//   +0x10  lockBits()       0x10008730: copies [this+0x4c0] to [this+0x4cc], bumps [this+0x4c8], returns it.
//   +0x14  lockAt8(x, y)    forwarder 0x1000a810 -> slot +0x0c (0x1000a83f).
//   +0x18  bits()           forwarder 0x1000a750 -> slot +0x10 (0x1000a777).
//   +0x1c  lockAt16(x, y)   forwarder 0x1000a870 -> slot +0x0c (0x1000a89f).
//   +0x20  bits()           forwarder 0x1000a7b0 -> slot +0x10 (0x1000a7d7).
//   +0x24  unlock(flag)     0x100087b0: subtracts flag from [this+0x4c8], clearing [this+0x4cc] at <= 0.
//   +0xc8  getClipRect(r)   copies the clip rectangle into the caller's RECT.
//   +0xcc  clipRect()       0x1000a990: returns this+0x44.      +0xd4 boundsRect() 0x1000aa30: returns this+0x54.
//   +0xd8  width()          0x1000aa70: bounds.right - bounds.left (0x1000aa93).
//   +0xdc  height()         0x1000aab0: bounds.bottom - bounds.top (0x1000aad3).
//   +0xe0  pitch()          0x1000aaf0: returns [this+0x40] (0x1000ab10), the row stride in PIXELS.
//   +0xe4  format()         0x1000ab30: returns this+0x24, so element [1] is the format code at this+0x28.
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
typedef void(__fastcall* GetRectSlot_t)(void*, void*, RECT*);
typedef void*(__fastcall* AddrSlot_t)(void*, void*, int, int);
typedef void*(__fastcall* BitsSlot_t)(void*, void*);
typedef void(__fastcall* UnlockSlot_t)(void*, void*, int);
typedef int(__fastcall* IntSlot_t)(void*, void*);
typedef int*(__fastcall* FormatSlot_t)(void*, void*);
typedef unsigned short*(__fastcall* TableSlot_t)(void*, void*);

inline void* slot(void* self, unsigned off) {
    return *reinterpret_cast<void**>(*reinterpret_cast<char**>(self) + off);
}
// +0xcc, the clip rectangle (this+0x44)
inline RECT* vClip(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xcc))(self, 0); }
// +0xd4, the bounds rectangle (this+0x54)
inline RECT* vBounds(void* self) { return reinterpret_cast<RectSlot_t>(slot(self, 0xd4))(self, 0); }
// +0xc8, a copy of the clip rectangle into the caller's buffer
inline void vGetClipRect(void* self, RECT* r) { reinterpret_cast<GetRectSlot_t>(slot(self, 0xc8))(self, 0, r); }
// +0x14 / +0x1c, the two forwarders onto slot +0x0c
inline void* vLockAt8(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x14))(self, 0, x, y); }
inline void* vLockAt16(void* self, int x, int y) { return reinterpret_cast<AddrSlot_t>(slot(self, 0x1c))(self, 0, x, y); }
// +0x10 / +0x18 / +0x20, the base pointer (the two forwarders call slot +0x10)
inline void* vLockBits(void* self) { return reinterpret_cast<BitsSlot_t>(slot(self, 0x10))(self, 0); }
inline void* vBits18(void* self) { return reinterpret_cast<BitsSlot_t>(slot(self, 0x18))(self, 0); }
inline void* vBits20(void* self) { return reinterpret_cast<BitsSlot_t>(slot(self, 0x20))(self, 0); }
// +0x24
inline void vUnlock(void* self, int flag) { reinterpret_cast<UnlockSlot_t>(slot(self, 0x24))(self, 0, flag); }
// +0xd8 / +0xdc / +0xe0
inline int vWidth(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0xd8))(self, 0); }
inline int vHeight(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0xdc))(self, 0); }
inline int vPitch(void* self) { return reinterpret_cast<IntSlot_t>(slot(self, 0xe0))(self, 0); }
// +0xe4, a pointer to this+0x24: [0] is the depth, [1] the format code
inline int* vFormat(void* self) { return reinterpret_cast<FormatSlot_t>(slot(self, 0xe4))(self, 0); }
// Palette +0x18 / +0x1c
inline unsigned short* vTable565(void* pal) { return reinterpret_cast<TableSlot_t>(slot(pal, 0x18))(pal, 0); }
inline unsigned short* vTable555(void* pal) { return reinterpret_cast<TableSlot_t>(slot(pal, 0x1c))(pal, 0); }

// ------------------------------------------------------------------ the module's own helpers, called where the
// originals call them (through their module addresses, so the comparison is of this code and not of a copy).

typedef int(__cdecl* Intersect_t)(RECT*, const RECT*, const RECT*);   // 0x10008590
typedef void(__cdecl* SetRect_t)(RECT*, int, int, int, int);          // 0x10008360 (x, y, w, h)
typedef int(__cdecl* Abs_t)(int);                                     // 0x1007f370 (CRT abs)
typedef void(__fastcall* Line4_t)(void*, void*, int, int, int, int);  // hline/vline, hline16/vline16
typedef int(__stdcall* IntersectRectApi_t)(RECT*, const RECT*, const RECT*);   // USER32, IAT slot 0x1012c5e4

inline int jIntersect(RECT* d, const RECT* a, const RECT* b) {
    return reinterpret_cast<Intersect_t>(JgldBase() + 0x8590)(d, a, b);
}
inline void jSetRect(RECT* r, int x, int y, int w, int h) {
    reinterpret_cast<SetRect_t>(JgldBase() + 0x8360)(r, x, y, w, h);
}
inline int jAbs(int v) { return reinterpret_cast<Abs_t>(JgldBase() + 0x7f370)(v); }
// The USER32 IntersectRect blit16from16 calls through jgld's import slot at VA 0x1012c5e4 (call at 0x1000f706);
// read the slot so both arms enter the same routine.
inline int jIntersectRectApi(RECT* d, const RECT* a, const RECT* b) {
    IntersectRectApi_t f = *reinterpret_cast<IntersectRectApi_t*>(JgldBase() + 0x12c5e4);
    return f(d, a, b);
}

// The field at Surface+0x7c, read directly (not through a slot) as the Palette of this surface: blitKey8to16
// tests it at 0x10012a7f and line16 at 0x1000b1c2.
inline void* surfacePalette(void* self) {
    return *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x7c);
}
// The row stride read as a FIELD, not through slot +0xe0: blitT_8to8 reads [dst+0x40] at 0x10013e99 and
// [this+0x40] at 0x10013ea2 (and at the matching addresses in its other three copy loops).
inline int surfacePitchField(void* self) {
    return *reinterpret_cast<int*>(reinterpret_cast<char*>(self) + 0x40);
}
// The module global at VA 0x1012873c: when a surface carries no Palette of its own, the Palette of the object
// this global points at (its field +4) is used instead (blitKey8to16 0x10012a90, line16 0x1000b1d3).
inline void* paletteClient() { return *reinterpret_cast<void**>(JgldBase() + 0x12873c); }

// The loop-state globals these bodies park values in. None is read by anything else in the module and the
// running game never writes any of them (log/c3/alt/c3bc_probe.py), so they are not state regions; what each
// one is set to is verified through the pixels the loops write.
inline int& gBlitTRem() { return *reinterpret_cast<int*>(JgldBase() + 0x1284b4); }   // blitT_8to8 cols & 3
inline int& gKeyCols() { return *reinterpret_cast<int*>(JgldBase() + 0x12848c); }    // blitKey8to16 pixels per row
inline int& gKeyDstSkip() { return *reinterpret_cast<int*>(JgldBase() + 0x128490); } // blitKey8to16 dest row skip
inline int& gKeyRows() { return *reinterpret_cast<int*>(JgldBase() + 0x128494); }    // blitKey8to16 row counter
inline unsigned short*& gKeyTable() {                                                // blitKey8to16 colour table
    return *reinterpret_cast<unsigned short**>(JgldBase() + 0x12849c);
}
inline int& gKeyGroups() { return *reinterpret_cast<int*>(JgldBase() + 0x1284a0); }  // blitKey8to16 cols >> 2
inline int& gKeySrcSkip() { return *reinterpret_cast<int*>(JgldBase() + 0x1284a4); } // blitKey8to16 source row skip

// =================================================================================== 0x00013c50 blitT_8to8

// 0x00013c50  Surface::blitT_8to8(dst, sx, sy, dx, dy, w, h) (VA 0x10013c50): copies a w x h block of 8-bit
// pixels from (sx, sy) of `this` to (dx, dy) of dst, with four separate loops for the case dst == this so that
// an overlapping copy still reads each byte before it is overwritten. `ret 0x1c`, returns int.
//
// Entry tests: [this+0x4c0] (the pixel base field, read directly) zero returns 7 (0x10013c7d jne over
// 0x10013c7f), a null dst returns 16 (0x10013c8d jne over 0x10013c8f). A negative sx adds itself to w, is
// subtracted from dx and becomes 0 (0x10013c9d jge over 0x10013c9f..0x10013cb1); a negative sy does the same to
// h and dy (0x10013cbc jge over 0x10013cbe..0x10013cd0). An sx past `this`'s width (slot +0xd8) or an sy past
// its height (slot +0xdc) returns 0 (0x10013cf1 jle, 0x10013d14 jle, so sx == width is still accepted); w is cut
// to width - sx when sx + w overflows the width (0x10013d3c jle skips the cut at 0x10013d58) and h to
// height - sy likewise (0x10013d7a jle, 0x10013d96).
//
// The destination rectangle (dx, dy, w, h) is built by rectFromXYWH 0x10008360 (0x10013dad) and intersected with
// the CLIP RECTANGLE OF dst, slot +0xcc on it (0x10013dbf), by intersect 0x10008590 (0x10013dd5); an empty
// intersection returns 0 (0x10013ddf jne over 0x10013de1). Whatever the intersection trimmed off the left and
// the top is added to sx and sy (0x10013de8..0x10013e01) and w and h are replaced by its width and height
// (0x10013e04..0x10013e13).
//
// Then `cmp dst, this` at 0x10013e19 (je 0x10013ef1) picks one of five copy bodies. All five lock the
// destination through dst's slot +0x14 and the source through `this`'s slot +0x14, read both row strides from
// the FIELD [surface+0x40] (not slot +0xe0), park cols & 3 in the module global 0x101284b4, and copy each row
// as `rep movsd` of cols >> 2 dwords followed by `rep movsb` of cols & 3 bytes:
//
//   dst != this      (0x10013e22): lock at (rc.left, rc.top) and (sx, sy), cld, row step +(pitch - cols).
//   dst == this and dx <  sx and dy <  sy (0x10013f09): identical to the above.
//   dst == this and dx <  sx and dy >= sy (0x10013fd8): lock at (rc.left, rc.top + h - 1) and (sx, sy + h - 1),
//                                 cld, row step -(pitch + cols), so the rows run bottom to top.
//   dst == this and dx >= sx and dy <  sy (0x100140b5): lock at (rc.left + k, rc.top) and (sx + k, sy) with
//                                 k = cols - 1 when cols < 4 (0x100140c5 jge, 0x100140d3) and cols - 4 otherwise
//                                 (0x10014121), std, row step +(pitch + cols): the bytes of a row run right to
//                                 left and the rows top to bottom.
//   dst == this and dx >= sx and dy >= sy (0x100141f3): the same k (0x100141f7 jge, 0x1001420c / 0x10014268),
//                                 lock at (.. , rc.top + h - 1) / (.., sy + h - 1), std, row step -(pitch - cols).
//
// The two descending bodies are only correct when cols < 4 or cols % 4 == 0. With cols >= 4 and cols % 4 != 0
// the cursor starts at cols - 4 from the left of the row, `rep movsd` writes the top 4 * (cols >> 2) bytes of
// the row and `rep movsb` then continues DOWNWARD from four bytes below that, which is (cols & 3) bytes ending
// at rc.left - 3: the leftmost (cols & 3) bytes of the rectangle are left untouched and the three bytes before
// the rectangle are overwritten instead. The A/B drives that case (cols 5, 6, 7 with rc.left >= 4) and both arms
// write the same bytes outside the rectangle.
//
// Null returns: in the first three bodies the destination address is tested first and returns 3 with no unlock
// (0x10013e45 / 0x10013f2c / 0x10014002), and the source address returns 3 after releasing dst with slot +0x24
// and 1 (0x10013e74 -> 0x10013e82, 0x10013f5b -> 0x10013f69, 0x10014038 -> 0x10014046). In the two descending
// bodies BOTH addresses are taken before either is tested (0x10014161 / 0x10014171 and 0x100142af /
// 0x100142bf), with the same two outcomes. On success `this` is released first (0x10014348) and dst second
// (0x1001435e) and the function returns 0 (0x10014368).
typedef int(__fastcall* BlitT8to8_t)(void*, void*, void*, int, int, int, int, int, int);
BlitT8to8_t BlitT8to8_orig;

// One row of `rep movsd` + `rep movsb` with DF clear: dwords first, then the cols & 3 tail, both ascending.
inline void rowCopyUp(unsigned char*& d, unsigned char*& s, int cols) {
    for (int n = cols >> 2; n > 0; n--) {
        *reinterpret_cast<unsigned int*>(d) = *reinterpret_cast<const unsigned int*>(s);
        d += 4;
        s += 4;
    }
    for (int m = gBlitTRem(); m > 0; m--) {         // the count is re-read from 0x101284b4 once per row
        *d++ = *s++;
    }
}
// The same two string moves with DF set: each dword and each byte is written at the CURRENT address and both
// cursors then step BACKWARD, which is why the tail lands (cols & 3) bytes below where the dwords ended.
inline void rowCopyDown(unsigned char*& d, unsigned char*& s, int cols) {
    for (int n = cols >> 2; n > 0; n--) {
        *reinterpret_cast<unsigned int*>(d) = *reinterpret_cast<const unsigned int*>(s);
        d -= 4;
        s -= 4;
    }
    for (int m = gBlitTRem(); m > 0; m--) {
        *d-- = *s--;
    }
}

int __fastcall BlitT8to8_re(void* self, void*, void* dst, int sx, int sy, int dx, int dy, int w, int h) {
    if (*reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 0x4c0) == 0) return 7;   // 0x10013c76
    if (dst == 0) return 16;                                                               // 0x10013c89
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }                                             // 0x10013c99
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }                                             // 0x10013cb8
    if (sx > vWidth(self)) return 0;                                                       // 0x10013cee
    if (sy > vHeight(self)) return 0;                                                      // 0x10013d11
    if (sx + w > vWidth(self)) w = vWidth(self) - sx;                                      // 0x10013d3a
    if (sy + h > vHeight(self)) h = vHeight(self) - sy;                                    // 0x10013d78
    RECT rc;
    jSetRect(&rc, dx, dy, w, h);                                                           // 0x10013dad
    if (!jIntersect(&rc, &rc, vClip(dst))) return 0;                                       // 0x10013dd5
    sx += rc.left - dx;                                                                    // 0x10013de8
    sy += rc.top - dy;                                                                     // 0x10013df6
    w = rc.right - rc.left;                                                                // 0x10013e04
    h = rc.bottom - rc.top;                                                                // 0x10013e0d

    unsigned char* d;
    unsigned char* s;
    int dstep;                      // what is added to the destination cursor after each row
    int sstep;
    const bool same = (dst == self);                                                       // 0x10013e19
    if (!same || (dx < sx && dy < sy)) {                                 // 0x10013ef1 jge, 0x10013efd jge
        d = reinterpret_cast<unsigned char*>(vLockAt8(dst, rc.left, rc.top));
        if (d == 0) return 3;
        s = reinterpret_cast<unsigned char*>(vLockAt8(self, sx, sy));
        if (s == 0) { vUnlock(dst, 1); return 3; }
        dstep = surfacePitchField(dst) - w;                              // 0x10013eb9 / 0x10013fa0
        sstep = surfacePitchField(self) - w;                             // 0x10013ebf / 0x10013fa6
        gBlitTRem() = w & 3;                                             // 0x10013ece / 0x10013fb5
        for (int r = h; r > 0; r--) {
            rowCopyUp(d, s, w);
            d += dstep;
            s += sstep;
        }
    } else if (dx < sx) {                                                // dy >= sy: 0x10013fd8
        d = reinterpret_cast<unsigned char*>(vLockAt8(dst, rc.left, rc.top + h - 1));
        if (d == 0) return 3;
        s = reinterpret_cast<unsigned char*>(vLockAt8(self, sx, sy + h - 1));
        if (s == 0) { vUnlock(dst, 1); return 3; }
        dstep = -(surfacePitchField(dst) + w);                           // 0x1001407d, used by `sub` 0x100140a6
        sstep = -(surfacePitchField(self) + w);                          // 0x10014083, `sub` 0x100140a4
        gBlitTRem() = w & 3;                                             // 0x10014092
        for (int r = h; r > 0; r--) {
            rowCopyUp(d, s, w);
            d += dstep;
            s += sstep;
        }
    } else if (dy < sy) {                                                // dx >= sx: 0x100140b5
        const int k = (w < 4) ? (w - 1) : (w - 4);                       // 0x100140c1 jge
        d = reinterpret_cast<unsigned char*>(vLockAt8(dst, rc.left + k, rc.top));
        s = reinterpret_cast<unsigned char*>(vLockAt8(self, sx + k, sy));
        if (d == 0) return 3;                                            // 0x10014161
        if (s == 0) { vUnlock(dst, 1); return 3; }                       // 0x10014171
        dstep = surfacePitchField(dst) + w;                              // 0x100141ba, `add` 0x100141e3
        sstep = surfacePitchField(self) + w;                             // 0x100141c0, `add` 0x100141e1
        gBlitTRem() = w & 3;                                             // 0x100141cf
        for (int r = h; r > 0; r--) {
            rowCopyDown(d, s, w);
            d += dstep;
            s += sstep;
        }
    } else {                                                             // dx >= sx, dy >= sy: 0x100141f3
        const int k = (w < 4) ? (w - 1) : (w - 4);                       // 0x100141f7 jge
        d = reinterpret_cast<unsigned char*>(vLockAt8(dst, rc.left + k, rc.top + h - 1));
        s = reinterpret_cast<unsigned char*>(vLockAt8(self, sx + k, sy + h - 1));
        if (d == 0) return 3;                                            // 0x100142af
        if (s == 0) { vUnlock(dst, 1); return 3; }                       // 0x100142bf
        dstep = -(surfacePitchField(dst) - w);                           // 0x10014308, `sub` 0x10014331
        sstep = -(surfacePitchField(self) - w);                          // 0x1001430e, `sub` 0x1001432f
        gBlitTRem() = w & 3;                                             // 0x1001431d
        for (int r = h; r > 0; r--) {
            rowCopyDown(d, s, w);
            d += dstep;
            s += sstep;
        }
    }
    vUnlock(self, 1);                                                    // 0x10014348
    vUnlock(dst, 1);                                                     // 0x1001435e
    return 0;
}

// ================================================================================= 0x00012900 blitKey8to16

// 0x00012900  Surface::blitKey8to16(dst, sx, sy, dx, dy, w, h, key) (VA 0x10012900): copies a w x h block of
// 8-bit pixels from (sx, sy) of `this` into the 16-bit surface dst at (dx, dy), writing only where dst already
// holds the colour the key maps to. `ret 0x20`, returns int.
//
// The clipping preamble is blitT_8to8's, instruction for instruction: null dst returns 3 (0x10012921), negative
// sx / sy fold into w / dx and h / dy (0x10012931, 0x10012950), sx past width() or sy past height() returns 0
// (0x10012985 jle, 0x100129a8 jle), w and h are cut to the remaining width and height (0x100129d0, 0x10012a0e),
// the destination rectangle is built by rectFromXYWH 0x10008360 (0x10012a41) and intersected with dst's clip
// rectangle (slot +0xcc at 0x10012a53) by intersect 0x10008590 (0x10012a69), an empty intersection returning 0
// (0x10012a73).
//
// The colour table then comes from the PALETTE OF `this` ([this+0x7c] at 0x10012a7f, the client global
// 0x1012873c's field +4 when it is null, 0x10012a90) but is SELECTED by the FORMAT CODE OF dst (slot +0xe4 on
// dst, element [1], 0x10012ab5..0x10012ac2): code 0 takes Palette slot +0x18 (0x10012ae0) and code 1 slot +0x1c
// (0x10012afb), each stored into the module global 0x1012849c (0x10012aea / 0x10012b05); any other code returns 1
// (0x10012b0c) and a null Palette returns 16 (0x10012aa1).
//
// What the intersection trimmed is applied to sx, sy, w, h (0x10012b16..0x10012b41), the destination address
// comes from dst's slot +0x1c at (rc.left, rc.top) (0x10012b56) and returns 3 when NULL (0x10012b67), the source
// address from `this`'s slot +0x14 at (sx, sy) (0x10012b85) and returns 3 when NULL (0x10012b96). Both pitches
// come from slot +0xe0, dst's at 0x10012bac and `this`'s at 0x10012bc6, and four more globals are loaded:
// 0x101284a0 with cols >> 2 (0x10012bee, which nothing reads), 0x101284a4 with srcPitch - cols (0x10012bfa),
// 0x10128490 with (dstPitch - cols) * 2 (0x10012c07), 0x1012848c with cols (0x10012c2e) and 0x10128494 with rows
// (0x10012c37).
//
// The loop compares the 16-bit word at the destination with table[key] (computed once into ax at
// 0x10012c15..0x10012c22) and, where they are equal (0x10012c4d jne), replaces it with table[source byte]
// (0x10012c4f..0x10012c5e); the source cursor advances one byte and the destination two (0x10012c61, 0x10012c62)
// and the per-row counter is `dec edx` reloaded from 0x1012848c each row (0x10012c44, 0x10012c65). After each
// row the two skips are added (0x10012c68, 0x10012c6a) and the row counter in 0x10128494 is decremented
// (0x10012c6c). Both surfaces are released with slot +0x24 and 1, `this` first (0x10012c83) and dst second
// (0x10012c99), and the function returns 0 (0x10012ca3).
typedef int(__fastcall* BlitKey8to16_t)(void*, void*, void*, int, int, int, int, int, int, unsigned);
BlitKey8to16_t BlitKey8to16_orig;
int __fastcall BlitKey8to16_re(void* self, void*, void* dst, int sx, int sy, int dx, int dy, int w, int h,
                               unsigned key) {
    if (dst == 0) return 3;                                                                // 0x1001291d
    if (sx < 0) { w += sx; dx -= sx; sx = 0; }                                             // 0x1001292d
    if (sy < 0) { h += sy; dy -= sy; sy = 0; }                                             // 0x1001294c
    if (sx > vWidth(self)) return 0;                                                       // 0x10012982
    if (sy > vHeight(self)) return 0;                                                      // 0x100129a5
    if (sx + w > vWidth(self)) w = vWidth(self) - sx;                                      // 0x100129ce
    if (sy + h > vHeight(self)) h = vHeight(self) - sy;                                    // 0x10012a0c
    RECT rc;
    jSetRect(&rc, dx, dy, w, h);                                                           // 0x10012a41
    if (!jIntersect(&rc, &rc, vClip(dst))) return 0;                                       // 0x10012a69

    void* pal;
    if (surfacePalette(self) != 0) {
        pal = surfacePalette(self);                                                        // 0x10012a85
    } else {
        pal = *reinterpret_cast<void**>(reinterpret_cast<char*>(paletteClient()) + 4);     // 0x10012a95
    }
    if (pal == 0) return 16;                                                               // 0x10012a9b
    const int fmt = vFormat(dst)[1];                                                       // 0x10012ab5
    if (fmt == 0) {
        gKeyTable() = vTable565(pal);                                                      // 0x10012ae0
    } else if (fmt == 1) {
        gKeyTable() = vTable555(pal);                                                      // 0x10012afb
    } else {
        return 1;                                                                          // 0x10012b0c
    }

    sx += rc.left - dx;                                                                    // 0x10012b16
    sy += rc.top - dy;                                                                     // 0x10012b24
    w = rc.right - rc.left;                                                                // 0x10012b32
    h = rc.bottom - rc.top;                                                                // 0x10012b3b
    unsigned short* d = reinterpret_cast<unsigned short*>(vLockAt16(dst, rc.left, rc.top));
    if (d == 0) return 3;                                                                  // 0x10012b63
    unsigned char* s = reinterpret_cast<unsigned char*>(vLockAt8(self, sx, sy));
    if (s == 0) return 3;                                                                  // 0x10012b92
    const int dstPitch = vPitch(dst);                                                      // 0x10012bac
    const int srcPitch = vPitch(self);                                                     // 0x10012bc6
    const int rows = rc.bottom - rc.top;                                                   // 0x10012bd6
    const int cols = rc.right - rc.left;                                                   // 0x10012bdf
    gKeyGroups() = cols >> 2;                                                              // 0x10012bee (unread)
    gKeySrcSkip() = srcPitch - cols;                                                       // 0x10012bfa
    gKeyDstSkip() = (dstPitch - cols) * 2;                                                 // 0x10012c07
    const unsigned short keyColor = gKeyTable()[key & 0xff];                               // 0x10012c22
    const int srcSkip = gKeySrcSkip();                                                     // ecx, read once (0x10012c25)
    gKeyCols() = cols;                                                                     // 0x10012c2e
    gKeyRows() = rows;                                                                     // 0x10012c37
    const int dstSkip = gKeyDstSkip();                                                     // ebp, read once (0x10012c3e)
    do {
        int n = gKeyCols();                                                                // 0x10012c44
        do {
            if (*d == keyColor) *d = gKeyTable()[*s];                                      // 0x10012c4a / 0x10012c5e
            s++;                                                                           // 0x10012c61
            d++;                                                                           // 0x10012c62
        } while (--n != 0);                                                                // 0x10012c65
        s += srcSkip;                                                                      // 0x10012c68
        d = reinterpret_cast<unsigned short*>(reinterpret_cast<char*>(d) + dstSkip);       // 0x10012c6a
    } while (--gKeyRows() != 0);                                                           // 0x10012c6c
    vUnlock(self, 1);                                                                      // 0x10012c83
    vUnlock(dst, 1);                                                                       // 0x10012c99
    return 0;
}

// ================================================================================= 0x0000f6a0 blit16from16

// 0x0000f6a0  Surface::blit16from16(r, src, alpha, table) (VA 0x1000f6a0): writes a shaded version of a
// rectangle of `this` (the destination) over itself, taking the colour from src wherever `this` already holds
// 0x7c1f. `ret 0x10`, returns int.
//
// The shade index is the only floating point: `fild dword [alpha]` (0x1000f6bd) divides by the float 100.0f at
// VA 0x1011d34c (0x1000f6c0), multiplies by the float 15.0f at VA 0x1011d348 (0x1000f6c6) and converts with the
// CRT __ftol at 0x1007f12c (0x1000f6cc); there is no intermediate store, so the whole expression runs at the
// thread's precision control, and the result n gives shade = (15 - n) << 15 (0x1000f6d1..0x1000f6d8). The table
// argument is copied to a local (0x1000f6e1).
//
// The rectangle is intersected IN PLACE with the bounds rectangle of `this` (slot +0xd4 at 0x1000f6ee) by
// USER32's IntersectRect through the import slot 0x1012c5e4 (0x1000f706); an empty intersection returns 3
// (0x1000f715 jne over 0x1000f717). The destination address is `this`'s slot +0x1c at (r->left, r->top)
// (0x1000f738) and the source address src's slot +0x1c at the SAME point (0x1000f75c); NEITHER is tested for
// NULL, so a surface without pixels would fault and the A/B never passes one. w is r->right - r->left
// (0x1000f774), h the low 16 bits of r->bottom - r->top (`mov word ptr [ebp-0x18], dx` at 0x1000f783) and the
// row skip is (pitch() - w) * 2 from `this`'s slot +0xe0 (0x1000f791..0x1000f7a3) — the SAME skip is added to
// both cursors (0x1000f7f0, 0x1000f7f3), so a src whose stride differs from `this`'s walks off its own rows.
//
// The loop keeps w in the high half and h in the low half of one register (0x1000f7ae..0x1000f7b4) and counts
// both down 16 bits at a time (`dec cx` 0x1000f7eb, `dec dx` 0x1000f7fa), so a zero w or h would run 65536
// times; the intersection makes both at least 1. Per pixel the index is the shade OR'd with a 16-bit colour
// (only the low half of the index is OR'd, 0x1000f7cd / 0x1000f7da) and the word written is table[index]
// (0x1000f7d0 / 0x1000f7dd, store 0x1000f7d5 / 0x1000f7e2): the colour OR'd in is the SOURCE pixel when the
// destination pixel is 0x7c1f (0x1000f7c6 cmp, 0x1000f7cb jne) and the DESTINATION pixel otherwise. Nothing is
// released and the function returns 0 (0x1000f801).
typedef int(__fastcall* Blit16from16_t)(void*, void*, RECT*, void*, int, const unsigned short*);
Blit16from16_t Blit16from16_orig;
int __fastcall Blit16from16_re(void* self, void*, RECT* r, void* src, int alpha, const unsigned short* table) {
    // fild / fdiv 100.0f / fmul 15.0f / __ftol, written as the original's four instructions so that the
    // conversion is jgld's own (64-bit fistp, low dword kept) and the two float constants are the module's.
    const float* c100 = reinterpret_cast<const float*>(JgldBase() + 0x11d34c);
    const float* c15 = reinterpret_cast<const float*>(JgldBase() + 0x11d348);
    void* ftol = JgldBase() + 0x7f12c;
    int n;
    __asm {
        mov  eax, c100
        mov  ecx, c15
        mov  edx, ftol
        fild dword ptr [alpha]
        fdiv dword ptr [eax]
        fmul dword ptr [ecx]
        call edx
        mov  n, eax
    }
    const unsigned shade = static_cast<unsigned>(15 - n) << 15;          // 0x1000f6d1..0x1000f6d8
    const unsigned short* tbl = table;                                   // 0x1000f6e1

    if (!jIntersectRectApi(r, r, vBounds(self))) return 3;               // 0x1000f706 / 0x1000f715
    unsigned short* d = reinterpret_cast<unsigned short*>(vLockAt16(self, r->left, r->top));
    unsigned short* s = reinterpret_cast<unsigned short*>(vLockAt16(src, r->left, r->top));
    const int w = r->right - r->left;                                    // 0x1000f774
    const unsigned short h = static_cast<unsigned short>(r->bottom - r->top);   // 0x1000f783
    const int skip = (vPitch(self) - w) * 2;                             // 0x1000f79e / 0x1000f7a1

    unsigned pair = (static_cast<unsigned>(w) << 16) | h;                // 0x1000f7ae / 0x1000f7b4
    for (;;) {
        unsigned short cx = static_cast<unsigned short>(pair >> 16);     // 0x1000f7bf / 0x1000f7c1
        for (;;) {
            unsigned idx = shade;                                        // 0x1000f7c4
            if (*d == 0x7c1f) {                                          // 0x1000f7c6
                idx = (idx & 0xffff0000u) | static_cast<unsigned short>(idx | *s);   // 0x1000f7cd
            } else {
                idx = (idx & 0xffff0000u) | static_cast<unsigned short>(idx | *d);   // 0x1000f7da
            }
            *d = tbl[idx];                                               // 0x1000f7d0 / 0x1000f7dd
            s++;                                                         // 0x1000f7e5
            d++;                                                         // 0x1000f7e8
            if (--cx == 0) break;                                        // 0x1000f7eb
        }
        s = reinterpret_cast<unsigned short*>(reinterpret_cast<char*>(s) + skip);   // 0x1000f7f0
        d = reinterpret_cast<unsigned short*>(reinterpret_cast<char*>(d) + skip);   // 0x1000f7f3
        unsigned short dh = static_cast<unsigned short>(pair);
        if (--dh == 0) break;                                            // 0x1000f7fa
        pair = (pair & 0xffff0000u) | dh;
    }
    return 0;                                                            // 0x1000f801
}

// ====================================================================================== 0x0000fb70 line8

// 0x0000fb70  Surface::line8(x1, y1, x2, y2, c) (VA 0x1000fb70): a Bresenham line of the byte c on an 8-bit
// surface, clipped to the clip rectangle. `ret 0x14`, returns void.
//
// A vertical line (x1 == x2, 0x1000fb90 jne) is handed to vline 0x100103f0 (0x1000fba8) and a horizontal one
// (y1 == y2, 0x1000fbb5 jne) to hline 0x100103f0's twin 0x10010620 (0x1000fbcd); both return at once. x2 < x1
// swaps the two endpoints with two `xchg`-based exchanges written as inline assembly in the original
// (0x1000fbd8 jge over 0x1000fbda..0x1000fbea). If lockBits() (slot +0x10, 0x1000fbfb) is NULL the function
// returns without releasing anything (0x1000fc03); from here every exit goes through the single `unlock(1)` at
// 0x100101a3 (the eleven `jmp` stubs at 0x100101b7..0x100101d5 are VC6's forward-goto stubs).
//
// getClipRect (slot +0xc8, 0x1000fc1c) copies the clip rectangle into a local, dx = x2 - x1 (0x1000fc2f) and
// dy = y2 - y1 (0x1000fc38). `y1 < y2` (0x1000fc41 jge) picks the two downward octants and the other arm the two
// upward ones; each pair first rejects a line entirely outside the rectangle (downward 0x1000fc4d / 0x1000fc55 /
// 0x1000fc5d / 0x1000fc65, upward 0x1000ff00 / 0x1000ff08 / 0x1000ff10 / 0x1000ff18) and then splits on
// dx >= dy (0x1000fc72 jl / 0x1000ff32 jl) into the x-major and the y-major octant. In the upward pair dy is
// first replaced by the CRT abs 0x1007f370 (0x1000ff1b).
//
// Every octant is the same shape: the decision variable d starts at 2 * minor - major with the two increments
// 2 * minor and 2 * (minor - major); a first loop steps the line forward one pixel at a time while it is still
// left of the rectangle or outside it vertically; the line is dropped when that walk ends past the right or
// bottom edge; the first pixel is written at bits() (slot +0x18) + x1 + y1 * pitch() (slot +0xe0); the far
// endpoint is clamped to the last column or row of the rectangle; and the main loop writes one byte per step,
// leaving through the shared unlock when the minor axis crosses the rectangle.
typedef void(__fastcall* Line8_t)(void*, void*, int, int, int, int, int);
Line8_t Line8_orig;
void __fastcall Line8_re(void* self, void*, int x1, int y1, int x2, int y2, int c) {
    if (x1 == x2) {                                                      // 0x1000fb90
        reinterpret_cast<Line4_t>(JgldBase() + 0x10620)(self, 0, x1, y1, y2, c);   // vline, 0x1000fba8
        return;
    }
    if (y1 == y2) {                                                      // 0x1000fbb5
        reinterpret_cast<Line4_t>(JgldBase() + 0x103f0)(self, 0, x1, x2, y1, c);   // hline, 0x1000fbcd
        return;
    }
    if (x2 < x1) {                                                       // 0x1000fbd8
        int t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
    }
    if (vLockBits(self) == 0) return;                                    // 0x1000fbfb / 0x1000fc01
    RECT rc;
    vGetClipRect(self, &rc);                                             // 0x1000fc1c
    const int dx = x2 - x1;                                              // 0x1000fc2f
    int dy = y2 - y1;                                                    // 0x1000fc38
    char* p;
    int d, incE, incNE;
    if (y1 < y2) {                                                       // 0x1000fc41
        if (x1 > rc.right || y1 > rc.bottom || x2 < rc.left || y2 < rc.top) { vUnlock(self, 1); return; }
        if (dx >= dy) {                                                  // 0x1000fc72
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 < rc.top) {                        // 0x1000fc9c / 0x1000fca4
                if (d <= 0) d += incE; else { d += incNE; y1++; }        // 0x1000fcaa
                x1++;
            }
            if (x1 >= rc.right || y1 >= rc.bottom) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            *p = static_cast<char>(c);
            if (x2 >= rc.right) x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y1++;
                    if (y1 >= rc.bottom) { vUnlock(self, 1); return; }
                    d += incNE;
                    p += vPitch(self);
                }
                p++;
                x1++;
                *p = static_cast<char>(c);
            }
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) d += incE; else { d += incNE; x1++; }
                y1++;
            }
            if (x1 >= rc.right || y1 >= rc.bottom) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            *p = static_cast<char>(c);
            if (y2 >= rc.bottom) y2 = rc.bottom - 1;
            while (y1 < y2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x1++;
                    if (x1 >= rc.right) { vUnlock(self, 1); return; }
                    d += incNE;
                    p++;
                }
                y1++;
                p += vPitch(self);
                *p = static_cast<char>(c);
            }
        }
    } else {
        if (x1 > rc.right || y1 < rc.top || x2 < rc.left || y2 > rc.bottom) { vUnlock(self, 1); return; }
        dy = jAbs(dy);                                                   // 0x1000ff1b
        if (dx >= dy) {                                                  // 0x1000ff32
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) d += incE; else { d += incNE; y1--; }
                x1++;
            }
            if (x1 >= rc.right || y1 < rc.top) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            *p = static_cast<char>(c);
            if (x2 >= rc.right) x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y1--;
                    if (y1 < rc.top) { vUnlock(self, 1); return; }
                    d += incNE;
                    p -= vPitch(self);
                }
                x1++;
                p++;
                *p = static_cast<char>(c);
            }
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) d += incE; else { d += incNE; x1++; }
                y1--;
            }
            if (x1 >= rc.right || y1 < rc.top) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            *p = static_cast<char>(c);
            if (y2 < rc.top) y2 = rc.top;
            while (y1 > y2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x1++;
                    if (x1 >= rc.right) { vUnlock(self, 1); return; }
                    d += incNE;
                    p++;
                }
                y1--;
                p -= vPitch(self);
                *p = static_cast<char>(c);
            }
        }
    }
    vUnlock(self, 1);                                                    // 0x100101a3
}

// ===================================================================================== 0x0000b140 line16

// 0x0000b140  Surface::line16(x1, y1, x2, y2, c) (VA 0x1000b140): line8's 16-bit twin. `ret 0x14`, returns void.
//
// Differences from line8, all visible in the disassembly:
//   * the vertical and horizontal cases go to vline16 0x1000bde0 (0x1000b178) and hline16 0x1000ba90
//     (0x1000b19d), with the RAW colour: the colour block below has not run yet (0x1000b163, 0x1000b188);
//   * between the horizontal case and the endpoint swap there is the colour block the 16-bit primitives of
//     batch c3ay share: a colour whose bit 31 is clear (0x1000b1b2 jne) is an index into a Palette, taken from
//     [this+0x7c] (0x1000b1c2) or, when that is null and the client global 0x1012873c is not (0x1000b1da), from
//     that client's field +4 (0x1000b1e2); a null Palette skips the lookup (0x1000b1ec je). The table is chosen
//     by the format code of slot +0xe4 element [1] (0x1000b20f / 0x1000b215): 0 takes Palette slot +0x18
//     (0x1000b227) and 1 slot +0x1c (0x1000b24f), each indexed by the colour's low byte (0x1000b234 /
//     0x1000b25c) and zero-extended from a word (0x1000b23c / 0x1000b264); any other code returns at once
//     (0x1000b21b -> 0x1000b26d), before lockBits, so nothing is released. The colour is then masked to 16 bits
//     on every path (0x1000b275);
//   * the base comes from slot +0x20 instead of slot +0x18 (0x1000b399, 0x1000b4dd, 0x1000b661, 0x1000b7a6);
//   * the cursor is a word pointer: the first pixel is base + x1 * 2 + y1 * pitch() * 2 (0x1000b3a6 /
//     0x1000b3c6), a step along the line is `add 2` (0x1000b442) and a step between rows is pitch() * 2
//     (0x1000b439, and `shl eax, 1` + `sub` at 0x1000b6fe for the upward octants).
// Everything else — the four octants, their guards, their pre-walks, their endpoint clamps and the single
// `unlock(1)` at 0x1000b867 reached through fourteen forward-goto stubs (0x1000b87f..0x1000b899) — is line8's.
typedef void(__fastcall* Line16_t)(void*, void*, int, int, int, int, unsigned);
Line16_t Line16_orig;
void __fastcall Line16_re(void* self, void*, int x1, int y1, int x2, int y2, unsigned c) {
    if (x1 == x2) {                                                      // 0x1000b160
        reinterpret_cast<Line4_t>(JgldBase() + 0xbde0)(self, 0, x1, y1, y2, static_cast<int>(c));
        return;
    }
    if (y1 == y2) {                                                      // 0x1000b185
        reinterpret_cast<Line4_t>(JgldBase() + 0xba90)(self, 0, x1, x2, y1, static_cast<int>(c));
        return;
    }
    if ((c & 0x80000000u) == 0) {                                        // 0x1000b1b0
        void* pal = 0;                                                   // 0x1000b1b8
        if (surfacePalette(self) != 0) {
            pal = surfacePalette(self);                                  // 0x1000b1c8
        } else if (paletteClient() != 0) {                               // 0x1000b1d3
            pal = *reinterpret_cast<void**>(reinterpret_cast<char*>(paletteClient()) + 4);
        }
        if (pal != 0) {                                                  // 0x1000b1e8
            const int fmt = vFormat(self)[1];                            // 0x1000b1fc / 0x1000b209
            if (fmt == 0) {
                c = vTable565(pal)[c & 0xff];                            // 0x1000b227 / 0x1000b23c
            } else if (fmt == 1) {
                c = vTable555(pal)[c & 0xff];                            // 0x1000b24f / 0x1000b264
            } else {
                return;                                                  // 0x1000b21b -> 0x1000b26d
            }
        }
    }
    c &= 0xffff;                                                         // 0x1000b275
    if (x2 < x1) {                                                       // 0x1000b283
        int t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
    }
    if (vLockBits(self) == 0) return;                                    // 0x1000b2a1 / 0x1000b2ad
    RECT rc;
    vGetClipRect(self, &rc);                                             // 0x1000b2c2
    const int dx = x2 - x1;                                              // 0x1000b2d2
    int dy = y2 - y1;                                                    // 0x1000b2db
    unsigned short* p;
    const unsigned short v = static_cast<unsigned short>(c);
    int d, incE, incNE;
    if (y1 < y2) {                                                       // 0x1000b2e7
        if (x1 > rc.right || y1 > rc.bottom || x2 < rc.left || y2 < rc.top) { vUnlock(self, 1); return; }
        if (dx >= dy) {                                                  // 0x1000b318
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 < rc.top) {                        // 0x1000b342 / 0x1000b34a
                if (d <= 0) d += incE; else { d += incNE; y1++; }        // 0x1000b350
                x1++;
            }
            if (x1 >= rc.right || y1 >= rc.bottom) { vUnlock(self, 1); return; }
            p = reinterpret_cast<unsigned short*>(vBits20(self)) + x1 + y1 * vPitch(self);
            *p = v;
            if (x2 >= rc.right) x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y1++;
                    if (y1 >= rc.bottom) { vUnlock(self, 1); return; }
                    d += incNE;
                    p += vPitch(self);
                }
                p++;
                x1++;
                *p = v;
            }
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) d += incE; else { d += incNE; x1++; }
                y1++;
            }
            if (x1 >= rc.right || y1 >= rc.bottom) { vUnlock(self, 1); return; }
            p = reinterpret_cast<unsigned short*>(vBits20(self)) + x1 + y1 * vPitch(self);
            *p = v;
            if (y2 >= rc.bottom) y2 = rc.bottom - 1;
            while (y1 < y2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x1++;
                    if (x1 >= rc.right) { vUnlock(self, 1); return; }
                    d += incNE;
                    p++;
                }
                y1++;
                p += vPitch(self);
                *p = v;
            }
        }
    } else {
        if (x1 > rc.right || y1 < rc.top || x2 < rc.left || y2 > rc.bottom) { vUnlock(self, 1); return; }
        dy = jAbs(dy);                                                   // 0x1000b5cf
        if (dx >= dy) {                                                  // 0x1000b5e0
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) d += incE; else { d += incNE; y1--; }
                x1++;
            }
            if (x1 >= rc.right || y1 < rc.top) { vUnlock(self, 1); return; }
            p = reinterpret_cast<unsigned short*>(vBits20(self)) + x1 + y1 * vPitch(self);
            *p = v;
            if (x2 >= rc.right) x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    y1--;
                    if (y1 < rc.top) { vUnlock(self, 1); return; }
                    d += incNE;
                    p -= vPitch(self);
                }
                x1++;
                p++;
                *p = v;
            }
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) d += incE; else { d += incNE; x1++; }
                y1--;
            }
            if (x1 >= rc.right || y1 < rc.top) { vUnlock(self, 1); return; }
            p = reinterpret_cast<unsigned short*>(vBits20(self)) + x1 + y1 * vPitch(self);
            *p = v;
            if (y2 < rc.top) y2 = rc.top;
            while (y1 > y2) {
                if (d <= 0) {
                    d += incE;
                } else {
                    x1++;
                    if (x1 >= rc.right) { vUnlock(self, 1); return; }
                    d += incNE;
                    p++;
                }
                y1--;
                p -= vPitch(self);
                *p = v;
            }
        }
    }
    vUnlock(self, 1);                                                    // 0x1000b867
}

// ================================================================================= 0x00010870 dashedLine8

// 0x00010870  Surface::dashedLine8(x1, y1, x2, y2, c1, c2, len1, len2, phase) (VA 0x10010870): line8 with a
// dash pattern — len1 pixels of c1 then len2 of c2, repeating, starting at the given phase. `ret 0x24`,
// returns void.
//
// Everything outside the per-pixel write is line8's, at the same relative positions: a vertical line goes to
// dashedVLine8 0x10011880 (0x100108b8, after 0x10010893 jne) and a horizontal one to dashedHLine8 0x100114e0
// (0x100108ed, after 0x100108c8 jne), both with all eight of the remaining arguments; x2 < x1 (0x100108fd jge)
// swaps the two endpoints (0x100108ff..0x1001090e), the two run lengths (0x10010911..0x10010929, three xors),
// the two colours (0x1001092c..0x10010944, three xors) and replaces the phase with len1 + len2 - phase
// (0x10010947..0x10010950); lockBits() of slot +0x10 returning NULL leaves without releasing anything
// (0x10010969); getClipRect of slot +0xc8 copies the clip rectangle (0x1001097e); and the whole body leaves
// through one `unlock(1)` at 0x10011216, reached by fourteen forward-goto stubs (0x1001122e..0x10011248).
//
// Between getClipRect and the octants the phase becomes a signed cursor, exactly as in dashedHLine16: it is
// reduced modulo len1 + len2 by the SIGNED `idiv` at 0x10010995 (so len1 + len2 must not be 0), a phase at or
// past len1 becomes len2 - (phase - len1) and a phase below len1 becomes -(len1 - phase) (0x100109a0 jl with
// the arms at 0x100109a2 and 0x100109b6), and len1 is then negated (0x100109ca). The cursor is negative inside
// a c1 run and non-negative inside a c2 run.
//
// The four octants are line8's, split by y1 < y2 (0x100109e7 jge) and then by dx >= dy (0x10010a18 jl in the
// downward pair, 0x10010e36 jl in the upward one, whose dy is first replaced by the CRT abs 0x1007f370 at
// 0x10010e25); the guards are at 0x100109ed..0x10010a0b and 0x10010e02..0x10010e1a and the base address comes
// from slot +0x18 (0x10010aa2, 0x10010c86, 0x10010ec0, 0x100110a5) with the stride from slot +0xe0.
//
// What replaces line8's single `*p = c` is the same eight-instruction block at both write sites of every
// octant (0x10010ad9 / 0x10010afe, 0x10010bb0 / 0x10010bd5, 0x10010cbd / 0x10010ce2, 0x10010daa / 0x10010dcf,
// 0x10010ef7 / 0x10010f1c, 0x10010fcf / 0x10010ff4, 0x100110dc / 0x10011101, 0x100111c7 / 0x100111ec): a
// negative cursor writes c1 unless c1 is -1 and then counts UP, reloading the cursor with len2 when it reaches
// zero; a non-negative cursor writes c2 unless c2 is -1 and then counts DOWN, reloading it with the negated
// len1 at zero. Unlike dashedHLine16 0xcf90 the colours are NOT masked first, so here -1 really is reachable
// and really does leave a gap.
typedef void(__fastcall* DashedLine8_t)(void*, void*, int, int, int, int, int, int, int, int, int);
DashedLine8_t DashedLine8_orig;
typedef void(__fastcall* DashedLine8Call_t)(void*, void*, int, int, int, int, int, int, int, int);
void __fastcall DashedLine8_re(void* self, void*, int x1, int y1, int x2, int y2, int c1, int c2,
                               int len1, int len2, int phase) {
    if (x1 == x2) {                                                      // 0x10010890
        reinterpret_cast<DashedLine8Call_t>(JgldBase() + 0x11880)(self, 0, x1, y1, y2, c1, c2,
                                                                  len1, len2, phase);   // dashedVLine8
        return;
    }
    if (y1 == y2) {                                                      // 0x100108c5
        reinterpret_cast<DashedLine8Call_t>(JgldBase() + 0x114e0)(self, 0, x1, x2, y1, c1, c2,
                                                                  len1, len2, phase);   // dashedHLine8
        return;
    }
    if (x2 < x1) {                                                       // 0x100108fd
        int t = x1; x1 = x2; x2 = t;
        t = y1; y1 = y2; y2 = t;
        len1 ^= len2; len2 ^= len1; len1 ^= len2;                        // 0x10010911
        c1 ^= c2; c2 ^= c1; c1 ^= c2;                                    // 0x1001092c
        phase = len1 + len2 - phase;                                     // 0x10010947
    }
    if (vLockBits(self) == 0) return;                                    // 0x1001095d / 0x10010967
    RECT rc;
    vGetClipRect(self, &rc);                                             // 0x1001097e
    phase = phase % (len1 + len2);                                       // 0x10010995
    if (phase >= len1) {                                                 // 0x100109a0
        phase -= len1;
        phase = len2 - phase;
    } else {
        phase = len1 - phase;
        phase = -phase;                                                  // 0x100109c2
    }
    len1 = -len1;                                                        // 0x100109ca

    const int dx = x2 - x1;                                              // 0x100109d2
    int dy = y2 - y1;                                                    // 0x100109db
    char* p;
    int d, incE, incNE;
    // The per-pixel block, identical at all eight write sites.
    #define C3BC_PLOT                               \
        if (phase < 0) {                            \
            if (c1 != -1) *p = static_cast<char>(c1);   \
            phase++;                                \
            if (phase == 0) phase = len2;           \
        } else {                                    \
            if (c2 != -1) *p = static_cast<char>(c2);   \
            phase--;                                \
            if (phase == 0) phase = len1;           \
        }
    if (y1 < y2) {                                                       // 0x100109e7
        if (x1 > rc.right || y1 > rc.bottom || x2 < rc.left || y2 < rc.top) { vUnlock(self, 1); return; }
        if (dx >= dy) {                                                  // 0x10010a18
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) { d += incE; x1++; } else { d += incNE; x1++; y1++; }
            }
            if (x1 >= rc.right || y1 >= rc.bottom) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            C3BC_PLOT
            if (x2 >= rc.right) x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE; x1++; p++;
                } else {
                    y1++;
                    if (y1 >= rc.bottom) { vUnlock(self, 1); return; }
                    d += incNE; x1++; p = p + vPitch(self) + 1;
                }
                C3BC_PLOT
            }
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 < rc.top) {
                if (d <= 0) { d += incE; y1++; } else { d += incNE; y1++; x1++; }
            }
            if (x1 >= rc.right || y1 >= rc.bottom) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            C3BC_PLOT
            if (y2 >= rc.bottom) y2 = rc.bottom - 1;
            while (y1 < y2) {
                if (d <= 0) {
                    d += incE; y1++; p += vPitch(self);
                } else {
                    x1++;
                    if (x1 >= rc.right) { vUnlock(self, 1); return; }
                    d += incNE; y1++; p = p + vPitch(self) + 1;
                }
                C3BC_PLOT
            }
        }
    } else {
        if (x1 > rc.right || y1 < rc.top || x2 < rc.left || y2 > rc.bottom) { vUnlock(self, 1); return; }
        dy = jAbs(dy);                                                   // 0x10010e25
        if (dx >= dy) {                                                  // 0x10010e36
            d = dy * 2 - dx;
            incE = dy * 2;
            incNE = (dy - dx) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) { d += incE; x1++; } else { d += incNE; x1++; y1--; }
            }
            if (x1 >= rc.right || y1 < rc.top) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            C3BC_PLOT
            if (x2 >= rc.right) x2 = rc.right - 1;
            while (x1 < x2) {
                if (d <= 0) {
                    d += incE; x1++; p++;
                } else {
                    y1--;
                    if (y1 < rc.top) { vUnlock(self, 1); return; }
                    d += incNE; x1++; p = p - vPitch(self) + 1;
                }
                C3BC_PLOT
            }
        } else {
            d = dx * 2 - dy;
            incE = dx * 2;
            incNE = (dx - dy) * 2;
            while (x1 < rc.left || y1 >= rc.bottom) {
                if (d <= 0) { d += incE; y1--; } else { d += incNE; y1--; x1++; }
            }
            if (x1 >= rc.right || y1 < rc.top) { vUnlock(self, 1); return; }
            p = reinterpret_cast<char*>(vBits18(self)) + x1 + y1 * vPitch(self);
            C3BC_PLOT
            if (y2 < rc.top) y2 = rc.top;
            while (y1 > y2) {
                if (d <= 0) {
                    d += incE; y1--; p -= vPitch(self);
                } else {
                    x1++;
                    if (x1 >= rc.right) { vUnlock(self, 1); return; }
                    d += incNE; y1--; p = p - vPitch(self) + 1;
                }
                C3BC_PLOT
            }
        }
    }
    #undef C3BC_PLOT
    vUnlock(self, 1);                                                    // 0x10011216
}

}  // namespace

SG_HOOK("jgld.dll", 0x00013c50, Surface_blitT_8to8, BlitT8to8_re, BlitT8to8_orig);
SG_HOOK("jgld.dll", 0x00012900, Surface_blitKey8to16, BlitKey8to16_re, BlitKey8to16_orig);
SG_HOOK("jgld.dll", 0x0000f6a0, Surface_blit16from16, Blit16from16_re, Blit16from16_orig);
SG_HOOK("jgld.dll", 0x0000fb70, Surface_line8, Line8_re, Line8_orig);
SG_HOOK("jgld.dll", 0x0000b140, Surface_line16, Line16_re, Line16_orig);
SG_HOOK("jgld.dll", 0x00010870, Surface_dashedLine8, DashedLine8_re, DashedLine8_orig);
