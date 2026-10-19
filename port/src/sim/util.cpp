// Utility leaves of the simulation core (see portsim/sim/util.h). Phase 2.6 wrote Random::next, Random::range, clamp and
// distance; phase 3.0 added the direction, angle, sine, decay and bucket leaves. Written from the specification
// (analysis notes + vector corpus); no original address or code.
#include "portsim/sim/util.h"

#include <cmath>

#include "portsim/execonst.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   Random::range   1 = results 0 and 1 swapped, 2 = scaled by n + 1, 3 = rounded to nearest instead of truncated
//   sign            1 = 0 maps to 1
//   approxDistance  1 = halves with an arithmetic shift (floor), 2 = max + min / 2 without the shared truncation
//   direction8      1 = the |b| dominant test uses >= (ties change), 2 = quadrants a>0 b<=0 and a<=0 b>0 swapped
//   directionOf     1 = the tie |dx| == |dy| takes the horizontal axis
//   angleFixed      1 = coefficient 10 instead of 11, 2 = the a == 0 axis results swapped
//   sinScaled       1 = no interpolation (table entry only), 2 = the middle amp range shifted 16 + 0
//   foldRange       1 = bit 30 reflected as 0x40000000 - a
//   absDiff         1 = phase offset 0x3fffffff
//   bucketValue     1 = 18 maps to -1
//   decaySum        1 = x >> 3 (floor) instead of x / 8, 2 = loop bound 0x41 (BLIND: reads GREEN on the vectors)
//   decaySum2       1 = y step 0x7f
//   tileDistance    1 = scale 25 / 1000
//   cosScaled       1 = phase offset 0x40000000
//   initSinTable    1 = entries shifted by one step
int g_negctl_random_range = 0;
int g_negctl_sign = 0;
int g_negctl_approx_distance = 0;
int g_negctl_direction8 = 0;
int g_negctl_direction_of = 0;
int g_negctl_angle_fixed = 0;
int g_negctl_sin_scaled = 0;
int g_negctl_fold_range = 0;
int g_negctl_abs_diff = 0;
int g_negctl_bucket_value = 0;
int g_negctl_decay_sum = 0;
int g_negctl_decay_sum2 = 0;
int g_negctl_tile_distance = 0;
int g_negctl_cos_scaled = 0;
int g_negctl_init_sin_table = 0;

// Compatibility switches (port/CORE.md "Defects"), 1 = behave like the original.
//   sinScaled.readPastTable  model_defects#10: at index 255 the interpolation reads the dword after the 256-entry table
//                            (0 in the original); fixed: the segment is flat (entry 255 twice).
int g_defect_sin_past_table = 1;
}  // namespace

double random_next(uint32_t &seed) {
  seed = seed * 0x41C64E6Du + 0x3039u;
  return static_cast<double>((seed >> 16) & 0x7FFFu) * (1.0 / 32768.0);
}

uint32_t random_range(uint32_t &seed, uint32_t n) {
  const uint32_t count = n & 0xFFFFu;
  const double r = random_next(seed);
  switch (SG_NEGCTL(g_negctl_random_range)) {
    case 1: {
      uint32_t v = ftol_low32(r * static_cast<double>(count));
      return v == 0 ? 1u : v == 1 ? 0u : v;
    }
    case 2:
      return ftol_low32(r * static_cast<double>(count + 1));
    case 3:
      return ftol_low32(std::floor(r * static_cast<double>(count) + 0.5));
    default:
      return ftol_low32(r * static_cast<double>(count));
  }
}

int32_t clamp(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) v = lo;
  if (v > hi && hi >= lo) v = hi;
  return v;
}

int32_t distance(int32_t dx, int32_t dy) {
  int32_t scale = 1;
  if (abs32(dx) > 0x4000) {
    scale = 8;
    dx /= 8;
  }
  if (abs32(dy) > 0x4000) {
    dy /= 8;
    scale *= 8;
  }
  const int32_t sum = add32(mul32(dx, dx), mul32(dy, dy));
  return static_cast<int32_t>(ftol_low32(std::sqrt(static_cast<double>(sum)) * scale));
}

int32_t sign(int32_t v) {
  if (SG_NEGCTL(g_negctl_sign) == 1 && v == 0) return 1;
  if (v > 0) return 1;
  return v >= 0 ? 0 : -1;
}

int32_t approx_distance(int32_t dx, int32_t dy) {
  const int32_t a = dx < 0 ? neg32(dx) : dx, b = dy < 0 ? neg32(dy) : dy;
  switch (SG_NEGCTL(g_negctl_approx_distance)) {
    case 1:
      return a > b ? sar32(add32(b, mul32(a, 2)), 1) : sar32(add32(a, mul32(b, 2)), 1);
    case 2:
      return a > b ? add32(a, b / 2) : add32(b, a / 2);
    default:
      return a > b ? add32(b, mul32(a, 2)) / 2 : add32(a, mul32(b, 2)) / 2;
  }
}

int32_t direction8(int32_t a, int32_t b) {
  // {default, |a| > 2|b|, |b| > 2|a|} per sign quadrant; the second test wins when both hold
  static const int32_t kOct[4][3] = {{3, 2, 4}, {1, 2, 0}, {5, 6, 4}, {7, 6, 0}};
  int q = (a > 0 ? 0 : 2) + (b > 0 ? 0 : 1);
  if (SG_NEGCTL(g_negctl_direction8) == 2 && (q == 1 || q == 2)) q = 3 - q;
  const int32_t aa = abs32(a), ab = abs32(b);
  int32_t r = kOct[q][0];
  if (aa > mul32(ab, 2)) r = kOct[q][1];
  const bool axis = SG_NEGCTL(g_negctl_direction8) == 1 ? ab >= mul32(aa, 2) : ab > mul32(aa, 2);
  return axis ? kOct[q][2] : r;
}

int32_t direction_of(int32_t dx, int32_t dy) {
  const bool horiz = SG_NEGCTL(g_negctl_direction_of) == 1 ? abs32(dx) >= abs32(dy) : abs32(dx) > abs32(dy);
  if (horiz) return dx > 0 ? 2 : 6;
  return dy > 0 ? 4 : 0;
}

int32_t angle_fixed(int32_t x, int32_t y) {
  const int32_t ny = neg32(y);
  const int mode = SG_NEGCTL(g_negctl_angle_fixed);
  if (x == 0) {
    const bool down = ny <= 0;
    return i32((mode == 2 ? !down : down) ? 0x80000000u : 0u);
  }
  if (ny == 0) return i32(x <= 0 ? 0xC0000000u : 0x40000000u);
  const int32_t ax = abs32(x), ay = abs32(ny);
  const bool x_major = ax > ay;
  const int32_t t = x_major ? div32(shl32(ay, 14), ax) : div32(shl32(ax, 14), ay);
  const int32_t d = abs32(sub32(0x1333, t));
  const int32_t e = sar32(shl32(mul32(d, mode == 1 ? 10 : 11), 8), 14);
  const int32_t c = sar32(mul32(sub32(0x2800, e), t), 14);
  int32_t r;
  if (x > 0)
    r = ny > 0 ? (x_major ? sub32(0x4000, c) : c) : (x_major ? add32(c, 0x4000) : sub32(0x8000, c));
  else
    r = ny > 0 ? (x_major ? add32(c, 0xC000) : neg32(c)) : (x_major ? sub32(0xC000, c) : add32(c, 0x8000));
  return shl32(r, 16);
}

int32_t sin_scaled(Ctx &ctx, int32_t angle, int32_t amp) {
  const uint32_t ua = u32(angle);
  int32_t m = (ua & 0x80000000u) ? neg32(amp) : amp;
  const int32_t index = i32((ua & 0x3FFFFFFFu) >> 22);  // 0..255
  const int32_t frac = i32(ua & 0x3FFFFFu);
  const Ptr<int32_t> table = state_arr(ctx, &GameState::g_sinTable);
  const int32_t ta = table[index];
  // entry index + 1: at index 255 this is the dword after the table (model_defects#10)
  const int32_t tb = (index == 255 && !SG_DEFECT(g_defect_sin_past_table)) ? ta : table[index + 1];
  const int32_t base = SG_NEGCTL(g_negctl_sin_scaled) == 1 ? 0 : sar32(mul32(sub32(tb, ta), frac), 22);
  const int32_t v = (ua & 0x40000000u) ? add32(sub32(base, ta), 0xFFFF) : add32(base, ta);
  if (m < 0xFFFF) return sar32(mul32(v, m), 16);
  if (m < 0xFFFFFF) {
    if (SG_NEGCTL(g_negctl_sin_scaled) == 2) return mul32(v, sar32(m, 16));
    return sar32(mul32(v, sar32(m, 8)), 8);
  }
  return mul32(v, sar32(m, 16));
}

int32_t fold_range(Ctx &ctx, int32_t a, int32_t amp) {
  uint32_t ua = u32(a);
  if (ua & 0x80000000u) {
    amp = neg32(amp);
    ua &= 0x7FFFFFFFu;
  }
  if (ua & 0x40000000u) ua = (SG_NEGCTL(g_negctl_fold_range) == 1 ? 0x40000000u : 0x7FFFFFFFu) - ua;
  return sin_scaled(ctx, i32(ua), amp);
}

int32_t abs_diff(Ctx &ctx, int32_t a, int32_t amp) {
  return fold_range(ctx, add32(a, SG_NEGCTL(g_negctl_abs_diff) == 1 ? 0x3FFFFFFF : 0x40000000), amp);
}

int32_t cos_scaled(Ctx &ctx, int32_t a, int32_t amp) {
  return sin_scaled(ctx, add32(a, SG_NEGCTL(g_negctl_cos_scaled) == 1 ? 0x40000000 : 0x3FFFFFFF), amp);
}

void init_sin_table(Ctx &ctx) {
  // two doubles of the user's exe: [0] the amplitude, [1] the angle step (a quarter turn over 255 steps)
  const double amp = ctx.exe().get<double>("f_k_dbl_4bba48", 0);
  const double step = ctx.exe().get<double>("f_k_dbl_4bba48", 1);
  const Ptr<int32_t> table = state_arr(ctx, &GameState::g_sinTable);
  const int32_t n = static_cast<int32_t>(sizeof(GameState::g_sinTable) / sizeof(int32_t));
  for (int32_t i = 0; i < n; ++i) {
    const double x = static_cast<double>(SG_NEGCTL(g_negctl_init_sin_table) == 1 ? i + 1 : i) * step;
    table.at(i).set(i32(ftol_low32(std::sin(x) * amp)));
  }
  // the original then calls sinScaled(0x40000000, 100) and cosScaled(0xa02d82d8, 100) and drops both results (pure)
}

int32_t bucket_value(int32_t v) {
  if (v <= 5) return 0;
  if (v <= 9) return 1;
  if (v <= 0x11) return 2;
  if (SG_NEGCTL(g_negctl_bucket_value) == 1) return -1;
  return v <= 0x12 ? 3 : -1;
}

int32_t decay_sum(Ctx &ctx, int32_t x) {
  const int32_t s = (state_arr(ctx, &GameState::g_tileTypeAttrs) + 1).field(&TileTypeAttr::u21).get();
  const int32_t bound = SG_NEGCTL(g_negctl_decay_sum) == 2 ? 0x41 : 0x40;
  int32_t r = 0;
  do {
    r = add32(r, SG_NEGCTL(g_negctl_decay_sum) == 1 ? sar32(x, 3) : x / 8);
    const int32_t step = sar32(x, s);
    if (step == 0 && x >= bound) throw NoReturn{"decaySum"};  // x can no longer change
    x = sub32(x, step);
  } while (x >= bound);
  return r;
}

int32_t decay_sum2(int32_t x, int32_t y) {
  const int32_t ystep = SG_NEGCTL(g_negctl_decay_sum2) == 1 ? 0x7F : 0x80;
  int32_t r = 0, s = 0;
  // fixed-point watch: once x stops changing, (x, y) repeats every 2^32 / 0x80 steps; an unchanged s then repeats too
  bool watching = false;
  int32_t wy = 0, ws = 0;
  uint32_t since = 0;
  do {
    r = add32(r, x / 8);
    s = add32(s, y / 16);
    y = sub32(y, ystep);
    const int32_t nx = sub32(x, sar32(x, 4));
    if (nx == x && !watching) {
      watching = true;
      wy = y;
      ws = s;
      since = 0;
    } else if (watching && ++since == (1u << 25) && ystep == 0x80) {
      if (y == wy && s == ws) throw NoReturn{"decaySum2"};
      wy = y;
      ws = s;
      since = 0;
    }
    x = nx;
  } while (s > 0);
  return r;
}

int32_t tile_distance(int32_t x, int32_t y, int32_t tx, int32_t ty) {
  const int32_t d = distance(sub32(sub32(x, shl32(tx, 10)), 0x200), sub32(sub32(y, shl32(ty, 10)), 0x200));
  return mul32(d, 25) / (SG_NEGCTL(g_negctl_tile_distance) == 1 ? 1000 : 1024);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

// thiscall Random::next(this): this = the 4-byte seed
void a_random_next(Ctx &ctx, const uint32_t *args, Ret &ret) {
  ret.set_double(random_next(ctx.mem.ref<uint32_t>(args[0])));
}
// thiscall Random::range(this, n), ret 4
void a_random_range(Ctx &ctx, const uint32_t *args, Ret &ret) {
  ret.set_u32(random_range(ctx.mem.ref<uint32_t>(args[0]), args[1]));
}
void a_clamp(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(clamp(S(a, 0), S(a, 1), S(a, 2))); }
void a_distance(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(distance(S(a, 0), S(a, 1))); }
void a_sign(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(sign(S(a, 0))); }
void a_approx_distance(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(approx_distance(S(a, 0), S(a, 1))); }
void a_direction8(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(direction8(S(a, 0), S(a, 1))); }
void a_direction_of(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(direction_of(S(a, 0), S(a, 1))); }
void a_angle_fixed(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(angle_fixed(S(a, 0), S(a, 1))); }
void a_sin_scaled(Ctx &c, const uint32_t *a, Ret &ret) { ret.set_i32(sin_scaled(c, S(a, 0), S(a, 1))); }
void a_fold_range(Ctx &c, const uint32_t *a, Ret &ret) { ret.set_i32(fold_range(c, S(a, 0), S(a, 1))); }
void a_abs_diff(Ctx &c, const uint32_t *a, Ret &ret) { ret.set_i32(abs_diff(c, S(a, 0), S(a, 1))); }
void a_cos_scaled(Ctx &c, const uint32_t *a, Ret &ret) { ret.set_i32(cos_scaled(c, S(a, 0), S(a, 1))); }
void a_init_sin_table(Ctx &c, const uint32_t *, Ret &) { init_sin_table(c); }
void a_bucket_value(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(bucket_value(S(a, 0))); }
void a_decay_sum(Ctx &c, const uint32_t *a, Ret &ret) { ret.set_i32(decay_sum(c, S(a, 0))); }
void a_decay_sum2(Ctx &, const uint32_t *a, Ret &ret) { ret.set_i32(decay_sum2(S(a, 0), S(a, 1))); }
void a_tile_distance(Ctx &, const uint32_t *a, Ret &ret) {
  ret.set_i32(tile_distance(S(a, 0), S(a, 1), S(a, 2), S(a, 3)));
}

const FuncDef kFuncs[] = {
    {"Random::next", "thiscall", "double", 1, 0x027F, a_random_next, nullptr},
    {"Random::range", "thiscall", "int", 2, 0x027F, a_random_range, &g_negctl_random_range},
    {"clamp", "default", "int", 3, 0, a_clamp, nullptr},
    {"distance", "default", "int", 2, 0x027F, a_distance, nullptr},
    {"sign", "default", "int", 1, 0, a_sign, &g_negctl_sign},
    {"approxDistance", "default", "int", 2, 0, a_approx_distance, &g_negctl_approx_distance},
    {"direction8", "default", "int", 2, 0, a_direction8, &g_negctl_direction8},
    {"directionOf", "default", "int", 2, 0, a_direction_of, &g_negctl_direction_of},
    {"angleFixed", "default", "int", 2, 0, a_angle_fixed, &g_negctl_angle_fixed},
    {"sinScaled", "default", "int", 2, 0, a_sin_scaled, &g_negctl_sin_scaled},
    {"foldRange", "default", "int", 2, 0, a_fold_range, &g_negctl_fold_range},
    {"absDiff", "default", "int", 2, 0, a_abs_diff, &g_negctl_abs_diff},
    {"cosScaled", "default", "int", 2, 0, a_cos_scaled, &g_negctl_cos_scaled},
    {"initSinTable", "default", "void", 0, 0x027F, a_init_sin_table, &g_negctl_init_sin_table},
    {"bucketValue", "default", "int", 1, 0, a_bucket_value, &g_negctl_bucket_value},
    {"decaySum", "default", "int", 1, 0, a_decay_sum, &g_negctl_decay_sum},
    {"decaySum2", "default", "int", 2, 0, a_decay_sum2, &g_negctl_decay_sum2},
    {"tileDistance", "default", "int", 4, 0x027F, a_tile_distance, &g_negctl_tile_distance},
};

const DefectDef kDefects[] = {
    {"sinScaled.readPastTable", "model_defects#10",
     "interpolation at index 255 reads the dword after g_sinTable (0) instead of a flat segment",
     &g_defect_sin_past_table},
};
}  // namespace

extern const ModuleDef kModule_util = {"util", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                       sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
