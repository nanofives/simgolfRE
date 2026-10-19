// Terrain, tile-grid and heightfield functions (see portsim/sim/terrain.h). Phase 3.1a, written from the specification
// (analysis notes, the C3/C4 hook sources as reading reference, the disassembly of rows without a hook, the vector
// corpus); no original address or code.
#include "portsim/sim/terrain.h"

#include <utility>
#include <vector>

#include "portsim/ptr.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   freeAtTile               1 = compares x >> 9
//   raiseCornerLevelsAround  1 = test <=                         lowerCornerLevelsAround 1 = test >=
//   pushTripleEntry          1 = capacity 0xff
//   setGrids                 1 = b also into g_tileBuildCode's neighbour (y + 1)
//   floodMark                1 = neighbour test 0x20 only
//   bilinearSample           1 = floor division (sar 5)
//   clearTile                1 = setting 2 keeps the flags
//   canStep                  1 = neighbour walk byte not tested, 2 = neighbour flag mask 0x100 only (BLIND: no vector has
//                              a neighbour with flag 0x20 alone)
//   evalPlacementArea        1 = border cells ignored (sum returned for every kind)
//   nearestPlaced            1 = no early return for type >= 6 without flag 0x40
//   findNearestTargetTile    1 = distance bias 0x100, 2 = blocked cells counted (BLIND: no blocked cell has flag 0x200)
//   sampleHeight             1 = coarse sample weight 5, 2 = no clamp at 0x200 (BLIND: no vector exceeds 0x200)
//   propagateType11          1 = recursion into every unblocked neighbour
//   rebuildTileCoverage      1 = types >= 0x11 not marked, 2 = g_difficulty ignored (BLIND: no vector with difficulty 0
//                              reaches a type 6..0x10 record)
//   relaxTile42f630          1 = minimum not followed (each neighbour compared with the start height)
//   raiseFromNeighbours      1 = sameByte ignored
//   spreadShade              1 = divisor approxDistance + 1
//   heightBlend              1 = lower clamp bound 2 + (difficulty != 0)
//   rateTile                 1 = toward_zero36 clamped to 2
//   markTileKind11           1 = variant 1 instead of 2
//   spreadAllShade           1 = total capped at 0x6d5
//   genNoise                 1 = Random::range(0xf)
//   cornerRange              1 = the maximum never widens        cornerHeights 1 = corner 3 takes corner 5's blend
//   tileQuery449310          1 = flag 0 (cache skipped)          slopeY 1 = half-tile bound 0x1ff
//   slopeX                   1 = left half reversed              slopeMix 1 = p and q swapped
//   relaxEdges42f530         1 = second corner pair not tested   heightAt42fa30 1 = x weight 0x3ff
//   rebuildHeightfield       1 = flag 4 does not seed the level
//   scanTileLine             1 = the y + 1 neighbour not scored, 2 = one straight-line point on 0xd is enough
int g_negctl_scan_tile_line = 0;
int g_negctl_corner_range = 0;
int g_negctl_corner_heights = 0;
int g_negctl_tile_query = 0;
int g_negctl_slope_y = 0;
int g_negctl_slope_x = 0;
int g_negctl_slope_mix = 0;
int g_negctl_relax_edges = 0;
int g_negctl_height_at = 0;
int g_negctl_rebuild_heightfield = 0;
int g_negctl_height_blend = 0;
int g_negctl_rate_tile = 0;
int g_negctl_mark_kind11 = 0;
int g_negctl_spread_all_shade = 0;
int g_negctl_gen_noise = 0;
int g_negctl_free_at_tile = 0;
int g_negctl_raise_corner = 0;
int g_negctl_lower_corner = 0;
int g_negctl_push_triple = 0;
int g_negctl_set_grids = 0;
int g_negctl_flood_mark = 0;
int g_negctl_bilinear = 0;
int g_negctl_clear_tile = 0;
int g_negctl_can_step = 0;
int g_negctl_eval_placement = 0;
int g_negctl_nearest_placed = 0;
int g_negctl_find_nearest_target = 0;
int g_negctl_sample_height = 0;
int g_negctl_propagate11 = 0;
int g_negctl_rebuild_coverage = 0;
int g_negctl_relax_tile = 0;
int g_negctl_raise_from_nb = 0;
int g_negctl_spread_shade = 0;

Grid<int8_t> types(Ctx &c) { return state_grid(c, &GameState::g_tileType, kGridW); }
Grid<uint16_t> flags(Ctx &c) { return state_grid(c, &GameState::g_tileFlags, kGridW); }
Grid<int8_t> levels(Ctx &c) { return state_grid(c, &GameState::g_tileLevel, kGridW); }
Grid<uint8_t> variants(Ctx &c) { return state_grid(c, &GameState::g_tileVariant, kGridW); }
Ptr<TileTypeAttr> attr(Ctx &c, int32_t type) { return state_arr(c, &GameState::g_tileTypeAttrs) + type; }
int8_t walk_of(Ctx &c, int32_t type) { return attr(c, type).field(&TileTypeAttr::walk).get(); }
int8_t collar_of(Ctx &c, int32_t type) { return attr(c, type).field(&TileTypeAttr::collar).get(); }
int32_t dir_dx(Ctx &c, int32_t d) { return state_arr(c, &GameState::f_tab_dirDx_4c2878)[d]; }
int32_t dir_dy(Ctx &c, int32_t d) { return state_arr(c, &GameState::f_tab_dirDy_4c2898)[d]; }
Ptr<PlacedObject> placed(Ctx &c, int32_t i) { return state_arr(c, &GameState::g_placed_objects) + i; }
int8_t item_size(Ctx &c, int32_t type) {  // F_BuildItem +0x10: footprint side
  return (state_arr(c, &GameState::f_tab_buildItems_4c26b0) + type).field(&F_BuildItem::b10).get();
}
}  // namespace

// ================================================================================================== level 0
void free_at_tile(Ctx &ctx, int32_t tx, int32_t ty) {
  const int shift = SG_NEGCTL(g_negctl_free_at_tile) == 1 ? 9 : 10;
  const Ptr<BallRecord> recs = state_arr(ctx, &GameState::g_ballRecords);
  for (int32_t i = 0; i < 256; ++i) {
    const Ptr<BallRecord> r = recs + i;
    if (r.field(&BallRecord::x).get() == -1) continue;
    if (sar32(r.field(&BallRecord::start_x).get(), shift) == tx && sar32(r.field(&BallRecord::start_y).get(), shift) == ty)
      r.field(&BallRecord::x).set(-1);
  }
}

namespace {
// shared body of raise / lowerCornerLevelsAround; dir +1 raises, -1 lowers
void corner_levels_around(Ctx &ctx, int32_t x, int32_t y, int dir, bool wrong) {
  const Ptr<uint8_t> centre = state_arr(ctx, &GameState::g_cornerLevel) + add32(mul32(x, kCornerW), y);
  const int32_t c = centre.get();
  const Ptr<int16_t> kernel = state_arr(ctx, &GameState::g_levelFalloffKernel);
  Ptr<uint8_t> row = centre - (2 * kCornerW + 2);
  for (int32_t r = 0; r < 5; ++r, row = row + kCornerW) {
    for (int32_t k = 0; k < 5; ++k) {
      const int32_t kv = kernel[r * 5 + k];
      const Ptr<uint8_t> cell = row + k;
      const uint8_t v = cell.get();
      if (dir > 0) {
        const int32_t lim = sub32(c, kv);
        if (wrong ? v <= lim : v < lim) cell.set(static_cast<uint8_t>(v + 1));
      } else {
        const int32_t lim = add32(kv, c);
        if (wrong ? v >= lim : v > lim) cell.set(static_cast<uint8_t>(v - 1));
      }
    }
  }
}
}  // namespace

void raise_corner_levels_around(Ctx &ctx, int32_t x, int32_t y) {
  corner_levels_around(ctx, x, y, +1, SG_NEGCTL(g_negctl_raise_corner) == 1);
}
void lower_corner_levels_around(Ctx &ctx, int32_t x, int32_t y) {
  corner_levels_around(ctx, x, y, -1, SG_NEGCTL(g_negctl_lower_corner) == 1);
}

void push_triple_entry(Ctx &ctx, int32_t a, int32_t b, int32_t c) {
  int32_t &n = ctx.gs().unk_005a9cd4;
  if (n >= (SG_NEGCTL(g_negctl_push_triple) == 1 ? 0xFF : 0x100)) return;
  const int32_t i = n;
  (state_arr(ctx, &GameState::g_editQueueX) + i).set(a);
  (state_arr(ctx, &GameState::g_editQueueY) + i).set(b);
  (state_arr(ctx, &GameState::g_editQueueMode) + i).set(c);
  ctx.gs().unk_005a9cd4 = add32(i, 1);
}

void set_grids(Ctx &ctx, int32_t x, int32_t y, int32_t a, int32_t b) {
  const int32_t cell = add32(mul32(x, kGridW), y);
  (state_arr(ctx, &GameState::g_tileBuildCode) + cell).set(static_cast<uint8_t>(a));
  (state_arr(ctx, &GameState::g_tileRefund) + cell).set(static_cast<int8_t>(b));
  if (SG_NEGCTL(g_negctl_set_grids) == 1) (state_arr(ctx, &GameState::g_tileBuildCode) + add32(cell, 1)).set(static_cast<uint8_t>(b));
}

void flood_mark(Ctx &ctx, int32_t x, int32_t y) {
  const Grid<uint16_t> f = flags(ctx);
  const Ptr<uint16_t> here = f.cell(x, y);
  const uint16_t w = here.get();
  if (w & 0x40) return;
  const int32_t t = types(ctx)(x, y);
  here.set(static_cast<uint16_t>(w | 0x40));
  if (walk_of(ctx, t) <= 0) return;
  const uint16_t mask = SG_NEGCTL(g_negctl_flood_mark) == 1 ? 0x20 : 0x420;
  for (int32_t k = 0; k < 8; k += 2) {
    const int32_t nx = add32(dir_dx(ctx, k), x), ny = add32(dir_dy(ctx, k), y);
    const uint16_t w2 = f(nx, ny);
    if ((w2 & mask) != 0 && (w2 & 0x40) == 0) flood_mark(ctx, nx, ny);
  }
}

int32_t bilinear_sample(Ctx &ctx, int32_t a, int32_t b) {
  const int32_t ca = sub32(a, 0x80), cb = sub32(b, 0x80);
  const int32_t ix = sar32(ca, 8) & 0xF, iy = sar32(cb, 8) & 0xF;
  const int32_t fx = sar32(ca, 3) & 0x1F, fy = sar32(cb, 3) & 0x1F;
  const int32_t ifx = 0x20 - fx, ify = 0x20 - fy;
  const Ptr<int8_t> g = state_arr(ctx, &GameState::g_sampleGrid) + (iy + ix * 19);
  const int32_t c00 = g[0], c01 = g[1], c10 = g[19], c11 = g[20];
  const int32_t sum = c01 * ifx * fy + c10 * ify * fx + c00 * ifx * ify + c11 * fy * fx;
  return SG_NEGCTL(g_negctl_bilinear) == 1 ? sar32(sum, 5) : sum / 32;
}

void clear_tile(Ctx &ctx, int32_t x, int32_t y) {
  const Ptr<CourseRecord> rec = state_arr(ctx, &GameState::g_courseTable) + ctx.gs().g_courseIndex;
  const int8_t k = rec.field(&CourseRecord::setting).get();
  types(ctx).cell(x, y).set(static_cast<int8_t>(k == 2 ? 0x11 : 0x14));
  if (k == 2 && SG_NEGCTL(g_negctl_clear_tile) != 1) {
    const Ptr<uint16_t> f = flags(ctx).cell(x, y);
    f.set(static_cast<uint16_t>(f.get() & 0xFCDF));
  }
}

// ================================================================================================== level 1
int32_t can_step(Ctx &ctx, int32_t x, int32_t y, int32_t dir) {
  const int8_t t = types(ctx)(x, y);
  if (walk_of(ctx, t) <= 0) return 0;
  if (t == 0x16 || t == 0x15) return 0;
  const int32_t nx = add32(x, dir_dx(ctx, dir)), ny = add32(y, dir_dy(ctx, dir));
  const uint16_t mask = SG_NEGCTL(g_negctl_can_step) == 2 ? 0x100 : 0x120;
  if (flags(ctx)(nx, ny) & mask) return 0;
  if (walk_of(ctx, types(ctx)(nx, ny)) <= 0 && SG_NEGCTL(g_negctl_can_step) != 1) return 0;
  if (tile_blocked(ctx, nx, ny) != 0) return 0;
  return 1;
}

int32_t eval_placement_area(Ctx &ctx, int32_t x, int32_t y, int32_t n, int32_t kind) {
  const Grid<int8_t> ty = types(ctx);
  const Grid<uint16_t> fl = flags(ctx);
  // the type record's bytes +0x24 (amount) and +0x26 (collar group) read as signed bytes
  const auto amount = [&](int32_t t) { return static_cast<int32_t>(attr(ctx, t).field(&TileTypeAttr::u24).get()); };
  int32_t sum = 0;
  bool open_border = false;
  const bool ignore_border = SG_NEGCTL(g_negctl_eval_placement) == 1;
  if (n > -2) {
    for (int32_t i = -1; i <= n; ++i) {
      const int32_t xx = add32(x, i);
      for (int32_t j = -1; j <= n; ++j) {
        const int32_t yy = add32(y, j);
        if (i == -1 || j == -1 || i == n || j == n) {
          if (ty(xx, yy) != 0x11) open_border = true;
          continue;
        }
        if (kind == 0xC) {
          const int8_t ct = ctx.gs().g_courseType;
          if ((ct == 0 || ct == 2) && ty(xx, yy) != 0x11) return -1;
        }
        if (kind != 0) {
          const int8_t t = ty(xx, yy);
          if (t == 0x15) return -1;
          if (t == 0x16 || (fl(xx, yy) & 0x400)) {
            if (placed(ctx, object_at(ctx, xx, yy)).field(&PlacedObject::type).get() != kind) return -1;
          }
          if (ty(xx, yy) == 0) return -1;
          if (fl(xx, yy) & 0x8080) return -1;
        }
        if (tile_blocked(ctx, xx, yy)) return -1;
        const int8_t t = ty(xx, yy);
        if (collar_of(ctx, t) == 0xD) sum = add32(sum, amount(t));
        if (t == 0xC) sum = add32(sum, amount(0xC));
        if (t == 0x11 && kind != 0xC) sum = add32(sum, amount(0x11));
        if (t == 0x12) sum = add32(sum, amount(0x12));
        if (t == 0x13) sum = add32(sum, amount(0x13));
        if (fl(xx, yy) & 0x8000) return -1;
      }
    }
    if (open_border || ignore_border) return sum;
  }
  if (kind != 0 && kind != 4) return -1;
  return sum;
}

int32_t nearest_placed(Ctx &ctx, int32_t type, int32_t x, int32_t y) {
  int32_t best = -1;
  int32_t &dist = ctx.gs().g_nearTileDist;
  dist = 0xFFFF;
  for (int32_t i = 0; i < 256; ++i) {
    const Ptr<PlacedObject> r = placed(ctx, i);
    if (r.field(&PlacedObject::type).get() != type) continue;
    if (type >= 6 && !(r.field(&PlacedObject::flags).get() & 0x40) && SG_NEGCTL(g_negctl_nearest_placed) != 1) return best;
    const int32_t h = item_size(ctx, type) / 2;
    const int32_t dx = add32(sub32(shl32(add32(r.field(&PlacedObject::x).get(), h), 10), x), 0x200);
    const int32_t dy = add32(sub32(shl32(add32(r.field(&PlacedObject::y).get(), h), 10), y), 0x200);
    const int32_t d = distance(dx, dy);
    if (d < dist) {
      best = i;
      dist = d;
    }
  }
  return best;
}

void find_nearest_target_tile(Ctx &ctx, int32_t px, int32_t py, int32_t max_dist) {
  const int32_t x_lo = sub32(sar32(px, 10), 4), x_hi = add32(sar32(px, 10), 4);
  const int32_t y_lo = sub32(sar32(py, 10), 4), y_hi = add32(sar32(py, 10), 4);
  GameState &g = ctx.gs();
  g.g_nearTileDist = shl32(max_dist, 10);
  g.g_nearTileX = -1;
  const Grid<uint16_t> fl = flags(ctx);
  for (int32_t x = x_lo; x <= x_hi; ++x) {
    for (int32_t y = y_lo; y <= y_hi; ++y) {
      if (tile_blocked(ctx, x, y) && SG_NEGCTL(g_negctl_find_nearest_target) != 2) continue;
      if (!(fl(x, y) & 0x200)) continue;
      const int32_t bias = SG_NEGCTL(g_negctl_find_nearest_target) == 1 ? 0x100 : 0x200;
      const int32_t d = distance(add32(sub32(shl32(x, 10), px), bias), add32(sub32(shl32(y, 10), py), bias));
      if (d < g.g_nearTileDist) {
        g.g_nearTileDist = d;
        g.g_nearTileX = x;
        g.g_nearTileY = y;
      }
    }
  }
}

int32_t sample_height(Ctx &ctx, int32_t x, int32_t y) {
  x = sar32(x, 2);
  y = sar32(y, 2);
  const int32_t a = bilinear_sample(ctx, x, y) * (SG_NEGCTL(g_negctl_sample_height) == 1 ? 5 : 6);
  const int32_t b = bilinear_sample(ctx, mul32(x, 2), mul32(y, 2));
  const int32_t v = (a + b * 4) * 7 / 64 / 2;
  return SG_NEGCTL(g_negctl_sample_height) == 2 ? clamp(v, 0, 0x7FFFFFFF) : clamp(v, 0, 0x200);
}

uint32_t propagate_type11(Ctx &ctx, int32_t x, int32_t y) {
  const Ptr<uint8_t> here = variants(ctx).cell(x, y);
  int32_t v = here.get();
  for (int32_t d = 0; d < 8; ++d) {
    const int32_t nx = add32(dir_dx(ctx, d), x), ny = add32(dir_dy(ctx, d), y);
    if (tile_blocked(ctx, nx, ny) != 0) continue;
    if (collar_of(ctx, types(ctx)(nx, ny)) != 0x11) v = 0;
    if (variants(ctx)(nx, ny) == 0 && v != 0) v = 1;
  }
  if (v == here.get()) return here.va();  // eax: the cell's address
  here.set(static_cast<uint8_t>(v));
  uint32_t eax = 0;  // the original's eax: the last neighbour's tileBlocked, type * 0x30 or recursion result
  for (int32_t d = 0; d < 8; ++d) {
    const int32_t nx = add32(dir_dx(ctx, d), x), ny = add32(dir_dy(ctx, d), y);
    const int32_t blocked = tile_blocked(ctx, nx, ny);
    eax = u32(blocked);
    if (blocked != 0) continue;
    const int32_t t = types(ctx)(nx, ny);
    eax = u32(mul32(t, 0x30));
    if (collar_of(ctx, t) == 0x11 || SG_NEGCTL(g_negctl_propagate11) == 1) eax = propagate_type11(ctx, nx, ny);
  }
  return eax;
}

void rebuild_tile_coverage(Ctx &ctx) {
  const Ptr<PlacedObject> o0 = placed(ctx, 0);
  const int32_t py = o0.field(&PlacedObject::y).get();  // the clubhouse's tile
  const int32_t px = o0.field(&PlacedObject::x).get();
  const Ptr<uint16_t> fl0 = state_arr(ctx, &GameState::g_tileFlags);
  for (int32_t i = 0; i < 2500; ++i) {
    const Ptr<uint8_t> lo = (fl0 + i).as<uint8_t>();
    lo.set(static_cast<uint8_t>(lo.get() & 0xBF));
  }
  const int32_t w = item_size(ctx, 15);
  const Grid<uint16_t> fl = flags(ctx);
  for (int32_t row = px; row < add32(px, w); ++row)
    for (int32_t col = py; col < add32(py, w); ++col)
      if (fl(row, col) & 0x420) flood_mark(ctx, row, col);
  const Ptr<int32_t> lvl = state_arr(ctx, &GameState::g_objectTypeLevel);
  for (int32_t i = 0; i < 256; ++i) {
    const Ptr<PlacedObject> r = placed(ctx, i);
    const Ptr<uint8_t> flag = r.field(&PlacedObject::flags);
    const int32_t t = r.field(&PlacedObject::type).get();
    if (t < 6) continue;
    if (t >= 0x11) {
      if (SG_NEGCTL(g_negctl_rebuild_coverage) != 1) flag.set(static_cast<uint8_t>(flag.get() | 0x40));
      continue;
    }
    flag.set(static_cast<uint8_t>(flag.get() & 0xBF));
    int32_t n = item_size(ctx, t);
    if (t != 7) {
      const int32_t v = clamp(lvl[t], add32(r.field(&PlacedObject::value).get(), 1), 0x63);
      (lvl + t).set(v);
      n = add32(add32(v, n), -1);
    }
    for (int32_t dr = 0; dr < n; ++dr)
      for (int32_t dc = 0; dc < n; ++dc) {
        const int32_t idx = add32(add32(r.field(&PlacedObject::y).get(), dc), mul32(50, add32(r.field(&PlacedObject::x).get(), dr)));
        if ((fl0 + idx).as<uint8_t>().get() & 0x40) flag.set(static_cast<uint8_t>(flag.get() | 0x40));
      }
    const bool diff_test = SG_NEGCTL(g_negctl_rebuild_coverage) != 2;
    if ((diff_test && ctx.gs().g_difficulty == 0) || (ctx.gs().g_flags & 0x1000000u) != 0)
      flag.set(static_cast<uint8_t>(flag.get() | 0x40));
  }
}

int32_t relax_tile(Ctx &ctx, int32_t x, int32_t y) {
  int32_t lowered = 0;
  const Ptr<int8_t> here = levels(ctx).cell(x, y);
  const int32_t start = here.get();
  int32_t cur = start;
  for (int32_t d = 0; d < 8; d += 2) {
    const int32_t nx = add32(x, dir_dx(ctx, d)), ny = add32(y, dir_dy(ctx, d));
    if (tile_blocked(ctx, nx, ny) != 0) continue;
    if (static_cast<uint8_t>(types(ctx)(nx, ny)) != static_cast<uint8_t>(types(ctx)(x, y))) continue;
    const int32_t nh = levels(ctx)(nx, ny);
    if (nh >= (SG_NEGCTL(g_negctl_relax_tile) == 1 ? start : cur)) continue;
    cur = nh;
    here.set(static_cast<int8_t>(nh));
    lowered = 1;
  }
  return lowered;
}

int32_t raise_from_neighbours(Ctx &ctx, int32_t x, int32_t y, int32_t same_byte) {
  const Ptr<int8_t> here = levels(ctx).cell(x, y);
  int32_t best = here.get();
  int32_t changed = 0;
  if (SG_NEGCTL(g_negctl_raise_from_nb) == 1) same_byte = 0;
  for (int32_t k = 0; k < 8; k += 2) {
    const int32_t nx = add32(x, dir_dx(ctx, k)), ny = add32(y, dir_dy(ctx, k));
    if (tile_blocked(ctx, nx, ny)) continue;
    if (types(ctx)(nx, ny) != types(ctx)(x, y)) continue;
    if (same_byte && variants(ctx)(nx, ny) != variants(ctx)(x, y)) continue;
    const int32_t nl = levels(ctx)(nx, ny);
    if (nl > best) {
      best = nl;
      here.set(static_cast<int8_t>(best));
      changed = 1;
    }
  }
  return changed;
}

void spread_shade(Ctx &ctx, int32_t row, int32_t col, int32_t value) {
  const int32_t amount = add32(value, value) / 3;
  const Grid<uint8_t> plus = state_grid(ctx, &GameState::g_tileShadeB, kGridW);
  const Grid<uint8_t> minus = state_grid(ctx, &GameState::g_tileShadeA, kGridW);
  const int32_t extra = SG_NEGCTL(g_negctl_spread_shade) == 1 ? 1 : 2;
  for (int32_t di = 0; di < 15; ++di) {
    const int32_t r = add32(row, di - 7), rw = -21 + 3 * di;
    for (int32_t dj = 0; dj < 15; ++dj) {
      const int32_t c = add32(col, dj - 7), cw = -21 + 3 * dj;
      if (tile_blocked(ctx, r, c) != 0) continue;
      const int32_t d = approx_distance(rw, cw) + extra;
      if (amount > 0) {
        const Ptr<uint8_t> p = plus.cell(r, c);
        p.set(static_cast<uint8_t>(clamp(p.get() + amount / d, 0, 0xFF)));
      } else {
        const Ptr<uint8_t> p = minus.cell(r, c);
        p.set(static_cast<uint8_t>(clamp(p.get() - amount / d, 0, 0xFF)));
      }
    }
  }
}

// ================================================================================================== level 2
int32_t height_blend(Ctx &ctx, int32_t x, int32_t y) {
  GameState &g = ctx.gs();
  if ((g.g_flags & 1u) || x < 0 || x >= 50 || y < 0 || y >= 50) return 3;
  if (g.g_date != 0) {
    if (types(ctx)(x, y) == 0x14 && tile_blocked(ctx, x - 1, y) && tile_blocked(ctx, x, y + 1)) return 3;
    return (state_arr(ctx, &GameState::g_cornerLevel) + (x * kCornerW + y)).get();
  }
  const int8_t t = types(ctx)(x, y);
  if (t == 0x11 || types(ctx)(x + 1, y) == 0x11 || t == 0x12 || t == 0x13) return 3;
  const Ptr<CourseRecord> c = state_arr(ctx, &GameState::g_courseTable) + g.g_courseIndex;
  const int32_t sh = c.field(&CourseRecord::relief).get() == 2 ? 7 : 6;
  int32_t h = sample_height(ctx, shl32(x, sh), shl32(y, sh));
  const int32_t d = g.g_heightRelief;
  if (c.field(&CourseRecord::course_type).get() == 1 && y < 16) h = add32(h, mul32(16 - y, d) / -6);
  const int32_t lo = (g.g_difficulty != 0 ? 1 : 0) + (SG_NEGCTL(g_negctl_height_blend) == 1 ? 2 : 3);
  return clamp(add32(div32(h, d), 1), lo, 15);
}

int32_t rate_tile(Ctx &ctx, int32_t i) {
  const Ptr<Golfer> gp = state_arr(ctx, &GameState::g_golfers) + i;
  const bool flag1 = (gp.field(&Golfer::prefs).get() & 1) != 0;
  int32_t v = (flag1 ? 50 : 0) + 150;
  if (ctx.gs().g_difficulty >= 1)
    v += gp.field(&Golfer::cls).get() * 50 / 3;
  else
    v += flag1 ? 40 : 25;
  int32_t k = type_at_pos(ctx, gp.field(&Golfer::ball_x).get(), gp.field(&Golfer::ball_y).get());
  if (i >= 0x98) k = gp.field(&Golfer::strokes).as<uint8_t>().get() ? 2 : 0;
  if (gp.field(&Golfer::kind).get()) {
    const uint16_t ce = static_cast<uint16_t>(gp.field(&Golfer::skill_mask).get());
    const Ptr<uint8_t> sk = gp.field(&Golfer::skill);
    if (ce & 1) v += sk[0] * 4 - 20;
    if ((ce & 2) && !k) v += (sk[1] - 5) * 6;
  }
  const int8_t ee = gp.field(&Golfer::toward_zero36).get();
  if (ee > 0) v += clamp(ee, 0, SG_NEGCTL(g_negctl_rate_tile) == 1 ? 2 : 3) * v / 24;
  if (flag1) v = add32(v, mul32(ctx.gs().g_activeObjectLevel[10], 15));
  const int8_t m2 = walk_of(ctx, k);
  if (m2 > 0) v = sub32(v, mul32(clamp(m2, 0, 3), v) / 8);
  if (k) v = add32(v, v / -5);
  return v > 330 ? 330 : v;
}

uint32_t mark_tile_kind11(Ctx &ctx) {
  const Grid<int8_t> ty = types(ctx);
  const Grid<uint8_t> var = variants(ctx);
  for (int32_t x = 0; x < 50; ++x)
    for (int32_t y = 0; y < 50; ++y)
      if (static_cast<uint8_t>(collar_of(ctx, ty(x, y))) == 0x11)
        var.cell(x, y).set(SG_NEGCTL(g_negctl_mark_kind11) == 1 ? 1 : 2);
  uint32_t eax = 0;  // the original's eax: the last cell's type (sign-extended) or its propagateType11 result
  for (int32_t x = 0; x < 50; ++x)
    for (int32_t y = 0; y < 50; ++y) {
      eax = u32(ty(x, y));
      if (static_cast<uint8_t>(collar_of(ctx, ty(x, y))) == 0x11) eax = propagate_type11(ctx, x, y);
    }
  return eax;
}

void spread_all_shade(Ctx &ctx) {
  GameState &g = ctx.gs();
  int32_t total = 0;
  for (int32_t k = 0; k < 2500; ++k) total += g.g_ballStopCount[k];
  for (int32_t k = 0; k < 2500; ++k) g.g_tileShadeB[k] = 0;
  for (int32_t k = 0; k < 2500; ++k) g.g_tileShadeA[k] = 0;
  if (total < 10) return;
  if (total > 0x6D6) total = SG_NEGCTL(g_negctl_spread_all_shade) == 1 ? 0x6D5 : 0x6D6;
  for (int32_t i = 0; i < 50; ++i)
    for (int32_t j = 0; j < 50; ++j) {
      const int32_t cell = i * 50 + j;
      if (g.g_tileMoodUp[cell] != 0) spread_shade(ctx, i, j, g.g_tileMoodUp[cell] * 20000 / total);
      if (g.g_tileMoodDown[cell] != 0) spread_shade(ctx, i, j, g.g_tileMoodDown[cell] * -40000 / total);
    }
}

void gen_noise(Ctx &ctx) {
  GameState &g = ctx.gs();
  uint8_t *noise = reinterpret_cast<uint8_t *>(g.g_noiseSource);
  uint8_t *out = reinterpret_cast<uint8_t *>(g.g_sampleGrid);
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  const uint32_t n = SG_NEGCTL(g_negctl_gen_noise) == 1 ? 0x0F : 0x10;
  for (int r = 0; r < 18; ++r) {
    uint8_t *row = noise + r * 0x13;
    for (int c = 0; c < 0x12; ++c) row[c] = static_cast<uint8_t>(random_range(seed, n));
    row[0x12] = row[0];
  }
  for (int c = 0; c < 0x12; ++c) noise[18 * 0x13 + c] = noise[c];
  for (int r = 0; r < 17; ++r)
    for (int c = 0; c < 17; ++c) out[r * 0x13 + c] = noise[r * 0x13 + c];
  for (int r = 0; r < 17; ++r) out[r * 0x13 + 16] = out[r * 0x13];
  for (int c = 0; c < 17; ++c) out[16 * 0x13 + c] = out[c];
}

// ================================================================================================== level 3
int32_t scan_tile_line(Ctx &ctx, int32_t x0, int32_t y0, int32_t tx, int32_t ty, int32_t range, int32_t curve,
                       int32_t strict) {
  // type 1's walk byte reads 0 while the scan runs and 0xff afterwards (the original's own stores)
  const Ptr<int8_t> walk1 = attr(ctx, 1).field(&TileTypeAttr::walk);
  walk1.set(0);
  const int mode = SG_NEGCTL(g_negctl_scan_tile_line);
  const int32_t txw = shl32(tx, 10), tyw = shl32(ty, 10);
  const int32_t dx = add32(sub32(txw, x0), 0x200), dy = add32(sub32(tyw, y0), 0x200);
  const int32_t angle = angle_fixed(dx, dy);
  const int32_t dir = sar32(add32(sar32(angle, 28), 1), 1) & 7;
  const int32_t n = sar32(add32(distance(dx, dy), 0x200), 10);
  const int32_t steps = clamp(n, 0, range / 25 + 1);
  if (n < 0) {
    walk1.set(static_cast<int8_t>(0xFF));
    return 0;
  }
  const int32_t half = steps / 2;
  const auto ttype = [&](int32_t cell) { return static_cast<int32_t>((state_arr(ctx, &GameState::g_tileType) + cell).get()); };
  const auto byte2 = [&](int32_t t) { return static_cast<int32_t>(walk_of(ctx, t)); };
  const auto is13 = [&](int32_t t) { return static_cast<uint8_t>(collar_of(ctx, t)) == 0xD; };
  const auto sub_tile = [](int32_t v) { return sar32(v % 0x400, 4); };
  uint32_t acc = u32(steps) * u32(curve) * 0x15555554u;
  const uint32_t acc_step = u32(curve) * 0xEAAAAAACu;
  int32_t L = shl32(steps, 10), R = 0x200;
  int32_t score = 0, count13 = 0;
  for (int32_t i = 0; i <= n; ++i) {
    int32_t off = 0;
    if (curve != 0) {
      if (i <= half) off = i32(u32(curve) * 0x0FFFFFFCu);
      else if (i < steps) off = div32(i32(acc), (steps + 1) / 2);
    }
    const int32_t a = add32(angle, off);
    int32_t px = add32(x0, fold_range(ctx, a, shl32(i, 10)));
    int32_t py = sub32(y0, abs_diff(ctx, a, shl32(i, 10)));
    if (curve != 0 && i > half && i < steps) {
      const int32_t a2 = i32(u32(angle) - u32(curve) * 0x15555554u);
      px = add32(sub32(txw, fold_range(ctx, a2, L)), 0x200);
      py = add32(add32(abs_diff(ctx, a2, L), tyw), 0x200);
    }
    const int32_t fx = sub_tile(px), fy = sub_tile(py);
    const int32_t cell = add32(sar32(py, 10), mul32(sar32(px, 10), 50));
    const int32_t lead = steps <= 5 ? 1 : (steps >= 9 ? 3 : 2);
    const int32_t t = ttype(cell);
    if ((i >= steps - lead && strict == 0) || is13(t)) {
      const int32_t b2 = byte2(t);
      if (b2 > 0) score = add32(score, b2);
      if (is13(t)) score = add32(score, 0x20 / (i + 1));
      if (t == 0x11 || tile_blocked(ctx, sar32(px, 10), sar32(py, 10))) score = add32(score, 0x10);
      const int32_t ddx = dir_dx(ctx, dir), ddy = dir_dy(ctx, dir);
      if (fx < 0xF && ddy) score = add32(score, byte2(ttype(cell - 50)) / 2);
      if (fx > 0x30 && ddy) score = add32(score, byte2(ttype(cell + 50)) / 2);
      if (fy < 0xF && ddx) score = add32(score, byte2(ttype(cell - 1)) / 2);
      if (fy > 0x30 && ddx && mode != 1) score = add32(score, byte2(ttype(cell + 1)) / 2);
    }
    const int32_t qx = add32(x0, fold_range(ctx, angle, R));
    const int32_t qy = sub32(y0, abs_diff(ctx, angle, R));
    if (is13(ttype(add32(sar32(qy, 10), mul32(sar32(qx, 10), 50))))) ++count13;
    acc += acc_step;
    L = sub32(L, 0x400);
    R = add32(R, 0x400);
  }
  int32_t r = score;
  if (count13 >= (mode == 2 ? 1 : 2)) ++r;
  if (r < 0) r = 0;
  walk1.set(static_cast<int8_t>(0xFF));
  return r;
}

// ================================================================================================== heightfield
namespace {
uint32_t type_flags(Ctx &c, int32_t type) { return attr(c, type).field(&TileTypeAttr::flags).get(); }
// cornerRange over two output words: each step reads them back and writes them like the original's pointers
void corner_range_mem(Ctx &ctx, int32_t x, int32_t y, Ptr<int32_t> pmax, Ptr<int32_t> pmin) {
  const int32_t v = height_blend(ctx, x, y);
  pmax.set(v);
  pmin.set(v);
  const int32_t xs[3] = {add32(x, 1), add32(x, 1), x};
  const int32_t ys[3] = {y, sub32(y, 1), sub32(y, 1)};
  for (int i = 0; i < 3; ++i) {
    const int32_t w = height_blend(ctx, xs[i], ys[i]);
    if (w > pmax.get() && SG_NEGCTL(g_negctl_corner_range) != 1) pmax.set(w);
    if (w < pmin.get()) pmin.set(w);
  }
}
}  // namespace

void corner_range(Ctx &ctx, int32_t x, int32_t y, int32_t &mx, int32_t &mn) {
  const int32_t v = height_blend(ctx, x, y);
  mx = mn = v;
  const int32_t xs[3] = {add32(x, 1), add32(x, 1), x};
  const int32_t ys[3] = {y, sub32(y, 1), sub32(y, 1)};
  for (int i = 0; i < 3; ++i) {
    const int32_t w = height_blend(ctx, xs[i], ys[i]);
    if (w > mx && SG_NEGCTL(g_negctl_corner_range) != 1) mx = w;
    if (w < mn) mn = w;
  }
}

int32_t corner_heights(Ctx &ctx, int32_t x, int32_t y, int32_t corner, int32_t flag) {
  if (!(u32(x) < 50u && u32(y) < 50u && (corner & 1))) return 3;
  const int32_t cell = y + x * 50;
  if (flag != 0) {
    const int8_t cached = (state_arr(ctx, &GameState::g_cornerCache) + add32(cell * 8, corner)).as<int8_t>().get();
    if (cached != 0) return cached;
  }
  const uint32_t f = type_flags(ctx, types(ctx)(x, y));
  if (f & 2) {
    if (f & 1) return levels(ctx)(x, y);
    int32_t mx, mn;
    corner_range(ctx, x, y, mx, mn);
    return mn;
  }
  if (f & 4) {
    if (f & 1) return levels(ctx)(x, y);
    int32_t mx, mn;
    corner_range(ctx, x, y, mx, mn);
    return mx;
  }
  if (f & 8) return 3;
  const int32_t b1 = height_blend(ctx, x + 1, y - 1);
  const int32_t b3 = height_blend(ctx, x + 1, y);
  const int32_t b5 = height_blend(ctx, x, y);
  const int32_t b7 = height_blend(ctx, x, y - 1);
  switch (corner & 7) {
    case 1: return b1;
    case 3: return SG_NEGCTL(g_negctl_corner_heights) == 1 ? b5 : b3;
    case 5: return b5;
    default: return b7;
  }
}

int32_t tile_query449310(Ctx &ctx, int32_t a, int32_t b, int32_t c) {
  return corner_heights(ctx, a, b, c, SG_NEGCTL(g_negctl_tile_query) == 1 ? 0 : 1);
}

namespace {
// the four corners 5, 7, 1, 3 (cached) of a slope tile, or false (g_flags bit 0, types 7 / 9, type flags & 0xe)
bool slope_corners(Ctx &ctx, int32_t tx, int32_t ty, int32_t c[4]) {
  if (ctx.gs().g_flags & 1u) return false;
  const int8_t t = types(ctx)(tx, ty);
  if (t == 7 || t == 9) return false;
  if (type_flags(ctx, t) & 0xE) return false;
  c[0] = corner_heights(ctx, tx, ty, 5, 1);
  c[1] = corner_heights(ctx, tx, ty, 7, 1);
  c[2] = corner_heights(ctx, tx, ty, 1, 1);
  c[3] = corner_heights(ctx, tx, ty, 3, 1);
  return true;
}
}  // namespace

int32_t slope_y(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t tx = sar32(x, 10), ty = sar32(y, 10);
  int32_t c[4];
  if (!slope_corners(ctx, tx, ty, c)) return 0;
  const int32_t half = SG_NEGCTL(g_negctl_slope_y) == 1 ? 0x1FF : 0x200;
  return y > add32(shl32(ty, 10), half) ? c[3] - c[0] : c[2] - c[1];
}

int32_t slope_x(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t tx = sar32(x, 10), ty = sar32(y, 10);
  int32_t c[4];
  if (!slope_corners(ctx, tx, ty, c)) return 0;
  if (SG_NEGCTL(g_negctl_slope_x) == 1) return x > add32(shl32(tx, 10), 0x200) ? c[3] - c[2] : c[1] - c[0];
  return x > add32(shl32(tx, 10), 0x200) ? c[3] - c[2] : c[0] - c[1];
}

int32_t slope_mix(Ctx &ctx, int32_t x, int32_t y, int32_t dir) {
  int32_t p = slope_y(ctx, x, y);
  int32_t q = slope_x(ctx, x, y);
  if (dir & 1) {
    p = clamp(p, -1, 1);
    q = clamp(q, -1, 1);
  }
  if (SG_NEGCTL(g_negctl_slope_mix) == 1) std::swap(p, q);
  return add32(mul32(dir_dy(ctx, dir), q), mul32(dir_dx(ctx, dir), p));
}

void relax_edges(Ctx &ctx, int32_t x, int32_t y) {
  int32_t mx, mn;
  corner_range(ctx, x, y, mx, mn);  // outputs discarded
  const Ptr<int8_t> p = state_grid(ctx, &GameState::g_tileWallMask, kGridW).cell(x, y);
  p.set(0);
  for (int32_t k = 0; k < 8; k += 2) {
    const int32_t nx = add32(x, dir_dx(ctx, k)), ny = add32(y, dir_dy(ctx, k));
    if (tile_blocked(ctx, nx, ny)) continue;
    const int8_t bit = static_cast<int8_t>(1 << k);
    if (corner_heights(ctx, x, y, k - 1, 0) < corner_heights(ctx, nx, ny, k + 5, 0)) p.set(static_cast<int8_t>(p.get() | bit));
    if (SG_NEGCTL(g_negctl_relax_edges) == 1) continue;
    if (corner_heights(ctx, x, y, k + 1, 0) < corner_heights(ctx, nx, ny, k + 3, 0)) p.set(static_cast<int8_t>(p.get() | bit));
  }
}

int32_t height_at(Ctx &ctx, int32_t x, int32_t y) {
  const int32_t tx = sar32(x, 10), ty = sar32(y, 10);
  const uint32_t f = type_flags(ctx, types(ctx)(tx, ty));
  if (f & 8) return 0;
  if (f & 6) {
    int32_t mx, mn;
    corner_range(ctx, tx, ty, mx, mn);
    return f & 2 ? (mn - 3) * 16 : (mx - 3) * 16;
  }
  const int32_t h5 = corner_heights(ctx, tx, ty, 5, 1) - 3, h7 = corner_heights(ctx, tx, ty, 7, 1) - 3;
  const int32_t h1 = corner_heights(ctx, tx, ty, 1, 1) - 3, h3 = corner_heights(ctx, tx, ty, 3, 1) - 3;
  if (h5 == h7 && h5 == h1 && h5 == h3) return h5 * 16;
  const int32_t fx = sub32(x, shl32(tx, 10)), fy = sub32(y, shl32(ty, 10));
  const int32_t gx = SG_NEGCTL(g_negctl_height_at) == 1 ? 0x3FF : 0x400;
  const int32_t a = add32(mul32(sub32(gx, fx), h7), mul32(fx, h1));
  const int32_t b = add32(mul32(sub32(0x400, fx), h5), mul32(fx, h3));
  return mul32(add32(mul32(a, sub32(0x400, fy)), mul32(b, fy)), 16) / 1024 / 1024;
}

uint32_t rebuild_heightfield(Ctx &ctx) {
  GameState &g = ctx.gs();
  g.f_w_4c2e00[1] = -1;
  for (int i = 0; i < 2500; ++i) g.g_tileLevel[i] = 0;
  for (int i = 0; i < 20000; ++i) g.g_cornerCache[i] = 0;
  const Grid<int8_t> ty = types(ctx);
  for (int32_t x = 0; x < 50; ++x) {
    for (int32_t y = 0; y < 50; ++y) {
      const int32_t cell = x * 50 + y;
      uint8_t *c = g.g_cornerCache + cell * 8;
      c[1] = static_cast<uint8_t>(corner_heights(ctx, x, y, 1, 0));
      c[3] = static_cast<uint8_t>(corner_heights(ctx, x, y, 3, 0));
      c[5] = static_cast<uint8_t>(corner_heights(ctx, x, y, 5, 0));
      c[7] = static_cast<uint8_t>(corner_heights(ctx, x, y, 7, 0));
      if (type_flags(ctx, ty(x, y)) & 6) {
        int32_t third, fourth;
        corner_range(ctx, x, y, third, fourth);
        const uint32_t f = type_flags(ctx, ty(x, y));
        if (f & 2) g.g_tileLevel[cell] = static_cast<int8_t>(fourth);
        if ((f & 4) && SG_NEGCTL(g_negctl_rebuild_heightfield) != 1) g.g_tileLevel[cell] = static_cast<int8_t>(third);
      }
    }
  }
  // pass 2 repeats until a sweep changes nothing; its state is the level grid alone, so a grid seen before means
  // the original loops forever (NoReturn)
  std::vector<std::vector<int8_t>> seen;
  for (;;) {
    int32_t changed = 0;
    for (int32_t x = 0; x < 50; ++x) {
      for (int32_t y = 0; y < 50; ++y) {
        const uint32_t f = type_flags(ctx, ty(x, y));
        if (!(f & 1)) continue;
        if (f & 2) changed = add32(changed, relax_tile(ctx, x, y));
        if (type_flags(ctx, ty(x, y)) & 4) {
          const int8_t same = attr(ctx, ty(x, y)).field(&TileTypeAttr::u27).get();
          changed = add32(changed, raise_from_neighbours(ctx, x, y, same == 0x10 ? 1 : 0));
        }
      }
    }
    if (!changed) break;
    std::vector<int8_t> now(g.g_tileLevel, g.g_tileLevel + 2500);
    for (const auto &s : seen)
      if (s == now) throw NoReturn{"rebuildHeightfield"};
    seen.push_back(std::move(now));
  }
  for (int32_t x = 0; x < 50; ++x)
    for (int32_t y = 0; y < 50; ++y) relax_edges(ctx, x, y);
  g.g_tileWallMask[0] = static_cast<int8_t>((g.g_tileWallMask[0] & 0xFD) | 8);
  g.g_flags &= 0xFFFBFFFFu;
  return g.g_flags;  // eax at the original's ret
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_free_at_tile(Ctx &c, const uint32_t *a, Ret &) { free_at_tile(c, S(a, 0), S(a, 1)); }
void a_raise_corner(Ctx &c, const uint32_t *a, Ret &) { raise_corner_levels_around(c, S(a, 0), S(a, 1)); }
void a_lower_corner(Ctx &c, const uint32_t *a, Ret &) { lower_corner_levels_around(c, S(a, 0), S(a, 1)); }
void a_push_triple(Ctx &c, const uint32_t *a, Ret &) { push_triple_entry(c, S(a, 0), S(a, 1), S(a, 2)); }
void a_set_grids(Ctx &c, const uint32_t *a, Ret &) { set_grids(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)); }
void a_flood_mark(Ctx &c, const uint32_t *a, Ret &) { flood_mark(c, S(a, 0), S(a, 1)); }
void a_bilinear(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(bilinear_sample(c, S(a, 0), S(a, 1))); }
void a_clear_tile(Ctx &c, const uint32_t *a, Ret &) { clear_tile(c, S(a, 0), S(a, 1)); }
void a_can_step(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(can_step(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_eval_placement(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(eval_placement_area(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)));
}
void a_nearest_placed(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(nearest_placed(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_find_nearest_target(Ctx &c, const uint32_t *a, Ret &) { find_nearest_target_tile(c, S(a, 0), S(a, 1), S(a, 2)); }
void a_sample_height(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(sample_height(c, S(a, 0), S(a, 1))); }
void a_propagate11(Ctx &c, const uint32_t *a, Ret &) { propagate_type11(c, S(a, 0), S(a, 1)); }
void a_rebuild_coverage(Ctx &c, const uint32_t *, Ret &) { rebuild_tile_coverage(c); }
void a_relax_tile(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(relax_tile(c, S(a, 0), S(a, 1))); }
void a_raise_from_nb(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(raise_from_neighbours(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_spread_shade(Ctx &c, const uint32_t *a, Ret &) { spread_shade(c, S(a, 0), S(a, 1), S(a, 2)); }
void a_height_blend(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(height_blend(c, S(a, 0), S(a, 1))); }
void a_rate_tile(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(rate_tile(c, S(a, 0))); }
void a_mark_kind11(Ctx &c, const uint32_t *, Ret &) { mark_tile_kind11(c); }
void a_spread_all_shade(Ctx &c, const uint32_t *, Ret &) { spread_all_shade(c); }
void a_gen_noise(Ctx &c, const uint32_t *, Ret &) { gen_noise(c); }
void a_corner_range(Ctx &c, const uint32_t *a, Ret &) {
  corner_range_mem(c, S(a, 0), S(a, 1), Ptr<int32_t>(c.mem, a[2]), Ptr<int32_t>(c.mem, a[3]));
}
void a_corner_heights(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(corner_heights(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }
void a_tile_query(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(tile_query449310(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_slope_y(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(slope_y(c, S(a, 0), S(a, 1))); }
void a_slope_x(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(slope_x(c, S(a, 0), S(a, 1))); }
void a_slope_mix(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(slope_mix(c, S(a, 0), S(a, 1), S(a, 2))); }
void a_relax_edges(Ctx &c, const uint32_t *a, Ret &) { relax_edges(c, S(a, 0), S(a, 1)); }
void a_height_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(height_at(c, S(a, 0), S(a, 1))); }
void a_rebuild_heightfield(Ctx &c, const uint32_t *, Ret &) { rebuild_heightfield(c); }
void a_scan_tile_line(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(scan_tile_line(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3), S(a, 4), S(a, 5), S(a, 6)));
}

const FuncDef kFuncs[] = {
    {"freeAtTile", "default", "void", 2, 0, a_free_at_tile, &g_negctl_free_at_tile},
    {"raiseCornerLevelsAround", "default", "void", 2, 0, a_raise_corner, &g_negctl_raise_corner},
    {"lowerCornerLevelsAround", "default", "void", 2, 0, a_lower_corner, &g_negctl_lower_corner},
    {"pushTripleEntry", "default", "void", 3, 0, a_push_triple, &g_negctl_push_triple},
    {"setGrids", "default", "void", 4, 0, a_set_grids, &g_negctl_set_grids},
    {"floodMark", "default", "void", 2, 0, a_flood_mark, &g_negctl_flood_mark},
    {"bilinearSample", "default", "int", 2, 0, a_bilinear, &g_negctl_bilinear},
    {"clearTile", "default", "void", 2, 0, a_clear_tile, &g_negctl_clear_tile},
    {"canStep", "default", "int", 3, 0, a_can_step, &g_negctl_can_step},
    {"evalPlacementArea", "default", "int", 4, 0, a_eval_placement, &g_negctl_eval_placement},
    {"nearestPlaced", "default", "int", 3, 0x027F, a_nearest_placed, &g_negctl_nearest_placed},
    {"findNearestTargetTile", "default", "void", 3, 0x027F, a_find_nearest_target, &g_negctl_find_nearest_target},
    {"sampleHeight", "default", "int", 2, 0, a_sample_height, &g_negctl_sample_height},
    {"propagateType11", "default", "void", 2, 0, a_propagate11, &g_negctl_propagate11},
    {"rebuildTileCoverage", "default", "void", 1, 0, a_rebuild_coverage, &g_negctl_rebuild_coverage},
    {"relaxTile42f630", "default", "int", 2, 0, a_relax_tile, &g_negctl_relax_tile},
    {"raiseFromNeighbours", "default", "int", 3, 0, a_raise_from_nb, &g_negctl_raise_from_nb},
    {"spreadShade", "default", "void", 3, 0, a_spread_shade, &g_negctl_spread_shade},
    {"heightBlend", "default", "int", 2, 0, a_height_blend, &g_negctl_height_blend},
    {"rateTile", "default", "int", 1, 0, a_rate_tile, &g_negctl_rate_tile},
    {"markTileKind11", "default", "void", 0, 0, a_mark_kind11, &g_negctl_mark_kind11},
    {"spreadAllShade", "default", "void", 0, 0, a_spread_all_shade, &g_negctl_spread_all_shade},
    {"genNoise", "default", "void", 0, 0x027F, a_gen_noise, &g_negctl_gen_noise},
    {"cornerRange", "default", "void", 4, 0, a_corner_range, &g_negctl_corner_range},
    {"cornerHeights", "default", "int", 4, 0, a_corner_heights, &g_negctl_corner_heights},
    {"tileQuery449310", "default", "int", 3, 0, a_tile_query, &g_negctl_tile_query},
    {"slopeY", "default", "int", 2, 0, a_slope_y, &g_negctl_slope_y},
    {"slopeX", "default", "int", 2, 0, a_slope_x, &g_negctl_slope_x},
    {"slopeMix", "default", "int", 3, 0, a_slope_mix, &g_negctl_slope_mix},
    {"relaxEdges42f530", "default", "void", 2, 0, a_relax_edges, &g_negctl_relax_edges},
    {"heightAt42fa30", "default", "int", 2, 0, a_height_at, &g_negctl_height_at},
    {"rebuildHeightfield", "default", "void", 0, 0, a_rebuild_heightfield, &g_negctl_rebuild_heightfield},
    {"scanTileLine", "default", "int", 7, 0x027F, a_scan_tile_line, &g_negctl_scan_tile_line},
};
}  // namespace

extern const ModuleDef kModule_terrain = {"terrain", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
