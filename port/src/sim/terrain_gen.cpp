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
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   generateRandomCourse  1 = camera y = clubhouse column - 3, 2 = the border sweep (setting != 2) skips its third
//                         clearTile (the far column), 3 = the corner-level fill stops at 50 x 50 (row / column 50 kept)
int g_negctl_generate = 0;

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
}  // namespace

int32_t generate_random_course(Ctx &ctx, int32_t n) {
  GameState &g = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_generate);
  const Ptr<int8_t> type = state_arr(ctx, &GameState::g_tileType);
  const Ptr<uint16_t> word = state_arr(ctx, &GameState::g_tileFlags);
  const Ptr<int8_t> cost = state_arr(ctx, &GameState::g_tilePathCost);
  const Ptr<uint8_t> level = state_arr(ctx, &GameState::g_cornerLevel);
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
  const auto cell = [](int32_t x, int32_t y) { return add32(mul32(x, 50), y); };
  const auto corner = [](int32_t x, int32_t y) { return add32(mul32(x, kCornerW), y); };
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
      for (int32_t k = 0; k < 2500; ++k) set_type(k, 0x0E);
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
  for (int32_t k = 0; k < 2500; ++k) set_type(k, s10);
  {
    const int8_t c = path_cost(ty(0));
    for (int32_t k = 0; k < 2500; ++k) (cost + k).set(c);
  }
  for (int32_t k = 0; k < 2500; ++k) {
    (state_arr(ctx, &GameState::g_ballStopCount) + k).set(0);
    (state_arr(ctx, &GameState::g_tileMoodUp) + k).set(0);
    (state_arr(ctx, &GameState::g_tileMoodDown) + k).set(0);
    (state_arr(ctx, &GameState::g_tileVariant) + k).set(0);
  }

  // ---- C: blobs
  int32_t p = 0;
  {
    int32_t total = 0, limit = 5000, t = 0;
    for (;;) {
      int32_t cnt = 0;
      int32_t r = rnd(0x32);
      int32_t c = rnd(0x32);
      const int32_t ct = course_type();
      if (u32(ct) <= 4) {
        switch (ct) {
          case 0: t = (c >= 0x19 ? 1 : 0) + 0xD; if (c0 == 9) t = 0xE; break;
          case 1: t = ((~p & 1) | 6) << 1; break;
          case 2: t = (p & 1) ? (((p & 2) | 0x18) >> 1) : 0xE; break;
          case 3: t = ((p & 1) ? 6 : 0) + 5; break;
          default: t = ((p & 1) << 3) | 4; break;
        }
      }
      while (r >= 0 && c >= 0 && r < 0x32 && c < 0x32 && cnt < limit / 128) {
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
      if (total > 0x4E2) break;
      ++p;
      limit += 0x9C4;
      if (!(limit < 0xAFC8)) break;
    }
  }

  // ---- D: the river (setting != 2)
  {
    const int32_t rivers = setting() == 2 ? 0 : 1;
    int32_t bestk = 0;  // the original's slot is always written before it is read (sampleHeight < 999)
    for (p = 0; p < rivers; ++p) {
      const int32_t r0 = rnd(0x10);
      int32_t col = 0x31;
      const int32_t base = (5 * p + 5) * 10;
      int32_t row = add32(base / 3, r0);  // imul 0x55555556: the quotient toward zero
      int32_t prevk = 0;
      while (row >= 0 && row <= 0x31) {
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
        if (ty(idx2) != 0x11 && height_blend(ctx, nrow, c1) == 3) {
          set_type(idx2, p == 0 ? ((~shl32(add32(nrow, c1), 1) & 4) | 8) : 0xC);
          if (g.g_courseType == 1) set_type(idx2, 0xA);
        }
        for (int32_t k = -2; k <= 2; k += 2) {
          const int32_t er = add32(dx(k), row), ec = add32(dy(k), col);
          if (p != 0 && er >= 0x30) continue;
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
        if (ty(cell(row, col)) == 0x11) break;  // row may be -1 or 50 here: read outside the grid (model_defects#6)
        if (col < 0) break;
      }
    }
  }

  // ---- E: the west shore (setting 1)
  if (setting() == 1) {
    int32_t width = 5;
    for (p = 0; p < 0x9C4; p += 0x32) {
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
      if (width > 1) {
        if (width > 0xC) --width;
      } else {
        width = 2;
      }
    }
  }

  // ---- F: flag bit 0x1000 cleared, the corner levels
  for (int32_t k = 0; k < 2500; ++k) and_hi(k, 0xEF);
  {
    const int32_t last = mode == 3 ? 0x31 : 0x32;
    for (int32_t x = 0; x <= last; ++x)
      for (int32_t y = 0; y <= last; ++y) lv(corner(x, y)).set(static_cast<uint8_t>(height_blend(ctx, x, y)));
  }

  // ---- G: 24 patch walks
  {
    int32_t t = 0;
    for (p = 0; p < 0x18; ++p) {
      int32_t x = rnd(4) + ((p & 3) * 50 >> 2) + 4;
      int32_t y = rnd(4) + ((p >> 2) * 50 >> 2) + 4;
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
      if (water || p == 0) t = 0x12;
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
        if (x < 0 || x >= 0x32 || y < 0 || y >= 0x32) break;
        ++steps;
        if (steps <= 4) continue;
        if (rnd(0x10) != 0) continue;
        break;
      }
    }
  }

  // ---- H: 16 pairs of Random::range(0x2a) whose 0x100 marks are cleared at once, type 0x10 -> 0xd
  for (int32_t k = 0; k < 16; ++k) {
    const int32_t s = rnd(0x2A);
    const int32_t r = rnd(0x2A);
    or_word(s + 50 * r + 204, 0x100);
  }
  for (int32_t k = 0; k < 2500; ++k) {
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
      return !(t == 0x11 && y <= 6);
    };
    int32_t m = 0, misses = 0;
    do {
      const int32_t x = rnd(0x2E) + 2;
      const int32_t y = rnd(0x2E) + 2;
      if (qualifies(x, y)) {
        or_word(cell(x, y), 0x100);
        ++m;
        misses = 0;
      } else {
        miss(misses, [&] {
          if (!(m < mul32(sub32(4, g.g_difficulty), 9))) return true;  // the loop ends anyway
          for (int32_t a = 2; a < 0x30; ++a)
            for (int32_t b = 2; b < 0x30; ++b)
              if (qualifies(a, b)) return true;
          return false;
        });
      }
    } while (m < mul32(sub32(4, g.g_difficulty), 9));
  }

  // ---- J: level smoothing next to water (rows and columns 0..50: one past the grid, model_defects#6)
  {
    static const int8_t kRow[5] = {0, 1, 1, 0, 0};
    static const int8_t kCol[5] = {-1, -1, 0, 0, -1};
    for (int32_t x = 0; x <= 50; ++x) {
      for (int32_t y = 0; y <= 0x32; ++y) {
        const int8_t a0 = ty(cell(x, y));
        if (a0 != 0x11 && a0 != 0x12) continue;
        for (int32_t k = 0; k < 4; ++k) {
          const int32_t x1 = kRow[k] + x, y1 = kCol[k] + y;
          if (x1 < 0 || x1 >= 0x32 || y1 < 0 || y1 >= 0x32) continue;
          if (ty(add32(add32(mul32(add32(dx(2 * k), x), 50), dy(2 * k)), y)) == 0x11) continue;
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
    int32_t misses = 0;
    for (;;) {
      cx = rnd(0x11) + 0xF;
      cy = rnd(0x11) + 0xF;
      if (ok(cx, cy)) break;
      miss(misses, [&] {
        for (int32_t a = 0xF; a < 0x20; ++a)
          for (int32_t b = 0xF; b < 0x20; ++b)
            if (ok(a, b)) return true;
        return false;
      });
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
          for (;;) {
            s2 = rnd(0x28) + 5;
            d2 = rnd(0x28) + 5;
            if (eval_placement_area(ctx, s2, d2, 2, 4) != -1) break;
            miss(misses, [&] {
              for (int32_t a = 5; a < 0x2D; ++a)
                for (int32_t b = 5; b < 0x2D; ++b)
                  if (eval_placement_area(ctx, a, b, 2, 4) != -1) return true;
              return false;
            });
          }
          obj_value(stamp_course_tiles(ctx, s2, d2, 4, 0)).set(0xE);
        }
        break;
      }
      case 7:
        for (int32_t k = 0; k < 2; ++k) {
          const int32_t rr = rnd(0xA);
          const int32_t x = (k != 0 ? rr : -rr) + 0x19;
          int32_t y = 0;
          for (;;) {
            const int8_t t = ty(cell(x, y));
            if ((t == 0x11 || t == 0x14) && y < 0x30) {
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
        if (sub32(8, g.g_difficulty) > 0) {
          int32_t done = 0;
          do {
            int32_t ix, misses = 0;
            for (;;) {
              const int32_t s = rnd(0x1E) + 0xA;
              const int32_t c = rnd(0x1E);
              ix = cell(s, c);
              if (ty(ix) == 0x11) break;
              miss(misses, [&] {
                for (int32_t a = 0xA; a < 0x28; ++a)
                  for (int32_t b = 0; b < 0x1E; ++b)
                    if (ty(cell(a, b)) == 0x11) return true;
                return false;
              });
            }
            or_word(ix, 0x100);
            ++done;
          } while (done < sub32(8, g.g_difficulty));
        }
        break;
      case 5: case 6: case 8: case 12: case 13: case 15: {
        {
          int32_t misses = 0;
          for (;;) {
            const int32_t s3 = rnd(0x28) + 3;
            const int32_t d3 = rnd(0x28) + 3;
            if (eval_placement_area(ctx, s3, d3, 5, 0xF) != -1) break;
            miss(misses, [&] {
              for (int32_t a = 3; a < 0x2B; ++a)
                for (int32_t b = 3; b < 0x2B; ++b)
                  if (eval_placement_area(ctx, a, b, 5, 0xF) != -1) return true;
              return false;
            });
          }
        }
        int32_t p3 = 0;
        if (sub32(8, g.g_difficulty) > 0) {
          do {
            rnd(7);
            rnd(7);
            const int32_t x = rnd(0x28) + 5;
            const int32_t y = rnd(0x28) + 5;
            do {  // one pass; `break` = the next object
              if (eval_placement_area(ctx, x - 1, y - 1, 3, 7) == -1) break;
              const int32_t ix = cell(x, y);
              if (ty(ix) == 0x11) break;
              if (c0 == 5) {  // a pond with a shore of type 5
                set_type(ix, 0x11);
                lv(corner(x, y) + 50).set(6);
                rnd(4);
                for (int32_t k = 0; k < 8; ++k) {
                  const int32_t nr = add32(dx(k), x), nc = add32(dy(k), y);
                  const int32_t nx = cell(nr, nc);
                  switch (k) {
                    case 0: case 6: set_type(nx, 0x11); lv(corner(nr, nc) + 50).set(6); break;
                    case 1: case 7: set_type(nx, 5); lv(corner(nr, nc) + 50).set(6); break;
                    case 2: set_type(nx, 0x11); lv(corner(nr, nc) + 50).set(3); break;
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
                  const int32_t ix2 = add32(add32(mul32(add32(dx(k), x), 50), dy(k)), y);
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
                    add_lv(b + 51, 2);
                    add_lv(b - 1, 2);
                    add_lv(b + 50, 2);
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
                  const int8_t t = ty(add32(add32(mul32(add32(dx(k), x), 50), dy(k)), y));
                  if (collar(t) == 0xD) obj_value(oi).set(0xA);
                  if (t == 0x11) {
                    obj_value(oi).set(9);
                    break;
                  }
                }
                for (int32_t k = 0; k < 8; ++k) {
                  const int32_t rv = obj_value(oi).get();
                  const int32_t ix2 = add32(add32(mul32(add32(dx(k), x), 50), dy(k)), y);
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
                  const int32_t ix2 = add32(add32(mul32(add32(dx(k), x), 50), dy(k)), y);
                  if (bb != 0) or_hi(ix2, 0x10);
                  else if (ty(ix2) != 0x11) or_word(ix2, static_cast<uint16_t>(((k & 1) << 9) | 0x20));
                }
                break;
              }
            } while (false);
            ++p3;
          } while (p3 < sub32(8, g.g_difficulty));
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
          const int32_t s = rnd(0x2E) + 2;
          const int32_t d = rnd(0x2E) + 2;
          if (!qualifies(s, d)) {
            miss(misses, [&] {
              for (int32_t a = 2; a < 0x30; ++a)
                for (int32_t b = 2; b < 0x30; ++b)
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
        } while (done < 0x10);
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
          s = rnd(0x27) + 5;
          d = rnd(0x27) + 5;
          ix = cell(s, d);
          if (ty(ix) == s10) break;
          miss(misses, [&] {
            for (int32_t a = 5; a < 0x2C; ++a)
              for (int32_t b = 5; b < 0x2C; ++b)
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
          for (int32_t k = 0; k < 8; ++k) set_type(add32(add32(mul32(add32(dx(k), s), 50), dy(k)), d), 0xB);
        ++p4;
      } while (p4 < g.g_difficulty);
    }
  }

  // ---- M: the walker at the clubhouse, the saved tile types, the border sweeps
  const int32_t w = i32(call_peer(ctx, "spawnWalker", {u32(-6)}).eax);
  {
    const int8_t px = static_cast<int8_t>(leaving.field(&HoleRecord::tee_x).get());
    const int8_t py = static_cast<int8_t>(leaving.field(&HoleRecord::tee_y).get());
    const Ptr<Walker> wk = state_arr(ctx, &GameState::g_walkers) + w;
    wk.field(&Walker::post_x).set(px);
    wk.field(&Walker::post_y).set(py);
  }
  int32_t count = 0;
  {
    const int32_t e5 = rec.field(&CourseRecord::acres).get() * 10;
    int32_t c = 0x19;
    do {
      ++count;
      --c;
    } while (c * c * 4 > e5);
  }
  const int8_t kind = rec.field(&CourseRecord::setting).get();
  for (int32_t k = 0; k < 2500; ++k) (state_arr(ctx, &GameState::g_tileTypeSaved) + k).set(ty(k));
  if (kind == 2) {
    count -= 0xC;
    for (int32_t x = 0; x < 0x32; ++x) {
      const int32_t jitter = rnd(3);
      const int32_t lim = clamp(abs32(x - 0x19), 8, 0x32) + count + jitter;
      for (int32_t a = 0, b = 0x31; a < lim; ++a, --b) {
        clear_tile(ctx, a, x);
        clear_tile(ctx, x, a);
        clear_tile(ctx, b, x);
        clear_tile(ctx, x, b);
      }
    }
    mark_tile_kind11(ctx);
    return i32(rebuild_heightfield(ctx));
  }
  for (int32_t a = 0, b = 0x31; a < count; ++a, --b) {
    for (int32_t s = 0; s < 0x32; ++s) {
      clear_tile(ctx, a, s);
      if (rec.field(&CourseRecord::setting).get() == 0) clear_tile(ctx, s, a);
      if (mode != 2) clear_tile(ctx, s, b);
      clear_tile(ctx, b, s);
    }
  }
  mark_tile_kind11(ctx);
  return i32(rebuild_heightfield(ctx));
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_generate(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(generate_random_course(c, static_cast<int32_t>(a[0]))); }

const FuncDef kFuncs[] = {
    {"generateRandomCourse", "default", "int", 1, 0x027F, a_generate, &g_negctl_generate},
};
}  // namespace

extern const ModuleDef kModule_terrain_gen = {"terrain_gen", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
