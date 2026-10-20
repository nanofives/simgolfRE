// Module `reports` (phase 3.8, stream 38c): the command effects of the two big report screens, split from their drawing
// the way 3.7b split the others (log/p3/p37b_notes.txt). Written from the hook transcriptions' behaviour (c3fr.cpp
// showCourseReport, c3er.cpp showEndOfYearScreen) as reading reference; no original code or address. What is kept:
//   * every simulation write (flags, report score, purse, debt counter, member history),
//   * every call into another stage (appendNumber / appendCents / appendHoleName / loadStoryStrings / landmarkName /
//     postEvent / addScore / drawScoreSummary / bucketValue), in the original's order with its arguments, because those
//     write the hashed formatting buffer g_textBuf and the story / news state,
//   * every division the original performs (a zero divisor stays a DivideError),
//   * the modal loop's input handling (the pump model of module commands).
// What is dropped: widget, image and string drawing, window refresh, the text assembled in g_textBuffer, waits and the
// audio requests (startSoundSlot; playSound only as the optional audio backend call).
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/util.h"
#include "commands_internal.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   showCourseReport     1 = the purse uses (score - 50) / 5 + 4, 2 = a passing report sets only flag bit 22,
//                        3 = the full-marks event is 0x14, 4 = the facility count starts at bit 7, 5 = a zero-star facility
//                        criterion does not zero the report, 6 = the leave clears the mode byte to 1
//   showEndOfYearScreen  1 = the member history stores the live count + 1, 2 = the debt counter is not cleared outside
//                        debt, 3 = a right click ends the screen too, 4 = the story-text event 0x120 passes the subject + 1
//                        to loadStoryStrings, 5 = addScore is not asked after the loop
int g_negctl_course_report = 0;
int g_negctl_end_of_year = 0;

using cm::call_peer;

void reset_text(Ctx &ctx) { ctx.mem.store<uint8_t>(state_va(ctx, &GameState::g_textBuffer), 0); }
void number(Ctx &ctx, int32_t v) {
  reset_text(ctx);
  call_peer(ctx, "appendNumber", {u32(v)});
}
void cents(Ctx &ctx, int32_t v) {
  reset_text(ctx);
  cm::append_cents(ctx, v);
}

// the one-button modal loop both screens end with: a Blocking pump per pass, then the exit tests. The course report
// leaves on any button or a key (button tested first), the end-of-year screen only on the left button or a key.
void report_loop(Ctx &ctx, bool left_only, const char *where) {
  GameState &gs = ctx.gs();
  for (;;) {
    cm::pump_messages(ctx, PumpKind::Blocking, where);
    const int32_t button = gs.g_rootInput[4];
    if (!left_only) {
      if (button != 0) break;
      if (cm::flush_redraw(ctx) != 0) break;
    } else {
      const bool end = button == 1 || (SG_NEGCTL(g_negctl_end_of_year) == 3 && button != 0);
      if (cm::flush_redraw(ctx) != 0) break;
      if (end) break;
    }
  }
}

constexpr uint32_t kHoleSize = 0x208;

// showCourseReport(mode): mode 0 the per-hole table, 1 / 2 the association's grading page (2 returns without waiting)
int32_t show_course_report(Ctx &ctx, int32_t mode) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const uint32_t holes_va = state_va(ctx, &GameState::g_holes);
  const int mutant = SG_NEGCTL(g_negctl_course_report);

  const int32_t thr = gs.g_difficulty != 0 ? 0x32 : 0x19;
  int32_t S[13] = {0};
  int32_t cnt_a = 0, cnt_b = 0, cnt_c = 0;
  int32_t nholes = 0, many = 0;
  // carried from hole to hole (the original keeps them in frame slots)
  int32_t cnt = 0, flags = 0, best = 0, wsum = 0, time = 0, pace = 0;
  int32_t d1 = 0, d2 = 0, d3 = 0, which = 0;
  int32_t r94[8] = {0}, rb8[8] = {0};

  for (int32_t h = 1; h < 0x13; ++h) {
    const uint32_t hr = holes_va + u32(mul32(static_cast<int32_t>(kHoleSize), h));
    const int32_t par = m.load<int8_t>(hr);
    bool draw_row = false;
    int32_t a = 0, b = 0, c = 0;
    if (par == 0) {
      if (mode != 0) continue;
    } else {
      pace = par;
      for (int r = 0; r < 8; ++r) {
        rb8[r] = shl32(par, 3);
        r94[r] = 8;
      }
      cnt = 0;
      wsum = 0;
      for (int32_t bb = 1; bb <= 7; ++bb)
        for (int32_t base = 0, r = 0; base < 0x58; base += 0xb, ++r)
          for (int32_t j = bb;; ++j) {
            const int32_t lim = bb == 7 ? 9 : bb;
            if (j > lim) break;
            const int32_t v = m.load<int16_t>(hr + 0x28 + 2 * u32(base + j));
            cnt = add32(cnt, v);
            wsum = add32(wsum, mul32(v, j));
            r94[r] = add32(r94[r], v);
            rb8[r] = add32(rb8[r], mul32(v, j));
          }
      ++nholes;
      if (!(add32(add32(m.load<int16_t>(hr + 0x104) / 2, m.load<int16_t>(hr + 0x110)), m.load<int16_t>(hr + 0xee)) < 8)) ++many;
      S[1] = add32(S[1], pace);
      S[0] = add32(S[0], m.load<int16_t>(hr + 4));
      d3 = d2 = d1 = which = flags = 0;
      best = 100;
      S[2] = add32(S[2], cnt == 0 ? 0 : div32(mul32(wsum, 100), cnt));
      const int32_t r20 = m.load<int32_t>(hr + 0x20);
      time = r20 == 0 ? 0 : div32(div32(m.load<int32_t>(hr + 0x1ec), r20), 40);
      S[3] = add32(S[3], time);
      pace = r20 == 0 ? 0
                      : div32(mul32(m.load<int16_t>(hr + 0x158), 100),
                              add32(add32(m.load<int32_t>(hr + 0x24) / 2, r20), 4));
      S[4] = add32(S[4], pace);
      if (r94[7] != 0) {
        const int32_t ref = div32(mul32(rb8[7], 100), r94[7]);
        const bool f40 = (gs.g_flags & 0x40u) != 0;
        if (r94[6] != 0) {
          d1 = sub32(div32(mul32(rb8[6], 100), r94[6]), ref);
          if (f40)
            d1 = add32(d1, sub32(div32(mul32(rb8[0], 100), r94[0]), div32(mul32(rb8[1], 100), r94[1])));
          S[5] = add32(S[5], d1);
          if (!(d1 < thr)) flags = 1;
          if (!(d1 >= 100)) {
            best = d1;
            which = 1;
          }
          if (!(d1 < thr)) ++cnt_a;
        }
        if (r94[5] != 0) {
          d2 = sub32(div32(mul32(rb8[5], 100), r94[5]), ref);
          if (f40)
            d2 = add32(d2, sub32(div32(mul32(rb8[0], 100), r94[0]), div32(mul32(rb8[2], 100), r94[2])));
          S[6] = add32(S[6], d2);
          if (!(d2 < thr)) flags |= 2;
          if (!(d2 >= best)) {
            best = d2;
            which = 2;
          }
          if (!(d2 < thr)) ++cnt_b;
        }
        if (r94[3] != 0) {
          d3 = sub32(div32(mul32(rb8[3], 100), r94[3]), ref);
          if (f40)
            d3 = add32(d3, sub32(div32(mul32(rb8[0], 100), r94[0]), div32(mul32(rb8[4], 100), r94[4])));
          S[7] = add32(S[7], d3);
          if (!(d3 < thr)) flags |= 4;
          if (!(d3 >= best)) {
            best = d3;
            which = 4;
          }
          if (!(d3 < thr)) ++cnt_c;
        }
      }
      if (!(m.load<int32_t>(hr + 0x1fc) >= 2)) ++S[8];
      a = m.load<int32_t>(hr + 0x1f4);
      b = m.load<int32_t>(hr + 0x1f0);
      c = m.load<int32_t>(hr + 0x1f8);
      S[9] = add32(S[9], cnt == 0 ? 0 : div32(mul32(a, 100), cnt));
      S[10] = add32(S[10], a);
      S[11] = add32(S[11], sub32(sub32(a, b), c));
      if (mode != 0) continue;
      draw_row = true;
    }
    if (draw_row) {   // one table row: only the numbers the original formats through the shared buffer
      reset_text(ctx);
      call_peer(ctx, "appendHoleName", {u32(h)});
      cents(ctx, cnt == 0 ? 0 : div32(mul32(wsum, 100), cnt));
      const int32_t dv[3] = {d1, d2, d3};
      for (int k = 0; k < 3; ++k) cents(ctx, dv[k]);
      number(ctx, cnt == 0 ? 0 : div32(mul32(a, 100), cnt));
      number(ctx, mul32(a, 100));
      number(ctx, mul32(sub32(sub32(a, b), c), 100));
    }
    if (h % 18 != 0 || nholes == 0) continue;   // the subtotal row after hole 18
    number(ctx, S[0]);
    cents(ctx, S[2]);
    (void)div32(S[4], nholes);   // the average pace, drawn as text
    cents(ctx, div32(S[5], nholes));
    cents(ctx, div32(S[6], nholes));
    cents(ctx, div32(S[7], nholes));
    number(ctx, div32(S[9], nholes));
    number(ctx, mul32(S[10], 100));
    number(ctx, mul32(S[11], 100));
  }

  if (mode == 0) {
    // the table's foot is drawing; the button belongs to the modal loop below
  } else {
    int32_t score = 0;
    int32_t crit;
    const int32_t hp1 = gs.g_holesPlusOne;
    const int32_t bucket = i32(call_peer(ctx, "bucketValue", {u32(sub32(hp1, 1))}).eax);
    if (!(static_cast<uint32_t>(bucket) > 3)) {
      switch (bucket) {
        case 0: crit = 5; flags = 0; cnt = 5; best = 5; break;
        case 1: crit = 9; flags = 1; cnt = 9; best = 9; break;
        case 2: crit = 0x11; best = 0x11; cnt = sub32(hp1, 1); flags = 2; break;
        default: crit = 0x12; flags = 3; cnt = 0x12; best = 0x12; break;
      }
    } else {
      crit = best;
    }
    const int32_t ideal = mul32(div32(mul32(crit, 100), 18), add32(mul32(flags, 5), 0x39));
    number(ctx, ideal);
    if (crit == 0x12) number(ctx, sub32(ideal, 1000));
    else number(ctx, sub32(ideal, div32(mul32(ideal, 10), add32(mul32(flags, 5), 0x14))));
    const int32_t step = sub32(4, flags);
    (void)div32(9, step);   // the criteria text divides 9 by it
    const int32_t half = cnt / 2;
    if (nholes != 0) {
      const int32_t len = S[0];
      number(ctx, len);
      int32_t g = best != 0x12 ? add32(div32(mul32(mul32(sub32(len, ideal), add32(flags, 4)), 5), ideal), 10)
                               : sub32(10, div32(sub32(ideal, len), 100));
      g = clamp(g, 0, 10);
      score = g;
      if (g == 0) score = -999;
      int32_t g2 = sub32(10, mul32(abs32(add32(sub32(best, hp1), 1)), step));
      if (g2 < 0) g2 = 0;
      int32_t sc = add32(score, g2);
      if (g2 == 0) sc = -999;
      const int32_t tm = S[3];
      int32_t g3 = sub32(10, div32(sub32(tm, 0xeb), 6));
      if (g3 == 0 && !(tm > 0x12c)) g3 = 1;
      g3 = clamp(g3, 0, 10);
      sc = add32(sc, g3);
      if (g3 == 0) sc = -999;
      const int32_t pc = div32(S[4], nholes);
      const int32_t g4 = clamp(add32(div32(sub32(pc, 0x6d), 10), 10), 0, 10);
      score = add32(sc, g4);
      if (g4 == 0) score = -999;
      const int32_t counts[5] = {S[8], many, cnt_a, cnt_b, cnt_c};
      for (int k = 0; k < 5; ++k) {
        const int32_t gc = clamp(add32(sub32(counts[k], cnt), 10), 0, 10);
        score = add32(score, gc);
        if (gc == 0) score = -999;
      }
      int32_t fac = 0;
      for (int bit = mutant == 4 ? 7 : 6; bit < 0x14; ++bit)
        if ((gs.g_buildingKindMask & (1u << bit)) != 0) ++fac;
      const int32_t gf = clamp(add32(mul32(sub32(fac, half), 2), 8), 0, 10);
      score = add32(score, gf);
      if (gf == 0 && mutant != 5) {
        score = -999;
      } else if (!(score < 0x5a)) {   // the tour events at the holes the report covers
        auto at = [&](int32_t hole, uint32_t off) { return m.load<int32_t>(holes_va + u32(mul32(static_cast<int32_t>(kHoleSize), hole)) + off); };
        if (hp1 > 9) call_peer(ctx, "postEvent", {0xc, u32(add32(shl32(at(9, 8), 10), 0x200)), u32(add32(shl32(at(9, 0xc), 10), 0x200))});
        if (hp1 > 0x12) call_peer(ctx, "postEvent", {0x10, u32(add32(shl32(at(18, 8), 10), 0x200)), u32(add32(shl32(at(18, 0xc), 10), 0x200))});
        if (score == 100 && hp1 > 0x12)
          call_peer(ctx, "postEvent", {mutant == 3 ? 0x14u : 0x15u, u32(add32(shl32(at(18, 8), 10), 0x200)), u32(add32(shl32(at(18, 0xc), 10), 0x200))});
      }
    }
    if (!(score > 0)) {
      gs.g_flags &= ~(1u << 22);
    } else {
      gs.g_courseReportScore = score;
      (void)div32(sub32(score, 0x28), 5);   // the grade word's index
      const int32_t prize = mutant == 1
                                ? mul32(mul32(mul32(add32(div32(sub32(score, 0x32), 5), 4), add32(flags, 1)), hp1), 2)
                                : mul32(mul32(mul32(add32(div32(sub32(score, 0x32), 5), 5), add32(flags, 1)), hp1), 2);
      gs.g_tournamentPurse = prize;
      number(ctx, prize);
      gs.g_flags |= mutant == 2 ? (1u << 22) : (1u << 22) | (1u << 13);
    }
  }
  if (mode == 2) return 0;
  report_loop(ctx, false, "showCourseReport");
  gs.g_ui_569498 = mutant == 6 ? 1 : 0;
  cm::pump_light(ctx, "showCourseReport");
  return 0;
}

// showEndOfYearScreen: the year's statistics rows (numbers formatted through the shared buffer), the membership history
// slot of this month, the month's highlight lines (two kinds call another stage), the board's debt warning counter, the
// one-button loop and the score summary
int32_t show_end_of_year_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mutant = SG_NEGCTL(g_negctl_end_of_year);
  cm::call_optional(ctx, "playSound", {0x7f, 100, 0, 0, 0});

  const int32_t month = gs.g_date / 1024;
  const int32_t this_idx = mod32(month, 500);
  const int32_t last_idx = mod32(sub32(month, 8), 500);
  auto hist = [&](uint32_t base, int32_t idx) { return static_cast<int32_t>(m.load<int16_t>(base + 2 * u32(idx))); };
  const uint32_t cash = state_va(ctx, &GameState::g_cashHistory);
  const uint32_t skill = state_va(ctx, &GameState::g_skillHistory);

  number(ctx, mul32(hist(cash, last_idx), 100));      // cash row: last year, this year
  number(ctx, mul32(hist(cash, this_idx), 100));
  cents(ctx, hist(skill, last_idx));                  // skill row
  cents(ctx, hist(skill, this_idx));
  // the membership row first copies the live count into this month's slot
  const int32_t members = static_cast<int16_t>(gs.g_eoy58b388 & 0xffffu);
  m.store<int16_t>(state_va(ctx, &GameState::g_memberTypeHistory) + 2 * u32(this_idx),
                   static_cast<int16_t>(mutant == 1 ? members + 1 : members));

  const uint32_t events = state_va(ctx, &GameState::g_eventHistory);
  for (int32_t k = last_idx; k <= this_idx; ++k) {
    const int32_t ev = static_cast<int16_t>(m.load<uint16_t>(events + 2 * u32(k)));
    if (ev == 0) continue;
    const int32_t kind = (ev & ~0xff) | (ev & 0xe0);
    const int32_t n = ev & 0x1f;
    if (kind == 0x120) call_peer(ctx, "loadStoryStrings", {u32(mutant == 4 ? n + 1 : n), u32(-1), 0, 0});
    else if (kind == 0x160) call_peer(ctx, "landmarkName", {u32(n), 0});
  }

  // the board's debt warnings: the counter picks the wording and is raised while the money is negative
  if (gs.g_cash_div100 < 0 && (gs.g_flags & (1u << 24)) == 0) gs.g_debtYears = add32(gs.g_debtYears, 1);
  else if (mutant != 2) gs.g_debtYears = 0;

  report_loop(ctx, true, "showEndOfYearScreen");
  cm::pump_light(ctx, "showEndOfYearScreen");
  if (mutant != 5 && i32(call_peer(ctx, "addScore", {}).eax) != -1) call_peer(ctx, "drawScoreSummary", {});
  return 0;
}

#define V0(fn) [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(fn(c)); }
const FuncDef kFuncs[] = {
    {"showCourseReport", "default", "void", 1, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(show_course_report(c, static_cast<int32_t>(a[0]))); },
     &g_negctl_course_report},
    {"showEndOfYearScreen", "default", "void", 0, 0, V0(show_end_of_year_screen), &g_negctl_end_of_year},
};
#undef V0
}  // namespace

extern const ModuleDef kModule_reports = {"reports", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
