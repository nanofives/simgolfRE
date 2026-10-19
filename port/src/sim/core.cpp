// Core state helpers (see portsim/sim/core.h). Phase 3.0, written from the specification; no original address or code.
#include "portsim/sim/core.h"

#include "portsim/ptr.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   staticInit_gRngZero  1 = stores 1
//   swapTableEntry       1 = the scratch record is not written (a direct swap), 2 = copies 0x3f dwords
int g_negctl_static_init_rng_zero = 0;
int g_negctl_swap_table_entry = 0;
}  // namespace

void static_init_rng_zero(Ctx &ctx) {
  state_obj(ctx, &GameState::g_rng).field(&Random::state).set(SG_NEGCTL(g_negctl_static_init_rng_zero) == 1 ? 1u : 0u);
}

void swap_table_entry(Ctx &ctx, int32_t a, int32_t b) {
  const Ptr<Golfer> golfers = state_arr(ctx, &GameState::g_golfers);
  const uint32_t ra = (golfers + a).va(), rb = (golfers + b).va(), scratch = (golfers + kGolferScratch).va();
  const uint32_t n = SG_NEGCTL(g_negctl_swap_table_entry) == 2 ? 0xFC : static_cast<uint32_t>(sizeof(Golfer));
  if (SG_NEGCTL(g_negctl_swap_table_entry) == 1) {
    for (uint32_t k = 0; k < n; k += 4) {
      const uint32_t t = ctx.mem.load<uint32_t>(ra + k);
      ctx.mem.store<uint32_t>(ra + k, ctx.mem.load<uint32_t>(rb + k));
      ctx.mem.store<uint32_t>(rb + k, t);
    }
    return;
  }
  rep_movs(ctx.mem, scratch, ra, n);
  rep_movs(ctx.mem, ra, rb, n);
  rep_movs(ctx.mem, rb, scratch, n);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_static_init_rng_zero(Ctx &c, const uint32_t *, Ret &) { static_init_rng_zero(c); }
void a_swap_table_entry(Ctx &c, const uint32_t *a, Ret &) {
  swap_table_entry(c, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
}

const FuncDef kFuncs[] = {
    {"staticInit_gRngZero", "default", "void", 0, 0, a_static_init_rng_zero, &g_negctl_static_init_rng_zero},
    {"swapTableEntry", "default", "void", 2, 0, a_swap_table_entry, &g_negctl_swap_table_entry},
};
}  // namespace

extern const ModuleDef kModule_core = {"core", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
