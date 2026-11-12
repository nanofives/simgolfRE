// Body family `buyland` of the graphics backend (roadmap S2.10; port/src/sim/gfx_body.h, listed in gfx_families.inc): the two
// text drawers the TRACTS FOR SALE screen (showBuyLandScreen, port/src/sim/buyland.cpp) reaches that no other family has.
// Specifications: c3dq.cpp (measureText2 = drawTextLinesAt), c3df.cpp (drawRight, measureMarkupRun), c3z.cpp
// (wrapTextToWidth); functions are cited by name.
//
//   drawTextLinesAt(this, text, x, y, width)
//       a NULL text or a width of 0 draws nothing. Per line: the budget is `width`; the line ends where wrapTextToWidth breaks
//       the remaining text (measureMarkupRun with this +0x40 == 0 is wrapTextToWidth on the base font +0x5c: the backend's
//       font 0); drawText(this, line, x, y, line length); y += Widget_value(this); a non-zero indent (+0x18, set from +0x14
//       at entry: read from the root window object, which no port code writes) narrows the width and moves x once. The
//       markup scanner (this +0x40 != 0) is reported, not modelled.
//   wrapTextToWidth(font, s, &budget, remain) (c3z.cpp Wrap_re): words start at s and at every space after s's first byte;
//       each word (its leading space included) is measured with measureTextWidth (the provider's width of n bytes) and taken
//       from the budget; the first word that does not fit ends the line: the break is the word's start + 1 (its space) for a
//       later word, and for the first word the next space + 1 when nothing was measured on this line before (the global
//       accumulator 0: always here, the line starts with it cleared), else the word. A run that fits returns NULL. The
//       last word (no space after it) is measured over the rest of the run; a first word that does not fit there returns
//       NULL when the accumulator is 0.
//   drawRight(this, text, x, y, w, len)
//       NULL text draws nothing; no base font (or its +4 = 0) draws nothing; n = min(strlen(text), len), n <= 0 draws
//       nothing; run = the width of the n bytes; drawRichText at x + (w - run) (32-bit wrapping). The screen passes
//       w = 0x80000000: the pen x is then x - run + 2^31, which GDI's TextOutA (the original's text output and the GDI text
//       provider's) takes as x - run (U-1291).
//
// Negative controls (g_negctl_gfx_buyland, sg_negctl_set "gfx.family.buyland"):
//   1 drawTextLinesAt's lines 1 pixel further apart        2 wrapTextToWidth breaks after the word that did not fit
//   3 drawRight ends 1 pixel left of x - run
#include <cstdint>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"

namespace portsim {

int g_negctl_gfx_buyland = 0;

namespace gfxp {
void draw_text_run(Gfx &g, Ctx &ctx, uint32_t target, const std::string &text, uint32_t va, int32_t x, int32_t y,
                   const char *who);   // gfx_prim_text.cpp
}

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_buyland); }

// Widget_value(target) (c3c.cpp Value_re) over the backend's font 0: +8 >= 0 gives +0x10 + +8, else +0xc
int32_t widget_value(Ctx &ctx, uint32_t f) {
  const int32_t i = ctx.mem.load<int32_t>(f + 8);
  return i >= 0 ? add32(ctx.mem.load<int32_t>(f + 0x10), i) : ctx.mem.load<int32_t>(f + 0xc);
}

// wrapTextToWidth over the run [s, s + remain) of the address space; returns the break's address or 0 (the run fits); false
// when the provider cannot measure
bool wrap_to_width(Ctx &ctx, TextMetrics *tm, uint32_t font, uint32_t s, int32_t budget, int32_t remain, uint32_t &brk) {
  const Memory &m = ctx.mem;
  int32_t left = budget;
  uint32_t word = s;
  auto measure = [&](uint32_t p, int32_t n, int32_t &w) { return tm->glyph_width(ctx, font, p, n, w); };
  for (;;) {
    if (remain == 0) {
      brk = 0;
      return true;
    }
    uint32_t sp = 0;
    for (int32_t k = 0; k < remain; ++k)   // memchr(word + 1, ' ', remain)
      if (m.load<uint8_t>(word + 1 + static_cast<uint32_t>(k)) == 0x20) {
        sp = word + 1 + static_cast<uint32_t>(k);
        break;
      }
    if (sp == 0) break;
    int32_t wd = 0;
    if (!measure(word, static_cast<int32_t>(sp - word), wd)) return false;
    left = sub32(left, wd);
    if (left < 0) {
      if (word == s) {
        brk = sp + 1;   // the accumulator is 0 at a line's start
        return true;
      }
      brk = mode() == 2 ? sp + 1 : word + 1;
      return true;
    }
    remain = add32(remain, static_cast<int32_t>(word - sp));
    word = sp;
  }
  int32_t wd = 0;
  if (!measure(word, remain, wd)) return false;
  left = sub32(left, wd);
  if (left >= 0 || word == s) {
    brk = 0;
    return true;
  }
  brk = word + 1;
  return true;
}

void b_draw_text_lines_at(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const uint32_t self = a[0], text = a[1];
  int32_t x = static_cast<int32_t>(a[2]), y = static_cast<int32_t>(a[3]), width = static_cast<int32_t>(a[4]);
  if (text == 0 || width == 0) return;
  g.ensure_root(ctx);
  if (self != g.root_va(ctx)) {
    g.unsup("drawTextLinesAt: a target other than the root surface");
    return;
  }
  if (ctx.mem.load<uint32_t>(self + 0x40) != 0) {
    g.unsup("drawTextLinesAt: the markup scanner (+0x40 set) is not modelled");
    return;
  }
  const uint32_t font = g.fonts[0];
  TextMetrics *tm = text_metrics(ctx);
  if (font == 0 || !tm) {
    g.unsup("drawTextLinesAt: no base font or no text provider");
    return;
  }
  const GfxBody draw_text = gfxb::find_body("drawText");
  int32_t indent = ctx.mem.load<int32_t>(self + 0x14);
  uint32_t cur = text;
  for (;;) {
    uint32_t brk = 0;
    const int32_t len = static_cast<int32_t>(vstrlen(ctx.mem, cur));
    if (!wrap_to_width(ctx, tm, font, cur, width, len, brk)) {
      g.unsup("drawTextLinesAt: the provider cannot measure");
      return;
    }
    const uint32_t args[5] = {self, cur, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                              brk != 0 ? brk - cur : static_cast<uint32_t>(len)};
    if (draw_text) draw_text(g, ctx, args, 5);
    y = add32(y, add32(widget_value(ctx, font), mode() == 1 ? 1 : 0));
    if (indent != 0) {
      width = sub32(width, indent);
      x = add32(x, indent);
      indent = 0;
    }
    if (brk == 0) break;
    cur = brk;
  }
}

void b_draw_right(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const uint32_t self = a[0];
  uint32_t text = a[1];
  if (text == 0) return;
  uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;
  struct FontBack {  // P24D fix 5d: the backend's font put back after a fitted draw
    Gfx &g;
    uint32_t f;
    ~FontBack() { g.fonts[0] = f; }
  } font_back{g, font};
  // P24D fix 5: the text in the current language before it is measured (its own width ends it at x + w; taken whole)
  const uint32_t tv = lang::language() != lang::kEnglish ? lang::redirect(ctx, text) : text;
  const int32_t cap = tv != text ? 0x7fffffff : static_cast<int32_t>(a[5]);
  if (tv != text) {  // P24D fix 5d: a smaller game font when the translation runs well past the English
    font = gfxb::lang_fit_font(ctx, font, text, std::min(static_cast<int32_t>(vstrlen(ctx.mem, text)), static_cast<int32_t>(a[5])), tv,
                               static_cast<int32_t>(vstrlen(ctx.mem, tv)));
    g.fonts[0] = font;
  }
  text = tv;
  std::string s = gfxb::vstring(ctx.mem, text);
  const int32_t n = std::min(static_cast<int32_t>(s.size()), cap);
  if (n <= 0) return;
  s.resize(static_cast<size_t>(n));
  TextMetrics *tm = text_metrics(ctx);
  int32_t run = 0;
  if (!tm || !tm->glyph_width(ctx, font, text, n, run)) {
    g.unsup("drawRight: the provider cannot measure");
    return;
  }
  int32_t x = add32(static_cast<int32_t>(a[2]), sub32(static_cast<int32_t>(a[4]), run));
  if (mode() == 3) x = sub32(x, 1);
  gfxp::draw_text_run(g, ctx, self, s, text, x, static_cast<int32_t>(a[3]), "drawRight");
}

const GfxBodyDef kBodies[] = {
    {"drawTextLinesAt", b_draw_text_lines_at},
    {"drawRight", b_draw_right},
};

}  // namespace

extern const GfxFamily kGfxFamily_buyland = {"buyland", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
