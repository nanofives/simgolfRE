// C3 batch c3p (2026-10-07) of golf_clean.exe: render-subsystem coordinate math, a pixel-address helper, a palette
// chunk decoder and three small object helpers. Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py
// golf_clean.exe 0x<addr> --list) and the C2 transcriptions; each body cites the address of every global, offset and
// callee it uses. __thiscall is emulated with __fastcall (ecx = this, edx unused). Callees, including the virtual
// methods reached through an object's vtable, are invoked through their original addresses, so a hooked callee still
// runs its own reimplementation and the path-1 A/B drives both arms.
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }
template <typename T> T field(const void* p, unsigned off) {
    return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(p) + off);
}

// A virtual method of `obj` taking no stack arguments (thiscall: ecx = obj), at vtable offset `off`.
typedef unsigned(__fastcall* Virt0_t)(void*, void*);
unsigned vcall0(void* obj, unsigned off) {
    const unsigned vtbl = *reinterpret_cast<const unsigned*>(obj);
    return reinterpret_cast<Virt0_t>(at<unsigned>(vtbl + off))(obj, 0);
}
// A virtual method of `obj` taking (buf, start, count) on the stack (thiscall, callee pops 3 dwords).
typedef unsigned(__fastcall* Virt3_t)(void*, void*, void*, int, int);
unsigned vcall3(void* obj, unsigned off, void* buf, int start, int count) {
    const unsigned vtbl = *reinterpret_cast<const unsigned*>(obj);
    return reinterpret_cast<Virt3_t>(at<unsigned>(vtbl + off))(obj, 0, buf, start, count);
}

// Signed division by a power of two that rounds toward zero (VC6's cdq / and / add / sar idiom).
inline int divTrunc(int v, int d) { return v / d; }
// 32-bit wrapping multiply (imul keeps the low dword).
inline int mul(int a, int b) { return static_cast<int>(static_cast<unsigned>(a) * static_cast<unsigned>(b)); }

// 0x00482e10  Link482::data(this): returns this+8 when the dword at this+4 is non-zero, else NULL (0x00482e10 reads
// [ecx+4]; 0x00482e15 jne to `lea eax, [ecx+8]` at 0x00482e1a; 0x00482e17 xor eax, eax).
typedef void*(__fastcall* Data_t)(void*, void*);
Data_t LinkData_orig;
void* __fastcall LinkData_re(void* self, void*) {
    if (field<int>(self, 4) == 0) return 0;
    return reinterpret_cast<char*>(self) + 8;
}

// 0x004837f0  C4837f0::ctor(this): stores 0 at this+4 (0x004837f2) and the vtable pointer 0x004ba468 at this+0
// (0x004837f9), and returns this (0x004837f0 mov eax, ecx).
typedef void*(__fastcall* Ctor_t)(void*, void*);
Ctor_t C4837f0Ctor_orig;
void* __fastcall C4837f0Ctor_re(void* self, void*) {
    *reinterpret_cast<unsigned*>(reinterpret_cast<char*>(self) + 4) = 0;
    *reinterpret_cast<unsigned*>(self) = 0x004ba468;
    return self;
}

// 0x00483060  Cache483060::detach(this): when the object pointer at this+4 is non-null (0x00483063 test / je), calls
// that object's virtual method at vtable +0xc with no stack arguments (0x00483067..0x00483069), ignoring its result;
// returns 0 either way (0x0048306c xor eax, eax). The field at this+4 is left unchanged.
typedef int(__fastcall* Detach_t)(void*, void*);
Detach_t CacheDetach_orig;
int __fastcall CacheDetach_re(void* self, void*) {
    void* child = field<void*>(self, 4);
    if (child != 0) vcall0(child, 0xc);
    return 0;
}

// 0x004796a0  Surface_pixelPtr(this, x, y): address of pixel (x, y) in the drawing surface held at this+4 (a jgld.dll
// Surface; its methods are reached through the vtable). Returns NULL when x >= width (virtual +0xd8, 0x004796af /
// 0x004796bf jge, signed) or y >= height (virtual +0xdc, 0x004796ce / 0x004796de jge, signed), with width and height
// taken as 0 when this+4 is null (0x004796ab / 0x004796ca). It then takes the base pointer from the virtual at +0x10
// (0x004796e9; no null check of this+4 there) and returns NULL when that base is 0 (0x004796ee). The bit depth is the
// dword the virtual at +0xe4 points at (0x004796ff / 0x00479709); with r = y * stride + x, where stride is the
// virtual at +0xe0 (0x0047972f etc.), the result is base + r for depth 8, base + 2r for 16, base + 3r for 24 and
// base + 4r for 32; any other depth returns NULL (switch on depth - 8 with `ja` at 0x00479711 and the index table at
// 0x00479804 selecting the jump table at 0x004797f0). The four case blocks re-test this+4 for null before the stride
// call (0x0047972b, 0x00479758, 0x00479787, 0x004797ba); this+4 was already dereferenced at 0x004796e7, so those
// null arms are kept for fidelity only.
typedef void*(__fastcall* PixelPtr_t)(void*, void*, int, int);
PixelPtr_t PixelPtr_orig;
void* __fastcall PixelPtr_re(void* self, void*, int x, int y) {
    void* s = field<void*>(self, 4);
    const int width = s ? static_cast<int>(vcall0(s, 0xd8)) : 0;
    if (x >= width) return 0;
    s = field<void*>(self, 4);
    const int height = s ? static_cast<int>(vcall0(s, 0xdc)) : 0;
    if (y >= height) return 0;
    char* const base = reinterpret_cast<char*>(vcall0(field<void*>(self, 4), 0x10));
    if (base == 0) return 0;
    s = field<void*>(self, 4);
    const int* depthp = s ? reinterpret_cast<const int*>(vcall0(s, 0xe4)) : 0;
    const int depth = *depthp;
    int scale;
    switch (depth) {
        case 8: scale = 1; break;
        case 16: scale = 2; break;
        case 24: scale = 3; break;
        case 32: scale = 4; break;
        default: return 0;
    }
    s = field<void*>(self, 4);
    const int stride = s ? static_cast<int>(vcall0(s, 0xe0)) : 0;
    const int r = mul(stride, y) + x;
    return base + mul(r, scale);
}

// 0x004826f0  Palette::decodeChunk(this, chunk): applies a palette-update chunk to the palette object reached through
// this+0x74. Returns 0 at once when this+0x74 is null (0x004826f9 / 0x004826fe). P = this+0x74; when P's object at
// P+4 is non-null its virtual at +0x10 copies the current 256 entries into a local 0x300-byte array (args (buf, 0,
// 0x100), 0x0048271c..0x0048272a). The chunk's packet count is the word at chunk+6 (0x0048272d); the packets follow
// at chunk+8. Each packet is a skip byte added to the running 8-bit entry index (starts at 0, 0x00482736 / 0x00482753)
// and a count byte (0 means 0x100, 0x0048275b..0x0048275f), followed by count 3-byte entries copied to local[index*3]
// .. local[index*3+2] with the index incremented (mod 256) after each (0x0048276e..0x00482798). Afterwards, when P+4
// is still null the virtual at P's vtable +0 is called on P (0x004827a5..0x004827ad), then the object at P+4 receives
// the array through its virtual at +0x14 with (buf, 0, 0x100) (0x004827af..0x004827c0). Returns 0 (0x004827c4).
// The packet loop's `count == 0` exit (0x00482767 test edx / je 0x0048279a) cannot be taken: count is 1..0x100.
typedef int(__fastcall* DecodeChunk_t)(void*, void*, const unsigned char*);
DecodeChunk_t DecodeChunk_orig;
int __fastcall DecodeChunk_re(void* self, void*, const unsigned char* chunk) {
    void* const P = field<void*>(self, 0x74);
    if (P == 0) return 0;
    unsigned char local[0x300];
    if (field<void*>(P, 4) != 0) vcall3(field<void*>(P, 4), 0x10, local, 0, 0x100);
    const unsigned char* p = chunk + 6;
    const unsigned packets = *reinterpret_cast<const unsigned short*>(p);
    p += 2;
    unsigned char index = 0;
    for (unsigned k = 0; k < packets; k++) {
        index = static_cast<unsigned char>(index + p[0]);
        unsigned count = p[1];
        p += 2;
        if (count == 0) count = 0x100;
        for (unsigned j = 0; j < count; j++) {
            unsigned char* const e = local + index * 3;
            e[0] = p[0];
            e[1] = p[1];
            e[2] = p[2];
            p += 3;
            index = static_cast<unsigned char>(index + 1);
        }
    }
    if (field<void*>(P, 4) == 0) vcall0(P, 0);
    vcall3(field<void*>(P, 4), 0x14, local, 0, 0x100);
    return 0;
}

// Callees of worldToScreen (both C3 elsewhere): cornerHeights (0x0040bfe0) and cornerRange (0x0042f4b0, fills the
// maximum through its third argument and the minimum through its fourth).
typedef int(__cdecl* CornerHeights_t)(int, int, int, int);
typedef void(__cdecl* CornerRange_t)(int, int, int*, int*);
const CornerHeights_t kCornerHeights = reinterpret_cast<CornerHeights_t>(0x0040bfe0);
const CornerRange_t kCornerRange = reinterpret_cast<CornerRange_t>(0x0042f4b0);

// 0x0042fb90  worldToScreen(wx, wy, sx, sy, margin): projects the world point (wx, wy) (1024 units per tile) to the
// screen and returns 1 when the result lies inside the view widened by margin, else 0. Globals: camera tile
// 0x004c2ba0 / 0x004c2ba4, zoom 0x004c2844 (Z), view width 0x00822c8c (W), view height 0x00822c90 (H), rotation
// 0x005685f4, height scale 0x004c2e00 (S), doubling flag 0x005a9cc0.
//  1. ix = ((wx - camX*1024 - 0x200) * Z) >> 7 and iy likewise from wy / camY (0x0042fb90..0x0042fbd5, arithmetic
//     shifts). W == 0x400: ix = ix*80/64, iy = iy*52/40 (0x0042fc0f..0x0042fc3a); W == 0x500: ix = ix*104/64,
//     iy = iy*68/40 (0x0042fbe8..0x0042fc3a); other W leave them (0x0042fbe6 jne). Divisions truncate toward zero.
//  2. Rotation 0: X = iy + W/2 + 8Z + ix, Y = 20iy/32 - 20ix/32 + H/2 (0x0042fc49..0x0042fc8e). 2: X = W/2 - ix + iy,
//     Y = -20ix/32 - 20iy/32 + H/2 - 5Z (0x0042fca5..0x0042fcf5). 4: X = W/2 - 8Z - iy - ix, Y = 20ix/32 - 20iy/32
//     + H/2 (0x0042fd06..0x0042fd57). 6: X = W/2 - iy + ix, Y = 20iy/32 + 20ix/32 + H/2 + 5Z (0x0042fd68..0x0042fdae).
//     Each is a separate equality test; any other rotation writes nothing and the tests below read *sx / *sy as the
//     caller left them.
//  3. Returns 0 unless -m <= X < W + m and -(m/2) <= Y < m/2 + H (0x0042fdc1..0x0042fe03).
//  4. Height: tile (tx, ty) = (wx >> 10, wy >> 10), cell = tx*50 + ty, f = the flags dword at 0x0057837c + 0x30 *
//     (signed type byte at 0x005722e8 + cell) (0x0042fe11..0x0042fe2d). f & 8: no change. Else f & 2: Y -= (min - 3)
//     * S * Z / 4 with min from cornerRange (0x0042fe3b..0x0042fe77). Else f & 4: Y -= (signed byte at 0x00543018 +
//     cell - 3) * S * Z / 4 (0x0042fe7e..0x0042fea5). Else: c_k = cornerHeights(tx, ty, k, 1) - 3 for k = 5, 7, 1, 3
//     (0x0042feac..0x0042fef2); when all four are equal Y -= c5 * S * Z / 4 (0x0042fef5..0x0042ff21); otherwise with
//     dx = wx - tx*1024, dy = wy - ty*1024 the bilinear blend B = ((0x400-dx)*c7 + dx*c1) * (0x400-dy) +
//     ((0x400-dx)*c5 + dx*c3) * dy, and Y += ((-(B*S*Z / 1024)) / 1024) / 4 (0x0042ff23..0x0042ffa6).
//  5. When the flag at 0x005a9cc0 is non-zero: X = 2X - 400, Y = 2Y - 300 (0x0042ffa8..0x0042ffc7).
//  6. Returns 1 when -m <= X < W + m and -(m/2) <= Y < m/2 + H again (0x0042ffc9..0x0042fffc), else 0.
typedef int(__cdecl* WorldToScreen_t)(int, int, int*, int*, int);
WorldToScreen_t WorldToScreen_orig;
int __cdecl WorldToScreen_re(int wx, int wy, int* sx, int* sy, int m) {
    const int Z = at<int>(0x004c2844);
    int ix = mul(wx - (at<int>(0x004c2ba0) << 10) - 0x200, Z) >> 7;
    int iy = mul(wy - (at<int>(0x004c2ba4) << 10) - 0x200, Z) >> 7;
    int W = at<int>(0x00822c8c);
    if (W == 0x400) {
        ix = divTrunc(mul(ix, 80), 64);
        iy = mul(iy, 52) / 40;
    } else if (W == 0x500) {
        ix = divTrunc(mul(ix, 104), 64);
        iy = mul(iy, 68) / 40;
    }
    const int H = at<int>(0x00822c90);
    const int ax = divTrunc(mul(ix, 20), 32), ay = divTrunc(mul(iy, 20), 32);
    if (at<int>(0x005685f4) == 0) {
        *sx = iy + W / 2 + Z * 8 + ix;
        *sy = ay - ax + H / 2;
        W = at<int>(0x00822c8c);
    }
    if (at<int>(0x005685f4) == 2) {
        *sx = W / 2 - ix + iy;
        *sy = divTrunc(mul(ix, -20), 32) - ay + H / 2 - at<int>(0x004c2844) * 5;
        W = at<int>(0x00822c8c);
    }
    if (at<int>(0x005685f4) == 4) {
        *sx = W / 2 - at<int>(0x004c2844) * 8 - iy - ix;
        *sy = ax - ay + H / 2;
        W = at<int>(0x00822c8c);
    }
    if (at<int>(0x005685f4) == 6) {
        *sx = W / 2 - iy + ix;
        *sy = ay + ax + H / 2 + at<int>(0x004c2844) * 5;
        W = at<int>(0x00822c8c);
    }
    const int nm = -m;
    if (*sx < nm || *sx >= W + m) return 0;
    const int half = m / 2;
    if (*sy < -half || *sy >= half + at<int>(0x00822c90)) return 0;

    const int tx = wx >> 10, ty = wy >> 10;
    const int cell = tx * 50 + ty;
    const unsigned f = at<unsigned>(0x0057837c + at<signed char>(0x005722e8 + cell) * 0x30);
    if (!(f & 8)) {
        if (f & 2) {
            int mx, mn;
            kCornerRange(tx, ty, &mx, &mn);
            *sy += -divTrunc(mul(mul(mn - 3, at<int>(0x004c2e00)), at<int>(0x004c2844)), 4);
        } else if (f & 4) {
            const int h = at<signed char>(0x00543018 + cell) - 3;
            *sy -= divTrunc(mul(mul(h, at<int>(0x004c2e00)), at<int>(0x004c2844)), 4);
        } else {
            const int c5 = kCornerHeights(tx, ty, 5, 1) - 3;
            const int c7 = kCornerHeights(tx, ty, 7, 1) - 3;
            const int c1 = kCornerHeights(tx, ty, 1, 1) - 3;
            const int c3 = kCornerHeights(tx, ty, 3, 1) - 3;
            int dz;
            if (c5 == c7 && c5 == c1 && c5 == c3) {
                dz = -divTrunc(mul(mul(at<int>(0x004c2e00), at<int>(0x004c2844)), c5), 4);
            } else {
                const int dx = wx - (tx << 10), dy = wy - (ty << 10);
                const int b = mul(mul(0x400 - dx, c7) + mul(dx, c1), 0x400 - dy) +
                              mul(mul(0x400 - dx, c5) + mul(dx, c3), dy);
                const int scaled = mul(mul(b, at<int>(0x004c2e00)), at<int>(0x004c2844));
                dz = divTrunc(divTrunc(-divTrunc(scaled, 1024), 1024), 4);
            }
            *sy += dz;
        }
    }
    if (at<int>(0x005a9cc0) != 0) {
        *sx = *sx * 2 - 400;
        *sy = *sy * 2 - 300;
    }
    if (*sx < nm || *sx >= at<int>(0x00822c8c) + m) return 0;
    if (*sy < -half || *sy >= half + at<int>(0x00822c90)) return 0;
    return 1;
}

// Callees of screenToTile: tileToScreen (0x0042f940), worldToScreen (0x0042fb90) and distance (0x0040acd0), all C3.
typedef int(__cdecl* TileToScreen_t)(int, int, int*, int*);
typedef int(__cdecl* Distance_t)(int, int);
const TileToScreen_t kTileToScreen = reinterpret_cast<TileToScreen_t>(0x0042f940);
const WorldToScreen_t kWorldToScreen = reinterpret_cast<WorldToScreen_t>(0x0042fb90);
const Distance_t kDistance = reinterpret_cast<Distance_t>(0x0040acd0);

// 0x00430020  screenToTile(px, py, tx, ty, useWorld): the tile under screen point (px, py), written to *tx / *ty.
//  1. When the flag at 0x005a9cc0 is non-zero, px = (px - 400)/2 + 400 and py = (py - 300)/2 + 300 (0x0043002e..
//     0x00430061; the adjusted values are also what step 4 measures against).
//  2. cx = px - W/2 - 0x40, cy = py - H/2 (W = 0x00822c8c, H = 0x00822c90); by rotation r (0x005685f4): r == 2
//     cx += 0x20, cy += 0x14; r == 4 cx += 0x40; r == 6 cx += 0x20, cy -= 0x14 (0x00430094..0x004300b5). Then
//     s = (cy << 5) / 40 (0x004300b8..0x004300cc), truncating.
//  3. A first guess from the camera tile (0x004c2ba0 / 0x004c2ba4) and the divisor q = 0x004c2840, all quotients
//     truncating: r == 0 (0x004300d4..0x0043011d): *tx = ((2camX+1)q - 2s + cx)/q/2, *ty = (q + 2(camY*q + s) + cx)
//     /q/2; r == 2 (0x00430134..0x00430179): *tx = ((2camX+1)q - 2s - cx)/q/2, *ty = ((2camY+1)q - 2s + cx)/q/2;
//     r == 4 (0x00430186..0x004301c8): *tx = (2(q*camX + s) - cx + q)/q/2, *ty = ((2camY+1)q - 2s - cx)/q/2;
//     r == 6 (0x004301d5..0x00430214): *tx = (q + 2(q*camX + s) + cx)/q/2, *ty = (2(camY*q + s) - cx + q)/q/2.
//  4. Hill climb (0x00430220..0x00430343): from (t, u) = (*tx, *ty) project the tile to the screen, with
//     tileToScreen(t, u) when useWorld == 0, else worldToScreen(t << 10, (u+1) << 10, ..., 0), into a screen
//     point (a, b) kept across projections; best = distance(px - a, 2(py - b)). For the 8 neighbours k = 0..7
//     (offsets dx at 0x004c2878, dy at 0x004c2898) the same projection and distance are taken, and a strictly smaller
//     distance stores the neighbour into *tx / *ty and becomes best. The climb repeats until a pass leaves *tx / *ty
//     unchanged.
typedef void(__cdecl* ScreenToTile_t)(int, int, int*, int*, int);
ScreenToTile_t ScreenToTile_orig;
void __cdecl ScreenToTile_re(int px, int py, int* tx, int* ty, int useWorld) {
    if (at<int>(0x005a9cc0) != 0) {
        px = (px - 400) / 2 + 400;
        py = (py - 300) / 2 + 300;
    }
    int cx = px - at<int>(0x00822c8c) / 2 - 0x40;
    int cy = py - at<int>(0x00822c90) / 2;
    const int rot = at<int>(0x005685f4);
    if (rot == 2) {
        cx += 0x20;
        cy += 0x14;
    } else if (rot == 4) {
        cx += 0x40;
    } else if (rot == 6) {
        cx += 0x20;
        cy -= 0x14;
    }
    const int s = (cy << 5) / 40;
    const int camX = at<int>(0x004c2ba0), camY = at<int>(0x004c2ba4);
    if (at<int>(0x005685f4) == 0) {
        const int q = at<int>(0x004c2840);
        *tx = (mul(camX * 2 + 1, q) - s * 2 + cx) / q / 2;
        *ty = (q + (mul(camY, q) + s) * 2 + cx) / q / 2;
    }
    if (at<int>(0x005685f4) == 2) {
        const int q = at<int>(0x004c2840);
        *tx = (mul(camX * 2 + 1, q) - s * 2 - cx) / q / 2;
        *ty = (mul(camY * 2 + 1, q) - s * 2 + cx) / q / 2;
    }
    if (at<int>(0x005685f4) == 4) {
        const int q = at<int>(0x004c2840);
        *tx = ((mul(q, camX) + s) * 2 - cx + q) / q / 2;
        *ty = (mul(camY * 2 + 1, q) - s * 2 - cx) / q / 2;
    }
    if (at<int>(0x005685f4) == 6) {
        const int q = at<int>(0x004c2840);
        *tx = (q + (mul(q, camX) + s) * 2 + cx) / q / 2;
        *ty = ((mul(camY, q) + s) * 2 - cx + q) / q / 2;
    }
    int a, b;  // screen point of the last projection; carried across projections like the original's locals
    for (;;) {
        const int t = *tx, u = *ty;
        if (useWorld == 0) kTileToScreen(t, u, &a, &b);
        else kWorldToScreen(t << 10, (u + 1) << 10, &a, &b, 0);
        int best = kDistance(px - a, (py - b) * 2);
        for (int k = 0; k < 8; k++) {
            const int nx = t + at<int>(0x004c2878 + k * 4);
            const int ny = u + at<int>(0x004c2898 + k * 4);
            if (useWorld == 0) kTileToScreen(nx, ny, &a, &b);
            else kWorldToScreen(nx << 10, (ny + 1) << 10, &a, &b, 0);
            const int d = kDistance(px - a, (py - b) * 2);
            if (d < best) {
                best = d;
                *tx = nx;
                *ty = ny;
            }
        }
        if (*tx == t && *ty == u) return;
    }
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00482e10, Link482_data, LinkData_re, LinkData_orig);
SG_HOOK("golf_clean.exe", 0x004837f0, C4837f0_ctor, C4837f0Ctor_re, C4837f0Ctor_orig);
SG_HOOK("golf_clean.exe", 0x00483060, Cache483060_detach, CacheDetach_re, CacheDetach_orig);
SG_HOOK("golf_clean.exe", 0x004796a0, Surface_pixelPtr, PixelPtr_re, PixelPtr_orig);
SG_HOOK("golf_clean.exe", 0x004826f0, Palette_decodeChunk, DecodeChunk_re, DecodeChunk_orig);
SG_HOOK("golf_clean.exe", 0x0042fb90, worldToScreen, WorldToScreen_re, WorldToScreen_orig);
SG_HOOK("golf_clean.exe", 0x00430020, screenToTile, ScreenToTile_re, ScreenToTile_orig);
