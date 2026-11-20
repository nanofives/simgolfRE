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
// P31D (narrateShot.wideViewFit): the side margin E the box test adds on each side, (view width - 800) / 2; 0 with the switch at 1
int32_t narrate_fit_margin(Ctx &ctx);

// P32D item 7 (play-test 14: "a margin of error when throwing the ball"), RENDER-ONLY. The app option --shot-spread (off by
// default here; main.cpp sets it) makes mainLoop's aim drawing show the margin of error of the player's pending shot.
void shot_margin_set(bool on);
bool shot_margin_on();
// The margin of the pending shot of golfer g (aim tile in the record's +0xc4 / +0xc8, as mainLoop keeps it while aiming), from
// the shot's own random draws taken at their extremes (every combination; shots.cpp "P32D") on `scratch` (overwritten with a
// copy of live's GameState; live is only read), the ball flown in the air only, to where it first comes down. ang_lo / ang_hi:
// the landing's angle from the no-random landing's direction (radians, world x / y); len_lo / len_hi: its distance over the
// no-random distance. false (valid 0) when no estimate could be made.
struct ShotMargin {
  bool valid = false;
  double ang_lo = 0, ang_hi = 0, len_lo = 1, len_hi = 1;
  int combos = 0;
  // the no-random flight passes a point where the game's tree test can catch the ball (world units), when tree is set
  bool tree = false;
  int32_t tree_x = 0, tree_y = 0;
};
// P39D: (px, py) != (-1, ..): the shot prepared towards that world point (a FreeTargetScope around prepareShot(g, 1, -1, 0, 0), the
// target tile = the point >> 10), as the player's real shot is with mainLoop.aimHalfTileLattice at 0
// P39D: while one is open on this thread, prepareShot(g, 1, -1, ..) (the player's real shot) whose target tile (+0xcc / +0xd0)
// holds (x, y) takes the shot vector towards (x, y) instead of the tile's centre; every other step is the real shot's. (The
// trial path x != -1 would skip the hazards, the thoughts and the hole's shot counter.) The tile's centre gives the original's.
struct FreeTargetScope {
  FreeTargetScope(int32_t x, int32_t y);
  ~FreeTargetScope();
  FreeTargetScope(const FreeTargetScope &) = delete;
  FreeTargetScope &operator=(const FreeTargetScope &) = delete;
 private:
  bool prev_on;
  int32_t prev_x, prev_y;
};
bool shot_margin(Ctx &live, Ctx &scratch, int32_t g, ShotMargin &out, int32_t px = -1, int32_t py = 0);

}  // namespace portsim

#endif  // PORTSIM_SIM_SHOTS_H
