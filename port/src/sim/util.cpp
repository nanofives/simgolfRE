// Utility leaves of the simulation core: Random::next, Random::range, clamp, distance (see portsim/sim/util.h).
// Written from the specification (analysis notes + vector corpus); no original address or code.
#include "portsim/sim/util.h"

#include <cmath>

#include "portsim/registry.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   Random::range  1 = results 0 and 1 swapped, 2 = scaled by n + 1, 3 = rounded to nearest instead of truncated
int g_negctl_random_range = 0;

int32_t abs32(int32_t v) {  // VC6's inline abs: INT_MIN stays INT_MIN
  return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v;
}
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
  const uint32_t ux = static_cast<uint32_t>(dx), uy = static_cast<uint32_t>(dy);
  const int32_t sum = static_cast<int32_t>(ux * ux + uy * uy);
  return static_cast<int32_t>(ftol_low32(std::sqrt(static_cast<double>(sum)) * scale));
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
// thiscall Random::next(this): this = the 4-byte seed
void a_random_next(Ctx &ctx, const uint32_t *args, Ret &ret) {
  ret.set_double(random_next(ctx.mem.ref<uint32_t>(args[0])));
}

// thiscall Random::range(this, n), ret 4
void a_random_range(Ctx &ctx, const uint32_t *args, Ret &ret) {
  ret.set_u32(random_range(ctx.mem.ref<uint32_t>(args[0]), args[1]));
}

void a_clamp(Ctx &, const uint32_t *args, Ret &ret) {
  ret.set_i32(clamp(static_cast<int32_t>(args[0]), static_cast<int32_t>(args[1]), static_cast<int32_t>(args[2])));
}

void a_distance(Ctx &, const uint32_t *args, Ret &ret) {
  ret.set_i32(distance(static_cast<int32_t>(args[0]), static_cast<int32_t>(args[1])));
}

const FuncDef kFuncs[] = {
    {"Random::next", "thiscall", "double", 1, 0x027F, a_random_next, nullptr},
    {"Random::range", "thiscall", "int", 2, 0x027F, a_random_range, &g_negctl_random_range},
    {"clamp", "default", "int", 3, 0, a_clamp, nullptr},
    {"distance", "default", "int", 2, 0x027F, a_distance, nullptr},
};
}  // namespace

extern const ModuleDef kModule_util = {"util", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
