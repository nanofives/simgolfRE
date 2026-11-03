// Body family `boot_screens` of the graphics backend (roadmap S2.7; port/src/sim/gfx_body.h, listed in gfx_families.inc): the
// drawing of the screens the boot and the title menu reach before a course: the loading screen (showLoadingScreen), Select
// Difficulty (selectDifficultyScreen), the theme-pack grid (showThemePackSelect) and the frame objects of Pick A Pro
// (pickAProScreen, whose redraw the port's screen function issues itself, menu.cpp). Specifications: shim/src/re/c3fi.cpp
// ShowLoadingScreen_re, c3en.cpp SelectDifficultyScreen_re, c3fw.cpp ShowThemePackSelect_re / PickAProScreen_re, c3dv.cpp
// DrawValueWidget_re (drawMessageBuffer), c3dq.cpp DrawTextSurface_re, c3dw.cpp DrawPanelBackdrop_re, c3df.cpp
// SurfaceDrawScaled_re; instruction addresses are in those sources. Each body draws through the core and family primitives
// (gfxb::find_body): loadImage, Surface_blit, Image_draw, Surface_blitThunk, setDrawTarget, drawCenteredText, drawTextWithColor,
// fillWindowRect, blitPanel, drawCenteredText2, Window::refreshFull, the frame-object lifetimes.
//
// A frame object of the original (a Surface or Image on the screen function's stack) is a gfx_local_key: S2.7 uses the
// offsets 0x70000 .. 0x73fff (loading 0x70000, difficulty 0x71000, theme packs 0x72000, pick a pro 0x73000).
//
//   showLoadingScreen.draw(percent, r, r2)
//       r and r2 are the screen's two Random::range draws (12 and 11) the port makes. A Surface and an Image are made; for r
//       <= 11 the Image loads loading picture r (the pool text at f_strpool_4c44ac + 0x1e4 - 0x2c r, flags 2); the Surface
//       takes its rectangle (0x5a, 0xaf, 0x26c, 0xca, trim) and is drawn at (0x5a, 0xaf); for r2 <= 10 the hint r2 (the
//       eleven pool texts of the original's table, in its order) is drawn as drawMessageBuffer(0x32, 0xc8, 0xf0, white) draws
//       the text buffer (the original copies it there: the text is the same bytes); a percent other than -1 draws the bar:
//       fillWindowRect(0x12b, 0x190, 0xca, 0x18, white), v = clamp((100 - percent) * 2, 0, 0xc8), fillWindowRect(0x1f4 - v,
//       0x191, v, 0x16, 0x80007b20), drawCenteredText(`Loading...`, 0x190, 0x194, black); then the frame objects are freed.
//       drawMessageBuffer as the original runs it: lines broken at ' ', newline and '^' when the summed one-character widths
//       pass cols * 8, each drawn with drawTextSurface when the line height (Widget_value - 4, at least 10) is above 0x10 and
//       the colour is 0x80007fff, else drawTextWithColor. drawTextSurface = setQuad(target, colour, 1, 0, 1) + drawText: a
//       shadow run in run-palette colour 1 at (x, y + 1) under the main run (the colour U-1230 measured for that entry, as
//       gfx_prim_text2.cpp draws it).
//   selectDifficultyScreen.setup / .redraw(sel) / .tip / .teardown
//       setup: an Image and five Surfaces; the Image loads the highlight sheet (f_strpool_4c84e8 + 0x290), the five rectangles
//       (0, 0, 0x152, 0x92), (400, 0, 0x152, 0x92), (0, 0x12c, 0x14c, 0x72), (400, 0x12c, 0x14c, 0x82), (0x2dc, 0x214, 0x44,
//       0x44) are cut (trim), then the Image loads the screen (+0x274). redraw: setDrawTarget(Root, g_fontRef_519a40), the
//       Image whole onto the root (Surface_blitThunk), the title (+0x260) centred at (0x25a, 0x2a) in black, the highlighted
//       choice's Surface at its table point (x 0xc1, 0x96, 0xa1, 0xc8, 0x2dc; y 0x20, 0xa1, 0x140, 0x1b4, 0x214), the four
//       captions (+0x258, +0x24c, +0x240, +0x234) centred at x 0x184, 0x168, 0x16e, 0x192 and y 0x71, 0xe3, 0x15a, 0x1d0, two
//       pixels lower and in 0x80004210 unless highlighted (then black). tip: the back button's tip, drawPanelBackdrop(0x2ff,
//       0x22d) over the text `Cancel` (+0xa8) the original copies to the text buffer. teardown: the objects freed.
//   showThemePackSelect.setup(count) / .redraw(count) / .hover(prev, hot) / .tip(text, hot, dwell) / .teardown
//       setup: per theme i of g_themeName (0x32 bytes each) the six FindFirstFileA probes Themes\<name><suffix> (suffixes
//       f_strpool_4e18e4, f_strpool_4c84e8 + 0x84, f_strpool_4e18ac, f_strpool_4e1868, f_strpool_4e4130 + 0x10,
//       f_strpool_4e4130) give one flag byte (bits 1, 2, 4, 8, 0x20, 0x40; the first probe stores, the others OR); eight
//       Surfaces A and four B, the Image loads the highlight sheet (f_strpool_4e40a0 + 0x70) and A's eight rectangles are cut,
//       then the screen (+0x54) and B's four. redraw: setDrawTarget(Root, g_font51a028), the Image whole, the title (+0x40)
//       drawTextWithColor at (0x4e, 0x22), setDrawTarget(Root, g_font51b360), the five column titles (+0x38, +0x2c, +0x20,
//       +0x14, +0xc) centred at x 0x15a, 0x1b4, 0x20e, 0x268, 0x2c2 and y 0x3e / 0x25 alternating, setDrawTarget(Root,
//       g_font51a028); per theme row (pen y 0x5c + 0x20 i): the current theme (its name equal to g_theme) gets A[7] at x
//       0x48 and its name in black at (0x49, y + 0xb), the others the name in 0x80004210; then for column b = 0..5 a set flag
//       bit b draws A[5] (current) or A[6] at (column x - 0x2b, y), a clear bit on the current row A[4]. Column 5's x is a
//       stack word the original never writes (U-1260): see COLUMN 5 below. hover: the previous hot button's B image and the
//       new one's A image at the button table point (x 0x245, 0x2d6, 0x301, 0x301; y 0x208, 0x215, 0x51, 0x1d7), refreshFull.
//       tip: setDrawTarget(Root, g_font51b360), at dwell 0x1f blitPanel(x, y + 0xe, 0x30, 0x10, 0), drawCenteredText2(text,
//       x + 0x18, y + 0x10, white), refreshFull. teardown: the objects freed.
//   pickAProScreen.setup / .textSurface(text, x, y, colour) / .teardown
//       setup: the 15 Surfaces and the Image of loadGameBrowserScreen's layout: the Image loads interface\Title_LoadGame_MO
//       (f_strpool_4c8808 + 0x50) cut into elements 5..10 and 14, then the pro sheet (+0x34) cut into elements 0..4; the
//       redraw is the screen function's (menu.cpp pick_a_pro_screen, with the same keys: gfx_local_key(0x73000 + 0x2c k) and
//       gfx_local_key(0x73400)). textSurface: drawTextSurface (the card's quote). teardown: the objects freed.
//
// COLUMN 5 (U-1252 settled, U-1260): showThemePackSelect's sixth column word is read (movsx) for flag bit 5 but never written,
// so the original draws that cell at x = w - 0x2b with w whatever the stack held. The four pinned runs of cov_r_s2_screens
// without the trace recorder (det_a, det_b, det_c, c_a) show it at x -17 (w = 0x1a: a 0x57-wide yellow cell over x 0..69 of the
// Standard row); the trace recorder's runs t_rec / t_rep1 show no such cell (their w puts it where it is not seen). The
// backend uses w = 0x1a; the environment variable SG_GFX_THEMEPACK_COL5 overrides it (a number, or `none`: no cell).
//
// Negative controls (g_negctl_gfx_boot_screens, sg_negctl_set "gfx.family.boot_screens"):
//   1 the loading bar's yellow part one pixel wider     2 the loading hint without its shadow
//   3 the difficulty captions not lowered when not highlighted   4 the difficulty screen drawn from the highlight sheet
//   5 the theme column titles in g_font51a028           6 the theme probe for characters (bit 2) skipped
//   7 the theme hover draws B images for the new hot button too   8 pick a pro's element 10 (disabled load) not cut
//   9 the difficulty tip drawn at y - 4 instead of y - 5
//   (10 .. 13 are read by the port's hunks through this registry switch: 10 BootSetup's title art not drawn, 11 loadFlic's
//   refreshFull after the loading screen not issued, 12 loadFlic's waitTicks(10, 0) clock reads not consumed, 13 pick a pro's
//   rows drawn at x 0x141)
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS  // the development switch SG_GFX_THEMEPACK_COL5 uses getenv
#endif
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "gfx_body.h"
#include "portsim/arith.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"

namespace portsim {

int g_negctl_gfx_boot_screens = 0;

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_boot_screens); }
uint32_t u32(int32_t v) { return static_cast<uint32_t>(v); }

constexpr uint32_t kWhite = 0x80007FFFu, kBlack = 0x80000000u, kGrey = 0x80004210u;
// the run palette's entry 1 as a direct colour word (gfx_prim_text2.cpp kRunPalette1, U-1230)
constexpr uint32_t kRunPalette1 = 0x80000000u | (0x80u >> 3) << 10;

void call(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> a) {
  uint32_t args[12] = {};
  int n = 0;
  for (uint32_t v : a)
    if (n < 12) args[n++] = v;
  if (GfxBody b = gfxb::find_body(name)) {
    b(g, ctx, args, n);
    return;
  }
  g.unsup(std::string("gfx_boot_screens: no body ") + name);
}
uint32_t root(Gfx &g, Ctx &ctx) { return g.root_va(ctx); }
uint32_t window(Ctx &ctx) { return state_va(ctx, &GameState::g_rootWindow); }
void refresh(Gfx &g, Ctx &ctx) { call(g, ctx, "Window::refreshFull", {window(ctx), 0}); }
void font(Gfx &g, Ctx &ctx, uint32_t f) { call(g, ctx, "setDrawTarget", {root(g, ctx), f, 0, 0, 0}); }
void image_draw(Gfx &g, Ctx &ctx, uint32_t s, int32_t x, int32_t y) { call(g, ctx, "Image_draw", {s, root(g, ctx), u32(x), u32(y), 0}); }
void blit_whole(Gfx &g, Ctx &ctx, uint32_t img) {
  call(g, ctx, "Surface_blitThunk", {img, root(g, ctx), 0, 0, 0, 0, 0x320, 0x258});
}
void cut(Gfx &g, Ctx &ctx, uint32_t dst, uint32_t img, int32_t x, int32_t y, int32_t w, int32_t h) {
  call(g, ctx, "Surface_blit", {dst, img, u32(x), u32(y), u32(w), u32(h), 1, 1});
}

// Widget_value(target) (c3c.cpp Value_re) of the backend's font 0, as gfx.cpp computes it
int32_t widget_value(Gfx &g, Ctx &ctx) {
  const uint32_t f = g.fonts[0] ? g.fonts[0] : ctx.gs().g_primarySurface;
  const int32_t i = ctx.mem.load<int32_t>(f + 8);
  return i >= 0 ? add32(ctx.mem.load<int32_t>(f + 0x10), i) : ctx.mem.load<int32_t>(f + 0xc);
}

// drawMessageBuffer(cols, x, y, colour) over the NUL-terminated text at `src` (the bytes the original has in the text buffer)
void draw_message(Gfx &g, Ctx &ctx, uint32_t src, int32_t cols, int32_t x, int32_t y, uint32_t colour) {
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) {
    g.unsup("showLoadingScreen.draw: no text provider");
    return;
  }
  const Memory &m = ctx.mem;
  int32_t H = sub32(widget_value(g, ctx), 4);
  if (H < 10) H = 10;
  const uint32_t len = vstrlen(m, src);
  if (len == 0) return;
  const bool surface = H > 0x10 && colour == kWhite;
  auto line_at = [&](uint32_t st, uint32_t e, int32_t yy) {
    std::string s;
    for (uint32_t k = st; k < e && k < len; ++k) s += static_cast<char>(m.load<uint8_t>(src + k));
    if (surface) {
      // drawTextSurface: setQuad(target, colour, 1, 0, 1); its clamp (pen + Widget_value at or past the surface height) is
      // not modelled
      if (add32(widget_value(g, ctx), yy) >= g.root.h) g.unsup("showLoadingScreen.draw: drawTextSurface's bottom clamp");
      if (mode() != 2) gfxb::draw_text_string(g, ctx, s, 0, x, add32(yy, 1), kRunPalette1, false, "drawTextSurface (shadow)");
      gfxb::draw_text_string(g, ctx, s, 0, x, yy, kWhite, false, "drawTextSurface");
    } else {
      gfxb::draw_text_string(g, ctx, s, 0, x, yy, colour, false, "drawTextWithColor");
    }
  };
  uint32_t st = 0, e = 0, i = 0;
  int32_t acc = 0;
  do {
    const uint8_t c = m.load<uint8_t>(src + i);
    int32_t w = 0;
    if (!tm->glyph_width(ctx, g.fonts[0], src + i, 1, w)) {
      g.unsup("showLoadingScreen.draw: the provider cannot measure");
      return;
    }
    acc = add32(acc, w);
    if (c == ' ' || c == 0x0a || c == '^') e = i;
    if (c == 0x0a || c == '^' || acc > cols * 8) {
      line_at(st, e, y);
      y = add32(y, H);
      acc = 0;
      st = e + 1;
      if (c == '^') st = e + 2;
      i = e;
      if (y > 0x248) return;
    }
    ++i;
  } while (i < len);
  if (acc > 0 && y <= 0x248) line_at(st, len, y);
}

// ------------------------------------------------------------------------------------------------ showLoadingScreen
uint32_t ls_surf() { return gfx_local_key(0x70000); }
uint32_t ls_img() { return gfx_local_key(0x70100); }

void b_loading(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const int32_t percent = static_cast<int32_t>(a[0]);
  const uint32_t r = a[1] & 0xffff, r2 = a[2] & 0xffff;
  call(g, ctx, "Surface_ctor", {ls_surf()});
  call(g, ctx, "Image_ctor", {ls_img()});
  if (r <= 11) call(g, ctx, "loadImage", {ls_img(), state_va(ctx, &GameState::f_strpool_4c44ac) + 0x1e4 - 0x2c * r, 0, 0, 0x100, 2});
  cut(g, ctx, ls_surf(), ls_img(), 0x5a, 0xaf, 0x26c, 0xca);
  image_draw(g, ctx, ls_surf(), 0x5a, 0xaf);
  if (r2 <= 10) {
    const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c3f74);
    const uint32_t hints[11] = {state_va(ctx, &GameState::f_strpool_4c4444), state_va(ctx, &GameState::f_strpool_4c43b4),
                                state_va(ctx, &GameState::f_strpool_4c433c), state_va(ctx, &GameState::f_strpool_4c42b4),
                                state_va(ctx, &GameState::f_strpool_4c41fc), state_va(ctx, &GameState::f_strpool_4c4168),
                                state_va(ctx, &GameState::f_strpool_4c4100), state_va(ctx, &GameState::f_strpool_4c40ac),
                                pool + 0xb0, pool + 0x74, pool + 0x40};
    draw_message(g, ctx, hints[r2], 0x32, 0xc8, 0xf0, kWhite);
  }
  if (percent != -1) {
    const int32_t v0 = static_cast<int32_t>((0x64u - static_cast<uint32_t>(percent)) << 1);
    const int32_t v = v0 < 0 ? 0 : v0 > 0xc8 ? 0xc8 : v0;
    call(g, ctx, "fillWindowRect", {window(ctx), 0x12b, 0x190, 0xca, 0x18, kWhite});
    const int32_t wv = mode() == 1 ? add32(v, 1) : v;
    call(g, ctx, "fillWindowRect", {window(ctx), u32(sub32(0x1f4, wv)), 0x191, u32(wv), 0x16, 0x80007B20u});
    call(g, ctx, "drawCenteredText", {state_va(ctx, &GameState::f_strpool_4c3f74) + 0x34, 0x190, 0x194, kBlack});
  }
  call(g, ctx, "Surface_free", {ls_surf()});
  call(g, ctx, "Image_dtor", {ls_img()});
  call(g, ctx, "Surface_free", {ls_surf()});
}

// ------------------------------------------------------------------------------------------------ selectDifficultyScreen
uint32_t sd_surf(uint32_t k) { return gfx_local_key(0x71000 + 0x2c * k); }
uint32_t sd_img() { return gfx_local_key(0x71400); }
uint32_t sd_pool(Ctx &ctx) { return state_va(ctx, &GameState::f_strpool_4c84e8); }

void b_sd_setup(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  for (uint32_t k = 0; k < 5; ++k) call(g, ctx, "Surface_ctor", {sd_surf(k)});
  call(g, ctx, "Image_ctor", {sd_img()});
  call(g, ctx, "loadImage", {sd_img(), sd_pool(ctx) + 0x290, 0, 0, 0x100, 2});
  cut(g, ctx, sd_surf(0), sd_img(), 0, 0, 0x152, 0x92);
  cut(g, ctx, sd_surf(1), sd_img(), 400, 0, 0x152, 0x92);
  cut(g, ctx, sd_surf(2), sd_img(), 0, 0x12c, 0x14c, 0x72);
  cut(g, ctx, sd_surf(3), sd_img(), 400, 0x12c, 0x14c, 0x82);
  cut(g, ctx, sd_surf(4), sd_img(), 0x2dc, 0x214, 0x44, 0x44);
  if (mode() != 4) call(g, ctx, "loadImage", {sd_img(), sd_pool(ctx) + 0x274, 0, 0, 0x100, 2});
}

void b_sd_redraw(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  static const int32_t kY[5] = {0x20, 0xa1, 0x140, 0x1b4, 0x214}, kX[5] = {0xc1, 0x96, 0xa1, 0xc8, 0x2dc};
  static const struct { uint32_t off; int32_t x, y; } kCap[4] = {{0x258, 0x184, 0x71}, {0x24c, 0x168, 0xe3}, {0x240, 0x16e, 0x15a},
                                                                 {0x234, 0x192, 0x1d0}};
  const int32_t sel = static_cast<int32_t>(a[0]);
  font(g, ctx, state_va(ctx, &GameState::g_fontRef_519a40));
  blit_whole(g, ctx, sd_img());
  call(g, ctx, "drawCenteredText", {sd_pool(ctx) + 0x260, 0x25a, 0x2a, kBlack});
  if (sel >= 0 && sel < 5) image_draw(g, ctx, sd_surf(u32(sel)), kX[sel], kY[sel]);
  for (int i = 0; i < 4; ++i) {
    const bool lit = sel == i;
    const int32_t y = kCap[i].y + (!lit && mode() != 3 ? 2 : 0);
    call(g, ctx, "drawCenteredText", {sd_pool(ctx) + kCap[i].off, u32(kCap[i].x), u32(y), lit ? kBlack : kGrey});
  }
}

// drawPanelBackdrop(0x2ff, 0x22d) over `Cancel`: setDrawTarget(Root, g_fontRef_519fd8); w = 3 * strlen; half = x raised to w
// and lowered to 0x320 - w; its Terrain::drawLine bar is not in the back buffer (gfx.cpp b_panel_backdrop); the text centred
// at (half, y - 5) in white
void b_sd_tip(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  const uint32_t text = sd_pool(ctx) + 0xa8;
  font(g, ctx, state_va(ctx, &GameState::g_fontRef_519fd8));
  const int32_t w = mul32(static_cast<int32_t>(vstrlen(ctx.mem, text)), 3);
  int32_t half = 0x2ff;
  if (half < w) half = w;
  if (half > sub32(0x320, w)) half = sub32(0x320, w);
  call(g, ctx, "drawCenteredText", {text, u32(half), u32(0x22d - (mode() == 9 ? 4 : 5)), kWhite});
}

void b_sd_teardown(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  call(g, ctx, "Image_dtor", {sd_img()});
  for (uint32_t k = 0; k < 5; ++k) call(g, ctx, "Surface_free", {sd_surf(k)});
}

// ------------------------------------------------------------------------------------------------ showThemePackSelect
uint32_t tp_a(uint32_t k) { return gfx_local_key(0x72000 + 0x2c * k); }
uint32_t tp_b(uint32_t k) { return gfx_local_key(0x72400 + 0x2c * k); }
uint32_t tp_img() { return gfx_local_key(0x72800); }
uint32_t tp_pool(Ctx &ctx) { return state_va(ctx, &GameState::f_strpool_4e40a0); }
const int32_t kTpX[4] = {0x245, 0x2d6, 0x301, 0x301};
const int32_t kTpY[4] = {0x208, 0x215, 0x51, 0x1d7};

struct ThemeState {
  std::vector<uint8_t> flags;
};
ThemeState &tst(Gfx &g) { return g.state_of<ThemeState>("boot_screens.themes"); }

// column 5's word (COLUMN 5 above): false = no cell
bool col5_word(int32_t &w) {
  w = 0x1a;
  if (const char *e = std::getenv("SG_GFX_THEMEPACK_COL5")) {
    if (std::strcmp(e, "none") == 0) return false;
    w = static_cast<int16_t>(std::strtol(e, nullptr, 0));
  }
  return true;
}

void b_tp_setup(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t count = static_cast<int32_t>(a[0]);
  const Memory &m = ctx.mem;
  const uint32_t suffix[6] = {state_va(ctx, &GameState::f_strpool_4e18e4), state_va(ctx, &GameState::f_strpool_4c84e8) + 0x84,
                              state_va(ctx, &GameState::f_strpool_4e18ac), state_va(ctx, &GameState::f_strpool_4e1868),
                              state_va(ctx, &GameState::f_strpool_4e4130) + 0x10, state_va(ctx, &GameState::f_strpool_4e4130)};
  static const uint8_t kBit[6] = {1, 2, 4, 8, 0x20, 0x40};
  ThemeState &ts = tst(g);
  ts.flags.assign(count > 0 ? static_cast<size_t>(count) : 0, 0);
  const std::string prefix = gfxb::vstring(m, state_va(ctx, &GameState::f_strpool_4c84e8));
  for (int32_t i = 0; i < count; ++i) {
    const std::string name = gfxb::vstring(m, state_va(ctx, &GameState::g_themeName) + 0x32u * u32(i));
    uint8_t f = 0;
    for (int s = 0; s < 6; ++s) {
      if (s == 1 && mode() == 6) continue;
      const std::string path = prefix + name + gfxb::vstring(m, suffix[s]);
      if (ctx.files_ptr != nullptr && !ctx.files().list(path).empty()) f = s == 0 ? 1 : static_cast<uint8_t>(f | kBit[s]);
    }
    ts.flags[static_cast<size_t>(i)] = f;
  }
  for (uint32_t k = 0; k < 8; ++k) call(g, ctx, "Surface_ctor", {tp_a(k)});
  for (uint32_t k = 0; k < 4; ++k) call(g, ctx, "Surface_ctor", {tp_b(k)});
  call(g, ctx, "Image_ctor", {tp_img()});
  static const struct { uint32_t e; int32_t x, y, w, h; } kA[8] = {
      {4, 0x001, 0x01, 0x57, 0x1f}, {5, 0x059, 0x01, 0x57, 0x1f}, {6, 0x0b1, 0x01, 0x57, 0x1f}, {7, 0x001, 0x21, 0xdd, 0x1f},
      {0, 0x109, 0x01, 0x47, 0x50}, {1, 0x151, 0x01, 0x30, 0x31}, {2, 0x182, 0x01, 0x13, 0x23}, {3, 0x196, 0x01, 0x13, 0x23}};
  static const struct { uint32_t e; int32_t x, y, w, h; } kB[4] = {
      {0, 0x245, 0x208, 0x47, 0x50}, {1, 0x2d6, 0x215, 0x30, 0x31}, {2, 0x301, 0x051, 0x13, 0x23}, {3, 0x301, 0x1d7, 0x13, 0x23}};
  call(g, ctx, "loadImage", {tp_img(), tp_pool(ctx) + 0x70, 0, 0, 0x100, 2});
  for (const auto &c : kA) cut(g, ctx, tp_a(c.e), tp_img(), c.x, c.y, c.w, c.h);
  call(g, ctx, "loadImage", {tp_img(), tp_pool(ctx) + 0x54, 0, 0, 0x100, 2});
  for (const auto &c : kB) cut(g, ctx, tp_b(c.e), tp_img(), c.x, c.y, c.w, c.h);
}

void b_tp_redraw(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t count = static_cast<int32_t>(a[0]);
  const Memory &m = ctx.mem;
  const uint32_t pool = tp_pool(ctx);
  const uint32_t f_main = state_va(ctx, &GameState::g_font51a028), f_cols = state_va(ctx, &GameState::g_font51b360);
  font(g, ctx, f_main);
  blit_whole(g, ctx, tp_img());
  call(g, ctx, "drawTextWithColor", {pool + 0x40, 0x4e, 0x22, kBlack});
  font(g, ctx, mode() == 5 ? f_main : f_cols);
  static const struct { uint32_t off; int32_t x, y; } kCol[5] = {
      {0x38, 0x15a, 0x3e}, {0x2c, 0x1b4, 0x25}, {0x20, 0x20e, 0x3e}, {0x14, 0x268, 0x25}, {0x0c, 0x2c2, 0x3e}};
  for (const auto &c : kCol) call(g, ctx, "drawCenteredText", {pool + c.off, u32(c.x), u32(c.y), kBlack});
  font(g, ctx, f_main);
  const ThemeState &ts = tst(g);
  const std::string theme = gfxb::vstring(m, state_va(ctx, &GameState::g_theme));
  static const int32_t kColX[5] = {0x15a, 0x1b4, 0x20e, 0x268, 0x2c2};
  int32_t w5 = 0;
  const bool has5 = col5_word(w5);
  int32_t y = 0x5c;
  for (int32_t i = 0; i < count; ++i) {
    const uint32_t nm = state_va(ctx, &GameState::g_themeName) + 0x32u * u32(i);
    const bool cur = gfxb::vstring(m, nm) == theme;
    if (cur) {
      image_draw(g, ctx, tp_a(7), 0x48, y);
      call(g, ctx, "drawTextWithColor", {nm, 0x49, u32(add32(y, 0xb)), kBlack});
    } else {
      call(g, ctx, "drawTextWithColor", {nm, 0x49, u32(add32(y, 0xb)), kGrey});
    }
    const int32_t fl = static_cast<int8_t>(static_cast<size_t>(i) < ts.flags.size() ? ts.flags[static_cast<size_t>(i)] : 0);
    for (int b = 0; b < 6; ++b) {
      if (b == 5 && !has5) {
        if (fl & (1 << b) || cur) g.unsup("showThemePackSelect.redraw: column 5's cell left out (SG_GFX_THEMEPACK_COL5=none)");
        continue;
      }
      const int32_t col = b < 5 ? kColX[b] : w5;
      if (fl & (1 << b))
        image_draw(g, ctx, tp_a(cur ? 5 : 6), sub32(col, 0x2b), y);
      else if (cur)
        image_draw(g, ctx, tp_a(4), sub32(col, 0x2b), y);
    }
    y = add32(y, 0x20);
  }
  refresh(g, ctx);
}

void b_tp_hover(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t prev = static_cast<int32_t>(a[0]), hot = static_cast<int32_t>(a[1]);
  if (prev >= 0 && prev < 4) image_draw(g, ctx, tp_b(u32(prev)), kTpX[prev], kTpY[prev]);
  if (hot >= 0 && hot < 4) image_draw(g, ctx, mode() == 7 ? tp_b(u32(hot)) : tp_a(u32(hot)), kTpX[hot], kTpY[hot]);
  refresh(g, ctx);
}

void b_tp_tip(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  const int32_t hot = static_cast<int32_t>(a[1]), dwell = static_cast<int32_t>(a[2]);
  if (hot < 0 || hot >= 4) return;
  font(g, ctx, state_va(ctx, &GameState::g_font51b360));
  if (dwell == 0x1f) call(g, ctx, "blitPanel", {u32(kTpX[hot]), u32(kTpY[hot] + 0xe), 0x30, 0x10, 0});
  call(g, ctx, "drawCenteredText2", {a[0], u32(kTpX[hot] + 0x18), u32(kTpY[hot] + 0x10), kWhite});
  refresh(g, ctx);
}

void b_tp_teardown(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  call(g, ctx, "Image_dtor", {tp_img()});
  for (uint32_t k = 0; k < 4; ++k) call(g, ctx, "Surface_free", {tp_b(k)});
  for (uint32_t k = 0; k < 8; ++k) call(g, ctx, "Surface_free", {tp_a(k)});
  tst(g).flags.clear();
}

// ------------------------------------------------------------------------------------------------ pickAProScreen
uint32_t pp_surf(uint32_t k) { return gfx_local_key(0x73000 + 0x2c * k); }
uint32_t pp_img() { return gfx_local_key(0x73400); }

void b_pp_setup(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  for (uint32_t k = 0; k < 15; ++k) call(g, ctx, "Surface_ctor", {pp_surf(k)});
  call(g, ctx, "Image_ctor", {pp_img()});
  static const struct { uint32_t e; int32_t x, y, w, h; } kMO[7] = {
      {5, 0x258, 0x208, 0x46, 0x46}, {6, 0x2e4, 0x212, 0x3c, 0x3c}, {7, 0x30a, 0x064, 0x16, 0x28}, {8, 0x30a, 0x17c, 0x16, 0x28},
      {9, 0x29e, 0x208, 0x46, 0x46}, {10, 0x258, 0x1b3, 0x46, 0x46}, {14, 0x29e, 0x1b3, 0x46, 0x46}};
  static const struct { uint32_t e; int32_t x, y, w, h; } kPlain[5] = {
      {0, 0x258, 0x208, 0x46, 0x46}, {1, 0x2e4, 0x212, 0x3c, 0x3c}, {2, 0x30a, 0x064, 0x16, 0x28}, {3, 0x30a, 0x17c, 0x16, 0x28},
      {4, 0x29e, 0x208, 0x46, 0x46}};
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c8808);
  call(g, ctx, "loadImage", {pp_img(), pool + 0x50, 0, 0, 0x100, 2});
  for (const auto &c : kMO)
    if (!(c.e == 10 && mode() == 8)) cut(g, ctx, pp_surf(c.e), pp_img(), c.x, c.y, c.w, c.h);
  call(g, ctx, "loadImage", {pp_img(), pool + 0x34, 0, 0, 0x100, 2});
  for (const auto &c : kPlain) cut(g, ctx, pp_surf(c.e), pp_img(), c.x, c.y, c.w, c.h);
}

// drawTextSurface(text, x, y, colour) (c3dq.cpp DrawTextSurface_re): setQuad(target, colour, 1, 0, 1); a pen y with y +
// Widget_value at or past the surface height moves to height - Widget_value - 1; drawText(text, x, y, strlen): the shadow run
// in run-palette colour 1 at (x, y + 1), then the main run
void b_pp_text_surface(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  if (a[0] == 0) return;
  const int32_t x = static_cast<int32_t>(a[1]);
  int32_t y = static_cast<int32_t>(a[2]);
  const int32_t v = widget_value(g, ctx);
  if (add32(v, y) >= g.root.h) y = sub32(sub32(g.root.h, v), 1);
  const std::string s = gfxb::vstring(ctx.mem, a[0]);
  gfxb::draw_text_string(g, ctx, s, a[0], x, add32(y, 1), kRunPalette1, false, "drawTextSurface (shadow)");
  gfxb::draw_text_string(g, ctx, s, a[0], x, y, a[3], false, "drawTextSurface");
}

void b_pp_teardown(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  call(g, ctx, "Image_dtor", {pp_img()});
  for (uint32_t k = 0; k < 15; ++k) call(g, ctx, "Surface_free", {pp_surf(k)});
}

const GfxBodyDef kBodies[] = {
    {"showLoadingScreen.draw", b_loading},
    {"selectDifficultyScreen.setup", b_sd_setup},
    {"selectDifficultyScreen.redraw", b_sd_redraw},
    {"selectDifficultyScreen.tip", b_sd_tip},
    {"selectDifficultyScreen.teardown", b_sd_teardown},
    {"showThemePackSelect.setup", b_tp_setup},
    {"showThemePackSelect.redraw", b_tp_redraw},
    {"showThemePackSelect.hover", b_tp_hover},
    {"showThemePackSelect.tip", b_tp_tip},
    {"showThemePackSelect.teardown", b_tp_teardown},
    {"pickAProScreen.setup", b_pp_setup},
    {"pickAProScreen.textSurface", b_pp_text_surface},
    {"pickAProScreen.teardown", b_pp_teardown},
};

}  // namespace

extern const GfxFamily kGfxFamily_boot_screens = {"boot_screens", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

}  // namespace portsim
