// C3 batch c3a (2026-10-06) of golf_clean.exe: terrain/golfer/sim/course table readers and writers, all __cdecl.
// Hand-written from the disassembly (py -3.12 re/tools/asm2inline.py golf_clean.exe 0x<addr> --list) and the
// matched sources in re/match/*.cpp; each body cites the address of every global and callee it relies on.
// Callees are invoked through their original addresses (a hooked callee runs its own reimplementation, which the
// path-1 A/B drives for both arms, so the comparison isolates this function's body).
#include "hooks.h"

namespace {

template <typename T> T at(unsigned addr) { return *reinterpret_cast<const T*>(addr); }

typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Int2_t)(int, int);
typedef int(__cdecl* Int3_t)(int, int, int);
typedef int(__cdecl* Int4_t)(int, int, int, int);
typedef void(__cdecl* VoidRange_t)(int, int, int*, int*);

// Callees (originals), invoked at their VAs.
const Int2_t kTileBlocked   = reinterpret_cast<Int2_t>(0x0040BF60);   // tileBlocked
const Int2_t kHeightBlend   = reinterpret_cast<Int2_t>(0x0040C170);   // heightBlend(x, y) (ignores a 3rd arg)
const Int2_t kBilinear      = reinterpret_cast<Int2_t>(0x004674C0);   // bilinearSample
const Int3_t kClamp3        = reinterpret_cast<Int3_t>(0x00467130);   // clamp(v, lo, hi)
const Int4_t kCornerHeights = reinterpret_cast<Int4_t>(0x0040BFE0);   // cornerHeights(a, b, c, d)
const VoidRange_t kCornerRange = reinterpret_cast<VoidRange_t>(0x0042F4B0);  // cornerRange
const Int2_t kDistance      = reinterpret_cast<Int2_t>(0x0040ACD0);   // distance
const Int2_t kShotReach     = reinterpret_cast<Int2_t>(0x004223F0);   // decaySum2 / shotReach
const Int1_t kPuttReach     = reinterpret_cast<Int1_t>(0x004223C0);   // decaySum / puttReach

// Neighbour offsets: 8 dwords each (the terrain walks use indices 0, 2, 4, 6).
const int* const kDirX = reinterpret_cast<const int*>(0x004C2878);
const int* const kDirY = reinterpret_cast<const int*>(0x004C2898);

// 0x004492f0  tileByte(x, y): the per-cell byte at 0x0056988c (row stride 50), zero-extended (0x00449300).
Int2_t TileByte_orig;
int __cdecl TileByte_re(int x, int y) {
    return at<unsigned char>(0x0056988C + x * 50 + y);
}

// 0x004493b0  tileFlag20(x, y): bit 0x20 of the flags word at 0x0053caf0 (cell (x*50 + y) * 2); the test is on the
// low byte (0x004493c1 / 0x004493c8), so the result is the word's 0x20 bit (0 or 0x20).
Int2_t TileFlag20_orig;
int __cdecl TileFlag20_re(int x, int y) {
    return at<unsigned char>(0x0053CAF0 + (x * 50 + y) * 2) & 0x20;
}

// 0x004675d0  thoughtFlag(g): the first thought word at 0x00579540 + g*0x100. Bit 0x8000 set -> 2 (0x004675e3);
// else bit 0x4000 clear -> 1, set -> 0 (not/shr 0xe/and 1 at 0x004675e9).
Int1_t ThoughtFlag_orig;
int __cdecl ThoughtFlag_re(int g) {
    unsigned short f = at<unsigned short>(0x00579540 + (g << 8));
    if (f & 0x8000) return 2;
    return (f & 0x4000) ? 0 : 1;
}

// 0x0045c420  thoughtBalance(g): over the 5 thought words at 0x00579540 + g*0x100 (+0, +2, +4, +6, +8), +1 when the
// top two bits are 0x4000, -1 when 0xc000 (0x0045c438..0x0045c44c).
Int1_t ThoughtBalance_orig;
int __cdecl ThoughtBalance_re(int g) {
    const unsigned base = 0x00579540 + (g << 8);
    int n = 0;
    for (int i = 0; i < 5; i++) {
        unsigned short w = at<unsigned short>(base + i * 2);
        if ((w & 0xc000) == 0x4000) n++;
        if ((w & 0xc000) == 0xc000) n--;
    }
    return n;
}

// 0x0040bfa0  typeAtPos(px, py): tile (px >> 10, py >> 10); 0x14 when tileBlocked (0x0040bf60), else the signed tile
// type at 0x005722e8 (row stride 50, 0x0040bfcc).
Int2_t TypeAtPos_orig;
int __cdecl TypeAtPos_re(int px, int py) {
    int x = px >> 10, y = py >> 10;
    if (kTileBlocked(x, y)) return 0x14;
    return at<signed char>(0x005722E8 + x * 50 + y);
}

// 0x00449310  tileQuery449310(a, b, c): cornerHeights(a, b, c, 1) (0x0040bfe0), the 4th argument fixed to 1
// (push 1 at 0x0044931c).
Int3_t TileQuery449310_orig;
int __cdecl TileQuery449310_re(int a, int b, int c) {
    return kCornerHeights(a, b, c, 1);
}

// 0x0042dba0  sampleHeight(x, y): with x, y >> 2, a = bilinearSample(x, y) * 6 and b = bilinearSample(x*2, y*2)
// (0x004674c0); clamp((a + b*4) * 7 / 64 / 2, 0, 0x200) (0x00467130). All divisions truncate toward zero.
Int2_t SampleHeight_orig;
int __cdecl SampleHeight_re(int x, int y) {
    x >>= 2;
    y >>= 2;
    int a = kBilinear(x, y) * 6;
    int b = kBilinear(x * 2, y * 2);
    return kClamp3((a + b * 4) * 7 / 64 / 2, 0, 0x200);
}

// 0x0042f4b0  cornerRange(x, y, pMax, pMin): seeds both to heightBlend(x, y) (0x0040c170), then widens the range over
// heightBlend at (x+1, y), (x+1, y-1), (x, y-1). pMax (esi, 0x0042f4e6) keeps the max, pMin (ecx, 0x0042f4f0) the min.
VoidRange_t CornerRange_orig;
void __cdecl CornerRange_re(int x, int y, int* pMax, int* pMin) {
    int v = kHeightBlend(x, y);
    *pMax = v;
    *pMin = v;
    const int xs[3] = { x + 1, x + 1, x };
    const int ys[3] = { y, y - 1, y - 1 };
    for (int i = 0; i < 3; i++) {
        int w = kHeightBlend(xs[i], ys[i]);
        if (w > *pMax) *pMax = w;
        if (w < *pMin) *pMin = w;
    }
}

// 0x00402930  freeAtTile(tx, ty): over the 256 records at 0x005736b0 (stride 0x24; x at +0, y at +4, id at +8),
// sets id (+8) to -1 wherever a live record's (x >> 10, y >> 10) equals (tx, ty) (0x00402943..0x00402957).
typedef void(__cdecl* Void2_t)(int, int);
Void2_t FreeAtTile_orig;
void __cdecl FreeAtTile_re(int tx, int ty) {
    for (unsigned id = 0x005736B8; id < 0x00575AB8; id += 0x24) {
        if (at<int>(id) == -1) continue;
        if ((at<int>(id - 8) >> 10) == tx && (at<int>(id - 4) >> 10) == ty)
            *reinterpret_cast<int*>(id) = -1;
    }
}

// 0x0042f6e0  raiseFromNeighbours(x, y, sameByte): over the 4 orthogonal neighbours (kDirX/kDirY indices 0,2,4,6),
// raises the signed level at 0x00543018 (cell x*50+y) to the greatest neighbour level that shares this cell's tile
// type (0x005722e8) and, when sameByte != 0, its tile byte (0x0056988c); skips blocked neighbours (0x0040bf60).
// Returns 1 if any raise happened (0x0042f779).
Int3_t RaiseFromNeighbours_orig;
int __cdecl RaiseFromNeighbours_re(int x, int y, int sameByte) {
    const unsigned cell = x * 50 + y;
    int best = at<signed char>(0x00543018 + cell);
    int changed = 0;
    for (int k = 0; k < 8; k += 2) {
        int nx = x + kDirX[k], ny = y + kDirY[k];
        if (kTileBlocked(nx, ny)) continue;
        const unsigned ncell = nx * 50 + ny;
        if (at<signed char>(0x005722E8 + ncell) != at<signed char>(0x005722E8 + cell)) continue;
        if (sameByte && at<unsigned char>(0x0056988C + ncell) != at<unsigned char>(0x0056988C + cell)) continue;
        int nl = at<signed char>(0x00543018 + ncell);
        if (nl > best) {
            best = nl;
            *reinterpret_cast<signed char*>(0x00543018 + cell) = static_cast<signed char>(best);
            changed = 1;
        }
    }
    return changed;
}

// 0x0042f530  relaxEdges42f530(x, y): clears the wall-mask byte at 0x005619a0 (cell x*50+y, 0x0042f568) after a
// cornerRange call (0x0042f4b0) whose outputs are discarded, then for each orthogonal neighbour (kDirX/kDirY indices
// 0,2,4,6) that is not blocked (0x0040bf60) sets bit k when cornerHeights(x, y, k-1, 0) < cornerHeights(nx, ny, k+5, 0)
// or cornerHeights(x, y, k+1, 0) < cornerHeights(nx, ny, k+3, 0) (all 0x0040bfe0).
Void2_t RelaxEdges_orig;
void __cdecl RelaxEdges_re(int x, int y) {
    int mx, mn;
    kCornerRange(x, y, &mx, &mn);
    unsigned char* p = reinterpret_cast<unsigned char*>(0x005619A0 + x * 50 + y);
    *p = 0;
    for (int k = 0; k < 8; k += 2) {
        int nx = x + kDirX[k], ny = y + kDirY[k];
        if (kTileBlocked(nx, ny)) continue;
        if (kCornerHeights(x, y, k - 1, 0) < kCornerHeights(nx, ny, k + 5, 0)) *p |= static_cast<unsigned char>(1 << k);
        if (kCornerHeights(x, y, k + 1, 0) < kCornerHeights(nx, ny, k + 3, 0)) *p |= static_cast<unsigned char>(1 << k);
    }
}

// 0x0040ddb0  nearestPlaced(type, x, y): resets the distance scratch at 0x00568d0c to 0xffff (0x0040ddcb), then over
// the 256 placed-object records at 0x0058bcb8 (stride 0x10; type word +0, tx word +2, ty word +4, flags byte +7),
// for each record of this type returns early once a type>=6 record lacks flag 0x40 (0x0040ddef); otherwise keeps the
// index of the nearest by distance (0x0040acd0) between the two tile centres (tile + objDef-size/2 at 0x004c26c0
// stride 20), each scaled << 10 with a +0x200 bias. Returns the best index or -1.
Int3_t NearestPlaced_orig;
int __cdecl NearestPlaced_re(int type, int x, int y) {
    int best = -1;
    *reinterpret_cast<int*>(0x00568D0C) = 0xFFFF;
    for (int i = 0; i < 256; i++) {
        const unsigned r = 0x0058BCB8 + i * 0x10;
        if (at<short>(r) != type) continue;
        if (type >= 6 && !(at<unsigned char>(r + 7) & 0x40)) return best;
        int h = at<signed char>(0x004C26C0 + type * 20) / 2;
        int d = kDistance((at<short>(r + 2) + h) * 0x400 - x + 0x200,
                          (at<short>(r + 4) + h) * 0x400 - y + 0x200);
        if (d < at<int>(0x00568D0C)) {
            best = i;
            *reinterpret_cast<int*>(0x00568D0C) = d;
        }
    }
    return best;
}

// 0x00422430  shotPower(dist, lie, putt): a 10-slot cache (dist 0x005a47b8, lie 0x005685c8, value 0x0053fd20, write
// index 0x005a9ce0). On a (dist, lie) hit returns the cached value (0x004224d9); otherwise stores the key, bisects an
// initial guess v = t*33 - t*t/48 + 64 (t = dist*20/25) toward target = dist*1024/25 using shotReach(v, lie)
// (0x004223f0) or, when putt, puttReach(v) (0x004223c0), halving the step until it is <= 2, then caches and returns v.
Int3_t ShotPower_orig;
int __cdecl ShotPower_re(int dist, int lie, int putt) {
    int* cacheDist = reinterpret_cast<int*>(0x005A47B8);
    int* cacheLie  = reinterpret_cast<int*>(0x005685C8);
    int* cacheVal  = reinterpret_cast<int*>(0x0053FD20);
    for (int i = 0; i < 10; i++)
        if (cacheDist[i] == dist && cacheLie[i] == lie) return cacheVal[i];
    int& next = *reinterpret_cast<int*>(0x005A9CE0);
    cacheDist[next] = dist;
    cacheLie[next] = lie;
    int t = dist * 20 / 25;
    int v = t * 33 - t * t / 48 + 64;
    int step = v / 2;
    int target = (dist << 10) / 25;
    do {
        int r = putt ? kPuttReach(v) : kShotReach(v, lie);
        if (r > target) v -= step;
        if (r < target) v += step;
        step /= 2;
    } while (step > 2);
    cacheVal[next] = v;
    next = (next + 1) % 10;
    return v;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x004492F0, tileByte, TileByte_re, TileByte_orig);
SG_HOOK("golf_clean.exe", 0x004493B0, tileFlag20, TileFlag20_re, TileFlag20_orig);
SG_HOOK("golf_clean.exe", 0x004675D0, thoughtFlag, ThoughtFlag_re, ThoughtFlag_orig);
SG_HOOK("golf_clean.exe", 0x0045C420, thoughtBalance, ThoughtBalance_re, ThoughtBalance_orig);
SG_HOOK("golf_clean.exe", 0x0040BFA0, typeAtPos, TypeAtPos_re, TypeAtPos_orig);
SG_HOOK("golf_clean.exe", 0x00449310, tileQuery449310, TileQuery449310_re, TileQuery449310_orig);
SG_HOOK("golf_clean.exe", 0x0042DBA0, sampleHeight, SampleHeight_re, SampleHeight_orig);
SG_HOOK("golf_clean.exe", 0x0042F4B0, cornerRange, CornerRange_re, CornerRange_orig);
SG_HOOK("golf_clean.exe", 0x00402930, freeAtTile, FreeAtTile_re, FreeAtTile_orig);
SG_HOOK("golf_clean.exe", 0x0042F6E0, raiseFromNeighbours, RaiseFromNeighbours_re, RaiseFromNeighbours_orig);
SG_HOOK("golf_clean.exe", 0x0042F530, relaxEdges42f530, RelaxEdges_re, RelaxEdges_orig);
SG_HOOK("golf_clean.exe", 0x0040DDB0, nearestPlaced, NearestPlaced_re, NearestPlaced_orig);
SG_HOOK("golf_clean.exe", 0x00422430, shotPower, ShotPower_re, ShotPower_orig);
