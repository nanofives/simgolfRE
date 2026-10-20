// showHoleStatsScreen (phase 3.8, stream 38e, U-0949): the per-hole statistics screen mainLoop opens for a negative
// g_msgState53df54. Module `holestats` (port/src/sim/holestats.cpp), tests port/test/test_holestats.py, notes
// log/p38/38e_notes.md.
#ifndef PORTSIM_SIM_HOLESTATS_H
#define PORTSIM_SIM_HOLESTATS_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// The original's clock call sites in showHoleStatsScreen, as InputSource::clock keys them (the call's return address minus
// mainLoop's frame-head return address): the first page (entry), the banner blink (a par byte of 0), the page test after a
// redraw, the page test of every input pass, and the page read that starts a redraw.
enum HoleStatsClockSite : int32_t {
  kHsClockEntry = 0x43739,
  kHsClockBanner = 0x4460a,
  kHsClockAfterDraw = 0x44950,
  kHsClockPass = 0x44a1d,
  kHsClockNewPage = 0x44a32,
};

// showHoleStatsScreen(hole): the screen's writes without its drawing (g_textBuffer and the itoa scratch as the original
// leaves them, the peers appendHoleName / appendCents / appendRatingLabel / buildGolferComment, the root handlers of the
// input it consumes, g_msgState53df54 = 0, pumpLight). The page shown follows the wall clock (timeGetTime >> 10), so every
// clock read comes from the context's InputSource (InputSource::clock); none = NeedInput{"showHoleStatsScreen"}.
void show_hole_stats_screen(Ctx &ctx, int32_t hole);

}  // namespace portsim

#endif  // PORTSIM_SIM_HOLESTATS_H
