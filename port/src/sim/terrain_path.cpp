// pathStep of the terrain module (see portsim/sim/terrain.h): the walkers' breadth-first path-cost wave and next
// direction. Phase 3.1a, written from the specification (re/analysis/sim/0042e7e0_pathStep.md, the C3 hook source as
// reading reference, the vector corpus); no original address or code.
#include "portsim/ptr.h"
#include "portsim/sim/terrain.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   pathStep  1 = diagonal steps cost the same as orthogonal ones, 2 = the target-side tie bonus (mask) not applied,
//             3 = the partner tile costs nothing extra
int g_negctl_path_step = 0;

// The debug overlay (g_flags bit 0x1000): writes every non-zero wave cost as text over its tile, refreshes the root
// window and waits for a click (pumpAndRedraw(0)). The port keeps its state writes (g_textBuffer = the last cost's
// decimal text, the _itoa buffer g_textBuf) and drops the drawing and the wait (UI; port/CORE.md, PHASE23_PLAN.md 8.2).
void debug_overlay(Ctx &ctx) {
  GameState &g = ctx.gs();
  for (int32_t gx = 0; gx < 50; ++gx)
    for (int32_t gy = 0; gy < 50; ++gy) {
      const uint8_t v = g.g_pathGrid[gx * 50 + gy];
      if (v == 0) continue;
      g.g_textBuffer[0] = 0;
      // the static CRT's _itoa(v, g_textBuf, 10) of a value 1..255
      char digits[4];
      int n = 0;
      for (uint32_t r = v; r; r /= 10) digits[n++] = static_cast<char>('0' + r % 10);
      const uint32_t buf = state_va(ctx, &GameState::g_textBuf);
      for (int i = 0; i < n; ++i) ctx.mem.store<char>(buf + u32(i), digits[n - 1 - i]);
      ctx.mem.store<char>(buf + u32(n), 0);
      vstrcat(ctx.mem, state_va(ctx, &GameState::g_textBuffer), buf);
    }
}
}  // namespace

int32_t path_step(Ctx &ctx, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5) {
  GameState &g = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_path_step);
  uint8_t *grid = g.g_pathGrid;
  for (int i = 0; i < 2500; ++i) grid[i] = 0;
  const int32_t dir0 = direction8(sub32(p1, p3), sub32(p2, p4));
  const int32_t sx = clamp(sar32(p1, 10), 0, 0x31), sy = clamp(sar32(p2, 10), 0, 0x31);
  const int32_t tx = clamp(sar32(p3, 10), 0, 0x31), ty = clamp(sar32(p4, 10), 0, 0x31);
  const Ptr<Golfer> golfers = state_arr(ctx, &GameState::g_golfers);
  const Ptr<Golfer> partner = golfers + (p5 ^ 1);
  int32_t ox = clamp(sar32(partner.field(&Golfer::x).get(), 10), 0, 0x31);
  const int32_t oy = clamp(sar32(partner.field(&Golfer::y).get(), 10), 0, 0x31);
  if (ox == sx && oy == sy) ox = -1;
  if (sx == tx && sy == ty) return dir0;
  const int32_t ex = sx - tx, ey = sy - ty;
  const int32_t dist = distance(ex, ey);
  const int32_t ang = angle_fixed(ex, ey);
  const int32_t a = sar32(add32(sar32(ang, 28), 1) ^ -7, 1) & 7;
  const Ptr<int16_t> qx = state_arr(ctx, &GameState::g_pathQueueX), qy = state_arr(ctx, &GameState::g_pathQueueY);
  qx.set(static_cast<int16_t>(sx));
  qy.set(static_cast<int16_t>(sy));
  int32_t tail = 1;
  grid[sx * 50 + sy] = 1;
  const uint32_t mask = (1u << ((a + 1) & 6)) | (1u << (a & 6));
  int32_t limit = ((g.g_flags & 0x100u) ? 0x5A : 0) + 0xA0;
  int32_t head = 0;
  const Grid<uint16_t> fl = state_grid(ctx, &GameState::g_tileFlags, kGridW);
  const Ptr<TileTypeAttr> attrs = state_arr(ctx, &GameState::g_tileTypeAttrs);
  do {
    const int32_t x = qx[head], y = qy[head];
    head = (head + 1) & 0x3FF;
    const int32_t flat = add32(y, mul32(x, 50));
    const int32_t cur = (state_arr(ctx, &GameState::g_pathGrid) + flat).get();
    if (!(cur > limit)) {
      for (int32_t k = 0; k < 8; ++k) {
        const int32_t nx = add32(state_arr(ctx, &GameState::f_tab_dirDx_4c2878)[k], x);
        const int32_t ny = add32(state_arr(ctx, &GameState::f_tab_dirDy_4c2898)[k], y);
        if (nx < 0 || ny < 0 || nx >= 0x32 || ny >= 0x32) continue;
        const int32_t flat2 = ny + nx * 50;
        const int32_t par = mode == 1 ? 0 : (k & 1);
        int32_t cost = g.g_tilePathCost[flat2] + par;
        const int32_t ndist = grid[flat2];
        if (nx == ox && ny == oy && mode != 3) cost += 2;
        if ((k & 1) == 0) {
          const uint32_t date = static_cast<uint32_t>(g.g_date);
          const int32_t v = static_cast<int32_t>(((~date & 0x40u) | 0x80u) >> 6);
          if (dist > v) {
            if ((g.g_tileFlags[flat2] & 0x420) == 0x20 && ((state_arr(ctx, &GameState::g_tileFlags) + flat).as<uint8_t>().get() & 0x20) != 0) {
              const int32_t r = direction_of(nx - sx, ny - sy);
              cost = (k != r) ? 1 : 0;
            }
          }
        }
        if ((g.g_flags & 0x100u) != 0 || (golfers + p5).field(&Golfer::hole).as<uint8_t>().get() == 0x13) {
          const int8_t t = g.g_tileType[flat2];
          if (t == 1 || t == 2 || t == 0 || static_cast<uint8_t>((attrs + t).field(&TileTypeAttr::collar).get()) == 7)
            cost += 1;
          else
            cost = sar32(cost + 1, 1);
        }
        const uint8_t tt = static_cast<uint8_t>(g.g_tileType[flat2]);
        if ((tt == 0x11 || tt == 0x14) && (g.g_tileFlags[flat2] & 0x20) == 0 && cur < 0x40) cost += 0x10;
        const int32_t total = cost + cur;
        if (ndist != 0 && ndist <= total) continue;
        if (total > 0xFF) continue;
        grid[flat2] = static_cast<uint8_t>(total);
        (qx + tail).set(static_cast<int16_t>(nx));
        (qy + tail).set(static_cast<int16_t>(ny));
        tail = (tail + 1) & 0x3FF;
        if (nx == tx && ny == ty) limit = total;
      }
    }
  } while (head != tail && g.g_flag_83afcc == 0);
  if ((g.g_flags & 0x1000u) != 0) debug_overlay(ctx);
  const int32_t target = ty + tx * 50;
  int32_t best = -1;
  int32_t best_cost = grid[target] * 2 + 1;
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  const int32_t rnd = static_cast<int32_t>(random_range(seed, 8) & 0xFFFFu);
  const bool target_path = static_cast<uint8_t>(g.g_tileType[target]) == 0x11 && (g.g_tileFlags[target] & 0x20) != 0;
  for (int32_t n = 0; n < 8; ++n) {
    const int32_t k = (n + rnd) & 7;
    const int32_t nx = state_arr(ctx, &GameState::f_tab_dirDx_4c2878)[k] + tx;
    const int32_t ny = state_arr(ctx, &GameState::f_tab_dirDy_4c2898)[k] + ty;
    if (nx < 0 || ny < 0 || nx >= 0x32 || ny >= 0x32) continue;
    if (tile_blocked(ctx, nx, ny) != 0) continue;
    const int32_t d = grid[ny + nx * 50];
    if (d == 0) continue;
    const int32_t par = k & 1;
    int32_t cand = par + d * 2;
    if (target_path && par != 0) continue;
    if ((mask & (1u << (k ^ 4))) != 0 && mode != 2) cand -= 1;
    if (cand < best_cost) {
      best = k;
      best_cost = cand;
    }
  }
  if ((g.g_flags & 0x100u) == 0 && target_path) {
    const Ptr<Golfer> me = golfers + p5;
    const int32_t gdx = sub32(sub32(me.field(&Golfer::x).get(), shl32(tx, 10)), 0x200);
    const int32_t gdy = sub32(sub32(me.field(&Golfer::y).get(), shl32(ty, 10)), 0x200);
    if (u32(best) <= 6) {
      const int32_t orig = best;
      bool apply = true;
      switch (best) {
        case 0:
          if (abs32(gdx) <= 0x100) return best;
          best = ((gdx <= 0 ? 0 : -1) & -2) + 1;
          break;
        case 2:
          if (abs32(gdy) <= 0x100) return best;
          best = ((gdy <= 0 ? 0 : -1) & -2) + 3;
          break;
        case 4:
          if (abs32(gdx) <= 0x100) return best;
          best = ((gdx <= 0 ? 0 : -1) & 2) - 5;
          break;
        case 6:
          if (abs32(gdy) <= 0x100) return best;
          best = ((gdy <= 0 ? 0 : -1) & 2) - 3;
          break;
        default:
          apply = false;
          break;
      }
      if (apply) {
        best &= 7;
        if (best != orig) g.g_flags |= 0x80000u;
      }
    }
  }
  if (best == -1 && (sx != tx || sy != ty)) best = direction8(sx - tx, sy - ty);
  return best;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_path_step(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(path_step(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4))); }

const FuncDef kFuncs[] = {
    {"pathStep", "default", "int", 5, 0x027F, a_path_step, &g_negctl_path_step},
};
}  // namespace

extern const ModuleDef kModule_terrain_path = {"terrain_path", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
