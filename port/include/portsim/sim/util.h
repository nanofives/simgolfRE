// Utility leaves of the simulation core: the RNG, clamps, distances, directions, the fixed-point sine (phase 2.6 first
// client, extended by phase 3.0). Specifications: the analysis notes re/analysis/util/<addr>_<name>.md and
// re/analysis/sim/<addr>_<name>.md of each function, and the vector corpus (port/corpus/vectors/<function>.vec).
// Arithmetic follows portsim/arith.h: every + - * shown below wraps modulo 2^32.
#ifndef PORTSIM_SIM_UTIL_H
#define PORTSIM_SIM_UTIL_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// Random::next: the 32-bit LCG step seed = seed * 1103515245 + 12345 (wrapping), then ((seed >> 16) & 0x7fff) / 32768,
// a double in [0, 1). The seed is the whole Random object (4 bytes).
double random_next(uint32_t &seed);

// Random::range: random_next(seed) * (n & 0xffff), truncated toward zero (the original's __ftol); a value in
// [0, n & 0xffff), or 0 when that is 0. The original returns eax (callers keep ax).
uint32_t random_range(uint32_t &seed, uint32_t n);

// clamp: v raised to lo, then lowered to hi only when v > hi and hi >= lo (hi < lo leaves the raised v).
int32_t clamp(int32_t v, int32_t lo, int32_t hi);

// distance: sqrt(dx*dx + dy*dy) of a fixed-point vector, truncated toward zero. A component with |c| > 0x4000 is divided
// by 8 (toward zero) and the result scaled by 8 per such component. The squares wrap in 32 bits like the original;
// a negative sum gives NaN, which the original's conversion turns into 0.
int32_t distance(int32_t dx, int32_t dy);

// sign: 1 for v > 0, 0 for v == 0, -1 for v < 0.
int32_t sign(int32_t v);

// approxDistance: octagonal length of (dx, dy): with a = |dx|, b = |dy| (INT_MIN stays negative),
// (b + 2a) / 2 when a > b, else (a + 2b) / 2 (division truncates toward zero).
int32_t approx_distance(int32_t dx, int32_t dy);

// direction8: octant 0..7 of the vector (a, b) from the sign quadrant and the 2:1 ratio tests (|a| > 2|b|, then
// |b| > 2|a|, doubled values wrapping): a>0 b>0 {3, 2, 4}, a>0 b<=0 {1, 2, 0}, a<=0 b>0 {5, 6, 4}, a<=0 b<=0 {7, 6, 0}
// as {default, |a| dominant, |b| dominant}.
int32_t direction8(int32_t a, int32_t b);

// directionOf: dominant axis of (dx, dy) as 0 / 2 / 4 / 6: |dx| > |dy| gives 2 (dx > 0) or 6, else 4 (dy > 0) or 0.
int32_t direction_of(int32_t dx, int32_t dy);

// angleFixed: atan2-style angle of (x, -y) in the high 16 bits (0x10000 = a full turn): axis cases first, then the
// ratio t = (min << 14) / max (idiv), the correction c = ((0x2800 - ((11 * |0x1333 - t|) << 8 >> 14)) * t) >> 14 and a
// per-quadrant base.
int32_t angle_fixed(int32_t x, int32_t y);

// sinScaled: amp * sine of a 32-bit angle (0x40000000 = a quarter turn) by linear interpolation in g_sinTable,
// with amp pre-shifted by 0 / 8 / 16 bits when it is >= 0xffff / >= 0xffffff (signed) and the product shifted back.
// Reads table entry index + 1 = 256 at the top of the quarter (defect model_defects#10, switch sinScaled.readPastTable).
int32_t sin_scaled(Ctx &ctx, int32_t angle, int32_t amp);
// foldRange: reflects the angle into the first half-quarter-turn range (sign bit -> amp negated; bit 30 ->
// 0x7fffffff - a) and returns sinScaled.
int32_t fold_range(Ctx &ctx, int32_t a, int32_t amp);
// absDiff (the hooks.csv name; it is the cosine): foldRange(a + 0x40000000, amp).
int32_t abs_diff(Ctx &ctx, int32_t a, int32_t amp);

// cosScaled: sinScaled(a + 0x3fffffff, amp) (a quarter turn minus one).
int32_t cos_scaled(Ctx &ctx, int32_t a, int32_t amp);
// initSinTable (boot): g_sinTable[i] = __ftol(sin(i * step) * amp) for i = 0..255, with amp and step the two doubles of
// the exe constant f_k_dbl_4bba48 (ExeConstants; throws MissingResource without one). Entry 255 is the top of the
// quarter wave; the dword after the table is never written (model_defects#10). The CRT sin gives all 256 entries of the
// original's x87 fsin table (checked against the 190 segment start images).
void init_sin_table(Ctx &ctx);

// bucketValue: v <= 5 -> 0, <= 9 -> 1, <= 17 -> 2, == 18 -> 3, else -1 (signed).
int32_t bucket_value(int32_t v);

// decaySum: r += x / 8, x -= x >> s with s = g_tileTypeAttrs[1].u21 (signed byte, count mod 32), at least once and
// while x >= 0x40; returns r. When a step leaves x >= 0x40 unchanged the original never returns: throws NoReturn.
int32_t decay_sum(Ctx &ctx, int32_t x);
// decaySum2: r += x / 8, s += y / 16, y -= 0x80, x -= x >> 4, at least once and while s > 0; returns r. Throws
// NoReturn when the state (x, y, s) provably repeats (x fixed and s unchanged after a full 2^25-step period of y).
int32_t decay_sum2(int32_t x, int32_t y);

// tileDistance: distance from world point (x, y) to the centre of tile (tx, ty) (0x400 units per tile, centre +0x200),
// times 25 / 1024 (truncating).
int32_t tile_distance(int32_t x, int32_t y, int32_t tx, int32_t ty);

}  // namespace portsim

#endif  // PORTSIM_SIM_UTIL_H
