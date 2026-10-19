// The shot cycle of the simulation core (phase 3.4, 2026-10-19): shot power, the shot set-up (prepareShot), the AI aim
// search (chooseShotTarget) with its trial flights (trialShot), and the shot narration checks (scanShotArea,
// narrateShot). Specifications: the analysis notes re/analysis/sim/<addr>_<name>.md (prepareShot = the note
// 00424120_simulateShot.md, chooseShotTarget = 00422fb0_simulateBallFlight.md), re/analysis/ai/004226a0_trialShot.md,
// log/c3/c3ea_purpose.md (per-jcc tables), the golfer record map re/analysis/data/golfer_record.md and the vector corpus.
//
// The cycle is recursive: prepareShot (a real shot, no explicit target) -> chooseShotTarget -> trialShot -> prepareShot
// (a trial, explicit target). Every + - * wraps (portsim/arith.h); golfer indices, hole bytes and tile types are used
// unchecked, as the original does (portsim/ptr.h).
#ifndef PORTSIM_SIM_SHOTS_H
#define PORTSIM_SIM_SHOTS_H

#include <cstdint>

#include "portsim/registry.h"

namespace portsim {

// shotPower(dist, lie, putt): the launch speed that carries `dist` (yards). A 10-entry ring cache keyed by (dist, lie)
// (g_shotPowerCacheDist / g_shotPowerCacheArg2 -> g_shotPowerCache, write index g_shotPowerRingIndex, used unchecked);
// on a miss: t = dist * 20 / 25, v = t * 33 - t * t / 48 + 64, step = v / 2, then while bisecting toward
// target = (dist << 10) / 25 with decaySum2(v, lie) (or decaySum(v) for a putt) the step halves until it is <= 2.
int32_t shot_power(Ctx &ctx, int32_t dist, int32_t lie, int32_t putt);

// wobble(): s = Random::range(g_rng, 101) - 50; |s| < 20 -> |s| >> 1, < 40 -> |s| - 10, else 2|s| - 50; negated unless
// sign(s) == 1.
int32_t wobble(Ctx &ctx);

// trialShot(g, x, y, aim): flies golfer g's ball toward world point (x, y) with aim -1 / 0 / 1 on a copy of the record
// (golfer record 155 holds the original while it runs and restores it at the end). Leaves the landing point in
// g_shot56a78c / g_shot56a790, appends track points to g's story pair slot, advances g_rng. Returns 0.
int32_t trial_shot(Ctx &ctx, int32_t g, int32_t x, int32_t y, int32_t aim);

// chooseShotTarget(g) (the hook name; the analysis note calls it simulateBallFlight): the AI aim search over the 21 x 21
// tiles around the reach point, two variants x three aims, 2 / 4 / 8 trial flights per cell and pass. Sets g's target
// tile, g_shotTargetX / Y, unk_005a9ce8 (variant), g_shotFlags5a7140; returns the chosen aim (-1, 0, 1).
int32_t choose_shot_target(Ctx &ctx, int32_t g);

// prepareShot(g, mode, x, y, aim) (the note's simulateShot): sets up golfer g's shot (direction, speed, height, spin,
// club, lie) toward its hole, or toward world point (x, y) when x != -1 (a trial); a real shot (mode 0, x == -1) also
// runs the aim search and emits the golfer's thoughts. Returns the flag word +0x10 after bit 0 is cleared.
int32_t prepare_shot(Ctx &ctx, int32_t g, int32_t mode, int32_t x, int32_t y, int32_t aim);

// scanShotArea(slot): for story pair slot `slot` decides whether its last shot is narrated (narrateShot).
void scan_shot_area(Ctx &ctx, int32_t slot);

// narrateShot(slot, dist, yards, flag): narrates the shot of story pair slot `slot` (flag 1 = a missed shot): the
// sentence in g_textBuffer, the skill the shot rewards or costs, the SimFoto bookkeeping (hole flags, golfer fields).
// Drawing, sound and the modal pump are not part of the port (U-0883).
int32_t narrate_shot(Ctx &ctx, int32_t slot, int32_t dist, int32_t yards, int32_t flag);

}  // namespace portsim

#endif  // PORTSIM_SIM_SHOTS_H
