// The shot cycle (see portsim/sim/shots.h). Phase 3.4, 2026-10-19, written from the specification: the analysis notes,
// the per-jcc tables in log/c3/c3ea_purpose.md, the golfer record map, the C3 reimplementations in shim/src/re (reading
// reference: they cite every instruction) and the vector corpus. No original address or code.
//
// Callees owned by other stage B children (terrain 3.1a, course 3.1b, golfers 3.3, stories 3.5) are called through the
// port registry by their hooks.csv name (Foreign below): nothing is stubbed, and a callee that is not registered yet
// throws MissingResource (SG_E_NO_RESOURCE) naming it.
//
// x87: the only x87 code of these functions (outside Random::range / distance, which util owns) is the integer square
// root `fild; fsqrt; __ftol`. sqrt_ftol runs that sequence as inline x87 on Win32, under whatever control word the call
// runs at, and as SSE2 on x64 (exact at 0x027f; for an int32 argument the truncated root is the same under every
// rounding control at 53- and 64-bit precision, see log/p3/p34_notes.txt section 2).
//
// Stack frames. The originals read stack slots they never write: prepareShot's dirs[t] for a tile type the walk never
// met, chooseShotTarget's strict flag [F+0xb4] (log/c3/c3ea_purpose.md). The vector corpus was emulated on a zeroed stack,
// so those slots start at 0 here (U-0880). prepareShot's counts[] / dirs[] are indexed by a signed tile type without a
// bound: a type in 32..127 lands in dirs[] or the 2500-byte frame array exactly as in the original (one frame array
// below), a negative type would write below counts[] into other locals and throws FrameOverrun (U-0882).
#include "portsim/sim/shots.h"

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/bridge.h"
#include "portsim/sim/text.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {

// A frame write the port does not model (U-0882): reported as SG_E_EXCEPTION.
struct FrameOverrun {
  const char *where;
};

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   shotPower         1 = bisection runs while step > 1, 2 = the ring index is not advanced
//   wobble            1 = the middle band gives |s| - 9
//   trialShot         1 = air drag speed >> 4 instead of >> 5, 2 = the bounce keeps 1/16 of vz instead of b/12
//   prepareShot       1 = club clamp upper bound 10, 2 = power cap from reach * 3 / 4, 3 = the square root rounded by
//                     the current rounding control instead of __ftol's truncation (also in chooseShotTarget)
//   chooseShotTarget  1 = 1 trial per cell in the first pass, 2 = variant 1 (the cell corner) never tried,
//                     3 = hole-distance divisor always 4 (BLIND), 4 = landing value weight 4 (BLIND),
//                     5 = prune tolerance starts at 0x40 (BLIND)
//   scanShotArea      1 = narrate when D > k * v (instead of (k + 1) * v)
//   narrateShot       1 = the skill cap bucket * 2 + 5 (BLIND), 2 = the off-screen exit sets hole flag bit 2
int g_negctl_shot_power = 0;
int g_negctl_wobble = 0;
int g_negctl_trial_shot = 0;
int g_negctl_prepare_shot = 0;
int g_negctl_choose_shot_target = 0;
int g_negctl_scan_shot_area = 0;
int g_negctl_narrate_shot = 0;

// Compatibility switches (port/CORE.md "Defects"), 1 = behave like the original.
//   shotPower.cacheIgnoresPutt          model_defects#18: the cache key leaves out `putt`, so a putt result can answer a
//                                       full shot and back; fixed: putt calls bypass the cache
//   chooseShotTarget.strictFlagSticky   [F+0xb4] is never cleared: once one landing sets it, every later scan of the
//                                       search runs strict; fixed: recomputed per landing
//   prepareShot.hazardCellTransposed    the hazard probe stores its cell as y * 50 + x; fixed: x * 50 + y
//   narrateShot.maskNotRestored         two missed-shot exits return before the skill-mask restore; fixed: restored
int g_defect_shot_power_putt = 1;
int g_defect_strict_sticky = 1;
int g_defect_hazard_transposed = 1;
int g_defect_mask_not_restored = 1;

// ------------------------------------------------------------------------------------------------ foreign callees
class Foreign {
 public:
  Foreign(const char *name, const char *owner) : name_(name), owner_(owner) {}
  int32_t operator()(Ctx &c, std::initializer_list<int32_t> a) const {
    if (!def_) {
      const int i = registry_find(name_);
      if (i < 0) throw MissingResource{name_};
      def_ = registry_at(i, nullptr);
    }
    uint32_t args[8] = {};
    size_t n = 0;
    for (int32_t v : a) args[n++] = u32(v);
    Ret r;
    def_->fn(c, args, r);
    return i32(r.eax);
  }
  const char *owner() const { return owner_; }

 private:
  const char *name_, *owner_;
  mutable const FuncDef *def_ = nullptr;
};

const Foreign f_heightAt("heightAt42fa30", "3.1a");          // (x, y) world -> height
const Foreign f_slopeMix("slopeMix", "3.1a");               // (x, y, octant)
const Foreign f_heightBlend("heightBlend", "3.1a");         // (tx, ty, 0)
const Foreign f_rateTile("rateTile", "3.1a");               // (g) -> reach in yards
const Foreign f_scanTileLine("scanTileLine", "3.1a");       // (x0, y0, x1, y1, reach, aim, strict)
const Foreign f_holeQuadrants("holeQuadrants", "3.1b");     // (x, y, n)
const Foreign f_appendTrack("appendPairTrackPoint", "3.3");  // (slot)
const Foreign f_typeBit7Clear("typeBit7Clear", "3.3");      // (g)
const Foreign f_thoughtFlag("thoughtFlag", "3.5");          // (g)
const Foreign f_emitThought("emitGolferThought", "3.5");    // (g, kind, arg)
const Foreign f_buildGolferName("buildGolferName", "3.1b"); // (g, with_title)
const Foreign f_appendHoleName("appendHoleName", "3.1b");   // (hole)
const Foreign f_postEvent("postEvent", "3.5");             // (id, a, b)
const Foreign f_startMessage("startMessage", "3.7");       // (arg, prio, sound): ui straddler, draws g_rng

// ------------------------------------------------------------------------------------------------ record and tables
// golfer record byte offsets (re/analysis/data/golfer_record.md)
enum : uint32_t {
  kX = 0x00, kY = 0x04, kFlags = 0x10, kSkillMask = 0x16, kKind = 0x18, kPrefs = 0x19, kShotValue = 0x1b, kClub = 0x1c,
  kHole = 0x21, kStrokes = 0x22, kMoodByte = 0x36, k7a = 0x7a, k84 = 0x84, kClubsUsed = 0xa0, kPartner = 0xa2,
  kShotKind = 0xac, kType = 0xb6, kTargetX = 0xcc, kTargetY = 0xd0, kBallX = 0xd4, kBallY = 0xd8, kBallZ = 0xdc,
  kDir = 0xe0, kSpeed = 0xe4, kVz = 0xe8, kSpin = 0xec, kSkill = 0xf0,
};
constexpr int32_t kTrialCopy = 155;  // the golfer record trialShot keeps the original in (golfer_record.md)
// flag bits written as shifts (test_static.py reads 6-digit hex literals in the exe's range as addresses)
constexpr uint32_t kGolferMishit = 1u << 22;  // golfer flags +0x10: set by the mishit roll and by the lie roll
constexpr uint32_t kFlagAiSearch = 1u << 23;  // g_flags: chooseShotTarget is running (no mishit rolls meanwhile)

struct Rec {
  const Memory *m;
  uint32_t va;
  int32_t i(uint32_t o) const { return m->load<int32_t>(va + o); }
  void i(uint32_t o, int32_t v) const { m->store<int32_t>(va + o, v); }
  uint32_t u(uint32_t o) const { return m->load<uint32_t>(va + o); }
  void u(uint32_t o, uint32_t v) const { m->store<uint32_t>(va + o, v); }
  int32_t b(uint32_t o) const { return m->load<uint8_t>(va + o); }
  int32_t sb(uint32_t o) const { return static_cast<int8_t>(m->load<uint8_t>(va + o)); }
  void b(uint32_t o, int32_t v) const { m->store<uint8_t>(va + o, static_cast<uint8_t>(v)); }
  int32_t w(uint32_t o) const { return m->load<uint16_t>(va + o); }
  int32_t sw(uint32_t o) const { return static_cast<int16_t>(m->load<uint16_t>(va + o)); }
  void w(uint32_t o, int32_t v) const { m->store<uint16_t>(va + o, static_cast<uint16_t>(v)); }
  int32_t hole() const { return sb(kHole); }
};

// The views every shot function uses. Indices wrap and are unchecked like the original's address arithmetic.
struct World {
  Ctx &c;
  const Memory &m;
  uint32_t golfers, tiles, flags, attrs, holes, variant, walls, dir_x, dir_y;
  explicit World(Ctx &ctx)
      : c(ctx), m(ctx.mem), golfers(state_va(ctx, &GameState::g_golfers)), tiles(state_va(ctx, &GameState::g_tileType)),
        flags(state_va(ctx, &GameState::g_tileFlags)), attrs(state_va(ctx, &GameState::g_tileTypeAttrs)),
        holes(state_va(ctx, &GameState::g_holes)), variant(state_va(ctx, &GameState::g_tileVariant)),
        walls(state_va(ctx, &GameState::g_tileWallMask)), dir_x(state_va(ctx, &GameState::f_tab_dirDx_4c2878)),
        dir_y(state_va(ctx, &GameState::f_tab_dirDy_4c2898)) {}
  GameState &gs() const { return c.gs(); }
  Rec rec(int32_t g) const { return Rec{&m, golfers + u32(shl32(g, 8))}; }
  static int32_t cell(int32_t x, int32_t y) { return add32(mul32(x, 50), y); }
  int32_t tile(int32_t idx) const { return static_cast<int8_t>(m.load<uint8_t>(tiles + u32(idx))); }
  int32_t tile_u(int32_t idx) const { return m.load<uint8_t>(tiles + u32(idx)); }
  int32_t tflags(int32_t idx) const { return m.load<uint16_t>(flags + u32(mul32(idx, 2))); }
  int32_t tvariant(int32_t idx) const { return m.load<uint8_t>(variant + u32(idx)); }
  int32_t twall(int32_t idx) const { return static_cast<int8_t>(m.load<uint8_t>(walls + u32(idx))); }
  // byte `off` of tile type t's attribute bytes (TileTypeAttr +0x20 + off)
  uint32_t attr_va(int32_t t, uint32_t off) const { return attrs + 0x20u + u32(mul32(t, 0x30)) + off; }
  int32_t ts(int32_t t, uint32_t off) const { return static_cast<int8_t>(m.load<uint8_t>(attr_va(t, off))); }
  int32_t tu(int32_t t, uint32_t off) const { return m.load<uint8_t>(attr_va(t, off)); }
  // hole h's target tile (HoleRecord +0x18 / +0x1c), and other hole dwords
  int32_t hole_i(int32_t h, uint32_t off) const { return m.load<int32_t>(holes + u32(mul32(h, 0x208)) + off); }
  void hole_i(int32_t h, uint32_t off, int32_t v) const { m.store<int32_t>(holes + u32(mul32(h, 0x208)) + off, v); }
  int32_t hole_x(int32_t h) const { return hole_i(h, 0x18); }
  int32_t hole_y(int32_t h) const { return hole_i(h, 0x1c); }
  int32_t dx(int32_t k) const { return m.load<int32_t>(dir_x + u32(mul32(k, 4))); }
  int32_t dy(int32_t k) const { return m.load<int32_t>(dir_y + u32(mul32(k, 4))); }
  // Random::range(g_rng, n) & 0xffff
  int32_t rnd(int32_t n) const { return i32(random_range(c.gs().g_rng.state, u32(n)) & 0xFFFFu); }
};

int32_t octant(int32_t dir) { return sar32(add32(sar32(dir, 28), 1), 1) & 7; }

// fild dword / fsqrt / __ftol (truncation, low dword): see the file comment.
int32_t sqrt_ftol(int32_t v) {
#if defined(_M_IX86) && defined(_MSC_VER)
  int64_t q;
  uint16_t cw, cw2;
  const int nearest = SG_NEGCTL(g_negctl_prepare_shot) == 3;
  __asm {
    fild dword ptr [v]
    fsqrt
    fnstcw cw
    mov ax, cw
    or ah, 0x0c
    cmp nearest, 0
    je chop
    mov ax, cw
  chop:
    mov cw2, ax
    fldcw cw2
    fistp qword ptr [q]
    fldcw cw
  }
  return i32(static_cast<uint32_t>(static_cast<uint64_t>(q)));
#else
  if (SG_NEGCTL(g_negctl_prepare_shot) == 3) return i32(ftol_low32(std::nearbyint(std::sqrt(static_cast<double>(v)))));
  return i32(ftol_low32(std::sqrt(static_cast<double>(v))));
#endif
}

// startMessage(arg, prio, sound) (a ui function that changes simulation state): the registered one when 3.7
// has registered it, else this copy of its body (re/analysis/ui/0040cb00_startMessage.md, the pattern of 3.3 / 3.5):
// refused (0) while a message runs or waits and prio <= 0, or in panel mode 3; otherwise the text buffer becomes the
// message, its timer strlen / (3 + (difficulty != 0)) + 16, and two draws of g_rng place it.
int32_t start_message(Ctx &ctx, int32_t arg, int32_t prio, int32_t sound) {
  if (registry_find("startMessage") >= 0) return f_startMessage(ctx, {arg, prio, sound});
  GameState &gs = ctx.gs();
  const Ptr<uint8_t> dir = state_obj(ctx, &GameState::g_ui_569498).as<uint8_t>();
  if (((dir.get() != 0 || gs.g_msgState53df54 != 0) && prio <= 0) || gs.g_panelListMode == 3) return 0;
  gs.g_msgState53df54 = 0;
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  vstrcpy(ctx.mem, state_va(ctx, &GameState::unk_005a6d40), text);
  dir.set(1);
  (state_arr(ctx, &GameState::g_ui_5a34e4) + 2).set(arg);
  gs.g_messageTimer = static_cast<int32_t>(vstrlen(ctx.mem, text)) / ((gs.g_difficulty != 0 ? 1 : 0) + 3) + 16;
  (state_arr(ctx, &GameState::f_w_4c2e00) + 2).set(sound);
  if (prio < 0) (state_arr(ctx, &GameState::g_ui_5694a0) + 1).set(neg32(prio));
  const Ptr<int32_t> out = state_arr(ctx, &GameState::g_ui_56d1a8);
  out.set(i32(random_range(gs.g_rng.state, 600)));
  (out + 1).set(add32(i32(random_range(gs.g_rng.state, 200)), 200));
  return 1;
}

// VC6 _itoa(v, buf, 10): '-' and the decimal digits of |v| (as unsigned), NUL-terminated
void vitoa10(const Memory &m, uint32_t buf, int32_t v) {
  char tmp[16];
  int n = 0;
  uint32_t a = u32(v);
  if (v < 0) a = 0u - a;
  do {
    tmp[n++] = static_cast<char>('0' + a % 10u);
    a /= 10u;
  } while (a);
  uint32_t p = buf;
  if (v < 0) m.store<uint8_t>(p++, '-');
  while (n) m.store<uint8_t>(p++, static_cast<uint8_t>(tmp[--n]));
  m.store<uint8_t>(p, 0);
}
}  // namespace

// ================================================================================================== shotPower, wobble
int32_t shot_power(Ctx &ctx, int32_t dist, int32_t lie, int32_t putt) {
  GameState &s = ctx.gs();
  const bool cached = !(putt != 0 && !SG_DEFECT(g_defect_shot_power_putt));
  if (cached)
    for (int i = 0; i < 10; ++i)
      if (s.g_shotPowerCacheDist[i] == dist && s.g_shotPowerCacheArg2[i] == lie) return s.g_shotPowerCache[i];
  const int32_t next = s.g_shotPowerRingIndex;  // used unchecked
  if (cached) {
    (state_arr(ctx, &GameState::g_shotPowerCacheDist) + next).set(dist);
    (state_arr(ctx, &GameState::g_shotPowerCacheArg2) + next).set(lie);
  }
  const int32_t t = mul32(dist, 20) / 25;
  int32_t v = add32(sub32(mul32(t, 33), mul32(t, t) / 48), 64);
  int32_t step = v / 2;
  const int32_t target = shl32(dist, 10) / 25;
  const int32_t stop = SG_NEGCTL(g_negctl_shot_power) == 1 ? 1 : 2;
  do {
    const int32_t r = putt ? decay_sum(ctx, v) : decay_sum2(v, lie);
    if (r > target) v = sub32(v, step);
    if (r < target) v = add32(v, step);
    step /= 2;
  } while (step > stop);
  if (!cached) return v;
  (state_arr(ctx, &GameState::g_shotPowerCache) + next).set(v);
  if (SG_NEGCTL(g_negctl_shot_power) != 2) s.g_shotPowerRingIndex = add32(next, 1) % 10;
  return v;
}

int32_t wobble(Ctx &ctx) {
  const int32_t r = i32(random_range(ctx.gs().g_rng.state, 0x65) & 0xFFFFu);
  const int32_t s = r - 0x32;
  const int32_t sg = sign(s);
  int32_t m = s < 0 ? -s : s;
  if (m < 0x14)
    m >>= 1;
  else if (m < 0x28)
    m -= SG_NEGCTL(g_negctl_wobble) == 1 ? 9 : 10;
  else
    m = m + m - 0x32;
  return sg != 1 ? -m : m;
}

// ================================================================================================== trialShot
int32_t trial_shot(Ctx &ctx, int32_t g, int32_t x, int32_t y, int32_t aim) {
  const World w(ctx);
  GameState &s = ctx.gs();
  int32_t pair = -1;  // the LAST story pair slot whose golfer word equals g
  const Ptr<ShotTrack> tracks = state_arr(ctx, &GameState::g_storyPairs);
  for (int32_t k = 0; k < 16; ++k)
    if (static_cast<int16_t>((tracks + k).as<uint16_t>().get()) == g) pair = k;
  const Rec r = w.rec(g);
  const uint32_t copy = w.golfers + u32(kTrialCopy) * 0x100u;
  rep_movs(ctx.mem, copy, r.va, 0x100);
  prepare_shot(ctx, g, 0, x, y, aim);
  r.i(kSpin, r.i(kSpin) / 2);
  const bool slopes = (s.g_shotGolferPrefs & 4u) != 0;
  const int32_t drag = SG_NEGCTL(g_negctl_trial_shot) == 1 ? 4 : 5;
  int32_t steps = 0;
  while (r.i(kSpeed) != 0) {
    const int32_t ty = sar32(r.i(kBallY), 10), tx = sar32(r.i(kBallX), 10);
    const int32_t type = tile_blocked(ctx, tx, ty) ? 0x14 : w.tile(World::cell(tx, ty));
    const int32_t h0 = f_heightAt(ctx, {r.i(kBallX), r.i(kBallY)});
    const int32_t dx = fold_range(ctx, r.i(kDir), r.i(kSpeed) / 16);
    const int32_t dy = abs_diff(ctx, r.i(kDir), r.i(kSpeed) / 16);
    r.i(kBallX, add32(r.i(kBallX), dx));
    r.i(kBallY, sub32(r.i(kBallY), dy));
    const int32_t z = add32(r.i(kBallZ), r.i(kVz) / 32);
    r.i(kBallZ, z);
    if (z != 0 || r.i(kVz) != 0) {
      const int32_t vz = sub32(r.i(kVz), 0x40);
      r.i(kVz, vz);
      if (pair != -1 && vz > 0 && vz < 0x40) f_appendTrack(ctx, {pair});
    }
    steps = add32(steps, 1);
    if (pair != -1 && (steps & 7) == 0) f_appendTrack(ctx, {pair});
    const int32_t fx = sar32(r.i(kBallX), 6) & 15, fy = sar32(r.i(kBallY), 6) & 15;
    int32_t edge = 0;
    if (fx <= 1 && w.tile(World::cell(sub32(tx, 1), ty)) != type) edge = 1;
    if (fy <= 1 && w.tile(sub32(World::cell(tx, ty), 1)) != type) edge = 1;
    if (fx >= 14 && w.tile(World::cell(add32(tx, 1), ty)) != type) edge = 1;
    if (fy >= 14 && w.tile(add32(World::cell(tx, ty), 1)) != type) edge = 1;
    const int32_t oct = octant(r.i(kDir));
    if (r.i(kBallZ) > 1) {  // in the air
      const int32_t h1 = f_heightAt(ctx, {r.i(kBallX), r.i(kBallY)});
      r.i(kBallZ, add32(r.i(kBallZ), sub32(h0, h1)));
      r.i(kSpeed, sub32(r.i(kSpeed), sar32(r.i(kSpeed), drag)));
      r.i(kDir, add32(r.i(kDir), r.i(kSpin)));
      const int32_t in = in_range(ctx, type, r.i(kBallZ), tx, ty);
      if (s.unk_005a9cc4 != 2 && in != 0) {
        int32_t d = distance(sub32(sub32(r.i(kBallX), shl32(tx, 10)), 0x200),
                             sub32(sub32(r.i(kBallY), shl32(ty, 10)), 0x200));
        if (r.b(kKind) != 0 && (r.w(kSkillMask) & 0x200) != 0) d = add32(d, mul32(r.b(0xf9), d) / 4);
        const int32_t roll = w.rnd(0x300);
        if (d < roll) {  // the ball hits the object and is deflected
          r.i(kDir, add32(r.i(kDir), shl32(w.rnd(0x80) + 0x40, 24)));
          const int32_t take = w.rnd(r.i(kSpeed));
          r.i(kSpeed, sub32(r.i(kSpeed), take));
          r.u(kFlags, r.u(kFlags) | 2u);
        }
      }
    } else {  // on the ground
      int32_t fr = slopes ? clamp(sub32(w.ts(type, 1), f_slopeMix(ctx, {r.i(kBallX), r.i(kBallY), oct})), 0, 0x63)
                          : w.ts(type, 1);
      const int32_t side = f_slopeMix(ctx, {r.i(kBallX), r.i(kBallY), (oct + 2) & 7});
      if (fr < 2 && edge)
        fr = 2;
      else if (fr > 4 && steps > 0x80)
        fr = 4;
      if (slopes) r.i(kDir, sub32(r.i(kDir), shl32(side, 25)));
      if (fr < 5)
        r.i(kSpeed, sub32(r.i(kSpeed), sar32(r.i(kSpeed), fr) / 2));
      else
        r.i(kSpeed, add32(sub32(r.i(kSpeed), sar32(r.i(kSpeed), 6)), 0x20));
      if (type == 0x11) {
        if (!edge) r.i(kSpeed, r.i(kSpeed) / 2);
      } else if (type == 10) {
        const int32_t ax = 8 - fx < 0 ? fx - 8 : 8 - fx, ay = 8 - fy < 0 ? fy - 8 : 8 - fy;
        if (ax <= 2 && ay <= 2) r.i(kSpeed, r.i(kSpeed) / 2);
      }
      if (tx != sar32(r.i(kBallX), 10)) {
        const int32_t mask = dx > 0 ? 4 : 0x40;
        if (mask & w.twall(World::cell(tx, ty))) r.i(kDir, neg32(r.i(kDir)));
      }
      if (ty != sar32(r.i(kBallY), 10)) {
        const int32_t mask = dy > 0 ? 1 : 0x10;
        if (mask & w.twall(World::cell(tx, ty))) r.i(kDir, sub32(INT32_MIN, r.i(kDir)));
      }
    }
    if (r.i(kBallZ) <= 0 && r.i(kVz) < 0) {  // the bounce
      const int32_t inner = (fx >= 5 && fx < 11 && fy >= 5) ? 1 : 0;
      const int32_t cell = World::cell(tx, ty);
      int32_t b = w.ts(type, 0);
      if (s.unk_005a9cc4 == 0) {
        if (b < 2 && edge) b = 2;
        if (inner && (w.tflags(cell) & 0x20)) b = 4;
      }
      const int32_t keep = SG_NEGCTL(g_negctl_trial_shot) == 2 ? r.i(kVz) / 16 : mul32(b, r.i(kVz)) / 12;
      const int32_t v = clamp(sub32(-0x40, keep), 0, 9999);
      r.i(kVz, v);
      if (v < 0x80) r.i(kVz, 0);
      r.i(kBallZ, 0);
      if (pair != -1) f_appendTrack(ctx, {pair});
      if (r.u(kFlags) & 0x100u) {
        r.u(kDir, r.u(kDir) ^ 0x80000000u);
        r.u(kFlags, r.u(kFlags) & ~0x100u);
        if (r.b(kKind) == 0) {
          r.i(kSpeed, w.rnd(shl32(r.i(kSpeed), 1)));
        } else {
          const int32_t sp = r.i(kSpeed);
          const int32_t roll = w.rnd(shl32(sp, 1));
          r.i(kSpeed, add32(div32(mul32(sub32(roll, sp), 3), r.b(0xf9) + 5), sp));
        }
      }
      if (r.b(kFlags) & 0x80) {
        r.i(kSpeed, r.i(kSpeed) / 2);
        r.u(kFlags, (r.u(kFlags) & ~0x80u) | 0x100u);
      }
      if (slopes) {
        const int32_t k = mul32(r.i(kVz), 0xaec33);
        const int32_t o = add32(add32(sar32(r.i(kDir), 29), 1) & ~1, 2);
        r.i(kDir, sub32(r.i(kDir), mul32(f_slopeMix(ctx, {r.i(kBallX), r.i(kBallY), o & 7}), k)));
        const int32_t c1 = clamp(f_slopeMix(ctx, {r.i(kBallX), r.i(kBallY), oct}), -2, 2);
        r.i(kSpeed, add32(r.i(kSpeed), shl32(neg32(mul32(c1, r.i(kVz))), 1)));
        const int32_t vz = r.i(kVz);
        const int32_t c2 = clamp(f_slopeMix(ctx, {r.i(kBallX), r.i(kBallY), oct}), -2, 2);
        r.i(kVz, add32(mul32(c2, vz) / 2, vz));
      }
      if (type == 0x11) {
        if (!edge && (s.unk_005a9cc4 != 0 || !inner || !(w.tflags(cell) & 0x20))) {
          r.i(kVz, 0);
          r.i(kSpeed, 0);
        }
      } else if (type == 0xc && r.i(kSpeed) > 0x100 && !edge) {
        r.i(kDir, add32(r.i(kDir), shl32(0x50 - w.rnd(0xa0), 24)));
      }
    }
    if (r.i(kSpeed) < 0x40 && r.i(kBallZ) == 0 && r.i(kVz) == 0) {  // at rest
      s.g_shot56a78c = r.i(kBallX);
      s.g_shot56a790 = r.i(kBallY);
      r.i(kSpeed, 0);
    }
  }
  rep_movs(ctx.mem, r.va, copy, 0x100);
  return 0;
}

// ================================================================================================== prepareShot
int32_t prepare_shot(Ctx &ctx, int32_t g, int32_t mode, int32_t x, int32_t y, int32_t aim_in) {
  const World w(ctx);
  GameState &s = ctx.gs();
  const Rec r = w.rec(g);
  // counts[32] (frame +0x58), dirs[32] (+0xd8) and the 2500-byte array (+0x158) as one frame region; dirs[] and the
  // array start at 0 (U-0880), counts[] is zeroed by the original before use.
  int32_t frame[(0xb1c - 0x58) / 4] = {};
  auto counts = [&](int32_t t) -> int32_t & {
    if (t < 0) throw FrameOverrun{"prepareShot: counts[] with a negative tile type (U-0882)"};
    return frame[t];
  };
  auto dirs = [&](int32_t t) -> int32_t & {
    if (t < -32) throw FrameOverrun{"prepareShot: dirs[] with a tile type below -32 (U-0882)"};
    return frame[32 + t];
  };
  int32_t aim = aim_in;  // the argument slot is rewritten
  const int32_t hole0 = r.hole();
  const int32_t reach = f_rateTile(ctx, {g});
  if (r.u(kFlags) & 1u) {
    ctx.mem.store<uint8_t>(w.attr_va(0x11, 2), 0x20);
    ctx.mem.store<uint8_t>(w.attr_va(0x14, 2), 0x20);
  }
  s.g_shotRedrawGolfer = -1;
  if ((s.g_flags & 0x20u) && s.g_msgState53df54 > 0) s.g_shotRedrawGolfer = s.g_msgState53df54 - 1;
  const int32_t btx = sar32(r.i(kBallX), 10), bty = sar32(r.i(kBallY), 10);  // F+0x34 / F+0x40
  const int32_t bcell = World::cell(btx, bty);                                // F+0x50
  int32_t lie = w.tile(bcell);                                                // F+0x14
  if (mode == 0) {
    r.i(kTargetX, w.hole_x(hole0));
    r.i(kTargetY, w.hole_y(hole0));
  } else {
    const int32_t m = s.g_playerShotStyle;
    aim = 0;
    if (m <= 1) aim = m;
  }
  int32_t ex = add32(sub32(shl32(r.i(kTargetX), 10), r.i(kBallX)), 0x200);
  int32_t ey = add32(sub32(shl32(r.i(kTargetY), 10), r.i(kBallY)), 0x200);
  if (x != -1) {
    ex = sub32(x, r.i(kBallX));
    ey = sub32(y, r.i(kBallY));
    r.i(kTargetX, sar32(x, 10));
    r.i(kTargetY, sar32(y, 10));
  }
  int32_t dist;  // F+0x10
  {
    const int32_t sx = sar32(mul32(ex, 25), 10), sy = sar32(mul32(ey, 25), 10);
    dist = sqrt_ftol(add32(mul32(sy, sy), mul32(sx, sx)));
  }
  r.i(kDir, angle_fixed(ex, ey));
  r.u(kFlags, r.u(kFlags) & 0xefffffffu);
  const int32_t oct0 = octant(r.i(kDir));
  if (mode == 0 && x == -1) {
    const int32_t thr = ((r.u(kFlags) & 1u) || (r.b(kPrefs) & 4)) ? 0x19 : 0x4b;
    if (dist > thr && lie != 1) {  // the aim search
      const int32_t len = (shl32(clamp(reach - 25, 0, dist), 10) + 0x200) / 25;
      const int32_t px = sar32(add32(fold_range(ctx, r.i(kDir), len), r.i(kBallX)), 10);
      const int32_t y0 = r.i(kBallY);
      const int32_t py = sar32(sub32(y0, abs_diff(ctx, r.i(kDir), len)), 10);
      tile_distance(r.i(kBallX), y0, px, py);  // result unused
      distance(sub32(px, w.hole_x(r.hole())), sub32(py, w.hole_y(r.hole())));  // result unused
      aim = choose_shot_target(ctx, g);
      int32_t qx0, qy0;
      if (s.unk_005a9ce8 == 0) {
        qx0 = add32(sub32(shl32(r.i(kTargetX), 10), r.i(kBallX)), 0x200);
        qy0 = add32(sub32(shl32(r.i(kTargetY), 10), r.i(kBallY)), 0x200);
        r.u(kFlags, r.u(kFlags) & 0xefffffffu);
      } else {
        qx0 = sub32(shl32(r.i(kTargetX), 10), r.i(kBallX));
        qy0 = sub32(shl32(r.i(kTargetY), 10), r.i(kBallY));
        r.u(kFlags, r.u(kFlags) | 0x10000000u);
      }
      const int32_t qx = sar32(mul32(qx0, 25), 10), qy = sar32(mul32(qy0, 25), 10);
      r.i(kDir, angle_fixed(qx, qy));
      dist = sqrt_ftol(add32(mul32(qy, qy), mul32(qx, qx)));
      // (debug redraw when g_shotRedrawGolfer == g: Window::refreshFull + pumpAndRedraw(0), UI only: dropped, U-0881)
    } else {
      const int32_t tX = r.i(kTargetX), tY = r.i(kTargetY);
      s.g_shotFlags5a7140 = 0;
      s.g_shotTargetX = add32(shl32(tX, 10), 0x200);
      s.g_shotTargetY = add32(shl32(tY, 10), 0x200);
      if (lie != 1 && dist > 0x19) {  // green slope
        const int32_t o1 = oct0 & 6, o2 = (oct0 + 1) & 6;
        const int32_t sum = w.ts(w.tile(add32(sub32(mul32(sub32(tX, w.dx(o1)), 50), w.dy(o1)), tY)), 2) +
                            w.ts(w.tile(add32(sub32(mul32(sub32(tX, w.dx(o2)), 50), w.dy(o2)), tY)), 2) -
                            w.ts(w.tile(add32(add32(mul32(add32(w.dx(o1), tX), 50), w.dy(o1)), tY)), 2) -
                            w.ts(w.tile(add32(add32(mul32(add32(tX, w.dx(o2)), 50), w.dy(o2)), tY)), 2);
        if (sum >= 4) {
          const int32_t k = w.ts(lie, 2) <= 0 ? 6 : 0;
          dist = add32(dist, clamp(dist / 4 - k, 0, 12));
        }
        if (sum <= -4) dist = sub32(dist, 6);
      }
    }
  }
  // walk the line to the target, scoring its tiles
  const int32_t oct = octant(r.i(kDir));  // F+0x24
  int32_t acc = 4 - (g & 3);
  int32_t steps = dist / 25;  // F+0x2c
  int32_t rough = 0;          // F+0x20
  int32_t flag100 = 0;        // F+0x48
  int32_t water = 0;          // F+0x28
  int32_t f38 = 0, f1c = 0;
  int32_t f3c = 0, f4c = 0;  // the walk's last tile y / step count (never written when the walk does not run: U-0880)
  if (steps > 0) {
    int32_t i = 0;
    int32_t along = 0x200;  // F+0x18
    do {
      const int32_t px = sar32(add32(fold_range(ctx, r.i(kDir), along), r.i(kBallX)), 10);
      const int32_t py = sar32(sub32(r.i(kBallY), abs_diff(ctx, r.i(kDir), along)), 10);
      f3c = py;
      if (tile_blocked(ctx, px, py)) break;
      const int32_t idx = World::cell(px, py);
      const int32_t t = w.tile(idx);
      const int32_t b2 = w.ts(t, 2);
      acc = add32(acc, shl32(mul32(i, b2), 1));
      const int32_t n = i + 1;
      f4c = n;
      counts(t) = add32(counts(t), shl32(mul32(n, b2), 1));
      dirs(t) = oct ^ 4;
      if (w.tile_u(idx) == 0x11 && water == 0) water = i;
      if (along != 0x200 && w.tu(t, 6) == 0xd) ++rough;
      if (w.tflags(idx) & 0x100) flag100 = t;
      int32_t hA, hB;
      if (i > steps / 2) {
        hA = add32(f_heightBlend(ctx, {r.i(kTargetX), r.i(kTargetY), 0}), 1);
        hB = f_heightBlend(ctx, {px, py, 0});
      } else {
        hA = add32(f_heightBlend(ctx, {btx, bty, 0}), 1);
        hB = f_heightBlend(ctx, {px, py, 0});
      }
      if (hB > hA) rough += 2;
      const int32_t off = w.rnd(2) ? 0x0aaaaaaa : i32(0xf5555556u);
      const int32_t qx = sar32(add32(fold_range(ctx, add32(r.i(kDir), off), along), r.i(kBallX)), 10);
      const int32_t qy = sar32(sub32(r.i(kBallY), abs_diff(ctx, add32(r.i(kDir), off), along)), 10);
      acc = add32(acc, mul32(w.ts(w.tile(World::cell(qx, qy)), 2), n));
      i = n;
      along = add32(along, 0x400);
    } while (i < steps);
  }
  for (int32_t k = 0; k < 8; ++k) {  // around the target
    const int32_t nx = add32(w.dx(k), r.i(kTargetX)), ny = add32(w.dy(k), r.i(kTargetY));
    if (tile_blocked(ctx, nx, ny)) {
      acc = add32(acc, add32(shl32(steps, 2), 4));
    } else {
      const int32_t t = w.tile(World::cell(nx, ny));
      const int32_t v = mul32(w.ts(t, 2), add32(steps, 1)) / 2;
      acc = add32(acc, v);
      counts(t) = add32(counts(t), v);
      dirs(t) = k;
    }
  }
  int32_t bestType = f3c, bestDir = f4c;  // F+0x3c, F+0x4c: replaced by the first type whose count beats the best so far
  {
    int32_t best = -1;
    for (int32_t t = 0; t < 32; ++t)
      if (frame[t] > best) {
        best = frame[t];
        bestType = t;
        bestDir = frame[32 + t];
      }
  }
  {
    int32_t dif = div32(acc, add32(steps, 3));
    if (rough >= 2) dif += 4;
    if (w.tile_u(World::cell(sar32(r.i(kBallX), 10), sar32(r.i(kBallY), 10))) == 1) dif = 0;
    if (dist <= 0x28 && dif > 10) dif = 10;
    r.b(kShotValue, dif);
  }
  // distance, lie, power, height
  int32_t d = dist;
  if (r.b(kPrefs) & 4) {
    const int32_t h1 = f_heightBlend(ctx, {r.i(kTargetX), r.i(kTargetY), 0});
    const int32_t h2 = f_heightBlend(ctx, {btx, bty, 0});
    const int32_t dd = sub32(h1, h2);
    d = add32(d, dd > 0 ? mul32(dd, 25) / 8 : mul32(dd, 25) / 10);
  }
  int32_t cd = clamp(d, 0, reach);
  r.i(kSpeed, cd);
  dist = cd;
  {
    const int32_t q = sx8(u32(div32(mul32(sub32(reach, cd), 60), mul32(reach, 3))));
    r.b(kClub, q);
    r.b(kClub, clamp(q, lie != 0 ? 1 : 0, SG_NEGCTL(g_negctl_prepare_shot) == 1 ? 0xa : 0xb));
  }
  if (mode != 0 && s.g_playerShotStyle == 3 && r.sb(kClub) < 5) {
    cd = mul32(reach, 13) / 18;
    r.i(kSpeed, cd);
    dist = cd;
    r.b(kClub, 5);
  }
  if (lie == 1 && cd < 0x32 && !(r.b(kFlags) & 1)) r.b(kClub, 0xd);  // a putt
  {
    const int32_t s2 = mul32(r.i(kSpeed), 20) / 25;
    const int32_t sp = add32(sub32(mul32(s2, 33), mul32(s2, s2) / 48), 0x40);
    r.i(kSpeed, sp);
    const int32_t vz = sp / 8 + 0x200;
    r.i(kVz, vz);
    r.i(kSpeed, shot_power(ctx, mul32(cd, 4) / 5, vz, 0));
  }
  int32_t acu = (s.g_tournamentPrep & 2u) ? 12 : 20;
  if ((r.b(kPrefs) & 4) && s.g_activeObjectLevel[6] != 0 && (r.b(kKind) & 0xe0) != 0x20)
    acu = add32(acu, div32(acu, sub32(4, s.g_activeObjectLevel[6])));
  if (r.b(kSkillMask) & 0x10) acu = add32(acu, mul32(r.b(0xf4), acu) / 8);
  {
    const int32_t h = r.hole();
    if (w.tvariant(World::cell(w.hole_x(h), w.hole_y(h))) & 0x80) acu -= 10;
  }
  if (r.sb(kMoodByte) < 2) acu /= 2;
  const int32_t halfAcu = acu / 2;
  const int32_t err = add32(add32(w.rnd(halfAcu), halfAcu), 4);
  const int32_t maxPower =
      shot_power(ctx, SG_NEGCTL(g_negctl_prepare_shot) == 2 ? mul32(reach, 3) / 4 : mul32(reach, 4) / 5, r.i(kVz), 0);
  int32_t f2c = maxPower;
  r.i(kSpin, 0);
  if (s.unk_005a9cc4 < 2) r.i(kSpin, shl32(mul32(wobble(ctx), 5), 16));
  if (s.unk_005a9cc4 == 1) r.i(kSpin, r.i(kSpin) / 3);
  if (r.sb(kMoodByte) < 0) {
    const int32_t sp = r.i(kSpin);
    const int32_t c = clamp(neg32(r.sb(kMoodByte)), 0, 3);
    r.i(kSpin, add32(mul32(c, sp) / (lie != 1 ? 3 : 8), sp));
  }
  const int32_t lvl = s.g_difficulty;
  if (lie != 1 && (r.b(kKind) & 0xe0) != 0x20 && ((r.b(kPrefs) & 2) || (r.b(kFlags) & 1))) {
    const int32_t v = div32(r.i(kSpin), add32(s.g_activeObjectLevel[8], 2));
    r.i(kSpin, v);
    if (lvl < 2) r.i(kSpin, sub32(v, div32(v, add32(lvl, 2))));
  }
  if ((r.u(kFlags) & 0x4000000u) && (r.b(kKind) & 0xe0) != 0x20) r.i(kSpin, sub32(r.i(kSpin), r.i(kSpin) / 3));
  r.u(kFlags, r.u(kFlags) & 0xffbfffffu);
  int32_t bias = -3;  // F+0x18, the thought bias
  if (r.b(kClub) == 0xd) {  // a putt
    r.i(kSpin, 0);
    const int32_t k = (s.g_flags & 0x200000u) ? 2 : 1;
    if (mul32(k, dist) > err) {
      const int32_t roll = w.rnd(2);
      if (roll == 0)
        r.i(kSpin, shl32(w.rnd(0x96) + 0x96, 18));
      else if (roll == 1)
        r.i(kSpin, shl32(-0x96 - w.rnd(0x96), 18));
      if (dist <= 5) r.i(kSpin, 0);
      if (dist > 15) r.i(kSpin, r.i(kSpin) / 2);
      if (dist > 25) r.i(kSpin, r.i(kSpin) / 2);
      if (dist > 35) r.i(kSpin, r.i(kSpin) / 2);
    }
  } else {
    if (r.b(kKind) != 0) {
      if (g == s.g_playerGolfer) {
        r.i(kSpin, sub32(r.i(kSpin), mul32(sub32(3, lvl), r.i(kSpin)) / 6));
      } else if ((r.b(kKind) & 0xe0) == 0x20) {
        r.i(kSpin, add32(mul32(sub32(3, lvl), r.i(kSpin)) / 3, r.i(kSpin)));
      } else {
        bias = r.sb(kMoodByte) - 3;
      }
      if (r.sb(kClub) > 3) {
        r.i(kSpin, mul32(r.i(kSpin), 6) / (r.b(0xf3) + 4));
        bias += r.b(0xf3);
      } else if (r.b(kStrokes) == 0) {
        r.i(kSpin, mul32(r.i(kSpin), 6) / (r.b(0xf2) + 4));
        bias += r.b(0xf2);
      }
      if (aim == 1) {
        r.i(kSpin, mul32(r.i(kSpin), 6) / (r.b(0xf5) + 3));
        bias += r.b(0xf5);
      } else if (aim == -1) {
        r.i(kSpin, mul32(r.i(kSpin), 6) / (r.b(0xf6) + 3));
        bias += r.b(0xf6);
      } else if (aim == 0 && s.g_playerShotStyle != 3) {
        r.i(kSpin, mul32(r.i(kSpin), 6) / 7);
        bias += 3;
      }
    }
    if (!(s.g_flags & kFlagAiSearch) && dist > 0x4b) {  // a mishit roll
      const int32_t roll = w.rnd(r.i(kSpeed));
      const int32_t a = abs32(r.i(kSpin));
      if (roll > add32(sar32(a, 9), 0x200)) {
        r.u(kFlags, r.u(kFlags) | kGolferMishit);
        bias += 4;
        if (g != s.g_playerGolfer)
          r.i(kSpin, r.i(kSpin) / 2);
        else
          r.i(kSpin, 0);
      }
    }
  }
  // aim offsets and curve
  int32_t dir = r.i(kDir);
  if (aim == 1)
    dir = add32(dir, 0x15555554);
  else if (aim == -1)
    dir = sub32(dir, 0x15555554);
  if (r.b(kClub) != 0xd) {
    const int32_t c = clamp(r.i(kSpin), i32(0xfc71c71du), 0x38e38e3);
    r.i(kDir, add32(r.i(kDir), mul32(c, 3)));
    r.i(kSpin, sub32(r.i(kSpin), c / 2));
    if (s.unk_005a9cc4 == 0 && (r.b(kKind) & 0xe0) == 0x20) {
      const int32_t sp = r.i(kSpin);
      r.i(kSpin, add32(mul32(sub32(dist, 100), sp) / 256, sp));
      const int32_t roll = w.rnd(0x71c6);
      r.i(kSpin, add32(r.i(kSpin), sub32(roll, 0x38e38e3) / 50));
    }
  }
  r.u(kFlags, r.u(kFlags) & 0xfffffe7fu);
  r.w(kShotKind, 0);
  const int32_t stopAt = ((r.u(kFlags) & 1u) && mode == 0) ? 1 : water;
  bool chip = false;
  if (((r.b(kPrefs) & 4) || (r.u(kFlags) & 2u)) && lie != 1 && stopAt != 1) {
    if (mode != 0) {
      chip = s.g_playerShotStyle == 4;
    } else {
      const int32_t py = sar32(sub32(r.i(kBallY), abs_diff(ctx, dir, 0x80)), 10);
      const int32_t px = sar32(add32(fold_range(ctx, dir, 0x80), r.i(kBallX)), 10);
      if (w.tu(w.tile(World::cell(px, py)), 6) == 0xd) {
        chip = true;
      } else {
        const int32_t py2 = sar32(sub32(r.i(kBallY), abs_diff(ctx, dir, 0x400)), 10);
        const int32_t px2 = sar32(add32(fold_range(ctx, dir, 0x400), r.i(kBallX)), 10);
        chip = w.tu(w.tile(World::cell(px2, py2)), 6) == 0xd;
      }
    }
  }
  if (chip) {  // a chip out
    r.i(kVz, clamp(r.i(kSpeed) / 12, 0, 0x100));
    r.i(kSpeed, add32(add32(sub32(mul32(reach, 33), mul32(reach, reach) / 48), r.i(kSpeed)), 0x40) / 2);
    r.i(kSpeed, shot_power(ctx, mul32(dist, 3) / 4, r.i(kVz), 0));
    if (stopAt != 0) r.i(kSpeed, shot_power(ctx, mul32(stopAt, 50) / 3, r.i(kVz), 0));
    aim = 0;
    r.w(kShotKind, 4);
  } else if ((r.b(kPrefs) & 4) && aim == 0 && lie != 1 && w.ts(lie, 2) == 0 && r.sb(kClub) >= 4 && dist > 0x19) {
    const bool go = mode == 0 ? w.tile_u(World::cell(r.i(kTargetX), r.i(kTargetY))) == 1 : s.g_playerShotStyle == 3;
    if (go) {  // a lob
      r.u(kFlags, r.u(kFlags) | 0x80u);
      const int32_t vz = r.i(kVz);
      r.i(kVz, add32(mul32(add32(dist, 0x32), vz) / 400, vz));
      r.i(kSpeed, shot_power(ctx, dist, r.i(kVz), 0));
      r.w(kShotKind, 3);
      r.i(kSpin, mul32(r.i(kSpin), 6) / (r.b(0xf7) + 3));
      bias += r.b(0xf7);
    }
  }
  const int32_t b7a = r.b(k7a);  // F+0x30 from here on
  int32_t e7 = b7a;
  if (x == -1) {  // a real shot: hazards on the way and the golfer's thoughts
    if (r.b(kClub) == 0xd) {
      if (w.tile_u(World::cell(r.i(kTargetX), r.i(kTargetY))) == 1) r.i(kSpeed, shot_power(ctx, dist + 2, 0, 1));
    } else {
      int32_t n = 0;  // F+0x20
      w.hole_i(r.hole(), 0x24, add32(w.hole_i(r.hole(), 0x24), 1));
      // the cell the thoughts name: transposed in the original (prepareShot.hazardCellTransposed)
      auto hazardCell = [](int32_t px, int32_t py) {
        return SG_DEFECT(g_defect_hazard_transposed) ? World::cell(py, px) : World::cell(px, py);
      };
      auto probes = [&] { return div32(shl32(f_heightBlend(ctx, {btx, bty, 0}), 4), add32(s.g_difficulty, 2)); };
      if (probes() > 0) {
        do {
          int32_t dr = r.i(kDir);
          dr = add32(dr, shl32(0xc - w.rnd(0x19), 24));
          const int32_t L = add32(sub32(dist, w.rnd(0xc8)), 0x64);
          if (L >= 0) {
            const int32_t st = shl32(L / 25, 10);
            const int32_t px = sar32(add32(fold_range(ctx, dr, st), r.i(kX)), 10);
            const int32_t py = sar32(sub32(r.i(kY), abs_diff(ctx, dr, st)), 10);
            const int32_t o3 = octant(dr);
            if (!tile_blocked(ctx, px, py)) {
              const int32_t idx = World::cell(px, py);
              const int32_t back = add32(sub32(mul32(sub32(px, w.dx(o3)), 50), w.dy(o3)), py);
              if ((w.tflags(idx) & 0x100) || w.tile_u(idx) == 0x13) {
                const int32_t h1 = add32(f_heightBlend(ctx, {btx, bty, 0}), 1);
                const int32_t h2 = f_heightBlend(ctx, {px, py, 0});
                if (h2 <= h1 && w.tu(w.tile(back), 6) != 0xd) flag100 = hazardCell(px, py);
              }
              if (w.tu(w.tile(idx), 7) == 0x10 && w.tu(w.tile(back), 6) != 0xd) {
                const int32_t o = object_at(ctx, px, py);
                const Ptr<PlacedObject> obj = state_arr(ctx, &GameState::g_placed_objects) + o;
                if (w.tile_u(idx) == 0x15) {
                  if (o != -1 && obj.as<int16_t>().get() == 5 && obj.byte(8).as<int32_t>().get() != 0)
                    f1c = obj.byte(8).as<int32_t>().get();
                  else
                    f38 = hazardCell(px, py);
                } else if (obj.as<int16_t>().get() == 4) {
                  if (obj.byte(8).as<int32_t>().get() < 0x10)
                    flag100 = hazardCell(px, py);
                  else
                    f38 = hazardCell(px, py);
                }
              }
            }
          }
          ++n;
        } while (n < probes());
        e7 = b7a;
      }
    }
    {
      const int32_t h1 = f_heightBlend(ctx, {r.i(kTargetX), r.i(kTargetY), 0});
      const int32_t h2 = f_heightBlend(ctx, {btx, bty, 0});
      r.b(kShotValue, r.b(kShotValue) + abs32(sub32(h2, h1)));
    }
    if (s.unk_005a9ce4 != 0 && w.ts(w.tile(World::cell(r.i(kTargetX), r.i(kTargetY))), 2) > 0 && w.ts(lie, 2) <= 0 &&
        mode == 0 && !(r.b(kFlags) & 1)) {
      f_emitThought(ctx, {g, 8, 0x14});
    } else if (f38 != 0) {
      if (f1c != 0)
        f_emitThought(ctx, {g, 0x16, f1c - 1});
      else
        f_emitThought(ctx, {g, 0x14, f38});
    } else if (f1c != 0) {
      f_emitThought(ctx, {g, 0x16, f1c - 1});
    } else if ((w.tflags(bcell) & 0x800) && f_thoughtFlag(ctx, {g}) != 0 && !(w.tflags(bcell) & 0x4000)) {
      f_emitThought(ctx, {g, 0x18, 0x14});
    } else {
      if (dist < 100 || mode != 0) s.g_shotFlags5a7140 &= ~3u;
      const int32_t v = i32(s.g_shotFlags5a7140);
      if (v != 0 && r.w(kShotKind) != 4) {
        if (v & 3)
          f_emitThought(ctx, {g, (~v & 1) | 0x20, 0x14});
        else if (w.ts(lie, 2) <= 0)
          f_emitThought(ctx, {g, 6, 0x14});
      } else {
        bool done = false;
        if (r.hole() > 1 && r.sb(kStrokes) == (g & 1) + 1) {
          const int32_t roll = w.rnd(3);
          if (w.hole_i(r.hole(), 0x1fc) <= roll) {
            f_emitThought(ctx, {g, 0x1d, 0x14});
            done = true;
          }
        }
        if (!done && s.g_difficulty != 0) {
          if (r.hole() > 1 && r.b(kStrokes) == 1) {
            const int32_t roll = w.rnd(3);
            if (w.hole_i(r.hole(), 0x1fc) > roll + 3) {
              f_emitThought(ctx, {g, 0x1e, 0x14});
              done = true;
            }
          }
          if (!done && s.g_difficulty != 0) {
            const int32_t roll = w.rnd(4);
            if (r.sb(kShotValue) <= add32(roll, mul32(s.g_difficulty, 2)) && dist > 100 && w.ts(lie, 2) <= 0 &&
                flag100 == 0) {
              f_emitThought(ctx, {g, 4, 0x14});
              r.b(kShotValue, 0);
              done = true;
            }
          }
        }
        if (!done) {
          const int32_t roll = w.rnd(5);
          const int32_t dif = r.sb(kShotValue);
          if (dif > add32(add32(roll, s.g_difficulty), 6) && dist > 0x28 && flag100 == 0) {
            int32_t kind;
            if (dif >= 0x18) {
              kind = 0x1f;
            } else {
              const int32_t dd = sub32(bestDir, oct) & 7;
              kind = (dd < 2 || dd > 6) ? 0x26 : ((dd == 2 || dd == 6) ? 5 : 0x25);
            }
            f_emitThought(ctx, {g, kind, bestType});
            r.b(kShotValue, r.sb(kShotValue) < 0x18 ? 10 : 20);
          } else {
            r.b(kShotValue, 6);
          }
        }
      }
    }
    r.u(kFlags, r.u(kFlags) & 0xffffff9fu);
    if (r.b(k7a) == e7 && r.b(k84) == 0) {
      if (flag100 != 0 && w.ts(lie, 2) < 1) {
        f_emitThought(ctx, {g, 0x1c, flag100});
      } else {
        if (aim == 1)
          f_emitThought(ctx, {g, 0x37, 0x14});
        else if (aim == -1)
          f_emitThought(ctx, {g, 0x38, 0x14});
        if ((r.b(kFlags) & 0x80) && dist > 100) f_emitThought(ctx, {g, 0x39, 0x14});
        if (r.w(kShotKind) == 4) f_emitThought(ctx, {g, r.b(kStrokes) != 0 ? 0x3c : 9, lie});
        if (dist > 0x4b) {
          const int32_t sh = (r.b(kPrefs) & 2) ? 15 : 16;
          if (r.i(kSpin) > shl32(0x1e, sh)) r.u(kFlags, r.u(kFlags) | 0x20u);
          if (r.i(kSpin) < shl32(-0x1e, sh)) r.u(kFlags, r.u(kFlags) | 0x40u);
        }
        const uint32_t tsv = state_va(ctx, &GameState::g_golferTypeState) + 0x16u +
                             u32(mul32(r.sw(kType), 0x2c)) + u32(r.hole());
        if (r.b(k7a) == e7 && r.b(k84) == 0 && r.b(kStrokes) == 0 && ctx.mem.load<uint8_t>(tsv) != 0 &&
            r.b(kKind) == 0)
          f_emitThought(ctx, {g, 0x3b, 0x14});
        const Rec pr = w.rec(r.sw(kPartner));
        if (r.b(k7a) == e7 && r.b(kStrokes) == 3 && ((r.b(kHole) + g) & 1) == 0 && r.b(kKind) == 0 &&
            pr.b(kKind) == 0) {
          const int32_t pg = g ^ 1;
          const int32_t t1 = f_typeBit7Clear(ctx, {pg});
          const int32_t t2 = f_typeBit7Clear(ctx, {g});
          if (t2 == t1 && pr.b(k84) == 0 && r.b(k84) == 0) {
            f_emitThought(ctx, {g, 0x30, 0x14});
            f_emitThought(ctx, {pg, 0x31, 0x14});
            pr.b(k84, pr.b(k84) + 1);
          }
          e7 = b7a;
        }
      }
    }
    if (r.b(k84) != 0) e7 = -1;
    if (r.b(k7a) == e7 && r.hole() > 1 && r.sb(kClub) < 0xd && !(r.sw(kClubsUsed) & shl32(1, r.b(kClub) & 31)))
      f_emitThought(ctx, {g, 0x36, r.sb(kClub)});
    r.w(kClubsUsed, r.w(kClubsUsed) | i32(shl32(1, r.b(kClub) & 31) & 0xffff));
    if (r.b(k7a) == e7) {
      {
        const int32_t h1 = f_heightBlend(ctx, {r.i(kTargetX), r.i(kTargetY), 0});
        const int32_t h2 = f_heightBlend(ctx, {btx, bty, 0});
        if (h2 > h1) f_emitThought(ctx, {g, 0x2e, 0x14});
      }
      {
        const int32_t h1 = f_heightBlend(ctx, {r.i(kTargetX), r.i(kTargetY), 0});
        const int32_t h2 = f_heightBlend(ctx, {btx, bty, 0});
        if (h2 < h1) f_emitThought(ctx, {g, 0x2d, 0x14});
      }
    }
    if (r.b(k7a) == e7 && r.b(kKind) == 0 && r.b(k84) == 0 && lie == 1 &&
        (add32(add32(r.b(kStrokes), g), r.b(kHole)) & 3) == 0)
      f_emitThought(ctx, {g, 0x3e, 0x14});
  }
  // the aim offset, the power cap, the lie
  r.u(kFlags, r.u(kFlags) & ~2u);
  int32_t spinAdd = 0;  // F+0x1c
  int32_t cap;
  if (aim == 1 || aim == -1) {
    const bool right = aim == 1;
    const int32_t nd = add32(r.i(kDir), right ? 0x15555554 : i32(0xeaaaaaacu));
    r.i(kDir, nd);
    if (r.b(kFlags) & 0x80) r.i(kDir, add32(nd, right ? i32(0xfaaaaaabu) : 0x5555555));
    const int32_t sp = r.i(kSpin);
    if ((right ? sp > 0 : sp < 0) && g == s.g_playerGolfer) r.i(kSpin, sp / 2);
    const int32_t mp = f2c, dd = dist;
    spinAdd = right ? i32(0xfdc656abu) : 0x239a955;
    const int32_t adj = add32(mp, div32(shl32(mp, 4), sub32(400, dd)));
    if (dd >= 300)
      r.i(kSpeed, adj);
    else if (dd >= 250)
      r.i(kSpeed, add32(div32(shl32(r.i(kSpeed), 4), sub32(400, dd)), r.i(kSpeed)));
    else
      r.i(kSpeed, add32(div32(shl32(r.i(kSpeed), 4), sub32(0x14a, dd)), r.i(kSpeed)));
    r.w(kShotKind, right ? 1 : 0xffff);
    cap = 999999;
  } else {
    cap = f2c;
  }
  const int32_t base = clamp(r.i(kSpeed), 0, cap);
  r.i(kSpeed, base);
  if (dist < 0x4b) {
    const int32_t lv = s.g_difficulty;
    bool turn = true;
    if (lv == 0) {
      if (g == mode) {
        r.i(kSpin, r.i(kSpin) / 3);
        turn = false;
      } else if (g == (mode | 1)) {
        r.i(kSpin, shl32(r.i(kSpin), 1));
      }
    }
    if (turn && lie != 1) {
      const int32_t k = ((r.b(kPrefs) & 7) == 7) ? lv + 4 : 9 - lv;
      r.i(kDir, add32(r.i(kDir), shl32(mul32(k, r.i(kSpin)), 1)));
    }
  }
  if (r.b(kKind) != 0 && w.ts(lie, 2) > 0) {
    const int32_t roll = w.rnd(mul32(w.ts(lie, 2), 10));
    if (r.b(0xf8) >= roll) {
      lie = 2;
      r.u(kFlags, r.u(kFlags) | kGolferMishit);
    }
  }
  if (s.unk_005a9cc4 == 0 && lie != 1) {
    const int32_t roll = w.rnd(0x5555);
    const int32_t b2 = w.ts(lie, 2);
    const int32_t sg = sign(r.i(kSpin));
    r.i(kDir, add32(r.i(kDir), mul32(mul32(sg, add32(roll, 0x1555555)), b2)));
    r.i(kSpin, mul32(b2 + 2, r.i(kSpin)) / 2);
  }
  if ((r.b(kFlags) & 1) && r.sb(kStrokes) > 6) {
    r.i(kSpeed, base);
    lie = -1;
  }
  r.i(kSpin, add32(r.i(kSpin), spinAdd));
  {  // the lie switch
    int32_t sp = r.i(kSpeed);
    auto spread = [&](int32_t n, int32_t div) {  // roll = rnd(n), then speed = speed / div - roll + speed
      const int32_t roll = w.rnd(n);
      sp = r.i(kSpeed);
      r.i(kSpeed, add32(sub32(sp / div, roll), sp));
    };
    switch (lie) {
      case -1:
        break;
      case 0:
        spread(sp / 4, 8);
        break;
      case 1:
        spread(sp / 8, 12);
        if (r.b(kClub) == 0xd) r.i(kVz, 0);
        break;
      case 2:
        if (!(w.tflags(bcell) & 0x800)) {
          spread(sp / 3, 6);
          break;
        }
        goto def;
      case 4:
      case 5:
      case 8:
      case 21:
      case 22: {
        spread(sp / 2, 4);
        const int32_t roll = w.rnd(r.i(kSpin) / 2);
        r.i(kSpin, add32(r.i(kSpin), roll));
        if (lie == (s.g_courseType != 1 ? 5 : 4)) r.i(kDir, add32(r.i(kDir), shl32(r.i(kSpin), 2)));
        if (!(s.g_tournamentPrep & 4u)) break;
        sp = r.i(kSpeed);
      }
        [[fallthrough]];
      case 10:
      case 13:
      case 14:
      case 15:
      case 16:
        spread(sp / 2, 4);
        break;
      case 9:
        r.i(kSpeed, w.rnd(shl32(sp, 1)));
        r.i(kDir, add32(r.i(kDir), shl32(r.i(kSpin), 4)));
        break;
      case 11:
      case 12: {
        const int32_t roll = w.rnd(sp);
        sp = r.i(kSpeed);
        r.i(kSpeed, add32(sp / 2, roll));
        break;
      }
      default:
      def : {
        const int32_t roll = w.rnd(sp);
        sp = r.i(kSpeed);
        r.i(kSpeed, add32(sub32(sp / 2, roll), sp));
        break;
      }
    }
  }
  {  // pull toward the base power
    if (((r.b(kPrefs) & 2) && w.ts(lie, 2) <= 0) || (r.b(kFlags) & 1))
      r.i(kSpeed, add32(base, r.i(kSpeed)) / 2);
    const int32_t lv = s.unk_005a9cc4;
    if (lv == 0) r.i(kSpeed, add32(mul32(w.ts(lie, 2), sub32(r.i(kSpeed), base)) / 8, r.i(kSpeed)));
    if (r.i(kSpeed) < base / 3) r.i(kSpeed, base / 3);
    if (lv >= 2) r.i(kSpeed, base);
    if (lv == 1) r.i(kSpeed, add32(r.i(kSpeed), shl32(base, 1)) / 3);
    const int32_t sp = r.i(kSpeed);
    const int32_t c = clamp(bias, -10, 16);
    r.i(kSpeed, add32(mul32(c, sub32(base, sp)) / 16, sp));
    r.i(kSpin, clamp(r.i(kSpin), i32(0xeaaaaaabu), 0x15555555));
  }
  const uint32_t fl = r.u(kFlags) & ~1u;
  r.u(kFlags, fl);
  ctx.mem.store<uint8_t>(w.attr_va(0x11, 2), 8);
  ctx.mem.store<uint8_t>(w.attr_va(0x14, 2), 8);
  return i32(fl);
}

// ================================================================================================== chooseShotTarget
int32_t choose_shot_target(Ctx &ctx, int32_t g) {
  const World w(ctx);
  GameState &s = ctx.gs();
  // A: scores, B: distance from the probe point to the hole, C: the probe angle with bit 0 = "got 25 closer"
  // (21 x 21 cells x {variant 0, 1} x {aim -1, 0, 1})
  std::vector<int32_t> A(0xa56, 0), B(0xa56), C(0xa56, 0);
  int32_t strict = 0;  // [F+0xb4]: never initialised by the original (U-0880)
  s.g_flags |= kFlagAiSearch;
  const Rec r = w.rec(g);
  s.g_shotGolferPrefs = u32(r.b(kPrefs));
  s.unk_0059b738 = 1;
  s.unk_005a9ce4 = 1;
  s.unk_005a9ce8 = 0;
  int32_t bestX = -1;  // F+0x78
  int32_t bestAim = 0;  // F+0x6c
  int32_t bestY = 0, bestHalf = 0;  // F+0xa4, F+0xac
  const int32_t reach = f_rateTile(ctx, {g});  // F+0x4c
  const int32_t entryTX = r.i(kTargetX), entryTY = r.i(kTargetY);
  const int32_t bx = sar32(r.i(kBallX), 10), by = sar32(r.i(kBallY), 10);
  const int32_t ballCell = World::cell(bx, by);
  const int32_t ballType = w.tile(ballCell);
  if (s.unk_005a9cc4 == 0 && !(r.b(kFlags) & 1) && (w.ts(ballType, 2) <= 0 || r.b(kKind) == 0)) s.unk_005a9cc4 = 1;
  int32_t aimMask = w.ts(ballType, 2) > 0 ? 0 : ((r.b(kPrefs) & 4) ? 3 : 0);  // F+0x3c
  if (r.b(kKind) != 0) {
    const int32_t m = r.w(kSkillMask);
    if (!(m & 0x20)) aimMask &= ~1;
    if (!(m & 0x40)) aimMask &= ~2;
  }
  const int32_t hole = r.hole();  // F+0x80 = hole * 0x208, fixed at entry
  r.i(kTargetX, w.hole_x(hole));
  r.i(kTargetY, w.hole_y(hole));
  int32_t holeDist;  // F+0x5c
  {
    const int32_t dx = sar32(mul32(add32(sub32(shl32(r.i(kTargetX), 10), r.i(kBallX)), 0x200), 25), 10);
    const int32_t dy = sar32(mul32(add32(sub32(shl32(r.i(kTargetY), 10), r.i(kBallY)), 0x200), 25), 10);
    holeDist = sqrt_ftol(add32(mul32(dy, dy), mul32(dx, dx)));
    r.i(kDir, angle_fixed(dx, dy));
  }
  int32_t outOfReach = 0;  // F+0x88
  r.b(kStrokes, r.b(kStrokes) + 1);  // rateTile at one stroke more
  if (holeDist > add32(f_rateTile(ctx, {g}), reach)) outOfReach = 1;
  r.b(kStrokes, r.b(kStrokes) - 1);
  int32_t aimX, aimY, div;  // F+0x54, F+0x8c, F+0x68
  {
    const int32_t len = (shl32(clamp(reach - 25, 0, holeDist), 10) + 0x200) / 25;
    aimX = sar32(add32(fold_range(ctx, r.i(kDir), len), r.i(kBallX)), 10);
    const int32_t y0 = r.i(kBallY);
    aimY = sar32(sub32(y0, abs_diff(ctx, r.i(kDir), len)), 10);
    tile_distance(r.i(kBallX), y0, aimX, aimY);                                       // result unused
    distance(sub32(aimX, w.hole_x(r.hole())), sub32(aimY, w.hole_y(r.hole())));       // result unused
    const int32_t d = tile_distance(bx, by, w.hole_x(r.hole()), w.hole_y(r.hole()));
    div = d < 100 ? 2 : 4;
    if (d > 200) div = 6;
    if (SG_NEGCTL(g_negctl_choose_shot_target) == 3) div = 4;
  }
  int32_t trialsRun = 0;                                                   // F+0x44
  int32_t trials = SG_NEGCTL(g_negctl_choose_shot_target) == 1 ? 1 : 2;  // F+0x38
  int32_t alive = 0;                                                       // F+0x34
  int32_t maxB = -1000, minB = 0xffff, minAng = 0x0fffffff, maxAng = i32(0xe0000000u);
  for (;;) {  // one pass
    std::fill(B.begin(), B.end(), 0);
    int32_t best = 99999;  // F+0x60
    for (int32_t ox = -10; ox <= 10; ++ox) {
      const int32_t rowBase = (ox + 10) * 21;
      for (int32_t oy = -10; oy <= 10; ++oy) {
        const int32_t cellIdx = rowBase + oy + 10;
        const int32_t co = cellIdx * 6;
        const int32_t a1 = A[co + 1];
        if (a1 > 99999) continue;
        const int32_t cx = add32(aimX, ox), cy = add32(oy, aimY);
        const int32_t wx = shl32(cx, 10), wy = shl32(cy, 10);  // F+0x84, F+0x74
        const int32_t ex = sar32(mul32(add32(sub32(wx, r.i(kBallX)), 0x200), 25), 10);
        const int32_t ey = sar32(mul32(add32(sub32(wy, r.i(kBallY)), 0x200), 25), 10);
        const int32_t cellDist = sqrt_ftol(add32(mul32(ey, ey), mul32(ex, ex)));  // F+0x7c
        int32_t ang = angle_fixed(ex, ey);                                         // F+0x40
        if (a1 == 0) {  // first visit: vet the cell
          for (int k = 0; k < 6; ++k) A[co + k] = 100000;
          if ((r.b(kFlags) & 1) && cx == entryTX && cy == entryTY) continue;
          if (tile_blocked(ctx, cx, cy)) continue;
          if (cellDist > add32(reach, 16)) continue;
          if (!(cellDist >= reach / 3 || cellDist >= holeDist / 3 || (r.b(kFlags) & 1))) continue;
          if (cx == bx && cy == by) continue;
          const int32_t t = w.tile(World::cell(cx, cy));
          if (t == 0) continue;
          const int32_t t2 = w.ts(t, 2);
          if (t2 > 1) continue;
          int32_t nbOk = 1;
          if (t2 == 1) {
            nbOk = 0;
            for (int32_t k = 0; k < 8; k += 2)
              if (w.ts(w.tile(add32(mul32(add32(w.dx(k), cx), 50), add32(w.dy(k), cy))), 2) <= 0) nbOk = 1;
          }
          const int32_t d1 = distance(sub32(cx, w.hole_x(r.hole())), sub32(cy, w.hole_y(r.hole())));
          const int32_t d2 = distance(sub32(bx, w.hole_x(r.hole())), sub32(by, w.hole_y(r.hole())));
          if (w.tile_u(World::cell(cx, cy)) == 1 && d1 > 3) continue;
          const int32_t e = nbOk ? 1 : d2 / 2;
          if (add32(e, d1) > d2) continue;
          for (int k = 0; k < 6; ++k) A[co + k] = 0;
        }
        // evaluate the cell: variant v (0: the cell centre, 1: its corner) x aim (-1, 0, 1)
        for (int32_t v = 0; v < 2; ++v) {
          if (v != 0 && w.ts(w.tile(ballCell), 2) > 0) continue;
          if (v != 0 && SG_NEGCTL(g_negctl_choose_shot_target) == 2) continue;
          for (int32_t aim = -1; aim <= 1; ++aim) {
            const int32_t idx = v * 3 + 1 + aim + co;
            int32_t &score = A[idx];
            if (score >= 99999) continue;
            if (aim == -1 && !(aimMask & 1)) {
              score = 100000;
              continue;
            }
            if (aim == 1 && !(aimMask & 2)) {
              score = 100000;
              continue;
            }
            if (aim != 0 && cellDist <= 0x4b) {
              score = 100000;
              continue;
            }
            int32_t trial = 0;    // F+0x1c
            int32_t landings = 0;  // F+0x28
            if (trials > 0) {
              int32_t left = trials;
              do {
                if (v == 0) {
                  trial_shot(ctx, g, add32(wx, 0x200), add32(wy, 0x200), aim);
                  ++trialsRun;
                } else {
                  trial_shot(ctx, g, shl32(cx, 10), shl32(cy, 10), aim);
                }
                if (r.b(kPrefs) & 4) {  // the 8 cells around the landing point
                  int32_t sc = trial;
                  for (int32_t k = 0; k < 8; ++k) {
                    const int32_t dv = 3 + (k & 1);
                    const int32_t px = sar32(add32(shl32(w.dx(k), 10) / dv, s.g_shot56a78c), 10);
                    const int32_t py = sar32(add32(shl32(w.dy(k), 10) / dv, s.g_shot56a790), 10);
                    if (tile_blocked(ctx, px, py)) {
                      sc = add32(sc, w.ts(0x14, 2));
                    } else {
                      const int32_t ix = World::cell(px, py);
                      sc = add32(sc, w.ts(w.tile(ix), 2));
                      if (w.tflags(ix) & 0x80) sc = sub32(sc, 1);
                    }
                  }
                  trial = sc;
                }
                const int32_t lx = sar32(s.g_shot56a78c, 10), ly = sar32(s.g_shot56a790, 10);
                const int32_t landCell = World::cell(lx, ly);
                int32_t lv = w.ts(w.tile(landCell), 2);
                if (tile_blocked(ctx, lx, ly) || (w.tflags(landCell) & 0x400)) {
                  lv = w.ts(0x14, 2);
                } else if (w.tflags(landCell) & 0x80) {
                  if (static_cast<int16_t>(w.tflags(landCell) & 0x1f) == static_cast<int16_t>(r.hole()))
                    --lv;
                  else
                    lv += 2;
                }
                if (ang & 1) ang ^= 1;
                if (w.ts(w.tile(landCell), 2) <= 0) ++landings;
                int32_t td;
                if (v == 0)
                  td = tile_distance(add32(wx, 0x200), add32(wy, 0x200), w.hole_x(r.hole()), w.hole_y(r.hole()));
                else
                  td = tile_distance(shl32(cx, 10), shl32(cy, 10), w.hole_x(r.hole()), w.hole_y(r.hole()));
                B[idx] = td;
                {
                  const int32_t t2 = tile_distance(s.g_shot56a78c, s.g_shot56a790, w.hole_x(r.hole()),
                                                   w.hole_y(r.hole()));
                  const int32_t b = B[idx];
                  if (sub32(b, t2) > 25 && b > shl32(t2, 1)) ang |= 1;
                }
                C[idx] = ang;
                {
                  const int32_t t3 = tile_distance(s.g_shot56a78c, s.g_shot56a790, w.hole_x(hole), w.hole_y(hole));
                  trial = add32(add32(div32(t3, div), trial), mul32(lv, SG_NEGCTL(g_negctl_choose_shot_target) == 4 ? 4 : 8));
                }
                if (trials == 4 && s.unk_005a9cc4 != 2 && (r.b(kPrefs) & 4) && outOfReach == 0) {
                  const int32_t hx = w.hole_x(r.hole()), hy = w.hole_y(r.hole());
                  if (lv > 0) {
                    const int32_t sc = f_scanTileLine(ctx, {s.g_shot56a78c, s.g_shot56a790, hx, hy, reach, 0, 0});
                    trial = add32(trial, mul32(sc, 2));
                  } else {
                    const int32_t t4 = tile_distance(s.g_shot56a78c, s.g_shot56a790, hx, hy);
                    int32_t rch = reach;
                    if (r.b(kStrokes) == 0) rch = sub32(rch, rch / 5);
                    if (!SG_DEFECT(g_defect_strict_sticky)) strict = 0;
                    if ((r.b(kPrefs) & 4) && w.ts(w.tile(landCell), 2) <= 0 && t4 >= 0x32 &&
                        div32(mul32(sub32(rch, t4), 60), mul32(rch, 3)) >= 4)
                      strict = 1;
                    int32_t m = f_scanTileLine(ctx, {s.g_shot56a78c, s.g_shot56a790, w.hole_x(r.hole()),
                                                     w.hole_y(r.hole()), rch, 0, strict});
                    if (aimMask & 1) {
                      const int32_t sc = f_scanTileLine(ctx, {s.g_shot56a78c, s.g_shot56a790, w.hole_x(r.hole()),
                                                              w.hole_y(r.hole()), rch, -1, 0});
                      if (sc < m) m = sc;
                    }
                    if (aimMask & 2) {
                      const int32_t sc = f_scanTileLine(ctx, {s.g_shot56a78c, s.g_shot56a790, w.hole_x(r.hole()),
                                                              w.hole_y(r.hole()), rch, 1, 0});
                      if (sc < m) m = sc;
                    }
                    trial = add32(trial, m / 2);
                  }
                }
              } while (--left != 0);
            }
            // accumulate, record the best
            const int32_t a = add32(A[idx], trial);
            A[idx] = a;
            if (landings >= div32(mul32(add32(s.g_difficulty, 4), trials), 8)) s.unk_005a9ce4 = 0;
            if (a < best) {
              bestY = cy;
              s.g_shotTargetY = s.g_shot56a790;
              bestX = cx;
              s.unk_005a9ce8 = v;
              s.g_shotTargetX = s.g_shot56a78c;
              bestAim = aim;
              best = a;
              bestHalf = C[idx] & 1;
            }
            if (s.g_debugShotGolfer == g) {  // debug overlay: the score text (drawing dropped, U-0881)
              const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
              const uint32_t num = state_va(ctx, &GameState::g_textBuf);
              ctx.mem.store<uint8_t>(text, 0);
              vitoa10(ctx.mem, num, trial);
              vstrcat(ctx.mem, text, num);
            }
          }
        }
      }
    }
    // prune the cells, then decide whether to run another pass with twice the trials
    if (trials >= 8 || s.unk_005a9cc4 == 2) break;
    int32_t tol = SG_NEGCTL(g_negctl_choose_shot_target) == 5 ? 0x40 : 0x80;  // F+0x40
    for (;;) {
      minB = 0xffff;
      const int32_t adx = add32(sub32(shl32(w.hole_x(r.hole()), 10), r.i(kBallX)), 0x200);
      const int32_t ady = add32(sub32(shl32(w.hole_y(r.hole()), 10), r.i(kBallY)), 0x200);
      alive = 0;
      maxB = -1000;
      minAng = 0x0fffffff;
      maxAng = i32(0xe0000000u);
      const int32_t holeAng = angle_fixed(adx, ady);  // F+0x64
      int32_t row = -10;                               // F+0x28
      int32_t rowBase = 0;                             // F+0x24
      do {
        for (int32_t col = 0; col - 10 <= 10; ++col) {
          int32_t dead = 0;
          const int32_t c = rowBase + col;
          if (A[c * 6 + 1] > 99999) continue;
          for (int32_t k = 0; k < 6; ++k) {
            const int32_t slot = c * 6 + k;
            const int32_t a = A[slot];
            if (a >= 99999) {
              ++dead;
            } else if (a > add32(tol, best)) {
              A[slot] = 99999;
              ++dead;
            } else {
              ++alive;
              if (trials == 4) {
                const int32_t b = B[slot];
                if (b != 0) {
                  if (b < minB) minB = b;
                  if (b > maxB) maxB = b;
                  const int32_t td =
                      tile_distance(r.i(kBallX), r.i(kBallY), add32(row, aimX), sub32(add32(col, aimY), 10));
                  if (td > 100) {
                    const int32_t an = sub32(C[slot], holeAng);
                    if (an < minAng) minAng = an;
                    if (an > maxAng) maxAng = an;
                  }
                }
              }
            }
          }
          if (dead == 6) A[c * 6 + 1] = 100000;
        }
        ++row;
        rowBase += 21;
      } while (rowBase <= 0x1a4);
      // (pumpInput483bd0: the message pump runs here in the original; UI, dropped, U-0881)
      if (alive > 4 && tol > 16 && add32(mul32(mul32(alive, trials), 3), trialsRun) > 0xfa) {
        tol /= 2;
        continue;
      }
      break;
    }
    if (trials == 4) {
      s.g_shotFlags5a7140 = 0;
      if (sub32(maxB, minB) > 0x4b)
        s.g_shotFlags5a7140 = 1;
      else if (sub32(maxAng, minAng) > 0x15555555)
        s.g_shotFlags5a7140 = 2;
    }
    trials *= 2;
    if (alive <= 1) break;
  }
  // (debug redraw when g_debugShotGolfer == g: Window::refreshFull + pumpAndRedraw(0), UI only: dropped, U-0881)
  int32_t ret;
  if (bestX != -1) {
    r.i(kTargetX, bestX);
    r.i(kTargetY, bestY);
    if (bestHalf != 0) s.g_shotFlags5a7140 |= 4u;
    ret = bestAim;
  } else {
    s.unk_005a9ce8 = 0;
    r.i(kTargetX, w.hole_x(hole));
    r.i(kTargetY, w.hole_y(hole));
    ret = 0;
  }
  s.g_flags &= ~kFlagAiSearch;
  if (s.unk_005a9cc4 != 2) s.unk_005a9cc4 = 0;
  s.g_shotGolferPrefs = 7;
  return ret;
}

// ================================================================================================== scanShotArea
void scan_shot_area(Ctx &ctx, int32_t slot) {
  const World w(ctx);
  GameState &s = ctx.gs();
  if (s.g_zoomScale < 4) return;
  const int32_t e = s.f_w_4c2e00[3] & 0xff;
  const int32_t z = static_cast<int16_t>(ctx.mem.load<uint16_t>(state_va(ctx, &GameState::g_storyPairs) +
                                                                 u32(slot) * 904u));
  if (z == e) return;
  const Rec r = w.rec(z);
  if ((r.b(kKind) & 0xe0) == 0x20 && z != s.g_playerGolfer) return;
  const int32_t p0 = r.i(0xc4), p1 = r.i(0xc8);
  const int32_t q0 = r.i(kBallX), q1 = r.i(kBallY);
  const int32_t D = mul32(distance(sub32(p0, q0), sub32(p1, q1)), 25) / 1024;
  const int32_t hole = r.hole();
  const int32_t R = tile_distance(q0, q1, w.hole_x(hole), w.hole_y(hole));
  int32_t k = R;
  const int32_t T = type_at_pos(ctx, p0, p1);
  int32_t S = 0;
  if (z == s.g_playerGolfer)
    for (uint32_t i = 0; i < 10; ++i) S += r.b(kSkill + i);
  const int32_t U = type_at_pos(ctx, r.i(kBallX), r.i(kBallY));
  const int32_t attrU = w.ts(U, 2);
  if (attrU > 0) {
    if (z != s.g_playerGolfer) return;
    const int32_t n = div32(mul32(w.ts(T, 2) + 4, 250), add32(s.g_difficulty, 2));
    if (w.rnd(n) >= attrU * S) return;
    narrate_shot(ctx, slot, R, D, 1);
    return;
  }
  const int32_t b = r.sb(kShotValue);
  int32_t v = 0x14;
  if (b < 5) v = 0x19;
  if (b < 10) v += 5;
  if (b > 10) v -= 5;
  if (k == 0) {
    v /= 2;
  } else if (k > 0x19 && b > 0) {
    const int32_t W2 = type_at_pos(ctx, r.i(kBallX), r.i(kBallY));
    if (!(w.ts(W2, 2) > 0)) {
      k = 0x19;
      v = div32(200, r.sb(kShotValue));
    }
  }
  if (z == s.g_playerGolfer) {
    const int32_t at = w.ts(T, 2);
    if (at > 0) {
      const int32_t prod = mul32(clamp(at, 0, 3), v);
      int32_t hi = i32(static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(i32(0xd5555555u)) * prod) >> 32));
      hi = add32(hi, i32(u32(hi) >> 31));
      v = add32(v, hi);
    }
    const int32_t roll = w.rnd(S);
    const int32_t x = mul32(add32(add32(roll, mul32(s.g_difficulty, 4)), 5), v);
    v = x / 32;
    if (f_holeQuadrants(ctx, {r.i(kX), r.i(kY), 4}) != 0) v /= 2;
  }
  const int32_t lim = SG_NEGCTL(g_negctl_scan_shot_area) == 1 ? mul32(k, v) : mul32(add32(k, 1), v);
  if (D > lim || ((s.g_forcedEventBits & 0x10u) && (z == s.g_playerGolfer || s.g_playerGolfer == -1))) {
    narrate_shot(ctx, slot, R, D, 0);
    s.g_forcedEventBits &= 0xffffffefu;
  }
}

// ================================================================================================== narrateShot
// A straddler (re/port/scope_straddlers.tsv) assigned to 3.4. Kept: every write to simulation state (the snapshot
// records 152 / 153, the hole flags, the skill reward and its RNG draws, the golfer fields, f_w_4c2e00[2], the text
// buffer) and the calls that write state (buildGolferName, appendHoleName, appendClubName, postEvent, startMessage,
// rateTile, emitGolferThought, worldToScreen's result; startMessage through start_message). Dropped (UI / audio, no simulation state, U-0883): the two flight
// lines (setLinePoint / lineTo), drawWindow, fillWindowRect, Widget_fillRect, drawCenteredText, drawTextSurface,
// playSound, Window::refreshFull, the modal pumpAndRedraw(0) and saveBmp. The no-frame exit returns playSound's eax in
// the original; the port returns 0 there (the only caller, scanShotArea, ignores the result).
int32_t narrate_shot(Ctx &ctx, int32_t shot, int32_t dist, int32_t yards, int32_t flag) {
  const World w(ctx);
  GameState &s = ctx.gs();
  const Memory &m = ctx.mem;
  const uint32_t S = state_va(ctx, &GameState::g_storyPairs) + u32(mul32(shot, 0x388));
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t num = state_va(ctx, &GameState::g_textBuf);
  const uint32_t poolA = state_va(ctx, &GameState::f_strpool_4c4f98), poolB = state_va(ctx, &GameState::f_strpool_4c4ed0),
                 poolC = state_va(ctx, &GameState::f_strpool_4c4e64), poolPos = state_va(ctx, &GameState::f_strpool_4c4f90);
  auto cat = [&](uint32_t src) { vstrcat(m, text, src); };
  auto catNum = [&](int32_t v) {
    vitoa10(m, num, v);
    cat(num);
  };
  auto name = [&](int32_t who) { f_buildGolferName(ctx, {who, 0}); };
  auto holeFlag = [&](const Rec &r) {
    return state_va(ctx, &GameState::g_golferTypeState) + 3u + u32(mul32(0x2c, r.sw(kType))) + u32(r.hole());
  };
  const int32_t g = static_cast<int16_t>(m.load<uint16_t>(S));
  int32_t who = g;
  const Rec R = w.rec(g);
  int32_t minX = R.i(8), maxX = minX, minY = R.i(0xc), maxY = minY;
  {
    const Rec P = w.rec(R.sw(kPartner));
    const int32_t px = P.i(8), py = P.i(0xc);
    if (!(px >= minX)) minX = px;
    if (!(px <= maxX)) maxX = px;
    if (!(py >= minY)) minY = py;
    if (!(py <= maxY)) maxY = py;
  }
  // (flag 0, not the followed golfer: two calls of emptyFunc_4a0890, a lone `ret`)
  int32_t ok = 1;
  if (minX <= 0 || minY <= 0) {
    if (g != s.g_playerGolfer) {
      const uint32_t f = holeFlag(R);
      m.store<uint8_t>(f, static_cast<uint8_t>(m.load<uint8_t>(f) | (SG_NEGCTL(g_negctl_narrate_shot) == 2 ? 2u : 1u)));
      return i32(f);
    }
    ok = 0;
  } else {
    rep_movs(m, w.golfers + 0x98u * 0x100u, S + 0x188, 0x100);
    who = 0x98;
    rep_movs(m, w.golfers + 0x99u * 0x100u, S + 0x288, 0x100);
    w.rec(0x98).w(kPartner, 0x99);
    int32_t sx = 0, sy = 0;
    const int32_t n = static_cast<int16_t>(m.load<uint16_t>(S + 4));
    for (int32_t i = 0; i < n; ++i)  // the shadow line (drawing dropped); worldToScreen still runs for its out values
      world_to_screen(ctx, m.load<int32_t>(S + 8 + 4u * u32(i)), m.load<int32_t>(S + 0x88 + 4u * u32(i)), sx, sy, 0);
    for (int32_t i = 0; i < static_cast<int16_t>(m.load<uint16_t>(S + 4)); ++i) {
      if (world_to_screen(ctx, m.load<int32_t>(S + 8 + 4u * u32(i)), m.load<int32_t>(S + 0x88 + 4u * u32(i)), sx, sy,
                          0) == 0)
        continue;
      const int32_t v = mul32(mul32(m.load<int32_t>(S + 0x108 + 4u * u32(i)), s.f_w_4c2e00[0]), s.g_zoomScale);
      sy = add32(sy, neg32(v / 4) / 16);
      if (!(sx >= minX)) minX = sx;
      if (!(sx <= maxX)) maxX = sx;
      if (!(sy >= minY)) minY = sy;
      if (!(sy <= maxY)) maxY = sy;
    }
  }
  // the sentence
  m.store<uint8_t>(text, 0);
  name(g);
  const int32_t tileType = type_at_pos(ctx, R.i(kBallX), R.i(kBallY));
  const Rec W = w.rec(who);
  const int32_t club = W.sb(kClub);
  if (flag != 0) {
    const int32_t par = W.sb(kHole) % 2;
    if (club > 4) {
      if (par == 0)
        cat(poolA + 0x128);
      else if (par == 1)
        cat(poolA + 0x134);
    } else {
      if (par == 0)
        cat(poolA + 0x110);
      else if (par == 1)
        cat(poolA + 0x11c);
    }
    append_club_name(ctx, club);
    cat(poolA + 0x104);
    cat(w.attrs + u32(mul32(0x30, tileType)));  // the tile type's name
  } else {
    if (club == 0)
      cat(poolA + 0xf4);
    else if (club < 7)
      cat(poolA + 0xe8);
    else
      cat(poolA + (club != 0xd ? 0xdcu : 0xd0u));
    const int32_t lie = W.sb(kShotValue);
    if (lie > 0xa) cat(poolA + 0xc8);
    if (lie == 0xa) cat(poolA + 0xc0);
    if (club == 0) {
      cat(poolA + 0xa8);
    } else if (club == 0xd) {
      cat(poolA + 0xb8);
    } else {
      append_club_name(ctx, club);
      cat(poolA + 0xb0);
    }
    catNum(yards);
    cat(poolA + 0xa0);
    uint32_t tail;
    if (dist != 0) {
      if (type_at_pos(ctx, R.i(kBallX), R.i(kBallY)) == 1) {
        cat(poolA + 0x94);
        if (dist <= 7) {
          catNum(mul32(dist, 2));
          cat(poolA + 0x8c);
        } else {
          catNum(dist);
          cat(poolA + 0x84);
        }
        tail = poolA + 0x78;
      } else {
        tail = poolA + 0x60;
      }
    } else {
      tail = poolA + 0x38;
      if (R.i(kBallX) == add32(shl32(w.hole_x(R.hole()), 10), 0x200) &&
          R.i(kBallY) == add32(shl32(w.hole_y(R.hole()), 10), 0x200))
        tail = poolA + 0x50;
    }
    cat(tail);
  }
  cat(poolA + 0x30);
  f_appendHoleName(ctx, {W.sb(kHole)});
  cat(poolA + 0x2c);
  if (flag == 0) {
    const int32_t p = W.sw(kPartner);
    if (w.rec(p).sb(kHole) > 0) {
      const uint32_t kind = u32(W.sw(0xae));
      auto possessive = [&] { cat(poolPos + (f_typeBit7Clear(ctx, {W.sw(kPartner)}) != 0 ? 4u : 0u)); };
      if (kind <= 5) {
        switch (kind) {
          case 0:
            name(p);
            cat(poolA + 0x14);
            break;
          case 1:
            name(p);
            cat(poolB + 0x84);
            break;
          case 2:
            cat(poolB + 0x78);
            name(p);
            cat(poolB + 0x68);
            cat(poolB + (f_typeBit7Clear(ctx, {W.sw(kPartner)}) != 0 ? 0x60u : 0x58u));
            cat(state_va(ctx, &GameState::f_strpool_4c4944));
            break;
          case 4:
            name(p);
            cat(poolA);
            possessive();
            cat(poolB + 0xb8);
            break;
          default:
            name(p);
            cat(poolB + 0xa8);
            possessive();
            cat(poolB + 0x9c);
            break;
        }
      }
    }
  }
  // the box must fit the screen
  if (!(minX >= 0x38)) {
    if (!(minX >= 0))
      ok = 0;
    else
      minX = 0x38;
  }
  if (!(minY >= 0x38)) {
    if (!(minY >= 0))
      ok = 0;
    else
      minY = 0x38;
  }
  if (!(maxX <= 0x2f8)) ok = 0;
  const int32_t followed = s.g_playerGolfer;
  if (!(maxY <= 0x230)) {
    ok = 0;
    if (g != followed) return maxY;
  } else if (ok == 0) {
    if (g != followed) return maxY;
  }
  // the skill reward
  int32_t savedMask = W.sw(kSkillMask);
  W.w(kSkillMask, 0xffff);
  {
    const int32_t sg = static_cast<int16_t>(m.load<uint16_t>(S));
    bool reward;
    if (sg == followed)
      reward = true;
    else if ((s.g_flags & 0x200000u) != 0)
      reward = false;
    else
      reward = sg == (followed ^ 1);
    if (reward) {
      const uint32_t table = sg != followed ? 0x10u : 0u;
      const int32_t bucket = bucket_value(sub32(s.g_holesPlusOne, 1));
      int32_t cap = add32(mul32(bucket, 2), SG_NEGCTL(g_negctl_narrate_shot) == 1 ? 5 : 6);
      if (bucket == 3) cap = 0x63;
      const uint32_t lv = state_va(ctx, &GameState::g_playerSkills) + table;
      for (uint32_t k = 0; k < 12; ++k)
        if (!(static_cast<int8_t>(m.load<uint8_t>(lv + k)) < cap)) W.w(kSkillMask, W.w(kSkillMask) & ~i32(1u << k));
      if (flag != 0) {
        W.w(kSkillMask, 0);
        for (uint32_t k = 0; k < 12; ++k)
          if (static_cast<int8_t>(m.load<uint8_t>(lv + k)) > 3) W.w(kSkillMask, W.w(kSkillMask) | i32(1u << k));
      }
      int32_t tries = 0;
      const int32_t target = type_at_pos(ctx, W.i(0xc4), W.i(0xc8));
      int32_t pick;
      if (target == 1 && (W.b(kSkillMask) & 0x10) != 0) {
        pick = 0x10;
      } else {
        for (;;) {
          pick = 0;
          const int32_t roll = w.rnd(0xc);
          const int32_t mk = W.w(kSkillMask);
          if (roll <= 9) {
            switch (roll) {
              case 0:
                if (w.ts(target, 2) > 0 && (mk & 0x100)) pick = 0x100;
                break;
              case 1:
                if (((W.b(kFlags) & 2) != 0 || dist == 0) && (mk & 0x200)) pick = 0x200;
                break;
              case 2:
                if (W.b(kStrokes) == 0 && W.sb(kClub) < 2 && (W.b(kSkillMask) & 2)) pick = 2;
                break;
              case 3:
                if (W.b(kStrokes) == 0 && W.sb(kClub) <= 3 && (W.b(kSkillMask) & 4)) pick = 4;
                break;
              case 4:
                if (W.sw(kShotKind) == 1 && (W.b(kSkillMask) & 0x20)) pick = 0x20;
                break;
              case 5:
                if (W.sw(kShotKind) == -1 && (W.b(kSkillMask) & 0x40)) pick = 0x40;
                break;
              case 6:
                if (W.sw(kShotKind) == 3 && (W.b(kSkillMask) & 0x80)) pick = 0x80;
                break;
              case 7:
                if (yards > f_rateTile(ctx, {who}) && (W.b(kSkillMask) & 1)) pick = 1;
                break;
              case 8:
                if (W.sb(kClub) >= 3 && W.sb(kClub) < 0xb) pick = 8;
                break;
              default:
                if (tries > 0x32 || dist == 0) {
                  if (type_at_pos(ctx, W.i(kBallX), W.i(kBallY)) != 1)
                    pick = 0x200;
                  else
                    pick = dist != 0 ? 0x10 : 0x200;
                }
                break;
            }
          }
          ++tries;
          if (tries >= 100) break;
          if (pick == 0) continue;
          if ((pick & W.sw(kSkillMask)) != 0) break;
        }
      }
      bool restore = false;
      if (flag != 0) {
        if (pick == 0) {
          if (!SG_DEFECT(g_defect_mask_not_restored)) W.w(kSkillMask, savedMask);
          return 0;
        }
      } else if (pick == 0) {
        restore = true;
      }
      if (!restore && W.b(kKind) == 0) restore = true;
      if (!restore) {
        int32_t k = 0;
        if ((pick & 1) == 0) do {
            ++k;
          } while ((pick & shl32(1, k)) == 0);
        const uint32_t lvk = lv + u32(k);
        const uint32_t skillName = m.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c2c3c) + 4u * u32(k));
        if (flag != 0) {  // a missed shot costs a level
          const int32_t cur = static_cast<int8_t>(m.load<uint8_t>(lvk));
          if (cur <= 2) {
            if (!SG_DEFECT(g_defect_mask_not_restored)) W.w(kSkillMask, savedMask);
            return i32((table & 0xffffff00u) | zx8(u32(cur)));
          }
          m.store<uint8_t>(lvk, static_cast<uint8_t>(cur - 1));
          cat(state_va(ctx, &GameState::f_strpool_4c3f70));
          name(who);
          cat(poolB + (f_typeBit7Clear(ctx, {who}) != 0 ? 0x44u : 0x30u));
          cat(skillName);
          cat(poolB + 0x28);
        } else if (static_cast<int8_t>(m.load<uint8_t>(lvk)) < 0xa) {
          cat(state_va(ctx, &GameState::f_strpool_4c3f70));
          name(who);
          const int32_t cur = static_cast<int8_t>(m.load<uint8_t>(lvk));
          if (cur == 0) {  // a new skill
            m.store<uint8_t>(lvk, static_cast<uint8_t>(clamp(sub32(3, s.g_difficulty), 1, 3)));
            cat(poolC);
            cat(skillName);
            cat(state_va(ctx, &GameState::f_strpool_4c4e60));
            savedMask |= shl32(1, k);
            if (g == s.g_playerGolfer) {
              const uint32_t mw = state_va(ctx, &GameState::g_skillMaskBlock) + 8u;
              m.store<uint16_t>(mw, static_cast<uint16_t>(m.load<uint16_t>(mw) | (1u << k)));
            }
            R.w(kSkillMask, R.w(kSkillMask) | i32(1u << k));
          } else {  // a level up
            const int32_t nv = static_cast<int8_t>(cur + 1);
            m.store<uint8_t>(lvk, static_cast<uint8_t>(nv));
            cat(poolB + 0x20);
            cat(skillName);
            cat(poolB + 0x8);
            catNum(nv);
            cat(poolB);
            if (static_cast<int8_t>(m.load<uint8_t>(lvk)) == cap) {
              cat(poolC + 0x24);
              cat(m.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c2a18) + 4u + u32(mul32(4, bucket))));
              cat(poolC + 0x18);
            }
          }
          const int32_t fw = s.g_playerGolfer;
          if (g == fw) {
            w.rec(fw).b(kSkill + u32(k), m.load<uint8_t>(lvk));
            f_postEvent(ctx, {2, W.i(kX), W.i(kY)});
            // (playSound(0x2f, 100, 0, 0, 0): audio, dropped)
          }
        }
      }
    }
  }
  W.w(kSkillMask, savedMask);
  if (ok == 0) {
    start_message(ctx, i32(0x800023e8u), 1, g);
    return 0;  // the original returns playSound(0x95, ...)'s eax here (U-0883)
  }
  // the SimFoto frame (drawing dropped)
  int32_t any = 1;
  {
    uint32_t q = state_va(ctx, &GameState::g_golferTypeState) + 3u + u32(R.hole());
    for (int t = 0; t < 0x4c; ++t, q += 0x2c)
      if (m.load<uint8_t>(q) & 1) any = 0;
  }
  const int32_t fw = s.g_playerGolfer;
  bool frame;
  if ((s.g_preferences & 2u) == 0) {
    any = 0;
    frame = g == fw;
  } else if (any == 0 || fw != -1) {
    frame = g == fw;
  } else {
    frame = true;
  }
  if (frame) {
    s.f_w_4c2e00[2] = g;  // read by drawWindow
    s.f_w_4c2e00[2] = -1;
  }
  {
    const int32_t d = s.g_date % 8192;
    m.store<uint8_t>(text, 0);
    catNum(add32(d / 1024, 3));
    cat(state_va(ctx, &GameState::f_strpool_4c4e54));
    catNum(mul32(d % 1024, 30) / 1024 + 1);
    cat(state_va(ctx, &GameState::f_strpool_4c4e54));
    catNum(add32(s.g_year, 0x7d1));
  }
  {
    const uint32_t f = holeFlag(R);
    m.store<uint8_t>(f, static_cast<uint8_t>(m.load<uint8_t>(f) | 1u));
  }
  if ((any != 0 && s.g_playerGolfer == -1) || g == s.g_playerGolfer) {
    // (pumpAndRedraw(0): the modal wait, dropped; the key it leaves decides the SimFoto save)
    if (m.load<int32_t>(state_va(ctx, &GameState::g_rootInput) + 0x3c) == 0x79) {
      const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c4970);
      vstrcpy(m, text, pool + 0x4d8);
      name(who);
      catNum(s.g_date);
      cat(pool + 0x4d0);
      // (saveBmp(text, box +- 0x2a): a file write, dropped)
    }
  }
  R.u(kFlags, R.u(kFlags) | 0x20000u);
  if (flag == 0) {
    if ((R.b(0x70) & 0x7f) == 1) return 0;
    const int32_t r = f_emitThought(ctx, {g, 1, 1});
    R.b(0x1d, 0xc);
    R.w(0x9e, 0xff9d);
    R.b(0x1a, (R.b(0x1a) - 2) & 7);
    return r;
  }
  const int32_t b = R.sb(kMoodByte);
  if (!(b < 0)) R.b(kMoodByte, 0xff);
  return i32((u32(flag) & 0xffffff00u) | zx8(u32(b)));
}

// ================================================================================================== adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_shot_power(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(shot_power(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_wobble(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(wobble(c)); }
void a_trial_shot(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(trial_shot(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }
void a_choose_shot_target(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(choose_shot_target(c, S(a, 0))); }
void a_narrate_shot(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(narrate_shot(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)));
}
void a_scan_shot_area(Ctx &c, const uint32_t *a, Ret &) { scan_shot_area(c, S(a, 0)); }
void a_prepare_shot(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(prepare_shot(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4)));
}

const FuncDef kFuncs[] = {
    {"shotPower", "default", "int", 3, 0, a_shot_power, &g_negctl_shot_power},
    {"wobble", "default", "int", 0, 0x027F, a_wobble, &g_negctl_wobble},
    {"trialShot", "default", "int", 4, 0x027F, a_trial_shot, &g_negctl_trial_shot},
    {"chooseShotTarget", "default", "int", 1, 0x027F, a_choose_shot_target, &g_negctl_choose_shot_target},
    {"prepareShot", "default", "int", 5, 0x027F, a_prepare_shot, &g_negctl_prepare_shot},
    {"narrateShot", "default", "int", 4, 0x027F, a_narrate_shot, &g_negctl_narrate_shot},
    {"scanShotArea", "default", "void", 1, 0x027F, a_scan_shot_area, &g_negctl_scan_shot_area},
};

const DefectDef kDefects[] = {
    {"shotPower.cacheIgnoresPutt", "model_defects#18", "the result cache key leaves out the putt flag",
     &g_defect_shot_power_putt},
    {"chooseShotTarget.strictFlagSticky", "model_defects#37",
     "the strict-scan flag is never initialised or cleared within a search", &g_defect_strict_sticky},
    {"prepareShot.hazardCellTransposed", "model_defects#38", "the hazard cell passed to the thoughts is y * 50 + x",
     &g_defect_hazard_transposed},
    {"narrateShot.maskNotRestored", "model_defects#39",
     "two missed-shot exits skip restoring the named golfer's skill mask", &g_defect_mask_not_restored},
};
}  // namespace

extern const ModuleDef kModule_shots = {"shots", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                        sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
