"""S2.8: the end-of-year screen's drawing calls in the mirror's port/src/sim/reports.cpp (run from the mirror root)."""
import pathlib
import re

p = pathlib.Path('port/src/sim/reports.cpp')
s = p.read_text()


def rep(old, new, count=1):
    global s
    pat = re.escape(old).replace("\\\n", "\n").replace("\n", "\n(?:[ \t]*\n)*")
    found = re.findall(pat, s)
    assert len(found) == count, (old[:80], len(found))
    s = re.sub(pat, lambda m: new, s)


rep('''#include "commands_internal.h"
''', '''#include "commands_internal.h"
#include "gfx_reports.h"
#include <string>
''')

rep('''int32_t show_end_of_year_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mutant = SG_NEGCTL(g_negctl_end_of_year);
  cm::call_optional(ctx, "playSound", {0x7f, 100, 0, 0, 0});
''', '''//
// Roadmap S2.8: the highlight lines are built in g_textBuffer as the original builds them (the month name, ': ', the event's
// text pieces, with loadStoryStrings / landmarkName at their places and the subject's digits through itoa into g_ui_821c50),
// which the port now does too (the buffers are not simulation objects); with an attached backend the screen is drawn as
// c3er.cpp CareerRetirementFlow_re draws it: its frame Image and six Surfaces (backend keys gfx_local_key(0x5400 ..)) take the
// top, bottom and line strips of interface\\infoscreens\\endoyear.pcx and of its alpha mask, the screen is dimmed, the top strip
// blended on, the title and heads, the four statistics rows (label + verdict, last year's value ending at 0x259, this year's
// ending at 0x1f4 in the verdict's colour), one line strip per highlight line and debt warning (the line strip's height apart
// from the top strip's height + 0x28), the bottom strip, the close button 8 below it; the objects are freed at the end.
int32_t show_end_of_year_screen(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mutant = SG_NEGCTL(g_negctl_end_of_year);
  const gfxr::ScreenDraw d(ctx);
  const uint32_t root = d.root(), font_b = state_va(ctx, &GameState::g_fontRef_821ee8);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4d21a4), buf = d.buf(), num = state_va(ctx, &GameState::g_ui_821c50);
  const uint32_t img = gfx_local_key(0x5400), top = gfx_local_key(0x5500), top_a = gfx_local_key(0x5600),
                 bottom = gfx_local_key(0x5700), bottom_a = gfx_local_key(0x5800), line = gfx_local_key(0x5900),
                 line_a = gfx_local_key(0x5a00);
  if (d.on) {
    d.gx("reports.begin", {});
    d.gx("Image_ctor", {img});
    for (uint32_t o : {top, top_a, bottom, bottom_a, line, line_a}) d.gx("Surface_ctor", {o});
    d.gx("Surface_createFull", {img, 0x320, 0x258, 0, 1, 0, 0});
    d.gx("loadImage", {img, pool + 0x898, 0, 0, 0x100, 1});
    d.gx("Surface_blitTakeDirty", {top, img, 0xbb, 0x28, 0x1ad, 0xb0, 1, 0});
    d.gx("Surface_blitTakeDirty", {bottom, img, 0xbb, 0x176, 0x1ad, 0x37, 1, 0});
    d.gx("Surface_blitTakeDirty", {line, img, 0xbb, 0x123, 0x1ad, 0xf, 1, 0});
    d.gx("loadImage", {img, pool + 0x86c, 0, 0, 0x100, 1});
    d.gx("Surface_blitTakeDirty", {top_a, img, 0xbb, 0x28, 0x1ad, 0xb0, 1, 0});
    d.gx("Surface_blitTakeDirty", {bottom_a, img, 0xbb, 0x176, 0x1ad, 0x37, 1, 0});
    d.gx("Surface_blitTakeDirty", {line_a, img, 0xbb, 0x123, 0x1ad, 0xf, 1, 0});
    d.gx("Image_releaseAll", {img});
  }
  cm::call_optional(ctx, "playSound", {0x7f, 100, 0, 0, 0});
  const uint32_t button_quad = m.load<uint32_t>(state_va(ctx, &GameState::g_rootWindow) + 0x2e0);   // the root surface's +0x6c
  d.gx("Surface_drawSprite", {state_va(ctx, &GameState::g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
  d.gx("Surface_stretchBlit", {top, top_a, root, 0xbb, 0x28, 0});
  d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
  d.text(
      [&] {
        cm::cpy(ctx, buf, pool + 0x85c);
        d.append(std::to_string(add32(static_cast<int16_t>(gs.g_year), 0x7d0)));
      },
      [&](uint32_t t) { d.gx("drawStringAt", {root, state_va(ctx, &GameState::g_fontRef_821020), t, 0x196, 0x37, d.len(t)}); });
  d.gx("drawStringAt", {root, font_b, pool + 0x850, 0x1d1, 0x5d, d.len(pool + 0x850)});
  d.gx("drawStringAt", {root, font_b, pool + 0x844, 0x236, 0x5d, d.len(pool + 0x844)});
  d.gx("drawStringAt", {root, font_b, pool + 0x838, 0x194, 0xc8, d.len(pool + 0x838)});
''')

rep('''  number(ctx, mul32(hist(cash, last_idx), 100));      // cash row: last year, this year
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
  }''', '''  // one statistics row's drawing (the numbers formatted again inside ScreenDraw::text, which gives the buffers back): kind 0
  // appendNumber(v * 100), 1 itoa, 2 appendCents
  auto stat_row = [&](uint32_t base, int32_t y, uint32_t label, int kind) {
    if (!d.on) return;
    const int32_t last = hist(base, last_idx), now = hist(base, this_idx);
    auto value = [&](int32_t v) {
      d.put("");
      if (kind == 0) call_peer(ctx, "appendNumber", {u32(mul32(v, 100))});
      else if (kind == 1) d.append(std::to_string(v));
      else cm::append_cents(ctx, v);
    };
    d.text(
        [&] {
          cm::cpy(ctx, buf, label);
          cm::cat(ctx, buf, pool + (last < now ? 0x818u : last == now ? 0x80cu : 0x800u));
        },
        [&](uint32_t t) { d.gx("drawStringAt", {root, font_b, t, 0x12d, u32(y), d.len(t)}); });
    d.text([&] { value(last); }, [&](uint32_t t) { d.gx("drawStringAt2", {root, font_b, t, 0x259, u32(y), d.len(t)}); });
    d.text([&] { value(now); },
           [&](uint32_t t) {
             d.gx("Widget_setQuad",
                  {root, last < now ? 0x80001284u : last != now ? 0x80007d08u : 0x80000000u, 0xffffffffu, 2, 2});
             d.gx("drawStringAt2", {root, font_b, t, 0x1f4, u32(y), d.len(t)});
             d.gx("Widget_setQuad", {root, 0x80000000u, 0xffffffffu, 2, 2});
           });
  };
  number(ctx, mul32(hist(cash, last_idx), 100));      // cash row: last year, this year
  number(ctx, mul32(hist(cash, this_idx), 100));
  stat_row(cash, 0x73, pool + 0x824, 0);
  stat_row(state_va(ctx, &GameState::g_funHistory), 0x87, pool + 0x7e8, 1);
  cents(ctx, hist(skill, last_idx));                  // skill row
  cents(ctx, hist(skill, this_idx));
  stat_row(skill, 0x9b, pool + 0x7d0, 2);
  // the membership row first copies the live count into this month's slot
  const int32_t members = static_cast<int16_t>(gs.g_eoy58b388 & 0xffffu);
  m.store<int16_t>(state_va(ctx, &GameState::g_memberTypeHistory) + 2 * u32(this_idx),
                   static_cast<int16_t>(mutant == 1 ? members + 1 : members));
  stat_row(state_va(ctx, &GameState::g_memberTypeHistory), 0xaf, pool + 0x7b8, 1);

  // the highlight lines, built in the text buffer as the original builds them (see above)
  int32_t y = add32(d.cut_h(top), 0x28);
  const int32_t step = d.cut_h(line);
  auto draw_line = [&]() {
    d.gx("Surface_stretchBlit", {line, line_a, root, 0xbb, u32(y), 0});
    d.gx("drawStringAt", {root, font_b, buf, 0x194, u32(add32(y, 2)), d.len(buf)});
    y = add32(y, step);
  };
  auto cat_num = [&](int32_t v) {   // strcat(buf, itoa(v, g_ui_821c50, 10))
    const std::string t = std::to_string(v);
    for (size_t i = 0; i <= t.size(); ++i) m.store<uint8_t>(num + static_cast<uint32_t>(i), static_cast<uint8_t>(i < t.size() ? t[i] : 0));
    cm::cat(ctx, buf, num);
  };
  const uint32_t events = state_va(ctx, &GameState::g_eventHistory);
  for (int32_t k = last_idx; k <= this_idx; ++k) {
    const int32_t ev = static_cast<int16_t>(m.load<uint16_t>(events + 2 * u32(k)));
    if (ev == 0) continue;
    cm::cpy(ctx, buf, m.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c2908) + 4 * u32(clamp(sub32(k, last_idx), 0, 7))));
    cm::cat(ctx, buf, pool + 0x7b4);
    const int32_t kind = (ev & ~0xff) | (ev & 0xe0);
    const int32_t n = ev & 0x1f;
    uint32_t tail = 0;
    if (kind > 0xc0) {
      if (kind == 0xe0) {
        cm::cat(ctx, buf, state_va(ctx, &GameState::g_golferTypes) + 0x10);
        cm::cat(ctx, buf, pool + 0x754);
        cat_num(n);
        tail = pool + 0x744;
      } else if (kind == 0x100) {
        cm::cat(ctx, buf, m.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c2c10) + 8));
        tail = pool + 0x730;
      } else if (kind == 0x120) {
        cm::cat(ctx, buf, pool + 0x720);
        call_peer(ctx, "loadStoryStrings", {u32(mutant == 4 ? n + 1 : n), u32(-1), 0, 0});
      } else if (kind == 0x140) {
        tail = pool + 0x6f0;
      } else if (kind == 0x160) {
        cm::cat(ctx, buf, pool + 0x70c);
        call_peer(ctx, "landmarkName", {u32(n), 0});
      }
    } else if (kind == 0xc0) {
      cm::cat(ctx, buf, state_va(ctx, &GameState::g_celebrities) + 0x25 * u32(n));
      tail = pool + 0x760;
    } else if (kind == 0x20) {
      cm::cat(ctx, buf, state_va(ctx, &GameState::f_strpool_4c4970) + 8);
      cat_num(n);
      cm::cat(ctx, buf, pool + 0x7a8);
      if (m.load<int8_t>(state_va(ctx, &GameState::g_holes) + kHoleSize * u32(n)) == 0) {   // a hole without a par drops the line
        m.store<uint8_t>(buf, 0);
        continue;
      }
    } else if (kind == 0x40) {
      cm::cat(ctx, buf, state_va(ctx, &GameState::f_tab_buildItems_4c26b0) + 0x14 * u32(n));
      tail = pool + 0x7a0;
    } else if (kind == 0x60) {
      cm::cat(ctx, buf, pool + 0x770);
      tail = state_va(ctx, &GameState::g_golfPros) + 0x38 * u32(n);
    } else if (kind == 0x80 || kind == 0xa0) {
      cm::cat(ctx, buf, state_va(ctx, &GameState::f_strpool_4c4970) + 8);
      cat_num(n);
      tail = pool + (kind == 0x80 ? 0x790u : 0x780u);
    }
    if (tail) cm::cat(ctx, buf, tail);
    if (m.load<uint8_t>(buf) == 0) continue;
    draw_line();
  }''')

rep('''  if (gs.g_cash_div100 < 0 && (gs.g_flags & (1u << 24)) == 0) gs.g_debtYears = add32(gs.g_debtYears, 1);
  else if (mutant != 2) gs.g_debtYears = 0;

  report_loop(ctx, true, "showEndOfYearScreen");
  cm::pump_light(ctx, "showEndOfYearScreen");
  if (mutant != 5 && i32(call_peer(ctx, "addScore", {}).eax) != -1) call_peer(ctx, "drawScoreSummary", {});
  return 0;
}''', '''  if (gs.g_cash_div100 < 0 && (gs.g_flags & (1u << 24)) == 0) {
    const int32_t warn = gs.g_debtYears;
    gs.g_debtYears = add32(gs.g_debtYears, 1);
    d.gx("Widget_setQuad", {root, 0x80007d08u, 0xffffffffu, 2, 2});
    static const uint32_t kWarn[3][2] = {{0x5e4, 0x5b4}, {0x64c, 0x620}, {0x6b8, 0x680}};   // f_strpool_4d21a4 offsets
    if (d.on && warn >= 0 && warn <= 2)
      for (int k = 0; k < 2; ++k)
        d.text([&] { cm::cpy(ctx, buf, pool + kWarn[warn][k]); }, [&](uint32_t) { draw_line(); });
  } else if (mutant != 2) {
    gs.g_debtYears = 0;
  }
  d.gx("Surface_stretchBlit", {bottom, bottom_a, root, 0xbb, u32(y), 0});
  d.gx("Widget_setQuad", {root, button_quad, 0xffffffffu, 2, 2});
  y = add32(y, 8);
  d.gx("Image_draw", {state_va(ctx, &GameState::g_spriteRef_820f40), root, 0x226, u32(y), 0});
  d.gx("Window::refreshFull", {d.window(), 0});

  report_loop(ctx, true, "showEndOfYearScreen");
  cm::pump_light(ctx, "showEndOfYearScreen");
  if (mutant != 5 && i32(call_peer(ctx, "addScore", {}).eax) != -1) call_peer(ctx, "drawScoreSummary", {});
  for (uint32_t o : {top, bottom, line, top_a, bottom_a, line_a, line_a, line, bottom_a, bottom, top_a, top}) d.gx("Surface_free", {o});
  d.gx("Image_dtor", {img});
  return 0;
}''')
p.write_text(s)
print("patched")
