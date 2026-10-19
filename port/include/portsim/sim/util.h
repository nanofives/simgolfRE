// Utility leaves of the simulation core (phase 2.6 first client; phase 3.0 owns this module from here on).
// Specifications: re/analysis/util/0045c1a0_Random_next.md, 0045c1e0_Random_range.md, 00467130_clamp.md,
// 0040acd0_distance.md and the vector corpus (port/corpus/vectors/<function>.vec).
#ifndef PORTSIM_SIM_UTIL_H
#define PORTSIM_SIM_UTIL_H

#include <cstdint>

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

}  // namespace portsim

#endif  // PORTSIM_SIM_UTIL_H
