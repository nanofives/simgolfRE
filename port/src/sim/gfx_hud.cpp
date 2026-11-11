// Body family `hud` of the graphics backend (roadmap S3.6; port/include/portsim/sim/gfx_hud.h, port/src/sim/gfx_body.h,
// listed in gfx_families.inc). Specifications: the C3 / C4 hook sources named at each body (shim/src/re/*.cpp) and the
// matched jgld routines (re/match/jgld_*.cpp); functions are cited by name, their instruction addresses are in those sources.
//
// Surface_blit3(this, dst, x, y, w, h, x2, y2, w2, h2) (c3h.cpp SurfaceBlit3_re: jgld Surface::stretchTo, a GDI StretchBlt
// SRCCOPY of this's rectangle (x, y, w, h) onto dst's (x2, y2, w2, h2)). The core body (gfx.cpp) models an 8-bit image onto
// the root at 1:1; gfxh::surface_blit3 takes every other case the frame code reaches:
//   - a 16-bit source: the root surface (g_rootWindow + 0x274, the address f_stateptr_4c1570 holds) or a family surface;
//   - a destination other than the root: a family surface, made at its first use with the root's size (both call sites
//     that copy the root out, P5_tournament's Image at F+0x36c and the routing render's Image at F+0x624, created it
//     0x320 x 0x258 with Surface_createFull / Surface_createFull_fwd1 just before);
//   - a rectangle of another size (the golfer magnifier of P4_golferOne, P5_zoom's centre 400 x 300 over the whole
//     800 x 600): StretchBlt's magnification repeats source pixels, destination column x2 + i reading source column
//     x + i * w / w2 (and the same for rows); a shrink is reported (no call site shrinks).
//   Both rectangles are clipped to their surfaces. When the source and destination are one surface the source rectangle is
//   read from a copy taken before the first write (see U-1350 for what the references showed).
//
// Negative controls (g_negctl_gfx_hud, sg_negctl_set "gfx.family.hud"):
//   1 a 16-bit copy writes one row less (the rectangle's last row is left)
//   2 a magnification reads source column x + (i + 1) * w / w2 (one source pixel to the right per step)
//   3 loadInterfaceArt.images does nothing            4 every interface-art cut takes the other trim setting
//   5 the half-bright table is never chosen (the shade falls to g_color16Lut's block 1)
//   6 Surface_drawScaled draws one pixel to the right
//   7 a SpriteRef no modelled cut reached is reported unsupported (the pre-S3.6 behaviour)
//   8 a scaled drawB reads one source column to the right (the unscaled copy unchanged)
//   9 drawGolferActionPanel's shot button 4 takes the next frame image   10 drawMatchScoreboard's hole columns one pixel right
//   (roadmap S3.6b, the employee panels) 11 the golfer list's names one pixel right   12 the golfer list's scroll bar 7 px tall
//   13 the detail panel's list-slot images (entries 8..15) one pixel lower   14 the detail panel's text block one pixel lower
//   15 the detail panel's three buttons with no selection take the selection branch's images
//   16 (P22D fix 1) the golfer list's columns past its limit drawn inside the panel's layer again (the badges cut at its right border)
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "portsim/sim/gfx_hud.h"
#include "portsim/sim/gfx_scene.h"   // P23D fix 3: gfxs::bubble_shade

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include "commands_internal.h"
#include "gfx_body.h"
#include "portsim/sim/bridge.h"  // P22D fix 2: view_list_columns
#include "portsim/sim/gfx_scene.h"
#include "portsim/sim/gfx_text.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/util.h"

namespace portsim {

extern int g_defect_undo_button_help;  // tick.cpp mainLoop.undoButtonHelpOnly (P13A item 12)
extern int g_defect_shot_lie_mixed_tile;  // tick.cpp mainLoop.shotButtonsReadBallTile (P22B; b_action_panel)
namespace {
// P13A: the bulldoze mode the bulldozer armed (g_flags bit 27, commands_panels.inc undo_button): the Build palette (slot 0x15)
// draws the button lit, with the art of its own hover, while it is set (drawTerrainToolbar, which pickBuilding's -3 shares, already
// does: its own art at (0xfd, 0x222); the amenities palette's items 7 / 8 are not lit)
bool p13a_bulldoze_on(const GameState &gs) { return !SG_DEFECT(g_defect_undo_button_help) && (gs.g_flags & (1u << 27)) != 0; }
}  // namespace

int g_negctl_gfx_hud = 0;

// hoverTip.tenFrameDwell (P6B, 2026-11-03, Mariano's play-test: `the build palette's popup should come sooner`): every HUD panel
// keeps one hover counter pair, the last hovered control g_panelState + 4 and its count g_ui_5aa554,
// and draws a control's tip only once the count is above 10: showInfoCursorPanel `cmp [g_ui_5aa554], 0xa; jle` at
// showInfoCursorPanel+0xc7 (the tool tip) and `cmp eax, 0xa; jle` at showInfoCursorPanel+0x76f (the slot tip), drawTerrainToolbar+0x140,
// drawBuildingPanel+0x55, drawAmenitiesPanel+0x4fd, drawEmployeeViewPanel+0x6ca, drawGolferActionPanel+0x635 (the same compare).
// The counter grows by one per frame on the same control, so the tip comes 11 frames after the first hovered frame (about 1.0 s
// at mainLoop's 87..100 ms frame). Default 1 = the original's 10; play (0): 3, the tip 4 frames after the first hover (about
// 0.37 s, near Windows' own 400 ms hover time). The counters are ui class (the HUD panels' drawing keeps its own copy); at 0
// drawBuildingPanel's tip also builds its text in g_textBuffer (class hidden) and appendNumber's g_textBuf (NON_CAUSAL) sooner.
int g_defect_tip_dwell = 1;
int32_t hover_tip_dwell() { return SG_DEFECT(g_defect_tip_dwell) ? 10 : 3; }

namespace {

int mode() { return SG_NEGCTL(g_negctl_gfx_hud); }

struct HudState {
  std::map<uint32_t, Surface16> s16;                          // family surfaces by the original object's key
  std::map<uint32_t, std::array<uint16_t, 256>> palettes;     // palette objects loadImage filled (their RGB555 words)
  std::map<uint32_t, uint64_t> uncut;                         // SpriteRefs a draw read that no modelled cut reached (draw counts)
};
HudState &st(Gfx &g) { return g.state_of<HudState>("hud"); }

// development: SG_GFX_HUD_LOG=<file> appends one line per case this family reports
void dlog(const char *fmt, const uint32_t *a, int n) {
  static FILE *fh = std::getenv("SG_GFX_HUD_LOG") ? std::fopen(std::getenv("SG_GFX_HUD_LOG"), "a") : nullptr;
  if (!fh) return;
  std::fprintf(fh, "%s", fmt);
  for (int i = 0; i < n; ++i) std::fprintf(fh, " %d", static_cast<int>(a[i]));
  std::fprintf(fh, "\n");
  std::fflush(fh);
}

// the 16-bit surface behind a key: the root or a family surface; nullptr when the backend holds none
Surface16 *surface16(Gfx &g, Ctx &ctx, uint32_t key, bool create) {
  if (key == g.root_va(ctx)) return &g.root;
  HudState &s = st(g);
  auto it = s.s16.find(key);
  if (it != s.s16.end()) return &it->second;
  if (!create) return nullptr;
  Surface16 &n = s.s16[key];
  n.make(g.root.w, g.root.h);
  return &n;
}

}  // namespace

namespace gfxh {

void after_present(Gfx &g, Ctx &) {
  static const char *env = std::getenv("SG_GFX_HUD_BGFILL");
  if (!env || !*env) return;
  const uint16_t w = static_cast<uint16_t>(std::strtoul(env, nullptr, 0));
  std::fill(g.root.px.begin(), g.root.px.end(), w);
}

Surface16 *family_surface(Gfx &g, uint32_t key) {
  auto &m = st(g).s16;
  auto it = m.find(key);
  return it == m.end() ? nullptr : &it->second;
}

std::array<uint16_t, 256> *palette_of(Gfx &g, uint32_t key, bool create) {
  auto &m = st(g).palettes;
  auto it = m.find(key);
  if (it != m.end()) return &it->second;
  return create ? &m[key] : nullptr;
}

bool surface_blit3(Gfx &g, Ctx &ctx, const uint32_t *a) {
  g.ensure_root(ctx);
  const int32_t x = static_cast<int32_t>(a[2]), y = static_cast<int32_t>(a[3]), w = static_cast<int32_t>(a[4]),
                h = static_cast<int32_t>(a[5]), x2 = static_cast<int32_t>(a[6]), y2 = static_cast<int32_t>(a[7]),
                w2 = static_cast<int32_t>(a[8]), h2 = static_cast<int32_t>(a[9]);
  Surface16 *src = surface16(g, ctx, a[0], false);
  const bool img_src = src == nullptr && g.images.count(a[0]) != 0;
  if (src == nullptr && !img_src) {
    dlog("blit3 no-source", a, 10);
    return false;
  }
  if (w <= 0 || h <= 0 || w2 <= 0 || h2 <= 0 || w2 < w || h2 < h) {
    dlog("blit3 shrink-or-empty", a, 10);
    g.unsup("Surface_blit3: a shrinking or empty rectangle");
    return true;
  }
  Surface16 *dst = surface16(g, ctx, a[1], true);
  // the source as 16-bit words: a family surface or the root is read from a copy (one surface may be both ends)
  Surface16 tmp;
  if (img_src) {
    const Image8 &im = g.images[a[0]];
    tmp.make(im.w, im.h);
    gfxb::copy8(im, tmp, 0, 0, im.w, im.h, 0, 0);
  } else {
    tmp = *src;
  }
  const bool scaled = w != w2 || h != h2;
  const int32_t rows = mode() == 1 && !scaled ? h2 - 1 : h2;
  for (int32_t r = 0; r < rows; ++r) {
    const int32_t dy = add32(y2, r);
    const int32_t sy = add32(y, static_cast<int32_t>((static_cast<int64_t>(r) * h) / h2));
    if (dy < 0 || dy >= dst->h || sy < 0 || sy >= tmp.h) continue;
    for (int32_t c = 0; c < w2; ++c) {
      const int32_t dx = add32(x2, c);
      const int64_t step = mode() == 2 && scaled ? c + 1 : c;
      const int32_t sx = add32(x, static_cast<int32_t>((step * w) / w2));
      if (dx < 0 || dx >= dst->w || sx < 0 || sx >= tmp.w) continue;
      uint16_t &o = dst->px[static_cast<size_t>(dy) * static_cast<size_t>(dst->w) + static_cast<size_t>(dx)];
      o = tmp.px[static_cast<size_t>(sy) * static_cast<size_t>(tmp.w) + static_cast<size_t>(sx)];
      hud_mark(&o);  // P10C fix 2
    }
  }
  return true;
}

}  // namespace gfxh

// ------------------------------------------------------------------------------------------------ the interface art
// loadInterfaceArt(1)'s image half (c3fv.cpp LoadInterfaceArt_re after its full == 0 return): a working frame Image that
// every load replaces, and cuts from it into the global SpriteRefs (Surface_blit = cropSprite, gfxb::crop_sprite). The
// generated function iart_images (below) is the body's order of loads and cuts with its loops as written; every address
// in it is an offset from the GameState's start (GSO), which the helpers add to the state's base.
namespace {

enum ArtKind : uint8_t { ArtLoad, ArtCut };
struct ArtOp {
  ArtKind k;
  uint32_t a, b, c, d, e, f, h;   // Load: path, palette object, ofs, count, flags; Cut: dest, x, y, w, h, kind, trim
};

struct Art {
  Gfx &g;
  Ctx &ctx;
  uint32_t base;
  Image8 img;
  // loadImage(img, path, palette, ofs, count, flags) (c3ee.cpp LoadImage_re): a file that does not open (6) leaves the
  // image as it was; a palette object argument gets the file's palette (kept in the family state for later draws)
  void load(uint32_t path, uint32_t pal, int32_t ofs, int32_t count, uint32_t flags) {
    Image8 t;
    if (gfxb::load_image(g, ctx, base + path, pal ? base + pal : 0, ofs, count, flags, t) != 0) return;
    img = std::move(t);
    if (pal) st(g).palettes[base + pal] = img.pal;
  }
  void load_pcx(uint32_t path) { load(path, 0, 0, 0x100, 2); }
  // Surface_blit(dest, img, x, y, w, h, kind, trim) (c3h.cpp SurfaceBlit_re -> cropSprite)
  void cut(uint32_t dest, int32_t x, int32_t y, int32_t w, int32_t h, int32_t, int32_t trim) {
    gfxb::crop_sprite(g, base + dest, img, x, y, w, h, (static_cast<uint8_t>(trim) != 0) != (mode() == 4));
  }
  // Surface_createFull on the frame Image: its handle is replaced by an empty surface (no pixels a cut can read)
  void reset_image() { img = Image8(); }
  // Surface_createFull on a global Surface: the backend's entries of that key are dropped (gfx_surface.cpp's rule)
  void erase(uint32_t key) {
    g.images.erase(base + key);
    g.sprites.erase(base + key);
  }
  void run(const ArtOp *op, size_t n) {
    for (size_t i = 0; i < n; ++i, ++op) {
      if (op->k == ArtLoad)
        load(op->a, op->b, static_cast<int32_t>(op->c), static_cast<int32_t>(op->d), op->e);
      else
        cut(op->a, static_cast<int32_t>(op->b), static_cast<int32_t>(op->c), static_cast<int32_t>(op->d),
            static_cast<int32_t>(op->e), static_cast<int32_t>(op->f), static_cast<int32_t>(op->h));
    }
  }
  // c3fv.cpp's cut helpers, as written there: Row (n cells, the origin stepping (dx, dy)), Grid (rows x cols), TableRow
  // (rectangles from two short tables of the exe's data: x, y pairs at xy and w, h pairs at wh), SetlRow, PairRows, WideRow
  void row(uint32_t dest, int n, int x, int dx, int y, int dy, int w, int h, int a, int b) {
    for (int i = 0; i < n; i++, dest += 0x2C, x += dx, y += dy) cut(dest, x, y, w, h, a, b);
  }
  void grid(uint32_t dest, int rows, int cols, int x0, int dx, int y0, int dy, int w, int h, int a, int b) {
    for (int r = 0; r < rows; r++)
      for (int c = 0; c < cols; c++, dest += 0x2C) cut(dest, x0 + dx * c, y0 + dy * r, w, h, a, b);
  }
  int16_t s16(uint32_t off) { return ctx.mem.load<int16_t>(base + off); }
  void table_row(uint32_t dest, uint32_t end, uint32_t xy, uint32_t wh) {
    for (uint32_t k = 0; dest < end; dest += 0x2C, k += 4) cut(dest, s16(xy + k), s16(xy + 2 + k), s16(wh + k), s16(wh + 2 + k), 1, 0);
  }
  void setl_row(uint32_t dest) {
    int i = 0;
    for (int x = 3; x < 0x2C3; x += 0x58, i++, dest += 0x2C) cut(dest, x, 0x1E0 + (i < 4 ? 1 : 0), 0x50, 0x50, 1, 0);
  }
  void pair_rows(uint32_t b) {
    for (int y0 = 0xC8; y0 < 0x12C; y0 += 0x32, b += 0x84) {
      uint32_t d = b;
      for (int j = 0, y = y0; j < 2; j++, d += 0x2C, y += 0x14) cut(d, 0, y, 0x23, 0x0C, 1, 0);
    }
  }
  void wide_row(uint32_t dest, int w0, int h0, int h1) {
    int i = 0;
    for (int x = 2; x < 0x322; x += 0xC8, i++, dest += 0x2C) cut(dest, x, 0, i >= 2 ? 0xB6 : w0, i >= 2 ? h1 : h0, 1, 1);
  }
  // LoadHeads(ctr, up, low, base) (c3fv.cpp): the heads listing listMatchingFiles left in g_fileNames (0x64-byte names;
  // loaders.cpp clears the table before it lists, so the listing ends at the first empty name); a counter from 0x13 that
  // stops the scan at 0x48; each name starting with `up` or `low` is loaded from the heads directory (f_strpool_4cd6a4 + 0,
  // the text menu.cpp's load_heads builds) and cut into three Surfaces 0x2c apart at rows 0, 0x8c, 0x118
  void heads(uint32_t, char up, char low, uint32_t b) {
    const uint32_t names = state_va(ctx, &GameState::g_fileNames) - base;
    const uint32_t n_max = static_cast<uint32_t>(sizeof(ctx.gs().g_fileNames) / 0x64);
    const std::string dir = gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4cd6a4));
    int ctr = 0x13;
    for (uint32_t i = 0; i < n_max; ++i) {
      const uint32_t name = base + names + 0x64 * i;
      const char c = static_cast<char>(ctx.mem.load<uint8_t>(name));
      if (c == 0) break;
      if (ctr >= 0x48) break;
      if (c != up && c != low) continue;
      const std::string path = dir + gfxb::vstring(ctx.mem, name);
      // the path built in a scratch block of the backend's own: loadImage reads its name from the address space
      Image8 t;
      if (gfxb::load_image_named(g, ctx, path, 0, 0, 0x100, 2, t) == 0) img = std::move(t);
      cut(b + 0x84 * ctr, 0, 0, 0x8C, 0x8C, 1, 1);
      cut(b + 0x2C + 0x84 * ctr, 0, 0x8C, 0x8C, 0x8C, 1, 1);
      cut(b + 0x58 + 0x84 * ctr, 0, 0x118, 0x8C, 0x8C, 1, 1);
      ++ctr;
    }
  }
};

#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))
// BEGIN GENERATED loadInterfaceArt images (log/roadmap/S3/6_gen_iart.py)
// 48 tables, 236 body lines; regenerate after changing c3fv.cpp's LoadInterfaceArt_re
const ArtOp kIa1[] = {
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2f4c), GSO(g_images, 0x0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x248
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2f4c), GSO(g_images, 0x58), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x262
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2f38), GSO(g_images, 0xb0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x27c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2f24), GSO(g_images, 0x108), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x296
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2f10), GSO(g_images, 0x160), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x2b0
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2efc), GSO(g_images, 0x1b8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x2ca
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ee8), GSO(g_images, 0x210), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x2e4
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ed4), GSO(g_images, 0x268), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x2fe
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ec0), GSO(g_images, 0x2c0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x318
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2eac), GSO(g_images, 0x318), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x332
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e98), GSO(g_images, 0x370), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x34c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e88), GSO(g_images, 0xa50), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x366
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e70), GSO(g_images, 0x1290), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x380
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e58), GSO(g_images, 0x12e8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x39a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e40), GSO(g_images, 0x1340), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x3b4
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e28), GSO(g_images, 0x1398), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x3ce
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2e0c), GSO(g_images, 0x13f0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x3e8
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2df4), GSO(g_images, 0x1448), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x402
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2dd4), GSO(g_images, 0x14f8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x41c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2dbc), GSO(g_images, 0x1658), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x436
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2d9c), GSO(g_images, 0x16b0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x450
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2d80), GSO(g_images, 0x1708), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x46a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2d58), GSO(g_images, 0x1760), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x484
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2d38), GSO(g_images, 0x17b8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x49e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2d18), GSO(g_images, 0x1810), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x4b8
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2cfc), GSO(g_images, 0x1a20), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x4d2
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2cd8), GSO(g_images, 0x1b80), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x4ec
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2cbc), GSO(g_images, 0x1c30), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x506
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ca4), GSO(g_images, 0x1ce0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x520
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2c84), GSO(g_images, 0x1d38), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x53a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2c68), GSO(g_images, 0x1d90), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x554
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2c48), GSO(g_images, 0x1de8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x56e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2c30), GSO(g_images, 0x1e40), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x588
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2c10), GSO(g_images, 0x1e98), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x5a2
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2bf4), GSO(g_images, 0x1f48), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x5bc
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2bd8), GSO(g_images, 0x1fa0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x5d6
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2bc0), GSO(g_images, 0x2050), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x5f0
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ba4), GSO(g_images, 0x20a8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x60a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2b88), GSO(g_images, 0x23c0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x624
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2b68), GSO(g_images, 0x26d8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x63e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2b48), GSO(g_images, 0x24c8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x658
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2b30), GSO(g_images, 0x22b8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x672
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2b14), GSO(g_images, 0x2578), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x68c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2af8), GSO(g_images, 0x2368), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x6a6
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ad8), GSO(g_images, 0x25d0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x6c0
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2abc), GSO(g_images, 0x2628), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x6da
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2aa0), GSO(g_images, 0x2520), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x6f4
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2a84), GSO(g_images, 0x2680), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x70e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2a68), GSO(g_images, 0x2260), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x728
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2a48), GSO(g_images, 0x2730), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x742
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2a24), GSO(g_images, 0x2310), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x75c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2a00), GSO(g_images, 0x2470), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x776
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x29e4), GSO(g_images, 0x2788), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x790
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x29c4), GSO(g_images, 0x2418), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x7aa
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x29a8), GSO(g_images, 0x27e0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x7c4
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2988), GSO(g_images, 0x2838), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x7de
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2964), GSO(g_images, 0x2890), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x7f8
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2948), GSO(g_images, 0x2940), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x812
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2930), GSO(g_images, 0x2998), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x82c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2914), GSO(g_images, 0x29f0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x846
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x28f8), GSO(g_images, 0x2a48), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x860
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x28dc), GSO(g_images, 0x2aa0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x87a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x28c0), GSO(g_images, 0x2af8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x894
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x28a4), GSO(g_images, 0x2b50), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x8ae
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2888), GSO(g_images, 0x2ba8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x8c8
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x286c), GSO(g_images, 0x2c00), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x8e2
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2854), GSO(g_images, 0x2c58), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x8fc
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2838), GSO(g_images, 0x2cb0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x916
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x281c), GSO(g_images, 0x2d08), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x930
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2804), GSO(g_images, 0x2d60), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x94a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x27e8), GSO(g_images, 0x2db8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x964
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x27cc), GSO(g_images, 0x2e10), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x97e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x27b0), GSO(g_images, 0x2e68), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x998
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2794), GSO(g_images, 0x2ec0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x9b2
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2774), GSO(g_images, 0x2f18), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x9cc
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2760), GSO(g_images, 0x3020), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0x9e6
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x274c), GSO(g_images, 0x3078), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa00
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2738), GSO(g_images, 0x30d0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa1a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2724), GSO(g_images, 0x3128), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa34
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2710), GSO(g_images, 0x3180), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa4e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x26fc), GSO(g_images, 0x31d8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa68
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x26e8), GSO(g_images, 0x3230), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa82
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x26d4), GSO(g_images, 0x3288), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xa9c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x26c0), GSO(g_images, 0x32e0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xab6
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x26ac), GSO(g_images, 0x3338), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xad0
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2694), GSO(g_images, 0x3390), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xaea
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2678), GSO(g_images, 0x40a0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xb04
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x265c), GSO(g_images, 0x3700), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xb1e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2640), GSO(g_images, 0x3758), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xb38
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x261c), GSO(g_images, 0x37b0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xb52
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2604), GSO(g_images, 0x3808), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xb6c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x25e4), GSO(g_images, 0x3860), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xb86
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x25c4), GSO(g_images, 0x38b8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xba0
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x25a8), GSO(g_images, 0x3910), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xbba
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2580), GSO(g_images, 0x3968), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xbd4
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x255c), GSO(g_images, 0x39c0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xbee
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2540), GSO(g_images, 0x3a18), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xc08
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2528), GSO(g_images, 0x3a70), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xc22
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x250c), GSO(g_images, 0x3ac8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xc3c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x24f0), GSO(g_images, 0x3b20), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xc56
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x24cc), GSO(g_images, 0x3b78), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xc70
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x24ac), GSO(g_images, 0x3bd0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xc8a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x248c), GSO(g_images, 0x3c28), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xca4
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2474), GSO(g_images, 0x3c80), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xcbe
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x245c), GSO(g_images, 0x3cd8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xcd8
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2448), GSO(g_images, 0x3d88), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xcf2
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2430), GSO(g_images, 0x3de0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xd0c
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2418), GSO(g_images, 0x3e38), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xd26
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x25a8), GSO(g_images, 0x3e90), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xd40
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x23f4), GSO(g_images, 0x3ee8), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xd5a
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x23dc), GSO(g_images, 0x3f40), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xd74
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x2ba4), GSO(g_images, 0x3f98), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xd8e
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x23c0), GSO(g_images, 0x3ff0), 0x0, 0x100, 0x0, 0, 0},  // loadInterfaceArt+0xda8
};
const ArtOp kIa2[] = {
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x84), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x2db4
};
const ArtOp kIa3[] = {
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x60), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x2e6d
};
const ArtOp kIa4[] = {
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x38), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x2f28
};
const ArtOp kIa5[] = {
    {ArtLoad, GSO(f_strpool_4cd6a4, 0x14), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x2fd0
};
const ArtOp kIa6[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x408c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x333f
    {ArtCut, GSO(g_surfaces53ba48, 0x0), 0xe0, 0xe5, 0x20, 0x16, 0x1, 0x1},  // loadInterfaceArt+0x3360
    {ArtLoad, GSO(f_strpool_4c95f4, 0x406c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3379
    {ArtCut, GSO(g_surfaces53ba48, 0x2c), 0xe0, 0xe5, 0x20, 0x16, 0x1, 0x1},  // loadInterfaceArt+0x339a
    {ArtLoad, GSO(f_strpool_4c95f4, 0x4050), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x33b3
    {ArtCut, GSO(g_surfaces53ba48, 0x58), 0xe0, 0xe5, 0x20, 0x16, 0x1, 0x1},  // loadInterfaceArt+0x33d4
    {ArtLoad, GSO(f_strpool_4c95f4, 0x4024), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x33ed
    {ArtCut, GSO(g_surfaces53ba48, 0x84), 0xe0, 0xe5, 0x20, 0x16, 0x1, 0x1},  // loadInterfaceArt+0x340e
    {ArtLoad, GSO(f_strpool_4c95f4, 0x400c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3427
    {ArtCut, GSO(g_surfaces53ba48, 0xb0), 0x0, 0x0, 0x20, 0x20, 0x1, 0x1},  // loadInterfaceArt+0x3442
    {ArtCut, GSO(g_surfaces53ba48, 0xdc), 0x20, 0x0, 0x20, 0x20, 0x1, 0x1},  // loadInterfaceArt+0x345d
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3ffc), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3476
};
const ArtOp kIa7[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3fe0), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x34be
};
const ArtOp kIa8[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3fc4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3503
};
const ArtOp kIa9[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3e3c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x36f4
    {ArtCut, GSO(g_sprite5787a0, 0x0), 0x0, 0x0, 0x3c, 0x78, 0x1, 0x1},  // loadInterfaceArt+0x370f
    {ArtLoad, GSO(f_strpool_4c5ec8, 0x1c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3728
    {ArtCut, GSO(g_sprite58b650, 0x0), 0xc0, 0x0, 0x2, 0x2, 0x1, 0x1},  // loadInterfaceArt+0x3746
    {ArtCut, GSO(g_sprite5791c8, 0x0), 0xc0, 0x2, 0x2, 0x2, 0x1, 0x1},  // loadInterfaceArt+0x3764
    {ArtCut, GSO(g_sprite567b08, 0x0), 0xc4, 0x0, 0x4, 0x4, 0x1, 0x1},  // loadInterfaceArt+0x3782
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3e2c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x37eb
};
const ArtOp kIa10[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3dd8), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3990
};
const ArtOp kIa11[] = {
    {ArtCut, GSO(g_sprite5787d0, 0x0), 0x200, 0x80, 0x20, 0xa, 0x1, 0x1},  // loadInterfaceArt+0x3a0f
    {ArtCut, GSO(g_sprite5a4100, 0x0), 0x200, 0x8c, 0x8, 0x8, 0x1, 0x1},  // loadInterfaceArt+0x3a30
    {ArtCut, GSO(g_sprite5423e8, 0x0), 0x225, 0xd7, 0x2b, 0x15, 0x1, 0x1},  // loadInterfaceArt+0x3a51
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3dc0), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3a6a
};
const ArtOp kIa12[] = {
    {ArtCut, GSO(g_sprites53fd48, 0x3c8), 0x11b, 0x165, 0xf8, 0x92, 0x1, 0x0},  // loadInterfaceArt+0x3af8
    {ArtCut, GSO(g_sprites53fd48, 0x3f4), 0x114, 0x124, 0x101, 0x2f, 0x1, 0x0},  // loadInterfaceArt+0x3b1c
    {ArtCut, GSO(g_sprites53fd48, 0x420), 0x13d, 0x1ea, 0xb7, 0x34, 0x1, 0x0},  // loadInterfaceArt+0x3b40
    {ArtCut, GSO(g_sprites53fd48, 0x44c), 0x9, 0xba, 0xf8, 0x13, 0x1, 0x0},  // loadInterfaceArt+0x3b61
    {ArtCut, GSO(g_sprites53fd48, 0x478), 0x9, 0x74, 0xf8, 0x1e, 0x1, 0x0},  // loadInterfaceArt+0x3b7f
    {ArtCut, GSO(g_sprites53fd48, 0x4a4), 0x1fe, 0xd, 0x84, 0xa2, 0x1, 0x0},  // loadInterfaceArt+0x3ba3
    {ArtCut, GSO(g_sprites53fd48, 0x4d0), 0x289, 0xd, 0x82, 0xa3, 0x1, 0x0},  // loadInterfaceArt+0x3bc7
};
const ArtOp kIa13[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3dac), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3c15
    {ArtCut, GSO(g_sprites53fd48, 0x580), 0x29c, 0x1ff, 0x35, 0x34, 0x1, 0x0},  // loadInterfaceArt+0x3c36
    {ArtCut, GSO(g_sprites53fd48, 0x5ac), 0x3f, 0xfb, 0xe5, 0xd1, 0x1, 0x0},  // loadInterfaceArt+0x3c5a
    {ArtCut, GSO(g_sprites53fd48, 0x5d8), 0x3a, 0x31, 0xcf, 0xb2, 0x1, 0x0},  // loadInterfaceArt+0x3c7b
    {ArtCut, GSO(g_sprites53fd48, 0x604), 0x33, 0x1e2, 0xb8, 0x50, 0x1, 0x0},  // loadInterfaceArt+0x3c9c
    {ArtCut, GSO(g_sprites53fd48, 0x630), 0x2dc, 0x1ff, 0x35, 0x34, 0x1, 0x0},  // loadInterfaceArt+0x3cbd
};
const ArtOp kIa14[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d94), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3d60
};
const ArtOp kIa15[] = {
    {ArtCut, GSO(g_sprites5a34f8, 0x3c8), 0x11b, 0x165, 0xf8, 0x92, 0x1, 0x0},  // loadInterfaceArt+0x3dee
    {ArtCut, GSO(g_sprites5a34f8, 0x3f4), 0x114, 0x124, 0x101, 0x2f, 0x1, 0x0},  // loadInterfaceArt+0x3e12
    {ArtCut, GSO(g_sprites5a34f8, 0x420), 0x13d, 0x1ea, 0xb7, 0x34, 0x1, 0x0},  // loadInterfaceArt+0x3e36
    {ArtCut, GSO(g_sprites5a34f8, 0x44c), 0x9, 0xba, 0xf8, 0x13, 0x1, 0x0},  // loadInterfaceArt+0x3e57
    {ArtCut, GSO(g_sprites5a34f8, 0x478), 0x9, 0x74, 0xf8, 0x1e, 0x1, 0x0},  // loadInterfaceArt+0x3e75
    {ArtCut, GSO(g_sprites5a34f8, 0x4a4), 0x1fe, 0xd, 0x84, 0xa2, 0x1, 0x0},  // loadInterfaceArt+0x3e99
    {ArtCut, GSO(g_sprites5a34f8, 0x4d0), 0x289, 0xd, 0x82, 0xa3, 0x1, 0x0},  // loadInterfaceArt+0x3ebd
};
const ArtOp kIa16[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d7c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x3f0b
    {ArtCut, GSO(g_sprites5a34f8, 0x630), 0x2dc, 0x1ff, 0x35, 0x34, 0x1, 0x0},  // loadInterfaceArt+0x3f2c
    {ArtCut, GSO(g_sprites5a34f8, 0x580), 0x29c, 0x1ff, 0x35, 0x34, 0x1, 0x0},  // loadInterfaceArt+0x3f4d
    {ArtCut, GSO(g_sprites5a34f8, 0x5ac), 0x3f, 0xfb, 0xe5, 0xd1, 0x1, 0x0},  // loadInterfaceArt+0x3f71
    {ArtCut, GSO(g_sprites5a34f8, 0x5d8), 0x3a, 0x31, 0xcf, 0xb2, 0x1, 0x0},  // loadInterfaceArt+0x3f92
    {ArtCut, GSO(g_sprites5a34f8, 0x604), 0x33, 0x1e2, 0xb8, 0x50, 0x1, 0x0},  // loadInterfaceArt+0x3fb3
};
const ArtOp kIa17[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d64), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4056
};
const ArtOp kIa18[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d4c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4111
};
const ArtOp kIa19[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d34), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x419d
    {ArtCut, GSO(g_sprites5a53c8, 0x18c), 0x13d, 0x0, 0x10, 0x10, 0x1, 0x1},  // loadInterfaceArt+0x41bb
};
const ArtOp kIa20[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d1c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4209
};
const ArtOp kIa21[] = {
    {ArtCut, GSO(g_sprites5a53c8, 0x528), 0x0, 0x96, 0x28, 0x18, 0x1, 0x0},  // loadInterfaceArt+0x42ac
    {ArtCut, GSO(g_sprites5a53c8, 0x554), 0x0, 0xc8, 0x23, 0xc, 0x1, 0x0},  // loadInterfaceArt+0x42ca
    {ArtCut, GSO(g_sprites5a53c8, 0x580), 0x0, 0xdc, 0x23, 0xc, 0x1, 0x0},  // loadInterfaceArt+0x42e8
};
const ArtOp kIa22[] = {
    {ArtCut, GSO(g_sprites5a53c8, 0x478), 0x20, 0x64, 0xbf, 0x18, 0x1, 0x0},  // loadInterfaceArt+0x43bf
    {ArtCut, GSO(g_sprites5a53c8, 0x4fc), 0x0, 0x12c, 0x8c, 0x8c, 0x1, 0x0},  // loadInterfaceArt+0x43e3
};
const ArtOp kIa23[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3d04), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x444d
};
const ArtOp kIa24[] = {
    {ArtCut, GSO(g_sprites58b680, 0x528), 0x0, 0x96, 0x28, 0x18, 0x1, 0x0},  // loadInterfaceArt+0x44f0
    {ArtCut, GSO(g_sprites58b680, 0x554), 0x0, 0xc8, 0x23, 0xc, 0x1, 0x0},  // loadInterfaceArt+0x450e
    {ArtCut, GSO(g_sprites58b680, 0x580), 0x0, 0xdc, 0x23, 0xc, 0x1, 0x0},  // loadInterfaceArt+0x452c
};
const ArtOp kIa25[] = {
    {ArtCut, GSO(g_sprites58b680, 0x478), 0x20, 0x64, 0xbf, 0x18, 0x1, 0x0},  // loadInterfaceArt+0x4603
    {ArtCut, GSO(g_sprites58b680, 0x4fc), 0x0, 0x12c, 0x8c, 0x8c, 0x1, 0x0},  // loadInterfaceArt+0x4627
};
const ArtOp kIa26[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3cec), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4691
};
const ArtOp kIa27[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3cd4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x46e9
};
const ArtOp kIa28[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3cc0), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4741
};
const ArtOp kIa29[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3ca8), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x478c
};
const ArtOp kIa30[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c90), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x47d7
    {ArtCut, GSO(g_sprites542f28, 0x0), 0x30, 0x6, 0xb7, 0x3a, 0x1, 0x0},  // loadInterfaceArt+0x47f5
    {ArtCut, GSO(g_sprites542f28, 0x2c), 0x287, 0xd, 0x8a, 0x24, 0x1, 0x0},  // loadInterfaceArt+0x4816
    {ArtCut, GSO(g_sprites542f28, 0x58), 0x2a3, 0x37, 0x6e, 0x25, 0x1, 0x0},  // loadInterfaceArt+0x4834
    {ArtCut, GSO(g_sprites542f28, 0x84), 0x2b9, 0x63, 0x58, 0x24, 0x1, 0x0},  // loadInterfaceArt+0x4852
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c78), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x486b
    {ArtCut, GSO(g_sprites59ab00, 0x0), 0x30, 0x6, 0xb7, 0x3a, 0x1, 0x0},  // loadInterfaceArt+0x4889
    {ArtCut, GSO(g_sprites59ab00, 0x2c), 0x287, 0xd, 0x8a, 0x24, 0x1, 0x0},  // loadInterfaceArt+0x48aa
    {ArtCut, GSO(g_sprites59ab00, 0x58), 0x2a3, 0x37, 0x6e, 0x25, 0x1, 0x0},  // loadInterfaceArt+0x48c8
    {ArtCut, GSO(g_sprites59ab00, 0x84), 0x2b9, 0x63, 0x58, 0x24, 0x1, 0x0},  // loadInterfaceArt+0x48e6
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c60), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x48ff
    {ArtCut, GSO(g_sprites520590, 0x0), 0x30, 0x6, 0xb7, 0x3a, 0x1, 0x1},  // loadInterfaceArt+0x491d
    {ArtCut, GSO(g_sprites520590, 0x2c), 0x287, 0xd, 0x8a, 0x24, 0x1, 0x1},  // loadInterfaceArt+0x493e
    {ArtCut, GSO(g_sprites520590, 0x58), 0x2a3, 0x37, 0x6e, 0x25, 0x1, 0x1},  // loadInterfaceArt+0x495c
    {ArtCut, GSO(g_sprites520590, 0x84), 0x2b9, 0x63, 0x58, 0x24, 0x1, 0x1},  // loadInterfaceArt+0x497a
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c4c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4993
};
const ArtOp kIa31[] = {
    {ArtCut, GSO(g_sprites59df20, 0x6e0), 0x12c, 0x1f4, 0x39, 0x26, 0x1, 0x1},  // loadInterfaceArt+0x4ada
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c34), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4af3
    {ArtCut, GSO(g_sprites59df20, 0x70c), 0x0, 0x102, 0x320, 0x156, 0x1, 0x1},  // loadInterfaceArt+0x4b17
    {ArtCut, GSO(g_sprites59df20, 0x738), 0x1b0, 0x69, 0x57, 0x98, 0x1, 0x1},  // loadInterfaceArt+0x4b38
    {ArtCut, GSO(g_sprites59df20, 0x764), 0x207, 0x69, 0x57, 0x98, 0x1, 0x1},  // loadInterfaceArt+0x4b59
    {ArtCut, GSO(g_sprites59df20, 0x790), 0x266, 0x0, 0x5d, 0x98, 0x1, 0x1},  // loadInterfaceArt+0x4b7a
    {ArtCut, GSO(g_sprites59df20, 0x7bc), 0x2c3, 0x0, 0x5d, 0x98, 0x1, 0x1},  // loadInterfaceArt+0x4b9b
    {ArtCut, GSO(g_sprites59df20, 0x7e8), 0x0, 0x0, 0x85, 0x89, 0x1, 0x1},  // loadInterfaceArt+0x4bbc
    {ArtCut, GSO(g_sprites59df20, 0x814), 0x96, 0x0, 0x85, 0x89, 0x1, 0x1},  // loadInterfaceArt+0x4be0
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c1c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4bf9
    {ArtCut, GSO(g_sprites58b390, 0x18c), 0x40, 0x21, 0x192, 0xf4, 0x1, 0x0},  // loadInterfaceArt+0x4c1a
    {ArtCut, GSO(g_sprites58b390, 0x1b8), 0x40, 0x12c, 0x192, 0x8a, 0x1, 0x0},  // loadInterfaceArt+0x4c3e
};
const ArtOp kIa32[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3c04), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4c92
    {ArtCut, GSO(g_sprite59bf98, 0x0), 0x0, 0x0, 0x73, 0xe5, 0x1, 0x1},  // loadInterfaceArt+0x4cb0
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3bec), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4cc9
    {ArtCut, GSO(g_sprites541a18, 0x1e4), 0x40, 0x21, 0x192, 0xf4, 0x1, 0x1},  // loadInterfaceArt+0x4cea
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3bd4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4d03
    {ArtCut, GSO(g_sprites541a18, 0x18c), 0x40, 0x21, 0x192, 0xf4, 0x1, 0x0},  // loadInterfaceArt+0x4d24
    {ArtCut, GSO(g_sprites541a18, 0x1b8), 0x40, 0x12c, 0x192, 0x8a, 0x1, 0x0},  // loadInterfaceArt+0x4d48
};
const ArtOp kIa33[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3bb8), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4d9c
    {ArtCut, GSO(g_sprites58b390, 0x0), 0x0, 0x1b3, 0xd7, 0xa5, 0x1, 0x0},  // loadInterfaceArt+0x4dc0
};
const ArtOp kIa34[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3b9c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4e4f
    {ArtCut, GSO(g_sprites541a18, 0x0), 0x0, 0x1b3, 0xd7, 0xa5, 0x1, 0x0},  // loadInterfaceArt+0x4e73
};
const ArtOp kIa35[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3b7c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4f34
    {ArtCut, GSO(g_sprites58b390, 0x2c), 0xd8, 0x1e2, 0x248, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x4f58
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3b60), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4f71
    {ArtCut, GSO(g_sprites541a18, 0x2c), 0xd8, 0x1e2, 0x248, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x4f95
    {ArtCut, GSO(g_sprites570cb8, 0xe70), 0x28, 0x226, 0x22, 0x28, 0x1, 0x1},  // loadInterfaceArt+0x4fb3
    {ArtCut, GSO(g_sprites570cb8, 0xf20), 0x96, 0x1e0, 0x22, 0x22, 0x1, 0x1},  // loadInterfaceArt+0x4fd4
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3b44), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x4fed
    {ArtCut, GSO(g_sprites58b390, 0x160), 0xd7, 0x1e2, 0x249, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x5011
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3b28), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x502a
    {ArtCut, GSO(g_sprites541a18, 0x160), 0xd7, 0x1e2, 0x249, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x504e
    {ArtCut, GSO(g_sprites58b1a0, 0x108), 0x0, 0x190, 0x29, 0x2c, 0x1, 0x1},  // loadInterfaceArt+0x506c
};
const ArtOp kIa36[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3b0c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x511b
    {ArtCut, GSO(g_sprites58b390, 0x58), 0xd8, 0x1e2, 0x248, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x513f
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3af4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5158
    {ArtCut, GSO(g_sprites541a18, 0x58), 0xd8, 0x1e2, 0x248, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x517c
    {ArtCut, GSO(g_sprites58b1a0, 0xb0), 0x96, 0x1e0, 0x22, 0x22, 0x1, 0x1},  // loadInterfaceArt+0x519d
    {ArtCut, GSO(g_sprites5a7c08, 0x0), 0x0, 0x0, 0x4a, 0x40, 0x1, 0x1},  // loadInterfaceArt+0x51b8
    {ArtCut, GSO(g_sprites5a7c08, 0x2c), 0x64, 0x0, 0x4d, 0x40, 0x1, 0x1},  // loadInterfaceArt+0x51d3
    {ArtCut, GSO(g_sprites5a7c08, 0x58), 0xc8, 0x0, 0x4d, 0x40, 0x1, 0x1},  // loadInterfaceArt+0x51f1
    {ArtCut, GSO(g_sprites5a7c08, 0xb0), 0x0, 0x64, 0x4d, 0x3a, 0x1, 0x1},  // loadInterfaceArt+0x520c
    {ArtCut, GSO(g_sprites5a7c08, 0xdc), 0x64, 0x64, 0x4d, 0x3a, 0x1, 0x1},  // loadInterfaceArt+0x5227
    {ArtCut, GSO(g_sprites5a7c08, 0x108), 0xc8, 0x64, 0x4d, 0x3a, 0x1, 0x1},  // loadInterfaceArt+0x5245
    {ArtCut, GSO(g_sprites5a7c08, 0x160), 0x28, 0x226, 0x22, 0x28, 0x1, 0x1},  // loadInterfaceArt+0x5263
    {ArtCut, GSO(g_sprites5a7c08, 0x18c), 0x50, 0x226, 0x22, 0x28, 0x1, 0x1},  // loadInterfaceArt+0x5281
    {ArtCut, GSO(g_sprites5a7c08, 0x1b8), 0x78, 0x226, 0x22, 0x28, 0x1, 0x1},  // loadInterfaceArt+0x529f
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3ad8), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x52b8
    {ArtCut, GSO(g_sprites58b390, 0x84), 0xd6, 0x1e2, 0x24a, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x52dc
    {ArtCut, GSO(g_sprites542e50, 0x0), 0x17c, 0xfa, 0x44, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x52fd
    {ArtCut, GSO(g_sprites542e50, 0x2c), 0x1c2, 0xfa, 0x2f, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x531e
    {ArtCut, GSO(g_sprites542e50, 0x58), 0x258, 0xfa, 0x43, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x533f
    {ArtCut, GSO(g_sprites542e50, 0x84), 0x1f4, 0xfa, 0x43, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x5360
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3abc), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5379
    {ArtCut, GSO(g_sprites541a18, 0x84), 0xd6, 0x1e2, 0x24a, 0x76, 0x1, 0x0},  // loadInterfaceArt+0x539d
    {ArtCut, GSO(g_sprites58b1a0, 0xdc), 0x0, 0x190, 0x22, 0x22, 0x1, 0x1},  // loadInterfaceArt+0x53bb
};
const ArtOp kIa37[] = {
    {ArtCut, GSO(g_sprite58b358, 0x0), 0x32, 0x226, 0x26, 0x24, 0x1, 0x1},  // loadInterfaceArt+0x5519
    {ArtCut, GSO(g_sprite56a760, 0x0), 0x0, 0x226, 0x26, 0x24, 0x1, 0x1},  // loadInterfaceArt+0x5538
    {ArtCut, GSO(g_sprites55e930, 0x0), 0x17c, 0xfa, 0x44, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x5558
    {ArtCut, GSO(g_sprites55e930, 0x2c), 0x1c2, 0xfa, 0x2f, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x5578
    {ArtCut, GSO(g_sprites55e930, 0x58), 0x258, 0xfa, 0x43, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x5598
    {ArtCut, GSO(g_sprites55e930, 0x84), 0x1f4, 0xfa, 0x43, 0x4f, 0x1, 0x0},  // loadInterfaceArt+0x55b8
};
const ArtOp kIa38[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3aa4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x561f
    {ArtCut, GSO(g_sprites58b390, 0xb0), 0xd7, 0x1da, 0x249, 0x7e, 0x1, 0x0},  // loadInterfaceArt+0x5642
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3a8c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5659
    {ArtCut, GSO(g_sprites541a18, 0xb0), 0xd7, 0x1da, 0x249, 0x7e, 0x1, 0x0},  // loadInterfaceArt+0x567c
    {ArtCut, GSO(g_sprites58b1a0, 0x0), 0x0, 0x190, 0x21, 0x21, 0x1, 0x1},  // loadInterfaceArt+0x5699
    {ArtCut, GSO(g_sprites58b1a0, 0x2c), 0x0, 0x1c2, 0x21, 0x21, 0x1, 0x1},  // loadInterfaceArt+0x56b6
    {ArtCut, GSO(g_sprites59b778, 0x0), 0x0, 0x0, 0x79, 0x2c, 0x1, 0x1},  // loadInterfaceArt+0x56cf
};
const ArtOp kIa39[] = {
    {ArtCut, GSO(g_sprites59b778, 0x23c), 0x1c2, 0xdc, 0x7, 0x7, 0x1, 0x1},  // loadInterfaceArt+0x571f
    {ArtCut, GSO(g_sprites59b778, 0x2c0), 0x1c2, 0xe6, 0x7, 0x7, 0x1, 0x1},  // loadInterfaceArt+0x5740
    {ArtCut, GSO(g_sprites59b778, 0x1e4), 0x0, 0x96, 0x24, 0xd, 0x1, 0x1},  // loadInterfaceArt+0x575d
    {ArtCut, GSO(g_sprites59b778, 0x210), 0x0, 0xfa, 0x24, 0xd, 0x1, 0x1},  // loadInterfaceArt+0x577a
    {ArtCut, GSO(g_sprites59b778, 0x268), 0x0, 0x12c, 0x2a, 0x2a, 0x1, 0x1},  // loadInterfaceArt+0x5797
    {ArtCut, GSO(g_sprites59b778, 0x294), 0x32, 0x12c, 0x2a, 0x2a, 0x1, 0x1},  // loadInterfaceArt+0x57b5
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3a70), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x57cc
    {ArtCut, GSO(g_sprites58b390, 0xdc), 0xd6, 0x1da, 0x24a, 0x7e, 0x1, 0x0},  // loadInterfaceArt+0x57ef
    {ArtCut, GSO(g_sprites586f50, 0x0), 0x0, 0x0, 0x50, 0x32, 0x1, 0x1},  // loadInterfaceArt+0x5808
    {ArtCut, GSO(g_sprites586f50, 0x2c), 0x0, 0x64, 0x50, 0x32, 0x1, 0x1},  // loadInterfaceArt+0x5822
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3a58), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5839
    {ArtCut, GSO(g_sprites541a18, 0xdc), 0xd6, 0x1da, 0x24a, 0x7e, 0x1, 0x0},  // loadInterfaceArt+0x585c
    {ArtCut, GSO(g_sprites541a18, 0x108), 0x129, 0x12c, 0x1f7, 0x7a, 0x1, 0x1},  // loadInterfaceArt+0x5880
};
const ArtOp kIa40[] = {
    {ArtCut, GSO(g_sprites583a80, 0x630), 0x0, 0x190, 0x21, 0x21, 0x1, 0x1},  // loadInterfaceArt+0x59dc
    {ArtCut, GSO(g_sprites583a80, 0x6e0), 0x0, 0x1c2, 0x21, 0x21, 0x1, 0x1},  // loadInterfaceArt+0x59fa
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3a3c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5a13
    {ArtCut, GSO(g_sprites58b390, 0x134), 0xd7, 0x1da, 0x249, 0x7e, 0x1, 0x0},  // loadInterfaceArt+0x5a37
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3a24), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5a50
    {ArtCut, GSO(g_sprites541a18, 0x134), 0xd7, 0x1da, 0x249, 0x7e, 0x1, 0x0},  // loadInterfaceArt+0x5a74
    {ArtCut, GSO(g_sprites58b1a0, 0x58), 0x0, 0x190, 0x21, 0x21, 0x1, 0x1},  // loadInterfaceArt+0x5a92
    {ArtCut, GSO(g_sprites58b1a0, 0x84), 0x0, 0x1c2, 0x21, 0x21, 0x1, 0x1},  // loadInterfaceArt+0x5ab0
};
const ArtOp kIa41[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3a04), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5bca
};
const ArtOp kIa42[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x39e4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5c37
};
const ArtOp kIa43[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x39c4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5ca4
};
const ArtOp kIa44[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x39a4), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5d11
};
const ArtOp kIa45[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3984), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5d7e
};
const ArtOp kIa46[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x396c), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5deb
};
const ArtOp kIa47[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3950), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5e3c
};
const ArtOp kIa48[] = {
    {ArtLoad, GSO(f_strpool_4c95f4, 0x3934), 0, 0x0, 0x100, 0x2, 0, 0},  // loadInterfaceArt+0x5e8d
};
void iart_images(Art &A) {
    A.reset_image();
    A.run(kIa1, sizeof(kIa1) / sizeof(kIa1[0]));
    A.erase(GSO(g_image5aa778, 0x0));
    A.run(kIa2, sizeof(kIa2) / sizeof(kIa2[0]));
    for (uint32_t b = GSO(g_sprites5628e8, 0x254c), i = 0; b < GSO(g_sprites5628e8, 0x2f18); b += 0x84, i++) {
        const int x = static_cast<int>(i) / 2 * 0x8C, y = (i & 1) * 0x1A4;
        A.cut(b - 0x2C, x, y, 0x8C, 0x8C, 1, 1);
        A.cut(b, x, y + 0x8C, 0x8C, 0x8C, 1, 1);
        A.cut(b + 0x2C, x, y + 0x118, 0x8C, 0x8C, 1, 1);
    }
    A.run(kIa3, sizeof(kIa3) / sizeof(kIa3[0]));
    for (uint32_t b = GSO(g_sprites5628e8, 0x2c), i = 0; b < GSO(g_sprites5628e8, 0x9f8); b += 0x84, i++) {
        const int x = static_cast<int>(i) / 2 * 0x8C, y = (i & 1) * 0x1A4;
        A.cut(b - 0x2C, x, y, 0x8C, 0x8C, 1, 1);
        A.cut(b, x, y + 0x8C, 0x8C, 0x8C, 1, 1);
        A.cut(b + 0x2C, x, y + 0x118, 0x8C, 0x8C, 1, 1);
    }
    A.run(kIa4, sizeof(kIa4) / sizeof(kIa4[0]));
    for (uint32_t b = GSO(g_sprites59e7c0, 0x2c), i = 0; b < GSO(g_sprites59e7c0, 0x9f8); b += 0x84, i++) {
        const int x = static_cast<int>(i) / 2 * 100, y = (i & 1) * 0x174;
        A.cut(b - 0x2C, x, y + 4, 0x5A, 0x78, 1, 1);
        A.cut(b, x, y + 0x80, 0x5A, 0x78, 1, 1);
        A.cut(b + 0x2C, x, y + 0xFC, 0x5A, 0x78, 1, 1);
    }
    A.run(kIa5, sizeof(kIa5) / sizeof(kIa5[0]));
    for (uint32_t b = GSO(g_sprites59e7c0, 0xa7c), i = 0; b < GSO(g_sprites59e7c0, 0x1448); b += 0x84, i++) {
        const int x = static_cast<int>(i) / 2 * 100, y = (i & 1) * 0x174;
        A.cut(b - 0x2C, x, y + 4, 0x5A, 0x78, 1, 1);
        A.cut(b, x, y + 0x80, 0x5A, 0x78, 1, 1);
        A.cut(b + 0x2C, x, y + 0xFC, 0x5A, 0x78, 1, 1);
    }
    A.heads(GSO(g_loopCounter59b76c, 0x0), 'F', 'f', GSO(g_sprites5628e8, 0x0));
    A.heads(GSO(g_loopCounter59b76c, 0x4), 'M', 'm', GSO(g_sprites5628e8, 0x2520));
    A.run(kIa6, sizeof(kIa6) / sizeof(kIa6[0]));
    A.row(GSO(g_surfaces59b050, 0x0), 40, 0, 0x10, 0x144, 0, 0x10, 0x10, 1, 1);
    A.run(kIa7, sizeof(kIa7) / sizeof(kIa7[0]));
    A.row(GSO(g_sprites567a20, 0x0), 5, 0, 0x5A, 0, 0, 0x5A, 0x50, 1, 0);
    A.run(kIa8, sizeof(kIa8) / sizeof(kIa8[0]));
    A.row(GSO(g_sprites53de78, 0x0), 5, 0, 0x5A, 0, 0, 0x5A, 0x50, 1, 0);
    {
        static const uint32_t kFemale[8] = {GSO(f_strpool_4c95f4, 0x3fac), GSO(f_strpool_4c95f4, 0x3f94), GSO(f_strpool_4c95f4, 0x3f7c), GSO(f_strpool_4c95f4, 0x3f64),
                                            GSO(f_strpool_4c95f4, 0x3f4c), GSO(f_strpool_4c95f4, 0x3f34), GSO(f_strpool_4c95f4, 0x3f1c), GSO(f_strpool_4c95f4, 0x3f00)};
        static const uint32_t kMale[8] = {GSO(f_strpool_4c95f4, 0x3eec), GSO(f_strpool_4c95f4, 0x3ed8), GSO(f_strpool_4c95f4, 0x3ec4), GSO(f_strpool_4c95f4, 0x3eb0),
                                          GSO(f_strpool_4c95f4, 0x3e98), GSO(f_strpool_4c95f4, 0x3e80), GSO(f_strpool_4c95f4, 0x3e68), GSO(f_strpool_4c95f4, 0x3e50)};
        for (int i = 0; i < 8; i++) {
            A.load_pcx(kFemale[i]);
            A.cut(GSO(g_surfaces5439e0, 0x0) + 0x2C * i, 0, 0, 0x3C, 0x78, 1, 1);
        }
        for (int i = 0; i < 8; i++) {
            A.load_pcx(kMale[i]);
            A.cut(GSO(g_surfaces5439e0, 0x160) + 0x2C * i, 0, 0, 0x3C, 0x78, 1, 1);
        }
    }
    A.run(kIa9, sizeof(kIa9) / sizeof(kIa9[0]));
    {
        static const short kEdge[8] = {1, 0x13, 0x2C, 0x52, 0x71, 0x8E, 0xA1, 0xAE};
        for (uint32_t row = GSO(g_surfaces587ea8, 0x134), k = 0; row < GSO(g_sprites58a538, 0x124); row += 0x9A0, k++) {
            uint32_t d = row;
            for (int j = 1; j < 8; j++) {
                const int x0 = j > 1 ? 0x21A - (j / 2) * 0xB3 : 0x21A;
                const int y = ((j & 1) ? 0x0C : 0x105) + 0x3C * static_cast<int>(k) + 1;
                for (int s = 0; s < 7; s++, d += 0x2C)
                    A.cut(d, kEdge[s] + x0, y, kEdge[s + 1] - kEdge[s] - 1, 0x2E, 1, 1);
            }
        }
    }
    {
        static const uint32_t kData[4] = {GSO(f_strpool_4c95f4, 0x3e18), GSO(f_strpool_4c95f4, 0x3e08), GSO(f_strpool_4c95f4, 0x3de4), GSO(f_strpool_4c95f4, 0x3df8)};
        for (int c = 0; c < 4; c++) {
            A.load_pcx(kData[c]);
            for (int s = 0; s < 0x14; s++)
                A.cut(GSO(g_surfaces59ca58, 0x0) + 0x370 * c + 0x2C * s, (s & 7) * 0x39, (s / 8) * 0x28, 0x39, 0x28, 1, 1);
        }
    }
    A.run(kIa10, sizeof(kIa10) / sizeof(kIa10[0]));
    A.row(GSO(g_sprites56a550, 0x0), 12, 0, 0x40, 0x140, 0, 0x40, 0x50, 1, 1);
    A.row(GSO(g_sprites58a538, 0x948), 18, 0, 0x10, 0x1BB, 0, 0x10, 0x1E, 1, 1);
    A.run(kIa11, sizeof(kIa11) / sizeof(kIa11[0]));
    A.row(GSO(g_sprites53fd48, 0x0), 11, 9, 0, 0xDD, 0x23, 0xF8, 0x1D, 1, 0);
    A.row(GSO(g_sprites53fd48, 0x1e4), 11, 0x223, 0, 0xDD, 0x23, 0xF8, 0x1D, 1, 0);
    A.run(kIa12, sizeof(kIa12) / sizeof(kIa12[0]));
    A.row(GSO(g_sprites53fd48, 0x4fc), 3, 0x119, 0, 0xBA, 0x23, 0xF8, 0x1E, 1, 0);
    A.run(kIa13, sizeof(kIa13) / sizeof(kIa13[0]));
    A.table_row(GSO(g_sprites53fd48, 0x688), GSO(g_sprites53fd48, 0xa50), GSO(f_tab_point16_4c2d38, 0x0), GSO(f_tab_point16_4c2d38, 0x64));
    A.table_row(GSO(g_sprites53fd48, 0xb84), GSO(g_surfaces540950, 0x0), GSO(f_tab_point16_4c2d38, 0x58), GSO(f_tab_point16_4c2d38, 0xbc));
    A.run(kIa14, sizeof(kIa14) / sizeof(kIa14[0]));
    A.row(GSO(g_sprites5a34f8, 0x0), 11, 9, 0, 0xDD, 0x23, 0xF8, 0x1D, 1, 0);
    A.row(GSO(g_sprites5a34f8, 0x1e4), 11, 0x223, 0, 0xDD, 0x23, 0xF8, 0x1D, 1, 0);
    A.run(kIa15, sizeof(kIa15) / sizeof(kIa15[0]));
    A.row(GSO(g_sprites5a34f8, 0x4fc), 3, 0x11A, 0, 0xBA, 0x23, 0xF8, 0x1E, 1, 0);
    A.run(kIa16, sizeof(kIa16) / sizeof(kIa16[0]));
    A.table_row(GSO(g_sprites5a34f8, 0x688), GSO(g_sprites5a34f8, 0xa50), GSO(f_tab_point16_4c2d38, 0x0), GSO(f_tab_point16_4c2d38, 0x64));
    A.table_row(GSO(g_sprites5a34f8, 0xb84), GSO(g_sprite5a4100, 0x0), GSO(f_tab_point16_4c2d38, 0x58), GSO(f_tab_point16_4c2d38, 0xbc));
    A.run(kIa17, sizeof(kIa17) / sizeof(kIa17[0]));
    A.grid(GSO(g_sprites5612e8, 0x0), 10, 3, 0, 0x32, 0, 0x32, 0x2E, 0x28, 1, 0);
    A.grid(GSO(g_sprites5612e8, 0x528), 3, 3, 0xC8, 0x11, 0, 0x11, 0x10, 0x10, 1, 0);
    A.row(GSO(g_sprites561260, 0x0), 3, 0x12C, 0x32, 0, 0, 0x1A, 0x1A, 1, 1);
    A.run(kIa18, sizeof(kIa18) / sizeof(kIa18[0]));
    A.grid(GSO(g_sprites58ccb8, 0x0), 10, 3, 0, 0x32, 0, 0x32, 0x2E, 0x28, 1, 0);
    A.grid(GSO(g_sprites58ccb8, 0x528), 3, 3, 0xC8, 0x11, 0, 0x11, 0x10, 0x10, 1, 0);
    A.run(kIa19, sizeof(kIa19) / sizeof(kIa19[0]));
    A.grid(GSO(g_surfaces541580, 0x0), 3, 3, 0, 0x11, 0, 0x11, 0x10, 0x10, 1, 1);
    A.run(kIa20, sizeof(kIa20) / sizeof(kIa20[0]));
    A.grid(GSO(g_sprites5a53c8, 0x0), 3, 3, 0, 0x11, 0, 0x11, 0x10, 0x10, 1, 0);
    for (int i = 0, x = 0x12C; x < 0x1C2; i++, x += 0x32) {
        A.cut(GSO(g_surfaces5678b8, 0x0) + 0xB0 + 0x2C * i, x, 0x8C, 0x32, 0x32, 1, 0);
        A.cut(GSO(g_surfaces5678b8, 0x0) + 0x2C * i, x, 0x12C, 0x1E, 0x1E, 1, 0);
    }
    A.run(kIa21, sizeof(kIa21) / sizeof(kIa21[0]));
    A.row(GSO(g_sprites5a53c8, 0x210), 3, 0x12C, 0x11, 0x22, 0, 0x10, 0x10, 1, 0);
    A.row(GSO(g_sprites5a53c8, 0x294), 3, 0x168, 0x28, 0x22, 0, 0x1E, 0x2A, 1, 0);
    A.row(GSO(g_sprites5a53c8, 0x318), 3, 0, 0x11, 0x3C, 0, 0x10, 0x13, 1, 0);
    A.row(GSO(g_sprites5a53c8, 0x39c), 3, 0x12C, 0x11, 0x22, 0, 0x10, 0x10, 1, 0);
    A.run(kIa22, sizeof(kIa22) / sizeof(kIa22[0]));
    A.pair_rows(GSO(g_sprites5a53c8, 0x420));
    A.run(kIa23, sizeof(kIa23) / sizeof(kIa23[0]));
    A.grid(GSO(g_sprites58b680, 0x0), 3, 3, 0, 0x11, 0, 0x11, 0x10, 0x10, 1, 0);
    for (int i = 0, x = 0x12C; x < 0x1C2; i++, x += 0x32) {
        A.cut(GSO(g_sprites56a7b8, 0x0) + 0xB0 + 0x2C * i, x, 0x8C, 0x32, 0x32, 1, 0);
        A.cut(GSO(g_sprites56a7b8, 0x0) + 0x2C * i, x, 0x12C, 0x1E, 0x1E, 1, 0);
    }
    A.run(kIa24, sizeof(kIa24) / sizeof(kIa24[0]));
    A.row(GSO(g_sprites58b680, 0x210), 3, 0x12C, 0x11, 0x22, 0, 0x10, 0x10, 1, 0);
    A.row(GSO(g_sprites58b680, 0x294), 3, 0x168, 0x28, 0x22, 0, 0x1E, 0x2A, 1, 0);
    A.row(GSO(g_sprites58b680, 0x318), 3, 0, 0x11, 0x3C, 0, 0x10, 0x13, 1, 0);
    A.row(GSO(g_sprites58b680, 0x39c), 3, 0x12C, 0x11, 0x22, 0, 0x10, 0x10, 1, 0);
    A.run(kIa25, sizeof(kIa25) / sizeof(kIa25[0]));
    A.pair_rows(GSO(g_sprites58b680, 0x420));
    A.run(kIa26, sizeof(kIa26) / sizeof(kIa26[0]));
    A.setl_row(GSO(g_sprites541710, 0x0));
    A.run(kIa27, sizeof(kIa27) / sizeof(kIa27[0]));
    A.setl_row(GSO(g_sprites5791f8, 0x0));
    A.run(kIa28, sizeof(kIa28) / sizeof(kIa28[0]));
    A.row(GSO(g_sprites541710, 0x160), 8, 3, 0x58, 0x18B, 0, 0x50, 0x50, 1, 0);
    A.run(kIa29, sizeof(kIa29) / sizeof(kIa29[0]));
    A.row(GSO(g_sprites5791f8, 0x160), 8, 3, 0x58, 0x18B, 0, 0x50, 0x50, 1, 0);
    A.run(kIa30, sizeof(kIa30) / sizeof(kIa30[0]));
    A.grid(GSO(g_sprites59df20, 0x0), 3, 2, 0, 0x96, 0x12C, 0x32, 0x70, 0x23, 1, 1);
    A.grid(GSO(g_sprites59df20, 0x108), 5, 2, 0, 0x96, 0x32, 0x32, 0x70, 0x1C, 1, 1);
    A.row(GSO(g_sprites59df20, 0x2c0), 4, 0, 0x32, 0, 0, 0x23, 0x25, 1, 1);
    A.grid(GSO(g_sprites59df20, 0x370), 8, 2, 0x12C, 0x32, 0, 0x32, 0x32, 0x32, 1, 1);
    A.grid(GSO(g_sprites59df20, 0x630), 2, 2, 0x1F4, 0x64, 0, 0x64, 0x3C, 0x3D, 1, 1);
    A.run(kIa31, sizeof(kIa31) / sizeof(kIa31[0]));
    A.grid(GSO(g_surfaces567390, 0x0), 10, 3, 0x1F4, 0x64, 0, 0x32, 0x39, 0x32, 1, 0);
    A.run(kIa32, sizeof(kIa32) / sizeof(kIa32[0]));
    A.grid(GSO(g_surfaces56a948, 0x0), 10, 3, 0x1F4, 0x64, 0, 0x32, 0x39, 0x32, 1, 0);
    A.run(kIa33, sizeof(kIa33) / sizeof(kIa33[0]));
    for (uint32_t s = GSO(g_sprites568fc8, 0x2c), y = 0; s < GSO(g_sprites568fc8, 0x134); s += 0x58, y += 0x64) {
        A.cut(s - 0x2C, 0, y, 0x4E, 0x51, 1, 0);
        A.cut(s, 0x64, y, 0x4E, 0x51, 1, 0);
    }
    for (uint32_t s = GSO(g_sprites568fc8, 0x160), y = 0x32; s < GSO(g_sprites568fc8, 0x478); s += 0x58, y += 0x32)
        A.cut(s, 0x258, y, 0x24, 0x26, 1, 1);
    A.run(kIa34, sizeof(kIa34) / sizeof(kIa34[0]));
    A.grid(GSO(g_sprites5873a8, 0x0), 3, 3, 0, 0x64, 0, 0x64, 0x4E, 0x51, 1, 0);
    A.row(GSO(g_sprites5873a8, 0x18c), 3, 0x12C, 0, 0, 0x64, 0xD6, 0x5B, 1, 1);
    A.grid(GSO(g_sprites5873a8, 0x210), 9, 3, 0x258, 0x32, 0x32, 0x32, 0x24, 0x26, 1, 1);
    A.run(kIa35, sizeof(kIa35) / sizeof(kIa35[0]));
    A.grid(GSO(g_sprites571d40, 0x0), 3, 3, 0, 0x64, 0, 0x64, 0x58, 0x63, 1, 1);
    A.row(GSO(g_sprites571d40, 0x18c), 3, 0x190, 0x64, 0, 0, 0x48, 0x63, 1, 1);
    A.row(GSO(g_sprites571d40, 0x210), 3, 0, 0x32, 0x1C2, 0, 0x22, 0x28, 1, 1);
    A.run(kIa36, sizeof(kIa36) / sizeof(kIa36[0]));
    for (uint32_t s = GSO(g_sprites59ba68, 0x84), x = 0x12C, xb = 0; x < 0x258; s += 0x2C, x += 0x64, xb += 0x32) {
        const int xa = static_cast<int>(x) - 0x12C;
        A.cut(s - 0x84, xa, 0, 0x3C, 0x38, 1, 1);
        A.cut(s, xa, 0x64, 0x40, 0x32, 1, 1);
        A.cut(s + 0x84, xa, 0xC8, 0x3E, 0x32, 1, 1);
        A.cut(s + 0x108, x, 0, 0x33, 0x2F, 1, 1);
        A.cut(s + 0x420, x, 0x32, 0x33, 0x2D, 1, 1);
        A.cut(s + 0x18C, x, 0x64, 0x33, 0x2D, 1, 1);
        A.cut(s + 0x210, x, 0x96, 0x33, 0x2F, 1, 1);
        A.cut(s + 0x294, x, 0xC8, 0x33, 0x2D, 1, 1);
        A.cut(s + 0x318, xb, 0x1C2, 0x24, 0x24, 1, 1);
        A.cut(s + 0x39C, xb, 0x1F4, 0x26, 0x24, 1, 1);
    }
    A.run(kIa37, sizeof(kIa37) / sizeof(kIa37[0]));
    for (uint32_t d = GSO(g_sprites55e930, 0x160), x = 0x12C; x < 0x2BC; d += 0x2C, x += 0x64) {
        A.cut(d - 0xB0, x, 0x15E, 0x2F, 0x28, 1, 1);
        A.cut(d, x, 0x190, 0x2F, 0x29, 1, 1);
    }
    A.run(kIa38, sizeof(kIa38) / sizeof(kIa38[0]));
    {
        uint32_t d = GSO(g_sprites59b778, 0x2c);
        for (int x = 0x252; x > 0x1B2; x -= 0x10, d += 0x2C)
            A.cut(d, x, 0x64, 0x10, 0x10, 1, 1);
    }
    A.run(kIa39, sizeof(kIa39) / sizeof(kIa39[0]));
    {
        int k = 0;
        for (uint32_t s = GSO(g_sprites583a80, 0xb0); s < GSO(g_sprites583a80, 0x160); s += 0x2C, k++) {
            const int xa = 0x32 * k, xb = 0x64 * k, o = (k == 0) ? 1 : 0;
            A.cut(s - 0xB0, xa, 0x12C, 0x2A, 0x2C, 1, 1);
            A.cut(s, xa, 0x1F4, 0x26, 0x20, 1, 1);
            A.cut(s + 0xB0, xa, 0x217, 0x26, 0x1E, 1, 1);
            A.cut(s + 0x160, xa, 0x235, 0x26, 0x20, 1, 1);
            A.cut(s + 0x210, xb, o, 0x50, 0x32, 1, 1);
            A.cut(s + 0x2C0, xb, o + 0x32, 0x50, 0x32, 1, 1);
            A.cut(s + 0x370, xb, o + 0x64, 0x50, 0x32, 1, 1);
            A.cut(s + 0x420, xb, o + 0x96, 0x50, 0x32, 1, 1);
            A.cut(s + 0x4D0, xb, o + 0xC8, 0x50, 0x32, 1, 1);
        }
    }
    A.run(kIa40, sizeof(kIa40) / sizeof(kIa40[0]));
    for (int r = 0, y = 0x1F4; y < 0x258; r++, y += 0x32)
        A.row(GSO(g_sprites5a8c90, 0x0) + 0xB0 * r, 3, 0, 0x32, y, 0, 0x2A, 0x2A, 1, 1);
    for (int r = 0, y = 0xC8; y < 0x190; r++, y += 0x64)
        A.row(GSO(g_sprites5a8c90, 0x160) + 0xB0 * r, 3, 0, 0x32, y, 0, 0x10, 0x3E, 1, 1);
    A.grid(GSO(g_sprites5a8c90, 0x2c0), 4, 4, 0x190, 0x64, 0, 0x32, 0x34, 0x30, 1, 1);
    for (int r = 0; GSO(g_sprites5a8c90, 0x580) + 0xB0 * r < GSO(g_sheetFirstCell, 0x0); r++)
        A.row(GSO(g_sprites5a8c90, 0x580) + 0xB0 * r, 3, 0, 0x64, 0x32 * r, 0, 0x3F, 0x2B, 1, 1);
    A.run(kIa41, sizeof(kIa41) / sizeof(kIa41[0]));
    A.wide_row(GSO(g_sprites5a4138, 0x0), 0x3C, 0x32, 0x4A);
    A.run(kIa42, sizeof(kIa42) / sizeof(kIa42[0]));
    A.wide_row(GSO(g_sprites53c570, 0x0), 0x3B, 0x31, 0x49);
    A.run(kIa43, sizeof(kIa43) / sizeof(kIa43[0]));
    A.wide_row(GSO(g_sprites53c570, 0xb0), 0x3B, 0x31, 0x49);
    A.run(kIa44, sizeof(kIa44) / sizeof(kIa44[0]));
    A.wide_row(GSO(g_sprites53c570, 0x160), 0x3B, 0x31, 0x49);
    A.run(kIa45, sizeof(kIa45) / sizeof(kIa45[0]));
    A.wide_row(GSO(g_sprites53c570, 0x210), 0x3B, 0x31, 0x49);
    A.run(kIa46, sizeof(kIa46) / sizeof(kIa46[0]));
    for (int c = 0; GSO(g_sprites53c570, 0x2c0) + 0xB0 * c < GSO(g_tileFlags, 0x0); c++)
        A.row(GSO(g_sprites53c570, 0x2c0) + 0xB0 * c, 4, 0x32 * c, 0, 0, 0x32, 0x14, 0x18, 1, 1);
    A.run(kIa47, sizeof(kIa47) / sizeof(kIa47[0]));
    for (int c = 0; GSO(g_sprites5a4138, 0x2c0) + 0xB0 * c < GSO(g_namePoolLength, 0x0); c++)
        A.row(GSO(g_sprites5a4138, 0x2c0) + 0xB0 * c, 4, 0x32 * c, 0, 0, 0x32, 0x14, 0x18, 1, 1);
    A.run(kIa48, sizeof(kIa48) / sizeof(kIa48[0]));
    {
        uint32_t d = GSO(g_sprites58a538, 0x318);
        for (int i = 0; d < GSO(g_sprites58a538, 0x630); i++, d += 0x2C) {
            const int q = i / 9, r = i % 9;
            A.cut(d - 0x318, 17 * r + 3, 14 * q + 4, 0x11, 0x0E, 1, 1);
            A.cut(d, 18 * r + 4, 17 * q + 0x24, 0x12, 0x11, 1, 1);
            A.cut(d + 0x318, 19 * r + 4, 21 * q + 0x49, 0x13, 0x15, 1, 1);
        }
    }
}
// END GENERATED loadInterfaceArt images
#undef GSO

// loadInterfaceArt.images (menu.cpp hands it to the backend after the table of the full arm): the whole image half
void b_iart_images(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  Art A{g, ctx, state_va(ctx, &GameState::f_strpool_4c95f4) - static_cast<uint32_t>(offsetof(GameState, f_strpool_4c95f4)), {}};
  if (mode() == 3) return;
  iart_images(A);
}

// ------------------------------------------------------------------------------------------------ sprite draws
// The Surface wrappers the HUD draws through (c3da.cpp, c3df.cpp, c3dq.cpp): each reaches one jgld Sprite slot of the
// surface's handle with the display scale set. Modelled at the scale (1, 1, 1) the HUD passes; any other scale is reported
// (the blitters then scale the sprite, which the backend does not model).
//   Surface_blit2(this, dst, x, y, mode, mask)       c3da.cpp: this +8 != 0 draws nothing (0x18), a NULL dst nothing (0x10);
//                                                     handle slot +0x6c = Sprite::drawG at (x + left trim, y + top trim):
//                                                     0xff transparent, 0xf8 shades the destination through the table `mode`
//                                                     points at, every other byte the palette word (gfxb::draw_g16_lut)
//   Surface_drawScaled2(this, dst, x, y, mode, sx, sy, den, mask)   c3da.cpp: the scale set, the trims scaled, Surface_blit2
//   drawScaledSprite421b90(this, dst, x, y, mode, s, den, mask)     c3da.cpp: Surface_drawScaled2 with sx = sy = s
//   Surface_drawScaled(this, dst, x, y, nx, ny, den, mask)          c3df.cpp: this +8 != 0 is the load arm (reported); a NULL
//                                                     dst nothing; the scale set and handle slot +0x44 = Sprite::drawB at the
//                                                     scaled trims + (x, y): bytes >= 0xfe transparent (gfxb::draw16)
//   drawScaledSprite421b60(this, dst, x, y, n, den, mask)           c3dq.cpp: Surface_drawScaled with nx = ny = n
// The table `mode` names: the value of g_halfBrightLut (gameMain's 0x10000-word table, word i = ((((i >> 10) & 0x1e) << 5 |
// (i >> 5) & 0x1e) << 4) | (i >> 1) & 0xf, c3fv.cpp GameMain_re) or of g_color16Lut (build16BitColorTable's block 1,
// gfxb::shade_lut). Both are heap pointers the port's GameState holds as 0 (pointer class), so a value equal to both is
// taken as the half-bright table: every port call site of these drops passes the g_halfBrightLut word (U-1351).
namespace {

const std::array<uint16_t, 0x10000> &half_bright_lut() {
  static const std::array<uint16_t, 0x10000> t = [] {
    std::array<uint16_t, 0x10000> a{};
    for (uint32_t i = 0; i < 0x10000; ++i)
      a[i] = static_cast<uint16_t>(((((i >> 10) & 0x1e) << 5 | ((i >> 5) & 0x1e)) << 4) | ((i >> 1) & 0xf));
    return a;
  }();
  return t;
}

// the sprite behind a SpriteRef key, after the wrappers' own guards (true: draw it)
const Sprite8 *draw_source(Gfx &g, Ctx &ctx, const uint32_t *a, const char *who, bool load_arm_reports) {
  if (!gfx_is_local_key(a[0]) && ctx.mem.load<uint32_t>(a[0] + 8) != 0) {
    if (load_arm_reports) g.unsup(std::string(who) + ": the load arm (a file name at +8)");
    return nullptr;
  }
  if (a[1] == 0) return nullptr;
  if (a[1] != g.root_va(ctx)) {
    g.unsup(std::string(who) + ": a destination other than the root surface");
    return nullptr;
  }
  auto it = g.sprites.find(a[0]);
  if (it == g.sprites.end()) {
    // a SpriteRef no modelled cut reached: in the original its handle (+4) is NULL and the wrapper returns 7 with nothing
    // drawn (c3df.cpp / c3da.cpp: `this->m_4 == NULL -> 7`). The keys are kept (gfx_hud.dumpUncut) and
    // port/test/test_gfx_hud.py checks each against the original's record in a segment start image (U-1352).
    if (mode() == 7) g.unsup(std::string(who) + ": a SpriteRef the backend has no sprite for");
    ++st(g).uncut[a[0]];
    dlog(who, a, 2);
    return nullptr;
  }
  return &it->second;
}

void blit2(Gfx &g, Ctx &ctx, uint32_t self, uint32_t dst, int32_t x, int32_t y, uint32_t lut, uint32_t mask, const char *who) {
  g.ensure_root(ctx);
  const uint32_t a[2] = {self, dst};
  const Sprite8 *s = draw_source(g, ctx, a, who, false);
  if (!s) return;
  if (mask != 0) {
    g.unsup(std::string(who) + ": a mask");
    return;
  }
  const uint16_t *t;
  size_t n;
  if (lut == ctx.gs().g_halfBrightLut && mode() != 5) {
    t = half_bright_lut().data();
    n = half_bright_lut().size();
  } else if (lut == ctx.gs().g_color16Lut) {
    t = gfxb::shade_lut().data();
    n = gfxb::shade_lut().size();
  } else {
    g.unsup(std::string(who) + ": a shade table other than g_halfBrightLut / g_color16Lut");
    return;
  }
  gfxb::draw_g16_lut(g, *s, g.root, add32(x, s->left), add32(y, s->top), t, n);
  if (g.hud.bubble_depth > 0) gfxs::bubble_shade(g, ctx, *s, add32(x, s->left), add32(y, s->top), t, n);   // P23D fix 3
}

void b_blit2(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  blit2(g, ctx, a[0], a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), a[4], a[5], "Surface_blit2");
}
bool unit_scale(Gfx &g, uint32_t sx, uint32_t sy, uint32_t den, const char *who) {
  if (sx == 1 && sy == 1 && den == 1) return true;
  g.unsup(std::string(who) + ": a display scale other than (1, 1, 1)");
  return false;
}
void b_draw_scaled2(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (!unit_scale(g, a[5], a[6], a[7], "Surface_drawScaled2")) return;
  blit2(g, ctx, a[0], a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), a[4], a[8], "Surface_drawScaled2");
}
void b_draw_scaled_421b90(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (!unit_scale(g, a[5], a[5], a[6], "drawScaledSprite421b90")) return;
  blit2(g, ctx, a[0], a[1], static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3]), a[4], a[7], "drawScaledSprite421b90");
}
// Sprite::drawB of an 8-bit sprite onto the 16-bit root under the display scale (nx, ny, den) (jgld draw16_1001bc40,
// re/match/jgld_blit16.cpp): |nx| == |den| and |ny| == |den| is the unscaled copy at (x, y); otherwise fx = (float)|nx| / den,
// fy likewise (single precision), the destination rectangle (x + (int)fx, y + (int)fy, (int)(w * fx), (int)(h * fy)) is
// intersected with the surface, and with the 16.16 steps sx = (den << 16) / |nx|, sy likewise the destination pixel
// (j, i) of the clipped rectangle reads source column (sx * (offX + j)) >> 16 and row (sy * (offY + i)) >> 16 (offX / offY =
// the clip's cut into the rectangle: the inner loop adds the step's fraction with carry into the source pointer, the row step
// picks the dword before or at jgld's step pointer by the fraction's carry, and those two dwords are (sy >> 16) rows and
// one row more, both less the row's advance); bytes >= 0xfe are transparent. A negative nx with a scaled rectangle is the
// x-mirrored body (the same rectangle and rows; the source column is w - 1 - (sx * (offX + j)) >> 16: the pointer starts at
// m_width - 1 less the first step and the carry chain subtracts); the other mirrored bodies (ny < 0, or nx < 0 unscaled) are
// reported, and so is a source pixel outside the sprite (none of the HUD's calls reaches one).
void draw_b16(Gfx &g, const Sprite8 &s, int32_t x, int32_t y, int32_t nx, int32_t ny, int32_t den, const char *who) {
  const bool mirror = nx < 0 && nx != INT32_MIN && -nx != den;
  if (mirror) nx = -nx;
  if (nx < 0 || ny < 0 || den <= 0) {
    const uint32_t lg[7] = {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(nx), static_cast<uint32_t>(ny),
                            static_cast<uint32_t>(den), static_cast<uint32_t>(s.w), static_cast<uint32_t>(s.h)};
    dlog("drawB mirrored", lg, 7);
    g.unsup(std::string(who) + ": a mirrored or non-positive display scale");
    return;
  }
  const int32_t dx = mode() == 6 ? 1 : 0;
  if (nx == den && ny == den) {
    gfxb::draw16(s, g.root, add32(x, dx), y);
    return;
  }
  const float fx = static_cast<float>(nx) / static_cast<float>(den), fy = static_cast<float>(ny) / static_cast<float>(den);
  const int32_t L = add32(add32(x, static_cast<int32_t>(fx)), dx), T = add32(y, static_cast<int32_t>(fy));
  const int32_t W = static_cast<int32_t>(static_cast<double>(s.w) * static_cast<double>(fx));
  const int32_t H = static_cast<int32_t>(static_cast<double>(s.h) * static_cast<double>(fy));
  const int32_t cl = std::max(L, 0), ct = std::max(T, 0), cr = std::min(add32(L, W), g.root.w), cb = std::min(add32(T, H), g.root.h);
  if (cl >= cr || ct >= cb) return;
  const uint64_t stx = static_cast<uint64_t>(static_cast<uint32_t>(den << 16) / static_cast<uint32_t>(nx));
  const uint64_t sty = static_cast<uint64_t>(static_cast<uint32_t>(den << 16) / static_cast<uint32_t>(ny));
  for (int32_t yy = ct; yy < cb; ++yy) {
    const uint64_t sr = (sty * static_cast<uint64_t>(yy - T)) >> 16;
    for (int32_t xx = cl; xx < cr; ++xx) {
      uint64_t sc = (stx * static_cast<uint64_t>(xx - L)) >> 16;
      if (mode() == 8 && sc + 1u < static_cast<uint64_t>(s.w)) ++sc;
      if (mirror) sc = static_cast<uint64_t>(s.w) - 1u - sc;
      if (sr >= static_cast<uint64_t>(s.h) || sc >= static_cast<uint64_t>(s.w)) {
        g.unsup(std::string(who) + ": a scaled read outside the sprite");
        return;
      }
      const uint8_t b = s.px[static_cast<size_t>(sr) * static_cast<size_t>(s.w) + static_cast<size_t>(sc)];
      if (b < 0xfe) {
        uint16_t &o = g.root.px[static_cast<size_t>(yy) * static_cast<size_t>(g.root.w) + static_cast<size_t>(xx)];
        o = s.pal[b];
        hud_mark(&o);  // P10C fix 2
      }
    }
  }
}

// Surface_drawScaled (c3df.cpp SurfaceDrawScaled_re): the trims scaled with a 32-bit product and a truncating division
// (left * nx / den + x, top * ny / den + y), then drawB under the scale (draw_b16)
void draw_scaled(Gfx &g, Ctx &ctx, const uint32_t *a, uint32_t nx, uint32_t ny, uint32_t den, uint32_t mask, const char *who) {
  g.ensure_root(ctx);
  const Sprite8 *s = draw_source(g, ctx, a, who, true);
  if (!s) return;
  if (mask != 0) {
    g.unsup(std::string(who) + ": a mask");
    return;
  }
  const int32_t n = static_cast<int32_t>(nx), m = static_cast<int32_t>(ny), d = static_cast<int32_t>(den);
  if (d <= 0) {
    g.unsup(std::string(who) + ": a non-positive scale denominator (0: the original's idiv faults)");
    return;
  }
  const int32_t px = add32(static_cast<int32_t>(a[2]), mul32(s->left, n) / d);
  const int32_t py = add32(static_cast<int32_t>(a[3]), mul32(s->top, m) / d);
  draw_b16(g, *s, px, py, n, m, d, who);
  if (g.hud.bubble_depth > 0) gfxs::bubble_scaled(g, ctx, *s, px, py, n, m, d);   // P23D fix 5
}
void b_draw_scaled(Gfx &g, Ctx &ctx, const uint32_t *a, int) { draw_scaled(g, ctx, a, a[4], a[5], a[6], a[7], "Surface_drawScaled"); }
void b_draw_scaled_421b60(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  draw_scaled(g, ctx, a, a[4], a[4], a[5], a[6], "drawScaledSprite421b60");
}

// ------------------------------------------------------------------------------------------------ frames and panels
// a core or family body by its drop name (the original reaches it through a call)
void call(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  uint32_t a[12] = {};
  size_t i = 0;
  for (uint32_t v : args) a[i++] = v;
  if (GfxBody b = gfxb::find_body(name)) {
    b(g, ctx, a, static_cast<int>(args.size()));
    return;
  }
  g.unsup(std::string("gfx_hud: no body ") + name);
}
uint32_t va(Ctx &ctx, uint32_t off) {
  return state_va(ctx, &GameState::f_strpool_4c95f4) - static_cast<uint32_t>(offsetof(GameState, f_strpool_4c95f4)) + off;
}
#define GSO(sym, off) (static_cast<uint32_t>(offsetof(GameState, sym)) + static_cast<uint32_t>(off))

// the 16-pixel rounding of the frame drawers (c3da.cpp round16): a size not a multiple of 16 grows by 15 - (size & 15) and
// the position moves back by half of that
void round16(int32_t &pos, int32_t &size) {
  const int32_t low = size & 0xf;
  if (low != 0) {
    const int32_t r = 0xf - low;
    size += r;
    pos -= r / 2;
  }
}
// c3da.cpp frameBorder: the eight pieces s[k] over masks m[k] (0 top-left, 1 top edge, 2 bottom edge, 3 left edge, 4 right
// edge, 5 top-right, 6 bottom-right, 7 bottom-left) through Surface_stretchBlit; the edges repeat every 16 from pos + 16,
// (len - 16 - 1) >> 4 times, when len - 16 > 16
void frame_border(Gfx &g, Ctx &ctx, int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t *s, const uint32_t *m) {
  const uint32_t root = g.root_va(ctx);
  auto piece = [&](int k, int32_t px, int32_t py) {
    call(g, ctx, "Surface_stretchBlit", {s[k], m[k], root, static_cast<uint32_t>(px), static_cast<uint32_t>(py), 0});
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
      piece(4, x + w - 0x10, yi);
      yi += 0x10;
    } while (--n != 0);
  }
  piece(5, x + w - 0x10, y);
  piece(6, x + w - 0x10, y + h - 0x10);
  piece(7, x, y + h - 0x10);
}
// drawFrameR3(x, y, w, h, fill) (c3da.cpp DrawFrameR3_re): the scale (1, 1, 1), 16-rounding, blitPanel(x, y, w, h, 1) when
// fill != 0, then the border from g_sprites5a53c8 cells 0, 1, 7, 3, 5, 2, 8, 6 over the same cells of g_sprites58b680
void b_draw_frame_r3(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  call(g, ctx, "Display::setScale", {1, 1, 1});
  int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), w = static_cast<int32_t>(a[2]), h = static_cast<int32_t>(a[3]);
  round16(x, w);
  round16(y, h);
  if (a[4] != 0)
    call(g, ctx, "blitPanel", {static_cast<uint32_t>(x), static_cast<uint32_t>(y), static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1});
  static const uint32_t kCell[8] = {0, 1, 7, 3, 5, 2, 8, 6};
  uint32_t s[8], m[8];
  for (int k = 0; k < 8; ++k) {
    s[k] = va(ctx, GSO(g_sprites5a53c8, 0x2c * kCell[k]));
    m[k] = va(ctx, GSO(g_sprites58b680, 0x2c * kCell[k]));
  }
  frame_border(g, ctx, x, y, w, h, s, m);
}
// drawFrameS2(x, y, w, h, fill) (c3da.cpp DrawFrameS2_re): the scale (1, 1, 1), 16-rounding, when fill != 0
// fillWindowRect(g_rootWindow, x + 2, y + 2, w - 4, h - 4, 0x80007fdc), then the border in the style s = the signed byte
// g_courseType: Surfaces g_spriteRef_562368 + 0x160 s at the offsets 0x2c, 0xb0, 0, 0x58, 0x108, 0xdc, 0x134, 0x84 (pieces 0..7),
// masks g_spriteRef_59d920 + 0x160 s at the same offsets
void b_draw_frame_s2(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  call(g, ctx, "Display::setScale", {1, 1, 1});
  int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]), w = static_cast<int32_t>(a[2]), h = static_cast<int32_t>(a[3]);
  round16(x, w);
  round16(y, h);
  if (a[4] != 0)
    call(g, ctx, "fillWindowRect", {state_va(ctx, &GameState::g_rootWindow), static_cast<uint32_t>(x + 2), static_cast<uint32_t>(y + 2),
                                    static_cast<uint32_t>(w - 4), static_cast<uint32_t>(h - 4), 0x80007fdcu});
  static const uint32_t kOff[8] = {0x2c, 0xb0, 0, 0x58, 0x108, 0xdc, 0x134, 0x84};
  const uint32_t style = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_courseType)))) * 0x160);
  uint32_t s[8], m[8];
  for (int k = 0; k < 8; ++k) {
    s[k] = va(ctx, GSO(g_spriteRef_562368, 0)) + style + kOff[k];
    m[k] = va(ctx, GSO(g_spriteRef_59d920, 0)) + style + kOff[k];
  }
  frame_border(g, ctx, x, y, w, h, s, m);
}

// drawTerrainToolbar(a, b) (c3ei.cpp DrawTerrainToolbar_re), the drawing: sel = hitBuildTool433c60(a, b) (a pure hit test,
// called through the registry); Surface_stretchBlit of g_sprites541a18 cell 8 over g_sprites58b390 cell 8 at (0xd7, 0x1e2);
// setDrawTarget(g_fontRef_519fd8); sel == -2: Image_draw g_sprites58b1a0 + 0x108 at (0xfe, 0x1e3); bit 27 of g_flags:
// Image_draw g_sprites571d40 + 0x23c at (0xfd, 0x222); sel == -3: g_sprites571d40 + 0x210 at (0xfd, 0x222), else sel >= 0:
// g_sprites571d40 + 0x84 sel at the words of f_tab_point16_4c7a88 + 4 sel; always g_sprites571d40 + 0x2c + 0x84 c at the words
// of entry c = g_ui_542f20. The hover tip after ten frames of dwell (drawPanelBackdrop of a label the original copies into
// g_textBuffer) is not drawn: the port's drop keeps neither the dwell counter nor the text (reported, U-1353).
void toolbar_dwell(Gfx &g, Ctx &ctx, int32_t sel, int32_t a, int32_t b);   // roadmap S3.7 (after InfoPanelState)
void b_terrain_toolbar(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const uint32_t root = g.root_va(ctx);
  const int32_t sel = static_cast<int32_t>(cm::call_peer(ctx, "hitBuildTool433c60", {a[0], a[1]}).eax);
  call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0x160)), va(ctx, GSO(g_sprites58b390, 0x160)), root, 0xd7, 0x1e2, 0});
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519fd8), 0, 0, 0});
  auto img = [&](uint32_t s, int32_t x, int32_t y) { call(g, ctx, "Image_draw", {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0}); };
  auto pt = [&](int32_t i, int k) { return static_cast<int32_t>(ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7a88, 4 * i + 2 * k)))); };
  if (sel == -2) img(va(ctx, GSO(g_sprites58b1a0, 0x108)), 0xfe, 0x1e3);
  if (ctx.gs().g_flags & (1u << 27)) img(va(ctx, GSO(g_sprites571d40, 0x23c)), 0xfd, 0x222);
  if (sel == -3)
    img(va(ctx, GSO(g_sprites571d40, 0x210)), 0xfd, 0x222);
  else if (sel >= 0)
    img(va(ctx, GSO(g_sprites571d40, 0)) + 0x84u * static_cast<uint32_t>(sel), pt(sel, 0), pt(sel, 1));
  const int32_t cur = ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_ui_542f20));
  img(va(ctx, GSO(g_sprites571d40, 0x2c)) + 0x84u * static_cast<uint32_t>(cur), pt(cur, 0), pt(cur, 1));
  toolbar_dwell(g, ctx, sel, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
}

// showMainRadialMenu.draw(x, y, tip) (c3fi.cpp ShowMainRadialMenu_re, the drawing; tick.cpp's showMainRadialMenu fallback
// hands it over after its own state steps, so the blink counter g_panelState+0xc is the incremented one and tip != 0 when
// the fallback copied the hovered spot's label into g_textBuffer): Surface_stretchBlit of g_sprites541a18 cell 0 over
// g_sprites58b390 at (0, 0x1b3); s = nearestMenuSpot(x, y); s != -1 and s != g_panelListMode: k = the signed byte
// f_tab_bytes_4c7994[s], Surface_stretchBlit of g_sprites5873a8 + 0x84 k over g_sprites568fc8 + 0x58 k at the words of
// f_tab_point16_4c7930 + 0x30 + 4 k; g_zoomScale == 4: nothing more of the next two unless unk_005a9cbc != 0, then
// Surface_drawScaled g_sprites5873a8 + 0x268 at the words f_tab_point16_4c7970 + 0; g_zoomScale == 1: + 0x2ec at + 4; bit 2
// of g_flags with bit 2 of the blink counter: + 0x4d0 at + 0x14; then the mode m = g_panelListMode (unsigned, <= 4): 0
// Surface_stretchBlit + 0x2c / g_sprites568fc8 + 0x2c at (7, 0x1b4) and Surface_drawScaled + 0x18c at (0x22, 0x1fd); 1 + 0xb0 /
// + 0x84 at (0x54, 0x1cf) and + 0x1b8 at (0x5f, 0x20d); 2..4 + 0x134 / + 0xdc at (0x92, 0x1f8) and + 0x1e4 at (0x8e, 0x22b);
// tip: drawPanelBackdrop at the words of f_tab_point16_4c7930 + 4 s
void b_radial_menu(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  const uint32_t root = g.root_va(ctx);
  auto stretch = [&](uint32_t s, uint32_t m, int32_t x, int32_t y) {
    call(g, ctx, "Surface_stretchBlit", {s, m, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0});
  };
  auto scaled = [&](uint32_t s, int32_t x, int32_t y) {
    const uint32_t b[8] = {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1, 1, 1, 0};
    draw_scaled(g, ctx, b, 1, 1, 1, 0, "showMainRadialMenu");
  };
  auto w16 = [&](uint32_t off) { return static_cast<int32_t>(ctx.mem.load<int16_t>(va(ctx, off))); };
  const uint32_t spr = va(ctx, GSO(g_sprites5873a8, 0)), msk = va(ctx, GSO(g_sprites568fc8, 0));
  call(g, ctx, "Display::setScale", {1, 1, 1});
  stretch(va(ctx, GSO(g_sprites541a18, 0)), va(ctx, GSO(g_sprites58b390, 0)), 0, 0x1b3);
  const int32_t s = static_cast<int32_t>(cm::call_peer(ctx, "nearestMenuSpot", {a[0], a[1]}).eax);
  const int32_t mode_now = ctx.gs().g_panelListMode;
  if (s != -1 && s != mode_now) {
    const int32_t k = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(f_tab_bytes_4c7994, 0)) + static_cast<uint32_t>(s)));
    stretch(spr + 0x84u * static_cast<uint32_t>(k), msk + 0x58u * static_cast<uint32_t>(k), w16(GSO(f_tab_point16_4c7930, 0x30 + 4 * k)),
            w16(GSO(f_tab_point16_4c7930, 0x32 + 4 * k)));
  }
  bool skip = false;
  const int32_t zoom = ctx.gs().g_zoomScale;
  if (zoom == 4) {
    if (ctx.mem.load<int32_t>(state_va(ctx, &GameState::unk_005a9cbc)) == 0)
      skip = true;
    else
      scaled(spr + 0x268, w16(GSO(f_tab_point16_4c7970, 0)), w16(GSO(f_tab_point16_4c7970, 2)));
  }
  if (!skip && zoom == 1) scaled(spr + 0x2ec, w16(GSO(f_tab_point16_4c7970, 4)), w16(GSO(f_tab_point16_4c7970, 6)));
  if ((ctx.gs().g_flags & 4) != 0 && (ctx.mem.load<uint32_t>(va(ctx, GSO(g_panelState, 0xc))) & 4) != 0)
    scaled(spr + 0x4d0, w16(GSO(f_tab_point16_4c7970, 0x14)), w16(GSO(f_tab_point16_4c7970, 0x16)));
  const uint32_t m = static_cast<uint32_t>(mode_now);
  if (m == 0) {
    stretch(spr + 0x2c, msk + 0x2c, 7, 0x1b4);
    scaled(spr + 0x18c, 0x22, 0x1fd);
  } else if (m == 1) {
    stretch(spr + 0xb0, msk + 0x84, 0x54, 0x1cf);
    scaled(spr + 0x1b8, 0x5f, 0x20d);
  } else if (m <= 4) {
    stretch(spr + 0x134, msk + 0xdc, 0x92, 0x1f8);
    scaled(spr + 0x1e4, 0x8e, 0x22b);
  }
  if (a[2] != 0 && s >= 0)
    call(g, ctx, "drawPanelBackdrop", {static_cast<uint32_t>(w16(GSO(f_tab_point16_4c7930, 4 * s))),
                                       static_cast<uint32_t>(w16(GSO(f_tab_point16_4c7930, 4 * s + 2)))});
}
// showInfoCursorPanel.draw(x, y) (the original's showInfoCursorPanel, written from its instructions: re/analysis/ui/
// 00433190_showInfoCursorPanel.md is the C2 transcription; commands_layout.inc keeps its state, and hands the drawing over).
//   g_buildPanelSlot != 0: drawAmenitiesPanel(x, y) instead (its drawing is not modelled: reported).
//   The point table's last three entries (13..15) take the course type's positions: entry 13 + j = the words at
//   f_tab_point16_4c79b4 + 0x74 + 24 t + 8 j and + 4 (t = the signed byte g_courseType; the original stores them into the
//   table, the port's layout does not, so the drawing reads them from their source).
//   sel = hitTestSlot432f90(x, y) (a pure hit test, through the registry). The hover tip (sel the previous frame's and its
//   dwell counter above 10: a drawFrameR3 box with the tool's name and price for sel < 0x14, a text tip for the others) is
//   not drawn (reported, U-1353); the family keeps the original's dwell counter (g_panelState+4 / g_ui_5aa554) itself.
//   The panel: Surface_stretchBlit of g_sprites541a18 + 0x2c over g_sprites58b390 + 0x2c at (0xd8, 0x1e2); sel == 0x15:
//   Surface_drawScaled g_sprites570cb8 + 0xe70 at (0xf6, 0x232); sel == 0x16: + 0xf20 at (0xdc, 0x1fe); for i = 0..15 the
//   tool icon g_sprites570cb8 + 0x2c (k + 4 i) at point i, k = 2 when f_w_4c2848 equals the signed byte f_tab_bytes_4c79a0[i],
//   else 1 when i == sel, else 0; the hole icon g_sprites570cb8 + 0x84 at point 0 when the dword +8 of hole record
//   g_holesPlusOne (g_holes + 0x208 n) is set or n >= 0x13; when the dword +0x18 of that record is 0 and n < 0x13 the original
//   queues an animated flag sprite (cellAt + addSprite: the sprite queue, S3.5's; reported); setDrawTarget(g_fontRef_519fd8);
//   for i = 0..15 the tool's type name (g_tileTypeAttrs + 0x30 b, b = the signed byte f_tab_bytes_4c79a0[i], first byte - 0x20)
//   drawCenteredText (the label loop's call) at (point i x + 0x20, point i y + 4) in 0x80007fff, when g_difficulty
//   == 0 or f_w_4c2848 == b.
void with_scratch_buffer(Ctx &ctx, const std::string &s, const std::function<void(uint32_t)> &f);
struct InfoPanelState {
  bool init = false;
  int32_t prev = -1, count = 0;
};
InfoPanelState &info_panel_state(Gfx &g, Ctx &ctx) {
  InfoPanelState &ip = g.state_of<InfoPanelState>("hud.infoPanel");
  if (!ip.init) {
    ip.init = true;
    ip.prev = ctx.mem.load<int32_t>(va(ctx, GSO(g_panelState, 4)));
    ip.count = ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_ui_5aa554));
  }
  return ip;
}
// drawTerrainToolbar's dwell (c3ei.cpp DrawTerrainToolbar_re: the counter pair g_panelState + 4 / g_ui_5aa554 the other panels
// share): a new sel restarts it, the same one counts; after more than ten frames on a button the tip (roadmap S3.7)
void toolbar_dwell(Gfx &g, Ctx &ctx, int32_t sel, int32_t a, int32_t b) {
  InfoPanelState &ip = info_panel_state(g, ctx);
  if (sel != ip.prev) {
    ip.count = 0;
    ip.prev = sel;
    return;
  }
  ++ip.count;
  if (sel != -1 && ip.count > hover_tip_dwell()) gfxt::terrain_toolbar_tip(g, ctx, sel, a, b);
}

// drawAmenityRow's drawing (c3ei.cpp DrawAmenityRow_re; its hit result is commands_layout.inc's amenity_row_hit): with n = p3
// (3 p3 for p2 == 0x1a3), x0 = p1 - (47 n + bias) / 4 + 0x220 (arithmetic shift), the strip: n / 2 pieces Surface_stretchBlit of
// g_sprites55e930 + 0x2c (i != 0) over g_sprites542e50 + the same at (x, 0x1dd), x += 0x44 after the first and 0x2f after the
// others, then the end piece (n & 1) | 2; for each row i (p10 != 0 skips a clear bit i): px = (A + (A < 0)) / 2 + x0 + 0x2c with
// A = 0x2f i, py = 0x1f4 (+ 0x1b for odd i); the hover is i when approxDistance(p7 - px, 2 (p8 - py)) < 0x14; k = 3, 0 when
// the hover is i, 1 when g_panelState + 0x10 is i; Surface_drawScaled of (odd ? g_sprites55e930 + 0x160 : + 0xb0) + 0x2c k at
// (px - 0x16, py - 0x14); the amenity itself is queued with cellAt / addSprite (the sprite queue: reported); the display
// scale is set back to (1, 1, 1) at the end.
int32_t amenity_row(Gfx &g, Ctx &ctx, int32_t p1, int32_t p2, int32_t p3, int32_t p4, int32_t p5, int32_t p6, int32_t p7,
                    int32_t p8, int32_t p9, uint32_t p10, uint32_t p11) {
  const uint32_t root = g.root_va(ctx);
  if (p2 == 0x1a3) p3 = mul32(p3, 3);
  const int32_t n = p3, n47 = mul32(47, n);
  const int32_t x0 = add32(sub32(p1, sar32(add32(n47, (n47 >> 31) & 3), 2)), 0x220);
  const int32_t half = sar32(add32(n, (n >> 31) & 1), 1);
  int32_t x = x0;
  const uint32_t strip = va(ctx, GSO(g_sprites55e930, 0)), smask = va(ctx, GSO(g_sprites542e50, 0));
  auto piece = [&](uint32_t b) {
    call(g, ctx, "Surface_stretchBlit", {strip + 0x2cu * b, smask + 0x2cu * b, root, static_cast<uint32_t>(x), 0x1dd, 0});
  };
  for (int32_t i = 0; i < half; ++i) {
    piece(i != 0 ? 1u : 0u);
    x = add32(x, i != 0 ? 0x2f : 0x44);
  }
  piece(static_cast<uint32_t>(n & 1) | 2u);
  // roadmap S3.5: each row's amenity is a sprite-sheet cell drawn through addSprite (c3ei.cpp DrawAmenityRow_re): p9 == -1
  // takes 0xf0 for p6 >= -2, else 0x8c; id = B + p2 (B += p4 per row); for p3 == 1 and id != 0x168 first the cell of sheet
  // k2 = id + 3 (id + 1 for 0x1ec / 0x1ca), then the cell of sheet id, both at column (its columns - 1), row 0 (cellAt, read
  // only), drawn by gfxs::add_sprite(px, py, 0, p9, p9, cell, p6, i + p5 [| 0x400 for p11's bit i]); p2 / p5 then step as the
  // original's tail does (0x1a9 -> 0x1af, 0x1a3 -> 0x1a9, 0x226: p5 - 1)
  if (p9 == -1) p9 = p6 >= -2 ? 0xf0 : 0x8c;
  auto cols = [&](int32_t s) {
    return ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_sheetCols) + 4u * static_cast<uint32_t>(s));
  };
  auto cell = [&](int32_t s) {
    return cm::call_peer(ctx, "cellAt", {static_cast<uint32_t>(s), static_cast<uint32_t>(sub32(cols(s), 1)), 0}).eax;
  };
  int32_t hover = -1, A = 0, B = 0;
  for (int32_t i = 0; i < n; ++i, A = add32(A, 0x2f), B = add32(B, p4)) {
    if (p10 != 0 && (p10 & (1u << (static_cast<uint32_t>(i) & 31u))) == 0) continue;
    const int32_t id = add32(B, p2);
    const int32_t px = add32(add32((A + (A < 0 ? 1 : 0)) / 2, x0), 0x2c);
    const int32_t py = 0x1f4 + ((i & 1) ? 0x1b : 0);
    if (approx_distance(sub32(p7, px), mul32(sub32(p8, py), 2)) < 0x14) hover = i;
    uint32_t k = 3;
    if (hover == i) k = 0;
    if (ctx.mem.load<int32_t>(va(ctx, GSO(g_panelState, 0x10))) == i) k = 1;
    const uint32_t b[8] = {strip + ((i & 1) ? 0x160u : 0xb0u) + 0x2cu * k, root, static_cast<uint32_t>(sub32(px, 0x16)),
                           static_cast<uint32_t>(sub32(py, 0x14)), 1, 1, 1, 0};
    draw_scaled(g, ctx, b, 1, 1, 1, 0, "drawAmenityRow");
    if (p3 == 1 && id != 0x168) {
      const int32_t k2 = (id == 0x1ec || id == 0x1ca) ? add32(id, 1) : add32(id, 3);
      const int32_t v[8] = {px, py, 0, p9, p9, static_cast<int32_t>(cell(k2)), p6, add32(i, p5)};
      gfxs::add_sprite(g, ctx, v);
    }
    int32_t a8 = add32(i, p5);
    if (p11 & (1u << (static_cast<uint32_t>(i) & 31u))) a8 |= 0x400;
    const int32_t v[8] = {px, py, 0, p9, p9, static_cast<int32_t>(cell(id)), p6, a8};
    gfxs::add_sprite(g, ctx, v);
    if (p2 == 0x1a9) {
      const int32_t hi = static_cast<int32_t>((static_cast<int64_t>(add32(n, n)) * 0x55555556LL) >> 32);
      const int32_t q = hi + static_cast<int32_t>(static_cast<uint32_t>(hi) >> 31);
      if (i + 1 == q) {
        const int32_t h2 = static_cast<int32_t>((static_cast<int64_t>(n) * 0x55555555LL) >> 32);
        const int32_t s = sar32(sub32(h2, n), 1);
        p2 = 0x1af;
        p5 = add32(p5, s + static_cast<int32_t>(static_cast<uint32_t>(s) >> 31));
      }
    } else if (p2 == 0x1a3) {
      const int32_t hi = static_cast<int32_t>((static_cast<int64_t>(n) * 0x55555556LL) >> 32);
      const int32_t q = hi + static_cast<int32_t>(static_cast<uint32_t>(hi) >> 31);
      if (i + 1 == q) {
        p2 = 0x1a9;
        p5 = sub32(p5, q);
      }
    } else if (p2 == 0x226) {
      p5 = sub32(p5, 1);
    }
  }
  call(g, ctx, "Display::setScale", {1, 1, 1});   // the display slot 0x9c gets (1, 1, 1) at the end, as in the original
  return hover;
}

// drawAmenitiesPanel(a, b) (c3ei.cpp DrawAmenitiesPanel_re), the drawing: Surface_stretchBlit of g_sprites541a18 + 0x84 over
// g_sprites58b390 + 0x84 at (0xd6, 0x1e2); L = pickAmenity434980(a, b) (a pure hit test, through the registry): L > -1 Image_draw
// g_sprites59ba68 + 0x84 L at the words of f_tab_point16_4c7a88 + 0x10 + 4 L, L == -2 g_sprites58b1a0 + 0xdc at (0xfe, 0x1e3);
// M = f_w_4c2848 + 0xc: e = 0 for M 3, 1 for 0, 2 for 5, 3 for 1, 4 for 4, 5 for 2, else none; f_w_4c2848 == 0x10 or M == 0x10:
// e = 6; M == 0x13: e = 9; with an e, Image_draw g_sprites59ba68 + 0x2c + 0x84 e at its words; g_ui_56d1a8 + 8 <= 0: Image_draw
// g_sprites59ba68 + 0x160 at the words of entry 2; then by M (each followed by the e image again): 1 the row (-0x54, 0x208, 5),
// 4 (g_amenityUnlockMask != 0) the row (0, 0x168, 0xe, mask), 0x13 (-0x10, 0x226, 8), 2 (0xd, 0x1a3, 5), 0x10 (0x64, 0x12f, 7).
// The tips (M == 4's two drawPanelBackdrop lines for a hovered row, the dwell tip after ten frames: drawMapLine bars and
// drawCenteredText) are tooltips: reported. The dwell counter is the one showInfoCursorPanel's drawing keeps.
void amenities_panel(Gfx &g, Ctx &ctx, int32_t a, int32_t b) {
  const uint32_t root = g.root_va(ctx);
  GameState &gs = ctx.gs();
  auto img = [&](uint32_t s, int32_t x, int32_t y) { call(g, ctx, "Image_draw", {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0}); };
  auto pt = [&](int32_t i, int k) { return static_cast<int32_t>(ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7a88, 0x10 + 4 * i + 2 * k)))); };
  const uint32_t pics = va(ctx, GSO(g_sprites59ba68, 0));
  call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0x84)), va(ctx, GSO(g_sprites58b390, 0x84)), root, 0xd6, 0x1e2, 0});
  const int32_t L = static_cast<int32_t>(cm::call_peer(ctx, "pickAmenity434980", {static_cast<uint32_t>(a), static_cast<uint32_t>(b)}).eax);
  if (L > -1) img(pics + 0x84u * static_cast<uint32_t>(L), pt(L, 0), pt(L, 1));
  if (L == -2) img(va(ctx, GSO(g_sprites58b1a0, 0xdc)), 0xfe, 0x1e3);
  const int32_t M = gs.f_w_4c2848[3];
  int32_t e = -1;
  if (M == 3) e = 0;
  else if (M == 0) e = 1;
  else if (M == 5) e = 2;
  else if (M == 1) e = 3;
  else if (M == 4) e = 4;
  else if (M == 2) e = 5;
  if (gs.f_w_4c2848[0] == 0x10 || M == 0x10) e = 6;
  if (M == 0x13) e = 9;
  auto e_image = [&]() {
    if (e != -1) img(pics + 0x2cu + 0x84u * static_cast<uint32_t>(e), pt(e, 0), pt(e, 1));
  };
  e_image();
  if (ctx.mem.load<int32_t>(va(ctx, GSO(g_ui_56d1a8, 8))) <= 0) img(pics + 0x160, pt(2, 0), pt(2, 1));
  const uint32_t mask = static_cast<uint32_t>(gs.g_amenityUnlockMask);
  if (M == 1) amenity_row(g, ctx, -0x54, 0x208, 5, M, 0xaa, 4, a, b, 0x28, 0, 0);
  else if (M == 4 && mask != 0) {   // roadmap S3.7: the hovered row's two drawPanelBackdrop lines
    const int32_t row = amenity_row(g, ctx, 0, 0x168, 0xe, 1, 0x64, -4, a, b, -1, mask, 0);
    if (row != -1) gfxt::amenities_row_tip(g, ctx, row, a, b);
  }
  else if (M == 0x13) amenity_row(g, ctx, -0x10, 0x226, 8, 1, 0xa9, 4, a, b, 0x28, 0, 0);
  else if (M == 2) amenity_row(g, ctx, 0xd, 0x1a3, 5, 0, 0x2d, -6, a, b, 0x28, 0, 0);
  else if (M == 0x10) amenity_row(g, ctx, 0x64, 0x12f, 7, 1, 0xb4, 2, a, b, 0x64, 0, 0);
  if (M == 1 || M == 4 || M == 0x13 || M == 2 || M == 0x10) e_image();
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519fd8), 0, 0, 0});
  InfoPanelState &ip = info_panel_state(g, ctx);
  if (L != ip.prev) {
    ip.count = 0;
    ip.prev = L;
    return;
  }
  ++ip.count;
  if (L != -1 && ip.count > hover_tip_dwell()) {
    gfxt::amenities_tip(g, ctx, L, a, b);   // roadmap S3.7
    if (g.hover_help) gfxt::hover_help_amenity(g, ctx, L, a, b);   // P11C fix 2 (gfx.h gfx_set_hover_help)
  }
}

void b_info_panel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  GameState &gs = ctx.gs();
  if (gs.g_buildPanelSlot != 0) {
    amenities_panel(g, ctx, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
    return;
  }
  const uint32_t root = g.root_va(ctx);
  const int32_t ct = static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_courseType)));
  auto pt = [&](int32_t i, int k) -> int32_t {
    if (i >= 13) return ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 0x74)) + static_cast<uint32_t>(24 * ct + 8 * (i - 13) + 4 * k));
    return ctx.mem.load<int16_t>(va(ctx, GSO(f_tab_point16_4c79b4, 4 * i + 2 * k)));
  };
  auto tid = [&](int32_t i) { return static_cast<int32_t>(static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(f_tab_bytes_4c79a0, i))))); };
  const int32_t sel = static_cast<int32_t>(cm::call_peer(ctx, "hitTestSlot432f90", {a[0], a[1]}).eax);
  InfoPanelState &ip = info_panel_state(g, ctx);
  if (sel == ip.prev && sel != -1 && sel < 0x14 && ip.count > hover_tip_dwell()) {
    gfxt::info_panel_tool_tip(g, ctx, sel);   // roadmap S3.7
    if (g.hover_help) gfxt::hover_help_tool(g, ctx, sel);   // P11C (gfx.h gfx_set_hover_help)
  }
  auto stretch = [&](uint32_t s, uint32_t m, int32_t x, int32_t y) {
    call(g, ctx, "Surface_stretchBlit", {s, m, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0});
  };
  auto scaled = [&](uint32_t s, int32_t x, int32_t y) {
    const uint32_t b[8] = {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1, 1, 1, 0};
    draw_scaled(g, ctx, b, 1, 1, 1, 0, "showInfoCursorPanel");
  };
  const uint32_t spr = va(ctx, GSO(g_sprites570cb8, 0));
  stretch(va(ctx, GSO(g_sprites541a18, 0x2c)), va(ctx, GSO(g_sprites58b390, 0x2c)), 0xd8, 0x1e2);
  if (sel == 0x15 || p13a_bulldoze_on(gs)) scaled(spr + 0xe70, 0xf6, 0x232);  // P13A: lit while bulldoze is armed
  if (sel == 0x16) scaled(spr + 0xf20, 0xdc, 0x1fe);
  const int32_t tool = gs.f_w_4c2848[0];
  for (int32_t i = 0; i < 16; ++i) {
    uint32_t k = sel == i ? 1 : 0;
    if (tool == tid(i)) k = 2;
    scaled(spr + 0x2cu * (k + 4u * static_cast<uint32_t>(i)), pt(i, 0), pt(i, 1));
  }
  const int32_t n = gs.g_holesPlusOne;
  const uint32_t rec = va(ctx, GSO(g_holes, 0)) + 0x208u * static_cast<uint32_t>(n);
  if (ctx.mem.load<uint32_t>(rec + 8) != 0 || n >= 0x13) scaled(spr + 0x84, pt(0, 0), pt(0, 1));
  if (ctx.mem.load<uint32_t>(rec + 0x18) == 0 && n < 0x13) {
    // roadmap S3.5: the hole flag, a sprite-sheet cell (showInfoCursorPanel, cellAt + addSprite): sheet c + 0x189 with c the
    // signed g_courseType, column g_date % its columns when the tool f_w_4c2848 is 1 (else 0), row 3; addSprite(point 1 x +
    // 0x1f, point 1 y + 0x18, 0, 0x28, 0x28, cell, 4, 0x63), drawn at once by the scene family (gfxs::add_sprite)
    const int32_t sheet = add32(static_cast<int32_t>(gs.g_courseType), 0x189);
    const int32_t fc = ctx.mem.load<int32_t>(state_va(ctx, &GameState::g_sheetCols) + 4u * static_cast<uint32_t>(sheet));
    const int32_t col = tool == 1 ? mod32(gs.g_date, fc) : 0;
    const uint32_t c = cm::call_peer(ctx, "cellAt", {static_cast<uint32_t>(sheet), static_cast<uint32_t>(col), 3}).eax;
    const int32_t v[8] = {add32(pt(1, 0), 0x1f), add32(pt(1, 1), 0x18), 0, 0x28, 0x28, static_cast<int32_t>(c), 4, 0x63};
    gfxs::add_sprite(g, ctx, v);
  }
  call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519fd8), 0, 0, 0});
  for (int32_t i = 0; i < 16; ++i) {
    const int32_t b = tid(i);
    if (gs.g_difficulty != 0 && tool != b) continue;
    std::string name = gfxb::vstring(ctx.mem, va(ctx, GSO(g_tileTypeAttrs, 0)) + 0x30u * static_cast<uint32_t>(b));
    if (!name.empty()) name[0] = static_cast<char>(static_cast<uint8_t>(name[0]) + 0xe0);
    with_scratch_buffer(ctx, name, [&](uint32_t buf) {
      gfxb::draw_text_string(g, ctx, name, buf, add32(pt(i, 0), 0x20), add32(pt(i, 1), 4), 0x80007fffu, true, "showInfoCursorPanel");
    });
  }
  if (sel == ip.prev) {
    ++ip.count;
    if (sel >= 0x14 && ip.count > hover_tip_dwell()) {
      gfxt::info_panel_slot_tip(g, ctx, sel);   // roadmap S3.7
      if (g.hover_help) gfxt::hover_help_slot(g, ctx, sel);   // P11C fix 2 (gfx.h gfx_set_hover_help)
    }
  } else {
    ip.count = 0;
    ip.prev = sel;
  }
}


// The text a registered sim function writes into g_textBuffer, without keeping its writes: the whole GameState is saved
// before the call and put back after it (a drawing body leaves the GameState as it found it).
std::string scratch_text(Ctx &ctx, const std::string &start, const char *name, std::initializer_list<uint32_t> args) {
  GameState &gs = ctx.gs();
  std::vector<uint8_t> saved(sizeof(GameState));
  std::memcpy(saved.data(), &gs, sizeof(GameState));
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
  for (size_t i = 0; i <= start.size(); ++i) ctx.mem.store<uint8_t>(buf + static_cast<uint32_t>(i), i < start.size() ? static_cast<uint8_t>(start[i]) : 0);
  cm::call_peer(ctx, name, args);
  std::string out = gfxb::vstring(ctx.mem, buf);
  std::memcpy(&gs, saved.data(), sizeof(GameState));
  return out;
}

// runs f with `s` NUL-terminated in g_textBuffer, then puts the buffer's bytes back
void with_scratch_buffer(Ctx &ctx, const std::string &s, const std::function<void(uint32_t)> &f) {
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
  const size_t n = std::min(s.size(), sizeof(GameState::g_textBuffer) - 1);
  std::vector<uint8_t> saved(n + 1);
  for (size_t i = 0; i <= n; ++i) saved[i] = ctx.mem.load<uint8_t>(buf + static_cast<uint32_t>(i));
  for (size_t i = 0; i <= n; ++i) ctx.mem.store<uint8_t>(buf + static_cast<uint32_t>(i), i < n ? static_cast<uint8_t>(s[i]) : 0);
  f(buf);
  for (size_t i = 0; i <= n; ++i) ctx.mem.store<uint8_t>(buf + static_cast<uint32_t>(i), saved[i]);
}

// drawMatchScoreboard(who) (c3eh.cpp DrawMatchScoreboard_re), the drawing: in g_fontRef_519fd8 the two header texts
// f_strpool_4c8808 + 0xa0 at (0x19a, 0x207) and f_strpool_4c88f8 at (0x19e, 0x216) in 0x80007fff; in g_font51b360 the
// opponent line by the opponent record's (who ^ 1) +0x18 byte & 0xe0: 0x20 with g_flags bit 21: f_strpool_4d558c + 0x3c /
// + 0x30 at (0x145, 0x20a / 0x215), 0x20 without: + 0x24 at (0x14e, 0x20a) and f_strpool_4c61dc + 4, appendNumber(2000
// g_challengeLevel), + 0x1c at (0x140, 0x215), else + 0x10 / + 0x08 at (0x14e, 0x20a / 0x215), all in 0x80000000; then two
// rows (y 0x228, 0x23d), cur = who then its partner (word +0xa2): buildGolferName(cur, 0) (+ f_strpool_4d558c for who), in
// g_fontRef_519fd8 when its Text_put width in g_font51b360 is above 0x73, drawTextWithColor at (0x140, y) in 0x80000000; per
// hole column col (x = 0x1aa + 0x12 col below 0x300, hole record g_holes + 0x208 col) with a non-zero par byte: for who the
// column number and par centred at (x, y - 0x22) / (x, y - 0x13) in 0x80006318; the golfer's hole byte +0x21 h: h < col
// nothing, h == col the stroke byte +0x22 in 0x80004210, h > col the score byte +0x23 + col in the colour by score against
// par (equal 0x80002108, one under 0x80006000, more 0x80007d08, one over 0x80000018, more 0x80004010), centred at (x, y).
void match_scoreboard(Gfx &g, Ctx &ctx, int32_t who) {
  const uint32_t root = g.root_va(ctx);
  const uint32_t big = state_va(ctx, &GameState::g_fontRef_519fd8), small = state_va(ctx, &GameState::g_font51b360);
  auto font = [&](uint32_t f) { call(g, ctx, "setDrawTarget", {root, f, 0, 0, 0}); };
  // the text is put into g_textBuffer for the call (the provider measures centred text by its address) and the buffer is
  // put back afterwards
  auto text = [&](const std::string &s, int32_t x, int32_t y, uint32_t c, bool centre) {
    with_scratch_buffer(ctx, s, [&](uint32_t buf) { gfxb::draw_text_string(g, ctx, s, buf, x, y, c, centre, "drawMatchScoreboard"); });
  };
  auto pool = [&](uint32_t sym_off) { return gfxb::vstring(ctx.mem, va(ctx, sym_off)); };
  const uint32_t golfers = va(ctx, GSO(g_golfers, 0));
  auto rec = [&](int32_t k) { return golfers + (static_cast<uint32_t>(k) << 8); };
  font(big);
  text(pool(GSO(f_strpool_4c8808, 0xa0)), 0x19a, 0x207, 0x80007fffu, false);
  text(pool(GSO(f_strpool_4c88f8, 0)), 0x19e, 0x216, 0x80007fffu, false);
  font(small);
  const uint8_t opp = ctx.mem.load<uint8_t>(rec(who ^ 1) + 0x18);
  if ((opp & 0xe0) == 0x20) {
    if (ctx.gs().g_flags & (1u << 21)) {
      text(pool(GSO(f_strpool_4d558c, 0x3c)), 0x145, 0x20a, 0x80000000u, false);
      text(pool(GSO(f_strpool_4d558c, 0x30)), 0x145, 0x215, 0x80000000u, false);
    } else {
      text(pool(GSO(f_strpool_4d558c, 0x24)), 0x14e, 0x20a, 0x80000000u, false);
      const std::string s = scratch_text(ctx, pool(GSO(f_strpool_4c61dc, 4)), "appendNumber",
                                         {static_cast<uint32_t>(mul32(ctx.gs().g_challengeLevel, 2000))}) +
                            pool(GSO(f_strpool_4d558c, 0x1c));
      text(s, 0x140, 0x215, 0x80000000u, false);
    }
  } else {
    text(pool(GSO(f_strpool_4d558c, 0x10)), 0x14e, 0x20a, 0x80000000u, false);
    text(pool(GSO(f_strpool_4d558c, 0x08)), 0x14e, 0x215, 0x80000000u, false);
  }
  int32_t cur = who;
  const int32_t x0 = mode() == 10 ? 0x1ab : 0x1aa;
  for (int32_t y = 0x228; y < 0x24e; y += 0x15) {
    std::string name = scratch_text(ctx, "", "buildGolferName", {static_cast<uint32_t>(cur), 0});
    if (cur == who) name += pool(GSO(f_strpool_4d558c, 0));
    int32_t w = 0;
    bool measured = false;
    if (TextMetrics *tm = text_metrics(ctx))
      with_scratch_buffer(ctx, name, [&](uint32_t buf) { measured = tm->glyph_width(ctx, small, buf, static_cast<int32_t>(name.size()), w); });
    if (!measured) {
      g.unsup("drawMatchScoreboard: the provider cannot measure a name");
      return;
    }
    font(w > 0x73 ? big : small);
    text(name, 0x140, y, 0x80000000u, false);
    font(small);
    const uint32_t r = rec(cur);
    for (int32_t col = 0, x = x0; x < 0x300; ++col, x += 0x12) {
      const int8_t par = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(g_holes, 0)) + 0x208u * static_cast<uint32_t>(col)));
      if (par == 0) continue;
      if (cur == who) {
        text(std::to_string(col), x, y - 0x22, 0x80006318u, true);
        text(std::to_string(par), x, y - 0x13, 0x80006318u, true);
      }
      const int32_t h = static_cast<int8_t>(ctx.mem.load<uint8_t>(r + 0x21));
      if (h < col) continue;
      uint32_t colour = 0x80004210u;
      std::string s;
      if (h == col) {
        s = std::to_string(static_cast<int8_t>(ctx.mem.load<uint8_t>(r + 0x22)));
      } else {
        const int32_t sc = static_cast<int8_t>(ctx.mem.load<uint8_t>(r + 0x23 + static_cast<uint32_t>(col)));
        s = std::to_string(sc);
        colour = 0x80002108u;
        if (sc < par) colour = 0x80006000u;
        if (sc < par - 1) colour = 0x80007d08u;
        if (sc > par) colour = 0x80000018u;
        if (sc > par + 1) colour = 0x80004010u;
      }
      text(s, x, y, colour, true);
    }
    cur = ctx.mem.load<int16_t>(r + 0xa2);
  }
}
// drawGolferActionPanel.draw(x, y) (c3fb.cpp DrawGolferActionPanel_re, the drawing; commands_layout.inc keeps the state and
// hands this over unless the detail panel is up): Surface_stretchBlit of g_sprites541a18 + 0xdc over g_sprites58b390 + 0xdc at
// (0xd6, 0x1da); h = hitGolferPanel435f00(x, y) (a pure hit test, through the registry); h != -1: button image
// g_sprites583a80 + 0xb0 h at the words of g_golferPanelLayout + 4 h, stretched over g_sprites586f50 for h in 4..8, else
// Image_draw. With a player golfer p = g_playerGolfer: the images + 0x108 / + 0x1b8 / + 0x268 at buttons 1..3; in a match
// (word +0xcc of p's record != -1) Image_draw g_sprites541a18 + 0x108 at (0x129, 0x1ed) and drawMatchScoreboard(p);
// otherwise the five shot buttons, frame + 0x2c a over mask g_sprites586f50 + 0x2c c: button 4 (+ 0x2c0) a, c = 1, 1 when
// g_playerShotStyle is 0, else (h != 4) ? (3, 1) : (0, 0); the lie class (the signed byte g_tileTypeAttrs + 0x22 + 0x30 t,
// t = typeAtPos of p's +0xd4 / +0x04) <= 0: buttons 5..7 (+ 0x370, + 0x420, + 0x4d0) each 1, 1 when the style is -1 / 1 / 3,
// else c = (h != 4), a = (h != k) ? 3 : 0; a class above 0: + 0x3c8, + 0x478, + 0x528 over mask + 0x2c (greyed); button 8
// (+ 0x580): style 0: c = 1, a = (h != 8) ? 3 : 0; else c = (h != 4), a = 1 for style 4 else (h != 8) ? 3 : 0. With no
// player golfer: + 0x108 at button 1 when g_holesPlusOne <= 1; + 0x1b8 at button 2 unless g_flags bit 13 with more than one
// hole and a pro chosen (g_proChallenger != -1); + 0x268 at button 3 unless g_flags bit 22; then the bag: for each of the ten
// skill bytes g_playerSkills[i] != 0 the text club name (f_ptrtab_4c2c3c[i]) + f_strpool_4c60c8 + decimal 10 n +
// f_strpool_4c60c4 in g_fontRef_519fd8 at (col, ly), ly += 0xc, at ly >= 0x258 col 0x281 and ly 0x21c again (from 0x1ee,
// 0x21c). The dwell tip after ten calls on the same button (drawPanelBackdrop) is a tooltip: reported.
void b_action_panel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  GameState &gs = ctx.gs();
  const uint32_t root = g.root_va(ctx);
  const uint32_t imgs = va(ctx, GSO(g_sprites583a80, 0)), masks = va(ctx, GSO(g_sprites586f50, 0));
  auto bx = [&](int32_t k) { return static_cast<uint32_t>(static_cast<int32_t>(ctx.mem.load<int16_t>(va(ctx, GSO(g_golferPanelLayout, 4 * k))))); };
  auto by = [&](int32_t k) { return static_cast<uint32_t>(static_cast<int32_t>(ctx.mem.load<int16_t>(va(ctx, GSO(g_golferPanelLayout, 4 * k + 2))))); };
  auto img = [&](uint32_t s, int32_t k) { call(g, ctx, "Image_draw", {s, root, bx(k), by(k), 0}); };
  auto stretch = [&](uint32_t s, uint32_t m, int32_t k) { call(g, ctx, "Surface_stretchBlit", {s, m, root, bx(k), by(k), 0}); };
  auto shot = [&](uint32_t off, int32_t fa, int32_t fc, int32_t k) {
    if (mode() == 9 && k == 4) fa = (fa + 1) % 4;
    stretch(imgs + off + 0x2cu * static_cast<uint32_t>(fa), masks + 0x2cu * static_cast<uint32_t>(fc), k);
  };
  call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0xdc)), va(ctx, GSO(g_sprites58b390, 0xdc)), root, 0xd6, 0x1da, 0});
  const int32_t h = static_cast<int32_t>(cm::call_peer(ctx, "hitGolferPanel435f00", {a[0], a[1]}).eax);
  if (h != -1) {
    const uint32_t im = imgs + 0xb0u * static_cast<uint32_t>(h);
    if (h >= 4 && h <= 8) stretch(im, masks, h);
    else img(im, h);
  }
  const int32_t p = gs.g_playerGolfer;
  if (p != -1) {
    img(imgs + 0x108, 1);
    img(imgs + 0x1b8, 2);
    img(imgs + 0x268, 3);
    const uint32_t rec = va(ctx, GSO(g_golfers, 0)) + (static_cast<uint32_t>(p) << 8);
    if (ctx.mem.load<int32_t>(rec + 0xcc) != -1) {
      call(g, ctx, "Image_draw", {va(ctx, GSO(g_sprites541a18, 0x108)), root, 0x129, 0x1ed, 0});
      match_scoreboard(g, ctx, p);
    } else {
      const int32_t t = static_cast<int32_t>(cm::call_peer(ctx, "typeAtPos", {ctx.mem.load<uint32_t>(rec + 0xd4), ctx.mem.load<uint32_t>(rec + (SG_DEFECT(g_defect_shot_lie_mixed_tile) ? 4u : 0xd8u))}).eax);   // P22B mainLoop.shotButtonsReadBallTile
      const bool rough = static_cast<int8_t>(ctx.mem.load<uint8_t>(va(ctx, GSO(g_tileTypeAttrs, 0x22)) + 0x30u * static_cast<uint32_t>(t))) <= 0;
      int32_t fa, fc;
      auto pick = [&](int32_t want, int32_t k) {
        if (gs.g_playerShotStyle == want) fa = fc = 1;
        else { fc = h != 4 ? 1 : 0; fa = h != k ? 3 : 0; }
      };
      if (gs.g_playerShotStyle == 0) fa = fc = 1;
      else { fc = h != 4 ? 1 : 0; fa = h != 4 ? 3 : 0; }
      shot(0x2c0, fa, fc, 4);
      if (!rough) {
        stretch(imgs + 0x3c8, masks + 0x2c, 5);
        stretch(imgs + 0x478, masks + 0x2c, 6);
        stretch(imgs + 0x528, masks + 0x2c, 7);
      } else {
        pick(-1, 5);
        shot(0x370, fa, fc, 5);
        pick(1, 6);
        shot(0x420, fa, fc, 6);
        pick(3, 7);
        shot(0x4d0, fa, fc, 7);
      }
      const int32_t s2 = gs.g_playerShotStyle;
      if (s2 == 0) { fc = 1; fa = h != 8 ? 3 : 0; }
      else { fc = h != 4 ? 1 : 0; fa = s2 == 4 ? 1 : (h != 8 ? 3 : 0); }
      shot(0x580, fa, fc, 8);
    }
  } else {
    if (gs.g_holesPlusOne <= 1) img(imgs + 0x108, 1);
    if (!(gs.g_flags & 0x2000u) || gs.g_holesPlusOne <= 1 || gs.g_proChallenger == -1) img(imgs + 0x1b8, 2);
    if ((gs.g_flags & (1u << 22)) == 0) img(imgs + 0x268, 3);
    call(g, ctx, "setDrawTarget", {root, state_va(ctx, &GameState::g_fontRef_519fd8), 0, 0, 0});
    const std::string sep = gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4c60c8));
    const std::string end = gfxb::vstring(ctx.mem, state_va(ctx, &GameState::f_strpool_4c60c4));
    int32_t col = 0x1ee, ly = 0x21c;
    for (int32_t i = 0; i < 10; ++i) {
      const int32_t n = static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_playerSkills) + static_cast<uint32_t>(i)));
      if (n == 0) continue;
      std::string s = gfxb::vstring(ctx.mem, ctx.mem.load<uint32_t>(va(ctx, GSO(f_ptrtab_4c2c3c, 4 * i)))) + sep + std::to_string(mul32(n, 10)) + end;
      gfxb::draw_text_string(g, ctx, s, 0, col, ly, 0x80007fffu, false, "drawGolferActionPanel");
      ly += 0xc;
      if (ly >= 0x258) { col = 0x281; ly = 0x21c; }
    }
  }
  InfoPanelState &ip = info_panel_state(g, ctx);
  if (h != ip.prev) {
    ip.count = 0;
    ip.prev = h;
    return;
  }
  ++ip.count;
  if (h != -1 && ip.count > hover_tip_dwell())   // roadmap S3.7
    gfxt::action_panel_tip(g, ctx, h, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
}
// ------------------------------------------------------------------------------------------------ the employee panels
// (roadmap S3.6b, U-1359). commands_layout.inc keeps the two panels' state (lists, scroll, selection, hire trigger) and
// hands their drawing over BEFORE its own writes, so every value the original reads after its own update is recomputed here
// from the same rule (the detail panel's scroll step and its list of this frame); nothing is written into the GameState.

// TEST-ONLY call log of the two panels (gfx.call gfx_hud.recordPanels(1) clears and starts it, (0) stops it and writes
// `$gfx/hud_panel_calls.txt`): one line per drawing call the panel bodies make, `<callee> <args as unsigned decimal>` and
// for a text call `|<text>`; port/test/test_gfx_hud.py compares it with the calls the ORIGINAL panel functions make under
// the emulator oracle (SG_EMU_ORACLE=1)
struct PanelRec {
  bool on = false;
  std::string log;
};
PanelRec &panel_rec(Gfx &g) { return g.state_of<PanelRec>("hud.panelRec"); }
void prec(Gfx &g, const char *name, std::initializer_list<uint32_t> args, const std::string *text = nullptr) {
  PanelRec &r = panel_rec(g);
  if (!r.on) return;
  r.log += name;
  for (uint32_t v : args) r.log += " " + std::to_string(v);
  if (text) r.log += "|" + *text;
  r.log += "\n";
}
void b_record_panels(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  PanelRec &r = panel_rec(g);
  if (a[0] != 0) {
    r.on = true;
    r.log.clear();
    return;
  }
  r.on = false;
  gfxb::put_file(ctx, "$gfx/hud_panel_calls.txt", std::vector<uint8_t>(r.log.begin(), r.log.end()));
}

// the text a registered sim function appends to an empty g_textBuffer when it writes nothing else (buildGolferName,
// appendWalkerName): only the buffer's bytes are saved and put back (test_gfx_hud.py checks a whole panel draw leaves the
// GameState unchanged)
std::string buffer_text(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const uint32_t buf = state_va(ctx, &GameState::g_textBuffer);
  const uint32_t n = static_cast<uint32_t>(sizeof(GameState::g_textBuffer));
  std::vector<uint8_t> saved(n);
  for (uint32_t i = 0; i < n; ++i) saved[i] = ctx.mem.load<uint8_t>(buf + i);
  ctx.mem.store<uint8_t>(buf, 0);
  cm::call_peer(ctx, name, args);
  std::string out = gfxb::vstring(ctx.mem, buf);
  for (uint32_t i = 0; i < n; ++i) ctx.mem.store<uint8_t>(buf + i, saved[i]);
  return out;
}
// a call of another family's body when the tree holds one (addSprite: the scene family's, S3.5; drawCircle: the overlay
// lines, S3.8), else the reason
void call_or_report(Gfx &g, Ctx &ctx, const char *name, std::initializer_list<uint32_t> args, const char *reason) {
  if (gfxb::find_body(name)) call(g, ctx, name, args);
  else g.unsup(reason);
}
// the dwell tip of the panels' buttons (the same counter pair as the other HUD panels: g_panelState + 4 / g_ui_5aa554):
// counted here, the tip itself (drawMapLine bars or drawPanelBackdrop and centred text) is a tooltip (S3.7, reported)
void dwell(Gfx &g, Ctx &ctx, int32_t hover, const char *tip, const std::function<void()> &draw_tip) {
  InfoPanelState &ip = info_panel_state(g, ctx);
  if (hover != ip.prev) {
    ip.count = 0;
    ip.prev = hover;
    return;
  }
  ++ip.count;
  if (hover != -1 && ip.count > hover_tip_dwell()) {
    prec(g, "tooltip", {});
    (void)tip;
    draw_tip();   // roadmap S3.7
  }
}

// drawEmployeeViewPanel.draw(x, y) (c3fb.cpp DrawEmployeeViewPanel_re, the drawing; commands_layout.inc hands it over unless
// the detail panel is up): Surface_stretchBlit of g_sprites541a18 + 0xb0 over g_sprites58b390 + 0xb0 at (0xd7, 0x1da);
// h = hitEmployeeSlot435570(x, y) (pure, through the registry): Image_draw g_sprites58b1a0 + 0 at (0xf0, 0x1ee) for -3,
// + 0x2c at (0xd9, 0x20a) for -4, g_sprites59b778 + 0x1e4 at (0x135, 0x24b) for -5, + 0x210 at (0x2f4, 0x24b) for -6;
// setDrawTarget(g_fontRef_519fd8). Then the golfers g = (g_lastAllocatedGolfer - i + 0x98) % 0x98, i = 0..0x97, that are on
// the course (signed byte +0x21 > 0) or whose partner p (word +0xa2, not 0xffff) names g back and is on the course; past
// the scroll offset g_ui_5aa6b8[4]. Columns start at x1 0x136 with the name x d 0x145, b 0x146, pips c 0x156, face a 0x19c,
// rows at y 0x1f2. g_zoomScale < 4 (compact): 6 rows of 14 px, columns 0x3c apart (a column change past d 0x2f3 skips that
// golfer); the mood face (g_sprites59b778 + 0x2c clamp(word +0xa4 + 2, 1, 10)) Surface_drawScaled at (x1, y), the name
// (buildGolferName(g, 0)) drawTextWithColor at (d, y + 1) in the golfer's colour. Else (wide): 4 rows of 21 px, columns 0x79
// apart (past d 0x2b7 skipped); setDrawTarget(g_fontRef_519fd8); for odd g the hole badge g_sprites59b778 + 0 at (x1, y)
// with the decimal of the signed byte +0x21 (f_strpool_4c61dc + 8 when it is 0x13; the partner's +0x21 when it is <= 0)
// drawTextWithColor at (x1 + 5 for >= 10 else + 8, y + 0xf) in 0x80000000, and with word +0xb0 != -1 four pips g_sprites59b778
// + 0x2c0 then word +0xb2 pips + 0x23c at (c + 10 k, y + 0x11); setDrawTarget(g_font51b360); on the course: the name
// drawTextWithColor at (b, y + 5) in the colour and the face at (a, y + 2). The colour: 0x80000000, 0x80004210 for word +0xaa
// above 0xa0, 0x80000018 for +0xa8 above 0x10, 0x80006000 for +0xa6 above 0x10, 0x80007d08 for bit 29 of dword +0x10
// (signed compares, the last that holds wins). With a count n: the scroll bar fillWindowRect(g_rootWindow, top + 0x15f,
// 0x24e, bottom - top - 1, 6, 0x8000739f), top = clamp(s 400 / n, 0, 0x18f), bottom = clamp((s + page) 400 / n, 1, 0x190),
// page 0x10 wide / 0x30 compact. Last setDrawTarget(g_fontRef_519fd8) and the dwell tip (-4, -3, -2 have texts).
// P22D fix 2 (log/roadmap/P22/P22D_notes.md item 5b): the golfer list's added columns (k = 4 .. 3 + used) as layers right of the
// panel, with the panel's own art. Called after the panel's layer (index pi of g.hud.layers) was closed, with the root as the frame
// has it; the root is given back unchanged (every layer made here is drawn into the root and the root restored, so the 800 x 600
// frame is the original's). MemberPanel (the art cut g_sprites541a18 + 0xb0, loadInterfaceArt+0x567c, x 0xd7..0x31f) row by row:
// x 405..643 of every row is one column repeated (top border, list colour, scroll track), the list's right border is x 795..799
// (rows 0x1da..0x24a and 0x257), and rows 0x24b..0x256 hold the scroll track's right end and the page arrows from x 750 on. With
// E = 0x79 used:
// - the panel: its right end (from x 795, rows 0x24b..0x256 from x 750) becomes the repeated column (pixels its own drawing put
//   over the border stay);
// - a layer of that right end, `shift` E (the presenter puts it E columns right; a point over it reaches the game E right, the next
//   page arrow's moved place in hitEmployeeSlot435570);
// - per four added columns m, a layer drawn in the root at the list's columns 0..3 (shift 0x1e4 (m + 1)) over the art, cut to its
//   columns' place and with the art's own pixels replaced by the repeated column.
void view_panel_wide(Gfx &g, Ctx &ctx, size_t pi, int32_t used, std::vector<std::pair<std::function<void(int32_t)>, int32_t>> &cols,
                     const std::function<void()> &art) {
  Gfx::HudCapture &c = g.hud;
  if (used <= 0 || pi >= c.layers.size()) return;
  constexpr int32_t kRep = 600;            // a column of x 405..643 (the repeated one)
  constexpr int32_t kTrackY0 = 0x24b, kTrackY1 = 0x257;  // the scroll track's rows (half-open)
  constexpr int32_t kStep = 0x79, kGroup = 4 * 0x79;
  auto end_x = [&](int32_t y) { return y >= kTrackY0 && y < kTrackY1 ? 750 : 795; };  // the right end's first column per row
  auto get = [](const GfxHudLayer &l, int32_t x, int32_t y) {
    if (x < l.x0 || x >= l.x1 || y < l.y0 || y >= l.y1) return GfxLayerPx{};
    return l.px[static_cast<size_t>(y - l.y0) * static_cast<size_t>(l.x1 - l.x0) + static_cast<size_t>(x - l.x0)];
  };
  auto set = [](GfxHudLayer &l, int32_t x, int32_t y, const GfxLayerPx &p) {
    if (x < l.x0 || x >= l.x1 || y < l.y0 || y >= l.y1) return;
    l.px[static_cast<size_t>(y - l.y0) * static_cast<size_t>(l.x1 - l.x0) + static_cast<size_t>(x - l.x0)] = p;
  };
  auto same = [](const GfxLayerPx &a, const GfxLayerPx &b) { return a.kind == b.kind && a.px == b.px && a.src == b.src && a.k == b.k; };
  const std::vector<uint16_t> frame = g.root.px;
  auto restore = [&]() {
    g.root.px = frame;
    c.mark = frame;
  };
  // the art alone (its pixels and blend records), taken out of the frame's layers again
  gfx_layer_begin(ctx, kGfxLayerPanel);
  art();
  const size_t nb = c.layers.size();
  gfx_layer_end(ctx);
  if (c.layers.size() != nb + 1) {
    restore();
    return;
  }
  const GfxHudLayer B = c.layers.back();
  c.layers.pop_back();
  restore();
  const int32_t E = kStep * used;
  GfxHudLayer &P = c.layers[pi];
  GfxHudLayer R;
  R.id = P.id;
  R.anchor = P.anchor;
  R.shift = E;
  R.x0 = 746;  // four columns of the repeated one before the right end: the smooth taps at its left edge see the list's colour
  R.x1 = std::max(P.x1, 800);
  R.y0 = P.y0;
  R.y1 = P.y1;
  R.px.assign(static_cast<size_t>(R.x1 - R.x0) * static_cast<size_t>(R.y1 - R.y0), GfxLayerPx{});
  for (int32_t y = P.y0; y < P.y1; ++y) {
    const bool track = y >= kTrackY0 && y < kTrackY1;
    const GfxLayerPx rep = get(B, kRep, y);
    for (int32_t x = R.x0; x < R.x1; ++x) {
      if (x < end_x(y)) {
        set(R, x, y, rep);
        continue;
      }
      const GfxLayerPx p = get(P, x, y);
      set(R, x, y, track ? p : get(B, x, y));  // the arrows (and the -6 hover image) move; the border is the art's own
      if (track || same(p, get(B, x, y))) set(P, x, y, rep);
    }
  }
  c.layers.push_back(std::move(R));
  // the added columns, four per layer
  const int32_t groups = (used + 3) / 4;
  for (int32_t m = 0; m < groups; ++m) {
    const int32_t S = kGroup * (m + 1);
    gfx_layer_begin(ctx, kGfxLayerPanel);
    art();
    for (auto &e : cols)
      if (e.second >= 4 + 4 * m && e.second < 8 + 4 * m && e.second < 4 + used) e.first(S);
    const size_t nx = c.layers.size();
    gfx_layer_end(ctx);
    if (c.layers.size() != nx + 1) {
      restore();
      continue;
    }
    GfxHudLayer &X = c.layers.back();
    X.shift = S;
    const int32_t lo = 794 + kGroup * m;
    for (int32_t y = X.y0; y < X.y1; ++y) {
      const int32_t hi = m + 1 == groups ? end_x(y) + E : 794 + kGroup * (m + 1);
      const GfxLayerPx rep = get(B, kRep, y);
      for (int32_t x = X.x0; x < X.x1; ++x) {
        const int32_t vx = x + S;
        if (vx < lo || vx >= hi) {
          set(X, x, y, GfxLayerPx{});
          continue;
        }
        if (same(get(X, x, y), get(B, x, y))) set(X, x, y, rep);
      }
    }
    restore();
  }
}

// P22D fix 3 (gfx.h gfx_set_meter_tips; log/roadmap/P22/P22D_notes.md item 8): the status boxes' tips, from P6_StatusBar with the
// cursor's view point (F+0xb8, F+0xb0 after its View_toLocal). The boxes are its three framed images: drawScaledSprite421b90 of
// g_sprites520590 + 0x2c / + 0x58 / + 0x84 at (0x289, 0xd), (0x2a5, 0x37), (0x2bb, 0x63), cut 0x8a x 0x24, 0x6e x 0x25, 0x58 x 0x24
// (loadInterfaceArt+0x493e / +0x495c / +0x497a). A box under the cursor for more status bars than the HUD tips' dwell
// (hover_tip_dwell: hoverTip.tenFrameDwell) gets the game's own name for it: "Cash" (f_strpool_4c8808 + 0xe0) and "Fun Rating"
// (+ 0xd4), the save browser's column titles (loadGameBrowserScreen), and "Skill Rating" (f_strpool_4c615c + 0x60 without its
// ": ", the label mainLoop copies and erases before it draws the skill value), as drawPanelBackdrop centred left of the box.
void b_meter_tips(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  if (!g.meter_tips) return;
  struct Box {
    int32_t x, y, w, h;
    uint32_t text;
  };
  const Box boxes[3] = {{0x289, 0xd, 0x8a, 0x24, GSO(f_strpool_4c8808, 0xe0)},
                        {0x2a5, 0x37, 0x6e, 0x25, GSO(f_strpool_4c8808, 0xd4)},
                        {0x2bb, 0x63, 0x58, 0x24, GSO(f_strpool_4c615c, 0x60)}};
  const int32_t x = static_cast<int32_t>(a[0]), y = static_cast<int32_t>(a[1]);
  int32_t hit = -1;
  for (int32_t k = 0; k < 3; ++k)
    if (x >= boxes[k].x && x < boxes[k].x + boxes[k].w && y >= boxes[k].y && y < boxes[k].y + boxes[k].h) hit = k;
  if (hit != g.meter_prev) {
    g.meter_prev = hit;
    g.meter_count = 0;
    return;
  }
  ++g.meter_count;
  if (hit < 0 || g.meter_count <= hover_tip_dwell()) return;
  const Box &b = boxes[hit];
  std::string s = gfxb::vstring(ctx.mem, va(ctx, b.text));
  while (!s.empty() && (s.back() == ' ' || s.back() == ':')) s.pop_back();
  const int32_t half = 3 * static_cast<int32_t>(s.size());  // drawPanelBackdrop's half width (3 per character)
  gfxt::meter_tip(g, ctx, s, b.x - half - 6, b.y + b.h / 2 + 5);
}

void b_view_panel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const uint32_t root = g.root_va(ctx);
  const uint32_t big = state_va(ctx, &GameState::g_fontRef_519fd8), small = state_va(ctx, &GameState::g_font51b360);
  auto font = [&](uint32_t f) {
    prec(g, "setDrawTarget", {f});
    call(g, ctx, "setDrawTarget", {root, f, 0, 0, 0});
  };
  auto img = [&](uint32_t s, int32_t x, int32_t y) {
    prec(g, "Image_draw", {s, static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
    call(g, ctx, "Image_draw", {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0});
  };
  auto scaled = [&](uint32_t s, int32_t x, int32_t y) {
    prec(g, "Surface_drawScaled", {s, static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
    const uint32_t b[8] = {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1, 1, 1, 0};
    draw_scaled(g, ctx, b, 1, 1, 1, 0, "drawEmployeeViewPanel");
  };
  auto text = [&](const std::string &s, int32_t x, int32_t y, uint32_t c) {
    prec(g, "drawTextWithColor", {static_cast<uint32_t>(x), static_cast<uint32_t>(y), c}, &s);
    gfxb::draw_text_string(g, ctx, s, 0, x, y, c, false, "drawEmployeeViewPanel");
  };
  const uint32_t pics = va(ctx, GSO(g_sprites59b778, 0));
  const uint32_t golfers = va(ctx, GSO(g_golfers, 0));
  auto rec = [&](int32_t k) { return golfers + (static_cast<uint32_t>(k) << 8); };
  auto sbyte = [&](int32_t k, uint32_t off) { return static_cast<int32_t>(static_cast<int8_t>(m.load<uint8_t>(rec(k) + off))); };
  auto sword = [&](int32_t k, uint32_t off) { return static_cast<int32_t>(m.load<int16_t>(rec(k) + off)); };
  auto clampv = [&](int32_t v, int32_t lo, int32_t hi) {
    return static_cast<int32_t>(cm::call_peer(ctx, "clamp", {static_cast<uint32_t>(v), static_cast<uint32_t>(lo), static_cast<uint32_t>(hi)}).eax);
  };
  auto face = [&](int32_t k) { return pics + 0x2cu * static_cast<uint32_t>(clampv(add32(sword(k, 0xa4), 2), 1, 10)); };
  auto colour = [&](int32_t k) {
    uint32_t c = 0x80000000u;
    if (sword(k, 0xaa) > 0xa0) c = 0x80004210u;
    if (sword(k, 0xa8) > 0x10) c = 0x80000018u;
    if (sword(k, 0xa6) > 0x10) c = 0x80006000u;
    if (m.load<uint32_t>(rec(k) + 0x10) & (1u << 29)) c = 0x80007d08u;
    return c;
  };
  prec(g, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0xb0)), va(ctx, GSO(g_sprites58b390, 0xb0)), 0xd7, 0x1da});
  call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0xb0)), va(ctx, GSO(g_sprites58b390, 0xb0)), root, 0xd7, 0x1da, 0});
  const int32_t h = static_cast<int32_t>(cm::call_peer(ctx, "hitEmployeeSlot435570", {a[0], a[1]}).eax);
  if (h == -3) img(va(ctx, GSO(g_sprites58b1a0, 0)), 0xf0, 0x1ee);
  else if (h == -4) img(va(ctx, GSO(g_sprites58b1a0, 0x2c)), 0xd9, 0x20a);
  else if (h == -5) img(pics + 0x1e4, 0x135, 0x24b);
  else if (h == -6) img(pics + 0x210, 0x2f4, 0x24b);
  font(big);
  int32_t x1 = 0x136, row = 0, fa = 0x19c, fb = 0x146, fc = 0x156, fd = 0x145, ly = 0x1f2, count = 0;
  const int32_t skip = gs.g_ui_5aa6b8[4];
  const bool compact = gs.g_zoomScale < 4;
  // P22D fix 1 (log/roadmap/P22/P22D_notes.md item 5a): a column past the limit (the original skips only its first golfer, so the
  // rest of it is drawn at x1 0x136 + 4 * 0x79 = 794 and beyond, and the root keeps the hole badges' first 6 columns at the panel's
  // right border) is drawn here only without the layered capture. With it (the app's wide HUD), its calls run after the panel's
  // layer is closed and belong to no layer: the root gets the same pixels (nothing the original draws later in this function
  // reaches x 794: the scroll bar ends at 0x15f + 0x190, the -6 arrow image at 0x2f4 + 0x24), the panel layer shows the border whole
  const Gfx::HudCapture &hc = g.hud;
  const bool defer = hc.on && hc.depth == 1 && hc.cur == kGfxLayerPanel && hc.any && !hc.aborted && mode() != 16;
  std::vector<std::function<void()>> beyond;
  // P22D fix 2 (play switch drawEmployeeViewPanel.fourColumns = 0, the host's view block word +0x40 = n): 4 + n wide columns. The
  // added ones (column 4 on, x1 0x136 + 0x79 k past the root's width) are drawn by view_panel_wide below into layers the presenter
  // places right of the panel; the scroll bar's page is 4 (4 + n) golfers
  const int32_t ext = compact ? 0 : view_list_columns(ctx);
  struct Added {
    std::function<void(int32_t)> draw;  // the entry at its column's places moved left by the argument
    int32_t col;
  };
  std::vector<Added> added;
  const int32_t limit = compact ? 0x2f3 : 0x2b7 + 0x79 * ext;
  for (int32_t i = 0; i < 0x98; ++i) {
    const int32_t gi = mod32(add32(sub32(gs.g_lastAllocatedGolfer, i), 0x98), 0x98);
    const bool active = sbyte(gi, 0x21) > 0;
    bool show = false;
    const uint16_t p = m.load<uint16_t>(rec(gi) + 0xa2);
    if (p != 0xffff) {
      const int32_t q = static_cast<int16_t>(p);
      if (sword(q, 0xa2) == gi && sbyte(q, 0x21) > 0) show = true;
    }
    if (!show && !active) continue;
    ++count;
    if (count <= skip) continue;
    const int32_t old = row++;
    if (old == (compact ? 6 : 4)) {
      row = 1;
      const int32_t step = compact ? 0x3c : 0x79;
      x1 += step, fd += step, fc += step, fb += step, fa += step;
      ly = 0x1f2;
      if (fd > limit) continue;
    }
    // one golfer's entry at the column's places (x1, fa, fb, fc, fd) and row y
    auto entry = [&, gi, active](int32_t ex1, int32_t efa, int32_t efb, int32_t efc, int32_t efd, int32_t ey) {
      if (compact) {
        const uint32_t c = colour(gi);
        scaled(face(gi), ex1, ey);
        text(buffer_text(ctx, "buildGolferName", {static_cast<uint32_t>(gi), 0}), mode() == 11 ? efd + 1 : efd, ey + 1, c);
        return;
      }
      font(big);
      if (gi & 1) {
        scaled(pics, ex1, ey);
        const int32_t st = sbyte(gi, 0x21);
        std::string s = std::to_string(st);
        if (st == 0x13) s = gfxb::vstring(m, va(ctx, GSO(f_strpool_4c61dc, 8)));
        if (st <= 0) s = std::to_string(sbyte(sword(gi, 0xa2), 0x21));
        text(s, ex1 + (st >= 10 ? 5 : 8), ey + 0xf, 0x80000000u);
        if (sword(gi, 0xb0) != -1) {
          for (int32_t k = 0; k < 4; ++k) scaled(pics + 0x2c0, efc + 10 * k, ey + 0x11);
          for (int32_t k = 0; k < sword(gi, 0xb2); ++k) scaled(pics + 0x23c, efc + 10 * k, ey + 0x11);
        }
      }
      font(small);
      if (active) {
        text(buffer_text(ctx, "buildGolferName", {static_cast<uint32_t>(gi), 0}), mode() == 11 ? efb + 1 : efb, ey + 5, colour(gi));
        scaled(face(gi), efa, ey + 2);
      }
    };
    const int32_t col = compact ? 0 : (x1 - 0x136) / 0x79;
    if (defer && fd > limit) {  // P22D fix 1: a column past the limit, after the layer
      beyond.push_back([entry, x1, fa, fb, fc, fd, ly]() { entry(x1, fa, fb, fc, fd, ly); });
      // the font the entry leaves selected is what the next entry or the scroll bar would see (font(small) after a wide entry)
      if (!compact) font(small);
    } else if (defer && ext > 0 && col >= 4) {  // P22D fix 2: an added column, drawn by view_panel_wide
      added.push_back({[entry, x1, fa, fb, fc, fd, ly](int32_t s) { entry(x1 - s, fa - s, fb - s, fc - s, fd - s, ly); }, col});
      font(small);
    } else {
      entry(x1, fa, fb, fc, fd, ly);
    }
    ly += compact ? 0xe : 0x15;
  }
  if (count != 0) {
    const int32_t top = clampv(div32(mul32(skip, 400), count), 0, 0x18f);
    const int32_t page = compact ? 0x30 : 4 * (4 + ext);  // P22D fix 2 (n = 0: the original's 0x10)
    const int32_t bottom = clampv(div32(mul32(add32(skip, page), 400), count), 1, 0x190);
    const uint32_t fr[5] = {static_cast<uint32_t>(top + 0x15f), 0x24e, static_cast<uint32_t>(bottom - top - 1), mode() == 12 ? 7u : 6u,
                            0x8000739fu};
    prec(g, "fillWindowRect", {fr[0], fr[1], fr[2], fr[3], fr[4]});
    call(g, ctx, "fillWindowRect", {state_va(ctx, &GameState::g_rootWindow), fr[0], fr[1], fr[2], fr[3], fr[4]});
  }
  font(big);
  const int32_t hover = static_cast<int32_t>(cm::call_peer(ctx, "hitEmployeeSlot435570", {a[0], a[1]}).eax);
  dwell(g, ctx, hover, "tooltip: drawEmployeeViewPanel's dwell tip (drawMapLine bar and centred text)", [&]() {
    gfxt::view_panel_tip(g, ctx, hover, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
  });
  if (!beyond.empty() || !added.empty()) {  // P22D fix 1: the columns past the limit, outside every layer (the panel's layer is
                                            // reopened, empty); fix 2: the added columns' layers
    const size_t before = g.hud.layers.size();
    gfx_layer_end(ctx);
    for (const std::function<void()> &f : beyond) f();
    font(big);  // the selection the function leaves (set before the tip)
    g.hud.mark = g.root.px;  // their pixels are no layer's: the next boundary's gap does not see them
    if (!added.empty() && g.hud.layers.size() == before + 1 && g.hud.layers.back().id == kGfxLayerPanel) {
      int32_t used = 0;
      for (const Added &e : added) used = std::max(used, e.col - 3);
      std::vector<std::pair<std::function<void(int32_t)>, int32_t>> cols;
      for (Added &e : added) cols.emplace_back(std::move(e.draw), e.col);
      view_panel_wide(g, ctx, before, std::min(used, ext), cols, [&]() {
        call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0xb0)), va(ctx, GSO(g_sprites58b390, 0xb0)), root, 0xd7, 0x1da, 0});
      });
      font(big);
    }
    gfx_layer_begin(ctx, kGfxLayerPanel);
  }
}

// drawEmployeeDetailPanel.draw(x, y) (c3fc.cpp DrawEmployeeDetailPanel_re, the drawing). The positions P(i) are the word
// pairs of f_tab_point16_4c7af0 + 4 i (entries 8..15 are the eight list slots); the images are g_sprites5a8c90 + offset.
// Surface_stretchBlit of g_sprites541a18 + 0x134 over g_sprites58b390 + 0x134 at (0xd7, 0x1da); the scroll s =
// g_ui_5aa558, which the original drops by 2 when positive while the previous count n = g_ui_5aa6b8[2] is below 7; s > 0
// draws the up arrow + 0x1b8 at P(2); n > 8 the down arrow + 0x268 at P(3); h = hitEmployee436b00(x, y) (pure): 2 counts
// only while s != 0 and 3 while n > 8 (else -1); -2 Image + 0x58 of g_sprites58b1a0 at (0x10e, 0x1db), -3 + 0x84 at (0xf0,
// 0x1ee); h >= 8 + 0x580 + 0xb0 (h & 1) at P(h); h 0..7 lit: + 0xb0 h at P(h); always + 0xdc at P(1). The selected slot e =
// g_employeePanelMode != -1: + 0x5ac + 0xb0 (e & 1) at P(8 + e), and drawCircle around the employee record
// g_employeeDetailList[e] (the previous frame's list) with g_zoomScale = g_ui_5a34e4[1] during the call (S3.8's overlay
// lines: reported without a drawCircle body). The list: the employee records (g_walkers + 0x10 + 0x4c k, k < 64) with a
// negative type other than -6 and a non-zero flags byte, past -s, slots 0..7 each a sprite-sheet cell: table (word +0xe
// below 0xb: 0x20e, not 0xc: 0x216, else 0x21e) + t, t = -2 - type (+4 with flags bit 3), cellAt(table, word +0xc %
// g_sheetCols[table], (g_cameraRot - byte +6 - 2) & 7), addSprite(P(8 + slot) + (0x1e, 0x18), y, 0x1e, 0x28, cell, 4, t +
// 0x82) (the scene family's body, S3.5; reported without it). Then for e != -1 with the record of THIS frame's list: flags
// 0 ends the drawing; else the buttons + 0x2c0 (+ 0x84 unless h == 4) at P(4), + 0x2ec at P(4) when f_w_4c2e00[4] is the
// record, + 0x370 (+ 0x84 unless h == 5) at P(5), + 0x420 (+ 0x84 unless h == 6) at P(6), and the text block centred at x
// 0x2bc: the name (appendWalkerName) at y 0x216 in the current font; setDrawTarget(g_fontRef_519fd8); f_strpool_4c80d0 +
// 0x3cc + appendDate(word +0x12 << 10) at 0x222, + 0x3c4 + appendNumber(100 word +0x14) at 0x22e, the role text (8 texts
// by role = ((flags & 8) >> 3) - (2 type + 4), unsigned; above 7 the previous text again) at 0x23c; setDrawTarget
// (g_font51b360); the decimal of word +0x16 at 0x248, all in 0x80000000. With no selection + 0x318, + 0x3c8, + 0x478 at
// P(4), P(5), P(6). n + s >= 0x14 (this frame's): + 0x58 at P(0); g_ui_59ca54 == 0: the hire button + 0x2c at P(0). Last
// the dwell tip (reported).
void b_detail_panel(Gfx &g, Ctx &ctx, const uint32_t *a, int) {
  g.ensure_root(ctx);
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const uint32_t root = g.root_va(ctx);
  const uint32_t pics = va(ctx, GSO(g_sprites5a8c90, 0));
  auto px = [&](int32_t i) { return static_cast<int32_t>(m.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7af0, 4 * i)))); };
  auto py = [&](int32_t i) { return static_cast<int32_t>(m.load<int16_t>(va(ctx, GSO(f_tab_point16_4c7af0, 4 * i + 2)))); };
  auto img_at = [&](uint32_t s, int32_t x, int32_t y) {
    prec(g, "Surface_drawScaled", {s, static_cast<uint32_t>(x), static_cast<uint32_t>(y)});
    const uint32_t b[8] = {s, root, static_cast<uint32_t>(x), static_cast<uint32_t>(y), 1, 1, 1, 0};
    draw_scaled(g, ctx, b, 1, 1, 1, 0, "drawEmployeeDetailPanel");
  };
  auto img = [&](uint32_t off, int32_t i) { img_at(pics + off, px(i), mode() == 13 && i >= 8 ? py(i) + 1 : py(i)); };
  auto font = [&](uint32_t f) {
    prec(g, "setDrawTarget", {f});
    call(g, ctx, "setDrawTarget", {root, f, 0, 0, 0});
  };
  auto centred = [&](const std::string &s, int32_t y0) {
    const int32_t y = mode() == 14 ? y0 + 1 : y0;
    prec(g, "drawCenteredText", {0x2bc, static_cast<uint32_t>(y), 0x80000000u}, &s);
    with_scratch_buffer(ctx, s, [&](uint32_t buf) {
      gfxb::draw_text_string(g, ctx, s, buf, 0x2bc, y, 0x80000000u, true, "drawEmployeeDetailPanel");
    });
  };
  prec(g, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0x134)), va(ctx, GSO(g_sprites58b390, 0x134)), 0xd7, 0x1da});
  call(g, ctx, "Surface_stretchBlit", {va(ctx, GSO(g_sprites541a18, 0x134)), va(ctx, GSO(g_sprites58b390, 0x134)), root, 0xd7, 0x1da, 0});
  int32_t scroll = gs.g_ui_5aa558;
  const int32_t prev_n = gs.g_ui_5aa6b8[2];
  if (scroll > 0) {
    if (prev_n < 7) scroll = sub32(scroll, 2);
    if (scroll > 0) img(0x1b8, 2);
  }
  if (prev_n > 8) img(0x268, 3);
  int32_t hit = static_cast<int32_t>(cm::call_peer(ctx, "hitEmployee436b00", {a[0], a[1]}).eax);
  bool lit = false;
  if (hit == 2) {
    if (scroll == 0) hit = -1;
    else lit = true;
  } else if (hit == 3) {
    if (!(prev_n > 8)) hit = -1;
    else lit = true;
  } else if (hit == -1) {
  } else if (hit < 0) {
    if (hit == -2) img_at(va(ctx, GSO(g_sprites58b1a0, 0x58)), 0x10e, 0x1db);
    else if (hit == -3) img_at(va(ctx, GSO(g_sprites58b1a0, 0x84)), 0xf0, 0x1ee);
  } else if (hit >= 8) {
    img(0x580 + 0xb0u * static_cast<uint32_t>(hit & 1), hit);
  } else {
    lit = true;
  }
  if (lit) img(0xb0u * static_cast<uint32_t>(hit), hit);
  img(0xdc, 1);
  const uint32_t emp = state_va(ctx, &GameState::g_walkers) + 0x10;
  auto erec = [&](int32_t k) { return emp + 0x4cu * static_cast<uint32_t>(k); };
  const Ptr<int32_t> list0 = state_arr(ctx, &GameState::g_employeeDetailList);
  int32_t sel = gs.g_employeePanelMode;
  if (sel != -1) {
    img(0x5ac + 0xb0u * static_cast<uint32_t>(sel & 1), 8 + sel);
    const uint32_t r = erec(*(list0 + sel));
    const int32_t type = static_cast<int8_t>(m.load<uint8_t>(r + 3));
    int32_t size = type != -4 ? 4 : 6;
    if (type == -4 && (m.load<uint8_t>(r + 2) & 8) != 0) size += 2;
    uint32_t c = 0x80006318u;
    if (type == -5) c = 0x800003ffu;
    else if (type == -4) c = 0x80007ff0u;
    else if (type == -3) c = 0x80000018u;
    const int32_t cx = (static_cast<int32_t>(static_cast<int8_t>(m.load<uint8_t>(r))) << 10) + 0x200;
    const int32_t cy = (static_cast<int32_t>(static_cast<int8_t>(m.load<uint8_t>(r + 1))) << 10) + 0x200;
    prec(g, "drawCircle", {static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), static_cast<uint32_t>(25 * size), c, static_cast<uint32_t>(gs.g_ui_5a34e4[1])});
    if (gfxb::find_body("drawCircle")) {
      const int32_t zoom = gs.g_zoomScale;
      gs.g_zoomScale = gs.g_ui_5a34e4[1];
      // P23E fix 5: the circle is drawn over the course round the employee, not on the panel. Inside the panel's HUD layer
      // (drawEmployeeDetailPanel is a kGfxLayerPanel callee) a presenter moved it with the panel's anchor, away from the employee in
      // a wide window (the original draws it round the employee): the layer is closed for the circle (its pixels are the centred
      // gap's) and the same layer opens again after it. Only with the HUD capture on (depth 1: the callee's own layer).
      const int32_t layer = g.hud.on && g.hud.depth == 1 ? g.hud.cur : -1;
      if (layer >= 0) gfx_layer_end(ctx);
      const uint32_t scope = g.hud.wide_scope;  // P23E fix 5 (gfx.h kGfxWideStaff): the circle's lines are recorded as still lines
      g.hud.wide_scope |= kGfxWideStaff;
      call(g, ctx, "drawCircle", {static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), static_cast<uint32_t>(25 * size), c});
      g.hud.wide_scope = scope;
      if (layer >= 0) gfx_layer_begin(ctx, layer);
      gs.g_zoomScale = zoom;
    } else {
      g.unsup("drawCircle: the selected employee's circle over the course (lineTo: the overlay lines)");
    }
  }
  // the list of this frame (the original rebuilds g_employeeDetailList here; the layout does it after this drawing)
  int32_t list[64];
  for (int32_t k = 0; k < 64; ++k) list[k] = *(list0 + k);
  int32_t slot = neg32(scroll), listed = slot;
  for (int32_t k = 0; k < 64; ++k) {
    const uint32_t r = erec(k);
    const int32_t type = static_cast<int8_t>(m.load<uint8_t>(r + 3));
    const uint8_t flags = m.load<uint8_t>(r + 2);
    if (type >= 0 || flags == 0 || type == -6) continue;
    if (slot < 0 || slot >= 8) {
      listed = slot = add32(slot, 1);
      continue;
    }
    const int32_t col = static_cast<int32_t>(static_cast<uint32_t>(sub32(sub32(gs.g_cameraRot, m.load<uint8_t>(r + 6)), 2)) & 7u);
    int32_t t = -2 - type;
    if (flags & 8) t += 4;
    const int16_t anim = m.load<int16_t>(r + 0xe);
    const int32_t table = add32(anim < 0xb ? 0x20e : anim != 0xc ? 0x216 : 0x21e, t);
    const int32_t cols = *(state_arr(ctx, &GameState::g_sheetCols) + table);
    if (cols == 0) {
      g.unsup("drawEmployeeDetailPanel: an employee sheet with no columns (the original's idiv faults)");
      return;
    }
    const int32_t phase = mod32(m.load<int16_t>(r + 0xc), cols);
    const int32_t sx = add32(px(8 + slot), 0x1e), sy = add32(py(8 + slot), 0x18);
    const uint32_t cell = cm::call_peer(ctx, "cellAt", {static_cast<uint32_t>(table), static_cast<uint32_t>(phase), static_cast<uint32_t>(col)}).eax;
    prec(g, "addSprite", {static_cast<uint32_t>(sx), static_cast<uint32_t>(sy), static_cast<uint32_t>(sy), 0x1e, 0x28, cell, 4,
                          static_cast<uint32_t>(t + 0x82)});
    call_or_report(g, ctx, "addSprite", {static_cast<uint32_t>(sx), static_cast<uint32_t>(sy), static_cast<uint32_t>(sy), 0x1e, 0x28, cell, 4,
                                         static_cast<uint32_t>(t + 0x82)},
                   "addSprite: the sprite queue (drawEmployeeDetailPanel's employees)");
    list[slot] = k;
    listed = slot = add32(slot, 1);
  }
  if (sel != -1) {
    const int32_t rk = list[sel];
    const uint32_t r = erec(rk);
    if (m.load<uint8_t>(r + 2) == 0) return;   // the original clears the selection and returns
    img(0x2c0 + (hit != 4 ? 0x84u : 0u), 4);
    if (gs.f_w_4c2e00[4] == rk) img(0x2ec, 4);
    img(0x370 + (hit != 5 ? 0x84u : 0u), 5);
    img(0x420 + (hit != 6 ? 0x84u : 0u), 6);
    const int32_t type = static_cast<int8_t>(m.load<uint8_t>(r + 3));
    const uint32_t role = ((m.load<uint8_t>(r + 2) & 8u) >> 3) - static_cast<uint32_t>(2 * type + 4);
    const std::string name = buffer_text(ctx, "appendWalkerName", {static_cast<uint32_t>(rk)});
    centred(name, 0x216);
    font(state_va(ctx, &GameState::g_fontRef_519fd8));
    auto pool = [&](uint32_t off) { return gfxb::vstring(m, va(ctx, GSO(f_strpool_4c80d0, off))); };
    centred(scratch_text(ctx, pool(0x3cc), "appendDate", {static_cast<uint32_t>(static_cast<int32_t>(m.load<int16_t>(r + 0x12)) << 10)}), 0x222);
    std::string s = scratch_text(ctx, pool(0x3c4), "appendNumber", {static_cast<uint32_t>(mul32(m.load<int16_t>(r + 0x14), 100))});
    centred(s, 0x22e);
    static const uint32_t kRole[8] = {0x3b0, 0x39c, 0x38c, 0x374, 0x360, 0x34c, 0x338, 0x320};
    if (role <= 7) s = pool(kRole[role]);
    centred(s, 0x23c);
    font(state_va(ctx, &GameState::g_font51b360));
    centred(std::to_string(static_cast<int32_t>(m.load<int16_t>(r + 0x16))), 0x248);
  } else {   // control 15: the selection branch's unlit images instead
    img(mode() == 15 ? 0x2c0 + 0x84 : 0x318, 4);
    img(mode() == 15 ? 0x370 + 0x84 : 0x3c8, 5);
    img(mode() == 15 ? 0x420 + 0x84 : 0x478, 6);
  }
  if (add32(listed, scroll) >= 0x14) img(0x58, 0);
  if (gs.g_ui_59ca54 == 0) img(0x2c, 0);
  dwell(g, ctx, hit, "tooltip: drawEmployeeDetailPanel's dwell tip (drawPanelBackdrop)", [&]() {
    gfxt::detail_panel_tip(g, ctx, hit, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]));
  });
}
#undef GSO

}  // namespace

// TEST-ONLY gfx_hud.dumpSprites (gfx.call, no frame code uses the name): `$gfx/hud_sprites.txt`, one line per SpriteRef
// the backend holds: `<key> <kept w> <kept h> <left trim> <top trim>` (cropSprite's +0x10, +0x14, +0x20, +0x24), keys in
// ascending order; port/test/test_gfx_hud.py compares them with the original's SpriteRef records of a segment start image
void b_dump_sprites(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  std::string s;
  char line[96];
  for (const auto &kv : g.sprites) {
    std::snprintf(line, sizeof line, "%u %d %d %d %d\n", kv.first, kv.second.w, kv.second.h, kv.second.left, kv.second.top);
    s += line;
  }
  gfxb::put_file(ctx, "$gfx/hud_sprites.txt", std::vector<uint8_t>(s.begin(), s.end()));
}

// loadWorldAssets.images (menu.cpp hands it to the backend at the start of loadWorldAssets): the build palette's cuts of
// c3fv.cpp LoadWorldAssets_re, for course type t = the signed byte g_courseType (each load below is flag 2, so the palette
// images the original loads into the working Image before them change nothing they cut):
//   t <= 3: the terrain-buttons sheet (f_strpool_4c95f4 + 0x2914 / 0x28b0 / 0x28d0 / 0x28f4 for t 0..3) cut into the SpriteRefs
//     from g_sprites570cb8 while below its + 0x8f0: row r = 0.., y = (r % 7) * 0x36, x from 0 (r < 7) or 0xf8 in steps of
//     0x3e, four 0x3e x 0x36 cells per row, kind 1, trim 1;
//   the tool strip of the same image: x from 0x208 in steps of 0x50 while below 0x2f8, d = g_sprites570cb8 + 0x9a0 + 0x2c k,
//     cells into d - 0xb0 / d / d + 0xb0 at y 0 / 0x96 / 0x12c with the type's sizes (c3fv.cpp kStrip, below);
//   the layout sheet (+ 0x2894 / 0x284c / 0x2864 / 0x287c): ten columns k of four 0x4b x 0x64 cells at x = 0x4b k, into
//     esi - 0x58, esi, esi - 0x2c, esi + 0x2c with esi = g_sprites53df58 + 0x58 + 0xb0 k (while below + 0x738), at y 0,
//     0x64, 0xc8, 0x12c.
void b_world_images(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  Art A{g, ctx, state_va(ctx, &GameState::f_strpool_4c95f4) - static_cast<uint32_t>(offsetof(GameState, f_strpool_4c95f4)), {}};
  if (mode() == 3) return;
  const uint32_t t = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(ctx.mem.load<uint8_t>(state_va(ctx, &GameState::g_courseType)))));
  if (t > 3) return;
  const uint32_t pool = static_cast<uint32_t>(offsetof(GameState, f_strpool_4c95f4));
  static const uint32_t kSheet[4] = {0x2914, 0x28b0, 0x28d0, 0x28f4};
  static const uint32_t kLayout[4] = {0x2894, 0x284c, 0x2864, 0x287c};
  static const int16_t kStrip[4][6] = {{0x41, 0x54, 0x3E, 0x46, 0x3E, 0x58}, {0x4B, 0x4A, 0x3E, 0x43, 0x41, 0x68},
                                       {0x48, 0x4F, 0x3E, 0x51, 0x41, 0x68}, {0x43, 0x54, 0x3E, 0x58, 0x43, 0x4C}};
  const uint32_t btn = static_cast<uint32_t>(offsetof(GameState, g_sprites570cb8));
  A.load_pcx(pool + kSheet[t]);
  {
    uint32_t dest = btn;
    for (int r = 0; dest < btn + 0x8f0; r++) {
      int x = r >= 7 ? 0xF8 : 0;
      for (int c = 0; c < 4; c++, dest += 0x2C, x += 0x3E) A.cut(dest, x, (r % 7) * 0x36, 0x3E, 0x36, 1, 1);
    }
  }
  {
    const int16_t *s = kStrip[t];
    uint32_t d = btn + 0x9a0;
    for (int x = 0x208; x < 0x2F8; x += 0x50, d += 0x2C) {
      A.cut(d - 0xB0, x, 0, s[0], s[1], 1, 1);
      A.cut(d, x, 0x96, s[2], s[3], 1, 1);
      A.cut(d + 0xB0, x, 0x12C, s[4], s[5], 1, 1);
    }
  }
  A.load_pcx(pool + kLayout[t]);
  const uint32_t lay = static_cast<uint32_t>(offsetof(GameState, g_sprites53df58));
  int x = 0;
  for (uint32_t d = lay + 0x58; d < lay + 0x738; d += 0xB0, x += 0x4B) {
    A.cut(d - 0x58, x, 0, 0x4B, 0x64, 1, 1);
    A.cut(d, x, 0x64, 0x4B, 0x64, 1, 1);
    A.cut(d - 0x2C, x, 0xC8, 0x4B, 0x64, 1, 1);
    A.cut(d + 0x2C, x, 0x12C, 0x4B, 0x64, 1, 1);
  }
}

// TEST-ONLY gfx_hud.dumpUncut: `$gfx/hud_uncut.txt`, one line `<key> <draws>` per SpriteRef a HUD draw read with no cut
void b_dump_uncut(Gfx &g, Ctx &ctx, const uint32_t *, int) {
  std::string s;
  char line[64];
  for (const auto &kv : st(g).uncut) {
    std::snprintf(line, sizeof line, "%u %llu\n", kv.first, static_cast<unsigned long long>(kv.second));
    s += line;
  }
  gfxb::put_file(ctx, "$gfx/hud_uncut.txt", std::vector<uint8_t>(s.begin(), s.end()));
}

const GfxBodyDef kBodies[] = {
    {"gfx_hud.dumpUncut", b_dump_uncut},
    {"loadInterfaceArt.images", b_iart_images},
    {"loadWorldAssets.images", b_world_images},
    {"gfx_hud.dumpSprites", b_dump_sprites},
    {"Surface_blit2", b_blit2},
    {"Surface_drawScaled2", b_draw_scaled2},
    {"drawScaledSprite421b90", b_draw_scaled_421b90},
    {"Surface_drawScaled", b_draw_scaled},
    {"drawScaledSprite421b60", b_draw_scaled_421b60},
    {"drawFrameR3", b_draw_frame_r3},
    {"drawFrameS2", b_draw_frame_s2},
    {"drawTerrainToolbar", b_terrain_toolbar},
    {"showMainRadialMenu.draw", b_radial_menu},
    {"showInfoCursorPanel.draw", b_info_panel},
    {"drawGolferActionPanel.draw", b_action_panel},
    {"drawEmployeeViewPanel.draw", b_view_panel},
    {"statusBar.tips", b_meter_tips},  // P22D fix 3
    {"drawEmployeeDetailPanel.draw", b_detail_panel},
    {"gfx_hud.recordPanels", b_record_panels},
};

}  // namespace

extern const GfxFamily kGfxFamily_hud = {"hud", kBodies, sizeof(kBodies) / sizeof(kBodies[0]), nullptr, 0};

namespace gfxh {
const std::array<uint16_t, 0x10000> &half_bright_table() { return half_bright_lut(); }
}  // namespace gfxh

}  // namespace portsim
