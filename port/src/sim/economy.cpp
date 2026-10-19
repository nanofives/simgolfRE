// Economy and membership, text-building leaves (phase 3.2, 2026-10-19). Written from the specification (hook sources,
// purpose notes, vector corpus); no original code or address. Conventions: port/CORE.md.
#include "portsim/sim/economy.h"

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/sim/tiles.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   computeRatingTrend    1 = sel 2 uses slot 6, 2 = outer weight starts at 0
//   appendDate            1 = year + 2000, 2 = month = date / 1024 % 7
//   appendNumber          1 = no zero padding below 100
//   appendRatingLabel     1 = q > 3 selects label 3
//   appendToBuffer45b8b0  1 = first free slot index > 0x1f
int g_negctl_compute_rating_trend = 0;
int g_negctl_append_date = 0;
int g_negctl_append_number = 0;
int g_negctl_append_rating_label = 0;
int g_negctl_append_to_buffer = 0;
//   clearCost             1 = type 4 costs 13, 2 = collar 13 uses cost * 2 / 2
//   rateLot               1 = ring sums use x + 1 for the east column, 2 = flag bits add 50
//   updateRollingStats    1 = date test without the 0x400 grace, 2 = decay shift 5
//   updateMembership      1 = ties counted from 0, 2 = 998 attempts
int g_negctl_clear_cost = 0;
int g_negctl_rate_lot = 0;
int g_negctl_update_rolling_stats = 0;
int g_negctl_update_membership = 0;

uint32_t text_va(Ctx &c) { return state_va(c, &GameState::g_textBuffer); }

// strcat of one character (the original appends one-character literals)
void append_char(Ctx &c, char ch) {
  const uint32_t t = text_va(c);
  const uint32_t n = vstrlen(c.mem, t);
  c.mem.store<uint8_t>(t + n, static_cast<uint8_t>(ch));
  c.mem.store<uint8_t>(t + n + 1, 0);
}

// strcat(g_textBuffer, itoa(v, g_textBuf, 10)): the CRT's __itoa leaves the digits (and '-') plus a NUL in the
// scratch buffer g_textBuf, then the text is appended from it.
void append_itoa(Ctx &c, int32_t v) {
  const uint32_t scratch = state_va(c, &GameState::g_textBuf);
  char tmp[16];
  int n = 0;
  uint32_t m = v < 0 ? u32(neg32(v)) : u32(v);
  do {
    tmp[n++] = static_cast<char>('0' + m % 10);
    m /= 10;
  } while (m);
  uint32_t o = 0;
  if (v < 0) c.mem.store<uint8_t>(scratch + o++, '-');
  while (n) c.mem.store<uint8_t>(scratch + o++, static_cast<uint8_t>(tmp[--n]));
  c.mem.store<uint8_t>(scratch + o, 0);
  vstrcat(c.mem, text_va(c), scratch);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ computeRatingTrend
// Over the hole record idx (stride 0x208): two 8-slot accumulators; weight slot j starts at the par byte << 3, count
// slot j at 8; for outer 1..9 and j 0..7 the signed word at record + 0x2a + (outer-1)*2 + j*0x16 (the histogram rows)
// adds to the count slot and word * outer to the weight slot. sel 1 / 2 / 4 pick slot 6 / 5 / 3 (non-zero count
// required); the result is w[p]*100/c[p] - w[7]*100/c[7]; 0 when c[7] == 0 or the pick fails.
int32_t compute_rating_trend(Ctx &ctx, int32_t idx, int32_t sel) {
  const Ptr<uint8_t> rec = state_arr(ctx, &GameState::g_holes).as<uint8_t>() + mul32(idx, 0x208);
  const int32_t par = sx8(rec.get());
  int32_t w[8], cnt[8];
  const int32_t w0 = SG_NEGCTL(g_negctl_compute_rating_trend) == 2 ? 0 : shl32(par, 3);
  for (int j = 0; j < 8; j++) {
    w[j] = w0;
    cnt[j] = 8;
  }
  for (int32_t outer = 1; outer <= 9; outer++)
    for (int32_t j = 0; j < 8; j++) {
      const int32_t s = sx16(rec.byte(0x2a + (outer - 1) * 2 + j * 0x16).as<uint16_t>().get());
      cnt[j] = add32(cnt[j], s);
      w[j] = add32(w[j], mul32(s, outer));
    }
  if (cnt[7] == 0) return 0;
  int32_t num, div;
  const int slot6 = SG_NEGCTL(g_negctl_compute_rating_trend) == 1 ? 6 : 5;
  if (cnt[6] != 0 && sel == 1) {
    num = w[6];
    div = cnt[6];
  } else if (cnt[slot6] != 0 && sel == 2) {
    num = w[slot6];
    div = cnt[slot6];
  } else if (cnt[3] != 0 && sel == 4) {
    num = w[3];
    div = cnt[3];
  } else {
    return 0;
  }
  return sub32(div32(mul32(num, 100), div), div32(mul32(w[7], 100), cnt[7]));
}

// ------------------------------------------------------------------------------------------------ appendDate
// year = date / 8192, month = date / 1024 % 8; g_year = (short)year; appends the month name (pointer table, month
// unchecked), a space and year + 2001.
void append_date(Ctx &ctx, int32_t date) {
  const int32_t year = date / 8192;
  int32_t month = date / 1024 % 8;
  if (SG_NEGCTL(g_negctl_append_date) == 2) month = date / 1024 % 7;
  ctx.gs().g_year = static_cast<int16_t>(year);
  const uint32_t name = (state_arr(ctx, &GameState::f_ptrtab_4c2908) + month).get();
  vstrcat(ctx.mem, text_va(ctx), name);
  append_char(ctx, ' ');
  append_itoa(ctx, add32(year, SG_NEGCTL(g_negctl_append_date) == 1 ? 2000 : 2001));
}

// ------------------------------------------------------------------------------------------------ appendNumber
// Thousands separators: '-' and negate for n < 0, recurse on n / 1000 and ',' for n >= 1000, then r = n % 1000 with
// zero padding ('0' when n >= 1000 and r < 100, another when n >= 100 and r < 10, never for n == 0), then itoa(r).
void append_number(Ctx &ctx, int32_t n) {
  if (n < 0) {
    append_char(ctx, '-');
    n = neg32(n);
  }
  if (n >= 1000) {
    append_number(ctx, n / 1000);
    append_char(ctx, ',');
  }
  const int32_t r = n % 1000;
  if (r < 100 && n != 0 && SG_NEGCTL(g_negctl_append_number) != 1) {
    if (n >= 1000) append_char(ctx, '0');
    if (r < 10 && n >= 100) append_char(ctx, '0');
  }
  append_itoa(ctx, r);
}

// ------------------------------------------------------------------------------------------------ appendRatingLabel
// v < 0 -> label at pool offset 0x30; else q = v / 25: 0 -> 0x28, 1 -> 0x20, 2 and 3 -> 0x10, above 3 -> 0 (all in
// f_strpool_4d3144); strcat onto the text buffer.
void append_rating_label(Ctx &ctx, int32_t v) {
  uint32_t off;
  if (v < 0) {
    off = 0x30;
  } else {
    const int32_t q = v / 25;
    if (q == 0) off = 0x28;
    else if (q == 1) off = 0x20;
    else if (q <= 3 || SG_NEGCTL(g_negctl_append_rating_label) == 1) off = 0x10;
    else off = 0;
  }
  vstrcat(ctx.mem, text_va(ctx), state_va(ctx, &GameState::f_strpool_4d3144) + off);
}

// ------------------------------------------------------------------------------------------------ appendToBuffer45b8b0
// The text store g_namePool[0x1002] with 128 offsets (-1 = empty) and lengths. An id != -1 whose offset is live is
// removed first (later bytes packed down, later offsets shifted); the slot is the id, or for -1 the first empty slot
// above 0x20; the text buffer is stored at the highest end when it fits below 0x1002. Returns the slot or -1.
int32_t append_to_buffer_45b8b0(Ctx &ctx, int32_t id) {
  const Ptr<uint8_t> buf = state_arr(ctx, &GameState::g_namePool).as<uint8_t>();
  const Ptr<int16_t> off = state_arr(ctx, &GameState::g_namePoolOffset);
  const Ptr<int16_t> len = state_arr(ctx, &GameState::g_namePoolLength);
  if (id != -1 && off.at(id).get() != -1) {
    const int32_t o = off.at(id).get();
    const int32_t n = add32(i32(vstrlen(ctx.mem, buf.va() + u32(o))), 1);
    const uint32_t count = u32(sub32(sub32(0x1002, n), o));
    const uint8_t *src = ctx.mem.at(buf.va() + u32(o) + u32(n), count);
    uint8_t *dst = ctx.mem.at(buf.va() + u32(o), count, true);
    std::memmove(dst, src, count);
    off.at(id).set(-1);
    for (int32_t i = 0; i < 128; i++)
      if (off.at(i).get() > o) off.at(i).set(static_cast<int16_t>(off.at(i).get() - n));
  }
  int32_t end = 0, slot = -1;
  const int32_t first = SG_NEGCTL(g_negctl_append_to_buffer) == 1 ? 0x1f : 0x20;
  for (int32_t i = 0; i < 128; i++) {
    const int32_t o = off.at(i).get();
    if (o != -1) {
      if (len.at(i).get() + o > end) end = len.at(i).get() + o;
    }
    if (o == -1 && slot == -1 && i > first) slot = i;
  }
  if (id != -1) slot = id;
  const int32_t n = add32(i32(vstrlen(ctx.mem, text_va(ctx))), 1);
  if (add32(n, end) < 0x1002) {
    vstrcpy(ctx.mem, buf.va() + u32(end), text_va(ctx));
    off.at(slot).set(static_cast<int16_t>(end));
    len.at(slot).set(static_cast<int16_t>(n));
    return slot;
  }
  return -1;
}

// ------------------------------------------------------------------------------------------------ clearCost
// The cost of clearing tile (x, y) by its type: 0 when blocked; type 4 -> 12, 0x14 -> 0, 0x15 -> -16; a type whose walk
// byte is <= 0 -> -8; 0x11 -> 32; 0x12 on course type 1 -> 16; collar group 13 -> cost * 5 / 2; else cost * walk / 2.
int32_t clear_cost(Ctx &ctx, int32_t x, int32_t y) {
  if (tile_blocked(ctx, x, y)) return 0;
  const int32_t t = state_grid(ctx, &GameState::g_tileType, 50)(x, y);
  if (t == 4) return SG_NEGCTL(g_negctl_clear_cost) == 1 ? 13 : 12;
  if (t == 0x14) return 0;
  if (t == 0x15) return -0x10;
  const Ptr<TileTypeAttr> a = state_arr(ctx, &GameState::g_tileTypeAttrs) + t;
  const int32_t walk = a.field(&TileTypeAttr::walk).get();
  const int32_t cost = a.field(&TileTypeAttr::cost).get();
  const int32_t collar = a.field(&TileTypeAttr::collar).get();
  if (walk <= 0) return -8;
  if (t == 0x11) return 0x20;
  if (ctx.gs().g_courseType == 1 && t == 0x12) return 0x10;
  if (collar == 13) return SG_NEGCTL(g_negctl_clear_cost) == 2 ? cost * 2 / 2 : cost * 5 / 2;
  return cost * walk / 2;
}

// ------------------------------------------------------------------------------------------------ rateLot
// Lot value: the clearCost sum over the 12-cell ring of the 2x2 footprint at (x, y), times the best hole value over
// holes 1..18, / 40. A hole counts when par != 0 and first_shots != 0: mood_sum * 1000 / (first_shots + u024 / 2 + 4)
// + (3 - difficulty) * 100, +100 per flag bit 0 / 1, divided by (nearest of tee, cup and the hole's moving point
// distance + 8); the maximum, raised by level word * best / 3 when that word is non-zero.
int32_t rate_lot(Ctx &ctx, int32_t x, int32_t y) {
  int32_t sum = 0;
  const int32_t east = SG_NEGCTL(g_negctl_rate_lot) == 1 ? 1 : 2;
  for (int32_t i = -1; i < 3; i++) {
    sum = add32(sum, clear_cost(ctx, add32(x, i), sub32(y, 1)));
    sum = add32(sum, clear_cost(ctx, add32(x, i), add32(y, 2)));
  }
  for (int32_t i = 0; i < 2; i++) {
    sum = add32(sum, clear_cost(ctx, sub32(x, 1), add32(y, i)));
    sum = add32(sum, clear_cost(ctx, add32(x, east), add32(y, i)));
  }
  int32_t best = 0;
  const Ptr<HoleRecord> holes = state_arr(ctx, &GameState::g_holes);
  const Ptr<HolePoints> pts = state_arr(ctx, &GameState::g_holePoints);
  for (int32_t i = 0; i < 18; i++) {
    const Ptr<HoleRecord> r = holes + (i + 1);
    if (r.field(&HoleRecord::par).get() == 0 || r.field(&HoleRecord::first_shots).get() == 0) continue;
    const int32_t n = r.field(&HoleRecord::first_shots).get();
    const int32_t m = r.field(&HoleRecord::u024).get();
    const int32_t s158 = r.field(&HoleRecord::mood_sum).get();
    int32_t v = add32(div32(mul32(s158, 1000), add32(add32(n, m / 2), 4)), mul32(sub32(3, ctx.gs().g_difficulty), 100));
    const int32_t flags = i32(r.field(&HoleRecord::flags).get());
    const int32_t bonus = SG_NEGCTL(g_negctl_rate_lot) == 2 ? 50 : 100;
    if (flags & 1) v = add32(v, bonus);
    if (flags & 2) v = add32(v, bonus);
    int32_t d = distance(sub32(x, r.field(&HoleRecord::tee_x).get()), sub32(y, r.field(&HoleRecord::tee_y).get()));
    int32_t d2 = distance(sub32(x, r.field(&HoleRecord::cup_x).get()), sub32(y, r.field(&HoleRecord::cup_y).get()));
    if (d2 < d) d = d2;
    const Ptr<HolePoints> p = pts + (i + 1);
    if (p.field(&HolePoints::b_x).get() != -1) {
      d2 = distance(sub32(x, sar32(p.field(&HolePoints::b_x).get(), 10)),
                    sub32(y, sar32(p.field(&HolePoints::b_y).get(), 10)));
      if (d2 < d) d = d2;
    }
    v = div32(v, add32(d, 8));
    if (v > best) best = v;
  }
  const int32_t level = ctx.gs().g_activeObjectLevel[12];
  if (level != 0) best = add32(best, div32(mul32(level, best), 3));
  return div32(mul32(best, sum), 40);
}

// ------------------------------------------------------------------------------------------------ updateRollingStats
// Over the 256 ball records (x = -1 is free): x += foldRange(direction, speed / 16); y -= absDiff(direction, speed / 16);
// height = height + vspeed / 32; when the height is <= 0 vspeed, speed and height become 0 and the record is freed once
// the date is past its stamp + 0x400; then speed -= speed >> 4 and, unless height and vspeed are both 0, vspeed -= 0x40.
void update_rolling_stats(Ctx &ctx) {
  const Ptr<BallRecord> recs = state_arr(ctx, &GameState::g_ballRecords);
  for (int32_t i = 0; i < 256; i++) {
    const Ptr<BallRecord> r = recs + i;
    const Ptr<int32_t> x = r.byte(8).as<int32_t>(), y = r.byte(0xc).as<int32_t>(), h = r.byte(0x10).as<int32_t>(),
                       dir = r.byte(0x14).as<int32_t>(), sp = r.byte(0x18).as<int32_t>(),
                       vs = r.byte(0x1c).as<int32_t>(), dt = r.byte(0x20).as<int32_t>();
    if (x.get() == -1) continue;
    x.set(add32(x.get(), fold_range(ctx, dir.get(), sp.get() / 16)));
    y.set(sub32(y.get(), abs_diff(ctx, dir.get(), sp.get() / 16)));
    const int32_t acc = add32(h.get(), vs.get() / 32);
    h.set(acc);
    if (acc <= 0) {
      vs.set(0);
      sp.set(0);
      h.set(0);
      const int32_t grace = SG_NEGCTL(g_negctl_update_rolling_stats) == 1 ? 0 : 0x400;
      if (ctx.gs().g_date > add32(dt.get(), grace)) x.set(-1);
    }
    sp.set(sub32(sp.get(), sar32(sp.get(), SG_NEGCTL(g_negctl_update_rolling_stats) == 2 ? 5 : 4)));
    if (h.get() != 0 || vs.get() != 0) vs.set(add32(vs.get(), -0x40));
  }
}

// ------------------------------------------------------------------------------------------------ updateMembership
namespace {
// typeBit7Clear (phase 3.3's function, 44 bytes, kept private here): 1 when bit 7 of the demographics byte of golfer
// g's type record is clear.
int32_t type_bit7_clear(Ctx &ctx, int32_t g) {
  const int32_t type = (state_arr(ctx, &GameState::g_golfers) + g).field(&Golfer::type).get();
  const uint8_t b = (state_arr(ctx, &GameState::g_golferTypes) + type).field(&GolferType::demographics).get();
  return (b & 0x80) ? 0 : 1;
}
}  // namespace

// Generates a new golfer in the next record slot s = counter % 0x98 (the counter increments) and returns s, or -1 with
// the decline message in the text buffer after 999 refused type draws.
// Spec: re/analysis/economy/00421bc0_updateMembership.md.
int32_t update_membership(Ctx &ctx, int32_t members_only) {
  auto range = [&](int32_t n) { return i32(random_range(ctx.gs().g_rng.state, u32(n)) & 0xFFFFu); };
  const int32_t counter = ctx.gs().g_nextGolferSlot;
  const int32_t s = mod32(counter, 0x98);
  ctx.gs().g_nextGolferSlot = add32(counter, 1);
  const Ptr<Golfer> g = state_arr(ctx, &GameState::g_golfers) + s;
  for (int32_t i = 0; i < 0x40; i++) g.byte(i * 4).as<uint32_t>().set(0);
  const Ptr<uint8_t> h = state_arr(ctx, &GameState::g_holes).as<uint8_t>();
  g.field(&Golfer::x).set(add32(mul32(h.byte(0x26a0).as<int32_t>().get(), 0x400), 0x600));
  g.field(&Golfer::y).set(add32(mul32(h.byte(0x26a4).as<int32_t>().get(), 0x400), 0x600));
  g.field(&Golfer::facing).set(static_cast<int8_t>(h.byte(0x209).get()));
  g.field(&Golfer::traits).set(static_cast<uint16_t>(range(0x7fff)));

  const bool all = (ctx.gs().g_flags & 0x40) != 0;
  const Ptr<int32_t> counts = state_arr(ctx, &GameState::g_golferClassSlotCounts) + mul32(s & 3, 8);
  int32_t m = 9999, n = SG_NEGCTL(g_negctl_update_membership) == 1 ? 0 : members_only;
  for (int32_t c = 0; c < 8; c++) {
    if (!all && (c < 3 || c == 4)) continue;
    const int32_t v = counts.at(c).get();
    if (v < m) {
      m = v;
      n = 1;
    } else if (v == m) {
      n++;
    }
  }
  int32_t pick = range(n);
  for (int32_t c = 0; c < 8; c++) {
    if (!all && (c < 3 || c == 4)) continue;
    if (counts.at(c).get() != m) continue;
    if (--pick < 0) {
      g.field(&Golfer::prefs).set(static_cast<uint8_t>(c));
      pick = 99;
    }
  }
  {
    const Ptr<int32_t> slot = state_arr(ctx, &GameState::g_golferClassSlotCounts) +
                              add32(g.field(&Golfer::prefs).get(), mul32(g.field(&Golfer::cls).get(), 8));
    slot.set(add32(slot.get(), 1));
  }

  const int32_t attempts = SG_NEGCTL(g_negctl_update_membership) == 2 ? 998 : 999;
  for (int32_t attempt = 0; attempt < attempts; attempt++) {
    g.field(&Golfer::type).set(static_cast<int16_t>(range(0x4b) + 1));
    const int32_t t = g.field(&Golfer::type).get();
    const Ptr<GolferType> gt = state_arr(ctx, &GameState::g_golferTypes) + t;
    uint8_t cat;
    if (type_bit7_clear(ctx, s) == 0) {
      cat = 6;
    } else {
      const uint8_t f = gt.field(&GolferType::personality).get();
      cat = 1;
      if (f & 0x11) cat = 3;
      if (f & 0xc) cat |= 4;
      if (cat == 1) cat = (s & 1) ? 3 : 5;
    }
    g.field(&Golfer::prefs).set(cat);
    const uint32_t custom = gt.field(&GolferType::custom).get();
    if (custom != 0) g.field(&Golfer::prefs).set(static_cast<uint8_t>(custom & 7));

    bool ok = true;
    for (int32_t j = 0; j < 0x98; j++) {
      if (j == s) continue;
      const Ptr<Golfer> o = state_arr(ctx, &GameState::g_golfers) + j;
      const int32_t status = o.field(&Golfer::hole).get();
      const int32_t other = o.field(&Golfer::type).get();
      if (status == -1) {
        if (other % 19 == t % 19) ok = false;
        if (other == t) ok = false;
      } else if (status > 0 && other == t) {
        ok = false;
      }
    }
    const Ptr<GolferTypeState> ts = state_arr(ctx, &GameState::g_golferTypeState) + t;
    if (ts.field(&GolferTypeState::ban).get() == 0xff) ok = false;
    if (members_only != 0 && ts.field(&GolferTypeState::membership).get() == 0) continue;
    if (!ok) continue;

    g.field(&Golfer::traits)
        .set(static_cast<uint16_t>(add32(shl32(range(0x80), 8), sx8(gt.field(&GolferType::demographics).get()))));
    g.field(&Golfer::cls).set(static_cast<int8_t>(t % 3));
    g.field(&Golfer::hole).set(static_cast<int8_t>(0xff));
    g.field(&Golfer::state).set(0xb);
    g.field(&Golfer::date_stamp).set(ctx.gs().g_date);
    g.field(&Golfer::mood).set(static_cast<int16_t>(range(3) + 3));
    if (ctx.gs().g_difficulty == 0) g.field(&Golfer::mood).set(4);
    const int32_t l9 = ctx.gs().g_activeObjectLevel[9];
    if (l9 != 0) g.field(&Golfer::mood).set(static_cast<int16_t>(add32(l9, 4)));
    if (ctx.gs().g_activeObjectLevel[11] != 0) g.field(&Golfer::flags).set(g.field(&Golfer::flags).get() | 0x10000u);
    return s;
  }
  // 999 refusals: the decline text, the counter steps back
  vstrcpy(ctx.mem, state_va(ctx, &GameState::g_textBuffer), state_va(ctx, &GameState::f_strpool_4c6ba4) + 0x108);
  ctx.gs().g_nextGolferSlot = mod32(sub32(ctx.gs().g_nextGolferSlot, 1), 0x98);
  return -1;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_compute_rating_trend(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(compute_rating_trend(c, i32(a[0]), i32(a[1]))); }
void a_append_date(Ctx &c, const uint32_t *a, Ret &) { append_date(c, i32(a[0])); }
void a_append_number(Ctx &c, const uint32_t *a, Ret &) { append_number(c, i32(a[0])); }
void a_append_rating_label(Ctx &c, const uint32_t *a, Ret &) { append_rating_label(c, i32(a[0])); }
void a_append_to_buffer(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(append_to_buffer_45b8b0(c, i32(a[0]))); }
void a_clear_cost(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(clear_cost(c, i32(a[0]), i32(a[1]))); }
void a_rate_lot(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(rate_lot(c, i32(a[0]), i32(a[1]))); }
void a_update_rolling_stats(Ctx &c, const uint32_t *, Ret &) { update_rolling_stats(c); }
void a_update_membership(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(update_membership(c, i32(a[0]))); }

const FuncDef kFuncs[] = {
    {"clearCost", "default", "int", 2, 0, a_clear_cost, &g_negctl_clear_cost},
    {"rateLot", "default", "int", 2, 0, a_rate_lot, &g_negctl_rate_lot},
    {"updateRollingStats", "default", "void", 0, 0, a_update_rolling_stats, &g_negctl_update_rolling_stats},
    {"updateMembership", "default", "int", 1, 0, a_update_membership, &g_negctl_update_membership},
    {"computeRatingTrend", "default", "int", 2, 0, a_compute_rating_trend, &g_negctl_compute_rating_trend},
    {"appendDate", "default", "void", 1, 0, a_append_date, &g_negctl_append_date},
    {"appendNumber", "default", "void", 1, 0, a_append_number, &g_negctl_append_number},
    {"appendRatingLabel", "default", "void", 1, 0, a_append_rating_label, &g_negctl_append_rating_label},
    {"appendToBuffer45b8b0", "default", "int", 1, 0, a_append_to_buffer, &g_negctl_append_to_buffer},
};
}  // namespace

extern const ModuleDef kModule_economy = {"economy", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
