// P39C: the whole green outlined while the player aims (greenline.h; log/roadmap/P39/P39C_notes.md). Drawing only: nothing in
// this file writes the GameState.
#include "portsim/sim/greenline.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>

#include "commands_internal.h"
#include "portsim/sim/bridge.h"
#include "portsim/sim/world.h"

namespace portsim {

namespace {

std::atomic<int> g_green_on{0};
constexpr uint8_t kTypeGreen = 1;          // tile type 1 (P22B_OnHoleGreen's isGreen)
constexpr uint16_t kFlagCup = 1u << 7;     // the cup tile's flag (P1_tile's test before P1_flag80)
constexpr uint16_t kFlagHole = (1u << 5) - 1;  // its hole number (P1_flag80 compares it with the golfer's +0x21 byte)

struct Cup {
  int32_t x, y, hole;
};

uint64_t grid_key(Ctx &ctx) {
  const int32_t n = world_tiles(ctx);
  const int8_t *t = grid_host(ctx, &GameState::g_tileType);
  const uint16_t *f = grid_host(ctx, &GameState::g_tileFlags);
  uint64_t h = 1469598103934665603ull ^ static_cast<uint64_t>(world_n(ctx));
  for (int32_t i = 0; i < n; ++i) {
    const uint32_t v = (t[i] == kTypeGreen ? 1u : 0u) | ((f[i] & kFlagCup) ? (2u | ((f[i] & kFlagHole) << 2)) : 0u);
    h = (h ^ v) * 1099511628211ull;
  }
  return h;
}
}  // namespace

void green_highlight_set(bool on) { g_green_on.store(on ? 1 : 0, std::memory_order_relaxed); }
bool green_highlight_on() { return g_green_on.load(std::memory_order_relaxed) != 0; }

int32_t green_highlight_hole(Ctx &ctx) {
  if (ctx.mem.load<int32_t>(GS(g_panelListMode)) != 3) return 0;
  const int32_t g = ctx.mem.load<int32_t>(GS(g_playerGolfer));
  if (g < 0 || g >= 0x98) return 0;   // -1: no player golfer (P1_flag80's test); the record table holds 0x98 golfers
  return ctx.mem.load<int8_t>(GS(g_golfers) + 0x21 + (static_cast<uint32_t>(g) << 8));
}

bool green_highlight_blink(Ctx &ctx) {
  return (ctx.mem.load<uint8_t>(GS(g_date)) & 4) != 0 || (ctx.mem.load<uint8_t>(GS(g_flags)) & 4) != 0;
}

GreenOutline green_outline_build(Ctx &ctx, int32_t hole) {
  GreenOutline o;
  o.hole = hole;
  if (hole <= 0) return o;
  const int32_t N = world_n(ctx), last = world_last(ctx);
  const int8_t *type = grid_host(ctx, &GameState::g_tileType);
  const uint16_t *flags = grid_host(ctx, &GameState::g_tileFlags);
  auto idx = [&](int32_t x, int32_t y) { return x * N + y; };
  auto green = [&](int32_t x, int32_t y) { return x >= 0 && x <= last && y >= 0 && y <= last && type[idx(x, y)] == kTypeGreen; };
  std::vector<Cup> cups;
  for (int32_t x = 0; x <= last; ++x)
    for (int32_t y = 0; y <= last; ++y) {
      const uint16_t f = flags[idx(x, y)];
      if ((f & kFlagCup) && (f & kFlagHole) != 0) cups.push_back(Cup{x, y, static_cast<int32_t>(f & kFlagHole)});
    }
  // the component: 8-connected green tiles from every cup of this hole (the cup tile and its 3 x 3 neighbours seed it)
  std::vector<uint8_t> in(static_cast<size_t>(N) * N, 0);
  std::vector<int32_t> stack;
  auto push = [&](int32_t x, int32_t y) {
    if (!green(x, y) || in[idx(x, y)]) return;
    in[idx(x, y)] = 1;
    stack.push_back(x);
    stack.push_back(y);
  };
  for (const Cup &c : cups)
    if (c.hole == hole)
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) push(c.x + dx, c.y + dy);
  while (!stack.empty()) {
    const int32_t y = stack.back();
    stack.pop_back();
    const int32_t x = stack.back();
    stack.pop_back();
    for (int dx = -1; dx <= 1; ++dx)
      for (int dy = -1; dy <= 1; ++dy) push(x + dx, y + dy);
  }
  // other holes' cups on the same green (the cup tile or one of its 3 x 3 neighbours in the component) take their nearer tiles
  std::vector<Cup> mine, rivals;
  for (const Cup &c : cups) {
    if (c.hole == hole) {
      mine.push_back(c);
      continue;
    }
    bool touches = false;
    for (int dx = -1; dx <= 1 && !touches; ++dx)
      for (int dy = -1; dy <= 1 && !touches; ++dy) {
        const int32_t x = c.x + dx, y = c.y + dy;
        touches = x >= 0 && x <= last && y >= 0 && y <= last && in[idx(x, y)];
      }
    if (touches) rivals.push_back(c);
  }
  auto d2 = [](const Cup &c, int32_t x, int32_t y) { return (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y); };
  std::vector<uint8_t> keep(in.size(), 0);
  for (int32_t x = 0; x <= last; ++x)
    for (int32_t y = 0; y <= last; ++y) {
      if (!in[idx(x, y)]) continue;
      int32_t dm = 1 << 30;
      for (const Cup &c : mine) dm = std::min(dm, d2(c, x, y));
      bool ours = true;
      for (const Cup &c : rivals) {
        const int32_t dr = d2(c, x, y);
        if (dr < dm || (dr == dm && c.hole < hole)) {   // equal distance: the lower hole number keeps the tile
          ours = false;
          break;
        }
      }
      if (ours) {
        keep[idx(x, y)] = 1;
        o.tiles.push_back(idx(x, y));
      }
    }
  auto kept = [&](int32_t x, int32_t y) { return x >= 0 && x <= last && y >= 0 && y <= last && keep[idx(x, y)]; };
  for (int32_t i : o.tiles) {
    const int32_t x = i / N, y = i % N;
    const int32_t x0 = x << 10, y0 = y << 10, x1 = ((x + 1) << 10) - 1, y1 = ((y + 1) << 10) - 1;
    if (!kept(x - 1, y)) o.edges.push_back(GreenEdge{x0, y0, x0, y1});
    if (!kept(x + 1, y)) o.edges.push_back(GreenEdge{x1, y0, x1, y1});
    if (!kept(x, y - 1)) o.edges.push_back(GreenEdge{x0, y0, x1, y0});
    if (!kept(x, y + 1)) o.edges.push_back(GreenEdge{x0, y1, x1, y1});
  }
  return o;
}

const GreenOutline &green_outline(Ctx &ctx, int32_t hole) {
  struct Cache {
    const Ctx *ctx = nullptr;
    int32_t hole = -1, n = 0;
    uint64_t key = 0;
    GreenOutline o;
  };
  static thread_local Cache c;
  const uint64_t key = grid_key(ctx);
  if (c.ctx != &ctx || c.hole != hole || c.n != world_n(ctx) || c.key != key) {
    c.o = green_outline_build(ctx, hole);
    c.ctx = &ctx;
    c.hole = hole;
    c.n = world_n(ctx);
    c.key = key;
  }
  return c.o;
}

int32_t green_outline_lines(Ctx &ctx, const GreenOutline &o, const GreenLineSink &sink, int32_t wider) {
  if (o.edges.empty()) return 0;
  const GameState &gs = ctx.gs();
  const int32_t kFar = 1 << 24;   // the screen test passes for any point of the map: the line's clip cuts it at the view's edge
  const int32_t E = wide_view_margin_w2s(ctx), W = gs.g_screenWidth, H = gs.g_screenHeight, pad = 16;
  const int32_t z = gs.g_zoomScale;
  const int32_t width = (z < 4 ? 2 : 3) + wider;   // the terrain tool's mark edges are 3 wide at zoom 4 (P4_holeGrid's drawMapLine)
  struct L {
    int32_t x1, y1, x2, y2;
  };
  std::vector<L> ls;
  ls.reserve(o.edges.size());
  for (const GreenEdge &e : o.edges) {
    int32_t ax = 0, ay = 0, bx = 0, by = 0;
    world_to_screen(ctx, e.x0, e.y0, ax, ay, kFar);
    world_to_screen(ctx, e.x1, e.y1, bx, by, kFar);
    if (ax < -E - pad && bx < -E - pad) continue;
    if (ax >= W + E + pad && bx >= W + E + pad) continue;
    if (ay < -pad && by < -pad) continue;
    if (ay >= H + pad && by >= H + pad) continue;
    ls.push_back(L{ax, ay, bx, by});
  }
  int32_t n = 0;
  for (const L &l : ls) {
    sink(l.x1, l.y1, l.x2, l.y2, kGreenLineColour, width, 7);
    ++n;
  }
  return n;
}

int32_t aim_free_snap_radius(Ctx &ctx) {
  const int32_t z = std::max<int32_t>(1, ctx.gs().g_zoomScale);
  return std::max<int32_t>(0x200, 0x800 / z);
}

void aim_free_point(Ctx &ctx, double mx, double my, int32_t cx, int32_t cy, int32_t hole, int32_t &px, int32_t &py, bool &snap,
                    int32_t *rx, int32_t *ry) {
  const int32_t last = world_last(ctx), kFar = 1 << 24;
  const double top = static_cast<double>((last + 1) << 10) - 1.0;
  auto proj = [&](double x, double y, double &sx, double &sy) {
    const int32_t ix = static_cast<int32_t>(std::lround(x)), iy = static_cast<int32_t>(std::lround(y));
    int32_t a = 0, b = 0;
    world_to_screen(ctx, ix, iy, a, b, kFar);
    double ex = 0, ey = 0;
    if (world_to_screen_exact(ix, iy, a, b, ex, ey)) {
      sx = ex;
      sy = ey;
    } else {
      sx = a;
      sy = b;
    }
  };
  double wx = (cx << 10) + 0x200, wy = (cy << 10) + 0x200;
  for (int it = 0; it < 8; ++it) {
    double sx = 0, sy = 0, ax = 0, ay = 0, bx = 0, by = 0;
    proj(wx, wy, sx, sy);
    const double ex = mx - sx, ey = my - sy;
    if (std::fabs(ex) < 0.25 && std::fabs(ey) < 0.25) break;
    const double h = 64.0;
    proj(wx + h, wy, ax, ay);
    proj(wx, wy + h, bx, by);
    const double j11 = (ax - sx) / h, j21 = (ay - sy) / h, j12 = (bx - sx) / h, j22 = (by - sy) / h;
    const double det = j11 * j22 - j12 * j21;
    if (std::fabs(det) < 1e-9) break;
    double dx = (j22 * ex - j12 * ey) / det, dy = (-j21 * ex + j11 * ey) / det;
    const double len = std::hypot(dx, dy);
    if (len > 1536.0) {
      dx *= 1536.0 / len;
      dy *= 1536.0 / len;
    }
    wx = std::min(std::max(wx + dx, 0.0), top);
    wy = std::min(std::max(wy + dy, 0.0), top);
  }
  px = static_cast<int32_t>(std::lround(wx));
  py = static_cast<int32_t>(std::lround(wy));
  if (rx) *rx = px;
  if (ry) *ry = py;
  snap = false;
  if (hole <= 0 || hole >= 0x13) return;
  const uint32_t rec = state_va(ctx, &GameState::g_holes) + 0x208u * static_cast<uint32_t>(hole);
  const int32_t ux = ctx.mem.load<int32_t>(rec + 0x18), uy = ctx.mem.load<int32_t>(rec + 0x1c);
  if (ux < 0 || ux > last || uy < 0 || uy > last) return;
  const int32_t qx = (ux << 10) + 0x200, qy = (uy << 10) + 0x200;
  if (std::hypot(static_cast<double>(px - qx), static_cast<double>(py - qy)) > aim_free_snap_radius(ctx)) return;
  const GreenOutline &o = green_outline(ctx, hole);
  const int32_t ti = (px >> 10) * (last + 1) + (py >> 10);
  if (!std::binary_search(o.tiles.begin(), o.tiles.end(), ti)) return;
  px = qx;
  py = qy;
  snap = true;
}

namespace {
// ---- test entries (port/test/test_p39c_green_highlight.py) ----
// greenLine.tiles(hole, out, cap): the outline's tile indices at out (dwords, at most cap); returns their count
void a_tiles(Ctx &c, const uint32_t *a, Ret &r) {
  const GreenOutline o = green_outline_build(c, static_cast<int32_t>(a[0]));
  for (size_t k = 0; k < o.tiles.size() && k < a[2]; ++k) c.mem.store<int32_t>(a[1] + 4 * static_cast<uint32_t>(k), o.tiles[k]);
  r.set_i32(static_cast<int32_t>(o.tiles.size()));
}
// greenLine.edges(hole, out, cap): the outline's sides (x0, y0, x1, y1 world units: 4 dwords each, at most cap sides); returns
// their count. Read through the cache (green_outline), as the frame does.
void a_edges(Ctx &c, const uint32_t *a, Ret &r) {
  const GreenOutline &o = green_outline(c, static_cast<int32_t>(a[0]));
  for (size_t k = 0; k < o.edges.size() && k < a[2]; ++k) {
    const GreenEdge &e = o.edges[k];
    const int32_t w[4] = {e.x0, e.y0, e.x1, e.y1};
    for (uint32_t j = 0; j < 4; ++j) c.mem.store<int32_t>(a[1] + 16 * static_cast<uint32_t>(k) + 4 * j, w[j]);
  }
  r.set_i32(static_cast<int32_t>(o.edges.size()));
}
// greenLine.lines(out, cap): the frame's lines now (green_highlight_hole, blink and option ignored: the hole the original marks,
// or none), 7 dwords each (drawMapLine's arguments), at most cap; returns their count
void a_lines(Ctx &c, const uint32_t *a, Ret &r) {
  const int32_t h = green_highlight_hole(c);
  uint32_t k = 0;
  const int32_t n = h <= 0 ? 0 : green_outline_lines(c, green_outline(c, h), [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                                                                   uint32_t col, int32_t w, int32_t al) {
    if (k < a[1]) {
      const uint32_t v[7] = {static_cast<uint32_t>(x1), static_cast<uint32_t>(y1), static_cast<uint32_t>(x2), static_cast<uint32_t>(y2),
                             col, static_cast<uint32_t>(w), static_cast<uint32_t>(al)};
      for (uint32_t j = 0; j < 7; ++j) c.mem.store<uint32_t>(a[0] + 28 * k + 4 * j, v[j]);
    }
    ++k;
  });
  r.set_i32(n);
}
// greenLine.option(on): sets the app option (as the app's --green-highlight); returns the previous value
void a_option(Ctx &, const uint32_t *a, Ret &r) {
  r.set_i32(green_highlight_on() ? 1 : 0);
  green_highlight_set(a[0] != 0);
}
// aimFree.point(mx * 16, my * 16, cx, cy, hole, out): aim_free_point with the local point in 1 / 16 pixels; out = px, py, snap,
// rx, ry, radius, and the exact projection of (rx, ry) x 16 (port/test/test_p39d_free_aim.py)
void a_aim_point(Ctx &c, const uint32_t *a, Ret &r) {
  int32_t px = 0, py = 0, rx = 0, ry = 0;
  bool snap = false;
  aim_free_point(c, static_cast<int32_t>(a[0]) / 16.0, static_cast<int32_t>(a[1]) / 16.0, static_cast<int32_t>(a[2]),
                 static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]), px, py, snap, &rx, &ry);
  // the exact projection of the unsnapped point, in 1 / 16 pixels (bridge.h world_to_screen_exact; the integer one when unavailable)
  int32_t sx = 0, sy = 0;
  world_to_screen(c, rx, ry, sx, sy, 1 << 24);
  double ex = sx, ey = sy;
  world_to_screen_exact(rx, ry, sx, sy, ex, ey);
  const int32_t w[8] = {px, py, snap ? 1 : 0, rx, ry, aim_free_snap_radius(c), static_cast<int32_t>(std::lround(ex * 16)),
                        static_cast<int32_t>(std::lround(ey * 16))};
  for (uint32_t k = 0; k < 8; ++k) c.mem.store<int32_t>(a[5] + 4 * k, w[k]);
  r.set_i32(snap ? 1 : 0);
}
// aimFree.held(out): Ctx::aim_free_g, x, y, bx, by, snap (the held free point, tick_frame.inc P39D_FreeAim)
void a_aim_held(Ctx &c, const uint32_t *a, Ret &r) {
  const int32_t w[6] = {c.aim_free_g, c.aim_free_x, c.aim_free_y, c.aim_free_bx, c.aim_free_by, c.aim_free_snap ? 1 : 0};
  for (uint32_t k = 0; k < 6; ++k) c.mem.store<int32_t>(a[0] + 4 * k, w[k]);
  r.set_i32(c.aim_free_g);
}
int g_negctl_greenline = 0;
const FuncDef kFuncs[] = {
    {"greenLine.tiles", "default", "int", 3, 0, a_tiles, &g_negctl_greenline},
    {"greenLine.edges", "default", "int", 3, 0, a_edges, &g_negctl_greenline},
    {"greenLine.lines", "default", "int", 2, 0, a_lines, &g_negctl_greenline},
    {"greenLine.option", "default", "int", 1, 0, a_option, &g_negctl_greenline},
    {"aimFree.point", "default", "int", 6, 0, a_aim_point, &g_negctl_greenline},
    {"aimFree.held", "default", "int", 1, 0, a_aim_held, &g_negctl_greenline},
};
}  // namespace

extern const ModuleDef kModule_greenline = {"greenline", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
