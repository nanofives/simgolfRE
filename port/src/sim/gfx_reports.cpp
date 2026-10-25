// Body family `reports` of the graphics backend (roadmap S2.8; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// primitives the report screens draw with (F1-F10 reports, hole stats, best scores, end of year, shortcuts / roster, the
// hire dialog and preferences) and the screen bodies whose geometry comes from the report art's sprite sizes. The screens
// make the calls in their port functions (commands_screens.inc, holestats.cpp, reports.cpp, routing.cpp, commands_menus.inc)
// through gfxr::ScreenDraw (gfx_reports.h). Specifications: c3da.cpp (Surface_drawSprite), c3dk.cpp (Sprite::drawAlpha),
// re/match/jgld_blit16.cpp (draw16_1002ed60), c3dq.cpp (Widget_draw / Widget_draw2 / drawStringAt / drawStringAt2), c3df.cpp
// (drawCentered2 / drawStringCentered / drawStringRightAt), c3be.cpp (Surface_box), c3ae.cpp (Widget_fillRect).
//
//   Surface_drawSprite(this, dst, x, y, mode, mask, extra)
//       this's handle slot +0x94 = jgld Sprite::drawAlpha(dst, m_20 + x, m_24 + y, float alpha = mode, mask, char extra).
//       Modelled: the root surface as dst, no mask, alpha in drawAlpha's quarter band [0.25 - g, 0.25 + g] (g = 0.03125, jgld's
//       tolerance float): the member draw16_1002ed60 (jgld_blit16.cpp), per sprite byte c: c >= 0xff skipped, with
//       extra != 0 also c >= 0xf8; otherwise the palette word P and the destination word D, both shifted left 4, blended per
//       field group (0x7c1f0 red + blue, 0x3e00 green): ((P + D) >> 1 + D) >> 1, masked, summed and shifted back, which is
//       floor((P + 3 D) / 4) per 5-bit channel (RGB555 masks: measured on cov_r_s2_screens, 99.4 % of the toolbar pixels of
//       rep_comments equal the quarter blend of the radial_hover frame with these masks, 21.9 % with the 565 masks, U-1271).
//       The clip is the destination's (Surface_box below narrows it). Guards (c3da.cpp SurfaceDrawSprite_re, c3dk.cpp): this
//       +8 != 0 is the load arm (reported), an alpha outside [0, 1] draws nothing, a display scale other than (1, 1, 1) is
//       reported. Roadmap S2.13 (U-1278): this family owns the five bodies S2.8 and S2.9 both registered (Surface_drawSprite,
//       Widget_fillRect, Widget_draw, drawStringAt, drawStringAt2); drawAlpha's flag byte is set to 1 at entry and cleared
//       only when the destination's format word +4 is 1; with it set the quarter band calls draw16_1002ed60, the member the
//       reference frames show (U-1271; instruction addresses in log/roadmap/S2/13_notes.md).
//   Widget_draw(this, font, text, x, y, len) / drawStringAt / drawStringAt2 (same arguments) / Widget_draw2(this, font, text,
//       x, y, w, len)
//       the font swap (c3dq.cpp FontSwap: a font whose +4 is not 0 replaces this +0x5c and zeroes +0x60 / +0x64 for the call),
//       then drawText (left at x), drawStringCentered (x - (run >> 1)), drawStringRightAt (x - run) or drawCentered2 (x +
//       (w - run) / 2, truncated toward zero); NULL text, no font (or +4 = 0) and min(strlen, len) <= 0 draw nothing; run =
//       the provider's width of the n characters; the text through prim_text's drawText path (gfxp::draw_text_run: the
//       target's Widget_setQuad colour, the baseline at y + the font's ascent).
//   Widget_fillRect(this, x1, y1, x2, y2, colour) -> jgld Surface::drawLine -> line16 (c3bc.cpp) with hline16 / vline16
//       (c3ay.cpp) for the axis-parallel cases, in the clip; a direct colour's low word.
//   Surface_box(this, x, y, w, h) -> jgld Surface::setClip(x, y, x + w, y + h) (the Surface vtable's slot +0x34);
//       Surface::callBounds (slot +0xc4) puts the whole surface back. Only this family's bodies honour the clip.
//   reports.begin
//       the first call of every screen function; nothing is drawn. A test-only underlay (negctl modes 101..103 fill the
//       root surface with 0x0000, 0x7fff, 0x2a55 there) tells the pixels a screen leaves depending on what lay beneath it
//       (the course the backend does not draw yet) from those it determines (port/test/test_gfx_reports.py).
//
// Negative controls (g_negctl_gfx_reports, sg_negctl_set "gfx.family.reports"):
//   1 Surface_drawSprite blends a half instead of a quarter   2 drawStringAt2 ends one pixel right of x - run
//   3 drawStringAt centres with (run + 1) >> 1                 4 Widget_draw draws one pixel lower
//   5 Widget_fillRect's line ends one pixel further right    6 Widget_draw2 centres with (w - run + 1) / 2
//   7 Surface_box leaves the clip as it was                    8 Surface::callBounds leaves the clip narrowed
//   9 Widget_fillRect's line one pixel left                    10 drawStringAt centres at x - run instead of x - (run >> 1)
//   20 Widget_draw one pixel to the right                      21 drawStringAt2 draws left-aligned at x instead of ending at x
//   (9, 10, 20, 21 were roadmap S2.9's dialogs modes 3, 8, 10, 11, moved here with the bodies by S2.13, U-1278)
//   the screens' own drawing controls (gfx_reports.h ScreenDraw::ctl): 11 financial year labels 1 lower, 12 comment rows 1
//   lower, 13 best-scores foot cell 1 lower, 14 hole-stats title 1 lower, 15 roster type names 1 right, 16 shortcuts middle line
//   black, 17 end-of-year lines 1 further apart, 18 hire band heads 1 lower, 19 histograph cash labels 1 right
//   (the screen bodies' modes are listed at each body; 101..103 are the underlay, not controls)
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"

namespace portsim {

int g_negctl_gfx_reports = 0;

namespace gfxf {
const int32_t *blit_scale(Gfx &g);   // gfx_prim_fill.cpp (Display::setScale's three words)
}
namespace gfxp {
void draw_text_run(Gfx &g, Ctx &ctx, uint32_t target, const std::string &text, uint32_t va, int32_t x, int32_t y,
                   const char *who);   // gfx_prim_text.cpp
}

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_reports); }

struct Clip {
  int32_t l = 0, t = 0, r = 0, b = 0;
  bool narrowed = false;
};
struct ReportState {
  Clip clip;
};
ReportState &st(Gfx &g) { return g.state_of<ReportState>("reports"); }
// the root surface's clip rectangle (the whole surface unless Surface_box narrowed it)
void clip_of(Gfx &g, int32_t &l, int32_t &t, int32_t &r, int32_t &b) {
  const Clip &c = st(g).clip;
  if (c.narrowed) {
    l = std::max(c.l, 0);
    t = std::max(c.t, 0);
    r = std::min(c.r, g.root.w);
    b = std::min(c.b, g.root.h);
  } else {
    l = t = 0;
    r = g.root.w;
    b = g.root.h;
  }
}

// ------------------------------------------------------------------------------------------------ Surface_drawSprite
uint16_t quarter_blend(uint16_t p, uint16_t d) {
  const uint32_t P = static_cast<uint32_t>(p) << 4, D = static_cast<uint32_t>(d) << 4;
  const int sh = mode() == 1 ? 1 : 2;   // mode 1: one halving only (a half blend)
  uint32_t e = (P & 0x7c1f0u) + (D & 0x7c1f0u), f = (P & 0x3e00u) + (D & 0x3e00u);
  e >>= 1;
  f >>= 1;
  if (sh == 2) {
    e = (e + (D & 0x7c1f0u)) >> 1;
    f = (f + (D & 0x3e00u)) >> 1;
  }
  return static_cast<uint16_t>((((e & 0x7c1f0u) + (f & 0x3e00u)) >> 4) & 0xffffu);
}

void b_draw_sprite(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  // c3da.cpp SurfaceDrawSprite_re: this +8 != 0 returns 0x18 before any drawing (the load arm is not modelled: reported)
  if (!gfx_is_local_key(a[0]) && ctx.mem.load<uint32_t>(a[0] + 8) != 0) {
    g.unsup("Surface_drawSprite: the load arm (a file name at +8)");
    return;
  }
  if (a[1] != g.root_va(ctx) || a[5] != 0) {
    g.unsup("Surface_drawSprite: a destination other than the root surface or a mask");
    return;
  }
  auto it = g.sprites.find(a[0]);
  if (it == g.sprites.end()) {
    g.unsup("Surface_drawSprite: a SpriteRef the backend has no sprite for");
    return;
  }
  float alpha;
  std::memcpy(&alpha, &a[4], 4);
  const double gt = 0.03125;
  if (static_cast<double>(alpha) < 0.0 || static_cast<double>(alpha) > 1.0) return;   // drawAlpha returns 3, draws nothing
  if (!(static_cast<double>(alpha) >= 0.25 - gt && static_cast<double>(alpha) <= 0.25 + gt)) {
    g.unsup("Surface_drawSprite: an alpha outside drawAlpha's quarter band");
    return;
  }
  const int32_t *scale = gfxf::blit_scale(g);
  if (scale[0] != 1 || scale[1] != 1 || scale[2] != 1) {
    g.unsup("Surface_drawSprite: a display scale other than (1, 1, 1)");
    return;
  }
  const Sprite8 &s = it->second;
  const bool flag = static_cast<uint8_t>(a[6]) != 0;
  const int32_t x = add32(static_cast<int32_t>(a[2]), s.left), y = add32(static_cast<int32_t>(a[3]), s.top);
  int32_t cl, ct, cr, cb;
  clip_of(g, cl, ct, cr, cb);
  const int32_t l = std::max(x, cl), t = std::max(y, ct), r = std::min(x + s.w, cr), b = std::min(y + s.h, cb);
  for (int32_t yy = t; yy < b; ++yy)
    for (int32_t xx = l; xx < r; ++xx) {
      const uint8_t c = s.px[static_cast<size_t>(yy - y) * static_cast<size_t>(s.w) + static_cast<size_t>(xx - x)];
      if (c >= 0xff || (flag && c >= 0xf8)) continue;
      uint16_t &w = g.root.px[static_cast<size_t>(yy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(xx)];
      w = quarter_blend(s.pal[c], w);
    }
}

// ------------------------------------------------------------------------------------------------ aligned text
enum class Align { Left, Centre, Right, InWidth };

void aligned(Gfx &g, Ctx &ctx, const uint32_t *a, Align al, const char *who) {
  const uint32_t self = a[0], font = a[1], text = a[2];
  int32_t x = static_cast<int32_t>(a[3]), y = static_cast<int32_t>(a[4]);
  const int32_t w = al == Align::InWidth ? static_cast<int32_t>(a[5]) : 0;
  const int32_t len = static_cast<int32_t>(al == Align::InWidth ? a[6] : a[5]);
  if (text == 0) return;
  const uint32_t saved[3] = {g.fonts[0], g.fonts[1], g.fonts[2]};
  struct Restore {
    Gfx &g;
    const uint32_t *s;
    ~Restore() {
      g.fonts[0] = s[0];
      g.fonts[1] = s[1];
      g.fonts[2] = s[2];
    }
  } restore{g, saved};
  if (font != 0 && ctx.mem.load<uint32_t>(font + 4) != 0) {
    g.fonts[0] = font;
    g.fonts[1] = g.fonts[2] = 0;
  }
  const uint32_t f = g.fonts[0];
  if (f == 0 || ctx.mem.load<uint32_t>(f + 4) == 0) return;
  std::string s = gfxb::vstring(ctx.mem, text);
  const int32_t n = std::min(static_cast<int32_t>(s.size()), len);
  if (n <= 0) return;
  s.resize(static_cast<size_t>(n));
  int32_t run = 0;
  if (al != Align::Left) {
    TextMetrics *tm = text_metrics(ctx);
    if (!tm || !tm->glyph_width(ctx, f, text, n, run)) {
      g.unsup(std::string(who) + ": the provider cannot measure");
      return;
    }
  }
  switch (al) {
    case Align::Left:
      if (mode() == 4) y = add32(y, 1);
      if (mode() == 20) x = add32(x, 1);
      break;
    case Align::Centre:
      x = sub32(x, mode() == 3 ? (run + 1) >> 1 : mode() == 10 ? run : run >> 1);
      break;
    case Align::Right:
      if (mode() != 21) x = sub32(x, run);
      if (mode() == 2) x = add32(x, 1);
      break;
    case Align::InWidth:
      x = add32(x, (mode() == 6 ? add32(sub32(w, run), 1) : sub32(w, run)) / 2);
      break;
  }
  gfxp::draw_text_run(g, ctx, self, s, text, x, y, who);
}
void b_widget_draw(Gfx &g, Ctx &ctx, const uint32_t *a, int) { aligned(g, ctx, a, Align::Left, "Widget_draw"); }
void b_draw_string_at(Gfx &g, Ctx &ctx, const uint32_t *a, int) { aligned(g, ctx, a, Align::Centre, "drawStringAt"); }
void b_draw_string_at2(Gfx &g, Ctx &ctx, const uint32_t *a, int) { aligned(g, ctx, a, Align::Right, "drawStringAt2"); }
void b_widget_draw2(Gfx &g, Ctx &ctx, const uint32_t *a, int) { aligned(g, ctx, a, Align::InWidth, "Widget_draw2"); }

// ------------------------------------------------------------------------------------------------ clip
void b_surface_box(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (a[0] != g.root_va(ctx)) {
    g.unsup("Surface_box: a surface other than the root");
    return;
  }
  Clip &c = st(g).clip;
  c.l = static_cast<int32_t>(a[1]);
  c.t = static_cast<int32_t>(a[2]);
  c.r = add32(c.l, static_cast<int32_t>(a[3]));
  c.b = add32(c.t, static_cast<int32_t>(a[4]));
  c.narrowed = mode() != 7;
}
void b_call_bounds(Gfx &g, Ctx &, const uint32_t *, int) {
  if (mode() != 8) st(g).clip.narrowed = false;
}

// ------------------------------------------------------------------------------------------------ Widget_fillRect
// jgld Surface::hline16 / vline16 (c3ay.cpp): nothing when the row (column) is outside the clip or the run has no length;
// the ends ordered, the run dropped when it lies entirely outside, clamped to [left, right - 1] ([top, bottom - 1]), every
// pixel of x1..x2 inclusive
void hline16(Surface16 &s, const int32_t *c, int32_t x1, int32_t x2, int32_t y, uint16_t v) {
  if (y < c[1] || y >= c[3] || x1 == x2) return;
  if (x1 > x2) std::swap(x1, x2);
  if (x1 >= c[2] || x2 < c[0]) return;
  if (x1 < c[0]) x1 = c[0];
  if (x2 >= c[2]) x2 = c[2] - 1;
  for (int32_t x = x1; x <= x2; ++x) s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] = v;
}
void vline16(Surface16 &s, const int32_t *c, int32_t x, int32_t y1, int32_t y2, uint16_t v) {
  if (x < c[0] || x >= c[2] || y1 == y2) return;
  if (y1 > y2) std::swap(y1, y2);
  if (y1 >= c[3] || y2 < c[1]) return;
  if (y1 < c[1]) y1 = c[1];
  if (y2 >= c[3]) y2 = c[3] - 1;
  for (int32_t y = y1; y <= y2; ++y) s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] = v;
}
// jgld Surface::line16 (c3bc.cpp Line16_re): vertical and horizontal lines through vline16 / hline16; otherwise the ends
// ordered by x, the line dropped when its box misses the clip (`>` / `<` tests), then the midpoint walk of the octant: a
// pre-walk up to the clip's left / top (bottom for an upward line), the first pixel, the major axis clamped to the clip and
// every step plotted, ending early when the minor axis leaves the clip
void line16(Surface16 &s, const int32_t *c, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint16_t v) {
  if (x1 == x2) return vline16(s, c, x1, y1, y2, v);
  if (y1 == y2) return hline16(s, c, x1, x2, y1, v);
  if (x2 < x1) {
    std::swap(x1, x2);
    std::swap(y1, y2);
  }
  const int32_t L = c[0], T = c[1], R = c[2], B = c[3];
  auto put = [&](int32_t x, int32_t y) {
    if (x >= 0 && y >= 0 && x < s.w && y < s.h) s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] = v;
  };
  const int32_t dx = x2 - x1;
  int32_t dy = y2 - y1;
  if (y1 < y2) {
    if (x1 > R || y1 > B || x2 < L || y2 < T) return;
    if (dx >= dy) {
      int32_t d = dy * 2 - dx;
      const int32_t ie = dy * 2, ine = (dy - dx) * 2;
      while (x1 < L || y1 < T) {
        if (d <= 0) d += ie;
        else { d += ine; ++y1; }
        ++x1;
      }
      if (x1 >= R || y1 >= B) return;
      put(x1, y1);
      if (x2 >= R) x2 = R - 1;
      while (x1 < x2) {
        if (d <= 0) d += ie;
        else {
          ++y1;
          if (y1 >= B) return;
          d += ine;
        }
        ++x1;
        put(x1, y1);
      }
    } else {
      int32_t d = dx * 2 - dy;
      const int32_t ie = dx * 2, ine = (dx - dy) * 2;
      while (x1 < L || y1 < T) {
        if (d <= 0) d += ie;
        else { d += ine; ++x1; }
        ++y1;
      }
      if (x1 >= R || y1 >= B) return;
      put(x1, y1);
      if (y2 >= B) y2 = B - 1;
      while (y1 < y2) {
        if (d <= 0) d += ie;
        else {
          ++x1;
          if (x1 >= R) return;
          d += ine;
        }
        ++y1;
        put(x1, y1);
      }
    }
  } else {
    if (x1 > R || y1 < T || x2 < L || y2 > B) return;
    dy = dy < 0 ? -dy : dy;
    if (dx >= dy) {
      int32_t d = dy * 2 - dx;
      const int32_t ie = dy * 2, ine = (dy - dx) * 2;
      while (x1 < L || y1 >= B) {
        if (d <= 0) d += ie;
        else { d += ine; --y1; }
        ++x1;
      }
      if (x1 >= R || y1 < T) return;
      put(x1, y1);
      if (x2 >= R) x2 = R - 1;
      while (x1 < x2) {
        if (d <= 0) d += ie;
        else {
          --y1;
          if (y1 < T) return;
          d += ine;
        }
        ++x1;
        put(x1, y1);
      }
    } else {
      int32_t d = dx * 2 - dy;
      const int32_t ie = dx * 2, ine = (dx - dy) * 2;
      while (x1 < L || y1 >= B) {
        if (d <= 0) d += ie;
        else { d += ine; ++x1; }
        --y1;
      }
      if (x1 >= R || y1 < T) return;
      put(x1, y1);
      if (y2 < T) y2 = T;
      while (y1 > y2) {
        if (d <= 0) d += ie;
        else {
          ++x1;
          if (x1 >= R) return;
          d += ine;
        }
        --y1;
        put(x1, y1);
      }
    }
  }
}

// Widget_fillRect(this, x1, y1, x2, y2, colour) -> Surface::drawLine(x1, y1, x2, y2, colour, 1) -> line16 on the 16-bit root
void b_widget_fill_rect(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (a[0] != g.root_va(ctx)) {
    g.unsup("Widget_fillRect: a surface other than the root");
    return;
  }
  if (!(a[5] & 0x80000000u)) {
    g.unsup("Widget_fillRect: a palette colour index");
    return;
  }
  int32_t c[4];
  clip_of(g, c[0], c[1], c[2], c[3]);
  int32_t x1 = static_cast<int32_t>(a[1]), x2 = static_cast<int32_t>(a[3]);
  if (mode() == 5) x2 = add32(x2, 1);
  if (mode() == 9) {
    x1 = sub32(x1, 1);
    x2 = sub32(x2, 1);
  }
  line16(g.root, c, x1, static_cast<int32_t>(a[2]), x2, static_cast<int32_t>(a[4]),
         static_cast<uint16_t>(a[5] & 0xffffu));
}

// ------------------------------------------------------------------------------------------------ the underlay (tests)
void b_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  g.ensure_root(ctx);
  const int m = mode();
  if (m < 101 || m > 103) return;
  static const uint16_t kFill[3] = {0x0000, 0x7fff, 0x2a55};
  std::fill(g.root.px.begin(), g.root.px.end(), kFill[m - 101]);
}

const GfxBodyDef kBodies[] = {
    {"Surface_drawSprite", b_draw_sprite}, {"Widget_draw", b_widget_draw},       {"drawStringAt", b_draw_string_at},
    {"drawStringAt2", b_draw_string_at2},  {"Widget_draw2", b_widget_draw2},     {"Surface_box", b_surface_box},
    {"Surface::callBounds", b_call_bounds}, {"reports.begin", b_begin},       {"Widget_fillRect", b_widget_fill_rect},
};

}  // namespace

bool gfx_sprite_cut(Ctx &ctx, uint32_t sref, int32_t &w, int32_t &h) {
  Gfx *g = gfx(ctx);
  if (!g) return false;
  auto it = g->sprites.find(sref);
  if (it == g->sprites.end()) {
    g->unsup("a screen's layout: a SpriteRef the backend has no sprite for");
    return false;
  }
  w = it->second.cut_w;
  h = it->second.cut_h;
  return true;
}

extern const GfxFamily kGfxFamily_reports = {"reports", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
