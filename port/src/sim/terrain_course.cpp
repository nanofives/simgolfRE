// Footprint stamping of the terrain module (see portsim/sim/terrain.h): stampCourseTiles and clearObjectFootprint.
// Phase 3.1a, written from the specification (re/analysis/terrain/0040e000_stampCourseTiles.md,
// re/analysis/course/0040e400_clearObjectFootprint.md, the C3 hook sources as reading reference, the vector corpus).
// rateLot (3.2) and freeRecordAtPos (3.1b) are called through the registry; pointsPopup (a ui straddler of phase 3.7
// that changes simulation state) is the registered function when there is one, else the local copy below (the pattern
// of golfers_glue.inc).
#include <initializer_list>

#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/terrain.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   stampCourseTiles      1 = the corner cell (r == side - 1, c == 0) also takes the level, 2 = kind 0xa marks all 1
//   clearObjectFootprint  1 = the refund leaves the finance year alone
int g_negctl_stamp = 0;
int g_negctl_clear_footprint = 0;
//   stepCourseEditQueue   1 = the camera glide steps x by two
int g_negctl_edit_queue = 0;
//   demolishObject        1 = the refund leaves the finance year alone
//   buildTournamentPrep   1 = a decoration below the cup faces 1 instead of 2
int g_negctl_demolish = 0;
int g_negctl_tournament = 0;

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

int8_t item_size(Ctx &c, int32_t type) {
  return (state_arr(c, &GameState::f_tab_buildItems_4c26b0) + type).field(&F_BuildItem::b10).get();
}

// pointsPopup (re/analysis/ui/0040c890): hole money and the floating money label ring
void points_popup(Ctx &ctx, int32_t pts, int32_t x, int32_t y, int32_t who) {
  if (registry_find("pointsPopup") >= 0) {
    call_peer(ctx, "pointsPopup", {u32(pts), u32(x), u32(y), u32(who)});
    return;
  }
  GameState &gs = ctx.gs();
  if (pts == 0 || (gs.g_flags & 0x1000000u)) return;
  if (who != -1) {
    const Ptr<int32_t> money = (state_arr(ctx, &GameState::g_holes) + who).field(&HoleRecord::money);
    money.set(sub32(money.get(), pts));
  }
  const int32_t slot = gs.g_points59abb0;
  (state_arr(ctx, &GameState::g_moneyLabelX) + slot).set(x);
  (state_arr(ctx, &GameState::g_moneyLabelY) + slot).set(y);
  (state_arr(ctx, &GameState::g_moneyLabelValue) + slot).set(pts);
  (state_arr(ctx, &GameState::g_moneyLabelTime) + slot).set(0x18);
  gs.g_points59abb0 = mod32(add32(slot, 1), 8);
}
}  // namespace

int32_t stamp_course_tiles(Ctx &ctx, int32_t x, int32_t y, int32_t kind, int32_t flags) {
  const Ptr<int32_t> extra = state_arr(ctx, &GameState::g_objectTypeLevel);
  const Ptr<uint8_t> level = state_arr(ctx, &GameState::g_cornerLevel);
  const Ptr<int8_t> type = state_arr(ctx, &GameState::g_tileType);
  const Ptr<int8_t> cost = state_arr(ctx, &GameState::g_tilePathCost);
  const Ptr<uint16_t> word = state_arr(ctx, &GameState::g_tileFlags);
  const Ptr<uint8_t> var = state_arr(ctx, &GameState::g_tileVariant);
  const int mode = SG_NEGCTL(g_negctl_stamp);
  int32_t side = item_size(ctx, kind);
  if (flags == -2) side = side + 1;
  if (kind >= 6) side = add32(side, extra[kind]);
  const uint8_t cost22 = static_cast<uint8_t>(
      (state_arr(ctx, &GameState::g_tileTypeAttrs) + 22).field(&TileTypeAttr::path_cost).get());
  for (int32_t c = 0; c < side; ++c) {
    for (int32_t r = 0; r < side; ++r) {
      if (r < side - 1 || c != 0 || mode == 1)
        (level + add32(mul32(add32(c, x), kCornerW), add32(y, r))).set((level + add32(mul32(x, kCornerW), y)).get());
      const int32_t idx = add32(add32(y, r), mul32(add32(c, x), 50));
      (type + idx).set(static_cast<int8_t>((kind != 5 ? 1 : 0) + 0x15));
      (cost + idx).set(static_cast<int8_t>(cost22));
      if (kind == 6) {
        if (c != 0 || r != 0) (type + idx).set(1);
      } else if (kind == 0xA) {
        if (c != 0 || r != 0) (type + idx).set(static_cast<int8_t>((c == side - 1 ? 1 : 0) + 4));
      } else if (kind == 0xC) {
        const uint8_t ct = static_cast<uint8_t>(ctx.gs().g_courseType);
        if ((ct == 0 || ct == 2) && (c != 0 || r != 0)) (type + idx).set(0x11);
      }
      const uint16_t w = (word + idx).get();
      (word + idx).set(static_cast<uint16_t>(w & 0xECFF));
      if (flags > 0) (word + idx).set(static_cast<uint16_t>((w & 0xECFF) | static_cast<uint16_t>(flags)));
      (var + idx).set(0xFF);
    }
  }
  if (kind == 0xA) {
    uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
    const int32_t n = side - 2;
    for (int k = 0; k < 4; ++k) {
      const uint32_t a = random_range(seed, u32(n)) & 0xFFFFu;
      const uint32_t b = random_range(seed, u32(n)) & 0xFFFFu;
      // the marks grid is g_tileType shifted by one row and one column
      const uint32_t at = a + 50u * (b + u32(x)) + u32(y) + 51u;
      (type + i32(at)).set(static_cast<int8_t>((k == 3 && mode != 2) ? 7 : 1));
    }
  } else if (kind == 0xF) {
    const Ptr<Walker> w0 = state_arr(ctx, &GameState::g_walkers);
    w0.field(&Walker::post_x).set(static_cast<int8_t>(x + 1));
    w0.field(&Walker::post_y).set(static_cast<int8_t>(y + 1));
  }
  if (flags < 0) return 0;
  const int32_t base = add32(y, mul32(50, x));
  (word + base).set(static_cast<uint16_t>((word + base).get() | static_cast<uint16_t>(kind + 1)));
  const Ptr<PlacedObject> recs = state_arr(ctx, &GameState::g_placed_objects);
  int32_t slot = 0;
  if (recs.field(&PlacedObject::type).get() != -1) {
    Ptr<PlacedObject> p = recs;
    do {
      p = p + 1;
      ++slot;
    } while (p.field(&PlacedObject::type).get() != -1);  // runs on past the table like the original
    if (slot >= 0x100) return 0;
  }
  const Ptr<PlacedObject> r = recs + slot;
  r.field(&PlacedObject::type).set(static_cast<int16_t>(kind));
  r.field(&PlacedObject::x).set(static_cast<int16_t>(x));
  r.field(&PlacedObject::y).set(static_cast<int16_t>(y));
  r.field(&PlacedObject::flags).set(0);
  r.field(&PlacedObject::value2).set(0);
  r.field(&PlacedObject::value).set(0);
  r.field(&PlacedObject::orientation).set(
      static_cast<uint8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_ui_5a34e4) + 0xC) & 3));
  const int32_t n = add32(extra[kind], item_size(ctx, kind));
  for (int32_t row = 0; row < n; ++row) {
    for (int32_t col = 0; col < n; ++col) (var + add32(add32(base, row * 50), col)).set(static_cast<uint8_t>(slot));
    for (int32_t col = 0; col < n; ++col) {
      const Ptr<uint16_t> p = word + add32(add32(base, row * 50), col);
      p.set(static_cast<uint16_t>((p.get() & 0xFEFF) | 0x0400));
    }
  }
  rebuild_heightfield(ctx);
  return slot;
}

uint32_t clear_object_footprint(Ctx &ctx, int32_t index) {
  GameState &g = ctx.gs();
  const Ptr<PlacedObject> rec = state_arr(ctx, &GameState::g_placed_objects) + index;
  const int32_t type = rec.field(&PlacedObject::type).get();
  const int32_t y = rec.field(&PlacedObject::y).get();
  const int32_t x = rec.field(&PlacedObject::x).get();
  if (type == 5) {  // a lot is refunded
    const int32_t rating = i32(call_peer(ctx, "rateLot", {u32(x), u32(y)}).eax);
    const int32_t value = rec.field(&PlacedObject::value).get();
    const int32_t refund = add32(value / 50, rating / 2);
    g.g_cash_div100 = sub32(g.g_cash_div100, refund);
    points_popup(ctx, neg32(refund), add32(mul32(x, 0x400), 0x200), add32(mul32(y, 0x400), 0x200), -1);
    if (SG_NEGCTL(g_negctl_clear_footprint) != 1) {
      const Ptr<int16_t> fy = (state_arr(ctx, &GameState::g_financeYears) + g.g_year % 100).field(&FinanceYear::home_sites);
      fy.set(static_cast<int16_t>(fy.get() - static_cast<int16_t>(refund)));
    }
    if (value != 0) call_peer(ctx, "freeRecordAtPos", {u32(x), u32(y)});
  }
  rec.field(&PlacedObject::type).set(-1);
  if (type == 0xF) g.g_flags |= 0x800u;
  int32_t side = item_size(ctx, type);
  const int32_t cell0 = add32(y, mul32(x, 50));
  const Ptr<uint16_t> word = state_arr(ctx, &GameState::g_tileFlags);
  (word + cell0).set(static_cast<uint16_t>((word + cell0).get() & 0xFDE0));
  if (type >= 6 && type != 7) side = add32(side, sub32((state_arr(ctx, &GameState::g_objectTypeLevel) + type).get(), 1));
  if (side > 0) {
    const uint8_t ct = static_cast<uint8_t>(g.g_courseType);
    const int8_t fill = (type == 0xC && (ct == 0 || ct == 2)) ? 0x11 : static_cast<int8_t>((g.g_courseIndex >= 0xC ? 1 : 0) + 4);
    int32_t row_base = mul32(x, 50);
    for (int32_t r = 0; r < side; ++r) {
      for (int32_t c = 0; c < side; ++c) {
        const int32_t cell = add32(add32(row_base, y), c);
        (word + cell).set(static_cast<uint16_t>((word + cell).get() & 0xFBDF));
        (state_arr(ctx, &GameState::g_tileType) + cell).set(fill);
        (state_arr(ctx, &GameState::g_tileVariant) + cell).set(0);
      }
      row_base = add32(row_base, 50);
    }
  }
  return rebuild_heightfield(ctx);
}

int32_t step_course_edit_queue(Ctx &ctx) {
  GameState &g = ctx.gs();
  const int mode_neg = SG_NEGCTL(g_negctl_edit_queue);
  const int32_t target = g.g_cameraGoalX;
  if (target != -1) {
    const int32_t cam0 = g.gCameraTileX;
    bool moving = cam0 != target;
    if (!moving) moving = g.gCameraTileY != g.g_cameraTargetY;
    if (moving) {
      const int32_t s0 = sign(sub32(target, cam0));
      const int32_t nx = add32(g.gCameraTileX, mode_neg == 1 ? 2 * s0 : s0);
      const int32_t dy = sub32(g.g_cameraTargetY, g.gCameraTileY);
      g.gCameraTileX = nx;
      const int32_t s1 = sign(dy);
      g.gCameraTileY = add32(g.gCameraTileY, s1);
      return s1;
    }
  }
  const int32_t i = g.g_ui_55e920;
  const int32_t len = g.unk_005a9cd4;
  g.g_ui_55e920 = add32(i, 1);
  if (add32(i, 1) > len) {
    const uint32_t f = g.g_flags & 0xFDFFFFFFu;
    g.g_flags = f;
    g.g_cameraGoalX = -1;
    return i32(f);
  }
  const Ptr<int32_t> qrow = state_arr(ctx, &GameState::g_editQueueX) + i;
  const Ptr<int32_t> qcol = state_arr(ctx, &GameState::g_editQueueY) + i;
  const Ptr<int32_t> qmode = state_arr(ctx, &GameState::g_editQueueMode) + i;
  const int32_t col = qcol.get(), row = qrow.get(), mode = qmode.get();
  const int32_t idx = add32(col, mul32(50, row));
  const Ptr<uint16_t> tw = state_arr(ctx, &GameState::g_tileFlags) + idx;
  const Ptr<uint8_t> twh = tw.as<uint8_t>().byte(1);
  const Ptr<uint8_t> anim = state_arr(ctx, &GameState::tile_anim_frame) + idx;
  const Ptr<HoleRecord> holes = state_arr(ctx, &GameState::g_holes);
  if (mode & 0x80) {
    tw.set(static_cast<uint16_t>(tw.get() ^ static_cast<uint16_t>(mode & 0xFF7F)));
  } else if (mode & 0x100) {
    stamp_course_tiles(ctx, row, col, mode & ~0x100, 0);
    const int32_t m2 = qmode.get() & ~0x100;
    g.f_w_4c2848[3] = m2;
    if (m2 == 8 || m2 == 7 || m2 == 0xC || m2 == 9 || m2 == 0xB || m2 == 0xD || m2 == 0xE || m2 == 4) {
      twh.set(static_cast<uint8_t>(twh.get() | 0x40));
      anim.set(0);
      uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
      const int32_t r = i32(random_range(seed, 0x12C) & 0xFFFFu);
      if (registry_find("playSound") >= 0) call_peer(ctx, "playSound", {0x90, 0x64, 0, u32(neg32(r)), 0});
    }
    g.f_w_4c2848[3] = -1;
  } else {
    const Ptr<uint8_t> lv = state_arr(ctx, &GameState::g_cornerLevel) + add32(mul32(51, row), col);
    if (mode == 0x7F) lv.set(static_cast<uint8_t>(lv.get() + 1));
    if (mode == 0x7E) lv.set(static_cast<uint8_t>(lv.get() - 1));
    if (mode < 0x7D) {
      int32_t H = g.g_holesPlusOne;
      const int32_t hole_no = mode == 0 ? H : H - 1;
      if (mode == 0 || (mode == 2 && g.g_date % 6 == 0)) {
        (holes + H).field(&HoleRecord::par).set(4);
        const Ptr<Golfer> golfers = state_arr(ctx, &GameState::g_golfers);
        const int32_t g1 = i32(call_peer(ctx, "updateMembership", {0}).eax);
        const Ptr<Golfer> a = golfers + g1;
        a.field(&Golfer::hole).set(static_cast<int8_t>(hole_no));
        const int32_t rr = add32(shl32(qrow.get(), 10), 0x200);
        a.field(&Golfer::ball_x).set(rr);
        a.field(&Golfer::x).set(rr);
        const int32_t cc = add32(shl32(qcol.get(), 10), 0x200);
        a.field(&Golfer::ball_y).set(cc);
        a.field(&Golfer::y).set(cc);
        a.field(&Golfer::strokes).set(1);
        a.field(&Golfer::wait).set(2);
        a.field(&Golfer::partner).set(static_cast<int16_t>(g1 ^ 1));
        const int32_t g2 = i32(call_peer(ctx, "updateMembership", {0}).eax);
        const Ptr<Golfer> b = golfers + g2;
        b.field(&Golfer::hole).set(static_cast<int8_t>(hole_no));
        const int32_t rr2 = shl32(qrow.get(), 10);
        b.field(&Golfer::ball_x).set(rr2);
        b.field(&Golfer::x).set(rr2);
        const int32_t cc2 = shl32(qcol.get(), 10);
        b.field(&Golfer::ball_y).set(cc2);
        b.field(&Golfer::y).set(cc2);
        b.field(&Golfer::strokes).set(1);
        b.field(&Golfer::wait).set(8);
        b.field(&Golfer::partner).set(static_cast<int16_t>(g2 ^ 1));
        H = g.g_holesPlusOne;
      }
      (state_arr(ctx, &GameState::g_tileType) + idx).set(static_cast<int8_t>(qmode.get()));
      const int32_t m3 = qmode.get();
      if ((state_arr(ctx, &GameState::g_tileTypeAttrs) + m3).field(&TileTypeAttr::collar).get() == 0xD) {
        twh.set(static_cast<uint8_t>(twh.get() | 0x40));
        anim.set(0);
      }
      const Ptr<HoleRecord> h = holes + H;
      if (m3 == 1) {
        if (h.field(&HoleRecord::cup_x).get() == 0) {
          h.field(&HoleRecord::cup_x).set(row);
          h.field(&HoleRecord::cup_y).set(col);
          const int32_t w0 = h.field(&HoleRecord::tee_x).get();
          tw.set(static_cast<uint16_t>(H | 0x80));
          if (w0 != 0) {
            g.g_cameraGoalX = add32(w0, row) / 2;
            const int32_t e = add32(h.field(&HoleRecord::tee_y).get(), col);
            h.field(&HoleRecord::par).set(4);
            g.g_cameraTargetY = e / 2;
            g.g_holesPlusOne = add32(H, 1);
          }
        }
      }
      if (qmode.get() == 0) {
        h.field(&HoleRecord::tee_x).set(row);
        h.field(&HoleRecord::tee_y).set(col);
        tw.set(static_cast<uint16_t>(tw.get() | static_cast<uint16_t>(H)));
        const int32_t w1 = h.field(&HoleRecord::cup_x).get();
        if (w1 != 0) {
          g.g_cameraGoalX = add32(w1, row) / 2;
          const int32_t e = add32(h.field(&HoleRecord::cup_y).get(), col);
          h.field(&HoleRecord::par).set(4);
          g.g_cameraTargetY = e / 2;
          g.g_holesPlusOne = add32(H, 1);
        }
      }
    }
  }
  mark_tile_kind11(ctx);
  rebuild_tile_coverage(ctx);
  g.f_w_4c2e00[1] = qrow.get();
  g.g_lastEditTileY = qcol.get();
  return i32(rebuild_heightfield(ctx));
}

namespace {
// The modal popup (showPopup, a ui function of phase 3.7) answers with the player's choice: the port has no answer of
// its own, so without a registered showPopup the call is a MissingResource.
int32_t show_popup(Ctx &ctx, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e) {
  return i32(call_peer(ctx, "showPopup", {u32(a), u32(b), u32(c), u32(d), u32(e)}).eax);
}
void play_sound(Ctx &ctx, std::initializer_list<uint32_t> args, uint32_t &eax) {
  // the audio backend's request: no simulation state; its eax (Snd::play's) is not simulation state either
  if (registry_find("playSound") >= 0) eax = call_peer(ctx, "playSound", args).eax;
}
// the static CRT's _itoa(v, buf, 10): '-' and the digits of the magnitude; returns buf
uint32_t itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
  char digits[12];
  int n = 0;
  uint32_t m = v < 0 ? 0u - u32(v) : u32(v);
  do {
    digits[n++] = static_cast<char>('0' + m % 10);
    m /= 10;
  } while (m);
  uint32_t p = buf;
  if (v < 0) ctx.mem.store<char>(p++, '-');
  while (n) ctx.mem.store<char>(p++, digits[--n]);
  ctx.mem.store<char>(p, 0);
  return buf;
}

// "really demolish <name>?" popup; on a zero answer clearObjectFootprint. Returns the original's eax.
int32_t confirm_demolish(Ctx &ctx, int32_t obj) {
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c4f98);
  vstrcpy(ctx.mem, text, pool + 0x1F0);
  const int16_t type = (state_arr(ctx, &GameState::g_placed_objects) + obj).field(&PlacedObject::type).get();
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_tab_buildItems_4c26b0) + u32(mul32(type, 20)));
  vstrcat(ctx.mem, text, pool + 0x1D4);
  const int32_t r = show_popup(ctx, 0x190, 0x64, 1, 1, 0);
  if (r != 0) return r;
  return i32(clear_object_footprint(ctx, obj));
}
}  // namespace

int32_t demolish_object(Ctx &ctx, int32_t x, int32_t y) {
  GameState &g = ctx.gs();
  const int32_t blocked = tile_blocked(ctx, x, y);
  if (blocked != 0) return blocked;
  const int32_t idx = add32(mul32(x, 50), y);
  const Ptr<uint16_t> w = state_arr(ctx, &GameState::g_tileFlags) + idx;
  const Ptr<uint8_t> wh = w.as<uint8_t>().byte(1);
  wh.set(static_cast<uint8_t>(wh.get() & 0xFE));
  const Ptr<HoleRecord> holes = state_arr(ctx, &GameState::g_holes);
  const Ptr<uint8_t> code = state_arr(ctx, &GameState::g_tileBuildCode) + idx;
  const Ptr<int8_t> type = state_arr(ctx, &GameState::g_tileType) + idx;
  uint32_t eax_hi = 0;  // the upper half of eax at the flag-word exits
  uint32_t eax = 0;
  const auto clear_hole = [&](int32_t hole, bool tee) {
    const Ptr<HoleRecord> h = holes + hole;
    h.field(&HoleRecord::par).set(0);
    (tee ? h.field(&HoleRecord::tee_x) : h.field(&HoleRecord::cup_x)).set(0);
    if (hole < g.g_holesPlusOne) g.g_holesPlusOne = hole;
  };
  if (w.get() & 0x8000) goto sound;
  if (w.get() & 0x400) {
    const int32_t obj = object_at(ctx, x, y);
    if (obj != -1) return confirm_demolish(ctx, obj);
    eax_hi = 0xFFFF0000u;
  }
  {
    const uint8_t s = code.get();
    if (s == 0xFF) goto none;
    if (s & 0x80) {  // a building
      if ((s & 0x7F) == 0x10) call_peer(ctx, "placeHabitat", {u32(x), u32(y), 4});
      const uint8_t b = code.get() & 0x7F;
      if (b == 0x13) w.set(static_cast<uint16_t>(w.get() & 0xFFDF));
      if (b == 1) wh.set(static_cast<uint8_t>(wh.get() & 0xFD));
      else if (b == 0) w.set(static_cast<uint16_t>(w.get() & 0xFFDF));
      else if (w.get() & 0x1000) w.set(static_cast<uint16_t>(w.get() & 0xEFFF));
      else w.set(static_cast<uint16_t>(w.get() & 0xEDDF));
    } else if (static_cast<int8_t>(s) < 0x7D) {  // a hole part
      const uint16_t dx = w.get();
      const int32_t hole = dx & 0x1F;
      const int8_t t = type.get();
      if (g.g_flags & 0x200000u) {
        if (t == 0) goto sound;
        if (t == 1 && (dx & 0x80)) goto sound;
      }
      if (t == 0) {
        if (hole != 0) {
          w.set(static_cast<uint16_t>(dx & 0xFFE0));
          (holes + hole).field(&HoleRecord::tee_x).set(0);
          (holes + hole).field(&HoleRecord::par).set(0);
          if (hole < g.g_holesPlusOne) g.g_holesPlusOne = hole;
        }
      } else if (t == 1) {
        if (hole != 0) {
          w.set(static_cast<uint16_t>(dx & 0xFFE0));
          (holes + hole).field(&HoleRecord::par).set(0);
          (holes + hole).field(&HoleRecord::cup_x).set(0);
          if (hole < g.g_holesPlusOne) g.g_holesPlusOne = hole;
        }
      }
      call_peer(ctx, "placeHabitat", {u32(x), u32(y), u32(static_cast<int32_t>(static_cast<int8_t>(s)))});
    }
    const int32_t refund = (state_arr(ctx, &GameState::g_tileRefund) + idx).get();
    g.g_cash_div100 = add32(g.g_cash_div100, refund);
    points_popup(ctx, refund, add32(shl32(x, 10), 0x200), add32(shl32(y, 10), 0x200), -1);
    const int32_t year_slot = g.g_year % 100;
    code.set(0xFF);
    const Ptr<int16_t> fy = (state_arr(ctx, &GameState::g_financeYears) + year_slot).field(&FinanceYear::build_course);
    if (SG_NEGCTL(g_negctl_demolish) != 1)
      fy.set(static_cast<int16_t>(fy.get() + static_cast<int16_t>((state_arr(ctx, &GameState::g_tileRefund) + idx).get())));
    goto rebuild;
  }
none:
  if (w.get() & 0x200) {
    w.set(static_cast<uint16_t>(w.get() & 0xFDFF));
    return i32(eax_hi | w.get());
  }
  eax_hi = 0;
  if (w.get() & 0x400) {
    const int32_t obj = object_at(ctx, x, y);
    if (obj != -1) return confirm_demolish(ctx, obj);
    eax_hi = 0xFFFF0000u;
  }
  if (w.get() & 0x20) {
    w.set(static_cast<uint16_t>(w.get() & 0xFFDF));
    return i32(eax_hi | w.get());
  }
  if (w.get() & 0x1000) {
    w.set(static_cast<uint16_t>(w.get() & 0xEFFF));
    return i32(eax_hi | w.get());
  }
  {
    const int8_t t = type.get();
    const int32_t hole = w.get() & 0x1F;
    if (t == 0) clear_hole(hole, true);
    else if (t == 1 && (w.get() & 0x80)) clear_hole(hole, false);
  }
rebuild:
  g.f_w_4c2e00[1] = x;
  g.g_lastEditTileY = y;
  rebuild_heightfield(ctx);
  return i32(mark_tile_kind11(ctx));
sound:
  play_sound(ctx, {0x18, 100, 0, 0, 0}, eax);
  return i32(eax);
}

int32_t build_tournament_prep(Ctx &ctx) {
  GameState &g = ctx.gs();
  const uint32_t text = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4e28e0);
  const uint32_t num = state_va(ctx, &GameState::g_itoaScratch838eac);
  vstrcpy(ctx.mem, text, pool + 0x1474);
  vstrcat(ctx.mem, text, pool + 0x1440);
  vstrcat(ctx.mem, text, pool + 0x1414);
  vstrcat(ctx.mem, text, pool + 0x13E0);
  int16_t changed[24] = {};
  int32_t count = 0;
  const Ptr<HoleRecord> holes = state_arr(ctx, &GameState::g_holes);
  for (int32_t hole = 1; hole <= 18; ++hole) {
    const Ptr<HoleRecord> h = holes + hole;
    const int8_t par = h.field(&HoleRecord::par).get();
    if (par == 0) continue;
    const int16_t len = h.field(&HoleRecord::length).get();
    int32_t want = 3;
    if (len >= 0x12C) want = 4;
    if (len >= 0x1F4) want = 5;
    if (want == par) continue;
    vstrcat(ctx.mem, text, pool + 0x13D0);
    vstrcat(ctx.mem, text, itoa10(ctx, hole, num));
    vstrcat(ctx.mem, text, pool + 0x13C0);
    vstrcat(ctx.mem, text, itoa10(ctx, par, num));
    vstrcat(ctx.mem, text, pool + 0x13B4);
    vstrcat(ctx.mem, text, itoa10(ctx, want, num));
    h.field(&HoleRecord::par).set(static_cast<int8_t>(want));
    vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c5ec8) + 0x110);
    changed[static_cast<int16_t>(count)] = static_cast<int16_t>(hole);
    ++count;
  }
  uint32_t mask = (g.g_flags & 0x4000000u) ? 0xFFFFFu : u32(show_popup(ctx, 0x190, 0x64, 0xFFFFF, 0, 0));
  g.g_tournamentPrep = i32(mask);
  const int32_t n = static_cast<int16_t>(count);
  uint32_t eax = 0;
  if (n > 0) {
    for (int32_t i = 0; i < n; ++i)
      if ((mask & (1u << ((i + 3) & 0x1F))) == 0) {
        const Ptr<int8_t> p = (holes + changed[i]).field(&HoleRecord::par);
        p.set(static_cast<int8_t>(p.get() + 1));
      }
    eax = u32(n);
  }
  uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
  const auto rnd2 = [&] { return (random_range(seed, 2) & 0xFFFFu) != 0; };
  for (int32_t hole = 1; hole <= 18; ++hole) {
    const Ptr<HoleRecord> h = holes + hole;
    const uint8_t par = static_cast<uint8_t>(h.field(&HoleRecord::par).get());
    eax = (eax & ~0xFFu) | par;
    if (par == 0) continue;
    if ((mask & 1) == 0) continue;
    for (int32_t pass = 0; pass < 2; ++pass) {
      if (pass != 0 && hole < g.g_holesPlusOne - 2) continue;
      int32_t dir = h.field(&HoleRecord::direction).get();
      int32_t y = h.field(&HoleRecord::cup_y).get();
      int32_t x = h.field(&HoleRecord::cup_x).get();
      if (pass != 0) dir = ((rnd2() ? 2 : -2) + dir) & 7;
      for (int32_t step = 0; step < 7; ++step) {
        if (rnd2()) x = add32(x, state_arr(ctx, &GameState::f_tab_dirDx_4c2878)[dir]);
        if (rnd2()) y = add32(y, state_arr(ctx, &GameState::f_tab_dirDy_4c2898)[dir]);
        if (step <= 1) continue;
        const int32_t idx = add32(mul32(x, 50), y);
        const int8_t t = (state_arr(ctx, &GameState::g_tileType) + idx).get();
        const uint8_t cls = static_cast<uint8_t>((state_arr(ctx, &GameState::g_tileTypeAttrs) + t).field(&TileTypeAttr::collar).get());
        if (cls != 4 && cls != 0xD) continue;
        if (t == 0x16) continue;
        if (tile_blocked(ctx, x, y) != 0) continue;
        if ((state_arr(ctx, &GameState::g_tileFlags) + idx).as<int16_t>().get() != 0) continue;
        const int32_t obj = stamp_course_tiles(ctx, x, y, pass != 0 ? 0x12 : 0x11, 0);
        const Ptr<PlacedObject> o = state_arr(ctx, &GameState::g_placed_objects) + obj;
        o.field(&PlacedObject::flags).set(static_cast<uint8_t>(o.field(&PlacedObject::flags).get() | 0x40));
        const int32_t dx = sub32(h.field(&HoleRecord::cup_x).get(), x);
        const int32_t dy = sub32(h.field(&HoleRecord::cup_y).get(), y);
        const int32_t ady = abs32(dy), adx = abs32(dx);
        const uint8_t orient = adx > ady ? (dx <= 0 ? 3 : 1) : (dy <= 0 ? 0 : (SG_NEGCTL(g_negctl_tournament) == 1 ? 1 : 2));
        o.field(&PlacedObject::orientation).set(orient);
        break;
      }
    }
    eax = 2;
    mask = u32(g.g_tournamentPrep);
  }
  return i32(eax);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }
void a_demolish(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(demolish_object(c, S(a, 0), S(a, 1))); }
void a_tournament(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(build_tournament_prep(c)); }
void a_edit_queue(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(step_course_edit_queue(c)); }
void a_stamp(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(stamp_course_tiles(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }
void a_clear_footprint(Ctx &c, const uint32_t *a, Ret &) { clear_object_footprint(c, S(a, 0)); }

const FuncDef kFuncs[] = {
    {"stampCourseTiles", "default", "int", 4, 0x027F, a_stamp, &g_negctl_stamp},
    {"clearObjectFootprint", "default", "void", 1, 0x027F, a_clear_footprint, &g_negctl_clear_footprint},
    {"stepCourseEditQueue", "default", "int", 1, 0x027F, a_edit_queue, &g_negctl_edit_queue},
    {"demolishObject", "default", "int", 3, 0x027F, a_demolish, &g_negctl_demolish},
    {"buildTournamentPrep", "default", "int", 1, 0x027F, a_tournament, &g_negctl_tournament},
};
}  // namespace

extern const ModuleDef kModule_terrain_course = {"terrain_course", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
