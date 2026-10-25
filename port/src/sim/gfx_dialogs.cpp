// Body family `dialogs` of the graphics backend (roadmap S2.9; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// dialogs the game draws over the course: showPopup's option box (every popup and the System menu), textInputBox's entry
// box, saveGameDialog's panel and the association's grading page of showCourseReport. Specifications: the C3 / C4 hook
// sources named at each body (shim/src/re/*.cpp, instruction addresses there) and the matched jgld routines
// (re/match/jgld_*.cpp). The screens make the drawing calls themselves (commands_popup.inc, saveload.cpp, reports.cpp);
// the bodies here are the callees the core and the earlier families did not model.
//
//   loadInterfaceArt.dialogArt
//       the cuts of loadInterfaceArt(1) the dialogs draw (c3fv.cpp LoadInterfaceArt_re after kIa17 / kIa18 / kIa20 /
//       kIa23; each load is loadImage(img, name, 0, 0, 0x100, 2), names as offsets into f_strpool_4c95f4):
//       InfoButtons (+0x3d64): the 3 x 3 panel border g_sprites5612e8 cells 30..38 (Grid at x 0xc8 + 0x11 c, y 0x11 r,
//       0x10 x 0x10, trim 0) and the three check boxes g_sprites561260 (Row at x 0x12c + 0x32 i, y 0, 0x1a x 0x1a, trim 1);
//       InfoButtons_A (+0x3d4c): the border masks g_sprites58ccb8 cells 30..38 (same grid); TransPopups (+0x3d1c) and
//       TransPopups_A (+0x3d04): the option bullets g_surfaces5678b8 / g_sprites56a7b8, three per load, cells i at
//       (0x12c + 0x32 i, 0x12c) 0x1e x 0x1e and cells 4 + i at (0x12c + 0x32 i, 0x8c) 0x32 x 0x32, trim 0. The loads the
//       other cuts of these files feed are the core's (loadInterfaceArt.images); loading a file twice is the same image.
//   drawPanelS2(x, y, w, h)
//       c3da.cpp DrawPanelS2_re: the display scale (1, 1, 1); the 16-rounding of w / x and h / y (a size whose low nibble
//       is not 0 grows by r = 15 - (size & 15), the position moves back by r / 2); fillWindowRect(root window, x + 4,
//       y + 4, w - 8, h - 8, 0x80004e79); then the eight border pieces top-left, top, bottom, left, right, top-right,
//       bottom-right, bottom-left = g_sprites5612e8 cells 30 + {0, 1, 7, 3, 5, 2, 8, 6} over the same cells of
//       g_sprites58ccb8 through Surface_stretchBlit (the core's mask blend); the edges repeat every 16 from pos + 16,
//       (len - 16 - 1) >> 4 times, when len - 16 > 16.
//   drawTextSurface(text, x, y, colour)
//       c3dq.cpp DrawTextSurface_re: Widget_setQuad(target, colour, 1, 0, 1) (a shadow in run-palette colour 1 one pixel
//       down: the measured RGB (0x80, 0, 0) of prim_text2, U-1230); y = h - v - 1 when v + y >= h (h the target's
//       height, v = Widget_value: the font's +8 >= 0 gives +0x10 + +8, else +0xc); drawText at (x, y), left-aligned.
//   (Widget_fillRect, Surface_drawSprite, Widget_draw, drawStringAt and drawStringAt2, which these dialogs also call, are the
//   `reports` family's bodies, gfx_reports.cpp: roadmap S2.13 kept one body per name, U-1278.)
//   showPopup.readColour(x, y) / showPopup.oddBox(left, y, w, h, inner) / showPopup.oddRepaint(left, y, w, h)
//       c3em.cpp ShowPopup_re for an odd y: the box colour is read once before the loop from the root surface at (x, y)
//       (pixelRed / Green / Blue, c3db.cpp: with the display's RGB555 format the three components give back the word,
//       so the colour is 0x80000000 | (word & 0x7fff); outside the surface the default 0); each pass fills (left, y, w, h)
//       with 0x80006318 and (left + 1, y + 1, inner + 0x2f, h - 2) with that colour; the leave on a click repaints
//       (left, y, w, h) with it. The colour comes from what lies under the box (the course), so these pixels are held
//       until S3 draws the course.
//   dialogs.begin / dialogs.end
//       markers the screens put around a dialog (showPopup, textInputBox, saveGameDialog, showCourseReport); they draw
//       nothing. With the held-pixel probe on (a negctl mode >= 0x10000 of this family, a measurement, not a control) the
//       root surface is filled when a dialog starts a stack, with the colour mode & 0x7fff below mode 0x20000, else with
//       a pseudo-random RGB555 pattern seeded by the mode's low word: depth 0 and some other
//       drawing call (handled or unsupported; the markers and Window::refreshFull, which leaves the root as it is, excluded)
//       since the last dialog ended; in probe mode the calls that started a new stack are reported (gfx.log). A pixel that differs
//       between two runs with two probe colours depends on what lies under the dialogs (port/test/test_gfx_dialogs.py).
//
// Negative controls (g_negctl_gfx_dialogs, sg_negctl_set "gfx.family.dialogs"):
//   1 drawPanelS2's fill colour 0x4e7a instead of 0x4e79      2 drawTextSurface draws no shadow
//   4 drawPanelS2 draws no right-edge pieces                   5 the option bullets cut one row lower
//   6 drawPanelS2 without the 16-rounding                      9 showPopup.oddBox's inner fill one row taller (h - 1)
//   (modes 3, 7, 8, 10, 11 belonged to the bodies that moved to `reports`: there they are modes 9, 1, 10, 20, 21)
//
// The icon row cut for the grading page's stars is described at its definition below.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"

namespace portsim {

int g_negctl_gfx_dialogs = 0;

namespace gfxf {
void fill_rect16(Surface16 &d, int32_t l, int32_t t, int32_t r, int32_t b, uint16_t c);   // gfx_prim_fill.cpp
}
namespace gfxp {   // gfx_prim_text.cpp: drawText's path in the target's quad colour
void draw_text_run(Gfx &g, Ctx &ctx, uint32_t target, const std::string &text, uint32_t va, int32_t x, int32_t y,
                   const char *who);
}

namespace {
int mode() { return SG_NEGCTL(g_negctl_gfx_dialogs); }
bool control(int m) { return mode() == m; }

// the run palette's entry 1 as a direct colour word: RGB (0x80, 0, 0), as prim_text2 measured it (U-1230)
constexpr uint32_t kRunPalette1 = 0x80000000u | (0x80u >> 3) << 10;

struct DState {
  int depth = 0;
  uint64_t mark = std::numeric_limits<uint64_t>::max();   // drawing calls counted when the last dialog ended
  uint32_t popup_colour = 0;                                // showPopup.readColour's word for an odd y
  std::map<std::string, uint64_t> at_end;                   // the call counts when the last dialog ended (probe mode)
};
DState &st(Gfx &g) { return g.state_of<DState>("dialogs"); }

// the markers, and Window::refreshFull, which copies the root surface to the back buffer and leaves the root as it is: neither
// breaks a stack (mainLoop refreshes between the System menu's answer and the popup it opens)
bool is_marker(const std::string &n) { return n == "dialogs.begin" || n == "dialogs.end" || n == "Window::refreshFull"; }
uint64_t drawing_calls(const Gfx &g) {
  uint64_t n = 0;
  for (const auto &kv : g.handled)
    if (!is_marker(kv.first)) n += kv.second;
  for (const auto &kv : g.unsupported) n += kv.second;
  return n;
}

void call_body(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> a) {
  GfxBody b = gfxb::find_body(name);
  if (!b) {
    g.unsup(std::string("dialogs: no body ") + name);
    return;
  }
  uint32_t args[12] = {};
  int n = 0;
  for (uint32_t v : a) args[n++] = v;
  b(g, ctx, args, n);
}

uint32_t pool(Ctx &ctx) { return state_va(ctx, &GameState::f_strpool_4c95f4); }

// ------------------------------------------------------------------------------------------------ the art
void cut_icons(Gfx &g, Ctx &ctx);
void b_dialog_art(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  Image8 img;
  auto load = [&](uint32_t off) {
    img = Image8{};
    if (gfxb::load_image(g, ctx, pool(ctx) + off, 0, 0, 0x100, 2, img) == 0) return true;
    g.unsup("loadInterfaceArt.dialogArt: a file that did not load");
    return false;
  };
  auto grid3 = [&](uint32_t dest) {   // Grid(dest, 3, 3, 0xc8, 0x11, 0, 0x11, 0x10, 0x10, 1, 0)
    for (uint32_t r = 0; r < 3; ++r)
      for (uint32_t c = 0; c < 3; ++c)
        gfxb::crop_sprite(g, dest + 0x2Cu * (3 * r + c), img, static_cast<int32_t>(0xC8 + 0x11 * c),
                          static_cast<int32_t>(0x11 * r), 0x10, 0x10, false);
  };
  auto bullets = [&](uint32_t dest) {   // three of each size, x 0x12c, 0x15e, 0x190
    const int32_t dy = control(5) ? 1 : 0;
    for (uint32_t i = 0; i < 3; ++i) {
      const int32_t x = static_cast<int32_t>(0x12C + 0x32 * i);
      gfxb::crop_sprite(g, dest + 0xB0 + 0x2C * i, img, x, 0x8C + dy, 0x32, 0x32, false);
      gfxb::crop_sprite(g, dest + 0x2C * i, img, x, 0x12C + dy, 0x1E, 0x1E, false);
    }
  };
  const uint32_t border = state_va(ctx, &GameState::g_sprites5612e8) + 0x2C * 30;
  const uint32_t masks = state_va(ctx, &GameState::g_sprites58ccb8) + 0x2C * 30;
  if (load(0x3D64)) {   // InfoButtons
    grid3(border);
    const uint32_t boxes = state_va(ctx, &GameState::g_sprites561260);
    for (uint32_t i = 0; i < 3; ++i)
      gfxb::crop_sprite(g, boxes + 0x2C * i, img, static_cast<int32_t>(0x12C + 0x32 * i), 0, 0x1A, 0x1A, true);
  }
  if (load(0x3D4C)) grid3(masks);                                                     // InfoButtons_A
  if (load(0x3D1C)) bullets(state_va(ctx, &GameState::g_surfaces5678b8));            // TransPopups
  if (load(0x3D04)) bullets(state_va(ctx, &GameState::g_sprites56a7b8));             // TransPopups_A
  cut_icons(g, ctx);
}

// ------------------------------------------------------------------------------------------------ drawPanelS2
void round16(int32_t &pos, int32_t &size) {
  const int32_t low = size & 0xf;
  if (low != 0) {
    const int32_t r = 0xf - low;
    size += r;
    pos -= r / 2;
  }
}

void b_draw_panel_s2(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), w = static_cast<int32_t>(a[2]),
          h = static_cast<int32_t>(a[3]);
  call_body(g, ctx, "Display::setScale", {1, 1, 1});
  if (!control(6)) {
    round16(x, w);
    round16(y, h);
  }
  const int32_t fx = add32(x, 4), fy = add32(y, 4);
  gfxf::fill_rect16(g.root, fx, fy, add32(fx, sub32(w, 8)), add32(fy, sub32(h, 8)), control(1) ? 0x4e7a : 0x4e79);
  const uint32_t s = state_va(ctx, &GameState::g_sprites5612e8) + 0x2C * 30;
  const uint32_t m = state_va(ctx, &GameState::g_sprites58ccb8) + 0x2C * 30;
  static const uint32_t kCell[8] = {0, 1, 7, 3, 5, 2, 8, 6};
  const uint32_t root = g.root_va(ctx);
  auto piece = [&](int i, int32_t px, int32_t py) {
    call_body(g, ctx, "Surface_stretchBlit", {s + 0x2Cu * kCell[i], m + 0x2Cu * kCell[i], root, u32(px), u32(py), 0});
  };
  piece(0, x, y);
  if (w - 0x10 > 0x10) {
    uint32_t n = static_cast<uint32_t>(w - 0x10 - 1) >> 4;
    int32_t xi = x + 0x10;
    do {
      piece(1, xi, y);
      piece(2, xi, y + h - 0x10);
      xi += 0x10;
    } while (--n != 0);
  }
  if (h - 0x10 > 0x10) {
    uint32_t n = static_cast<uint32_t>(h - 0x10 - 1) >> 4;
    int32_t yi = y + 0x10;
    do {
      piece(3, x, yi);
      if (!control(4)) piece(4, x + w - 0x10, yi);
      yi += 0x10;
    } while (--n != 0);
  }
  if (!control(4)) {
    piece(5, x + w - 0x10, y);
    piece(6, x + w - 0x10, y + h - 0x10);
  }
  piece(7, x, y + h - 0x10);
}

// ------------------------------------------------------------------------------------------------ text
// Widget_value(target) (c3c.cpp Value_re) over the backend's current font
int32_t widget_value(Gfx &g, Ctx &ctx) {
  const uint32_t f = g.fonts[0] ? g.fonts[0] : ctx.gs().g_primarySurface;
  const int32_t i = ctx.mem.load<int32_t>(f + 8);
  return i >= 0 ? add32(ctx.mem.load<int32_t>(f + 0x10), i) : ctx.mem.load<int32_t>(f + 0xc);
}

void b_draw_text_surface(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t x = static_cast<int32_t>(a[1]);
  int32_t y = static_cast<int32_t>(a[2]);
  const int32_t v = widget_value(g, ctx);
  if (add32(v, y) >= g.root.h) y = sub32(sub32(g.root.h, v), 1);
  if (a[0] == 0) return;
  const std::string text = gfxb::vstring(ctx.mem, a[0]);
  if (!control(2)) gfxb::draw_text_string(g, ctx, text, a[0], x, add32(y, 1), kRunPalette1, false, "drawTextSurface (shadow)");
  gfxb::draw_text_string(g, ctx, text, a[0], x, y, a[3], false, "drawTextSurface");
}

// ------------------------------------------------------------------------------------------------ showPopup, odd y
void b_popup_read_colour(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]);
  uint16_t w = 0;
  if (x >= 0 && y >= 0 && x < g.root.w && y < g.root.h)
    w = g.root.px[static_cast<size_t>(y) * static_cast<size_t>(g.root.w) + static_cast<size_t>(x)];
  st(g).popup_colour = 0x80000000u | (w & 0x7fffu);
}
void fill(Gfx &g, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) {
  gfxf::fill_rect16(g.root, x, y, add32(x, w), add32(y, h), static_cast<uint16_t>(c & 0xffff));
}
void b_popup_odd_box(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t l = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), w = static_cast<int32_t>(a[2]),
                h = static_cast<int32_t>(a[3]), inner = static_cast<int32_t>(a[4]);
  fill(g, l, y, w, h, 0x80006318u);
  fill(g, add32(l, 1), add32(y, 1), add32(inner, 0x2f), sub32(h, control(9) ? 1 : 2), st(g).popup_colour);
}
void b_popup_odd_repaint(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  fill(g, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]),
       st(g).popup_colour);
}

// ------------------------------------------------------------------------------------------------ the grading page
// loadInterfaceArt(1)'s Row(g_surfaces59b050, 40 cells at (0x10 i, 0x144), 0x10 x 0x10, trim 1) from the image of kIa6's last
// load (+0x3ffc into f_strpool_4c95f4; c3fv.cpp, after RUN(kIa6)): cell 17 is the report's star
void cut_icons(Gfx &g, Ctx &ctx) {
  Image8 img;
  if (gfxb::load_image(g, ctx, pool(ctx) + 0x3FFC, 0, 0, 0x100, 2, img) != 0) {
    g.unsup("loadInterfaceArt.dialogArt: a file that did not load");
    return;
  }
  const uint32_t dest = state_va(ctx, &GameState::g_surfaces59b050);
  for (uint32_t i = 0; i < 40; ++i) gfxb::crop_sprite(g, dest + 0x2C * i, img, static_cast<int32_t>(0x10 * i), 0x144, 0x10, 0x10, true);
}

// ------------------------------------------------------------------------------------------------ markers
std::map<std::string, uint64_t> call_counts(const Gfx &g) {
  std::map<std::string, uint64_t> m;
  for (const auto &kv : g.handled)
    if (!is_marker(kv.first)) m[kv.first] += kv.second;
  for (const auto &kv : g.unsupported) m[kv.first] += kv.second;
  return m;
}
void b_begin(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  DState &s = st(g);
  if (s.depth++ != 0) return;
  if (s.mark != std::numeric_limits<uint64_t>::max() && drawing_calls(g) == s.mark) return;   // the same stack goes on
  if (mode() >= 0x10000) {
    // probe mode only: name the calls that started a new stack (the log's `first` lines; a measurement aid)
    if (s.mark != std::numeric_limits<uint64_t>::max()) {
      std::string why;
      for (const auto &kv : call_counts(g)) {
        auto it = s.at_end.find(kv.first);
        if (it == s.at_end.end() || it->second != kv.second) why += (why.empty() ? "" : ", ") + kv.first;
      }
      g.unsup("dialogs.begin (probe): a new stack after " + why);
    }
    g.ensure_root(ctx);
    if (mode() >= 0x20000) {   // a pseudo-random RGB555 pattern (xorshift32 seeded by the mode's low word)
      uint32_t x = 0x9e3779b9u ^ static_cast<uint32_t>(mode() & 0xffff);
      for (uint16_t &w : g.root.px) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        w = static_cast<uint16_t>(x & 0x7fff);
      }
    } else {
      std::fill(g.root.px.begin(), g.root.px.end(), static_cast<uint16_t>(mode() & 0x7fff));
    }
  }
}
void b_end(Gfx &g, Ctx &, const uint32_t *, int) {
  DState &s = st(g);
  if (s.depth > 0) --s.depth;
  if (s.depth == 0) {
    s.mark = drawing_calls(g);
    if (mode() >= 0x10000) s.at_end = call_counts(g);
  }
}

const GfxBodyDef kBodies[] = {
    {"loadInterfaceArt.dialogArt", b_dialog_art},
    {"drawPanelS2", b_draw_panel_s2},
    {"drawTextSurface", b_draw_text_surface},
    {"showPopup.readColour", b_popup_read_colour},
    {"showPopup.oddBox", b_popup_odd_box},
    {"showPopup.oddRepaint", b_popup_odd_repaint},
    {"dialogs.begin", b_begin},
    {"dialogs.end", b_end},
};

}  // namespace

extern const GfxFamily kGfxFamily_dialogs = {"dialogs", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
