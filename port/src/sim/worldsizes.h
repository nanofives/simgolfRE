// P38W (Mariano 2026-11-20, log/roadmap/P38/P38W_notes.md): the world map offers courses of several sizes.
// Play switch courseActionMenu.worldSizes (1 = the original map, every course 50 x 50; play profile 0). At 0 every record of the
// course table carries its world size in byte +0x29, a byte no code of the original addresses (the whole-image scan, P38W_notes.md
// section 1): seedCourseTable writes it (P38W2: records 0..2 Standard 50, 3..11 Large 101, 12..15 Huge 152, from their acres; the reseed button redraws
// them with the records), the world map shows it (a size badge on the site's icon, acres scaled by (N / 50)^2 and price by ((N - 2) / 48)^2),
// the new course of the main menu and switchToCourse's next course of a career start at it. 0 in the byte = 50 (every record the
// original or an older save holds). The app's --world-size (Ctx::world_request) still overrides it (tests, development).
#pragma once

#include <cstdint>

#include "portsim/registry.h"
#include "portsim/sim/world.h"

namespace portsim {

extern int g_defect_world_sizes;   // courseActionMenu.worldSizes (1 = the original)
constexpr uint32_t kRecordWorldSize = 0x29;   // CourseRecord +0x29 (unk_29[0])

inline bool world_sizes_on() { return !SG_DEFECT(g_defect_world_sizes); }

// ---- P38W2 (Mariano 2026-11-20: "la cantidad de acres tiene que ser mas equilibrada"; log/roadmap/P38/P38W2_notes.md)
// The acres of record c (0..15, the price tiers' order): an even progression from 40 to 600, rounded to 10
// (40 80 110 150 190 230 260 300 340 380 410 450 490 530 560 600). The original's setting modifier (+-10 / +-20) is not kept: a +-20
// step against the ~37-acre progression would put a record below the one before it.
constexpr uint32_t kRecordAcres10 = 0x2A;   // CourseRecord +0x2a (unk_29[1], never addressed either): the acres / 10 (0 = the byte +0x1d)
inline int32_t world_record_acres(int32_t c) {
  if (c < 0) c = 0;
  if (c > 15) c = 15;
  return (((600 + 560 * c) * 2 + 150) / 300) * 10;   // 40 + 560 c / 15 to the nearest 10
}
// the rows a side cleared around the owned square for `acres` on an N x N world: the original's section M loop (c from N / 2 down
// while c * c * 4 > acres * 10) with its radius at N / 2 (0x19 at 50: the original's own count), so the square's area is the acres x 10
// to within its side's rounding
inline int32_t world_owned_border_at(int32_t n, int32_t acres) {
  int32_t c = n / 2, count = 0;
  do {
    ++count;
    --c;
  } while (c > 0 && c * c * 4 > acres * 10);
  return count;
}
// the coastal setting (1) keeps its waterfront: no band on the coast's side, so the owned land is (N - 2 k) rows by N - k columns;
// k so that this rectangle (its shore water included, the property's) is nearest the acres x 10
inline int32_t world_owned_border_coast(int32_t n, int32_t acres) {
  int32_t best = 1, err = -1;
  for (int32_t k = 1; k < n / 2; ++k) {
    const int32_t a = (n - 2 * k) * (n - k), e = a > acres * 10 ? a - acres * 10 : acres * 10 - a;
    if (err < 0 || e < err) best = k, err = e;
  }
  return best;
}
// the island setting (2): generateRandomCourse's sweep clears the sea of row / column v up to clamp(|v - N / 2|, 8 N / 50, N) +
// (count - 12 N / 50) + a jitter; `count` so that the island this leaves (the jitter at its mean) is nearest the acres x 10
inline int32_t world_owned_border_island(int32_t n, int32_t acres) {
  const int32_t l8 = (8 * n + 25) / 50, l12 = (12 * n + 25) / 50, l3 = (3 * n + 25) / 50;
  const int32_t jit = n == kWorldOriginal ? 1 : (l3 - 1) / 2;
  int32_t best = 1, err = -1;
  for (int32_t count = 1; count < n / 2 + l12; ++count) {
    const int32_t rest = count - l12 + jit;
    const auto lim = [&](int32_t v) {
      const int32_t d = v > n / 2 ? v - n / 2 : n / 2 - v;
      return (d < l8 ? l8 : d > n ? n : d) + rest;
    };
    int32_t area = 0;
    for (int32_t r = 0; r < n; ++r) {
      const int32_t lr = lim(r);
      for (int32_t c = lr; c <= n - 1 - lr; ++c) {
        const int32_t lc = lim(c);
        if (r >= lc && r <= n - 1 - lc) ++area;
      }
    }
    const int32_t e = area > acres * 10 ? area - acres * 10 : acres * 10 - area;
    if (err < 0 || e < err) best = count, err = e;
  }
  return best;
}
// P38C's rule (50's share of the side): the 50 x 50 count scaled by N / 50 (the sandbox and any record without its own acres)
inline int32_t world_owned_border_share(int32_t n, int32_t acres) {
  return (world_owned_border_at(kWorldOriginal, acres) * n + kWorldOriginal / 2) / kWorldOriginal;
}
// the record's world size from its acres: the smallest of 50 / 101 / 152 whose owned square leaves at least half a lot ((N - 2) / 6
// rows) for sale on every side (40..110 acres: 50, 150..450: 101, 490..600: 152)
inline int32_t world_size_for_acres(int32_t acres) {
  for (const int32_t n : {kWorldOriginal, 101, 152})
    if (2 * world_owned_border_at(n, acres) >= (n - 2) / 3) return n;
  return 152;
}
// the price of `acres`: the cheapest record's $1,250 an acre (40 acres, $50,000), in units of $100, to the nearest $1,000
inline int32_t world_price_of_acres(int32_t acres) { return ((acres * 125 + 50) / 100) * 10; }
// the byte's code: 0 = 50, else N = 3 * b + 2 (33 = 101, 50 = 152); a code outside world_size_ok gives 50
inline int32_t world_n_of_code(int32_t b) {
  const int32_t n = 3 * b + 2;
  return b == 0 || !world_size_ok(n) ? kWorldOriginal : n;
}
inline uint8_t world_code_of_n(int32_t n) { return n == kWorldOriginal ? 0 : static_cast<uint8_t>((n - 2) / 3); }
// course record c's world size (50 while the switch is at 1)
int32_t record_world_n(Ctx &ctx, int32_t c);
// P38W2: course record c's acres (byte +0x2a x 10 when the switch is at 0 and it is set, else the original's byte +0x1d)
int32_t record_acres(Ctx &ctx, int32_t c);
// P38W2: the owned border the generator clears for record c on the current world: world_owned_border_at for a record with its own
// acres generated at its own size, else P38C's share rule (world_owned_border_share)
int32_t record_owned_border(Ctx &ctx, int32_t c);
// seedCourseTable's sizes and (P38W2) acres (records 0..15); nothing while the switch is at 1
void seed_record_world_sizes(Ctx &ctx);

}  // namespace portsim
