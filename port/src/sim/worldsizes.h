// P38W (Mariano 2026-11-20, log/roadmap/P38/P38W_notes.md): the world map offers courses of several sizes.
// Play switch courseActionMenu.worldSizes (1 = the original map, every course 50 x 50; play profile 0). At 0 every record of the
// course table carries its world size in byte +0x29, a byte no code of the original addresses (the whole-image scan, P38W_notes.md
// section 1): seedCourseTable writes it (records 0..7 Standard 50, 8..11 Large 101, 12..15 Huge 152; the reseed button redraws
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
// the byte's code: 0 = 50, else N = 3 * b + 2 (33 = 101, 50 = 152); a code outside world_size_ok gives 50
inline int32_t world_n_of_code(int32_t b) {
  const int32_t n = 3 * b + 2;
  return b == 0 || !world_size_ok(n) ? kWorldOriginal : n;
}
inline uint8_t world_code_of_n(int32_t n) { return n == kWorldOriginal ? 0 : static_cast<uint8_t>((n - 2) / 3); }
// course record c's world size (50 while the switch is at 1)
int32_t record_world_n(Ctx &ctx, int32_t c);
// seedCourseTable's sizes (records 0..15); nothing while the switch is at 1
void seed_record_world_sizes(Ctx &ctx);

}  // namespace portsim
