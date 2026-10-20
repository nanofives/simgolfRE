// TEST-ONLY module (phase 3.8, stream 38f; header portsim/sim/integrator.h): per-context replacements of a trace run's
// recorded dispatches. Models nothing of the game: the harness (port/test/semantic.py) states which recorded batch of a
// segment's trace a list of commands stands for, then runs the segment with sg_run_frames.
//
//   integrator.setBatchOverrides(block, count)   reads `count` entries from the address space at `block`: u32 pos, u32 n,
//                                                then n commands of 16 u32 each (kind, handler, a0, a1, async_ctrl,
//                                                async_shift, t, arg[0..7], 0); replaces this context's table; returns
//                                                the number of entries
//   integrator.clearBatchOverrides()             empties this context's table
//   integrator.appliedCount()                    replacements handed to a pump since the last set / clear
#include <map>
#include <mutex>
#include <unordered_map>

#include "portsim/sim/integrator.h"

namespace portsim {

namespace {
int g_negctl_overrides = 0;  // 1 = every replacement is ignored (the recorded batch is dispatched): reads like no override

struct Table {
  std::map<uint32_t, std::vector<Command>> by_pos;
  uint64_t applied = 0;
};
std::mutex g_mu;
std::unordered_map<const Ctx *, Table> g_tables;

void a_set(Ctx &ctx, const uint32_t *a, Ret &r) {
  Table t;
  uint32_t p = a[0];
  const uint32_t count = a[1];
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t pos = ctx.mem.load<uint32_t>(p), n = ctx.mem.load<uint32_t>(p + 4);
    p += 8;
    std::vector<Command> cmds;
    for (uint32_t k = 0; k < n; ++k, p += 64) {
      Command c;
      c.kind = static_cast<CmdKind>(ctx.mem.load<uint32_t>(p));
      c.handler = ctx.mem.load<uint32_t>(p + 4);
      c.a0 = ctx.mem.load<int32_t>(p + 8);
      c.a1 = ctx.mem.load<int32_t>(p + 12);
      c.async_ctrl = ctx.mem.load<uint32_t>(p + 16);
      c.async_shift = ctx.mem.load<uint32_t>(p + 20);
      c.t = ctx.mem.load<uint32_t>(p + 24);
      for (int j = 0; j < 8; ++j) c.arg[j] = ctx.mem.load<int32_t>(p + 28 + 4 * j);
      cmds.push_back(c);
    }
    t.by_pos[pos] = std::move(cmds);
  }
  std::lock_guard<std::mutex> g(g_mu);
  g_tables[&ctx] = std::move(t);
  r.set_u32(count);
}

void a_clear(Ctx &ctx, const uint32_t *, Ret &r) {
  std::lock_guard<std::mutex> g(g_mu);
  g_tables.erase(&ctx);
  r.set_u32(0);
}

void a_applied(Ctx &ctx, const uint32_t *, Ret &r) {
  std::lock_guard<std::mutex> g(g_mu);
  auto it = g_tables.find(&ctx);
  r.set_u32(it == g_tables.end() ? 0u : static_cast<uint32_t>(it->second.applied));
}

const FuncDef kFuncs[] = {
    {"integrator.setBatchOverrides", "default", "uint32", 2, 0, a_set, &g_negctl_overrides},
    {"integrator.clearBatchOverrides", "default", "uint32", 0, 0, a_clear, nullptr},
    {"integrator.appliedCount", "default", "uint32", 0, 0, a_applied, nullptr},
};
}  // namespace

const std::vector<Command> *batch_override(Ctx &ctx, uint32_t pos) {
  if (SG_NEGCTL(g_negctl_overrides) == 1) return nullptr;
  std::lock_guard<std::mutex> g(g_mu);
  auto it = g_tables.find(&ctx);
  if (it == g_tables.end()) return nullptr;
  auto jt = it->second.by_pos.find(pos);
  if (jt == it->second.by_pos.end()) return nullptr;
  ++it->second.applied;
  return &jt->second;
}

extern const ModuleDef kModule_integrator = {"integrator", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
