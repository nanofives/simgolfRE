// Body family `editbox` of the graphics backend (roadmap S3.19; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// character editor's bio EditBox, a window-tree child of the root with its own window surface, drawn by EditBox::drawContents
// and composed into the presented frame at Window::refreshFull. Specifications (functions cited by name; their instruction
// addresses are in the hook sources and notes): re/analysis/ui/004864f0_EditBox_drawContents.md (the C2 transcription of
// drawContents), c3ep.cpp (EditBox::create), c3ef.cpp (Window::create, layoutViewTree), c3k.cpp (Window::innerSize),
// c3dq.cpp (Window::paint, drawTextLines), c3dm.cpp (Widget_clearArea), c3ab.cpp (Widget_fillRectR), c3ce.cpp (the jgld
// Surface slots callBounds / setClip / fillRectDispatch / drawLine), c3db.cpp (Widget_drawBevelBoxR), c3t.cpp
// (Widget_drawVLine), c3ct.cpp (EditBox::wrapToLines), c3s.cpp (EditBox::lineIndexAt), c3c.cpp / c3ae.cpp (Widget_value /
// Widget_get), c3df.cpp (wrapText, measureMarkupRun), c3ek.cpp (drawText), c3fm.cpp (Window::blitToTarget); log/roadmap/S3/
// 19_notes.md.
//
// THE WINDOW. EditBox::create builds the window with Window::create(x, y, w, h, flags 0x2020 or 0x102020, parent root): neither
// flag word has a bit of 0x1d, so layoutViewTree clears +0xa0 bit 1 (no edges, no frame shrink) and Window::innerSize changes
// nothing (no bit 0x4 / 0x8 / 0x400 / 0x11 / 0x10, no owner), so the window surface is w x h (Surface_createFull on the Image at
// +0x274; a new DIB section is zero-filled) at (x, y) of the root. chareditor.cpp's bio entry hands it over:
//
//   EditBox.window(eb, x, y, w, h)       the window surface (w x h, zero) at (x, y), shown; its first visible line from the
//                                        GameState word +0x57c (EditBox::resetFields' 0)
//   EditBox::drawContents(eb, paint)     the original's body, below, on the window surface; `paint` != 0 replaces +0x584 as
//                                        Window::paint's colour (a direct RGB555 word with bit 31: the bio's nearest white,
//                                        which the port does not store, U-1847)
//   EditBox.close(eb)                    EditBox::releaseState (Window::ctorInit takes the window off the tree): not composed
//
// DRAWCONTENTS (EditBox::on_redraw, the EditBox vtable's slot +0x58; Window::show and Window::doLayout call it, so every edit
// and the show draw it again): rect = the handle's getBounds {0, 0, w, h}; the handle's callBounds (clip = bounds);
// Window::paint(+0x584) = Widget_clearArea (the flag word has no 0x80000, so no paint through the parent) = fillRect16 of the
// clip; [+0x598] passes of Widget_drawBevelBoxR(rect, +0x590, +0x594), each shrinking rect by 1; rect shrunk by the inset
// +0x59c; Widget_fillRectR(rect) = setClip(rect); wrapToLines (width = surface width - 2 (border + inset), the lines and
// their count); the caret's line lineIndexAt(+0x5a4); n = (rect height + Widget_value - Widget_get) / Widget_value (idiv);
// the first visible line +0x57c lowered to the caret's line, or raised to caret line - n + 1 (both tests against the old
// value); drawTextLines from that line's start in rect; when +0x5a8 (the caret on) a vertical line at x = inset + border +
// drawMarkupRun(line start, text + caret - line start), y = Widget_value * (caret line - first) + inset + border -
// (Widget_value - Widget_get) / 2 down to that plus Widget_value, in the Image's style-0 colour (+0x2e0). The original
// STORES the first line into +0x57c: a ui-class GameState word, which the backend keeps in its own window state instead
// (it reads it once, at EditBox.window; U-1846). The other GameState write in the original's drawing is Widget_value's lazy
// fill of a NULL Image font slot +0x5c from g_primarySurface (the backend uses the same font without storing it).
//
// THE LINE TABLE. drawContents keeps wrapToLines' line starts in a 32-entry array at the top of its frame, and wrapToLines stores
// the text's end after the last line: with 32 lines that store is the dword above the array, drawContents' return address, so
// the original runs every call above and then returns into the text (found by the call oracle). The backend reports 32 lines
// or more as unsupported; record mode logs `fault return` instead of the first line. The bio cannot reach it: its 500 bytes
// break only at spaces in a 292-pixel width.
//
// THE WINDOW TREE'S COMPOSITION (gfxe::compose, called by gfx.cpp's Window::refreshFull after the root is copied to the back
// buffer): redrawDrawList walks the draw list and each window's owner blits its whole surface with Window::blitToTarget onto
// the display's back image at the window's position through its parents (Surface_blitThunk(target, 0, 0, x, y, w, h); the
// flag 0x1000000 DC path is not set for the EditBox), so each shown window surface is copied 1:1 at (x, y), clipped.
//
// COLOURS. Window::paint, Widget_drawBevelBoxR and the caret take palette indices; the text's style-0 colour (EditBox::create's
// Widget_setQuad of its default text colour word, 0 in the cov_r_bio captures) is index 0 too. A direct colour (bit 31) keeps its low 16 bits (jgld fillRect16 /
// line16); index 0 is black (jgld Palette::setDefault's entry 0); any other index is reported (the palettes at the bio are
// not measured, U-1847).
//
// TEST-ONLY record mode (port/test/test_gfx_s3_19.py's emulator CALL oracle, SG_EMU_ORACLE=1): `editbox.record(1)` makes
// drawContents log its callees the way the oracle's recorders log the ORIGINAL's (handle getBounds / callBounds,
// Window::paint, Widget_drawBevelBoxR, Widget_fillRectR, wrapToLines, drawTextLines, drawMarkupRun, Widget_drawVLine, then the
// first line it ends with) instead of drawing, with the oracle's scripted answers from `editbox.script(va)`: the words at va
// are (w, h, count, run base, run mask, count + 1 line offsets from the text); drawMarkupRun's width is the sum over its bytes
// of (base + (c & mask)). lineIndexAt, Widget_value and Widget_get run as in the original in both modes (from the memory).
// `editbox.record(0)` writes the log to `$gfx/editbox_calls.txt`.
//
// Negative controls (g_negctl_gfx_editbox, sg_negctl_set "gfx.family.editbox"):
//   1 the caret one pixel right                         2 the first line not raised when the caret is below the window
//   3 the inset left out of the text rectangle          4 the visible-line count without the (value - get) term
//   5 the window composed one pixel lower               6 Window::paint does not clear the surface
//   7 EditBox.close leaves the window composed          8 the bevel's light and dark colours swapped
//   9 wrapToLines' width 8 pixels wider
#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/gfx.h"

namespace portsim {

int g_negctl_gfx_editbox = 0;

namespace gfxt {   // gfx_text.cpp (roadmap S3.19): marked-up text on a window target
void set_text_target(Surface16 *s, const int32_t *clip, const uint32_t *fonts, uint32_t obj);
void load_text_obj(Gfx &g, uint32_t image, bool fresh, const uint32_t *w);
int32_t markup_width(Gfx &g, Ctx &ctx, const std::string &t, int32_t len);
int32_t draw_rich(Gfx &g, Ctx &ctx, const std::string &t, int32_t x, int32_t y, int32_t len, uint32_t main0, uint32_t shadow0,
                  int32_t dx0, int32_t dy0, const char *who);
size_t measure_markup_run(Gfx &g, Ctx &ctx, const std::string &t, size_t text0, int32_t &budget, int32_t len, int32_t &acc);
}  // namespace gfxt

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_editbox); }

// EditBox fields (c3ct.cpp's field list, c3ep.cpp, the drawContents transcription)
constexpr uint32_t kImage = 0x274, kHandleFlags = 0x9c, kText = 0x574, kFirst = 0x57c, kPaint = 0x584, kLight = 0x590,
                   kDark = 0x594, kBorder = 0x598, kInset = 0x59c, kCaret = 0x5a4, kCaretOn = 0x5a8;

struct Win {
  int32_t x = 0, y = 0;
  Surface16 s;
  int32_t clip[4] = {0, 0, 0, 0};
  int32_t first = 0;
};
struct Script {
  int32_t w = 0, h = 0, run_base = 0, run_mask = 0;
  std::vector<uint32_t> offs;   // count + 1 line offsets from the text
};
struct EbState {
  std::map<uint32_t, Win> win;   // by EditBox address
  bool rec = false;
  bool rec_overrun = false;   // the scripted table has 32 lines: the original's return address is overwritten (below)
  std::vector<std::string> log;
  Script script;
  int32_t rec_first = 0;
};
EbState &st(Gfx &g) { return g.state_of<EbState>("editbox"); }

std::string u(uint32_t v) { return std::to_string(v); }
std::string rect_s(const int32_t *r) {
  return u(static_cast<uint32_t>(r[0])) + " " + u(static_cast<uint32_t>(r[1])) + " " + u(static_cast<uint32_t>(r[2])) + " " +
         u(static_cast<uint32_t>(r[3]));
}

// THE IMAGE'S WORDS. The port's boot does not run the EditBox's static constructor (tick_boot.inc runs only the initializers
// whose state the simulation reads), so in the port's GameState the Image at EditBox + 0x274 can be all zero: its first word,
// the vtable Image_ctor stores, is then 0, and the drawing takes the words Image_ctor and EditBox::create's Widget_setQuad
// leave (c3dl.cpp ImageCtor_re, c3ep.cpp): markup on = f_cfg_4e4210's first word, style 0's colour = g_editBoxDefaults' third
// word, its shadow colour and offsets = f_cfg_EditBox_4e43f0's words 3..5, styles 1..3 colour and shadow -1 and offsets 2,
// everything else 0 (U-1848). An Image the GameState holds (its vtable word set: the original's captures, the segment images)
// is read as it is.
uint32_t image_word(Ctx &ctx, uint32_t img, uint32_t off) {
  const Memory &m = ctx.mem;
  if (m.load<uint32_t>(img) != 0) return m.load<uint32_t>(img + off);
  const uint32_t cfg = state_va(ctx, &GameState::f_cfg_EditBox_4e43f0);
  switch (off) {
    case 0x40: return m.load<uint32_t>(state_va(ctx, &GameState::f_cfg_4e4210));
    case 0x6c: return m.load<uint32_t>(state_va(ctx, &GameState::g_editBoxDefaults) + 8);
    case 0x7c: return m.load<uint32_t>(cfg + 0xc);
    case 0x8c: return m.load<uint32_t>(cfg + 0x10);
    case 0x9c: return m.load<uint32_t>(cfg + 0x14);
    case 0x70: case 0x74: case 0x78: case 0x80: case 0x84: case 0x88: return 0xffffffffu;
    case 0x90: case 0x94: case 0x98: case 0xa0: case 0xa4: case 0xa8: return 2;
    default: return 0;
  }
}

// Widget_value / Widget_get on the Image (c3c.cpp, c3ae.cpp): the font +0x5c (g_primarySurface when NULL); its +8 >= 0 gives
// +0x10 + +8, else +0xc; Widget_get = +0x10
uint32_t font_or_default(Ctx &ctx, uint32_t f) { return f != 0 ? f : ctx.gs().g_primarySurface; }
int32_t widget_value(Ctx &ctx, uint32_t font) {
  const uint32_t f = font_or_default(ctx, font);
  const int32_t i = ctx.mem.load<int32_t>(f + 8);
  return i >= 0 ? add32(ctx.mem.load<int32_t>(f + 0x10), i) : ctx.mem.load<int32_t>(f + 0xc);
}
int32_t widget_get(Ctx &ctx, uint32_t font) { return ctx.mem.load<int32_t>(font_or_default(ctx, font) + 0x10); }

// the callees drawContents reaches, drawn (Real) or logged with the oracle's scripted answers (Rec)
struct Callees {
  virtual ~Callees() = default;
  virtual void bounds(int32_t *r) = 0;
  virtual void call_bounds() = 0;
  virtual void paint(uint32_t c) = 0;
  virtual void bevel(const int32_t *r, uint32_t light, uint32_t dark) = 0;
  virtual void set_clip(const int32_t *r) = 0;
  virtual int32_t wrap(std::vector<uint32_t> &lines) = 0;   // wrapToLines: the line starts (count + 1, the text's end last)
  virtual void text_lines(uint32_t at, const int32_t *r) = 0;
  virtual int32_t run(uint32_t at, int32_t n) = 0;
  virtual void vline(int32_t x, int32_t y1, int32_t y2, uint32_t c) = 0;
  virtual void fault(const std::string &why) = 0;
  virtual uint32_t image(uint32_t off) = 0;   // a word of the EditBox's Image (+0x274 + off)
};

// EditBox::drawContents (re/analysis/ui/004864f0_EditBox_drawContents.md); `first` is the first visible line in and out
void draw_contents(Ctx &ctx, uint32_t eb, int32_t &first, uint32_t paint, Callees &k) {
  const Memory &m = ctx.mem;
  auto ld = [&](uint32_t off) { return m.load<int32_t>(eb + off); };
  int32_t r[4] = {0, 0, 0, 0};
  k.bounds(r);
  k.call_bounds();
  k.paint(paint);
  const int32_t border = ld(kBorder);
  for (int32_t i = 0; i < border; ++i) {   // `jle` over the count, then `jl` with the count read again
    const uint32_t light = static_cast<uint32_t>(ld(kLight)), dark = static_cast<uint32_t>(ld(kDark));
    if (mode() == 8) k.bevel(r, dark, light);
    else k.bevel(r, light, dark);
    r[0] = add32(r[0], 1);
    r[2] = sub32(r[2], 1);
    r[1] = add32(r[1], 1);
    r[3] = sub32(r[3], 1);
  }
  const int32_t inset = mode() == 3 ? 0 : ld(kInset);
  r[0] = add32(r[0], inset);
  r[1] = add32(r[1], inset);
  r[2] = sub32(r[2], inset);
  r[3] = sub32(r[3], inset);
  k.set_clip(r);
  std::vector<uint32_t> lines;
  const int32_t count = k.wrap(lines);
  // lineIndexAt(caret, lines, count) (c3s.cpp): the leading line starts at or below text + caret (unsigned), minus 1
  const uint32_t text = static_cast<uint32_t>(ld(kText));
  const uint32_t target = text + static_cast<uint32_t>(ld(kCaret));
  int32_t ci = 0;
  if (count > 0) {
    while (static_cast<size_t>(ci) < lines.size() && target >= lines[static_cast<size_t>(ci)]) {
      if (++ci >= count) break;
    }
  }
  ci -= 1;
  const uint32_t font = k.image(0x5c);
  const int32_t v = widget_value(ctx, font);
  const int32_t diff = sub32(v, widget_get(ctx, font));
  const int32_t v2 = widget_value(ctx, font);
  const int32_t span = mode() == 4 ? sub32(r[3], r[1]) : add32(sub32(r[3], r[1]), diff);
  if (v2 == 0 || (v2 == -1 && span == INT32_MIN)) return k.fault("divide");   // idiv raises #DE
  const int32_t n = span / v2;
  const int32_t old = first;
  const int32_t lim = sub32(add32(old, n), 1);   // `lea edx, [ecx + eax - 1]` with the value read before the stores
  if (ci < old) first = ci;
  if (ci > lim && mode() != 2) first = add32(sub32(ci, n), 1);
  if (first < 0 || first > count || static_cast<size_t>(first) >= lines.size())
    return k.fault("first line outside the line table");   // the original reads its 32-entry stack array there
  k.text_lines(lines[static_cast<size_t>(first)], r);
  if (ld(kCaretOn) == 0) return;
  if (ci < 0 || static_cast<size_t>(ci) >= lines.size()) return k.fault("caret line outside the line table");
  const uint32_t ls = lines[static_cast<size_t>(ci)];
  const int32_t w = k.run(ls, static_cast<int32_t>(target - ls));
  const int32_t v3 = widget_value(ctx, font);
  const int32_t base = add32(ld(kInset), ld(kBorder));
  const int32_t y1 = add32(mul32(v3, sub32(ci, first)), sub32(base, diff / 2));
  const int32_t y2 = add32(widget_value(ctx, font), y1);
  const int32_t x = add32(add32(base, w), mode() == 1 ? 1 : 0);
  k.vline(x, y1, y2, k.image(0x6c));
}

// ------------------------------------------------------------------------------------------------ record mode
struct Rec : Callees {
  Ctx &ctx;
  EbState &s;
  uint32_t eb_, text;
  Rec(Ctx &c, EbState &st_, uint32_t eb) : ctx(c), s(st_), eb_(eb), text(c.mem.load<uint32_t>(eb + kText)) {}
  void L(const std::string &l) { s.log.push_back(l); }
  void bounds(int32_t *r) override {
    r[0] = 0;
    r[1] = 0;
    r[2] = s.script.w;
    r[3] = s.script.h;
    L("bounds");
  }
  void call_bounds() override { L("callBounds"); }
  void paint(uint32_t c) override { L("paint " + u(c)); }
  void bevel(const int32_t *r, uint32_t a, uint32_t b) override { L("bevel " + rect_s(r) + " " + u(a) + " " + u(b)); }
  void set_clip(const int32_t *r) override { L("clip " + rect_s(r)); }
  int32_t wrap(std::vector<uint32_t> &lines) override {
    L("wrap");
    for (uint32_t o : s.script.offs) lines.push_back(text + o);
    const int32_t count = static_cast<int32_t>(s.script.offs.empty() ? 0 : s.script.offs.size() - 1);
    s.rec_overrun = count >= 32;
    return count;
  }
  void text_lines(uint32_t at, const int32_t *r) override { L("lines " + u(at - text) + " " + rect_s(r) + " 0 0"); }
  int32_t run(uint32_t at, int32_t n) override {
    L("run " + u(at - text) + " " + u(static_cast<uint32_t>(n)));
    int32_t w = 0;
    for (int32_t i = 0; i < n; ++i)
      w = add32(w, add32(s.script.run_base, ctx.mem.load<uint8_t>(at + static_cast<uint32_t>(i)) & s.script.run_mask));
    return w;
  }
  void vline(int32_t x, int32_t y1, int32_t y2, uint32_t c) override {
    L("vline " + u(static_cast<uint32_t>(x)) + " " + u(static_cast<uint32_t>(y1)) + " " + u(static_cast<uint32_t>(y2)) + " " +
      u(c));
  }
  void fault(const std::string &why) override { L("fault " + why); }
  uint32_t image(uint32_t off) override { return ctx.mem.load<uint32_t>(eb_ + kImage + off); }
};

// ------------------------------------------------------------------------------------------------ drawing
// jgld vline16 / hline16 (c3ay.cpp, as gfx_reports.cpp has them): nothing outside the clip or for a run of no length, the
// ends ordered and clamped to the clip, every pixel of the run inclusive
void vline16(Surface16 &s, const int32_t *c, int32_t x, int32_t y1, int32_t y2, uint16_t v) {
  if (x < c[0] || x >= c[2] || y1 == y2) return;
  if (y1 > y2) std::swap(y1, y2);
  if (y1 >= c[3] || y2 < c[1]) return;
  if (y1 < c[1]) y1 = c[1];
  if (y2 >= c[3]) y2 = c[3] - 1;
  for (int32_t y = y1; y <= y2; ++y) s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] = v;
}
void hline16(Surface16 &s, const int32_t *c, int32_t x1, int32_t x2, int32_t y, uint16_t v) {
  if (y < c[1] || y >= c[3] || x1 == x2) return;
  if (x1 > x2) std::swap(x1, x2);
  if (x1 >= c[2] || x2 < c[0]) return;
  if (x1 < c[0]) x1 = c[0];
  if (x2 >= c[2]) x2 = c[2] - 1;
  for (int32_t x = x1; x <= x2; ++x) s.px[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] = v;
}

struct Real : Callees {
  Gfx &g;
  Ctx &ctx;
  uint32_t eb;
  Win &w;
  bool failed = false;
  Real(Gfx &g_, Ctx &c, uint32_t e, Win &w_) : g(g_), ctx(c), eb(e), w(w_) {}
  bool colour(uint32_t c, uint16_t &v, const char *who) {
    if (c & 0x80000000u) {
      v = static_cast<uint16_t>(c & 0xffffu);
      return true;
    }
    if (c == 0) {
      v = 0;
      return true;
    }
    g.unsup(std::string("EditBox::drawContents: ") + who + ": a palette index other than 0 (the palettes are not measured)");
    return false;
  }
  void bounds(int32_t *r) override {
    r[0] = 0;
    r[1] = 0;
    r[2] = w.s.w;
    r[3] = w.s.h;
  }
  void call_bounds() override {
    w.clip[0] = 0;
    w.clip[1] = 0;
    w.clip[2] = w.s.w;
    w.clip[3] = w.s.h;
  }
  void paint(uint32_t c) override {
    if (ctx.mem.load<uint32_t>(eb + kHandleFlags) & (1u << 19)) {
      g.unsup("EditBox::drawContents: Window::paint through the parent (flag 0x80000)");
      return;
    }
    uint16_t v = 0;
    if (mode() == 6 || !colour(c, v, "Window::paint")) return;
    for (int32_t y = std::max(w.clip[1], 0); y < std::min(w.clip[3], w.s.h); ++y)
      for (int32_t x = std::max(w.clip[0], 0); x < std::min(w.clip[2], w.s.w); ++x)
        w.s.px[static_cast<size_t>(y) * static_cast<size_t>(w.s.w) + static_cast<size_t>(x)] = v;
  }
  // Widget_drawBevelBoxR(rect, light, dark) = Widget_drawBevelBox(l, t, r - 1, b - 1): top (l + 1 .. r', t) and left (l, t .. b'
  // - 1) light, bottom (l .. r' - 1, b') and right (r', t + 1 .. b') dark, through the handle's drawLine
  void bevel(const int32_t *r, uint32_t light, uint32_t dark) override {
    uint16_t lv = 0, dv = 0;
    if (!colour(light, lv, "Widget_drawBevelBoxR") || !colour(dark, dv, "Widget_drawBevelBoxR")) return;
    const int32_t l = r[0], t = r[1], rr = r[2] - 1, b = r[3] - 1;
    hline16(w.s, w.clip, l + 1, rr, t, lv);
    hline16(w.s, w.clip, l, rr - 1, b, dv);
    vline16(w.s, w.clip, l, t, b - 1, lv);
    vline16(w.s, w.clip, rr, t + 1, b, dv);
  }
  void set_clip(const int32_t *r) override {   // jgld setClip: the argument intersected with the bounds
    w.clip[0] = std::max(r[0], 0);
    w.clip[1] = std::max(r[1], 0);
    w.clip[2] = std::min(r[2], w.s.w);
    w.clip[3] = std::min(r[3], w.s.h);
    if (w.clip[2] < w.clip[0]) w.clip[2] = w.clip[0];
    if (w.clip[3] < w.clip[1]) w.clip[3] = w.clip[1];
  }
  std::string text_from(uint32_t at) { return gfxb::vstring(ctx.mem, at); }
  uint32_t image(uint32_t off) override { return image_word(ctx, eb + kImage, off); }
  void target() {
    uint32_t fonts[4];
    for (uint32_t i = 0; i < 4; ++i) fonts[i] = image(0x5c + 4 * i);
    if (fonts[0] == 0) fonts[0] = ctx.gs().g_primarySurface;   // Widget_value's lazy fill (not stored)
    gfxt::set_text_target(&w.s, w.clip, fonts, eb + kImage);
  }
  bool markup_on() {
    if (image(0x40) != 0) return true;
    g.unsup("EditBox::drawContents: an Image with markup off");
    return false;
  }
  // wrapToLines: wrapText(text, surface width - 2 (border + inset)) on the Image (c3ct.cpp, c3df.cpp): per line measureMarkupRun
  // over the rest of the text with the width as its budget; the line starts, then the text's end
  int32_t wrap(std::vector<uint32_t> &lines) override {
    const uint32_t text = ctx.mem.load<uint32_t>(eb + kText);
    if (text == 0 || !markup_on()) return 0;
    const std::string t = text_from(text);
    const int32_t pad = add32(ctx.mem.load<int32_t>(eb + kInset), ctx.mem.load<int32_t>(eb + kBorder));
    int32_t width = add32(sub32(w.s.w, add32(pad, pad)), mode() == 9 ? 8 : 0);
    int32_t ind = static_cast<int32_t>(image(0x14));   // +0x18 = the first-line indent +0x14
    target();
    size_t cur = 0;
    for (;;) {
      lines.push_back(text + static_cast<uint32_t>(cur));
      int32_t budget = width, acc = 0;
      const size_t next = gfxt::measure_markup_run(g, ctx, t, cur, budget, static_cast<int32_t>(t.size() - cur), acc);
      if (ind != 0) {
        width = sub32(width, ind);
        ind = 0;
      }
      if (next == std::string::npos) break;
      if (lines.size() >= 0x400 || next <= cur) {   // the original's stack array holds 32: past that its frame is overwritten
        gfxt::set_text_target(nullptr, nullptr, nullptr, 0);
        g.unsup("EditBox::drawContents: wrapToLines does not advance or passes its line table");
        failed = true;
        return 0;
      }
      cur = next;
    }
    gfxt::set_text_target(nullptr, nullptr, nullptr, 0);
    const int32_t count = static_cast<int32_t>(lines.size());
    lines.push_back(text + static_cast<uint32_t>(t.size()));
    if (count >= 32) {   // the end entry lands on drawContents' return address (THE LINE TABLE, above)
      g.unsup("EditBox::drawContents: 32 lines or more (the original's line table overflows onto its return address)");
      failed = true;
    }
    return count;
  }
  // drawTextLines(text, rect, 0, 0) (c3dq.cpp MeasureText_re): per line measureMarkupRun with the rectangle's width, drawText
  // (c3ek.cpp: min(strlen, n) characters through drawRichText) at (x, y), y += Widget_value, the indent once
  void text_lines(uint32_t at, const int32_t *r) override {
    if (failed || !markup_on()) return;
    const std::string t = text_from(at);
    int32_t x = r[0], width = sub32(r[2], r[0]), y = r[1];
    int32_t ind = static_cast<int32_t>(image(0x14));
    target();
    const uint32_t font = image(0x5c);
    const bool font_ok = ctx.mem.load<uint32_t>(font_or_default(ctx, font) + 4) != 0;
    size_t cur = 0;
    for (size_t guard = 0; guard < 0x400; ++guard) {
      int32_t budget = width, acc = 0;
      const size_t next = gfxt::measure_markup_run(g, ctx, t, cur, budget, static_cast<int32_t>(t.size() - cur), acc);
      const int32_t n = static_cast<int32_t>((next == std::string::npos ? t.size() : next) - cur);
      if (font_ok && n > 0)
        gfxt::draw_rich(g, ctx, t.substr(cur), x, y, n, image(0x6c), image(0x7c), static_cast<int32_t>(image(0x8c)),
                        static_cast<int32_t>(image(0x9c)), "EditBox::drawContents");
      y = add32(y, widget_value(ctx, font));
      if (ind != 0) {
        width = sub32(width, ind);
        x = add32(x, ind);
        ind = 0;
      }
      if (next == std::string::npos || next <= cur) break;
      cur = next;
    }
    gfxt::set_text_target(nullptr, nullptr, nullptr, 0);
  }
  int32_t run(uint32_t at, int32_t n) override {
    target();
    const int32_t v = gfxt::markup_width(g, ctx, gfxb::vstring(ctx.mem, at), n);
    gfxt::set_text_target(nullptr, nullptr, nullptr, 0);
    return v;
  }
  void vline(int32_t x, int32_t y1, int32_t y2, uint32_t c) override {
    uint16_t v = 0;
    if (colour(c, v, "the caret")) vline16(w.s, w.clip, x, y1, y2, v);
  }
  void fault(const std::string &why) override {
    g.unsup("EditBox::drawContents: the original faults or reads its stack here (" + why + ")");
  }
};

// ------------------------------------------------------------------------------------------------ bodies
// the text family's object for the EditBox's Image, from its words (image_word)
void load_obj(Gfx &g, Ctx &ctx, uint32_t eb, bool fresh) {
  uint32_t w[0xac / 4];
  for (uint32_t i = 0; i < 0xac / 4; ++i) w[i] = image_word(ctx, eb + kImage, 4 * i);
  gfxt::load_text_obj(g, eb + kImage, fresh, w);
}

void b_window(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  Win &w = st(g).win[a[0]];
  w = Win{};
  w.x = static_cast<int32_t>(a[1]);
  w.y = static_cast<int32_t>(a[2]);
  w.s.make(static_cast<int32_t>(a[3]), static_cast<int32_t>(a[4]));
  w.clip[2] = w.s.w;
  w.clip[3] = w.s.h;
  w.first = ctx.mem.load<int32_t>(a[0] + kFirst);
  load_obj(g, ctx, a[0], true);
}

void b_draw_contents(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  EbState &s = st(g);
  const uint32_t eb = a[0];
  if (s.rec) {
    Rec k(ctx, s, eb);
    s.rec_overrun = false;
    draw_contents(ctx, eb, s.rec_first, ctx.mem.load<uint32_t>(eb + kPaint), k);
    s.log.push_back(s.rec_overrun ? std::string("fault return") : "first " + u(static_cast<uint32_t>(s.rec_first)));
    return;
  }
  auto it = s.win.find(eb);
  if (it == s.win.end()) {
    g.unsup("EditBox::drawContents: no window (EditBox.window)");
    return;
  }
  load_obj(g, ctx, eb, false);
  Real k(g, ctx, eb, it->second);
  draw_contents(ctx, eb, it->second.first, a[1] != 0 ? a[1] : ctx.mem.load<uint32_t>(eb + kPaint), k);
}

void b_close(Gfx &g, Ctx &, const uint32_t *a, int) {
  if (mode() != 7) st(g).win.erase(a[0]);
}

void b_record(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  EbState &s = st(g);
  if (a[0] != 0) {
    s.rec = true;
    s.log.clear();
    return;
  }
  s.rec = false;
  std::string out;
  for (const std::string &l : s.log) out += l + "\n";
  gfxb::put_file(ctx, "$gfx/editbox_calls.txt", std::vector<uint8_t>(out.begin(), out.end()));
}

// editbox.script(va, eb): (w, h, count, run base, run mask, count + 1 line offsets) at va, and the record mode's first visible
// line from the EditBox's +0x57c
void b_script(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  EbState &s = st(g);
  const Memory &m = ctx.mem;
  Script sc;
  sc.w = m.load<int32_t>(a[0]);
  sc.h = m.load<int32_t>(a[0] + 4);
  const uint32_t count = m.load<uint32_t>(a[0] + 8);
  sc.run_base = m.load<int32_t>(a[0] + 12);
  sc.run_mask = m.load<int32_t>(a[0] + 16);
  for (uint32_t i = 0; i <= count && i < 0x40; ++i) sc.offs.push_back(m.load<uint32_t>(a[0] + 20 + 4 * i));
  s.script = sc;
  s.rec_first = m.load<int32_t>(a[1] + kFirst);
}

const GfxBodyDef kBodies[] = {
    {"EditBox.window", b_window},     {"EditBox::drawContents", b_draw_contents}, {"EditBox.close", b_close},
    {"editbox.record", b_record},     {"editbox.script", b_script},
};

}  // namespace

// The window tree's composition at Window::refreshFull (gfx.cpp): every shown window surface copied 1:1 onto the back buffer
// at its position (Window::blitToTarget), clipped to the back buffer
namespace gfxe {
void compose(Gfx &g, Ctx &) {
  auto p = g.family_state.find("editbox");
  if (p == g.family_state.end() || !p->second) return;
  const EbState &s = *static_cast<const EbState *>(p->second.get());
  for (const auto &kv : s.win) {
    const Win &w = kv.second;
    const int32_t oy = w.y + (mode() == 5 ? 1 : 0);
    for (int32_t y = 0; y < w.s.h; ++y) {
      const int32_t dy = oy + y;
      if (dy < 0 || dy >= g.back.h) continue;
      for (int32_t x = 0; x < w.s.w; ++x) {
        const int32_t dx = w.x + x;
        if (dx < 0 || dx >= g.back.w) continue;
        g.back.px[static_cast<size_t>(dy) * static_cast<size_t>(g.back.w) + static_cast<size_t>(dx)] =
            w.s.px[static_cast<size_t>(y) * static_cast<size_t>(w.s.w) + static_cast<size_t>(x)];
      }
    }
  }
}
}  // namespace gfxe

extern const GfxFamily kGfxFamily_editbox = {"editbox", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
