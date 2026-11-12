// Body family `text` of the graphics backend (roadmap S3.7; port/include/portsim/sim/gfx_text.h, port/src/sim/gfx_body.h,
// listed in gfx_families.inc). Specifications: the C3 / C4 hook sources named at each part (shim/src/re/*.cpp) and their notes
// re/analysis/ui/*.md; functions are cited by name, their instruction addresses are in those sources.
//
// THE ROOT TEXT OBJECT. Every frame text is drawn through the text object f_stateptr_4c1570 points at, the Image at
// g_rootWindow + 0x274. Image_ctor (c3dl.cpp ImageCtor_re) gives it: markup on (+0x40 = the read-only dword f_cfg_4e4210,
// which is 1 in the user's exe), the style / dropdown words of the three scanners 0, the link state 0, the colours of styles
// 1..3 (+0x70 / +0x74 / +0x78) and every shadow colour -1, every shadow offset 2. Widget_setQuad sets style 0's four words
// (prim_text family, which hands them here too), Widget_setQuad70 / 74 / 78 styles 1..3 (registered here). The fonts are the
// core's g.fonts (setDrawTarget: +0x5c base font, +0x60..+0x68 the style fonts).
//
//   drawRichText(text, x, y, len) (c3ek.cpp DrawRichText_re): with markup on, repeatedly the scanner (scanToken while +0x50
//       is 1, skipToken while the style +0x30 is 3, else scanMarkupText: c3l.cpp, c3s.cpp) gives the next delimiter; the text
//       before it is drawn by the run drawer in the current style, then the delimiter picks the next style: `{` 1, `[` 2, `}`
//       and `]` 0, a doubled one is one literal character drawn with the next run, `$` + the 6-byte link token (f_strpool_4e4218
//       + 0x3c) a link (style 3, link state 1, 5 more bytes skipped), `$` + the 9-byte tokens at + 0x30 / + 0x24 a dropdown
//       (+0x50 = 1, 9 more skipped; the second also style 3), `=` the link target state 2 (the text up to `>` is skipped), `^`
//       the dropdown arrow (+0x50 = 2, style 0). The style word persists in the object between calls (it is not reset at the
//       start of a text). The tail after each run registers hot spots (no pixels); the dropdown arrow image (+0x54) and the
//       combo of a filled g_richTextPairs slot are reported (the root's +0x54 is 0 from Image_ctor).
//   drawTextRun(text, x, y, len) (c3et.cpp AllocTextBuffer_re): '_' drawn as a space; font = the style's (+0x5c + 4 s when
//       set, else the base font), colour set ci = s when the style's main colour is not -1, else 0; baseline y + the tallest of
//       fonts 0..2 (+0x10, the TextMetrics ascent); the shadow run first at the shadow offsets unless the shadow colour is -1;
//       then the runs between tabs at the pen, the pen advanced by each run's width and, after a tab, to the next multiple of
//       30; returns x + the width of the ORIGINAL text ('_' kept) in the chosen font. A colour with bit 31 is a direct RGB555
//       word (r = (c >> 7) & 0xf8, g = (c >> 2) & 0xf8, b = (c << 3) & 0xff); any other is an index into the run palette, of
//       which only entry 1 is known (U-1230: RGB (0x80, 0, 0)); another index is reported.
//   drawMarkupRun(text, len) (c3o.cpp DrawMarkupRun_re): the width with the same tokens over its own style / dropdown words
//       (+0x38 / +0x4c), the dropdown arrow 0x1e (0x19 with +0x54 set) wide; Text_put = drawMarkupRun(text, strlen).
//   WINDOW TARGETS (roadmap S3.19): gfxt::set_text_target makes the run drawer, drawRichText and drawMarkupRun work on a
//       window-tree child's surface (clipped to its handle's setClip rectangle), with its Image's fonts and text object; with
//       one set, measureMarkupRun (c3df.cpp: the markup walk with wrapTextToWidth, c3z.cpp, and the break tail that moves a
//       mid-word break back to a space) serves the bio EditBox's wrapToLines / drawTextLines (gfx_editbox.cpp), and a colour
//       index 0 is black (U-1847). The root's paths are unchanged.
//
// Negative controls (g_negctl_gfx_text, sg_negctl_set "gfx.family.text"):
//   1 `{` does not change the style (the delimiter is still skipped)   2 a doubled `{{` draws no literal brace
//   3 the run drawer ignores tabs (one run)                             4 the shadow run is drawn one pixel lower
//   5 Text_put's width handed to the frame code is 8 more              6 a golfer / picture window one pixel right
//   7 drawWindowFrame's bottom strip one pixel lower                    8 the tool tip's frame one pixel right
//   9 drawMessageBuffer breaks a line 8 pixels later                    10 the drawPanelBackdrop tips one pixel lower
//   11 the amenities tip's value line one pixel lower
//   roadmap S3.13 (one body per name: the controls of S2.11's drawWindow.golfer and of the results family's drawStringRightAt
//   moved here with their bodies):
//   12 the golfer window's message text one pixel lower (was simfoto 2)   13 the golfer window's frame one pixel left (simfoto 3)
//   14 the golfer window's h < 0x60 text offset ignored (simfoto 4)      15 drawStringRightAt ends one pixel right of x - run
//                                                                           (results 1)
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gfx_text.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "commands_internal.h"
#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"

namespace portsim {

extern int g_defect_undo_button_help;  // tick.cpp mainLoop.undoButtonHelpOnly (P13A item 12)
namespace {
// P13A: the name the tips give the palettes' Undo button (the bulldozer): the original's f_strpool_4c7f1c + 0x10, or with
// mainLoop.undoButtonHelpOnly = 0 (the button arms the bulldoze mode, commands_panels.inc undo_button) "Bulldoze"
bool p13a_bulldoze() { return !SG_DEFECT(g_defect_undo_button_help); }
}  // namespace

int g_negctl_gfx_text = 0;
int32_t hover_tip_dwell();  // gfx_hud.cpp: the HUD tips' dwell (P6B hoverTip.tenFrameDwell)
// infoCursorTip.fixedBox (P11B fix 2, 2026-11-05, Mariano's play-test 5: `trim the blank area on the hover of the items ... the UI
// wraps below the bottom bar`): showInfoCursorPanel's tool tip is a fixed box, drawFrameR3(X - 0x50, 0x192, 0xa0, 0x70, 1)
// (0x70 at showInfoCursorPanel+0xf0, 0x192 at showInfoCursorPanel+0xfa), so it spans 800-space rows 402..514 whatever the description's
// length, and its horizontal meter is drawn at row 0x1ea (showInfoCursorPanel+0x4be, +0x4cb; its marker at 0x1e2, +0x4f0) under the
// vertical meter (0x1a9 .. 0x1d2, showInfoCursorPanel+0x42a / +0x417). The tip is drawn before the palette (the backdrop at (0xd8, 0x1e2)),
// so the palette covers its rows from 0x1e2 down, the horizontal meter included. Default 1 = the original's box. play (0): the
// description is measured first (the same wrap, nothing drawn), the horizontal meter goes 8 rows under the lower of the text's
// end and the vertical meter's base, the box ends 8 rows under it (height rounded up to 16, as drawFrameR3 rounds), and a box
// that would pass row 0x1e2 moves up with everything in it.
int g_defect_tip_box = 1;

namespace gfxs {   // gfx_simfoto.cpp: the held-pixel probe of drawWindow's golfer arm (roadmap S2.11; a measurement)
void window_probe(Gfx &g, Ctx &ctx, int32_t fx, int32_t fy, int32_t fh, int32_t bx, int32_t by);
void head_probe(Gfx &g, Ctx &ctx, int32_t bx, int32_t by);
}

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_text); }

// the run palette's entry 1 as the measured RGB (U-1230)
constexpr uint32_t kRunPalette1 = 0x80000000u | (0x80u >> 3) << 10;

struct TextObj {
  int32_t s30 = 0, s34 = 0, s38 = 0;      // drawRichText / measureMarkupRun / drawMarkupRun styles
  int32_t d50 = 0, d48 = 0, d4c = 0;      // their dropdown states
  int32_t link = 0;                        // +0x3c, shared
  uint32_t colour[4] = {0, 0xffffffffu, 0xffffffffu, 0xffffffffu};
  uint32_t shadow[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
  uint32_t dx[4] = {2, 2, 2, 2}, dy[4] = {2, 2, 2, 2};
};
struct TextState {
  std::map<uint32_t, TextObj> obj;   // by target (the root surface's address)
};
TextState &st(Gfx &g) { return g.state_of<TextState>("text"); }

// roadmap S3.19: a drawing target other than the root, the window surface of a window-tree child (the bio EditBox, drawn by
// the editbox family, gfx_editbox.cpp): its surface, the clip rectangle its handle's setClip holds, its Image's four fonts
// (+0x5c..+0x68) and the Image's address (the key of its text object). Set around one call by gfxt::set_text_target; with
// none set every path below is the root's, unchanged.
struct Target {
  Surface16 *s = nullptr;
  int32_t clip[4] = {0, 0, 0, 0};
  uint32_t fonts[4] = {0, 0, 0, 0};
  uint32_t obj = 0;
};
thread_local const Target *t_target = nullptr;
thread_local Target t_target_slot;

TextObj &root_obj(Gfx &g, Ctx &ctx) { return st(g).obj[t_target ? t_target->obj : g.root_va(ctx)]; }
const uint32_t *fonts_of(Gfx &g) { return t_target ? t_target->fonts : g.fonts; }

// ------------------------------------------------------------------------------------------------ measuring
// The provider measures a string in the address space: the text is put into g_textBuffer for the call and the bytes put back
// (gfx_hud.cpp with_scratch_buffer does the same).
// TEST-ONLY record mode (gfx_text.test op 1 / 2, port/test/test_gfx_text.py's emulator oracle): the run drawer logs the
// surface calls the original's drawTextRun makes (setFont, the colour calls, the text draws, end) instead of drawing, the widths
// follow the oracle's scripted font rule (sum of (c & 0xf) + 3 plus ((font object >> 8) & 7) per character; 0 when the
// FontRef's +4 is 0, measureTextWidth's NULL test), the baseline reads the FontRef's +0x10 word and a palette index i is the
// oracle's fake palette entry (3 i, 5 i, 7 i) & 0xff
struct Rec {
  bool on = false;
  std::vector<std::string> log;
};
Rec &rec() {
  static Rec r;
  return r;
}
void rlog(const std::string &s) {   // one line per call: a newline in a text is logged as `\n`
  std::string e;
  for (char c : s) e += c == '\n' ? std::string("\\n") : std::string(1, c);
  rec().log.push_back(e);
}

int32_t measure(Ctx &ctx, uint32_t font, const std::string &s) {
  if (rec().on) {
    if (font == 0) return 0;
    const uint32_t obj = ctx.mem.load<uint32_t>(font + 4);
    if (obj == 0) return 0;
    int32_t w = 0;
    for (char c : s) w = add32(w, (static_cast<uint8_t>(c) & 0xf) + 3);
    return add32(w, mul32(static_cast<int32_t>((obj >> 8) & 7), static_cast<int32_t>(s.size())));
  }
  TextMetrics *tm = text_metrics(ctx);
  if (!tm || font == 0 || s.empty()) return 0;
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
  const size_t n = std::min(s.size(), sizeof(GameState::g_textBuffer) - 1);
  std::vector<uint8_t> saved(n + 1);
  for (size_t i = 0; i <= n; ++i) saved[i] = ctx.mem.load<uint8_t>(buf + static_cast<uint32_t>(i));
  for (size_t i = 0; i <= n; ++i) ctx.mem.store<uint8_t>(buf + static_cast<uint32_t>(i), i < n ? static_cast<uint8_t>(s[i]) : 0);
  int32_t w = 0;
  if (!tm->glyph_width(ctx, font, buf, static_cast<int32_t>(n), w)) w = 0;
  for (size_t i = 0; i <= n; ++i) ctx.mem.store<uint8_t>(buf + static_cast<uint32_t>(i), saved[i]);
  return w;
}

uint32_t style_font(Gfx &g, int32_t s) {
  const uint32_t *f = fonts_of(g);
  if (s >= 0 && s < 4 && f[s] != 0) return f[s];
  return f[0];
}

// ------------------------------------------------------------------------------------------------ the scanners
bool is_delim(char c, int kind) {
  // kind 0 scanMarkupText: { } [ ] $; 1 skipToken: + '='; 2 scanToken: + '^'
  if (c == '{' || c == '}' || c == '[' || c == ']' || c == '$') return true;
  if (kind >= 1 && c == '=') return true;
  return kind == 2 && c == '^';
}
// returns the index of the delimiter (or where the count ran out) and leaves the remaining count, delimiter included, in len;
// a count of 0 on entry returns p unchanged
size_t scan(const std::string &t, size_t p, int32_t &len, int kind) {
  if (len == 0) return p;
  while (len != 0) {
    const char c = p < t.size() ? t[p] : '\0';
    if (is_delim(c, kind)) break;
    ++p;
    --len;
  }
  return p;
}
bool token_at(Ctx &ctx, const std::string &t, size_t p, uint32_t off, size_t n) {
  const uint32_t tok = state_va(ctx, &GameState::f_strpool_4e4218) + off;
  for (size_t i = 0; i < n; ++i) {
    const char a = p + i < t.size() ? t[p + i] : '\0';
    const char b = static_cast<char>(ctx.mem.load<uint8_t>(tok + static_cast<uint32_t>(i)));
    if (a != b) return false;
    if (a == '\0') return true;   // strncmp stops at a NUL in both
  }
  return true;
}

// ------------------------------------------------------------------------------------------------ the run drawer
struct RunColours {
  uint32_t main, shadow;
  int32_t dx, dy;
};

bool colour_ref(Gfx &g, uint32_t c, uint32_t &ref, const char *who) {
  if (!(c & 0x80000000u)) {
    // roadmap S3.19: on a window target index 0 is black, jgld Palette::setDefault's entry 0 (g_defaultPaletteLo's first
    // triple; the palette that answers at the bio EditBox is not measured, U-1847); the root path is unchanged
    if (c == 0 && t_target) {
      ref = 0;
      return true;
    }
    if (c != 1) {
      g.unsup(std::string(who) + ": a run palette colour index other than 1");
      return false;
    }
    c = kRunPalette1;
  }
  const uint32_t r = (c >> 7) & 0xf8, gr = (c >> 2) & 0xf8, b = (c << 3) & 0xff;
  ref = r | (gr << 8) | (b << 16);
  return true;
}

// record mode's drawTextRun: the original's surface calls in its order (c3et.cpp AllocTextBuffer_re)
int32_t draw_run_rec(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y, int32_t len, int32_t s,
                     const RunColours &rc) {
  const std::string orig = text.substr(0, static_cast<size_t>(len));
  std::string buf = orig;
  for (char &c : buf)
    if (c == '_') c = ' ';
  const uint32_t font = style_font(g, s);
  int32_t h = 0;
  const uint32_t *fonts = fonts_of(g);
  for (int i = 0; i < 3; ++i) {
    if (fonts[i] == 0) continue;
    const int32_t v = ctx.mem.load<int32_t>(fonts[i] + 0x10);
    if (v > h) h = v;
  }
  const int32_t yb = add32(y, h);
  rlog("setFont " + std::to_string(font != 0 ? font + 4 : 0));
  auto colour = [&](uint32_t c) {
    if (c & 0x80000000u) {
      rlog("rgb " + std::to_string((c >> 7) & 0xf8) + " " + std::to_string((c >> 2) & 0xf8) + " " + std::to_string((c << 3) & 0xff));
    } else {
      rlog("rgb " + std::to_string((3 * c) & 0xff) + " " + std::to_string((5 * c) & 0xff) + " " + std::to_string((7 * c) & 0xff));
    }
  };
  auto draw = [&](int32_t px, int32_t py, const std::string &seg) {
    rlog("draw " + std::to_string(px) + " " + std::to_string(py) + " " + seg);
  };
  if (rc.shadow != 0xffffffffu) {
    colour(rc.shadow);
    draw(add32(x, rc.dx), add32(add32(yb, rc.dy), mode() == 4 ? 1 : 0), buf);
  }
  colour(rc.main);
  int32_t pen = x;
  size_t start = 0;
  for (size_t i = 0; i <= buf.size(); ++i) {
    const bool end = i == buf.size();
    if (end || (buf[i] == '	' && mode() != 3)) {
      const std::string seg = buf.substr(start, i - start);
      draw(pen, yb, seg);
      pen = add32(pen, measure(ctx, font, seg));
      if (!end) pen = add32(pen, 30 - pen % 30);
      start = i + 1;
    }
  }
  rlog("end");
  return add32(x, measure(ctx, font, orig));
}

// drawTextRun over text[0, len) with the style s; returns the pen after the run
int32_t draw_run(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y, int32_t len, int32_t s,
                 const RunColours &rc, const char *who) {
  if (len <= 0) return x;
  if (rec().on) return draw_run_rec(g, ctx, text, x, y, len, s, rc);
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) {
    g.unsup(std::string(who) + ": no text provider");
    return x;
  }
  const std::string orig = text.substr(0, static_cast<size_t>(len));
  std::string buf = orig;
  for (char &c : buf)
    if (c == '_') c = ' ';
  const uint32_t font = style_font(g, s);
  int32_t asc = 0;
  const uint32_t *fonts = fonts_of(g);
  for (int i = 0; i < 3; ++i) {
    int32_t v = 0;
    if (fonts[i] && tm->glyph_ascent(ctx, fonts[i], v) && v > asc) asc = v;
  }
  const int32_t yb = add32(y, asc);
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return x;
  // the glyphs go to the root, or to a window target through a scratch copy of its surface of which only the clip rectangle
  // is kept (roadmap S3.19: the target's handle clips its text, setClip's region)
  auto put_glyphs = [&](const std::string &seg, int32_t px, int32_t py, uint32_t ref) {
    gfxb::TextRunScope ts;  // P9C: the run (inert with text capture off)
    if (!t_target) {
      gfxb::text_run_begin(g, g.root.px.data(), g.root.w, g.root.h, ts);
      lang::note("glyph", seg, false);  // P24D: SG_LANG_DUMP
      const bool ok = tm->draw_glyphs(ctx, font, seg, px, py, ref, g.root.px.data(), g.root.w, g.root.h);
      const int32_t clip[4] = {0, 0, g.root.w, g.root.h};
      gfxb::text_run_end(g, ctx, ts, g.root.px.data(), g.root.w, g.root.h, 0, 0, clip, font, seg, px, py, ref, 0);
      return ok;
    }
    Surface16 &d = *t_target->s;
    int32_t wx = 0, wy = 0;
    const bool placed = gfxe::window_origin(g, &d, wx, wy);
    if (placed) gfxb::text_run_begin(g, d.px.data(), d.w, d.h, ts);
    std::vector<uint16_t> tmp = d.px;
    lang::note("glyph", seg, false);  // P24D: SG_LANG_DUMP
    const bool ok = tm->draw_glyphs(ctx, font, seg, px, py, ref, tmp.data(), d.w, d.h);
    const int32_t *c = t_target->clip;
    for (int32_t yy = std::max(c[1], 0); yy < std::min(c[3], d.h); ++yy)
      for (int32_t xx = std::max(c[0], 0); xx < std::min(c[2], d.w); ++xx) {
        const size_t k = static_cast<size_t>(yy) * static_cast<size_t>(d.w) + static_cast<size_t>(xx);
        d.px[k] = tmp[k];
      }
    if (placed) gfxb::text_run_end(g, ctx, ts, d.px.data(), d.w, d.h, wx, wy, c, font, seg, px, py, ref, 1);
    return ok;
  };
  auto glyphs = [&](const std::string &seg, int32_t px, int32_t py, uint32_t colour) {
    uint32_t ref = 0;
    if (seg.empty() || !colour_ref(g, colour, ref, who)) return;
    if (!put_glyphs(seg, px, py, ref)) g.unsup(std::string(who) + ": the provider cannot draw");
  };
  auto runs = [&](int32_t px, int32_t py, uint32_t colour, bool advance_only) {
    int32_t pen = px;
    size_t start = 0;
    for (size_t i = 0; i <= buf.size(); ++i) {
      const bool end = i == buf.size();
      if (end || (buf[i] == '\t' && mode() != 3)) {
        const std::string seg = buf.substr(start, i - start);
        if (!advance_only) glyphs(seg, pen, py, colour);
        pen = add32(pen, measure(ctx, font, seg));
        if (!end) pen = add32(pen, 30 - pen % 30);
        start = i + 1;
      }
    }
  };
  // the shadow run (drawn whole at the shadow offsets: the original draws the full text once, tabs included, with slot +0xb8)
  if (rc.shadow != 0xffffffffu) {
    uint32_t ref = 0;
    if (colour_ref(g, rc.shadow, ref, who)) {
      const int32_t sy = add32(add32(yb, rc.dy), mode() == 4 ? 1 : 0);
      if (!put_glyphs(buf, add32(x, rc.dx), sy, ref)) g.unsup(std::string(who) + ": the provider cannot draw");
    }
  }
  runs(x, yb, rc.main, false);
  return add32(x, measure(ctx, font, orig));
}

RunColours colours_for(TextObj &o, int32_t s, uint32_t main0, uint32_t shadow0, int32_t dx0, int32_t dy0) {
  const int ci = (s >= 0 && s < 4 && o.colour[s] != 0xffffffffu && s != 0) ? s : 0;
  if (ci == 0) return {main0, shadow0, dx0, dy0};
  return {o.colour[ci], o.shadow[ci], static_cast<int32_t>(o.dx[ci]), static_cast<int32_t>(o.dy[ci])};
}

}  // namespace

namespace gfxt {

void set_quad(Gfx &g, uint32_t target, int k, uint32_t colour, uint32_t shadow, uint32_t dx, uint32_t dy) {
  if (k < 0 || k > 3) return;
  TextObj &o = st(g).obj[target];
  o.colour[k] = colour;
  o.shadow[k] = shadow;
  o.dx[k] = dx;
  o.dy[k] = dy;
}

// drawMarkupRun(text, len) on the root text object
int32_t markup_width(Gfx &g, Ctx &ctx, const std::string &t, int32_t len) {
  TextObj &o = root_obj(g, ctx);
  int32_t width = 0;
  size_t start = 0;
  for (;;) {
    size_t p;
    if (o.d4c == 1) p = scan(t, start, len, 2);
    else if (o.s38 == 3) p = scan(t, start, len, 1);
    else p = scan(t, start, len, 0);
    auto piece = [&](size_t a, size_t b) { return measure(ctx, style_font(g, o.s38), t.substr(a, b > a ? b - a : 0)); };
    if (len == 0) {
      width = add32(width, piece(start, p));
      if (o.d4c == 2) width = add32(width, 0x1e);
      o.link = 0;
      o.d4c = 0;
      return width;
    }
    int32_t style = o.s38;
    const char c = p < t.size() ? t[p] : '\0';
    switch (c) {
      case '$':
        if (token_at(ctx, t, p, 0x3c, 6)) {
          o.link = 1;
          style = 3;
        } else if (token_at(ctx, t, p, 0x30, 9)) {
          o.d4c = 1;
        } else if (token_at(ctx, t, p, 0x24, 9)) {
          o.d4c = 1;
          style = 3;
        }
        break;
      case '=': o.link = 2; break;
      case '^': o.d4c = 2; style = 0; break;
      case '{':
        if (p + 1 < t.size() && t[p + 1] == '{') { ++p; --len; } else if (mode() != 1) style = 1;
        break;
      case '[':
        if (p + 1 < t.size() && t[p + 1] == '[') { ++p; --len; } else style = 2;
        break;
      case '}':
        if (p + 1 < t.size() && t[p + 1] == '}') { ++p; --len; } else style = 0;
        break;
      case ']':
        if (p + 1 < t.size() && t[p + 1] == ']') { ++p; --len; } else style = 0;
        break;
      default: break;
    }
    width = add32(width, piece(start, p));
    const int32_t link = o.link;
    if (link == 1) {
      p += 5;
      len -= 5;
    } else if (o.d4c == 1) {
      p += 9;
      len -= 9;
    } else if (o.d4c == 2) {
      o.d4c = 0;
      width = add32(width, 0x1e);   // the root's +0x54 is 0 (Image_ctor), so the 0x19 arm cannot run
    }
    start = p + 1;
    if (link == 2) {
      const size_t gt = t.find('>', start);
      style = 0;
      if (gt != std::string::npos) {
        len = add32(len, static_cast<int32_t>(start) - static_cast<int32_t>(gt) - 1);
        start = gt + 1;
      }
      o.link = 0;
    }
    o.s38 = style;
    --len;
    if (len < 0) return width;   // a token past the count: the original reads on; the port stops at the text's end
  }
}

int32_t text_put(Gfx &g, Ctx &ctx, const std::string &text) {
  return markup_width(g, ctx, text, static_cast<int32_t>(text.size()));
}

// drawRichText(text, x, y, len) on the root with state-0 colours (main0, shadow0, dx0, dy0); returns the pen
int32_t draw_rich(Gfx &g, Ctx &ctx, const std::string &t, int32_t x, int32_t y, int32_t len, uint32_t main0, uint32_t shadow0,
                  int32_t dx0, int32_t dy0, const char *who) {
  TextObj &o = root_obj(g, ctx);
  size_t cur = 0;
  for (;;) {
    int32_t m = 2;
    size_t tok;
    if (o.d50 == 1) tok = scan(t, cur, len, 2);
    else if (o.s30 == 3) tok = scan(t, cur, len, 1);
    else tok = scan(t, cur, len, 0);
    if (len == 0) {
      return draw_run(g, ctx, t.substr(cur), x, y, static_cast<int32_t>(tok - cur), o.s30,
                      colours_for(o, o.s30, main0, shadow0, dx0, dy0), who);
    }
    bool keep = false;
    const char c = tok < t.size() ? t[tok] : '\0';
    switch (c) {
      case '$':
        if (token_at(ctx, t, tok, 0x3c, 6)) {
          o.link = 1;
          m = 3;
        } else if (token_at(ctx, t, tok, 0x30, 9)) {
          o.d50 = 1;
          keep = true;
        } else if (token_at(ctx, t, tok, 0x24, 9)) {
          o.d50 = 1;
          m = 3;
        } else {
          keep = true;
        }
        break;
      case '=': o.link = 2; keep = true; break;
      case '{':
        if (tok + 1 < t.size() && t[tok + 1] == '{' && mode() != 2) { ++tok; --len; keep = true; }
        else m = mode() == 1 ? o.s30 : 1;
        break;
      case '}':
        if (tok + 1 < t.size() && t[tok + 1] == '}') { ++tok; --len; keep = true; }
        else m = 0;
        break;
      case '[':
        if (tok + 1 < t.size() && t[tok + 1] == '[') { ++tok; --len; keep = true; }
        break;
      case ']':
        if (tok + 1 < t.size() && t[tok + 1] == ']') { ++tok; --len; keep = true; }
        else m = 0;
        break;
      case '^': o.d50 = 2; m = 0; break;
      default: keep = true; break;
    }
    if (keep) m = o.s30;
    if (o.d50 == 2) {
      if (ctx.mem.load<uint32_t>(state_va(ctx, &GameState::g_richTextPairs) + 8u * static_cast<uint32_t>(0)) != 0)
        g.unsup(std::string(who) + ": a dropdown combo in the text");
    } else {
      x = draw_run(g, ctx, t.substr(cur), x, y, static_cast<int32_t>(tok - cur), o.s30,
                   colours_for(o, o.s30, main0, shadow0, dx0, dy0), who);
    }
    const int32_t link = o.link;
    if (link == 1) {
      tok += 5;
      len -= 5;
    } else if (o.d50 == 1) {
      tok += 9;
      len -= 9;
    }
    ++tok;
    cur = tok;
    if (link == 2) {
      const size_t gt = t.find('>', cur);
      if (gt != std::string::npos) {
        len = add32(len, static_cast<int32_t>(cur) - static_cast<int32_t>(gt) - 1);
        cur = gt + 1;
      }
      m = 0;
      o.link = 0;
    }
    if (o.d50 == 2) o.d50 = 0;   // RichTail: the arrow image needs the root's +0x54, which is 0 (Image_ctor)
    --len;
    o.s30 = m;
    if (len < 0) return x;
  }
}

void draw_string(Gfx &g, Ctx &ctx, const std::string &text, int32_t x0, int32_t y0, uint32_t colour, bool centre,
                 const char *who) {
  g.ensure_root(ctx);
  if (!text_metrics(ctx) && !rec().on) {
    g.unsup(std::string(who) + ": no text provider");
    return;
  }
  if (text.empty()) return;
  const uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;
  const int32_t len = static_cast<int32_t>(text.size());
  int32_t x = x0;
  if (centre) x = add32(x0, (0 - markup_width(g, ctx, text, len)) / 2);
  draw_rich(g, ctx, text, x, y0, len, colour, 0xffffffffu, 2, 2, who);
}

// ------------------------------------------------------------------------------------------------ roadmap S3.19: window targets
// The bio EditBox (gfx_editbox.cpp) draws its text with these, on its own window surface and its own Image (the Image at
// EditBox + 0x274, in the GameState: g_characterEditorEditBox).

// the run drawer, drawRichText, drawMarkupRun and measureMarkupRun work on the window surface s (clip {l, t, r, b}), the
// Image's fonts and the text object of the Image at obj until the next call; s == nullptr goes back to the root
void set_text_target(Surface16 *s, const int32_t *clip, const uint32_t *fonts, uint32_t obj) {
  if (!s) {
    t_target = nullptr;
    return;
  }
  t_target_slot.s = s;
  for (int i = 0; i < 4; ++i) {
    t_target_slot.clip[i] = clip[i];
    t_target_slot.fonts[i] = fonts[i];
  }
  t_target_slot.obj = obj;
  t_target = &t_target_slot;
}

// The text object of the Image at `image` from its words w[off / 4] (off < 0xac, the caller reads them: gfx_editbox.cpp takes the
// constructor's words for an Image the port's state never constructed): with `fresh` (a new window) its style, dropdown and
// link words (they persist in the object between calls, as the original keeps them in the Image), and every time its per-style
// colours, shadow colours and shadow offsets (+0x6c, +0x7c, +0x8c, +0x9c plus 4 k: Image_ctor's and Widget_setQuad's words)
void load_text_obj(Gfx &g, uint32_t image, bool fresh, const uint32_t *w) {
  TextObj &o = st(g).obj[image];
  auto W = [&](uint32_t off) { return static_cast<int32_t>(w[off / 4]); };
  if (fresh) {
    o.s30 = W(0x30);
    o.s34 = W(0x34);
    o.s38 = W(0x38);
    o.link = W(0x3c);
    o.d48 = W(0x48);
    o.d4c = W(0x4c);
    o.d50 = W(0x50);
  }
  for (uint32_t k = 0; k < 4; ++k) {
    o.colour[k] = w[(0x6c + 4 * k) / 4];
    o.shadow[k] = w[(0x7c + 4 * k) / 4];
    o.dx[k] = w[(0x8c + 4 * k) / 4];
    o.dy[k] = w[(0x9c + 4 * k) / 4];
  }
}

namespace {
// wrapTextToWidth(font, s, &budget, remain) (c3z.cpp Wrap_re): greedy word wrap of t[s, s + remain) on one font, words
// delimited by a space found by memchr from the word's second byte; `acc` is the width accumulator the original keeps in a
// global (measureTextWidth on the font is the provider's width). Returns the break offset, or npos when the run fits.
size_t wrap_to_width(Ctx &ctx, uint32_t font, const std::string &t, size_t s, int32_t &budget, int32_t remain, int32_t &acc) {
  auto at = [&](size_t i) { return i < t.size() ? t[i] : '\0'; };
  auto width = [&](size_t a, int32_t n) {
    return n > 0 ? measure(ctx, font, t.substr(a < t.size() ? a : t.size(), static_cast<size_t>(n))) : 0;
  };
  int32_t left = budget;
  size_t word = s;
  for (;;) {
    if (remain == 0) {
      acc = add32(acc, sub32(budget, left));
      budget = left;
      return std::string::npos;
    }
    size_t sp = std::string::npos;
    for (int32_t k = 0; k < remain; ++k)
      if (at(word + 1 + static_cast<size_t>(k)) == ' ') {
        sp = word + 1 + static_cast<size_t>(k);
        break;
      }
    if (sp == std::string::npos) break;
    const int32_t wd = width(word, static_cast<int32_t>(sp - word));
    const int32_t prev = acc;
    left = sub32(left, wd);
    if (word == s) {
      if (left < 0) {
        budget = 0;
        return prev == 0 ? sp + 1 : word;
      }
    } else if (left < 0) {
      budget = 0;
      return word + 1;
    }
    remain = add32(remain, static_cast<int32_t>(word) - static_cast<int32_t>(sp));
    word = sp;
  }
  const int32_t wd = width(word, remain);
  const int32_t prev = acc;
  left = sub32(left, wd);
  if (left >= 0) {
    acc = add32(acc, sub32(budget, left));
    budget = left;
    return std::string::npos;
  }
  budget = 0;
  if (word == s) return prev == 0 ? std::string::npos : word;
  return word + 1;
}

// the tail that moves a mid-word break back to a space (c3df.cpp TidyA / TidyB / TidyC): the break itself after a space or
// at the piece start, else reverseFindChar(run start, break, ' ') (c3r.cpp: the bytes at break, break - 1, ... down to run
// start + 1; none found is NULL, which the caller takes as `the run fits`)
size_t tidy(const std::string &t, size_t brk, size_t piece, size_t run) {
  if (brk > 0 && brk - 1 < t.size() && t[brk - 1] == ' ') return brk;
  if (brk == piece) return brk;
  if (brk == run) return std::string::npos;
  for (size_t p = brk; p > run; --p)
    if (p < t.size() && t[p] == ' ') return p;
  return std::string::npos;
}
}  // namespace

// measureMarkupRun(text, &budget, len) (c3df.cpp MeasureMarkupRun_re) on the current target's Image (markup on): how much of
// t[text0, text0 + len) fits in budget pixels, walking the markup with the Image's measureMarkupRun words (+0x34 style,
// +0x3c link, +0x48 dropdown). Returns the break offset (the start of the line that continues) or npos when the run fits.
size_t measure_markup_run(Gfx &g, Ctx &ctx, const std::string &t, size_t text0, int32_t &budget, int32_t len, int32_t &acc) {
  TextObj &o = root_obj(g, ctx);
  const uint32_t *fonts = fonts_of(g);
  if (o.d48 == 2) o.d48 = 1;
  size_t start = text0;
  for (;;) {
    size_t p;
    if (o.d48 == 1) p = scan(t, start, len, 2);
    else if (o.s34 == 3) p = scan(t, start, len, 1);
    else p = scan(t, start, len, 0);
    const int32_t piece = static_cast<int32_t>(p - start);
    const uint32_t sf = (o.s34 >= 0 && o.s34 < 4) ? fonts[o.s34] : 0;
    if (len == 0) {
      const size_t brk = wrap_to_width(ctx, sf != 0 ? sf : fonts[0], t, start, budget, piece, acc);
      o.link = 0;
      if (brk != std::string::npos) return tidy(t, brk, start, text0);
      o.d48 = 0;
      return std::string::npos;
    }
    int32_t style = o.s34;
    const char c = p < t.size() ? t[p] : '\0';
    const char c1 = p + 1 < t.size() ? t[p + 1] : '\0';
    switch (c) {
      case '$':
        if (token_at(ctx, t, p, 0x3c, 6)) {
          o.link = 1;
          style = 3;
        } else if (token_at(ctx, t, p, 0x30, 9)) {
          o.d48 = 1;
        } else if (token_at(ctx, t, p, 0x24, 9)) {
          o.d48 = 1;
          style = 3;
        }
        break;
      case '=': o.link = 2; break;
      case '^': o.d48 = 2; style = 0; break;
      case '{': if (c1 == '{') { ++p; --len; } else style = 1; break;
      case '[': if (c1 == '[') { ++p; --len; } else style = 2; break;
      case '}': if (c1 == '}') { ++p; --len; } else style = 0; break;
      case ']': if (c1 == ']') { ++p; --len; } else style = 0; break;
      default: break;
    }
    // the text before the delimiter, on the font of the style that was current
    const size_t brk = wrap_to_width(ctx, sf != 0 ? sf : fonts[0], t, start, budget, piece, acc);
    if (brk != std::string::npos) {
      o.link = 0;
      return tidy(t, brk, start, text0);
    }
    const int32_t link = o.link;
    if (link == 1) {
      p += 5;
      len -= 5;
    } else if (o.d48 == 1) {
      p += 9;
      len -= 9;
    } else if (o.d48 == 2) {
      o.d48 = 0;
    }
    start = p + 1;
    if (link == 2) {
      const size_t gt = t.find('>', start);
      style = 0;
      if (gt != std::string::npos) {
        len = add32(len, static_cast<int32_t>(start) - static_cast<int32_t>(gt) - 1);
        start = gt + 1;
      }
      o.link = 0;
    }
    o.s34 = style;
    --len;
    if (len < 0) return std::string::npos;   // a token past the count: the original reads on; the port stops at the end
  }
}

}  // namespace gfxt

namespace {

// ------------------------------------------------------------------------------------------------ helpers over other bodies
uint32_t va(Ctx &ctx, uint32_t off) {
  return state_va(ctx, &GameState::f_strpool_4e4218) - static_cast<uint32_t>(offsetof(GameState, f_strpool_4e4218)) + off;
}
#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))
// a core or family body by its drop name (the original reaches it through a call)
void call(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  uint32_t a[12] = {};
  size_t i = 0;
  for (uint32_t v : args) a[i++] = v;
  if (rec().on && std::strcmp(name, "setDrawTarget") != 0) {   // record mode: the call, not its drawing
    std::string l = name;
    if (std::strcmp(name, "drawCenteredText") == 0) {   // (text, x, y, colour): the numbers, then the text
      for (size_t k = 1; k < args.size(); ++k) l += " " + std::to_string(a[k]);
      l += "|" + gfxb::vstring(ctx.mem, a[0]);
    } else {
      for (size_t k = 0; k < args.size(); ++k) l += " " + std::to_string(a[k]);
      if (std::strcmp(name, "drawPanelBackdrop") == 0)   // its text is the one in g_textBuffer
        l += "|" + gfxb::vstring(ctx.mem, state_va(ctx, &GameState::g_textBuffer));
    }
    rlog(l);
    return;
  }
  if (GfxBody b = gfxb::find_body(name)) {
    b(g, ctx, a, static_cast<int>(args.size()));
    return;
  }
  g.unsup(std::string("gfx_text: no body ") + name);
}
uint32_t u(int32_t v) { return static_cast<uint32_t>(v); }

// Widget_value(root) (c3c.cpp Value_re) on the base font: +8 >= 0 gives +0x10 + +8, else +0xc
int32_t widget_value(Gfx &g, Ctx &ctx) {
  const uint32_t f = g.fonts[0] ? g.fonts[0] : ctx.gs().g_primarySurface;
  const int32_t i = ctx.mem.load<int32_t>(f + 8);
  return i >= 0 ? add32(ctx.mem.load<int32_t>(f + 0x10), i) : ctx.mem.load<int32_t>(f + 0xc);
}

// drawText(root, text, x, y, limit) (c3ek.cpp DrawText_re) under a quad (main, shadow, dx, dy): no base font or one whose +4
// is 0 draws nothing; n = min(strlen, limit); drawRichText
void draw_text_q(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y, uint32_t main, uint32_t shadow, int32_t dx,
                 int32_t dy, const char *who) {
  const uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;
  if (text.empty()) return;
  gfxt::draw_rich(g, ctx, text, x, y, static_cast<int32_t>(text.size()), main, shadow, dx, dy, who);
}

// drawTextSurface(text, x, y, colour) (c3dq.cpp DrawTextSurface_re): setQuad(colour, 1, 0, 1); v = Widget_value; v + y >= the
// root's height moves y to height - v - 1; drawText(text, x, y, strlen)
void text_surface(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y, uint32_t colour) {
  const int32_t v = widget_value(g, ctx);
  if (add32(v, y) >= g.root.h) y = sub32(sub32(g.root.h, v), 1);
  draw_text_q(g, ctx, text, x, y, colour, 1, 0, 1, "drawTextSurface");
}

// drawMessageBuffer(cols, x, y, colour) (c3dv.cpp DrawValueWidget_re) over g_textBuffer: H = Widget_value - 4, at least 10;
// per character i the width of that character alone (Text_put of the two-byte string f_strpool_4c3f70 with its first byte
// replaced: drawMarkupRun, so a markup character measures 0 and moves the measure style) accumulates; ' ', newline and '^'
// mark the break e; a newline or '^', or a width above cols * 8, draws buf[st, e) at (x, y) through drawTextSurface when
// H > 16 and colour is 0x80007fff, else drawTextWithColor (setQuad(colour, -1, 2, 2)), y += H, st = e + 1 (e + 2 after a '^'),
// i = e; y > 0x248 stops. A pending line (acc > 0, y <= 0x248) is drawn the same way at the end. Returns y.
// P11B fix 2: draw = false measures only (the end y of the same wrap; nothing drawn, the text object's markup state put back)
// P24D: drawWindow's text in the current language, wrapped again (b_draw_window); null = g_textBuffer through lang::tr
thread_local const std::string *t_msg_text = nullptr;
int32_t message_buffer_impl(Gfx &g, Ctx &ctx, int32_t cols, int32_t x, int32_t y, uint32_t colour, bool draw) {
  // P24D: in the current language
  const std::string buf =
      t_msg_text ? *t_msg_text : lang::tr(gfxb::vstring(ctx.mem, state_va(ctx, &GameState::g_textBuffer)));
  if (!text_metrics(ctx) && !rec().on) {
    if (draw) g.unsup("drawMessageBuffer: no text provider");
    return y;
  }
  int32_t H = sub32(widget_value(g, ctx), 4);
  if (H < 10) H = 10;
  if (buf.empty()) return y;
  const bool shadowed = H > 0x10 && colour == 0x80007fffu;
  struct ObjBack {  // measure-only: the root text object as it was (text_put moves its markup words)
    TextObj *o;
    TextObj saved;
    ~ObjBack() { if (o) *o = saved; }
  } back{draw ? nullptr : &root_obj(g, ctx), draw ? TextObj{} : root_obj(g, ctx)};
  // the line is buf + st with the byte at e cut to NUL: up to e when e >= st, else up to the text's end (a break index left
  // behind st: the original then draws the rest of the buffer, once per pass until y passes 0x248)
  auto line = [&](size_t st, size_t e) {
    if (!draw) return;
    const std::string s = st < buf.size() ? buf.substr(st, e >= st ? e - st : std::string::npos) : std::string();
    if (shadowed) text_surface(g, ctx, s, x, y, colour);
    else draw_text_q(g, ctx, s, x, y, colour, 0xffffffffu, 2, 2, "drawTextWithColor");
  };
  size_t st = 0, e = 0, i = 0;
  int32_t acc = 0;
  do {
    const char c = buf[i];
    acc = add32(acc, gfxt::text_put(g, ctx, std::string(1, c)));
    if (c == ' ' || c == 0x0a || c == '^') e = i;
    if (c == 0x0a || c == '^' || acc > add32(cols * 8, mode() == 9 ? 8 : 0)) {
      line(st, e);
      y = add32(y, H);
      acc = 0;
      st = e + 1;
      if (buf[i] == '^') st = e + 2;
      i = e;
      if (y > 0x248) return y;
    }
    ++i;
  } while (i < buf.size());
  if (acc > 0 && y <= 0x248) {
    line(st, buf.size());
    y = add32(y, H);
  }
  return y;
}
int32_t message_buffer(Gfx &g, Ctx &ctx, int32_t cols, int32_t x, int32_t y, uint32_t colour) {
  return message_buffer_impl(g, ctx, cols, x, y, colour, true);
}

// drawWindowFrame(x, y, w, h, flag) (c3eh.cpp DrawWindowFrame_re): the display scale (1, 1, 1); w and x, h and y 16-rounded
// as the frame drawers do (w += 15 - (w & 15), x -= (15 - (w & 15)) >> 1 when w & 15 != 0); flag != 0: blitPanel(x, y - 1, w,
// h, 0); the bottom strip: for i = 12, 28, .. below w - 16 Surface_stretchBlit of g_sprites5a53c8 + 0x23c over
// g_sprites58b680 + 0x23c at (x + i, y + h - 16), then + 0x268 at (x + w - 12, ..) and + 0x210 at (x - 4, ..)
void window_frame(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h, int32_t flag) {
  call(g, ctx, "Display::setScale", {1, 1, 1});
  const int32_t rw = w & 0xf;
  if (rw != 0) {
    w = add32(w, 0xf - rw);
    x = sub32(x, (0xf - rw) >> 1);
  }
  const int32_t rh = h & 0xf;
  if (rh != 0) {
    h = add32(h, 0xf - rh);
    y = sub32(y, (0xf - rh) >> 1);
  }
  const uint32_t root = g.root_va(ctx);
  if (flag != 0) call(g, ctx, "blitPanel", {u(x), u(sub32(y, 1)), u(w), u(h), 0});
  const int32_t right = sub32(w, 16), row = add32(sub32(add32(y, h), 16), mode() == 7 ? 1 : 0);
  auto piece = [&](uint32_t off, int32_t px) {
    call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites5a53c8, off)), va(ctx, GSO(g_sprites58b680, off)), root, u(px), u(row), 0});
  };
  if (right > 12)
    for (int32_t i = 12; i < right; i += 16) piece(0x23c, add32(x, i));
  piece(0x268, sub32(add32(x, w), 12));
  piece(0x210, sub32(x, 4));
}

// P24D: wrapMessageBuffer's rule (commands.cpp wrap_message_buffer) over a host text `t` in place: a '^' becomes a space with a
// LF after it, an existing LF counts, a span [S, i) wider than cols * 8 (Text_put in the root's current fonts) breaks at its last
// space (that byte becomes LF), '_' becomes ' '. Returns the number of breaks. The root text object's markup words are put back
// (Text_put moves them). Results are kept per (text, cols): the ticker draws the same text every frame.
int32_t wrap_like_sim(Gfx &g, Ctx &ctx, std::string &t, int32_t cols) {
  static thread_local std::map<std::pair<std::string, int32_t>, std::pair<std::string, int32_t>> memo;
  const auto key = std::make_pair(t, cols);
  auto f = memo.find(key);
  if (f != memo.end()) {
    t = f->second.first;
    return f->second.second;
  }
  TextObj &o = root_obj(g, ctx);
  const TextObj saved = o;
  int32_t count = 0, S = 0, L = -1;
  for (int32_t i = 0; i < static_cast<int32_t>(t.size()); ++i) {
    if (t[static_cast<size_t>(i)] == '^') {
      t[static_cast<size_t>(i)] = ' ';
      S = i + 2;
      ++count;
      if (static_cast<size_t>(i) + 1 < t.size()) t[static_cast<size_t>(i) + 1] = '\n';
    }
    if (t[static_cast<size_t>(i)] == '\n') {
      ++count;
      S = i + 1;
    }
    if (i > S) {
      const int32_t w = gfxt::text_put(g, ctx, t.substr(static_cast<size_t>(S), static_cast<size_t>(i - S)));
      if (w > mul32(cols, 8) && L != -1) {
        i = L;
        ++count;
        L = -1;
        t[static_cast<size_t>(i)] = '\n';
        S = i + 1;
      }
    }
    if (t[static_cast<size_t>(i)] == '_') t[static_cast<size_t>(i)] = ' ';
    if (t[static_cast<size_t>(i)] == ' ') L = i;
  }
  o = saved;
  if (memo.size() > 512) memo.clear();
  memo.emplace(key, std::make_pair(t, count));
  return count;
}

// drawWindow(x, y, p3, mode) (c3ei.cpp DrawWindow_re), the drawing after the window height (the port's layout already wrapped
// g_textBuffer with wrapMessageBuffer and stored H in g_value587ea0): a[4] = h = 15 r + 0x20 of that wrap when the caller
// passes it (n == 5), else h = H. sel = f_w_4c2e00 + 8. With a golfer or picture (sel != -1) the window moves to (x + 0x1c,
// y + 0x2c) and mode is -2. mode >= 0 (the slide): drawWindowFrame(x - 8, y - 8, 0x1a0, (H mode) / 4 rounded toward zero, 1)
// only. mode -3 drawFrameS2, -2 drawFrameR3, else drawWindowFrame, each (x - 8, y - 8, 0x1a0, H + 0x10, 1/-1). sel == -1:
// setDrawTarget(g_font51b360), drawMessageBuffer(0x63, x + 0xc, y + 0x10, colour). sel >= 0: the golf-ball backdrop
// (Surface_stretchBlit g_sprites5a53c8 + 0x4fc over g_sprites58b680 + 0x4fc at (x - 0x32, y - 0x28)) and drawThoughtBubble(sel,
// x - 0x32, y - 0x28, -1) (c3ei.cpp DrawThoughtBubble_re: composeGolferSprite(sel), then Surface_drawScaled of the head
// g_sprites5628e8 + 0x2c (3 (app + 72 typeBit7Clear(sel)) + thoughtFlag(sel)) at (x, y), app = the signed byte +0x22 of the
// golfer's type record); -0xe <= sel <= -6 a picture of g_sprites53df58 (+0x2c - 16 (11 sel + 0x42)) at (x + 8, y - 0x14);
// -5 the backdrop at (x - 0x28, y - 0x28); -4 the course picture g_sprites541710 + 0x2c k over g_sprites5791f8 + 0x2c k at
// (x + 8, y), k = the location record's byte +0x18 of the course; then y += 0x10 and the icon of -3 / -2 / -0x14 / -0x15 /
// -0x16 (g_sprites567a20 + 0x2c / 0x58 / 0 / 0xb0 / 0x84 over g_sprites53de78 + the same) at (x, y); h < 0x60 centres the
// text: y += (H - h - 0x10) / 2; drawMessageBuffer(0x63, x + 0x64, y, colour). colour = 0x80007fff, or 0x80000018 for -3.
// Roadmap S3.13: this is drawWindow's only body (S2.11's core b_draw_window and simfoto's drawWindow.golfer are gone). For
// sel >= 0 the simfoto family's probe (a measurement, port/test/test_gfx_dialogs2.py) fills the frame's and the ball's
// rectangles before the frame and the ball's square again after the head.
void b_draw_window(Gfx &g, Ctx &ctx, const uint32_t *a, int n) {
  g.ensure_root(ctx);
  GameState &gs = ctx.gs();
  int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), mode = static_cast<int32_t>(a[3]);
  int32_t H = gs.g_value587ea0;
  int32_t h = n >= 5 ? static_cast<int32_t>(a[4]) : H;
  int32_t sel = gs.f_w_4c2e00[2];
  const uint32_t root = g.root_va(ctx);
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  // P24D: the text in the current language. The simulation wrapped the English text (wrapMessageBuffer: LF at the breaks, H from
  // its line count); a translation is wrapped again by the same rule in the same font and H / h follow its line count, for this
  // drawing only (g_textBuffer and g_value587ea0 are not written)
  std::string msg_tr;
  struct MsgScope {
    ~MsgScope() { t_msg_text = nullptr; }
  } msg_scope;
  if (lang::language() != lang::kEnglish) {
    const std::string src = gfxb::vstring(ctx.mem, state_va(ctx, &GameState::g_textBuffer));
    msg_tr = lang::tr(src);
    if (msg_tr != src) {
      const int32_t r = wrap_like_sim(g, ctx, msg_tr, sel != -1 ? 0x25 : 0x2c);
      h = add32(mul32(r, 15), 0x20);
      H = sel != -1 && h < 0x60 ? 0x60 : h;
      t_msg_text = &msg_tr;
    }
  }
  if (sel != -1) {
    if (n < 5 && H == 0x60) g.unsup("drawWindow: a golfer window whose wrapped line count is not known");
    y = add32(y, 0x2c);
    x = add32(x, SG_NEGCTL(g_negctl_gfx_text) == 6 ? 0x1d : 0x1c);
    mode = -2;
  }
  const int32_t m = SG_NEGCTL(g_negctl_gfx_text);
  if (sel >= 0) gfxs::window_probe(g, ctx, sub32(x, m == 13 ? 9 : 8), sub32(y, 8), add32(H, 0x10), sub32(x, 0x32), sub32(y, 0x28));
  if (mode >= 0) {
    const int32_t hm = mul32(H, mode);
    window_frame(g, ctx, sub32(x, 8), sub32(y, 8), 0x1a0, add32(hm, (hm >> 31) & 3) >> 2, 1);
    return;
  }
  const uint32_t fx = u(sub32(x, sel >= 0 && m == 13 ? 9 : 8)), fy = u(sub32(y, 8)), fh = u(add32(H, 0x10));
  if (mode == -3) call(g, ctx, "drawFrameS2", {fx, fy, 0x1a0, fh, u(-1)});
  else if (mode == -2) call(g, ctx, "drawFrameR3", {fx, fy, 0x1a0, fh, 1});
  else window_frame(g, ctx, sub32(x, 8), sub32(y, 8), 0x1a0, add32(H, 0x10), 1);
  const uint32_t colour = (mode + 3 != 0 ? 0x7fe7u : 0u) + 0x80000018u;
  if (sel == -1) {
    call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
    message_buffer(g, ctx, 0x63, add32(x, 0xc), add32(y, 0x10), colour);
    return;
  }
  auto stretch = [&](uint32_t s, uint32_t m, int32_t px, int32_t py) { call(g, ctx, "Surface_stretchBlit", {s, m, root, u(px), u(py), 0}); };
  auto scaled = [&](uint32_t s, int32_t px, int32_t py) { call(g, ctx, "Surface_drawScaled", {s, root, u(px), u(py), 1, 1, 1, 0}); };
  const uint32_t ball = va(ctx, GSO(g_sprites5a53c8, 0x4fc)), ball_m = va(ctx, GSO(g_sprites58b680, 0x4fc));
  if (sel >= 0) {
    stretch(ball, ball_m, sub32(x, 0x32), sub32(y, 0x28));
    // drawThoughtBubble(sel, x - 0x32, y - 0x28, -1)
    const uint32_t rec = state_va(ctx, &GameState::g_golfers) + (u(sel) << 8);
    const int32_t t = ctx.mem.load<int16_t>(rec + 0xb6);
    const uint32_t trec = state_va(ctx, &GameState::g_golferTypes) + u(t) * 0x230u;
    const int32_t app = static_cast<int8_t>(ctx.mem.load<uint8_t>(trec + 0x22));
    call(g, ctx, "composeGolferSprite", {u(sel)});
    const int32_t s = static_cast<int32_t>(cm::call_peer(ctx, "typeBit7Clear", {u(sel)}).eax);
    int32_t idx = mul32(3, add32(app, mul32(s, 72)));
    idx = add32(idx, static_cast<int32_t>(cm::call_peer(ctx, "thoughtFlag", {u(sel)}).eax));
    scaled(va(ctx, GSO(g_sprites5628e8, 0)) + 0x2cu * u(idx), sub32(x, 0x32), sub32(y, 0x28));
    gfxs::head_probe(g, ctx, sub32(x, 0x32), sub32(y, 0x28));
  }
  if (sel <= -6 && sel > -0xf) {
    const int32_t k = add32(mul32(sel, 11), 0x42);
    scaled(va(ctx, GSO(g_sprites53df58, 0x2c)) - 16u * u(k), add32(x, 8), sub32(y, 0x14));
  }
  if (sel == -5) stretch(ball, ball_m, sub32(x, 0x28), sub32(y, 0x28));
  if (sel == -4) {
    const int32_t c = gs.g_courseIndex;
    const int32_t v = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(g_courseTable, 0x1c)) + 0x2eu * u(c)));
    const int32_t k = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(f_tab_locations_4c1e90, 0x18)) + 0x82u * u(v)));
    stretch(va(ctx, GSO(g_sprites541710, 0)) + 0x2cu * u(k), va(ctx, GSO(g_sprites5791f8, 0)) + 0x2cu * u(k), add32(x, 8), y);
  }
  y = add32(y, 0x10);
  auto icon = [&](uint32_t off) { stretch(va(ctx, GSO(g_sprites567a20, off)), va(ctx, GSO(g_sprites53de78, off)), x, y); };
  if (sel == -3) icon(0x2c);
  if (sel == -2) icon(0x58);
  if (sel == -0x14) icon(0);
  if (sel == -0x15) icon(0xb0);
  if (sel == -0x16) icon(0x84);
  if (h < 0x60 && !(sel >= 0 && m == 14)) {
    const int32_t d = sub32(sub32(H, h), 0x10);
    y = add32(y, (d - (d >> 31)) >> 1);
  }
  if (sel >= 0 && m == 12) y = add32(y, 1);
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  message_buffer(g, ctx, 0x63, add32(x, 0x64), y, colour);
}

// The whole GameState saved and put back around `f` (a drawing body that runs a registered sim function or fills g_textBuffer
// for a drawer that reads it leaves the GameState as it found it; gfx_hud.cpp scratch_text does the same)
template <class F>
void with_saved_state(Ctx &ctx, F f) {
  GameState &gs = ctx.gs();
  std::vector<uint8_t> saved(sizeof(GameState));
  std::memcpy(saved.data(), &gs, sizeof(GameState));
  try {
    f();
  } catch (...) {
    std::memcpy(&gs, saved.data(), sizeof(GameState));
    throw;
  }
  std::memcpy(&gs, saved.data(), sizeof(GameState));
}
void put_str(Ctx &ctx, uint32_t at, const std::string &s) {
  for (size_t i = 0; i <= s.size(); ++i) ctx.mem.store<uint8_t>(at + static_cast<uint32_t>(i), i < s.size() ? static_cast<uint8_t>(s[i]) : 0);
}

}  // namespace

namespace gfxt {

// showInfoCursorPanel's tip of a tool (its first branch, before the panel itself is drawn), transcribed from the instructions
// (the function is C2: re/analysis/ui/00433190_showInfoCursorPanel.md; log/roadmap/S3/7_asm_showInfoCursorPanel.txt): with the
// tool sel (0..0x13) hovered for more than ten frames, X = clamp(point sel's x + 0x20, 0x50, 0x2d0) (f_tab_point16_4c79b4,
// whose entries 13..15 the function first refreshes from f_tab_rec24_4c7a2c for the course type); drawFrameR3(X - 0x50, 0x192,
// 0xa0, 0x70, 1); the title strip: Surface_stretchBlit of g_sprites5a53c8 + 0x318 over g_sprites58b680 + 0x318 at (X - 0x5c,
// 0x185), then for i = 0..7 the cell k = 1 (2 for i = 7) of the same pair at (X - 0x4c + 16 i, 0x185); the tool's tile type t =
// the signed byte f_tab_bytes_4c79a0[sel], its record r = g_tileTypeAttrs + 0x30 t: the name r + 0, the text
// f_strpool_4c7f1c + 0xc0 and __itoa(100 * the signed byte r + 0x23) with the first byte + 0xe0 (upper case), drawn with
// g_font51b360 by drawCenteredText at (X - 0xc, 0x18a) in 0x80000000; with g_fontRef_519fd8 the rating
// Surface_drawScaled of g_sprites59b778 + 0x2c clamp(2 (4 - the signed byte r + 0x22), 1, 0xa) at (X - 0x5a, 0x187); the
// description: lookupThemeText(the course type's theme table: 0 f_strpool_4c7f1c + 0xa8, 1 + 0x7c, 2 + 0x64, 3 + 0x94; none
// above 3, jump table at the function's end) of the key f_strpool_4c61dc + the name into the emptied g_textBuffer, then
// drawMessageBuffer(0x10, X - 0x46, 0x19a, 0x80007fff); the two meters, Widget_fillRect on the root: (X + 0x40, 0x1a9) to
// (X + 0x40, 0x1d2) in 0x80006318, (X + 0x40, 0x1d2 - 6 b) to (X + 0x40, 0x1d2) in 0x80004210 with b = the signed byte r + 0x20,
// (X + 0x3c, 0x1d2) to (X + 0x44, 0x1d2) in 0x80001284, the marker Image_draw of g_surfaces59b050 + 0x23c at (X + 0x3c,
// 0x1cd - 6 b); c = the byte r + 0x21: c != 0 the bar (X - 0x48, 0x1ea) to (X - 0x48 + (6 << c), 0x1ea) in 0x80004210, and the
// marker at (X - 0x4a + (6 << c), 0x1e2). The texts the original builds in g_textBuffer are built there under a saved state.
namespace {
// the tool tip's description into g_textBuffer (the caller holds the state saved): lookupThemeText of the course type's theme
// table and the key f_strpool_4c61dc + the type's name
void tool_tip_desc(Gfx &g, Ctx &ctx, int32_t ct, const std::string &name, bool report) {
  static const uint32_t kTheme[4] = {0xa8, 0x7c, 0x64, 0x94};
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
  const std::string key = gfxb::vstring(ctx.mem, va(ctx, GSO(f_strpool_4c61dc, 0))) + name;
  put_str(ctx, buf + 0x800, key);
  put_str(ctx, buf, "");
  if (ct >= 0 && ct <= 3) {
    try {   // the theme file through the context's GameFiles; without one the description stays empty (reported)
      cm::call_peer(ctx, "lookupThemeText", {va(ctx, GSO(f_strpool_4c7f1c, kTheme[ct])), buf + 0x800});
    } catch (...) {
      put_str(ctx, buf, "");
      if (report) g.unsup("showInfoCursorPanel tip: lookupThemeText failed (no theme file)");
    }
  }
}
// the tool tip's place (P11B fix 2): X, the box's height, the horizontal meter's row and the shift up dy: the original's 0x70 /
// 0x1ea / 0 (see g_defect_tip_box), else measured from the description (P11B_notes.md)
struct ToolTipLayout {
  int32_t ct = 0, X = 0, t = 0, box_h = 0x70, bar_y = 0x1ea, dy = 0;
  uint32_t rec = 0;
  std::string name;
};
ToolTipLayout tool_tip_layout(Gfx &g, Ctx &ctx, int32_t sel) {
  ToolTipLayout L;
  const uint32_t root = g.root_va(ctx);
  L.ct = static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_courseType)));
  int32_t px = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 4 * sel)));
  if (sel >= 13 && sel <= 15) px = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_rec24_4c7a2c, -4)) + u(24 * L.ct + 8 * (sel - 13)));
  L.X = static_cast<int32_t>(cm::call_peer(ctx, "clamp", {u(add32(px, 0x20)), 0x50, 0x2d0}).eax);
  L.t = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(f_tab_bytes_4c79a0, sel))));
  L.rec = va(ctx, GSO(g_tileTypeAttrs, 0)) + 0x30u * u(L.t);
  L.name = gfxb::vstring(ctx.mem, L.rec);
  if (!SG_DEFECT(g_defect_tip_box)) {
    call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519fd8), 0, 0, 0});  // the description's font
    int32_t text_end = 0x19a;
    with_saved_state(ctx, [&]() {
      tool_tip_desc(g, ctx, L.ct, L.name, false);
      text_end = message_buffer_impl(g, ctx, 0x10, sub32(L.X, 0x46), 0x19a, 0x80007fffu, false);
    });
    L.bar_y = add32(std::max(text_end, 0x1d2), 8);
    L.box_h = (sub32(add32(L.bar_y, 8), 0x192) + 15) & ~15;
    L.dy = std::max(0, sub32(add32(0x192, L.box_h), 0x1e2));
  }
  return L;
}
}  // namespace

int32_t info_panel_tool_tip_dy(Gfx &g, Ctx &ctx, int32_t sel) { return tool_tip_layout(g, ctx, sel).dy; }

void info_panel_tool_tip(Gfx &g, Ctx &ctx, int32_t sel) {
  GameState &gs = ctx.gs();
  const uint32_t root = g.root_va(ctx);
  const ToolTipLayout lay = tool_tip_layout(g, ctx, sel);
  const int32_t ct = lay.ct, X = lay.X, box_h = lay.box_h, bar_y = lay.bar_y, dy = lay.dy;
  const uint32_t rec = lay.rec;
  const std::string &name = lay.name;
  auto fill = [&](bool report) { tool_tip_desc(g, ctx, ct, name, report); };
  call(g, ctx, "drawFrameR3", {u(sub32(X, mode() == 8 ? 0x4f : 0x50)), u(sub32(0x192, dy)), 0xa0, u(box_h), 1});
  const uint32_t sp = va(ctx, GSO(g_sprites5a53c8, 0x318)), mk = va(ctx, GSO(g_sprites58b680, 0x318));
  call(g, ctx, "Surface_stretchBlit", {sp, mk, root, u(sub32(X, 0x5c)), u(sub32(0x185, dy)), 0});
  for (int32_t i = 0; i <= 7; ++i) {
    const uint32_t k = i == 7 ? 2u : 1u;
    call(g, ctx, "Surface_stretchBlit", {sp + 0x2cu * k, mk + 0x2cu * k, root, u(add32(sub32(X, 0x4c), 16 * i)), u(sub32(0x185, dy)), 0});
  }
  std::string title = name + gfxb::vstring(ctx.mem, va(ctx, GSO(f_strpool_4c7f1c, 0xc0))) +
                      std::to_string(mul32(static_cast<int8_t>(ctx.mem.load<uint8_t>(rec + 0x23)), 100));
  title[0] = static_cast<char>(static_cast<uint8_t>(title[0]) + 0xe0);
  if (lang::language() != lang::kEnglish && !name.empty()) {  // P24D: the tile's name (first letter raised, as above) translated
    std::string cap = name;
    cap[0] = static_cast<char>(static_cast<uint8_t>(cap[0]) + 0xe0);
    title = lang::tr(cap) + title.substr(name.size());
  }
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  draw_string(g, ctx, title, sub32(X, 0xc), sub32(0x18a, dy), 0x80000000u, true, "showInfoCursorPanel tip");
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519fd8), 0, 0, 0});
  const int32_t rate = static_cast<int32_t>(
      cm::call_peer(ctx, "clamp", {u(mul32(sub32(4, static_cast<int8_t>(ctx.mem.load<uint8_t>(rec + 0x22))), 2)), 1, 0xa}).eax);
  call(g, ctx, "Surface_drawScaled",
       {va(ctx, GSO(g_sprites59b778, 0)) + 0x2cu * u(rate), root, u(sub32(X, 0x5a)), u(sub32(0x187, dy)), 1, 1, 1, 0});
  with_saved_state(ctx, [&]() {
    fill(true);
    message_buffer(g, ctx, 0x10, sub32(X, 0x46), sub32(0x19a, dy), 0x80007fffu);
  });
  const int32_t b = static_cast<int8_t>(ctx.mem.load<uint8_t>(rec + 0x20));
  const uint32_t win = state_va(ctx, &GameState::g_rootWindow) + 0x274u;
  auto line = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t c) {
    call(g, ctx, "Widget_fillRect", {win, u(x1), u(sub32(y1, dy)), u(x2), u(sub32(y2, dy)), c});
  };
  line(add32(X, 0x40), 0x1a9, add32(X, 0x40), 0x1d2, 0x80006318u);
  line(add32(X, 0x40), sub32(0x1d2, mul32(6, b)), add32(X, 0x40), 0x1d2, 0x80004210u);
  line(add32(X, 0x3c), 0x1d2, add32(X, 0x44), 0x1d2, 0x80001284u);
  const uint32_t marker = va(ctx, GSO(g_surfaces59b050, 0x23c));
  call(g, ctx, "Image_draw", {marker, root, u(add32(X, 0x3c)), u(sub32(sub32(0x1cd, mul32(6, b)), dy)), 0});
  const uint32_t c = ctx.mem.load<uint8_t>(rec + 0x21);
  const int32_t bar = static_cast<int32_t>(6u << (c & 31));
  if (c != 0) line(sub32(X, 0x48), bar_y, add32(sub32(X, 0x48), bar), bar_y, 0x80004210u);
  call(g, ctx, "Image_draw", {marker, root, u(add32(sub32(X, 0x4a), bar)), u(sub32(sub32(bar_y, 8), dy)), 0});
  (void)gs;
}

namespace {
std::string pool(Ctx &ctx, uint32_t off) { return gfxb::vstring(ctx.mem, va(ctx, off)); }
// drawPanelBackdrop(x, y) over a text the original copies into g_textBuffer (the core body reads the buffer)
void backdrop(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y) {
  with_saved_state(ctx, [&]() {
    put_str(ctx, state_va(ctx, &GameState::g_textBuffer), text);
    call(g, ctx, "drawPanelBackdrop", {u(x), u(y)});
  });
}
// drawMapLine(x0, y, x1, y, 0x80000000, 10, 5): Terrain::drawLine's OpenGL bar (roadmap S3.8's line passes): drawn by that
// body when the tree holds one, else reported as the frame code reports it
void map_bar(Gfx &g, Ctx &ctx, int32_t x0, int32_t x1, int32_t y) {
  if (rec().on || gfxb::find_body("drawMapLine")) call(g, ctx, "drawMapLine", {u(x0), u(y), u(x1), u(y), 0x80000000u, 10, 5});
  else g.unsup("no body: drawMapLine");
}
// drawCenteredText(text, x, y, 0x80007fff) over a text the original holds in g_textBuffer
void centred(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y) {
  with_saved_state(ctx, [&]() {
    const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
    put_str(ctx, buf, text);
    call(g, ctx, "drawCenteredText", {buf, u(x), u(y), 0x80007fffu});
  });
}
std::string upper_first(std::string s) {
  if (!s.empty()) s[0] = static_cast<char>(static_cast<uint8_t>(s[0]) + 0xe0);
  return s;
}
}  // namespace

// showInfoCursorPanel's tip of a slot (sel >= 0x14, after the panel; the instructions after its tool loop): the record r of the
// signed byte f_tab_bytes_4c79a0[sel]; the text = r's name (first byte + 0xe0) + f_strpool_4c7f1c + 0x24 + __itoa(100 r + 0x23),
// replaced by f_strpool_4c7f1c + 0x18 / + 0x10 / + 0 for sel 0x14 / 0x15 / 0x16; with (px, py) the words of
// f_tab_point16_4c79b4 + 4 sel and n = strlen: the bar drawMapLine(px + 0x14 - 3 n, py + 1, px + 0x14 + 3 n, py + 1) and
// drawCenteredText(text, px + 0x14, py - 4, 0x80007fff)
void info_panel_slot_tip(Gfx &g, Ctx &ctx, int32_t sel) {
  const int32_t t = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(f_tab_bytes_4c79a0, sel))));
  const uint32_t rec = va(ctx, GSO(g_tileTypeAttrs, 0)) + 0x30u * u(t);
  std::string text = upper_first(gfxb::vstring(ctx.mem, rec)) + pool(ctx, GSO(f_strpool_4c7f1c, 0x24)) +
                     std::to_string(mul32(static_cast<int8_t>(ctx.mem.load<uint8_t>(rec + 0x23)), 100));
  if (sel == 0x14) text = pool(ctx, GSO(f_strpool_4c7f1c, 0x18));
  else if (sel == 0x15) text = p13a_bulldoze() ? std::string("Bulldoze") : pool(ctx, GSO(f_strpool_4c7f1c, 0x10));  // P13A
  else if (sel == 0x16) text = pool(ctx, GSO(f_strpool_4c7f1c, 0));
  const int32_t px = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 4 * sel)));
  const int32_t py = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 4 * sel + 2)));
  text = lang::tr(text);  // P24D fix 5: the bar spans the text in the current language
  const int32_t n3 = mul32(static_cast<int32_t>(text.size()), 3);
  map_bar(g, ctx, sub32(add32(px, 0x14), n3), add32(add32(px, 0x14), n3), add32(py, 1));
  centred(g, ctx, text, add32(px, 0x14), sub32(py, 4));
}

// drawTerrainToolbar's tip (c3ei.cpp DrawTerrainToolbar_re): by sel + 3 the text f_strpool_4c7f1c + 0x10 (-3), + 0x114 (-2),
// + 0x100 (0), + 0xec (1), + 0xd8 (2), + 0xc4 (3), else empty; drawPanelBackdrop(a, b - 5)
void terrain_toolbar_tip(Gfx &g, Ctx &ctx, int32_t sel, int32_t a, int32_t b) {
  static const int32_t kOff[7] = {0x10, 0x114, -1, 0x100, 0xec, 0xd8, 0xc4};
  const uint32_t k = u(add32(sel, 3));
  std::string text = k <= 6 && kOff[k] >= 0 ? pool(ctx, GSO(f_strpool_4c7f1c, kOff[k])) : std::string();
  if (k == 0 && p13a_bulldoze()) text = "Bulldoze";  // P13A: -3 is the bulldozer
  backdrop(g, ctx, text, a, sub32(b, mode() == 10 ? 4 : 5));
}

// ---- P11C (log/roadmap/P11/P11C_notes.md, Mariano's item 3: "a hover indicator over the terrain saying if it does something")
// The help line of tile type t: its record's name (first letter upper case, as showInfoCursorPanel's tip) and short clauses
// read from the LIVE record of g_tileTypeAttrs (loadWorldAssets renames records and patches the rough's lie per course type), in
// this order while they fit kHoverWords words: what the type is for (tees, green, water, out of bounds, the trees' class 13),
// the lie (+0x22: prepareShot turns a shot from lie > 0 by Random * lie, so <= 0 clean, 1 harder, 2..3 hard, more very hard),
// the golfers (updateGolfers: a landing on lie > 0 is thought 2 / 3, mood -1; a good landing on lie <= 0 is thought 1, +1), the
// ball (+0x21 roll: trialShot's ground loss (speed >> roll) / 2, 4 and up long, 2 less, 0..1 stops fast; +0x20 bounce: 5 and
// up high), the clearing price (+0x24: mainLoop's paint price adds it, 0x32 and up the warning sound). Numbers are never shown.
namespace {
constexpr int kHoverWords = 8;
constexpr int32_t kHoverToolY = 0x17b;  // the tool line's bar centre: its band (y - 5 .. y + 5) ends above the title strip 0x185
                                        // (P11B fix 2: the strip is at 0x185 - dy, info_panel_tool_tip_dy; the line follows it)
constexpr int32_t kHoverSlotRise = 0xc;   // fix 2: a slot line's bar centre above the slot point (the slot tip's text starts at y - 4)
constexpr int32_t kHoverAmenityRise = 2;  // fix 2: an amenity line's bar centre above the item point (amenities_tip's text starts at y + 5)
int words(const std::string &s) {
  int n = 0;
  bool in = false;
  for (char c : s) {
    const bool sp = c == ' ';
    if (!sp && !in) ++n;
    in = !sp;
  }
  return n;
}
}  // namespace

std::string hover_help_line(Ctx &ctx, int32_t t) {
  if (t < 0 || t > 22) return std::string();
  const uint32_t rec = va(ctx, GSO(g_tileTypeAttrs, 0)) + 0x30u * u(t);
  std::string name = gfxb::vstring(ctx.mem, rec);
  if (name.empty()) return std::string();
  if (name[0] >= 'a' && name[0] <= 'z') name[0] = static_cast<char>(name[0] - 0x20);
  auto sb = [&](uint32_t off) { return static_cast<int32_t>(static_cast<int8_t>(ctx.mem.load<uint8_t>(rec + off))); };
  const int32_t bounce = sb(0x20), roll = sb(0x21), lie = sb(0x22), clear = sb(0x24);
  const int32_t cls = static_cast<int32_t>(ctx.mem.load<uint8_t>(rec + 0x26));
  const bool penalty = t == 0x11 || t == 20;
  std::vector<std::string> c;
  if (t == 0) c.push_back("a hole starts here");
  if (t == 1) c.push_back("the cup goes here");
  if (t == 0x11) c.push_back("penalty stroke and a drop");
  if (t == 20) c.push_back("penalty stroke, shot replayed");
  if (cls == 13) c.push_back("balls can hit it");
  if (!penalty) {
    if (lie < 0) c.push_back("putting surface");
    else if (lie == 0) c.push_back("clean lie");
    else if (lie == 1) c.push_back("harder shots");
    else if (lie <= 3) c.push_back("hard shots");
    else c.push_back("very hard shots");
    if (lie > 0) c.push_back("upsets golfers");
    else if (t != 0 && t != 1) c.push_back("pleases golfers");
    if (roll >= 4) c.push_back("long roll");
    else if (roll == 2) c.push_back("less roll");
    else if (roll <= 1) c.push_back("ball stops fast");
    if (bounce >= 5) c.push_back("high bounce");
  }
  if (clear >= 0x32) c.push_back("very costly to clear");
  else if (clear > 0) c.push_back("costly to clear");
  std::string out;
  int n = 0;
  for (const std::string &s : c) {
    const int w = words(s);
    if (n + w > kHoverWords) continue;
    out += out.empty() ? lang::tr(s) : ", " + lang::tr(s);  // P24D: each clause and the name in the current language
    n += w;
  }
  return out.empty() ? lang::tr(name) : lang::tr(name) + ": " + out;
}

// the palette tool sel's line, drawn the way the game's one-line tips are (drawPanelBackdrop: the dark bar and the centred text
// in g_fontRef_519fd8), centred on showInfoCursorPanel's tip X and just above its title strip (y 0x185 - dy, P11B fix 2)
void hover_help_tool(Gfx &g, Ctx &ctx, int32_t sel) {
  if (sel < 0 || sel >= 0x14) return;
  const int32_t t = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(f_tab_bytes_4c79a0, sel))));
  const std::string text = hover_help_line(ctx, t);
  if (text.empty()) return;
  const ToolTipLayout lay = tool_tip_layout(g, ctx, sel);  // the tip's X and P11B's shift up dy
  backdrop(g, ctx, text, lay.X, sub32(kHoverToolY, lay.dy));
}

// ---- P11C fix 2: the lines of the items that show only a name (log/roadmap/P11/P11C_notes.md section 3). Fixed texts, each one
// the effect section 3 cites; the item's name is the original's own tip text (amenities_tip / info_panel_slot_tip), which this
// line follows with ": ".
// showInfoCursorPanel's slots (slot_action): 0x14 Info Cursor clears the tool, 0x15 Undo opens the help window about the right-
// click undo, 0x16 Improvements opens the amenities palette (g_buildPanelSlot = 1)
std::string hover_help_slot_text(int32_t sel) {
  switch (sel) {
    case 0x14: return "drops the tool; tiles show what they do";
    case 0x15: return p13a_bulldoze() ? "removes the last edit or building you click" : "right click the map to undo an edit";
    case 0x16: return "paths, benches, trees, landmarks and more";
    default: return std::string();
  }
}
// what amenity item L changes (section 3's table; "" = no line)
// (L 6, Scenic Trees, places the elm tree tile type 16: its clauses are that record's, hover_help_line)
std::string amenity_effect(Ctx &ctx, int32_t L) {
  switch (L) {
    case 0: return "cleaned balls curve less on the next shot";  // ball washer: flag 0x4000000, prepareShot spin -= spin / 3
    case 1: return "golfers walk faster and tire less";              // updateGolfers: step 6 on a path, energy + 1 (else +0x25)
    case 2: return "earns cash; worth more near good holes";         // home site: rateLot / 4 on placing, value grows by rateLot
    case 3: return "tired golfers rest here and recover energy";     // benches: energy counter +0xaa back to 0
    case 4: return "golfers walking by are pleased";                 // landmark id < 0x10 beside a golfer: thought 0xb, +1
    case 5: return "golfers walking by are pleased";                 // flower bed beside a golfer: thought 0xb, +1
    case 6: {
      const std::string l = hover_help_line(ctx, 16);
      const size_t k = l.find(": ");
      return k == std::string::npos ? std::string() : l.substr(k + 2);
    }
    case 9: return "golfers walk over water; crossing pleases them";  // bridge: path over type 0x11, thought 7, +1
    default: return std::string();
  }
}
// drawAmenitiesPanel's items L (amenities_tip's names: -2 Course Terrain, 0 Ballwasher, 1 Pathway, 2 Building Lot, 3 Benches,
// 4 Landmarks, 5 Flower Bed, 6 Scenic Trees, 7 / 8 Undo, 9 Scenic Bridge)
std::string hover_help_amenity_text(Ctx &ctx, int32_t L) {
  switch (L) {
    case -2: return "back to the terrain tools";
    case 7:
    case 8: return p13a_bulldoze() ? "removes the last edit or building you click" : "right click the map to undo an edit";
    default: return amenity_effect(ctx, L);
  }
}

// the slot sel's line above showInfoCursorPanel's slot tip (that tip's bar is at the slot point's y + 1, its text at y - 4)
void hover_help_slot(Gfx &g, Ctx &ctx, int32_t sel) {
  static const uint32_t kName[3] = {0x18, 0x10, 0};  // info_panel_slot_tip's names (f_strpool_4c7f1c)
  const std::string effect = hover_help_slot_text(sel);
  if (effect.empty()) return;
  const std::string name = sel == 0x15 && p13a_bulldoze() ? std::string("Bulldoze") : pool(ctx, GSO(f_strpool_4c7f1c, kName[sel - 0x14]));  // P13A
  const std::string text = lang::tr(name) + ": " + lang::tr(effect);  // P24D
  const int32_t px = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 4 * sel)));
  const int32_t py = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 4 * sel + 2)));
  backdrop(g, ctx, text, add32(px, 0x14), sub32(py, kHoverSlotRise));
}
// the amenity item L's line above amenities_tip (that tip's bar is at the item point's y + 0xa, at the cursor (a, b) for L -2)
void hover_help_amenity(Gfx &g, Ctx &ctx, int32_t L, int32_t a, int32_t b) {
  const std::string effect = hover_help_amenity_text(ctx, L);
  if (effect.empty()) return;
  // amenities_tip's names: f_strpool_4c80d0 offsets by L + 2 (L 7 / 8: f_strpool_4c7f1c + 0x10)
  static const int32_t kName[12] = {0x1a0, -1, 0x194, 0x18c, 0x17c, 0x174, 0x168, 0x15c, 0x14c, -1, -1, 0x13c};
  const uint32_t ci = u(add32(L, 2));
  if (ci > 11) return;
  std::string name = kName[ci] >= 0 ? pool(ctx, GSO(f_strpool_4c80d0, kName[ci])) : pool(ctx, GSO(f_strpool_4c7f1c, 0x10));
  if ((ci == 9 || ci == 10) && p13a_bulldoze()) name = "Bulldoze";  // P13A
  const std::string text = lang::tr(name) + ": " + lang::tr(effect);  // P24D
  int32_t x = a, y = b;
  if (L >= 0) {
    x = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7a88, 0x10 + 4 * L)));
    y = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7a88, 0x12 + 4 * L)));
  }
  backdrop(g, ctx, text, add32(x, 0x14), sub32(y, kHoverAmenityRise));
}

// the tile under the cursor (cx, cy) with no tool: after the game's own dwell (the frames the HUD tips wait, hover_tip_dwell)
// on one tile, its line above the cursor (below it near the top edge). The no-tool rule is P6_Cursor's (its distance line: no
// tool and no object tool, or g_panelListMode 2 / 5; here also 0, the Build palette open without a tool) and nothing shows
// while that line is drawn (a drag: g_rootInput word 13 != -1), with the cursor over the bottom HUD (y >= 0x1e0, the original's
// cursor labels' rule) or over a pixel of a HUD panel layer of the last published frame (with the app's layers), a build panel
// up, the original's own label shown (g_flags bit 27, P6_ToolLabel), or a building or placed object under it (their labels).
static bool over_hud_layer(const Gfx &g, int32_t x, int32_t y) {
  for (const GfxHudLayer &l : g.hud.published.layers) {
    if (l.id == kGfxLayerCentred || x < l.x0 || x >= l.x1 || y < l.y0 || y >= l.y1) continue;
    const size_t i = static_cast<size_t>(y - l.y0) * static_cast<size_t>(l.x1 - l.x0) + static_cast<size_t>(x - l.x0);
    if (i < l.px.size() && l.px[i].kind != 0) return true;
  }
  return false;
}
void hover_help_terrain(Gfx &g, Ctx &ctx, int32_t cx, int32_t cy) {
  struct Dwell {
    int32_t x = -1, y = -1, count = 0;
  };
  Dwell &d = g.state_of<Dwell>("text.hoverHelp");
  GameState &gs = ctx.gs();
  const int32_t tx = gs.g_cursorTileX, ty = gs.g_cursorTileY, mode = gs.g_panelListMode;
  const bool open = cy < 0x1e0 && cy >= 0 && gs.f_w_4c2848[0] == -1 && gs.f_w_4c2848[3] == -1 && gs.g_buildPanelSlot == 0 &&
                    (mode == 0 || mode == 2 || mode == 5) && gs.g_rootInput[13] == -1 && (gs.g_flags & (1u << 27)) == 0 &&
                    !over_hud_layer(g, cx, cy);
  if (!open) {
    d = Dwell{};
    return;
  }
  if (tx != d.x || ty != d.y) {
    d.x = tx;
    d.y = ty;
    d.count = 0;
    return;
  }
  if (++d.count <= hover_tip_dwell()) return;
  int32_t t = 20;
  if (cm::call_peer(ctx, "tileBlocked", {u(tx), u(ty)}).eax == 0) {
    const uint32_t cell = u(add32(mul32(tx, 50), ty));
    t = static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_tileType) + cell));
    const uint16_t fl = ctx.mem.load<uint16_t>(state_va(ctx, &GameState::g_tileFlags) + 2u * cell);
    if (t == 0x15 || t == 0x16 || (fl & (1u << 10)) != 0) return;
  }
  const std::string text = hover_help_line(ctx, t);
  if (text.empty()) return;
  // P23F fix 2 (gfx.h gfx_set_frame_edge_text): in a wide window a cursor near or past the frame's edge keeps its tip at the
  // cursor (drawPanelBackdrop holds it inside the window's columns, its bar and text are drawn past the frame's edge)
  const int32_t w3 = mul32(static_cast<int32_t>(text.size()), 3);
  struct TipScope {
    Gfx &g;
    explicit TipScope(Gfx &gg, bool on) : g(gg) { g.hud.tip_edge = on; }
    ~TipScope() { g.hud.tip_edge = false; }
  } tip(g, g.hud.edge_frame && g.hud.wide_e > 0 && (cx < w3 || cx > sub32(0x320, w3)));
  backdrop(g, ctx, text, cx, cy < 0x30 ? add32(cy, 0x24) : sub32(cy, 0x12));
}

// drawGolferActionPanel's tip (c3fb.cpp DrawGolferActionPanel_re): by hit - 1 the text f_strpool_4c80d0 + 0x29c, + 0x294
// followed by the challenger pro's name (g_golfPros + 0x38 g_proChallenger) or + 0x288 when there is none, + 0x274, + 0x264,
// + 0x250, + 0x23c, + 0x228, + 0x218, f_strpool_4c5d20 + 0xa8, f_strpool_4c80d0 + 0x208; drawPanelBackdrop(x, y - 5)
void action_panel_tip(Gfx &g, Ctx &ctx, int32_t hit, int32_t x, int32_t y) {
  std::string text;
  switch (hit - 1) {
    case 0: text = pool(ctx, GSO(f_strpool_4c80d0, 0x29c)); break;
    case 1: {
      text = pool(ctx, GSO(f_strpool_4c80d0, 0x294));
      const int32_t p = ctx.gs().g_proChallenger;
      text += p != -1 ? pool(ctx, GSO(g_golfPros, 0) + 0x38u * u(p)) : pool(ctx, GSO(f_strpool_4c80d0, 0x288));
      break;
    }
    case 2: text = pool(ctx, GSO(f_strpool_4c80d0, 0x274)); break;
    case 3: text = pool(ctx, GSO(f_strpool_4c80d0, 0x264)); break;
    case 4: text = pool(ctx, GSO(f_strpool_4c80d0, 0x250)); break;
    case 5: text = pool(ctx, GSO(f_strpool_4c80d0, 0x23c)); break;
    case 6: text = pool(ctx, GSO(f_strpool_4c80d0, 0x228)); break;
    case 7: text = pool(ctx, GSO(f_strpool_4c80d0, 0x218)); break;
    case 8: text = pool(ctx, GSO(f_strpool_4c5d20, 0xa8)); break;
    case 9: text = pool(ctx, GSO(f_strpool_4c80d0, 0x208)); break;
    default: break;
  }
  backdrop(g, ctx, text, x, sub32(y, mode() == 10 ? 4 : 5));
}

// drawEmployeeViewPanel's tip (c3fb.cpp DrawEmployeeViewPanel_re): -4 f_strpool_4c80d0 + 0x1f8, -3 the golfer type record 0's
// text (g_golferTypes + 0x10), -2 f_strpool_4c5d20 + 0xa8, else empty; n = 3 strlen: the bar drawMapLine(x - n, y - 5, x + n,
// y - 5) and drawCenteredText(text, x, y - 10, 0x80007fff)
void meter_tip(Gfx &g, Ctx &ctx, const std::string &text, int32_t x, int32_t y) { backdrop(g, ctx, text, x, y); }  // P22D fix 3

void view_panel_tip(Gfx &g, Ctx &ctx, int32_t hover, int32_t x, int32_t y) {
  std::string text;
  if (hover == -4) text = pool(ctx, GSO(f_strpool_4c80d0, 0x1f8));
  else if (hover == -3) text = pool(ctx, GSO(g_golferTypes, 0x10));
  else if (hover == -2) text = pool(ctx, GSO(f_strpool_4c5d20, 0xa8));
  text = lang::tr(text);  // P24D fix 5
  const int32_t n3 = mul32(static_cast<int32_t>(text.size()), 3);
  map_bar(g, ctx, sub32(x, n3), add32(x, n3), sub32(y, 5));
  centred(g, ctx, text, x, sub32(y, 10));
}

// drawEmployeeDetailPanel's tip (c3fc.cpp DrawEmployeeDetailPanel_re): by hit + 3 (jump table at the function's end) the text
// g_golferTypes + 0x10, f_strpool_4c5d20 + 0xa8, none, f_strpool_4c80d0 + 0x310, + 0x300, none, none, + 0x2ec, + 0x2c0,
// + 0x2d4; drawPanelBackdrop(x, y - 5)
void detail_panel_tip(Gfx &g, Ctx &ctx, int32_t hit, int32_t x, int32_t y) {
  std::string text;
  switch (hit + 3) {
    case 0: text = pool(ctx, GSO(g_golferTypes, 0x10)); break;
    case 1: text = pool(ctx, GSO(f_strpool_4c5d20, 0xa8)); break;
    case 3: text = pool(ctx, GSO(f_strpool_4c80d0, 0x310)); break;
    case 4: text = pool(ctx, GSO(f_strpool_4c80d0, 0x300)); break;
    case 7: text = pool(ctx, GSO(f_strpool_4c80d0, 0x2ec)); break;
    case 8: text = pool(ctx, GSO(f_strpool_4c80d0, 0x2c0)); break;
    case 9: text = pool(ctx, GSO(f_strpool_4c80d0, 0x2d4)); break;
    default: break;
  }
  backdrop(g, ctx, text, x, sub32(y, mode() == 10 ? 4 : 5));
}

// drawAmenitiesPanel's dwell tip (c3ei.cpp DrawAmenitiesPanel_re): by L + 2 (L -2 .. 9, jump table at the function's end) the
// text and value: -2 f_strpool_4c80d0 + 0x1a0 at the point (a, b), 0 + 0x194 / the word of f_tab_buildItems_4c26b0 + 0x4e,
// 1 + 0x18c / + 0x12, 2 + 0x17c / + 0x76, 3 + 0x174 / + 0x26, 4 + 0x168 / + 0x62, 5 + 0x15c / + 0x3a, 6 + 0x14c / the signed byte
// g_tileTypeAttrs + 0x323, 7 and 8 f_strpool_4c7f1c + 0x10 / none, 9 f_strpool_4c80d0 + 0x13c / + 0x18e; (x, y) the words of
// f_tab_point16_4c7a88 + 0x10 + 4 L; n = 3 strlen: the bar at (x + 0x14 -+ n, y + 0xa), drawCenteredText(text, x + 0x14, y + 5);
// a value v != 0: a second bar at y + 0x14 and drawCenteredText(the decimal of 100 v, or f_strpool_4c80d0 + 0x130 for L 4,
// x + 0x14, y + 0xf)
void amenities_tip(Gfx &g, Ctx &ctx, int32_t L, int32_t a, int32_t b) {
  struct Case {
    int32_t str;   // f_strpool_4c80d0 offset, or -0x100 - off for f_strpool_4c7f1c, or -1 none
    int32_t src;   // f_tab_buildItems_4c26b0 word offset, -2 the tile-type byte, -1 none
  };
  static const Case kCase[12] = {{0x1a0, -1}, {-1, -1},   {0x194, 0x4e}, {0x18c, 0x12}, {0x17c, 0x76},      {0x174, 0x26},
                                 {0x168, 0x62}, {0x15c, 0x3a}, {0x14c, -2}, {-0x110, -1}, {-0x110, -1}, {0x13c, 0x18e}};
  int32_t x = 0, y = 0, v = 0;
  std::string text;
  const uint32_t ci = u(add32(L, 2));
  if (L >= 0) {
    x = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7a88, 0x10 + 4 * L)));
    y = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7a88, 0x12 + 4 * L)));
  }
  if (ci <= 0xb) {
    const Case &c = kCase[ci];
    if (ci == 0) {
      x = a;
      y = b;
    }
    if (c.str >= 0) text = pool(ctx, GSO(f_strpool_4c80d0, c.str));
    else if (c.str <= -0x100) text = pool(ctx, GSO(f_strpool_4c7f1c, -0x100 - c.str));
    if (c.str == -0x110 && p13a_bulldoze()) text = "Bulldoze";  // P13A: items 7 / 8 are the bulldozer
    if (c.src >= 0) v = ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_buildItems_4c26b0, c.src)));
    else if (c.src == -2) v = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(g_tileTypeAttrs, 0x323))));
  }
  text = lang::tr(text);  // P24D fix 5
  const int32_t n3 = mul32(static_cast<int32_t>(text.size()), 3);
  const int32_t x0 = add32(sub32(x, n3), 0x14), x1 = add32(add32(n3, x), 0x14);
  map_bar(g, ctx, x0, x1, add32(y, 0xa));
  centred(g, ctx, text, add32(x, 0x14), add32(y, 5));
  if (v == 0) return;
  map_bar(g, ctx, x0, x1, add32(y, 0x14));
  text = L == 4 ? pool(ctx, GSO(f_strpool_4c80d0, 0x130)) : std::to_string(mul32(v, 100));
  centred(g, ctx, text, add32(x, 0x14), add32(y, mode() == 11 ? 0x10 : 0xf));
}

// drawAmenitiesPanel's hovered building row in mode 4 (c3ei.cpp DrawAmenitiesPanel_re): r = the row; the text = landmarkName(r,
// 0) with its first byte lowered by 0x20 when it is above 'Z' (signed), f_strpool_4c528c + 0x2c, then f_strpool_4c80d0 + 0x1f0
// when bit r of g_freeUnlockMask is set, else the decimal of 1000 r + 5000; drawPanelBackdrop(a, b - 0xc); then the title of
// r & 3 (f_strpool_4c80d0 + 0x1e0 / + 0x1d0 / + 0x1c0 / + 0x1b0) and drawPanelBackdrop(a, b - 2)
void amenities_row_tip(Gfx &g, Ctx &ctx, int32_t r, int32_t a, int32_t b) {
  std::string name;
  with_saved_state(ctx, [&]() {
    const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
    put_str(ctx, buf, "");
    cm::call_peer(ctx, "landmarkName", {u(r), 0});
    name = gfxb::vstring(ctx.mem, buf);
  });
  if (!name.empty() && static_cast<int8_t>(name[0]) > 0x5a) name[0] = static_cast<char>(static_cast<uint8_t>(name[0]) + 0xe0);
  std::string text = name + pool(ctx, GSO(f_strpool_4c528c, 0x2c));
  if (ctx.gs().g_freeUnlockMask & (1u << (u(r) & 31))) text += pool(ctx, GSO(f_strpool_4c80d0, 0x1f0));
  else text += std::to_string(add32(mul32(add32(mul32(r, 5), 0x19), 200), 0));
  backdrop(g, ctx, text, a, sub32(b, 0xc));
  static const int32_t kTitle[4] = {0x1e0, 0x1d0, 0x1c0, 0x1b0};
  backdrop(g, ctx, pool(ctx, GSO(f_strpool_4c80d0, kTitle[r & 3])), a, sub32(b, 2));
}

// drawStringRightAt(this, text, x, y, len) (c3df.cpp DrawString_re): a NULL text, no base font or n = min(strlen, len) <= 0
// draws nothing; drawRichText(text, x - drawMarkupRun(text, n), y, n) in the target's state-0 colours (Widget_setQuad).
// Roadmap S3.13: the only drawStringRightAt body (the results family's, S2.11, measured the run with the provider's plain
// glyph widths and drew through drawText; this one is the original's drawRichText / drawMarkupRun pair). Its one caller,
// showTournamentResults (tick_panels.inc), draws on the root surface after Widget_setQuad(root, 0x80001284, -1, 2, 2).
void string_right_at(Gfx &g, Ctx &ctx, const uint32_t *a) {
  g.ensure_root(ctx);
  if (a[1] == 0) return;
  if (a[0] != g.root_va(ctx)) {
    g.unsup("drawStringRightAt: a target other than the root surface");
    return;
  }
  const uint32_t font = g.fonts[0];
  if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) return;
  const uint32_t tva = lang::redirect(ctx, a[1]);  // P24D: the current language's text, whole (the count is the English one's)
  const std::string s = gfxb::vstring(ctx.mem, tva);
  int32_t n = static_cast<int32_t>(s.size());
  if (tva == a[1] && static_cast<int32_t>(a[4]) < n) n = static_cast<int32_t>(a[4]);
  if (n <= 0) return;
  TextObj &o = root_obj(g, ctx);
  const int32_t w = markup_width(g, ctx, s, n);
  draw_rich(g, ctx, s, add32(sub32(static_cast<int32_t>(a[2]), w), mode() == 15 ? 1 : 0), static_cast<int32_t>(a[3]), n, o.colour[0], o.shadow[0],
            static_cast<int32_t>(o.dx[0]), static_cast<int32_t>(o.dy[0]), "drawStringRightAt");
}

}  // namespace gfxt

namespace {

void b_string_right_at(Gfx &g, Ctx &ctx, const uint32_t *a, int) { gfxt::string_right_at(g, ctx, a); }

// Text_put(this, text) (c3df.cpp TextPut_re) as a frame-code drop: no pixels; its width (drawMarkupRun(text, strlen) in the
// root's current fonts; a NULL text 0) is handed back to the frame code (gfx_take_value). A target other than the root is
// reported.
void b_text_put(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (a[0] != g.root_va(ctx)) {
    g.unsup("Text_put: a target other than the root surface");
    return;
  }
  g.has_value = true;
  g.value = a[1] == 0 ? 0 : gfxt::text_put(g, ctx, gfxb::vstring(ctx.mem, lang::redirect(ctx, a[1])));  // P24D
  if (mode() == 5) g.value = add32(g.value, 8);
}

void b_window_frame(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  window_frame(g, ctx, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
               static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
}

void b_set_quad_k(Gfx &g, const uint32_t *a, int k) { gfxt::set_quad(g, a[0], k, a[1], a[2], a[3], a[4]); }
void b_set_quad70(Gfx &g, Ctx &, const uint32_t *a, int) { b_set_quad_k(g, a, 1); }
void b_set_quad74(Gfx &g, Ctx &, const uint32_t *a, int) { b_set_quad_k(g, a, 2); }
void b_set_quad78(Gfx &g, Ctx &, const uint32_t *a, int) { b_set_quad_k(g, a, 3); }

// gfx_text.test(op, ...): TEST-ONLY entry points of port/test/test_gfx_text.py. op 1 record on (log cleared, the root's text
// state reset to Image_ctor's), 2 record off and the log into `$gfx/text_calls.txt`; the original functions over the root:
// 3 drawTextWithColor(text, x, y, colour), 4 drawTextSurface(text, x, y, colour), 5 drawCenteredText(text, x, y, colour),
// 6 drawCenteredText2(text, x, y, colour), 7 drawMessageBuffer(cols, x, y, colour), 8 Text_put(text) (logs `width w`),
// 9 drawWindow(x, y, mode, h) after the caller stored H
void b_test(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  auto str = [&](uint32_t v) { return v ? gfxb::vstring(ctx.mem, v) : std::string(); };
  auto I = [&](int k) { return static_cast<int32_t>(a[k]); };
  switch (a[0]) {
    case 1:
      rec().on = true;
      rec().log.clear();
      st(g).obj[g.root_va(ctx)] = TextObj{};
      break;
    case 2: {
      rec().on = false;
      std::string out;
      for (const auto &l : rec().log) out += l + "\n";
      gfxb::put_file(ctx, "$gfx/text_calls.txt", std::vector<uint8_t>(out.begin(), out.end()));
      break;
    }
    case 3: if (a[1]) draw_text_q(g, ctx, str(a[1]), I(2), I(3), a[4], 0xffffffffu, 2, 2, "test"); break;
    case 4: if (a[1]) text_surface(g, ctx, str(a[1]), I(2), I(3), a[4]); break;
    case 5:
    case 6: {
      if (!a[1]) break;
      const uint32_t font = g.fonts[0];
      if (font == 0 || ctx.mem.load<uint32_t>(font + 4) == 0) break;
      const std::string s = str(a[1]);
      if (s.empty()) break;
      const int32_t n = static_cast<int32_t>(s.size());
      const int32_t x = add32(I(2), (0 - gfxt::markup_width(g, ctx, s, n)) / 2);
      if (a[0] == 5) gfxt::draw_rich(g, ctx, s, x, I(3), n, a[4], 0xffffffffu, 2, 2, "test");
      else gfxt::draw_rich(g, ctx, s, x, I(3), n, a[4], 1, 0, 1, "test");
      break;
    }
    case 7: message_buffer(g, ctx, I(1), I(2), I(3), a[4]); break;
    case 8: rlog("width " + std::to_string(gfxt::text_put(g, ctx, str(a[1])))); break;
    case 9: {
      const uint32_t d[5] = {a[1], a[2], 0, a[3], a[4]};
      b_draw_window(g, ctx, d, 5);
      break;
    }
    // the hover tips: 10 tool tip(sel), 11 slot tip(sel), 12 toolbar tip(sel, a, b), 13 action panel tip(hit, x, y), 14 view
    // panel tip(hover, x, y), 15 detail panel tip(hit, x, y), 16 amenities tip(L, a, b), 17 amenities row tip(r, a, b)
    case 10: gfxt::info_panel_tool_tip(g, ctx, I(1)); break;
    case 11: gfxt::info_panel_slot_tip(g, ctx, I(1)); break;
    case 12: gfxt::terrain_toolbar_tip(g, ctx, I(1), I(2), I(3)); break;
    case 13: gfxt::action_panel_tip(g, ctx, I(1), I(2), I(3)); break;
    case 14: gfxt::view_panel_tip(g, ctx, I(1), I(2), I(3)); break;
    case 15: gfxt::detail_panel_tip(g, ctx, I(1), I(2), I(3)); break;
    case 16: gfxt::amenities_tip(g, ctx, I(1), I(2), I(3)); break;
    case 17: gfxt::amenities_row_tip(g, ctx, I(1), I(2), I(3)); break;
    default: g.unsup("gfx_text.test: unknown op"); break;
  }
}

// loading.banner(done, expected) (P8A, the world load's banner): the game's own message window look, drawn the way drawWindow
// draws the ticker (drawFrameR3 at (x - 8, y - 8, 0x1a0, H + 0x10), the base banner font g_font51b360, drawTextSurface's
// colour 0x80007fff at (x + 0xc, y + 0x10)) over a COPY of the back buffer: the root surface is swapped for the copy while the
// body runs and put back, so the real back buffer, g.root_terrain and the GameState are untouched. The bar under the text is
// a track and a fill in the banner's own colours (white 0x7fff border, the panel dark 0x2108, the text-frame gold 0x7fe7);
// it fills done / expected (at most 95 %). The picture goes to `$gfx/banner.555`.
void b_loading_banner(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (g.back.px.empty() || g.back.w != g.root.w || g.back.h != g.root.h) return;
  Surface16 saved = std::move(g.root);
  const int saved_terrain = g.root_terrain;
  g.root = g.back;
  gfxb::text_banner_begin(g);  // P9C: the banner's runs, published with its picture
  const int32_t x = (g.root.w - 0x1a0) / 2 + 8, y = g.root.h * 5 / 12;
  const uint32_t root = g.root_va(ctx);
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  call(g, ctx, "drawFrameR3", {u(x - 8), u(y - 8), 0x1a0, 0x50, 1});
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  text_surface(g, ctx, "Loading...", x + 0xc, y + 0x10, 0x80007fffu);
  const int32_t tx = x + 0xc, ty = y + 0x30, tw = 0x1a0 - 0x28, th = 12;
  const double f = a[1] ? std::min(0.95, static_cast<double>(a[0]) / a[1]) : 0.5;
  const int32_t fw = static_cast<int32_t>((tw - 4) * f);
  for (int32_t yy = 0; yy < th; ++yy)
    for (int32_t xx = 0; xx < tw; ++xx) {
      const bool edge = yy == 0 || yy == th - 1 || xx == 0 || xx == tw - 1;
      const bool fill = !edge && yy >= 2 && yy < th - 2 && xx >= 2 && xx < 2 + fw;
      g.root.px[static_cast<size_t>(ty + yy) * g.root.w + (tx + xx)] = edge ? 0x7fff : fill ? 0x7fe7 : 0x2108;
    }
  gfxb::put_file(ctx, "$gfx/banner.555", gfxb::surface_bytes(g.root));
  gfxb::text_banner_end(g, g.root.px);  // P9C
  g.root = std::move(saved);
  g.root_terrain = saved_terrain;
}

// lang.selector(lang, open, hover) (P24D, app/lang_ui.h): the title menu's language dropdown on the root, before the menu's
// refreshFull, in the look of the game's banners (loading.banner): drawFrameR3 boxes and drawTextSurface's white in the base banner
// font g_font51b360. The closed box at (0x2a8, 4, 0x70, 0x20) names the current language (0 English, 1 Spanish) beside a small
// down arrow; open = 1 adds the list box under it, one 0x18 row per language, the row `hover` in the text-frame gold. The
// backend's fonts and the root text object are put back afterwards; nothing of the GameState is written.
void b_lang_selector(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  uint32_t fonts[4];
  std::memcpy(fonts, g.fonts, sizeof(fonts));
  TextObj &o = root_obj(g, ctx);
  const TextObj saved = o;
  const uint32_t root = g.root_va(ctx);
  constexpr int32_t X = 0x2a8, Y = 4, W = 0x70, H = 0x20, R = 0x18;
  static const char *const kName[2] = {"English", "Espa\xf1ol"};
  const int32_t cur = a[0] == 1 ? 1 : 0;
  auto arrow = [&](int32_t ax, int32_t ay, uint16_t c) {  // a down triangle 9 px wide, 5 rows, a dark shadow one row down
    for (int pass = 0; pass < 2; ++pass)
      for (int32_t r = 0; r < 5; ++r)
        for (int32_t k = r; k < 9 - r; ++k) {
          const int32_t px = ax + k, py = ay + r + (pass == 0 ? 1 : 0);
          if (px >= 0 && py >= 0 && px < g.root.w && py < g.root.h)
            g.root.px[static_cast<size_t>(py) * g.root.w + px] = pass == 0 ? 0x2108 : c;
        }
  };
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  call(g, ctx, "drawFrameR3", {u(X), u(Y), u(W), u(H), 1});
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
  text_surface(g, ctx, kName[cur], X + 0xa, Y + 8, 0x80007fffu);
  arrow(X + W - 0x14, Y + 0xe, 0x7fff);
  if (a[1] == 1) {
    call(g, ctx, "drawFrameR3", {u(X), u(Y + H), u(W), u(2 * R), 1});
    call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_font51b360), 0, 0, 0});
    for (int32_t i = 0; i < 2; ++i)
      text_surface(g, ctx, kName[i], X + 0xa, Y + H + R * i + 4, static_cast<int32_t>(a[2]) == i ? 0x80007fe7u : 0x80007fffu);
  }
  o = saved;
  std::memcpy(g.fonts, fonts, sizeof(fonts));
}

// hover.terrain(cx, cy) (P11C, gfx.h gfx_set_hover_help): the frame code's call with the cursor point View_toLocal gave (P6's
// frame words +0x68 / +0x58); draws only with the help on (the app's host), never writes the GameState
void b_hover_terrain(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (!g.hover_help || !g.hover_help_course) return;  // P13A: the course line is its own option
  g.ensure_root(ctx);
  gfxt::hover_help_terrain(g, ctx, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
}

const GfxBodyDef kBodies[] = {
    {"loading.banner", b_loading_banner},
    {"lang.selector", b_lang_selector},  // P24D
    {"hover.terrain", b_hover_terrain},
    {"Widget_setQuad70", b_set_quad70},
    {"Widget_setQuad74", b_set_quad74},
    {"Widget_setQuad78", b_set_quad78},
    {"gfx_text.test", b_test},
    {"drawWindow.frame", b_draw_window},
    {"drawWindowFrame", b_window_frame},
    {"drawStringRightAt", b_string_right_at},
    {"Text_put", b_text_put},
};

}  // namespace

extern const GfxFamily kGfxFamily_text = {"text", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
