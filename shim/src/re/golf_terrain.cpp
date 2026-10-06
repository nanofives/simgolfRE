// Terrain height and slope readers of golf_clean.exe (world coordinates: tile = coordinate >> 10). They call the
// original corner helpers (cornerHeights 0x0040bfe0, cornerRange 0x0042f4b0, clamp 0x00467130) through their
// addresses, so a hooked callee runs its own reimplementation. From re/analysis/terrain/<addr>_<name>.md.
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }

typedef int(__cdecl* Corner_t)(int x, int y, int corner, int flag);
typedef void(__cdecl* CornerRange_t)(int x, int y, int* mx, int* mn);
typedef int(__cdecl* Clamp_t)(int v, int lo, int hi);
typedef int(__cdecl* Xy_t)(int, int);
typedef int(__cdecl* Mix_t)(int, int, unsigned);

const Corner_t kCorner = reinterpret_cast<Corner_t>(0x0040bfe0);
const CornerRange_t kCornerRange = reinterpret_cast<CornerRange_t>(0x0042f4b0);
const Clamp_t kClamp = reinterpret_cast<Clamp_t>(0x00467130);

signed char tileType(int tx, int ty) { return at<signed char>(0x005722e8 + tx * 50 + ty); }

// Shared head of slopeX / slopeY: 0 when bit 0 of the byte at 0x0059e7b8 is set, when the tile type is 7 or 9, or
// when the type's flags byte (0x0057837c + type*0x30) has any of bits 0xe; otherwise the four corner heights 5, 7,
// 1, 3 of the tile (cornerHeights(tx, ty, corner, 1)).
bool slopeCorners(int tx, int ty, int c[4]) {
    if (at<unsigned char>(0x0059e7b8) & 1) return false;
    const signed char t = tileType(tx, ty);
    if (t == 7 || t == 9) return false;
    if (at<unsigned char>(0x0057837c + t * 0x30) & 0xe) return false;
    c[0] = kCorner(tx, ty, 5, 1);
    c[1] = kCorner(tx, ty, 7, 1);
    c[2] = kCorner(tx, ty, 1, 1);
    c[3] = kCorner(tx, ty, 3, 1);
    return true;
}

// 0x0040c3a0  slopeX(x, y): corner 3 - corner 1 in the right half of the tile (x > (tx << 10) + 0x200), else
// corner 5 - corner 7.
Xy_t SlopeX_orig;
int __cdecl SlopeX_re(int x, int y) {
    const int tx = x >> 10, ty = y >> 10;
    int c[4];
    if (!slopeCorners(tx, ty, c)) return 0;
    return x > (tx << 10) + 0x200 ? c[3] - c[2] : c[0] - c[1];
}

// 0x0040c2f0  slopeY(x, y): corner 3 - corner 5 in the lower half (y > (ty << 10) + 0x200), else corner 1 - corner 7.
Xy_t SlopeY_orig;
int __cdecl SlopeY_re(int x, int y) {
    const int tx = x >> 10, ty = y >> 10;
    int c[4];
    if (!slopeCorners(tx, ty, c)) return 0;
    return y > (ty << 10) + 0x200 ? c[3] - c[0] : c[2] - c[1];
}

// 0x0040c450  slopeMix(x, y, dir): p = slopeY(x, y) (0x0040c2f0), q = slopeX(x, y) (0x0040c3a0), both clamped to
// -1..1 when dir is odd; returns dy[dir] * q + dx[dir] * p with the direction tables at 0x004c2898 / 0x004c2878.
Mix_t SlopeMix_orig;
int __cdecl SlopeMix_re(int x, int y, unsigned dir) {
    int p = reinterpret_cast<Xy_t>(0x0040c2f0)(x, y);
    int q = reinterpret_cast<Xy_t>(0x0040c3a0)(x, y);
    if (dir & 1) {
        p = kClamp(p, -1, 1);
        q = kClamp(q, -1, 1);
    }
    return at<int>(0x004c2898 + dir * 4) * q + at<int>(0x004c2878 + dir * 4) * p;
}

// 0x0042fa30  heightAt(x, y): with f = the flags dword of the tile's type (0x0057837c + type*0x30): 0 when f & 8;
// (min corner - 3) * 16 when f & 2 and (max corner - 3) * 16 when f & 4 (cornerRange 0x0042f4b0); otherwise the
// corners 5, 7, 1, 3 minus 3 each: their common value * 16 when all four are equal, else the bilinear blend
// (((0x400-fx)*h7 + fx*h1)*(0x400-fy) + ((0x400-fx)*h5 + fx*h3)*fy) * 16 / 1024 / 1024 of the in-tile offsets.
Xy_t HeightAt_orig;
int __cdecl HeightAt_re(int x, int y) {
    const int tx = x >> 10, ty = y >> 10;
    const unsigned f = at<unsigned>(0x0057837c + tileType(tx, ty) * 0x30);
    if (f & 8) return 0;
    if (f & 6) {
        int mx, mn;
        kCornerRange(tx, ty, &mx, &mn);
        return f & 2 ? (mn - 3) * 16 : (mx - 3) * 16;
    }
    const int h5 = kCorner(tx, ty, 5, 1) - 3, h7 = kCorner(tx, ty, 7, 1) - 3;
    const int h1 = kCorner(tx, ty, 1, 1) - 3, h3 = kCorner(tx, ty, 3, 1) - 3;
    if (h5 == h7 && h5 == h1 && h5 == h3) return h5 * 16;
    const int fx = x - (tx << 10), fy = y - (ty << 10);
    return (((0x400 - fx) * h7 + fx * h1) * (0x400 - fy) + ((0x400 - fx) * h5 + fx * h3) * fy) * 16 / 1024 / 1024;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0040c3a0, slopeX, SlopeX_re, SlopeX_orig);
SG_HOOK("golf_clean.exe", 0x0040c2f0, slopeY, SlopeY_re, SlopeY_orig);
SG_HOOK("golf_clean.exe", 0x0040c450, slopeMix, SlopeMix_re, SlopeMix_orig);
SG_HOOK("golf_clean.exe", 0x0042fa30, heightAt42fa30, HeightAt_re, HeightAt_orig);
