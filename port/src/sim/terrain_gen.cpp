// The random course generator of the terrain module (see portsim/sim/terrain_gen.h). Phase 3.1a, written from the
// specification (re/analysis/terrain/00470a60_applyTerrainEdit.md, the C3 hook source as reading reference, the
// disassembly); no original address or code. Variable names follow the note's sections A..M; (x, y) = (row, column) of
// the note: cell = x * 50 + y, corner level x * 51 + y.
#include "portsim/sim/terrain_gen.h"

#include <initializer_list>

#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/terrain.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/tick.h"
#include "portsim/sim/util.h"
#include "portsim/sim/world.h"  // P38A: grid bases and the world size

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   generateRandomCourse  1 = camera y = clubhouse column - 3, 2 = the border sweep (setting != 2) skips its third
//                         clearTile (the far column), 3 = the corner-level fill stops at 50 x 50 (row / column 50 kept),
//                         4 = placementBound 0 on location 4 still stamps the object at the last draw, 5 = placementBound 0
//                         on locations 5, 6, 8, 12, 13, 15 still runs the object pass; P38C (only at N != 50): 6 = the
//                         counts of scattered things not scaled by the area, 7 = the lengths not scaled by the side
int g_negctl_generate = 0;
int g_negctl_flat_course = 0;  // P38B generateFlatCourse: 1 = the owned square not scaled by N / 50

// Compatibility switches (port/CORE.md "Defects"), 1 = behave like the original.
//   generateRandomCourse.searchBound  model_defects#52 (D52b1, D52b2): the type-0x11 search of layout kind 1 and the
//                            clubhouse search never end when their window holds no qualifying cell. 1 = NoReturn once
//                            no cell can qualify; fixed (0): the type-0x11 search gives up and skips that feature, the
//                            clubhouse search takes the window centre (x 0x17, y 0x17) and goes on.
int g_defect_search_bound = 1;
//   generateRandomCourse.placementBound  model_defects#52 (D52b3): the two feature-site searches gated by
//                            eval_placement_area (location 4: a 2 x 2 site, kind 4; locations 5, 6, 8, 12, 13, 15: a 5 x 5
//                            site, kind 0xF) never end when no cell the draws can reach is accepted. 1 = NoReturn once
//                            no cell is accepted; fixed (0): the search gives up and the feature is skipped. A second
//                            switch so that searchBound (D52b1, D52b2) keeps its meaning.
int g_defect_placement_bound = 1;
//   generateRandomCourse.startingFeatureOutside  P24E (play profile, Mariano's play-test 11 item 5): the clubhouse spot is drawn
//                            in rows / columns 0xF..0x1F and the free feature of locations 2, 4, 9, 11, 14 (kinds 0xD, 9, 6,
//                            0xE, 8: the practice green is kind 6) is stamped at (row - 1, column + 4), or (row - 1, column - 5)
//                            when the tile (row, column + 5) is off the grid, with no test against the border sweep that runs
//                            after them, so a clubhouse column of 30 or 31 on a 50-acre course puts the green past the edge.
//                            1 = the original. 0 = when the drawn spot leaves a tile of the clubhouse or of the feature on a
//                            tile the sweep will clear, the clubhouse takes the nearest spot of the same window that passes
//                            the original's own test (base type, evalPlacementArea) and keeps both inside; no draw is added.
int g_defect_feature_outside = 1;
//   generateRandomCourse.riverOutside  model_defects#6 (D6, P28A, decision G30, log/roadmap/P28/P28A_notes.md): the river's shore
//                            tile (nrow, column + 1) is read and, when heightBlend answers 3 there, written with no grid
//                            test: on a step at column 49 the index column + 1 = 50 is tile (nrow + 1, 0) on the west edge
//                            (heightBlend always answers 3 for column 50), and nrow = -1 / 50 reaches the course table / the
//                            bytes after the grid; the loop's end test also reads (row, column) after a step that left the
//                            grid. Fixed (0): a tile outside the grid is neither read nor written (it does not exist), and
//                            the end test leaves the loop for an outside (row, column) without the read (the loop's own
//                            bound row 0..0x31 / column >= 0 ends it there in both settings).
int g_defect_river_outside = 1;
//   generateRandomCourse.smoothingOutside  model_defects#6 (D6, P28A): the level smoothing next to water runs rows and columns
//                            0..50, so the cells of row / column 50 read tile_type one past the grid (column 50 = the next
//                            row's column 0), and a neighbour type test of an edge cell reads outside too. Fixed (0): a cell
//                            outside the grid has type 0x14 (tileBlocked / typeAtPos), so a row / column 50 cell is not water
//                            and smooths nothing, and an outside neighbour is not 0x11.
int g_defect_smoothing_outside = 1;

Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  uint32_t a[8] = {};
  int k = 0;
  for (uint32_t v : args) a[k++] = v;
  Ret r;
  registry_at(i, nullptr)->fn(ctx, a, r);
  return r;
}

// A search loop of the original that retries random cells until one qualifies. The state does not change while it
// misses, so after kProbe consecutive misses the port checks every cell the RNG can produce once: none qualifies =
// the original never leaves the loop. (Only an empty candidate set is treated as proof; a non-empty one keeps looping.)
constexpr int32_t kProbe = 1 << 16;
template <class F> void miss(int32_t &misses, F any_candidate) {
  if (++misses == kProbe && !any_candidate()) throw NoReturn{"generateRandomCourse"};
}
// The same probe for the two searches the fixed profile bounds: true = give up (switch 0 and no cell qualifies).
template <class F> bool miss_bounded(int32_t &misses, F any_candidate) {
  if (++misses != kProbe || any_candidate()) return false;
  if (SG_DEFECT(g_defect_search_bound)) throw NoReturn{"generateRandomCourse"};
  return true;
}
// The same probe for the two eval_placement_area searches (placementBound).
template <class F> bool miss_placement(int32_t &misses, F any_candidate) {
  if (++misses != kProbe || any_candidate()) return false;
  if (SG_DEFECT(g_defect_placement_bound)) throw NoReturn{"generateRandomCourse"};
  return true;
}

// P38C (log/roadmap/P38/P38C_notes.md): the generator at N != 50. Lengths that span the map (a river's band, the island's
// shape, the desert falloff, the windows of the location features) scale with the side, counts of scattered things (blob
// tiles, obstacle tiles, location objects, the difficulty objects, the patch grid) with the area, so a lake, a forest or a
// patch keeps its natural size and there are more of them. Both are the identity at 50, and every N = 50 path below is the
// original's own expression.
int32_t side_len(const Ctx &ctx, int32_t v) {
  if (SG_NEGCTL(g_negctl_generate) == 7) return v;   // negctl 7: lengths not scaled
  return (v * world_n(ctx) + kWorldOriginal / 2) / kWorldOriginal;
}
int32_t area_count(const Ctx &ctx, int32_t c) {
  if (world_n(ctx) == kWorldOriginal || SG_NEGCTL(g_negctl_generate) == 6) return c;   // negctl 6: counts not scaled
  const int32_t tiles50 = static_cast<int32_t>(kGridTiles);  // 2500
  return (c * world_n(ctx) * world_n(ctx) + tiles50 / 2) / tiles50;
}
}  // namespace

// Section M's `count` (the rows cleared on each side of the owned square): at 50 the original's loop (c from 0x19 down while
// c * c * 4 > acres * 10, count = 0x19 - c); at N the sweep radius is N / 2 and the owned square keeps the 50 proportion:
// round(count50 * N / 50) (P38B's flat course and P38C's generator; Mariano's pick, 2026-11-19).
int32_t owned_border(const Ctx &ctx, int32_t acres) {
  int32_t count50 = 0;
  {
    const int32_t e5 = acres * 10;
    int32_t c = 0x19;
    do {
      ++count50;
      --c;
    } while (c * c * 4 > e5);
  }
  return (count50 * world_n(ctx) + kWorldOriginal / 2) / kWorldOriginal;
}

int32_t generate_random_course(Ctx &ctx, int32_t n) {
  GameState &g = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_generate);
  const Ptr<int8_t> type = grid_arr(ctx, &GameState::g_tileType);
  const Ptr<uint16_t> word = grid_arr(ctx, &GameState::g_tileFlags);
  const Ptr<int8_t> cost = grid_arr(ctx, &GameState::g_tilePathCost);
  const Ptr<uint8_t> level = grid_arr(ctx, &GameState::g_cornerLevel);
  const Ptr<TileTypeAttr> attrs = state_arr(ctx, &GameState::g_tileTypeAttrs);
  const Ptr<int32_t> dir_dx = state_arr(ctx, &GameState::f_tab_dirDx_4c2878);
  const Ptr<int32_t> dir_dy = state_arr(ctx, &GameState::f_tab_dirDy_4c2898);
  const Ptr<PlacedObject> objects = state_arr(ctx, &GameState::g_placed_objects);
  const auto rnd = [&](int32_t range) {  // Random::range(range), ax kept
    uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
    return i32(random_range(seed, u32(range)) & 0xFFFFu);
  };
  const auto hi = [&](int32_t idx) { return (word + idx).as<uint8_t>().byte(1); };  // the flag word's high byte
  const auto path_cost = [&](int32_t t) { return (attrs + t).field(&TileTypeAttr::path_cost).get(); };
  const auto collar = [&](int32_t t) { return static_cast<int32_t>((attrs + t).field(&TileTypeAttr::collar).get()); };
  const auto set_type = [&](int32_t idx, int32_t t) { (type + idx).set(static_cast<int8_t>(t)); };
  const auto ty = [&](int32_t idx) { return (type + idx).get(); };
  const auto or_word = [&](int32_t idx, uint16_t m) { (word + idx).set(static_cast<uint16_t>((word + idx).get() | m)); };
  const auto or_hi = [&](int32_t idx, uint8_t m) { hi(idx).set(static_cast<uint8_t>(hi(idx).get() | m)); };
  const auto and_hi = [&](int32_t idx, uint8_t m) { hi(idx).set(static_cast<uint8_t>(hi(idx).get() & m)); };
  const auto lv = [&](int32_t off) { return level + off; };
  const auto add_lv = [&](int32_t off, int d) { lv(off).set(static_cast<uint8_t>(lv(off).get() + d)); };
  const int32_t N = world_n(ctx);  // P38A: the grid side (50)
  // P38C: N != 50 (the app's --world-size, play only): counts by the area, lengths by the side, the original's off-grid reads
  // and writes never taken (the side storage has no neighbour grid there). At 50 every expression below is the original's.
  const bool big = N != kWorldOriginal;
  const auto len = [&](int32_t v) { return side_len(ctx, v); };
  const auto many = [&](int32_t c) { return area_count(ctx, c); };
  // the island's (setting 2) cleared edge of row / column v: clamp(|v - 0x19|, 8, 0x32) + rest at 50 (rest = count - 0xc +
  // the jitter), at N clamp(|v - N / 2|, 8 N / 50, N) + rest with the 0xc also scaled
  const auto island_edge = [&](int32_t v, int32_t rest, int32_t jitter) {
    return clamp(abs32(v - N / 2), len(8), N) + rest + jitter;
  };
  const auto cell = [&](int32_t x, int32_t y) { return add32(mul32(x, N), y); };
  const auto corner = [&](int32_t x, int32_t y) { return add32(mul32(x, world_v(ctx)), y); };
  const auto dx = [&](int32_t k) { return dir_dx[k & 7]; };
  const auto dy = [&](int32_t k) { return dir_dy[k & 7]; };
  const auto obj_value = [&](int32_t oi) { return (objects + oi).field(&PlacedObject::value); };
  const auto obj_orient = [&](int32_t oi) { return (objects + oi).field(&PlacedObject::orientation); };

  // ---- the course record
  const uint8_t year = static_cast<uint8_t>(g.g_year);
  const Ptr<CourseRecord> rec = state_arr(ctx, &GameState::g_courseTable) + n;
  g.g_courseIndex = n;
  const int32_t c0 = rec.field(&CourseRecord::location_id).get();
  const int8_t ctype = rec.field(&CourseRecord::course_type).get();
  rec.field(&CourseRecord::year_bought).set(static_cast<int8_t>(year));
  const int32_t c2 = rec.field(&CourseRecord::relief).get();
  g.g_courseType = ctype;
  const auto setting = [&] { return rec.field(&CourseRecord::setting).get(); };
  const auto course_type = [&] { return static_cast<int32_t>(g.g_courseType); };

  // ---- A: the height relief
  int32_t e;
  if (c2 == 0) e = g.g_heightRelief = 0x30;
  else if (c2 == 1) e = g.g_heightRelief = 0x20;
  else if (c2 == 2) e = g.g_heightRelief = 0x10;
  else e = g.g_heightRelief;
  const int32_t quarter = n / 4;
  if (quarter == 0) e = g.g_heightRelief = add32(e, e / 2);
  else if (quarter == 1) e = g.g_heightRelief = add32(e, e / 4);
  else if (quarter == 3) e = g.g_heightRelief = add32(e, neg32(e / 4));
  const uint32_t flag = g.g_flags & 0x1000000u;
  if (flag != 0) g.g_heightRelief = add32(e, 0x10);

  // ---- B: the base type
  int32_t s10 = 0;
  bool s10_set = false;
  if (u32(ctype) <= 4) {
    if (ctype <= 3) {
      s10 = 4;
      s10_set = true;
    } else {
      for (int32_t k = 0; k < world_tiles(ctx); ++k) set_type(k, 0x0E);
    }
  }
  if (flag == 0) {
    if (n >= 8) {
      s10 = ctype != 1 ? 5 : 0xC;
      s10_set = true;
    }
    if (n >= 0xC) {
      s10 = ctype != 1 ? 0xB : 0xC;
      s10_set = true;
    }
  }
  // the original reads its stack slot here whatever it holds (U-filed by the integrator; see the module report)
  if (!s10_set) throw MissingResource{"generateRandomCourse: base type read from an uninitialised stack slot"};
  for (int32_t k = 0; k < world_tiles(ctx); ++k) set_type(k, s10);
  {
    const int8_t c = path_cost(ty(0));
    for (int32_t k = 0; k < world_tiles(ctx); ++k) (cost + k).set(c);
  }
  for (int32_t k = 0; k < world_tiles(ctx); ++k) {
    (grid_arr(ctx, &GameState::g_ballStopCount) + k).set(0);
    (grid_arr(ctx, &GameState::g_tileMoodUp) + k).set(0);
    (grid_arr(ctx, &GameState::g_tileMoodDown) + k).set(0);
    (grid_arr(ctx, &GameState::g_tileVariant) + k).set(0);
  }

  // ---- C: blobs
  int32_t p = 0;
  {
    int32_t total = 0, limit = 5000, t = 0;
    for (;;) {
      int32_t cnt = 0;
      int32_t r = rnd(N);  // a random tile (0x32 = N)
      int32_t c = rnd(N);
      const int32_t ct = course_type();
      if (u32(ct) <= 4) {
        switch (ct) {
          case 0: t = (c >= N / 2 ? 1 : 0) + 0xD; if (c0 == 9) t = 0xE; break;   // 0x19 = N / 2: the east half
          case 1: t = ((~p & 1) | 6) << 1; break;
          case 2: t = (p & 1) ? (((p & 2) | 0x18) >> 1) : 0xE; break;
          case 3: t = ((p & 1) ? 6 : 0) + 5; break;
          default: t = ((p & 1) << 3) | 4; break;
        }
      }
      while (r >= 0 && c >= 0 && r < N && c < N && cnt < limit / 128) {
        const int32_t idx = cell(r, c);
        ++cnt;
        set_type(idx, t);
        if (cnt == 0x30) or_hi(idx, 0x20);
        (cost + idx).set(path_cost(static_cast<int8_t>(t)));
        if (rnd(0x40) == 0) or_hi(idx, 1);
        else and_hi(idx, 0xFE);
        r = add32(r, rnd(3) - 1);
        c = add32(c, rnd(3) - 1);
      }
      total = add32(total, cnt);
      if (total > many(0x4E2)) break;   // P38C: half the area (0x4e2 of 2500 tiles)
      ++p;
      limit += 0x9C4;
      if (!(limit < 0xAFC8)) {
        if (!big) break;
        limit = 5000;   // P38C: the sizes of 50's 16 blobs again (natural size, more blobs) until half the area is walked
      }
    }
  }

  // ---- D: the river (setting != 2)
  {
    const int32_t rivers = setting() == 2 ? 0 : 1;
    int32_t bestk = 0;  // the original's slot is always written before it is read (sampleHeight < 999)
    for (p = 0; p < rivers; ++p) {
      const int32_t r0 = rnd(len(0x10));  // P38C: the start band scales with the side (rows N / 3 .. N / 3 + 16 N / 50)
      int32_t col = world_last(ctx);  // the east edge (0x31)
      const int32_t base = big ? mul32(N, p + 1) : (5 * p + 5) * 10;   // (5 p + 5) 10 = 50 (p + 1)
      int32_t row = add32(base / 3, r0);  // imul 0x55555556: the quotient toward zero
      int32_t prevk = 0;
      while (row >= 0 && row <= world_last(ctx)) {
        const int32_t tile = static_cast<uint8_t>(g.g_courseType) != 1 ? 0x11 : 0xB;
        const int32_t idx = cell(row, col);
        set_type(idx, tile);
        (word + idx).set(static_cast<uint16_t>(((word + idx).get() & ~0x100) | 0x1000));
        (cost + idx).set(path_cost(static_cast<int8_t>(tile)));
        if (rnd(0x40) == 0) or_hi(idx, 1);
        const int32_t c1 = add32(col, 1);
        int32_t best = 999, bestc = c1;
        int32_t nrow = add32(rnd(2) != 0 ? 1 : -1, row);
        const int32_t idx2 = cell(nrow, c1);
        const bool idx2_off = (big || !SG_DEFECT(g_defect_river_outside)) && !(nrow >= 0 && nrow < N && c1 >= 0 && c1 < N);  // P28A
        if (!idx2_off && ty(idx2) != 0x11 && height_blend(ctx, nrow, c1) == 3) {
          set_type(idx2, p == 0 ? ((~shl32(add32(nrow, c1), 1) & 4) | 8) : 0xC);
          if (g.g_courseType == 1) set_type(idx2, 0xA);
        }
        for (int32_t k = -2; k <= 2; k += 2) {
          const int32_t er = add32(dx(k), row), ec = add32(dy(k), col);
          if (p != 0 && er >= N - 2) continue;   // 0x30
          const int32_t h = sample_height(ctx, shl32(er, 7), shl32(ec, 7));
          if (h >= best) continue;
          best = h;
          nrow = er;
          bestc = ec;
          bestk = k;
        }
        if (bestk == neg32(prevk)) {
          col = sub32(col, 1);
        } else {
          col = bestc;
          row = nrow;
        }
        prevk = bestk;
        // row may be -1 or N (50) and col -1 here: read outside the grid (model_defects#6); P28A: at 0 leave without the read
        if ((big || !SG_DEFECT(g_defect_river_outside)) && !(row >= 0 && row < N && col >= 0 && col < N)) break;
        if (ty(cell(row, col)) == 0x11) break;
        if (col < 0) break;
      }
    }
  }

  // ---- E: the west shore (setting 1)
  if (setting() == 1) {
    int32_t width = len(5);   // P38C: the sea's band (2..12 columns at 50) scales with the side; the row-to-row step does not
    for (p = 0; p < world_tiles(ctx); p += N) {  // every row's first `width` columns (0x9c4, 0x32 = N * N, N)
      for (int32_t s = 0; s < width; ++s) {
        const int32_t ix = p + s;
        if (ty(ix) != 0x11) {
          const int32_t tile = g.g_courseType != 4 ? 0x11 : 0xC;
          set_type(ix, tile);
          (cost + ix).set(path_cost(static_cast<int8_t>(tile)));
          and_hi(ix, 0xFE);
          if (s == width - 1 && rnd(0x10) == 0) or_hi(ix, 1);
        }
        if (s == 0) set_type(p, 0x14);
      }
      width = width + rnd(5) - 2;
      if (width > len(2) - 1) {   // 1
        if (width > len(0xC)) --width;
      } else {
        width = len(2);
      }
    }
  }

  // ---- F: flag bit 0x1000 cleared, the corner levels
  for (int32_t k = 0; k < world_tiles(ctx); ++k) and_hi(k, 0xEF);
  {
    const int32_t last = mode == 3 ? world_last(ctx) : N;  // 0x31 / 0x32
    for (int32_t x = 0; x <= last; ++x)
      for (int32_t y = 0; y <= last; ++y) lv(corner(x, y)).set(static_cast<uint8_t>(height_blend(ctx, x, y)));
  }

  // ---- G: 24 patch walks
  {
    int32_t t = 0;
    // P38C: at 50 a 4 x 6 grid of spots 12.5 tiles apart (the last 8 lie past column 49 and land one row down, a single
    // tile each); at N a kx x kx grid at the same spacing, every spot on the map (kx = 4 N / 50 rounded: 8 at 101, 12 at 152)
    const int32_t kx = big ? (4 * N + kWorldOriginal / 2) / kWorldOriginal : 4;
    const int32_t patches = big && SG_NEGCTL(g_negctl_generate) != 6 ? kx * kx : 0x18;
    for (p = 0; p < patches; ++p) {
      int32_t x = rnd(4) + ((p % kx) * kWorldOriginal >> 2) + 4;   // p & 3
      int32_t y = rnd(4) + ((p / kx) * kWorldOriginal >> 2) + 4;   // p >> 2
      const int32_t ct = course_type();
      if (u32(ct) <= 4) {
        const uint8_t l = lv(corner(x, y)).get();
        switch (ct) {
          case 0: t = l > 3 ? 0xC : 0x11; break;
          case 1: t = l > 3 ? 0xC : 0xF; break;
          case 2: t = l > 3 ? 0xF : 0x11; break;
          case 3: t = l > 3 ? 0xF : 0xC; break;
          default: t = l > 4 ? 0xC : 0x11; break;
        }
      }
      bool water = false;
      const int32_t d = g.g_difficulty;
      if (d >= 2 && rnd(8 / (d - 1)) == 0) water = true;
      if (water || p == 0 || (big && p % 16 == 0)) t = 0x12;   // P38C: one sure pond per 16 patches (50 has 16 on the map)
      if (c0 == 2 && (t == 8 || t == 0x12)) t = 0xC;
      int32_t steps = 0;
      for (;;) {
        const int32_t idx = cell(x, y);
        if (((word + idx).get() & 0x1000) == 0) {
          set_type(idx, t);
          if (rnd(8) == 0 && t == 0x12) or_hi(idx, 1);
          (cost + idx).set(path_cost(ty(idx)));
          if (t == 0x11) lv(corner(x, y)).set(3);
        }
        if (rnd(2) != 0) x += rnd(2) != 0 ? 1 : -1;
        else y += rnd(2) != 0 ? 1 : -1;
        if (x < 0 || x >= N || y < 0 || y >= N) break;
        ++steps;
        if (steps <= 4) continue;
        if (rnd(0x10) != 0) continue;
        break;
      }
    }
  }

  // ---- H: 16 pairs of Random::range(0x2a) whose 0x100 marks are cleared at once, type 0x10 -> 0xd
  for (int32_t k = 0; k < 16; ++k) {
    const int32_t s = rnd(N - 8);   // 0x2a
    const int32_t r = rnd(N - 8);
    or_word(s + N * r + (4 * N + 4), 0x100);  // tile (r + 4, s + 4): 204 = 4 * 50 + 4
  }
  for (int32_t k = 0; k < world_tiles(ctx); ++k) {
    and_hi(k, 0xFE);
    if (ty(k) == 0x10) set_type(k, 0xD);
  }

  // ---- I: 9 * (4 - difficulty) unblocked tiles get flag 0x100
  {
    const auto qualifies = [&](int32_t x, int32_t y) {
      if (tile_blocked(ctx, x, y) != 0) return false;
      const int8_t t = ty(cell(x, y));
      if (t == 0x15 || t == 0x16 || t == 4 || t == 0x12) return false;
      if (t == (g.g_courseType != 1 ? 0xA : 0xB)) return false;
      return !(t == 0x11 && y <= len(6));   // P38C: the sea's columns scale (section E)
    };
    int32_t m = 0, misses = 0;
    const int32_t target = many(mul32(sub32(4, g.g_difficulty), 9));   // P38C: by the area
    do {
      const int32_t x = rnd(N - 4) + 2;   // 0x2e
      const int32_t y = rnd(N - 4) + 2;   // 0x2e
      if (qualifies(x, y)) {
        or_word(cell(x, y), 0x100);
        ++m;
        misses = 0;
      } else {
        miss(misses, [&] {
          if (!(m < target)) return true;  // the loop ends anyway
          for (int32_t a = 2; a < N - 2; ++a)
            for (int32_t b = 2; b < N - 2; ++b)
              if (qualifies(a, b)) return true;
          return false;
        });
      }
    } while (m < target);
  }

  // ---- J: level smoothing next to water (rows and columns 0..N: one past the grid, model_defects#6)
  {
    static const int8_t kRow[5] = {0, 1, 1, 0, 0};
    static const int8_t kCol[5] = {-1, -1, 0, 0, -1};
    for (int32_t x = 0; x <= N; ++x) {
      for (int32_t y = 0; y <= N; ++y) {
        const bool sm_fix = big || !SG_DEFECT(g_defect_smoothing_outside);  // P28A: type 0x14 outside the grid
        const int8_t a0 = sm_fix && (x >= N || y >= N) ? 0x14 : ty(cell(x, y));
        if (a0 != 0x11 && a0 != 0x12) continue;
        for (int32_t k = 0; k < 4; ++k) {
          const int32_t x1 = kRow[k] + x, y1 = kCol[k] + y;
          if (x1 < 0 || x1 >= N || y1 < 0 || y1 >= N) continue;
          const int32_t nx = add32(dx(2 * k), x), ny = add32(dy(2 * k), y);
          const bool n_off = sm_fix && !(nx >= 0 && nx < N && ny >= 0 && ny < N);
          if (!n_off && ty(add32(add32(mul32(add32(dx(2 * k), x), N), dy(2 * k)), y)) == 0x11) continue;
          const Ptr<uint8_t> c = lv(corner(x1, y1));
          const uint8_t v = c.get();
          if (v <= 3) continue;
          if (v != lv(corner(kRow[k + 1] + x, y) + kCol[k + 1]).get()) continue;
          if (v == 4) c.set(3);
          else c.set(static_cast<uint8_t>(c.get() + (rnd(2) != 0 ? 1 : -1)));
        }
      }
    }
  }

  // ---- K: the clubhouse, then the features of c0
  int32_t cx = 0, cy = 0;
  {
    const int32_t side = (state_arr(ctx, &GameState::f_tab_buildItems_4c26b0) + 15).field(&F_BuildItem::b10).get();
    const auto ok = [&](int32_t x, int32_t y) { return ty(cell(x, y)) == s10 && eval_placement_area(ctx, x, y, side, 0xF) != -1; };
    const int32_t club_lo = N / 2 - 10;
    int32_t misses = 0;
    for (;;) {
      cx = rnd(0x11) + club_lo;   // P38C: the 17 x 17 window centred at N / 2 (0xf = 25 - 10)
      cy = rnd(0x11) + club_lo;
      if (ok(cx, cy)) break;
      if (miss_bounded(misses, [&] {
            for (int32_t a = club_lo; a < club_lo + 0x11; ++a)
              for (int32_t b = club_lo; b < club_lo + 0x11; ++b)
                if (ok(a, b)) return true;
            return false;
          })) {
        cx = club_lo + 8;   // the window centre (0x17)
        cy = club_lo + 8;
        break;
      }
    }
    if (!SG_DEFECT(g_defect_feature_outside)) {  // P24E: keep the clubhouse and its free feature inside the border sweep
      // the sweep of section M: `count` from the acres; setting 2 clears an island edge of clamp(|x - 25|, 8, 50) + count
      // - 12 + Random::range(3) cells per row / column (the worst case, 2, is taken: the draw comes later)
      const int32_t count = owned_border(ctx, rec.field(&CourseRecord::acres).get());   // P38C: at N (count50 at 50)
      const int8_t set = setting();
      const auto island_lim = [&](int32_t v) { return island_edge(v, count - len(0xC), big ? len(3) - 1 : 2); };
      const auto cleared = [&](int32_t r, int32_t c) {
        if (r < 0 || r >= N || c < 0 || c >= N) return true;
        if (set == 2) return r < island_lim(c) || r > N - 1 - island_lim(c) || c < island_lim(r) || c > N - 1 - island_lim(r);
        return r < count || r > N - 1 - count || c > N - 1 - count || (set == 0 && c < count);
      };
      const Ptr<int32_t> type_level = state_arr(ctx, &GameState::g_objectTypeLevel);
      const auto footprint = [&](int32_t kind) {  // stampCourseTiles' side
        const int32_t s = (state_arr(ctx, &GameState::f_tab_buildItems_4c26b0) + kind).field(&F_BuildItem::b10).get();
        return kind >= 6 ? add32(s, type_level[kind]) : s;
      };
      const auto inside = [&](int32_t x, int32_t y, int32_t n) {
        for (int32_t r = 0; r < n; ++r)
          for (int32_t c = 0; c < n; ++c)
            if (cleared(x + r, y + c)) return false;
        return true;
      };
      const int32_t fkind = c0 == 2 ? 0xD : c0 == 4 ? 9 : c0 == 9 ? 6 : c0 == 11 ? 0xE : c0 == 14 ? 8 : -1;
      const auto fits = [&](int32_t x, int32_t y) {
        if (!inside(x, y, footprint(0xF))) return false;
        if (fkind < 0) return true;
        // the side section K's switch picks: tileBlocked(row, column + 5) reads a tile the clubhouse does not cover
        const int32_t fy = tile_blocked(ctx, x, add32(y, 5)) == 0 ? add32(y, 4) : sub32(y, 5);
        return inside(sub32(x, 1), fy, footprint(fkind));
      };
      if (!fits(cx, cy)) {
        int32_t best = -1, bx = cx, by = cy;
        for (int32_t a = club_lo; a < club_lo + 0x11; ++a)
          for (int32_t b = club_lo; b < club_lo + 0x11; ++b) {
            const int32_t d = abs32(a - cx) + abs32(b - cy);
            if ((best < 0 || d < best) && ok(a, b) && fits(a, b)) {
              best = d;
              bx = a;
              by = b;
            }
          }
        cx = bx;  // no spot fits: the drawn one stays (the original's)
        cy = by;
      }
    }
  }
  stamp_course_tiles(ctx, cx, cy, 0xF, 0x60);
  objects.field(&PlacedObject::flags).set(static_cast<uint8_t>(objects.field(&PlacedObject::flags).get() | 0x40));
  const int32_t cell0 = cell(cx, cy);
  const Ptr<HoleRecord> leaving = state_arr(ctx, &GameState::g_holes) + 19;  // the clubhouse's tee slots
  leaving.field(&HoleRecord::tee2_x).set(cx);
  leaving.field(&HoleRecord::tee_x).set(cx);
  (word + cell0).as<uint8_t>().set(static_cast<uint8_t>((word + cell0).as<uint8_t>().get() & 0xDF));
  leaving.field(&HoleRecord::tee2_y).set(cy);
  leaving.field(&HoleRecord::tee_y).set(cy);
  g.gCameraTileX = add32(cx, 6);
  g.gCameraTileY = sub32(cy, mode == 1 ? 3 : 4);
  camera_on_map(ctx);  // P6A mainLoop.cameraFollowOffGrid

  if (u32(c0) <= 0xF) {
    switch (c0) {
      case 2: case 4: case 9: case 11: case 14: {
        const int32_t kind = c0 == 2 ? 0xD : c0 == 4 ? 9 : c0 == 9 ? 6 : c0 == 11 ? 0xE : 8;
        if (tile_blocked(ctx, cx, add32(cy, 5)) == 0) {
          stamp_course_tiles(ctx, sub32(cx, 1), add32(cy, 4), kind, 0);
          for (int32_t k = 2; k <= 4; ++k) or_word(cell0 + k, 0x20);
        } else {
          stamp_course_tiles(ctx, sub32(cx, 1), sub32(cy, 5), kind, 0);
          for (int32_t k = 1; k <= 3; ++k) or_word(cell0 - k, 0x20);
        }
        rebuild_tile_coverage(ctx);
        if (c0 == 4 || c0 == 3) {
          int32_t s2, d2, misses = 0;
          bool gave_up = false;
          for (;;) {
            s2 = rnd(N - 10) + 5;   // 0x28
            d2 = rnd(N - 10) + 5;   // 0x28
            if (eval_placement_area(ctx, s2, d2, 2, 4) != -1) break;
            if (miss_placement(misses, [&] {
                  for (int32_t a = 5; a < N - 5; ++a)
                    for (int32_t b = 5; b < N - 5; ++b)
                      if (eval_placement_area(ctx, a, b, 2, 4) != -1) return true;
                  return false;
                })) {
              gave_up = mode != 4;  // no site for the object: it is not stamped
              break;
            }
          }
          if (!gave_up) obj_value(stamp_course_tiles(ctx, s2, d2, 4, 0)).set(0xE);
        }
        break;
      }
      case 7:
        for (int32_t k = 0; k < 2; ++k) {
          const int32_t rr = rnd(len(0xA));   // P38C: the band around the middle row scales with the side
          const int32_t x = (k != 0 ? rr : -rr) + N / 2;   // 0x19
          int32_t y = 0;
          for (;;) {
            const int8_t t = ty(cell(x, y));
            if ((t == 0x11 || t == 0x14) && y < N - 2) {   // 0x30
              ++y;
              continue;
            }
            break;
          }
          const int32_t oi = stamp_course_tiles(ctx, x, y, 4, 0);
          obj_value(oi).set(rnd(2) != 0 ? 7 : 0xD);
          add_lv(corner(x, y), 2);
        }
        break;
      case 1:
        if (many(sub32(8, g.g_difficulty)) > 0) {   // P38C: the object counts by the area
          int32_t done = 0;
          do {
            int32_t ix, misses = 0;
            bool gave_up = false;
            for (;;) {
              const int32_t s = rnd(len(0x1E)) + len(0xA);   // P38C: the window (rows 10..39, columns 0..29) by the side
              const int32_t c = rnd(len(0x1E));
              ix = cell(s, c);
              if (ty(ix) == 0x11) break;
              if (miss_bounded(misses, [&] {
                    for (int32_t a = len(0xA); a < len(0xA) + len(0x1E); ++a)
                      for (int32_t b = 0; b < len(0x1E); ++b)
                        if (ty(cell(a, b)) == 0x11) return true;
                    return false;
                  })) {
                gave_up = true;
                break;
              }
            }
            if (gave_up) break;  // no type-0x11 cell in the window: the remaining features of this kind are skipped
            or_word(ix, 0x100);
            ++done;
          } while (done < many(sub32(8, g.g_difficulty)));
        }
        break;
      case 5: case 6: case 8: case 12: case 13: case 15: {
        {
          int32_t misses = 0;
          bool gave_up = false;
          for (;;) {
            const int32_t s3 = rnd(N - 10) + 3;   // 0x28
            const int32_t d3 = rnd(N - 10) + 3;   // 0x28
            if (eval_placement_area(ctx, s3, d3, 5, 0xF) != -1) break;
            if (miss_placement(misses, [&] {
                  for (int32_t a = 3; a < N - 7; ++a)
                    for (int32_t b = 3; b < N - 7; ++b)
                      if (eval_placement_area(ctx, a, b, 5, 0xF) != -1) return true;
                  return false;
                })) {
              gave_up = true;
              break;
            }
          }
          if (gave_up && mode != 5) break;  // no site accepted: the feature (its objects) is skipped
        }
        int32_t p3 = 0;
        if (many(sub32(8, g.g_difficulty)) > 0) {   // P38C: the object counts by the area
          do {
            rnd(7);
            rnd(7);
            const int32_t x = rnd(N - 10) + 5;   // 0x28
            const int32_t y = rnd(N - 10) + 5;   // 0x28
            do {  // one pass; `break` = the next object
              if (eval_placement_area(ctx, x - 1, y - 1, 3, 7) == -1) break;
              const int32_t ix = cell(x, y);
              if (ty(ix) == 0x11) break;
              if (c0 == 5) {  // a pond with a shore of type 5
                set_type(ix, 0x11);
                lv(corner(x, y) + N).set(6);  // vertex (x + 1, y - 1): + 50 = + N
                rnd(4);
                for (int32_t k = 0; k < 8; ++k) {
                  const int32_t nr = add32(dx(k), x), nc = add32(dy(k), y);
                  const int32_t nx = cell(nr, nc);
                  switch (k) {
                    case 0: case 6: set_type(nx, 0x11); lv(corner(nr, nc) + N).set(6); break;
                    case 1: case 7: set_type(nx, 5); lv(corner(nr, nc) + N).set(6); break;
                    case 2: set_type(nx, 0x11); lv(corner(nr, nc) + N).set(3); break;
                    default:
                      if (rnd(2) != 0) set_type(nx, 0x11);
                      lv(corner(nr, nc) - 1).set(3);
                      break;
                  }
                }
                break;
              }
              if (c0 == 0xF || c0 == 0xC || c0 == 8) {
                const int32_t oi = stamp_course_tiles(ctx, x, y, 4, 0);
                obj_value(oi).set(0xB);
                if (c0 == 8) obj_value(oi).set(rnd(2) != 0 ? 6 : 2);
                else if (c0 == 0xC) obj_value(oi).set(3);
                obj_orient(oi).set(static_cast<uint8_t>(rnd(4)));
                for (int32_t k = 0; k < 8; ++k) {
                  const int32_t ix2 = add32(add32(mul32(add32(dx(k), x), N), dy(k)), y);
                  const int8_t t = ty(ix2);
                  if (t == 0x16 || t == 0x11) continue;
                  if (p3 & 1) {
                    set_type(ix2, c0 == 0xC ? 0xC : 4);
                    if (c0 == 0xF) {
                      set_type(ix2, 0x11);
                      continue;
                    }
                    if (k != 0) continue;
                    const int32_t b = corner(x, y);
                    add_lv(b, 2);
                    add_lv(b + world_v(ctx), 2);
                    add_lv(b - 1, 2);
                    add_lv(b + N, 2);
                  } else {
                    set_type(ix2, 5);
                    or_word(ix2, static_cast<uint16_t>(((k & 1) << 9) | 0x20));
                  }
                }
                break;
              }
              // (c0 == 0xa has a body here in the original, unreachable: case 10 takes the c0 == 0 branch)
              if (c0 == 0xD) {
                const int32_t oi = stamp_course_tiles(ctx, x, y, 4, 0);
                obj_value(oi).set(1);
                obj_orient(oi).set(static_cast<uint8_t>(rnd(4)));
                for (int32_t k = 0; k < 8; ++k) {
                  const int8_t t = ty(add32(add32(mul32(add32(dx(k), x), N), dy(k)), y));
                  if (collar(t) == 0xD) obj_value(oi).set(0xA);
                  if (t == 0x11) {
                    obj_value(oi).set(9);
                    break;
                  }
                }
                for (int32_t k = 0; k < 8; ++k) {
                  const int32_t rv = obj_value(oi).get();
                  const int32_t ix2 = add32(add32(mul32(add32(dx(k), x), N), dy(k)), y);
                  if (rv == 1) {
                    or_hi(ix2, 1);
                    set_type(ix2, 0xB);
                  }
                  if (rv == 3 && ty(ix2) == 4) set_type(ix2, 0xA);
                  if (rv == 9 && ty(ix2) == 4 && rnd(3) == 0) or_hi(ix2, 1);
                }
                break;
              }
              if (c0 == 6) {
                const int32_t oi = stamp_course_tiles(ctx, x, y, 4, 0);
                const int32_t bb = p3 & 1;
                obj_value(oi).set(8 + 4 * bb);
                obj_orient(oi).set(static_cast<uint8_t>(rnd(4)));
                for (int32_t k = 0; k < 8; ++k) {
                  const int32_t ix2 = add32(add32(mul32(add32(dx(k), x), N), dy(k)), y);
                  if (bb != 0) or_hi(ix2, 0x10);
                  else if (ty(ix2) != 0x11) or_word(ix2, static_cast<uint16_t>(((k & 1) << 9) | 0x20));
                }
                break;
              }
            } while (false);
            ++p3;
          } while (p3 < many(sub32(8, g.g_difficulty)));
        }
        break;
      }
      case 0: case 10: {
        const auto qualifies = [&](int32_t s, int32_t d) {
          if (tile_blocked(ctx, s, d) != 0) return false;
          const int32_t ix = cell(s, d);
          const int8_t t = ty(ix);
          if (t == 0x15 || t == 0x16 || t == 0x11) return false;
          return ((word + ix).get() & 0x400) == 0;
        };
        int32_t done = 0, misses = 0;
        do {
          const int32_t s = rnd(N - 4) + 2;   // 0x2e
          const int32_t d = rnd(N - 4) + 2;   // 0x2e
          if (!qualifies(s, d)) {
            miss(misses, [&] {
              for (int32_t a = 2; a < N - 2; ++a)
                for (int32_t b = 2; b < N - 2; ++b)
                  if (qualifies(a, b)) return true;
              return false;
            });
            continue;
          }
          misses = 0;
          const int32_t ix = cell(s, d);
          or_word(ix, 0x100);
          if (c0 == 0) set_type(ix, 0xB);
          else set_type(ix, 4);  // c0 == 0xa (the 0xd / 0xf arms of this chain are unreachable here)
          ++done;
        } while (done < many(0x10));   // P38C: by the area
        break;
      }
      default:  // 3: no feature
        break;
    }
  }

  // ---- L: one object per difficulty level on a tile of the base type
  {
    int32_t p4 = 0;
    if (g.g_difficulty > 0) {
      do {
        int32_t s, d, ix, misses = 0;
        for (;;) {
          s = rnd(N - 11) + 5;   // 0x27
          d = rnd(N - 11) + 5;   // 0x27
          ix = cell(s, d);
          if (ty(ix) == s10) break;
          miss(misses, [&] {
            for (int32_t a = 5; a < N - 6; ++a)
              for (int32_t b = 5; b < N - 6; ++b)
                if (ty(cell(a, b)) == s10) return true;
            return false;
          });
        }
        const int32_t oi = stamp_course_tiles(ctx, s, d, 4, 0);
        const int32_t ct = course_type();
        if (u32(ct) <= 3) obj_value(oi).set(ct == 0 ? 0x10 : ct == 1 ? 0x12 : 0x11);
        const int32_t rv = rnd(4);
        or_word(ix, 0x8000);
        obj_orient(oi).set(static_cast<uint8_t>(rv));
        if (g.g_courseType == 1)
          for (int32_t k = 0; k < 8; ++k) set_type(add32(add32(mul32(add32(dx(k), s), N), dy(k)), d), 0xB);
        ++p4;
      } while (p4 < many(g.g_difficulty));   // P38C: by the area
    }
  }

  // ---- M: the walker at the clubhouse, the saved tile types, the border sweeps
  const int32_t w = i32(call_peer(ctx, "spawnWalker", {u32(-6)}).eax);
  if (w >= 0) {  // w is -1 only with spawnWalker.fullTable at 0
    // P38B: through world.h (stored as the original's int8 while N <= 127)
    set_walker_post(ctx, w, 0, leaving.field(&HoleRecord::tee_x).get());
    set_walker_post(ctx, w, 1, leaving.field(&HoleRecord::tee_y).get());
  }
  int32_t count = owned_border(ctx, rec.field(&CourseRecord::acres).get());   // P38C: the sweep radius N / 2, 50's proportion
  const int8_t kind = rec.field(&CourseRecord::setting).get();
  for (int32_t k = 0; k < world_tiles(ctx); ++k) (grid_arr(ctx, &GameState::g_tileTypeSaved) + k).set(ty(k));
  if (kind == 2) {
    count -= len(0xC);
    int32_t walk = len(3) / 2;
    for (int32_t x = 0; x < N; ++x) {
      int32_t jitter = rnd(3);
      if (big) {   // P38C: the jitter walks (one draw a row as at 50) within 0 .. 3 N / 50 - 1, so the coast's bumps scale too
        walk = clamp(walk + jitter - 1, 0, len(3) - 1);
        jitter = walk;
      }
      const int32_t lim = island_edge(x, count, jitter);  // P38C: the island's shape scaled with the side
      for (int32_t a = 0, b = world_last(ctx); a < lim; ++a, --b) {
        clear_tile(ctx, a, x);
        clear_tile(ctx, x, a);
        clear_tile(ctx, b, x);
        clear_tile(ctx, x, b);
      }
    }
    mark_tile_kind11(ctx);
    return i32(rebuild_heightfield(ctx));
  }
  for (int32_t a = 0, b = world_last(ctx); a < count; ++a, --b) {
    for (int32_t s = 0; s < N; ++s) {
      clear_tile(ctx, a, s);
      if (rec.field(&CourseRecord::setting).get() == 0) clear_tile(ctx, s, a);
      if (mode != 2) clear_tile(ctx, s, b);
      clear_tile(ctx, b, s);
    }
  }
  mark_tile_kind11(ctx);
  return i32(rebuild_heightfield(ctx));
}

// ------------------------------------------------------------------------------------------------ P38B: the flat course
// generateFlatCourse (port-only, P38B; the app's --world-size N > 50 starts a new sandbox course with it in place of
// generateRandomCourse, whose walks are sized for 50 until phase C): the course record as generateRandomCourse writes it
// (index, type, year bought, height relief), every tile the base type (4 for course types 0..3, 0xe for 4, 5 / 0xc from
// course 8 and 0xb / 0xc from 12, as section B), its path cost, every corner level 4 (key 0x58's flat ground), the
// clubhouse (stampCourseTiles kind 0xf) at the centre with the leaving tee slots and the camera beside it, the walker
// posted at the clubhouse, g_tileTypeSaved = g_tileType, and the tiles outside the owned square for sale (0x14): the square
// keeps the original's proportion (section M's `count` from the acres, at 50 the rows cleared on each side, scaled by
// N / 50). markTileKind11 and rebuildHeightfield end it; returns rebuildHeightfield's eax.
int32_t generate_flat_course(Ctx &ctx, int32_t n) {
  GameState &g = ctx.gs();
  const int32_t N = world_n(ctx);
  const Ptr<CourseRecord> rec = state_arr(ctx, &GameState::g_courseTable) + n;
  g.g_courseIndex = n;
  const int8_t ctype = rec.field(&CourseRecord::course_type).get();
  rec.field(&CourseRecord::year_bought).set(static_cast<int8_t>(static_cast<uint8_t>(g.g_year)));
  g.g_courseType = ctype;
  const int32_t c2 = rec.field(&CourseRecord::relief).get();
  g.g_heightRelief = c2 == 0 ? 0x30 : c2 == 1 ? 0x20 : c2 == 2 ? 0x10 : g.g_heightRelief;
  int32_t base = u32(ctype) <= 3 ? 4 : 0xE;
  if ((g.g_flags & 0x1000000u) == 0) {
    if (n >= 8) base = ctype != 1 ? 5 : 0xC;
    if (n >= 0xC) base = ctype != 1 ? 0xB : 0xC;
  }
  const int8_t cost = (state_arr(ctx, &GameState::g_tileTypeAttrs) + base).field(&TileTypeAttr::path_cost).get();
  for (int32_t k = 0; k < world_tiles(ctx); ++k) {
    grid_ref(ctx, &GameState::g_tileType, k) = static_cast<int8_t>(base);
    grid_ref(ctx, &GameState::g_tilePathCost, k) = cost;
    grid_ref(ctx, &GameState::g_tileFlags, k) = 0;
    grid_ref(ctx, &GameState::g_ballStopCount, k) = 0;
    grid_ref(ctx, &GameState::g_tileMoodUp, k) = 0;
    grid_ref(ctx, &GameState::g_tileMoodDown, k) = 0;
    grid_ref(ctx, &GameState::g_tileVariant, k) = 0;
  }
  for (int32_t k = 0; k < world_verts(ctx); ++k) grid_ref(ctx, &GameState::g_cornerLevel, k) = 4;
  // the clubhouse near the centre
  const int32_t cx = N / 2 - 2, cy = N / 2 - 2;
  stamp_course_tiles(ctx, cx, cy, 0xF, 0x60);
  const Ptr<PlacedObject> objects = state_arr(ctx, &GameState::g_placed_objects);
  objects.field(&PlacedObject::flags).set(static_cast<uint8_t>(objects.field(&PlacedObject::flags).get() | 0x40));
  const Ptr<HoleRecord> leaving = state_arr(ctx, &GameState::g_holes) + 19;  // the clubhouse's tee slots
  leaving.field(&HoleRecord::tee2_x).set(cx);
  leaving.field(&HoleRecord::tee_x).set(cx);
  uint16_t &w0 = grid_ref(ctx, &GameState::g_tileFlags, tile_index(ctx, cx, cy));
  w0 = static_cast<uint16_t>(w0 & ~0x20u);
  leaving.field(&HoleRecord::tee2_y).set(cy);
  leaving.field(&HoleRecord::tee_y).set(cy);
  g.gCameraTileX = add32(cx, 6);
  g.gCameraTileY = sub32(cy, 4);
  const int32_t w = i32(call_peer(ctx, "spawnWalker", {u32(-6)}).eax);
  if (w >= 0) {
    set_walker_post(ctx, w, 0, cx);
    set_walker_post(ctx, w, 1, cy);
  }
  for (int32_t k = 0; k < world_tiles(ctx); ++k)
    grid_ref(ctx, &GameState::g_tileTypeSaved, k) = grid_ref(ctx, &GameState::g_tileType, k);
  // the owned square: section M's count at 50 (c from 0x19 down while c * c * 4 > acres * 10), scaled by N / 50
  int32_t count50 = 0;
  {
    const int32_t e5 = rec.field(&CourseRecord::acres).get() * 10;
    int32_t c = 0x19;
    do {
      ++count50;
      --c;
    } while (c * c * 4 > e5);
  }
  const int32_t count = SG_NEGCTL(g_negctl_flat_course) == 1 ? count50 : (count50 * N + kWorldOriginal / 2) / kWorldOriginal;
  for (int32_t x = 0; x < N; ++x)
    for (int32_t y = 0; y < N; ++y)
      if (x < count || y < count || x >= N - count || y >= N - count) grid_ref(ctx, &GameState::g_tileType, tile_index(ctx, x, y)) = 0x14;
  mark_tile_kind11(ctx);
  return i32(rebuild_heightfield(ctx));
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_generate(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(generate_random_course(c, static_cast<int32_t>(a[0]))); }

const FuncDef kFuncs[] = {
    {"generateRandomCourse", "default", "int", 1, 0x027F, a_generate, &g_negctl_generate},
    {"generateFlatCourse", "default", "int", 1, 0, [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(generate_flat_course(c, static_cast<int32_t>(a[0]))); },
     &g_negctl_flat_course},  // P38B: port-only
};

const DefectDef kDefects[] = {
    {"generateRandomCourse.searchBound", "model_defects#52",
     "the type-0x11 search (layout kind 1) and the clubhouse search never end when their window holds no qualifying cell",
     &g_defect_search_bound},
    {"generateRandomCourse.placementBound", "model_defects#52",
     "the feature-site searches gated by eval_placement_area (locations 4, 5, 6, 8, 12, 13, 15) never end when no cell is accepted",
     &g_defect_placement_bound},
    {"generateRandomCourse.startingFeatureOutside", "P24E",
     "the clubhouse spot and the free feature beside it (the practice green of location 9) are placed before the border sweep "
     "and never tested against it, so they can lie partly past the course edge",
     &g_defect_feature_outside},
    {"generateRandomCourse.riverOutside", "model_defects#6",
     "the river's shore tile at column + 1 (column 50 = the west edge of the next row; rows -1 / 50 = outside the grid) is read "
     "and written with no grid test; fixed: a tile outside the grid is neither read nor written", &g_defect_river_outside},
    {"generateRandomCourse.smoothingOutside", "model_defects#6",
     "the water level smoothing runs rows and columns 0..50 and reads tile_type one past the grid; fixed: a cell outside the grid "
     "has type 0x14 (not water)", &g_defect_smoothing_outside},
};
}  // namespace

extern const ModuleDef kModule_terrain_gen = {"terrain_gen", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                              sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
