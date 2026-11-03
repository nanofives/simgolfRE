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
#include "portsim/execonst.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/util.h"
#include "commands_internal.h"
#include "gfx_reports.h"
#include "portsim/sim/gfx.h"

#include <cstring>
#include <string>
#include <vector>

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   showCourseReport     1 = the purse uses (score - 50) / 5 + 4, 2 = a passing report sets only flag bit 22,
//                        3 = the full-marks event is 0x14, 4 = the facility count starts at bit 7, 5 = a zero-star facility
//                        criterion does not zero the report, 6 = the leave clears the mode byte to 1
//   showEndOfYearScreen  1 = the member history stores the live count + 1, 2 = the debt counter is not cleared outside
//                        debt, 3 = a right click ends the screen too, 4 = the story-text event 0x120 passes the subject + 1
//                        to loadStoryStrings, 5 = addScore is not asked after the loop, 6 = the loop's View_toLocal cursor
//                        read is left out (roadmap S2.13, U-1254)
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
//
// Roadmap S2.13 (U-1254): every pass also makes the original's View_toLocal(root window, &x, &y) cursor read, before the
// pump in the course report (c3fr.cpp ShowCourseReport_re: waitTicks, View_toLocal, the button hover, pumpMessages) and
// after it on the end-of-year screen (c3er.cpp CareerRetirementFlow_re: waitTicks, pumpMessages, View_toLocal,
// refreshFull, the button hover). Like the frame step's View_toLocal (tick_render.inc viewToLocal) the read is made when
// the display exists (g_graphicsDevice != 0), so a trace-driven source's cursor count stays aligned with the original's:
// without it the end-of-year screen left its CURSOR records unread and the frame's next read (P6's View_toLocal after the
// screen) got the cursor from before it (cov_r_s2_screens record 771). The value only selects the button's hover image
// (`hover`, drawn with a backend attached). Negative control: showEndOfYearScreen mode 6 leaves the read out.
struct Hover {
  int32_t x = 0, y = 0;   // the button's top left; the button is 0x2c x 0x2c
  bool lit = false;
};
bool backend_on() { return g_gfx_attached.load(std::memory_order_relaxed) != 0; }
void gx2(Ctx &ctx, const char *name, std::initializer_list<uint32_t> a) {
  gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
}
// View_toLocal(root window, &x, &y): the display's cursor when there is a display; false without one (x, y stay 0)
bool cursor_read(Ctx &ctx, int32_t &px, int32_t &py) {
  px = py = 0;
  if (ctx.gs().g_graphicsDevice == 0) return false;
  if (InputSource *src = input_source(ctx)) src->cursor(px, py);
  return true;
}
// the button's hover: the lit image while the point is inside, the plain one when it leaves, each followed by a refresh
void hover(Ctx &ctx, Hover *h, int32_t px, int32_t py) {
  if (!h || !backend_on()) return;
  const bool in = px >= h->x && px < add32(h->x, 0x2c) && py >= h->y && py < add32(h->y, 0x2c);
  if (in == h->lit) return;
  h->lit = in;
  const uint32_t win = state_va(ctx, &GameState::g_rootWindow);
  // the plain image g_spriteRef_820f40[0], the lit one [1] (+0x2c)
  gx2(ctx, "Image_draw", {state_va(ctx, &GameState::g_spriteRef_820f40) + (in ? 0x2cu : 0u), win + 0x274, u32(h->x), u32(h->y), 0});
  gx2(ctx, "Window::refreshFull", {win, 0});
}
void report_loop(Ctx &ctx, bool left_only, const char *where, Hover *h = nullptr) {
  GameState &gs = ctx.gs();
  int32_t px = 0, py = 0;
  for (;;) {
    if (!left_only && cursor_read(ctx, px, py)) hover(ctx, h, px, py);
    cm::pump_messages(ctx, PumpKind::Blocking, where);
    if (left_only && SG_NEGCTL(g_negctl_end_of_year) != 6) {
      const bool made = cursor_read(ctx, px, py);
      if (backend_on()) gx2(ctx, "Window::refreshFull", {state_va(ctx, &GameState::g_rootWindow), 0});
      if (made) hover(ctx, h, px, py);
    }
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

// ------------------------------------------------------------------------------------------------ the grading page's drawing
// Roadmap S2.9: the association's page (modes 1 / 2) as drawing calls for the graphics backend (gfx_drop; nothing without
// one attached), in the order of c3fr.cpp ShowCourseReport_re (instruction addresses there). The values are the ones the
// computation above produced (Page); each drawn text is built in g_textBuffer as the original builds it (string-pool
// pieces, _itoa base 10, appendNumber), drawn, and g_textBuffer and the itoa buffer g_textBuf are put back, so a run with
// a backend leaves the GameState as one without. Strings are offsets into f_strpool_4d2c28 unless named otherwise.
struct Page {
  int32_t flags = 0, crit = 0, ideal = 0, step = 1, cnt = 0, half = 0, holes = 0;
  int32_t len = 0, g = 0, hp1 = 0, g2 = 0, tm = 0, g3 = 0, pc = 0, g4 = 0, counts[5] = {0}, gc[5] = {0}, fac = 0, gf = 0;
  int32_t score = 0, prize = 0;
};
struct ReportDraw {
  Ctx &ctx;
  const Memory &m;
  uint32_t root, win, tb, itoa_buf, pool, f_ee8, f_020, f_f28;
  explicit ReportDraw(Ctx &c)
      : ctx(c), m(c.mem), root(state_va(c, &GameState::g_rootWindow) + 0x274), win(state_va(c, &GameState::g_rootWindow)),
        tb(state_va(c, &GameState::g_textBuffer)), itoa_buf(state_va(c, &GameState::g_textBuf)),
        pool(state_va(c, &GameState::f_strpool_4d2c28)), f_ee8(state_va(c, &GameState::g_fontRef_821ee8)),
        f_020(state_va(c, &GameState::g_fontRef_821020)), f_f28(state_va(c, &GameState::g_fontRef_821f28)) {}
  static bool on() { return g_gfx_attached.load(std::memory_order_relaxed) != 0; }
  void gx(const char *name, std::initializer_list<uint32_t> a) const { gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size())); }
  void quad(uint32_t c) const { gx("Widget_setQuad", {root, c, 0xffffffffu, 2, 2}); }
  // Widget_draw / drawStringAt / drawStringAt2 (target, font, text, x, y, strlen(text))
  void str(const char *kind, uint32_t font, uint32_t s, int32_t x, int32_t y) const {
    gx(kind, {root, font, s, u32(x), u32(y), vstrlen(m, s)});
  }
  void img(uint32_t s, int32_t x, int32_t y) const { gx("Image_draw", {s, root, u32(x), u32(y), 0}); }
  // a text built in the buffer by `build`, drawn with `kind`, then the two buffers put back
  template <class F>
  void text(F build, const char *kind, uint32_t font, int32_t x, int32_t y) const {
    std::vector<uint8_t> keep(0x1000), keep_itoa(0x10);
    std::memcpy(keep.data(), m.at(tb, 0x1000), 0x1000);
    std::memcpy(keep_itoa.data(), m.at(itoa_buf, 0x10), 0x10);
    m.store<uint8_t>(tb, 0);
    build();
    str(kind, font, tb, x, y);
    std::memcpy(m.at(tb, 0x1000, true), keep.data(), 0x1000);
    std::memcpy(m.at(itoa_buf, 0x10, true), keep_itoa.data(), 0x10);
  }
  void cat(uint32_t s) const { vstrcat(m, tb, s); }
  void cat_itoa(int32_t v) const {   // strcat(text, _itoa(v, g_textBuf, 10))
    const std::string d = std::to_string(v);
    std::memcpy(m.at(itoa_buf, static_cast<uint32_t>(d.size() + 1), true), d.c_str(), d.size() + 1);
    cat(itoa_buf);
  }
  void number(int32_t v) const { call_peer(ctx, "appendNumber", {u32(v)}); }
  void stars(int32_t n, int32_t y) const {   // n stars (g_surfaces59b050 cell 17) from x 0x12f, 0xe apart
    for (int32_t k = 0, x = 0x12f; k < n; ++k, x += 0xe) img(state_va(ctx, &GameState::g_surfaces59b050) + 0x2c * 17, x, y);
  }
  void unacceptable(int32_t y) const {   // the red `not acceptable` mark
    quad(0x80007d08u);
    str("drawStringAt", f_ee8, pool + 0x3ac, 0x182, y);
    quad(0x80000000u);
  }
  // the page head: the display scale, the 1/4 black sprite over the screen, the association's background over its mask
  void head() const {
    gx("Display::setScale", {1, 1, 1});
    gx("Surface_drawSprite", {state_va(ctx, &GameState::g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
    quad(0x80000000u);
    const uint32_t sga = state_va(ctx, &GameState::g_spriteRef_821c60);
    gx("Surface_stretchBlit", {sga, sga + 0x2c, root, 0x23, 0x1a, 0});
    quad(0x80000000u);
  }
  // P5A (2026-11-03, Mariano: `Course Report does nothing`): mode 0's per-hole table (F1, the Information menu's Course
  // Report) as c3fr.cpp ShowCourseReport_re draws it (showCourseReport+0x40..+0x35f8 there). The art is g_spriteRef_821308's cells:
  // [0] the head, [2] a row band, [4] the subtotal band, [6] the foot, [8 + k] the hole-kind / green marks, [0x19 + k] .. the
  // column icons; the icon x positions are f_tab_layoutInts_4ba62c's dwords 3..11 in the user's exe (.rdata).
  uint32_t art(uint32_t k) const { return state_va(ctx, &GameState::g_spriteRef_821308) + 0x2cu * k; }
  int32_t cut_h(uint32_t sref) const {
    int32_t w = 0, h = 0;
    gfx_sprite_cut(ctx, sref, w, h);
    return h;
  }
  int32_t lay(uint32_t i) const { return ctx.exe().get<int32_t>("f_tab_layoutInts_4ba62c", i); }
  void table_head() const {
    gx("Display::setScale", {1, 1, 1});
    gx("Surface_drawSprite", {state_va(ctx, &GameState::g_spriteRef_821040), root, 0, 0, 0x3e800000u, 0, 1});
    quad(0x80000000u);
    gx("Surface_stretchBlit", {art(0), art(1), root, 0, 0, 0});
    quad(0x80000000u);
  }
  // the icon of a percentage column: v < 0 -> 0, 0 <= v < 50 -> none, 50 <= v < 100 -> 2, v >= 100 -> 1
  static int pct_icon(int32_t v) { return v < 0x32 ? (v < 0 ? 0 : -1) : (v < 100 ? 2 : 1); }
  struct Row {
    int32_t h, par, len, cnt, wsum, time, pace, d[3], best, which, flags, a, b, c;
    uint32_t hflags;   // the hole record's +0x200 dword
    int32_t big;       // +0x104 / 2 + +0x110 + +0xee
    int32_t w1fc;      // +0x1fc
  };
  // one table row at y; returns the next row's y
  int32_t table_row(const Row &r, int32_t y) const {
    const uint32_t f = f_ee8, s = state_va(ctx, &GameState::f_strpool_4d3140), pct = state_va(ctx, &GameState::f_strpool_4c60c4);
    gx("Surface_stretchBlit", {art(2), art(3), root, 0, u32(y), 0});
    const int32_t ty = y + 2;
    quad(0x80000000u);
    const int fi = (r.hflags & 1) == 0 ? 0 : ((r.hflags & 2) ? 2 : 1);
    const int hi = (r.big < 8 ? 0 : 3) + fi + 2;
    text([&] { call_peer(ctx, "appendHoleName", {u32(r.h)}); if (hi > 2) img(art(8 + u32(hi)), 0x5a, y); }, "Widget_draw", f, 0xf,
         ty);
    text([&] { cat_itoa(r.len); }, "drawStringAt2", f, 0x97, ty);
    text([&] { cat_itoa(r.par); }, "drawStringAt", f, 0xac, ty);
    text([&] {
      cm::append_cents(ctx, r.cnt == 0 ? 0 : div32(mul32(r.wsum, 100), r.cnt));
      if ((r.hflags & 0xc) != 0) img(art(0x19), lay(3), y);
    }, "drawStringAt2", f, 0xe3, ty);
    text([&] {
      cat_itoa(r.time);
      cat(s);
      const int ti = !(r.time < mul32(r.par, 5)) ? 0 : (r.time > mul32(r.par, 3) ? -1 : 1);
      if (ti >= 0) img(art(0x19 + u32(ti)), lay(3), y);
    }, "drawStringAt2", f, 0x116, ty);
    text([&] {
      cat_itoa(r.pace);
      cat(pct);
      const int pi = !(r.pace >= 10) ? 0 : (r.pace < 0x32 ? -1 : 1);
      if (pi >= 0) img(art(0x1c + u32(pi)), lay(4), y);
    }, "drawStringAt2", f, 0x144, ty);
    static const int32_t kDx[3] = {0x171, 0x19f, 0x1ce};
    for (uint32_t c = 0; c < 3; ++c) {
      text([&] {
        cm::append_cents(ctx, r.d[c]);
        const int ic = pct_icon(r.d[c]);
        if (ic >= 0) img(art(0x1f + 3 * c + u32(ic)), lay(5 + c), y);
      }, "drawStringAt2", f, kDx[c], ty);
    }
    const int32_t flags = r.best < 100 ? (r.flags & ~r.which) : r.flags;
    text([&] {
      vstrcpy(m, tb, m.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c2a18) + 0x10 + 4 * u32(flags)));
      const int li = !(r.w1fc >= 2) ? 1 : (r.w1fc <= 2 ? -1 : 0);
      if (li >= 0) img(art(0x28 + u32(li)), lay(8), y);
    }, "Widget_draw", f, 0x1d7, ty);
    {
      const uint32_t gi = (r.hflags & 0x60) == 0 ? 1u : ((r.hflags >> 4) & 2u);
      img(art(8 + gi), 0x22b, ty - 1);
    }
    text([&] { number(r.cnt == 0 ? 0 : div32(mul32(r.a, 100), r.cnt)); }, "drawStringAt2", f, 0x27b, ty);
    text([&] { number(mul32(r.a, 100)); }, "drawStringAt2", f, 0x2c5, ty);
    text([&] {
      const int32_t net = sub32(sub32(r.a, r.b), r.c);
      number(mul32(net, 100));
      if (net < 0) img(art(0x31), lay(11), y);
    }, "drawStringAt2", f, 0x312, ty);
    return ty + 0xf;
  }
  // the subtotal row after hole 18 at y (S the running sums, time the last hole's time); returns the next y
  int32_t table_subtotal(const int32_t *S, int32_t holes, int32_t time, int32_t y) const {
    const uint32_t f = f_ee8, s = state_va(ctx, &GameState::f_strpool_4d3140), pct = state_va(ctx, &GameState::f_strpool_4c60c4);
    const uint32_t dollar = state_va(ctx, &GameState::f_strpool_4c61dc) + 4;
    gx("Surface_stretchBlit", {art(4), art(5), root, 0, u32(y - 2), 0});
    const int32_t ty = y + 9;
    text([&] { vstrcpy(m, tb, pool + 0x504); }, "Widget_draw", f, 0xf, ty);   // h is 18 here (the h == 9 text never shows)
    text([&] { number(S[0]); }, "drawStringAt2", f, 0x97, ty);
    text([&] { cat_itoa(S[1]); }, "drawStringAt", f, 0xac, ty);
    text([&] { cm::append_cents(ctx, S[2]); }, "drawStringAt2", f, 0xe3, ty);
    text([&] {
      if (!(S[3] < 0x3c)) {
        cat_itoa(S[3] / 60);
        cat(pool + 0x500);
      }
      cat_itoa(time % 60);   // the last hole's time, as the original
      cat(s);
    }, "drawStringAt2", f, 0x116, ty);
    text([&] { cat_itoa(div32(S[4], holes)); cat(pct); }, "drawStringAt2", f, 0x144, ty);
    static const int32_t kDx[3] = {0x171, 0x19f, 0x1ce};
    for (uint32_t c = 0; c < 3; ++c) {
      const int32_t v = div32(S[5 + c], holes);
      text([&] {
        cm::append_cents(ctx, v);
        const int ic = pct_icon(v);
        // the third column's index is a colour word with bit 31 set: its `jle` skips the icon on every value
        if (c < 2 && ic > 0) img(art(0x1f + 3 * c + u32(ic)), lay(5 + c), ty - 2);
      }, "drawStringAt2", f, kDx[c], ty);
    }
    text([&] { vstrcpy(m, tb, dollar); number(div32(S[9], holes)); }, "drawStringAt2", f, 0x27b, ty);
    text([&] { vstrcpy(m, tb, dollar); number(mul32(S[10], 100)); }, "drawStringAt2", f, 0x2c5, ty);
    text([&] {
      vstrcpy(m, tb, dollar);
      number(mul32(S[11], 100));
      if (S[11] < 0) img(art(0x31), lay(11), ty - 2);
    }, "drawStringAt2", f, 0x312, ty);
    return cut_h(art(4)) + ty - 0xb;
  }
  // the foot at y; returns the OK button's y (its x is 0x2de)
  int32_t table_foot(int32_t y, int32_t holes) const {
    const int32_t fy = holes != 0 ? y : y - 2;
    gx("Surface_stretchBlit", {art(6), art(7), root, 0, u32(fy), 0});
    const int32_t ly = fy + cut_h(art(6)) - 0x3f;
    str("drawStringAt", f_020, pool + 0x224, 0x13c, 0x23);
    str("drawStringAt", f_ee8, pool + 0x214, 0xb2, ly);
    str("drawStringAt", f_ee8, pool + 0x208, 0x14f, ly);
    str("drawStringAt", f_ee8, pool + 0x1fc, 0x1e7, ly);
    const uint32_t titles = state_va(ctx, &GameState::f_ptrtab_4d2054) + 0x24, xs = state_va(ctx, &GameState::g_ui_4d20ac);
    for (uint32_t i = 0; i < 12; ++i) {
      const uint32_t t = m.load<uint32_t>(titles + 4 * i);
      if (t == 0) continue;
      str("drawStringAt", f_ee8, t, m.load<int32_t>(xs + 4 * i), 0x4c);
    }
    return ly + 0xf;
  }
  void page(const Page &p) const {
    const uint32_t cl = state_va(ctx, &GameState::f_strpool_4c59e0);   // ")"
    str("Widget_draw", f_020, pool + 0x4dc, 0xbe, 0x2a);
    text([&] { cat(pool + 0x4c4); cat(m.load<uint32_t>(state_va(ctx, &GameState::f_ptrtab_4c2a18) + 4 * u32(p.flags))); },
         "Widget_draw", f_ee8, 0x4b, 0x4d);
    str("Widget_draw", f_ee8, pool + 0x4bc, 0x132, 0x4d);
    str("Widget_draw", f_ee8, pool + 0x4a8, 0x1f1, 0x4d);
    str("drawStringAt", f_ee8, pool + 0x494, 0x77, 0x68);
    text([&] {
      number(p.ideal);
      cat(pool + 0x48c);
      number(p.crit == 0x12 ? sub32(p.ideal, 1000) : sub32(p.ideal, div32(mul32(p.ideal, 10), add32(mul32(p.flags, 5), 0x14))));
      cat(cl);
    }, "Widget_draw", f_ee8, 0x1ea, 0x68);
    str("drawStringAt", f_ee8, pool + 0x47c, 0x77, 0x79);
    text([&] { cat_itoa(p.crit); cat(pool + 0x478); cat_itoa(sub32(p.crit, div32(9, p.step))); cat(cl); }, "Widget_draw", f_ee8,
         0x1ea, 0x79);
    str("drawStringAt", f_ee8, pool + 0x468, 0x77, 0x8a);
    str("Widget_draw", f_ee8, pool + 0x448, 0x1ea, 0x8a);
    str("drawStringAt", f_ee8, pool + 0x43c, 0x77, 0x9b);
    str("Widget_draw", f_ee8, pool + 0x434, 0x1ea, 0x9b);
    static const uint32_t kCrit[5] = {0x420, 0x410, 0x400, 0x3ec, 0x3d8};
    for (int c = 0; c < 5; ++c) {
      const int32_t ry = 0xac + 0x11 * c;
      str("drawStringAt", f_ee8, pool + kCrit[c], 0x77, ry);
      text([&] { cat_itoa(p.cnt); cat(pool + 0x478); cat_itoa(clamp(sub32(p.cnt, 9), 0, 0x63)); cat(cl); }, "Widget_draw", f_ee8,
           0x1ea, ry);
    }
    str("drawStringAt", f_ee8, pool + 0x3c0, 0x77, 0x101);
    text([&] { cat_itoa(add32(p.half, 1)); cat(pool + 0x478); cat_itoa(clamp(sub32(add32(p.half, 1), 4), 0, 0x63)); cat(cl); },
         "Widget_draw", f_ee8, 0x1ea, 0x101);
    if (p.holes != 0) {
      text([&] { number(p.len); cat(state_va(ctx, &GameState::f_strpool_4c615c) + 0x38); }, "drawStringAt", f_ee8, 0xee, 0x68);
      if (p.g > 0) stars(p.g, 0x68);
      if (p.g == 0) unacceptable(0x68);
      text([&] { cat_itoa(sub32(p.hp1, 1)); }, "drawStringAt", f_ee8, 0xee, 0x79);
      if (p.g2 > 0) stars(p.g2, 0x79);
      if (p.g2 == 0) unacceptable(0x79);
      text([&] {
        if (!(p.tm < 0x3c)) {
          cat_itoa(p.tm / 60);
          cat(pool + 0x3a8);
        }
        cat_itoa(p.tm % 60);
        cat(pool + 0x3a4);
      }, "drawStringAt", f_ee8, 0xee, 0x8a);
      if (p.g3 > 0) stars(p.g3, 0x8a);
      if (p.g3 == 0) unacceptable(0x8a);
      text([&] { cat_itoa(p.pc); cat(state_va(ctx, &GameState::f_strpool_4c60c4)); }, "drawStringAt", f_ee8, 0xee, 0x9b);
      if (p.g4 > 0) stars(p.g4, 0x9b);
      if (p.g4 == 0) unacceptable(0x9b);
      for (int c = 0; c < 5; ++c) {
        const int32_t ry = 0xac + 0x11 * c;
        text([&] { cat_itoa(p.counts[c]); }, "drawStringAt", f_ee8, 0xee, ry);
        if (p.gc[c] > 0) stars(p.gc[c], ry);
        if (p.gc[c] == 0) unacceptable(ry);
      }
      text([&] { cat_itoa(p.fac); }, "drawStringAt", f_ee8, 0xee, 0x101);
      if (p.gf > 0) stars(p.gf, 0x101);
      if (p.gf == 0) unacceptable(0x101);
    }
    str("Widget_draw", f_ee8, pool + 0x388, 0xae, 0x120);
    if (!(p.score > 0)) {
      quad(0x80007d08u);
      str("Widget_draw", f_ee8, pool + 0x380, 0x172, 0x50);
      str("drawStringAt", f_f28, pool + 0x368, 0x190, 0x136);
      quad(0x80000000u);
    } else {
      text([&] { cat_itoa(p.score); cat(pool + 0x360); }, "drawStringAt2", f_ee8, 0x18c, 0x4d);
      const uint32_t gi = u32(sub32(div32(sub32(p.score, 0x28), 5), 1));
      static const uint32_t kGrade[9] = {0x260, 0x278, 0x288, 0x2a0, 0x2bc, 0x2d4, 0x2ec, 0x2fc, 0x318};
      const uint32_t gr = gi > 0xb ? 0x248 : gi <= 8 ? kGrade[gi] : (p.cnt != 0x12 ? 0x32c : 0x344);
      text([&] { cat(pool + gr); }, "drawStringAt", f_020, 0x190, 0x136);
      text([&] { cat(state_va(ctx, &GameState::f_strpool_4c61dc) + 4); number(p.prize); cat(pool + 0x234); }, "drawStringAt", f_f28,
           0x190, 0x152);
    }
    img(state_va(ctx, &GameState::g_spriteRef_820f40), 0x2bd, 0x18e);   // the OK button
  }
};

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
  // P5A: mode 0's table is drawn as the rows are computed (the original's order); y is the table's pen (its L2c)
  ReportDraw rd(ctx);
  const bool table_on = mode == 0 && ReportDraw::on();
  int32_t table_y = 0;
  if (table_on) {
    rd.table_head();
    table_y = rd.cut_h(rd.art(0));
  }

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
      if (table_on) {
        ReportDraw::Row r{};
        r.h = h;
        r.par = par;
        r.len = m.load<int16_t>(hr + 4);
        r.cnt = cnt;
        r.wsum = wsum;
        r.time = time;
        r.pace = pace;
        r.d[0] = d1;
        r.d[1] = d2;
        r.d[2] = d3;
        r.best = best;
        r.which = which;
        r.flags = flags;
        r.a = a;
        r.b = b;
        r.c = c;
        r.hflags = m.load<uint32_t>(hr + 0x200);
        r.big = add32(add32(m.load<int16_t>(hr + 0x104) / 2, m.load<int16_t>(hr + 0x110)), m.load<int16_t>(hr + 0xee));
        r.w1fc = m.load<int32_t>(hr + 0x1fc);
        table_y = rd.table_row(r, table_y);
      }
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
    if (table_on) table_y = rd.table_subtotal(S, nholes, time, table_y);
  }

  Page pg;
  struct Mark {   // the dialog markers around the grading page (the backend's held-pixel probe reads them; gfx_dialogs.cpp)
    Ctx &c;
    bool on;
    Mark(Ctx &x, bool o) : c(x), on(o) {
      if (on) gfx_drop(c, "dialogs.begin", nullptr, 0);
    }
    ~Mark() {
      if (on) gfx_drop(c, "dialogs.end", nullptr, 0);
    }
  } mark(ctx, mode != 0 && ReportDraw::on());
  if (mode != 0 && ReportDraw::on()) rd.head();
  Hover ok{0x2bd, 0x18e, false};   // the page's OK button
  if (mode == 0) {
    // the table's foot and its OK button (x 0x2de, y the foot's text line + 0xf)
    if (table_on) {
      ok.x = 0x2de;
      ok.y = rd.table_foot(table_y, nholes);
      rd.img(state_va(ctx, &GameState::g_spriteRef_820f40), ok.x, ok.y);
    }
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
    pg.flags = flags;
    pg.crit = crit;
    pg.ideal = ideal;
    pg.step = step;
    pg.cnt = cnt;
    pg.half = half;
    pg.holes = nholes;
    pg.hp1 = hp1;
    if (nholes != 0) {
      const int32_t len = S[0];
      number(ctx, len);
      int32_t g = best != 0x12 ? add32(div32(mul32(mul32(sub32(len, ideal), add32(flags, 4)), 5), ideal), 10)
                               : sub32(10, div32(sub32(ideal, len), 100));
      g = clamp(g, 0, 10);
      pg.len = len;
      pg.g = g;
      score = g;
      if (g == 0) score = -999;
      int32_t g2 = sub32(10, mul32(abs32(add32(sub32(best, hp1), 1)), step));
      if (g2 < 0) g2 = 0;
      pg.g2 = g2;
      int32_t sc = add32(score, g2);
      if (g2 == 0) sc = -999;
      const int32_t tm = S[3];
      int32_t g3 = sub32(10, div32(sub32(tm, 0xeb), 6));
      if (g3 == 0 && !(tm > 0x12c)) g3 = 1;
      g3 = clamp(g3, 0, 10);
      pg.tm = tm;
      pg.g3 = g3;
      sc = add32(sc, g3);
      if (g3 == 0) sc = -999;
      const int32_t pc = div32(S[4], nholes);
      const int32_t g4 = clamp(add32(div32(sub32(pc, 0x6d), 10), 10), 0, 10);
      pg.pc = pc;
      pg.g4 = g4;
      score = add32(sc, g4);
      if (g4 == 0) score = -999;
      const int32_t counts[5] = {S[8], many, cnt_a, cnt_b, cnt_c};
      for (int k = 0; k < 5; ++k) {
        const int32_t gc = clamp(add32(sub32(counts[k], cnt), 10), 0, 10);
        pg.counts[k] = counts[k];
        pg.gc[k] = gc;
        score = add32(score, gc);
        if (gc == 0) score = -999;
      }
      int32_t fac = 0;
      for (int bit = mutant == 4 ? 7 : 6; bit < 0x14; ++bit)
        if ((gs.g_buildingKindMask & (1u << bit)) != 0) ++fac;
      const int32_t gf = clamp(add32(mul32(sub32(fac, half), 2), 8), 0, 10);
      pg.fac = fac;
      pg.gf = gf;
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
      pg.prize = prize;
      number(ctx, prize);
      gs.g_flags |= mutant == 2 ? (1u << 22) : (1u << 22) | (1u << 13);
    }
    pg.score = score;
    if (ReportDraw::on()) rd.page(pg);
  }
  if (mode == 2) return 0;
  if (ReportDraw::on()) rd.gx("Window::refreshFull", {rd.win, 0});   // the modal loop's first refresh
  report_loop(ctx, false, "showCourseReport", &ok);
  gs.g_ui_569498 = mutant == 6 ? 1 : 0;
  cm::pump_light(ctx, "showCourseReport");
  return 0;
}

// showEndOfYearScreen: the year's statistics rows (numbers formatted through the shared buffer), the membership history
// slot of this month, the month's highlight lines (two kinds call another stage), the board's debt warning counter, the
// one-button loop and the score summary
//
// Roadmap S2.8: the highlight lines are built in g_textBuffer as the original builds them (the month name, ': ', the event's
// text pieces, with loadStoryStrings / landmarkName at their places and the subject's digits through itoa into g_ui_821c50),
// which the port now does too (the buffers are not simulation objects); with an attached backend the screen is drawn as
// c3er.cpp CareerRetirementFlow_re draws it: its frame Image and six Surfaces (backend keys gfx_local_key(0x5400 ..)) take the
// top, bottom and line strips of interface\infoscreens\endoyear.pcx and of its alpha mask, the screen is dimmed, the top strip
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
  const int32_t month = gs.g_date / 1024;
  const int32_t this_idx = mod32(month, 500);
  const int32_t last_idx = mod32(sub32(month, 8), 500);
  auto hist = [&](uint32_t base, int32_t idx) { return static_cast<int32_t>(m.load<int16_t>(base + 2 * u32(idx))); };
  const uint32_t cash = state_va(ctx, &GameState::g_cashHistory);
  const uint32_t skill = state_va(ctx, &GameState::g_skillHistory);

  // one statistics row's drawing (the numbers formatted again inside ScreenDraw::text, which gives the buffers back): kind 0
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
  const int32_t step = add32(d.cut_h(line), gfxr::ScreenDraw::ctl(17) ? 1 : 0);
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
  }

  // the board's debt warnings: the counter picks the wording and is raised while the money is negative
  if (gs.g_cash_div100 < 0 && (gs.g_flags & (1u << 24)) == 0) {
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

  Hover close{0x226, y, false};   // the close button drawn above at (0x226, y)
  report_loop(ctx, true, "showEndOfYearScreen", &close);
  cm::pump_light(ctx, "showEndOfYearScreen");
  if (mutant != 5 && i32(call_peer(ctx, "addScore", {}).eax) != -1) call_peer(ctx, "drawScoreSummary", {});
  for (uint32_t o : {top, bottom, line, top_a, bottom_a, line_a, line_a, line, bottom_a, bottom, top_a, top}) d.gx("Surface_free", {o});
  d.gx("Image_dtor", {img});
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
