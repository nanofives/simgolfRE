// Small integer helpers of golf_clean.exe (all __cdecl, all leaves). Reimplemented from the C2 transcriptions
// re/analysis/<subsystem>/<addr>_<name>.md; each body states the facts it relies on with their addresses.
#include "hooks.h"

namespace {

typedef int(__cdecl* Int1_t)(int);
typedef int(__cdecl* Int2_t)(int, int);

// 0x00467170  ?approxDistance@@YAHHH@Z
// |dx|, |dy| (negated when < 0); returns (min + 2*max) / 2 with C (truncating) signed division.
Int2_t ApproxDistance_orig;
int __cdecl ApproxDistance_re(int dx, int dy) {
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx > dy ? (dy + dx * 2) / 2 : (dx + dy * 2) / 2;
}

// 0x0044faf0  bucketValue: thresholds 5, 9, 0x11, 0x12 (signed compares); above 0x12 returns -1.
Int1_t BucketValue_orig;
int __cdecl BucketValue_re(int v) {
    if (v <= 5) return 0;
    if (v <= 9) return 1;
    if (v <= 0x11) return 2;
    return v <= 0x12 ? 3 : -1;
}

// 0x004223c0  decaySum: sums x/8 while x decays by x >> shift, shift = the signed byte at 0x005783a1;
// the loop runs at least once and continues while x >= 0x40 (jge at 0x004223e5).
Int1_t DecaySum_orig;
int __cdecl DecaySum_re(int x) {
    const int shift = *reinterpret_cast<const signed char*>(0x005783a1);
    int r = 0;
    do {
        r += x / 8;
        x -= x >> shift;
    } while (x >= 0x40);
    return r;
}

// 0x004223f0  decaySum2: r += x/8 and s += y/16 per step, y -= 0x80, x -= x >> 4, while s > 0.
Int2_t DecaySum2_orig;
int __cdecl DecaySum2_re(int x, int y) {
    int r = 0, s = 0;
    do {
        r += x / 8;
        s += y / 16;
        y -= 0x80;
        x -= x >> 4;
    } while (s > 0);
    return r;
}

// 0x00404970  scaleX: (screen width at 0x00822c8c * v + 160) / 320, signed.
Int1_t ScaleX_orig;
int __cdecl ScaleX_re(int v) {
    const int w = *reinterpret_cast<const int*>(0x00822c8c);
    return (w * v + 160) / 320;
}

// 0x0046f1d0  tierPrice: jump table at 0x0046f270 indexed by tier when tier <= 15 unsigned (ja at
// 0x0046f1dc), otherwise 500; the result goes through v * 100 / 100.
Int1_t TierPrice_orig;
int __cdecl TierPrice_re(int tier) {
    static const int kPrice[16] = {500, 600, 700, 800, 1200, 1500, 2000, 2500,
                                   3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};
    const int v = static_cast<unsigned>(tier) <= 15 ? kPrice[tier] : 500;
    return v * 100 / 100;
}

// 0x0040bf60  tileBlocked: 1 when x or y is outside 0..49 (signed compares), or when the tile type byte
// at 0x005722e8 + x*50 + y equals 0x14; else 0.
Int2_t TileBlocked_orig;
int __cdecl TileBlocked_re(int x, int y) {
    if (x < 0 || x >= 50 || y < 0 || y >= 50) return 1;
    const signed char* types = reinterpret_cast<const signed char*>(0x005722e8);
    return types[x * 50 + y] == 0x14 ? 1 : 0;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x00467170, approxDistance, ApproxDistance_re, ApproxDistance_orig);
SG_HOOK("golf_clean.exe", 0x0044faf0, bucketValue, BucketValue_re, BucketValue_orig);
SG_HOOK("golf_clean.exe", 0x004223c0, decaySum, DecaySum_re, DecaySum_orig);
SG_HOOK("golf_clean.exe", 0x004223f0, decaySum2, DecaySum2_re, DecaySum2_orig);
SG_HOOK("golf_clean.exe", 0x00404970, scaleX, ScaleX_re, ScaleX_orig);
SG_HOOK("golf_clean.exe", 0x0046f1d0, tierPrice, TierPrice_re, TierPrice_orig);
SG_HOOK("golf_clean.exe", 0x0040bf60, tileBlocked, TileBlocked_re, TileBlocked_orig);
